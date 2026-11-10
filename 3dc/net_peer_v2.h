// A joiner and its host through a version 2 relay: handshake B over the joiner's relay port, its datagrams, resends and timers, and then
// the sealed game datagrams (CRYPTO.md sections 8 and 9, in the AVPx-Dedicated-Server repository). Written 2026-11-09 by Melanikus for the
// AvP port.
//
// WHAT IT DOES, in plain words. net_handshake_b.h knows the five messages of handshake B, and net_sealed_game.h how to seal a game datagram.
// This file runs them over a real network, where datagrams get lost, come twice, or come from someone who isn't playing:
//   - it draws the fresh keys and random bytes each message needs, for real (the operating system's randomness), in 5.1 point 14's order;
//   - it cuts each message into 1,200-byte datagrams ('AVPE', net_v2_fragments.h), and puts the other side's back together;
//   - it sends a message again when no answer comes, and gives up in the end ("the host doesn't answer");
//   - it answers a message that comes again with exactly the same bytes as before, never a fresh answer, which would make a second set of
//     keys (CRYPTO.md section 8, "Losses");
//   - once the handshake is done, it seals and opens the game's ENet datagrams, and nothing else ever goes out (no plaintext, ever).
//
// ONE ON EACH SIDE OF A JOINER'S PORT:
//   - NetPeerJoiner, in a joining game: one, for its one host;
//   - NetPeerHostSide, in a hosting game: one for each joiner, made when the relay says a joiner is coming on a port (NEW_JOINER).
//
// WHO SENDS WHAT, AND WHEN. The joiner sends B1, and B1 again if no B2 comes, on the schedule below; then B3 the same way until B4 comes.
// Once B4 checks, it has its keys, and sends B5, the last message. B5 can't be stood in for (it carries nJ, which the host needs for the
// check code), so the joiner sends B5 again on the same schedule until a sealed datagram from the host opens, which the host sends only
// once B5 has checked. The host answers each message as it comes, keeps each answer 10 seconds, and sends the same bytes again if the
// message comes again. No game byte is sealed by the joiner before B4 checks, or by the host before B5 checks.
//
// WHAT IS REFUSED, and dropped without an answer (AvP Tao 3: everything from outside is hostile):
//   - anything that isn't a 1,200-byte 'AVPE' datagram by every rule of net_v2_fragments.h, or a sealed 'AVPG' datagram;
//   - a datagram of another handshake (another id), or of a message this side doesn't expect, or with a cookie that isn't 16 zeros;
//   - a sealed datagram before the keys, or that doesn't open.
// A whole message that fails (its tag, its keys, the wrong host, a broken promise) ends the handshake for good, as Noise says: the player
// is told why, and joins again if they like.
//
// It's game-free, as net_relay.h is: the caller gives the time in milliseconds, hands over every datagram from the joiner's relay port,
// and sends what TakeSends gives it to that port. Its test is tests/net_v2b_test.cpp. Nothing here writes anything to the log.

#ifndef NET_PEER_V2_H
#define NET_PEER_V2_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "net_crypto.h"
#include "net_handshake_b.h"
#include "net_identity.h"
#include "net_pq.h"
#include "net_sealed_game.h"
#include "net_v2_fragments.h"

// B's kinds are named in net_handshake_b.h, and the fragments' file (Primary's) names the first and the last: they must agree
static_assert(kNetHandshakeBKindB1 == kNetV2KindB1 && kNetHandshakeBKindB5 == kNetV2KindB5, "handshake B's kinds are 16 to 20 in both files");

// How long to wait for an answer before sending a message again, one wait after each send. After the last wait with no answer, it gives
// up (CRYPTO.md sections 6 and 17: "after 0.5, 1, 2 and 4 s, then give up"). So a message goes at 0, again at 0.5, 1.5 and 3.5 s, and
// at 7.5 s it's given up: three sends again, every time counted from the first send, so a late frame never pushes the rest later. That's
// how Primary's handshake A does it too (agreed 2026-11-09).
static const uint32_t kNetPeerWaitsMs[] = { 500, 1000, 2000, 4000 };
size_t const kNetPeerWaits = sizeof(kNetPeerWaitsMs) / sizeof(kNetPeerWaitsMs[0]);
uint32_t const kNetPeerKeepAnswerMs = 10000;   // how long the host keeps B2 and B4, to send again byte for byte (section 17)

size_t const kNetPeerMostSends = 3;            // the most datagrams one message takes: B2 and B3 take 3

// Why a peer ended, beyond net_handshake_b.h's and net_sealed_game.h's own reasons
int const kNetPeerNoAnswer = 7;                // CRYPTO.md section 16's 7: not finished in time ("the host doesn't answer")
int const kNetPeerCouldntDraw = 110;           // the operating system's randomness, or an encapsulation, was refused

// One datagram for the caller to send to the joiner's relay port
struct NetPeerSend
{
	uint8_t bytes[kNetV2DatagramBytes];
	size_t size;
};

// Datagrams dropped, by why, for the log's one line at the end
struct NetPeerDrops
{
	uint32_t notOurs;        // not a datagram of this handshake at all: another magic, size, version, id, or a broken fragment
	uint32_t notNow;         // a message this side doesn't expect now, or one that came again past the time its answer was kept
	uint32_t badCookie;      // an 'AVPE' datagram whose cookie isn't 16 zeros (refused as 5, agreed with Primary, 2026-11-09)
	uint32_t sealedEarly;    // a sealed datagram before the keys
	uint32_t sealedBad;      // a sealed datagram that didn't open
};

// What both sides share: a message cut into datagrams, waiting to be sent or sent again
class NetPeerOutbox
{
public:
	NetPeerOutbox() : count(0), pending(false)
	{
	}

	// Cuts message into its datagrams under 'AVPE', the handshake's id and a cookie of 16 zeros, and makes them ready to send
	bool Fill(uint8_t kind, const uint8_t id[kNetV2IdBytes], const uint8_t *message, size_t size)
	{
		static const uint8_t kNoCookie[kNetV2CookieBytes] = {};
		count = NetV2_FragmentCount(size);

		if (count > kNetPeerMostSends)
		{
			count = 0;
			return false;
		}

		for (size_t i = 0; i < count; i++)
		{
			if (!NetV2_CutFragment(kNetV2MagicEndToEnd, kind, id, kNoCookie, message, size, i, datagrams[i].bytes))
			{
				count = 0;
				return false;
			}

			datagrams[i].size = kNetV2DatagramBytes;
		}

		pending = true;
		return true;
	}

	// The same datagrams again, byte for byte
	void Again()
	{
		pending = count > 0;
	}

	void Clear()
	{
		count = 0;
		pending = false;
	}

	size_t Take(NetPeerSend *out, size_t room)
	{
		if (!pending)
		{
			return 0;
		}

		size_t const n = count < room ? count : room;
		memcpy(out, datagrams, n * sizeof(NetPeerSend));
		pending = false;
		return n;
	}

private:
	NetPeerSend datagrams[kNetPeerMostSends];
	size_t count;
	bool pending;
};

// Reads one 'AVPE' datagram of handshake id as a fragment of the message kind, into assembly. 1 if it was taken and the message is now
// whole, 0 if it was taken, -1 if it isn't one, with drops counted.
inline int NetPeer_TakeFragment(const uint8_t *datagram, size_t size, uint8_t kind, const uint8_t id[kNetV2IdBytes], bool anyId,
	NetV2Assembly *assembly, NetPeerDrops *drops)
{
	static const uint8_t kNoCookie[kNetV2CookieBytes] = {};
	NetV2Fragment fragment;

	if (!NetV2_ReadFragment(kNetV2MagicEndToEnd, datagram, size, &fragment) || (!anyId && memcmp(fragment.id, id, kNetV2IdBytes) != 0))
	{
		drops->notOurs++;
		return -1;
	}

	if (memcmp(fragment.cookie, kNoCookie, sizeof(kNoCookie)) != 0)
	{
		drops->badCookie++;
		return -1;
	}

	if (fragment.kind != kind)
	{
		drops->notNow++;
		return -1;
	}

	if (!NetV2_AssemblyAdd(assembly, &fragment))
	{
		drops->notOurs++;
		return -1;
	}

	return NetV2_AssemblyComplete(assembly) ? 1 : 0;
}

// ----- The joiner's side

class NetPeerJoiner
{
public:
	NetPeerJoiner()
	{
		Reset();
	}

	~NetPeerJoiner()
	{
		Reset();
	}

	NetPeerJoiner(const NetPeerJoiner &) = delete;
	NetPeerJoiner &operator=(const NetPeerJoiner &) = delete;

	// Starts handshake B with the host the listing named, on this joiner's port of that region's relay. identity lasts as long as this.
	// False if a draw or the handshake refused; then Why() says why.
	bool Start(const char *region, uint16_t relayPort, const uint8_t hostFingerprint[kNetIdentityFingerprintBytes], const NetIdentity *identity, uint32_t now)
	{
		Reset();
		uint8_t ePublic[kNetCryptoP384PublicBytes], ekPublic[kNetPqKemPublicBytes], ekPrivate[kNetPqKemPrivateBytes], nJ[kNetHandshakeBNonceBytes];
		uint8_t b1[kNetHandshakeBB1Bytes];

		// The draws, in 5.1 point 14's order: e's P-384 key, e's ML-KEM key, nJ. Then the handshake's id, which is in no message.
		NetEcdhKey *const e = NetCrypto_EcdhNew(ePublic);
		bool const drawn = e != NULL && NetPq_KemKeypair(ekPublic, ekPrivate) && NetCrypto_Random(nJ, sizeof(nJ)) && NetCrypto_Random(id, sizeof(id));

		if (!drawn)
		{
			NetCrypto_EcdhFree(e);
			NetCrypto_Wipe(ekPrivate, sizeof(ekPrivate));
			NetCrypto_Wipe(nJ, sizeof(nJ));
			return Fail(kNetPeerCouldntDraw);
		}

		bool const written = handshake.WriteB1(region, relayPort, hostFingerprint, identity, e, ePublic, ekPublic, ekPrivate, nJ, b1);
		NetCrypto_Wipe(ekPrivate, sizeof(ekPrivate));
		NetCrypto_Wipe(nJ, sizeof(nJ));

		if (!written)
		{
			return Fail(handshake.Why());
		}

		if (!outbox.Fill(kNetHandshakeBKindB1, id, b1, sizeof(b1)))
		{
			return Fail(kNetHandshakeBCouldnt);
		}

		step = kWaitB2;
		Sent(now);
		return true;
	}

	// A datagram from the joiner's relay port: a handshake datagram, or a sealed game datagram. A game datagram that opens goes into
	// out (room for outRoom bytes), and *written says how many: that's ENet's to read. Anything else gives *written = 0.
	void Hear(const uint8_t *datagram, size_t size, uint8_t *out, size_t outRoom, size_t *written, uint32_t now)
	{
		*written = 0;

		if (!datagram || size < 4)
		{
			drops.notOurs++;
			return;
		}

		if (memcmp(datagram, kNetSealedGameMagic, 4) == 0)
		{
			HearSealed(datagram, size, out, outRoom, written, now);
			return;
		}

		if (step == kWaitB2)
		{
			if (NetPeer_TakeFragment(datagram, size, kNetHandshakeBKindB2, id, false, &assembly, &drops) == 1)
			{
				TakeB2(now);
			}
		}
		else if (step == kWaitB4)
		{
			if (NetPeer_TakeFragment(datagram, size, kNetHandshakeBKindB4, id, false, &assembly, &drops) == 1)
			{
				TakeB4(now);
			}
		}
		else
		{
			drops.notNow++;
		}
	}

	// Time goes by: sends a message again when its wait is over, and gives up after the last
	void Tick(uint32_t now)
	{
		bool const waiting = step == kWaitB2 || step == kWaitB4 || (step == kSealed && !heardFromHost);

		if (!waiting || static_cast<uint32_t>(now - sentAt) < kNetPeerWaitsMs[waits])
		{
			return;
		}

		if (waits + 1 >= kNetPeerWaits)
		{
			Fail(kNetPeerNoAnswer);
			return;
		}

		// The next wait starts when this one should have ended, not when the frame came
		sentAt += kNetPeerWaitsMs[waits];
		waits++;
		outbox.Again();
	}

	size_t TakeSends(NetPeerSend *out, size_t room)
	{
		return outbox.Take(out, room);
	}

	// Seals one of ENet's datagrams for the host. False, and nothing to send, before B4 has checked or after the end.
	bool Seal(const uint8_t *datagram, size_t size, uint8_t *out, size_t outRoom, size_t *written, uint32_t now)
	{
		*written = 0;

		if (step != kSealed)
		{
			return false;
		}

		if (sealed.Seal(datagram, size, out, outRoom, written, now))
		{
			return true;
		}

		EndIfWornOut();   // past 2^32 datagrams or 24 hours: the session is over, and a new handshake must start
		return false;
	}

	// Whether the keys are in: B4 checked, so ENet may connect
	bool Ready() const
	{
		return step == kSealed && sealed.Live();
	}

	bool Failed() const
	{
		return step == kFailed;
	}

	int Why() const
	{
		return why;
	}

	// The 6 digits for the join screen, once Ready; empty before
	const char *CheckCode() const
	{
		return code;
	}

	const NetPeerDrops &Drops() const
	{
		return drops;
	}

	const NetSealedGame &Sealed() const
	{
		return sealed;
	}

	// Ends everything: keys wiped, nothing more sent
	void Reset()
	{
		handshake.Forget();
		sealed.End(kNetSealedGameFine);
		outbox.Clear();
		NetV2_AssemblyClear(&assembly);

		// Modified 2026-11-09 by Melanikus for the AvP port (Snyk Code): what came of the handshake is wiped with NetCrypto_Wipe, which the
		// compiler must keep. A plain memset of memory that's never read again may be left out by an optimising compiler.
		NetCrypto_Wipe(id, sizeof(id));
		NetCrypto_Wipe(code, sizeof(code));
		memset(&drops, 0, sizeof(drops));
		step = kNothing;
		why = kNetHandshakeBFine;
		sentAt = 0;
		waits = 0;
		heardFromHost = false;
	}

private:
	static int const kNothing = 0;
	static int const kWaitB2 = 1;
	static int const kWaitB4 = 2;
	static int const kSealed = 3;
	static int const kFailed = 4;

	void Sent(uint32_t now)
	{
		sentAt = now;
		waits = 0;
	}

	bool Fail(int reason)
	{
		handshake.Abandon(reason);
		sealed.End(reason);
		outbox.Clear();
		NetV2_AssemblyClear(&assembly);
		why = reason;
		step = kFailed;
		return false;
	}

	void EndIfWornOut()
	{
		if (step == kSealed && !sealed.Live())
		{
			Fail(sealed.Ended());
		}
	}

	void TakeB2(uint32_t now)
	{
		uint8_t b3[kNetHandshakeBB3Bytes];
		uint8_t ciphertext[kNetPqKemCiphertextBytes], secret[kNetPqKemSecretBytes];
		bool const read = handshake.ReadB2(assembly.message, assembly.length);
		NetV2_AssemblyClear(&assembly);

		if (!read)
		{
			Fail(handshake.Why());
			return;
		}

		// The draw: an encapsulation to the host's own ML-KEM key
		if (!NetPq_KemEncapsulate(handshake.HostKemPublic(), ciphertext, secret))
		{
			Fail(kNetHandshakeBNotAKey);
			return;
		}

		bool const written = handshake.WriteB3(ciphertext, secret, b3);
		NetCrypto_Wipe(secret, sizeof(secret));

		if (!written || !outbox.Fill(kNetHandshakeBKindB3, id, b3, sizeof(b3)))
		{
			Fail(written ? kNetHandshakeBCouldnt : handshake.Why());
			return;
		}

		step = kWaitB4;
		Sent(now);
	}

	void TakeB4(uint32_t now)
	{
		uint8_t b5[kNetHandshakeBB5Bytes];
		NetHandshakeBKeys keys;
		bool const read = handshake.ReadB4(assembly.message, assembly.length, b5, &keys);
		NetV2_AssemblyClear(&assembly);

		if (!read)
		{
			Fail(handshake.Why());
			return;
		}

		bool const ready = NetHandshakeB_CheckCode(keys.transcript, code) && outbox.Fill(kNetHandshakeBKindB5, id, b5, sizeof(b5));

		if (ready)
		{
			sealed.Start(keys.split, true, now);
		}

		NetHandshakeB_WipeKeys(&keys);

		if (!ready)
		{
			Fail(kNetHandshakeBCouldnt);
			return;
		}

		step = kSealed;
		Sent(now);
	}

	void HearSealed(const uint8_t *datagram, size_t size, uint8_t *out, size_t outRoom, size_t *written, uint32_t now)
	{
		if (step != kSealed)
		{
			drops.sealedEarly++;
			return;
		}

		if (!sealed.Open(datagram, size, out, outRoom, written, now))
		{
			drops.sealedBad++;
			EndIfWornOut();
			return;
		}

		// The host's first sealed datagram: it has B5, so B5 needn't go again
		if (!heardFromHost)
		{
			heardFromHost = true;
			outbox.Clear();
		}
	}

	NetHandshakeBJoiner handshake;
	NetSealedGame sealed;
	NetPeerOutbox outbox;
	NetV2Assembly assembly;
	uint8_t id[kNetV2IdBytes];
	char code[kNetHandshakeBCheckCodeBytes];
	NetPeerDrops drops;
	int step;
	int why;
	uint32_t sentAt;
	size_t waits;
	bool heardFromHost;
};

// ----- The host's side, one for each joiner's port

class NetPeerHostSide
{
public:
	NetPeerHostSide()
	{
		Reset();
	}

	~NetPeerHostSide()
	{
		Reset();
	}

	NetPeerHostSide(const NetPeerHostSide &) = delete;
	NetPeerHostSide &operator=(const NetPeerHostSide &) = delete;

	// A joiner is coming on relayPort of this host's region's relay (NEW_JOINER). identity lasts as long as this.
	bool Start(const char *region, uint16_t relayPort, const NetIdentity *identity, uint32_t now)
	{
		Reset();

		if (NetHandshakeB_RegionLength(region) == 0 || relayPort == 0 || !identity || !identity->made)
		{
			return Fail(kNetHandshakeBBadStart);
		}

		memcpy(regionName, region, NetHandshakeB_RegionLength(region) + 1);
		port = relayPort;
		me = identity;
		startedAt = now;
		step = kWaitB1;
		return true;
	}

	// A datagram from this joiner's relay port; as NetPeerJoiner::Hear
	void Hear(const uint8_t *datagram, size_t size, uint8_t *out, size_t outRoom, size_t *written, uint32_t now)
	{
		*written = 0;

		if (!datagram || size < 4)
		{
			drops.notOurs++;
			return;
		}

		if (memcmp(datagram, kNetSealedGameMagic, 4) == 0)
		{
			HearSealed(datagram, size, out, outRoom, written, now);
			return;
		}

		switch (step)
		{
			case kWaitB1:
				// The first fragment fixes the handshake's id: the joiner chose it
				if (NetPeer_TakeFragment(datagram, size, kNetHandshakeBKindB1, NULL, true, &assembly, &drops) == 1)
				{
					TakeB1(now);
				}

				break;

			case kWaitB3:
				HearAgainOr(datagram, size, kNetHandshakeBKindB1, kNetHandshakeBKindB3, b1Kept, sizeof(b1Kept), now);
				break;

			case kWaitB5:
				HearAgainOr(datagram, size, kNetHandshakeBKindB3, kNetHandshakeBKindB5, b3Kept, sizeof(b3Kept), now);
				break;

			default:
				drops.notNow++;   // B5 sent again before the joiner has heard us, among others: harmless
				break;
		}
	}

	// Time goes by: a joiner that hasn't sent B1 within 10 s of the relay's word, or the next message within the 10 s the last answer is
	// kept, has gone
	void Tick(uint32_t now)
	{
		bool const waiting = step == kWaitB1 || step == kWaitB3 || step == kWaitB5;
		uint32_t const since = step == kWaitB1 ? startedAt : answeredAt;

		if (waiting && static_cast<uint32_t>(now - since) >= kNetPeerKeepAnswerMs)
		{
			Fail(kNetPeerNoAnswer);
		}
	}

	size_t TakeSends(NetPeerSend *out, size_t room)
	{
		return outbox.Take(out, room);
	}

	// Seals one of ENet's datagrams for this joiner. False, and nothing to send, before B5 has checked or after the end.
	bool Seal(const uint8_t *datagram, size_t size, uint8_t *out, size_t outRoom, size_t *written, uint32_t now)
	{
		*written = 0;

		if (step != kSealed)
		{
			return false;
		}

		if (sealed.Seal(datagram, size, out, outRoom, written, now))
		{
			return true;
		}

		EndIfWornOut();   // past 2^32 datagrams or 24 hours: the session is over, and a new handshake must start
		return false;
	}

	bool Ready() const
	{
		return step == kSealed && sealed.Live();
	}

	bool Failed() const
	{
		return step == kFailed;
	}

	int Why() const
	{
		return why;
	}

	const char *CheckCode() const
	{
		return code;
	}

	// The joiner's fingerprint, once Ready (to welcome back, or to ban); all 0 before
	const uint8_t *JoinerFingerprint() const
	{
		return joinerFingerprint;
	}

	const NetPeerDrops &Drops() const
	{
		return drops;
	}

	const NetSealedGame &Sealed() const
	{
		return sealed;
	}

	uint16_t Port() const
	{
		return port;
	}

	void Reset()
	{
		handshake.Forget();
		sealed.End(kNetSealedGameFine);
		outbox.Clear();
		NetV2_AssemblyClear(&assembly);

		// Modified 2026-11-09 by Melanikus for the AvP port (Snyk Code): what came of the handshake is wiped with NetCrypto_Wipe, which the
		// compiler must keep. A plain memset of memory that's never read again may be left out by an optimising compiler.
		NetCrypto_Wipe(id, sizeof(id));
		NetCrypto_Wipe(code, sizeof(code));
		NetCrypto_Wipe(regionName, sizeof(regionName));
		NetCrypto_Wipe(joinerFingerprint, sizeof(joinerFingerprint));
		NetCrypto_Wipe(b1Kept, sizeof(b1Kept));
		NetCrypto_Wipe(b3Kept, sizeof(b3Kept));
		memset(&drops, 0, sizeof(drops));
		me = NULL;
		port = 0;
		step = kNothing;
		why = kNetHandshakeBFine;
		startedAt = 0;
		answeredAt = 0;
	}

private:
	static int const kNothing = 0;
	static int const kWaitB1 = 1;
	static int const kWaitB3 = 2;
	static int const kWaitB5 = 3;
	static int const kSealed = 4;
	static int const kFailed = 5;

	bool Fail(int reason)
	{
		handshake.Abandon(reason);
		sealed.End(reason);
		outbox.Clear();
		NetV2_AssemblyClear(&assembly);
		why = reason;
		step = kFailed;
		return false;
	}

	void EndIfWornOut()
	{
		if (step == kSealed && !sealed.Live())
		{
			Fail(sealed.Ended());
		}
	}

	// While waiting for the next message: the last message again (the same id, the same bytes) gets the kept answer again, byte for
	// byte; the next message is taken; anything else is dropped. A repeat is put together apart from the next message, in its own
	// assembly, so the two can come mixed.
	void HearAgainOr(const uint8_t *datagram, size_t size, uint8_t againKind, uint8_t nextKind, const uint8_t *kept, size_t keptSize, uint32_t now)
	{
		NetV2Fragment peek;

		if (NetV2_ReadFragment(kNetV2MagicEndToEnd, datagram, size, &peek) && peek.kind == againKind)
		{
			int const got = NetPeer_TakeFragment(datagram, size, againKind, id, false, &repeat, &drops);

			if (got == 1)
			{
				bool const same = repeat.length == keptSize && memcmp(repeat.message, kept, keptSize) == 0;
				NetV2_AssemblyClear(&repeat);

				if (same && static_cast<uint32_t>(now - answeredAt) < kNetPeerKeepAnswerMs)
				{
					outbox.Again();
				}
				else
				{
					drops.notNow++;
				}
			}

			return;
		}

		if (NetPeer_TakeFragment(datagram, size, nextKind, id, false, &assembly, &drops) == 1)
		{
			if (nextKind == kNetHandshakeBKindB3)
			{
				TakeB3(now);
			}
			else
			{
				TakeB5(now);
			}
		}
	}

	void TakeB1(uint32_t now)
	{
		memcpy(id, assembly.id, sizeof(id));
		bool const read = handshake.ReadB1(regionName, port, me, assembly.message, assembly.length);

		if (read)
		{
			memcpy(b1Kept, assembly.message, sizeof(b1Kept));
		}

		NetV2_AssemblyClear(&assembly);

		if (!read)
		{
			Fail(handshake.Why());
			return;
		}

		// The draws, in 5.1 point 14's order: e's P-384 key, then the encapsulation to the joiner's fresh ML-KEM key
		uint8_t rePublic[kNetCryptoP384PublicBytes], ciphertext[kNetPqKemCiphertextBytes], secret[kNetPqKemSecretBytes];
		uint8_t b2[kNetHandshakeBB2Bytes];
		NetEcdhKey *const re = NetCrypto_EcdhNew(rePublic);

		if (!re)
		{
			Fail(kNetPeerCouldntDraw);
			return;
		}

		if (!NetPq_KemEncapsulate(handshake.JoinerEphemeralKemPublic(), ciphertext, secret))
		{
			NetCrypto_EcdhFree(re);
			Fail(kNetHandshakeBNotAKey);   // FIPS 203 refused the joiner's key
			return;
		}

		bool const written = handshake.WriteB2(re, rePublic, ciphertext, secret, b2);
		NetCrypto_Wipe(secret, sizeof(secret));

		if (!written || !outbox.Fill(kNetHandshakeBKindB2, id, b2, sizeof(b2)))
		{
			Fail(written ? kNetHandshakeBCouldnt : handshake.Why());
			return;
		}

		step = kWaitB3;
		answeredAt = now;
	}

	void TakeB3(uint32_t now)
	{
		bool const read = handshake.ReadB3(assembly.message, assembly.length);

		if (read)
		{
			memcpy(b3Kept, assembly.message, sizeof(b3Kept));
		}

		NetV2_AssemblyClear(&assembly);

		if (!read)
		{
			Fail(handshake.Why());
			return;
		}

		// The draws: the encapsulation to the joiner's own ML-KEM key, then nH
		uint8_t ciphertext[kNetPqKemCiphertextBytes], secret[kNetPqKemSecretBytes], nH[kNetHandshakeBNonceBytes];
		uint8_t b4[kNetHandshakeBB4Bytes];

		if (!NetPq_KemEncapsulate(handshake.JoinerKemPublic(), ciphertext, secret))
		{
			Fail(kNetHandshakeBNotAKey);
			return;
		}

		if (!NetCrypto_Random(nH, sizeof(nH)))
		{
			NetCrypto_Wipe(secret, sizeof(secret));
			Fail(kNetPeerCouldntDraw);
			return;
		}

		bool const written = handshake.WriteB4(ciphertext, secret, nH, b4);
		NetCrypto_Wipe(secret, sizeof(secret));
		NetCrypto_Wipe(nH, sizeof(nH));

		if (!written || !outbox.Fill(kNetHandshakeBKindB4, id, b4, sizeof(b4)))
		{
			Fail(written ? kNetHandshakeBCouldnt : handshake.Why());
			return;
		}

		step = kWaitB5;
		answeredAt = now;
	}

	void TakeB5(uint32_t now)
	{
		NetHandshakeBKeys keys;
		bool const read = handshake.ReadB5(assembly.message, assembly.length, &keys);
		NetV2_AssemblyClear(&assembly);

		if (!read)
		{
			Fail(handshake.Why());
			return;
		}

		bool const coded = NetHandshakeB_CheckCode(keys.transcript, code);

		if (coded)
		{
			sealed.Start(keys.split, false, now);
			memcpy(joinerFingerprint, keys.theirFingerprint, sizeof(joinerFingerprint));
		}

		NetHandshakeB_WipeKeys(&keys);
		outbox.Clear();

		if (!coded)
		{
			Fail(kNetHandshakeBCouldnt);
			return;
		}

		step = kSealed;
	}

	void HearSealed(const uint8_t *datagram, size_t size, uint8_t *out, size_t outRoom, size_t *written, uint32_t now)
	{
		if (step != kSealed)
		{
			drops.sealedEarly++;
			return;
		}

		if (!sealed.Open(datagram, size, out, outRoom, written, now))
		{
			drops.sealedBad++;
			EndIfWornOut();
		}
	}

	NetHandshakeBHost handshake;
	NetSealedGame sealed;
	NetPeerOutbox outbox;
	NetV2Assembly assembly;
	NetV2Assembly repeat;
	uint8_t id[kNetV2IdBytes];
	char code[kNetHandshakeBCheckCodeBytes];
	char regionName[kNetHandshakeBMostRegionBytes + 1];
	uint8_t joinerFingerprint[kNetIdentityFingerprintBytes];
	uint8_t b1Kept[kNetHandshakeBB1Bytes];
	uint8_t b3Kept[kNetHandshakeBB3Bytes];
	NetPeerDrops drops;
	const NetIdentity *me;
	uint16_t port;
	int step;
	int why;
	uint32_t startedAt;
	uint32_t answeredAt;
};

// ----- How often a host starts a handshake. Added 2026-11-09 by Melanikus for the AvP port (Primary's read of the wiring, finding 1).
//
// Each handshake a host starts costs it one B2 to work out (an ML-KEM encapsulation and two P-384 agreements), and a handshake that fails
// frees its place for another. So without a limit one joiner could send a good B1, then a bad B3, and again, as fast as the relay passes
// datagrams, and the host's game would stutter. With this:
//   - a port the relay has named (NEW_JOINER) starts at most kNetPeerMostStartsPerNaming handshakes, and then nothing more until the relay
//     names it again (Forget, once it isn't named any more): an honest joiner needs one, and a second if a datagram went astray;
//   - the host starts at most kNetPeerMostStartsPerSecond handshakes in any one second, from all ports together.
// It's game-free, as the rest of this file: the caller gives the time, and asks Allow before each start. Its test is net_v2b_test's section 9.
// What's left (Primary's read, 2026-11-09): 8 named ports at 3 starts each could use the host's 4 a second for about 6 seconds, and an
// honest joiner starting then has its B1 refused; it sends B1 again (0.5, 1.5 and 3.5 s), so it only waits.
uint32_t const kNetPeerMostStartsPerNaming = 3;
uint32_t const kNetPeerMostStartsPerSecond = 4;
size_t const kNetPeerStartLimitPorts = 8;   // a room's joiners (net_relay.h's kNetRelayMostJoiners), so every named port has a place

class NetPeerStartLimit
{
public:
	NetPeerStartLimit()
	{
		Reset();
	}

	// May a handshake start now for this port? True counts it as started. False leaves everything as it was, and counts the refusal.
	bool Allow(uint16_t relayPort, uint32_t now)
	{
		if (relayPort == 0)
		{
			refused++;
			return false;
		}

		if (!secondRunning || static_cast<uint32_t>(now - secondStartedAt) >= 1000)
		{
			secondRunning = true;
			secondStartedAt = now;
			startsThisSecond = 0;
		}

		Entry *entry = NULL;
		Entry *unused = NULL;

		for (size_t i = 0; i < kNetPeerStartLimitPorts; i++)
		{
			if (entries[i].port == relayPort)
			{
				entry = &entries[i];
			}
			else if (!unused && entries[i].port == 0)
			{
				unused = &entries[i];
			}
		}

		uint32_t const startsSoFar = entry ? entry->starts : 0;

		if ((!entry && !unused) || startsSoFar >= kNetPeerMostStartsPerNaming || startsThisSecond >= kNetPeerMostStartsPerSecond)
		{
			refused++;
			return false;
		}

		if (!entry)
		{
			entry = unused;
			entry->port = relayPort;
			entry->starts = 0;
		}

		entry->starts++;
		startsThisSecond++;
		return true;
	}

	// The relay doesn't name this port any more: its count goes, so the next time it's named it starts afresh
	void Forget(uint16_t relayPort)
	{
		for (size_t i = 0; i < kNetPeerStartLimitPorts; i++)
		{
			if (relayPort != 0 && entries[i].port == relayPort)
			{
				entries[i].port = 0;
				entries[i].starts = 0;
			}
		}
	}

	// Each port it still counts, for the caller to ask whether the relay still names it
	uint16_t PortAt(size_t i) const
	{
		return i < kNetPeerStartLimitPorts ? entries[i].port : 0;
	}

	uint32_t Refused() const
	{
		return refused;
	}

	void Reset()
	{
		memset(entries, 0, sizeof(entries));
		secondRunning = false;
		secondStartedAt = 0;
		startsThisSecond = 0;
		refused = 0;
	}

private:
	struct Entry
	{
		uint16_t port;     // 0 for none
		uint32_t starts;   // handshakes started for it since the relay named it
	};

	Entry entries[kNetPeerStartLimitPorts];
	bool secondRunning;
	uint32_t secondStartedAt;
	uint32_t startsThisSecond;
	uint32_t refused;
};

#endif
