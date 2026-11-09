// A game's protocol version 2 session with one relay: the cookie round, handshake A with its resends, then the sealed control channel.
// Written 2026-11-09 by Melanikus for the AvP port.
//
// WHAT IT DOES, as the game sees it (CRYPTO.md sections 6 and 7, in the AVPx-Dedicated-Server repository):
//
//      A0, COOKIE_ASK, 1,200 bytes ----------> the relay's control port
//      A0', COOKIE, 64 bytes <---------------- (proof the game is at its address, as in version 1)
//      A1, 3 datagrams, with the cookie ------>
//      A2, 2 datagrams <---------------------- the relay is proved: only the holder of its static keys can make one that opens
//      A3, 1 datagram, with the cookie ------> the game's key confirmation
//      ... from then on, every version 1 message (HOST_OPEN, JOIN, the hellos, ...) goes sealed, 104 bytes, both ways
//
// Like net_relay.h's machines, this one is told the time and what arrived, and says what to send. It never sends or reads anything
// itself, and draws no randomness: the game hands in the handshake's id and its draws (net_handshake_a.h), so its test can hand in the
// test vectors' instead. Nothing here knows about the game, so tests/net_v2a_test.cpp can try every case.
//
// LOSSES: a message gets no answer, so it goes again: after 0.5 s, then 1 s and 2 s more (at 0.5, 1.5 and 3.5 s from its first send),
// and 4 s after that, at 7.5 s, the session gives up, "the relay doesn't answer" (CRYPTO.md sections 6 and 17). A0 and A1 each have their
// own count from their first send. A3 goes once: if it's lost, the game's first sealed message stands in for it (section 6).
//
// EVERYTHING THAT ARRIVES IS HOSTILE (AvP Tao 3). A0' and A2 are taken only from the relay's control port, for this handshake's id, at
// their exact sizes; A2's datagrams must carry a cookie of 16 zeros, as the relay sends them (anything else is refusal 5). A2 that
// doesn't open ends the handshake for good, as section 5 says. Sealed messages are taken from any of the relay's ports (READY comes
// sealed from the joiner's port), but only ones that open under this session's keys, and only once each.

#ifndef NET_RELAY_V2_H
#define NET_RELAY_V2_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "net_crypto.h"
#include "net_v2_fragments.h"
#include "net_handshake_a.h"
#include "net_sealed_control.h"

size_t const kNetRelayV2MostSends = 8;        // what can wait to be sent at once: A1's 3 datagrams, and sealed messages
size_t const kNetRelayV2MostOpened = 8;       // sealed messages opened and not yet taken
size_t const kNetRelayV2A1Fragments = 3;      // A1's 3,249 bytes
size_t const kNetRelayV2A2Fragments = 2;      // A2's 1,689 bytes
uint32_t const kNetRelayV2GiveUpMs = 7500;
static const uint32_t kNetRelayV2ResendAtMs[3] = { 500, 1500, 3500 };

// Why a session ended: CRYPTO.md section 16's numbers where it has one, or net_handshake_a.h's
int const kNetRelayV2NoAnswer = 7;            // "the relay doesn't answer"
int const kNetRelayV2BadCookie = 5;           // an A2 datagram whose cookie isn't 16 zeros

// One datagram to send to the relay, at one of its ports
struct NetRelayV2Send
{
	uint16_t toPort;
	size_t size;
	uint8_t bytes[kNetV2DatagramBytes];
};

// One sealed message from the relay, opened, with the port it came from (net_relay.h's machines check that)
struct NetRelayV2Opened
{
	uint16_t fromPort;
	uint8_t message[kNetSealedControlMessageBytes];
};

// What was dropped, by reason, for the log: counts only
struct NetRelayV2Drops
{
	uint32_t notFromControl;   // a handshake datagram from a port other than the control port
	uint32_t notOurs;          // not this handshake's: another id, kind, shape or size
	uint32_t refused;          // a sealed message that didn't open, or came again
	uint32_t full;             // nowhere to keep it: the game didn't take what came before
};

class NetRelayV2Session
{
public:
	// A new session is all zeros. Not Wipe(): that frees e's key inside CNG, and in a new object that pointer isn't one yet.
	NetRelayV2Session()
	{
		NetCrypto_Wipe(this, sizeof(*this));
	}

	~NetRelayV2Session()
	{
		Wipe();
	}

	// Starts a session with the relay at controlPort, whose static keys are relay (checked against its fingerprint), in region. id is the
	// handshake's 8 random bytes, and draws its keys (both drawn by the game). The draws are taken over. False if any of it isn't good.
	bool Start(const char *region, const NetRelayStaticKeys *relayKeys, uint16_t controlPort, const uint8_t id[kNetV2IdBytes],
		NetHandshakeADraws *draws, uint32_t now)
	{
		Wipe();

		if (!region || !NetHandshakeA_RegionIsGood(region) || !relayKeys || controlPort == 0 || !id || !draws || !draws->e)
		{
			NetHandshakeA_WipeDraws(draws);
			step = kFailed;
			why = kNetHandshakeABadState;
			return false;
		}

		memcpy(regionName, region, strlen(region) + 1);
		relay = *relayKeys;
		port = controlPort;
		memcpy(handshakeId, id, sizeof(handshakeId));
		drawn = *draws;
		draws->e = NULL;
		NetHandshakeA_WipeDraws(draws);
		NetV2_CookieAsk(handshakeId, a0);
		step = kAskCookie;
		Phase(now);
		Queue(port, a0, sizeof(a0));
		return true;
	}

	// A datagram from the relay's address, from fromPort
	void Hear(uint16_t fromPort, const uint8_t *data, size_t size, uint32_t now)
	{
		if (!data)
		{
			return;
		}

		if (step == kAskCookie)
		{
			HearCookie(fromPort, data, size, now);
		}
		else if (step == kWaitA2)
		{
			HearA2(fromPort, data, size, now);
		}
		else if (step == kReady)
		{
			HearSealed(fromPort, data, size, now);
		}
	}

	// Time goes by: sends A0 or A1 again when its wait is over, and gives up after the last
	void Tick(uint32_t now)
	{
		if (step != kAskCookie && step != kWaitA2)
		{
			return;
		}

		uint32_t const since = now - phaseStarted;

		if (since >= kNetRelayV2GiveUpMs)
		{
			Fail(kNetRelayV2NoAnswer);
			return;
		}

		if (resends < 3 && since >= kNetRelayV2ResendAtMs[resends])
		{
			resends++;
			SendPhaseAgain();
		}
	}

	// A version 1 message (64 bytes), sealed for the relay's port toPort. False if the session isn't ready or is over (then it's ended).
	bool Seal(const uint8_t message[kNetSealedControlMessageBytes], uint16_t toPort, uint32_t now)
	{
		if (step != kReady || toPort == 0 || sendCount >= kNetRelayV2MostSends)
		{
			return false;
		}

		uint8_t sealedBytes[kNetSealedControlBytes];

		if (!NetSealedControl_Seal(&channel, message, sealedBytes, now))
		{
			Fail(kNetSealedOver);
			return false;
		}

		Queue(toPort, sealedBytes, sizeof(sealedBytes));
		return true;
	}

	// What to send now, oldest first, into out (room for most). Gives back how many.
	size_t TakeSends(NetRelayV2Send *out, size_t most)
	{
		size_t taken = 0;

		for (; taken < sendCount && taken < most && out; taken++)
		{
			out[taken] = sends[taken];
		}

		// What wasn't taken moves up
		for (size_t i = taken; i < sendCount; i++)
		{
			sends[i - taken] = sends[i];
		}

		sendCount -= taken;
		NetCrypto_Wipe(sends + sendCount, sizeof(sends[0]) * (kNetRelayV2MostSends - sendCount));
		return taken;
	}

	// The sealed messages opened since last time, oldest first, into out (room for most). Gives back how many.
	size_t TakeOpened(NetRelayV2Opened *out, size_t most)
	{
		size_t taken = 0;

		for (; taken < openedCount && taken < most && out; taken++)
		{
			out[taken] = opened[taken];
		}

		for (size_t i = taken; i < openedCount; i++)
		{
			opened[i - taken] = opened[i];
		}

		openedCount -= taken;
		NetCrypto_Wipe(opened + openedCount, sizeof(opened[0]) * (kNetRelayV2MostOpened - openedCount));
		return taken;
	}

	bool Ready() const
	{
		return step == kReady;
	}

	bool Failed() const
	{
		return step == kFailed;
	}

	// Why it failed (0 while it hasn't)
	int Why() const
	{
		return why;
	}

	const NetRelayV2Drops &Drops() const
	{
		return drops;
	}

	// Every key and secret wiped, and the session back to nothing
	void Wipe()
	{
		NetHandshakeA_Wipe(&handshake);
		NetHandshakeA_WipeDraws(&drawn);
		NetSealedControl_Wipe(&channel);
		NetCrypto_Wipe(this, sizeof(*this));
	}

private:
	static int const kNone = 0;
	static int const kAskCookie = 1;
	static int const kWaitA2 = 2;
	static int const kReady = 3;
	static int const kFailed = 4;

	void Phase(uint32_t now)
	{
		phaseStarted = now;
		resends = 0;
	}

	void Queue(uint16_t toPort, const uint8_t *bytes, size_t size)
	{
		if (sendCount >= kNetRelayV2MostSends || size > sizeof(sends[0].bytes))
		{
			drops.full++;
			return;
		}

		sends[sendCount].toPort = toPort;
		sends[sendCount].size = size;
		memcpy(sends[sendCount].bytes, bytes, size);
		sendCount++;
	}

	void SendPhaseAgain()
	{
		if (step == kAskCookie)
		{
			Queue(port, a0, sizeof(a0));
			return;
		}

		for (size_t i = 0; i < kNetRelayV2A1Fragments; i++)
		{
			Queue(port, a1[i], sizeof(a1[i]));
		}
	}

	void Fail(int reason)
	{
		Wipe();
		step = kFailed;
		why = reason;
	}

	// A0': the cookie, then A1 at once
	void HearCookie(uint16_t fromPort, const uint8_t *data, size_t size, uint32_t now)
	{
		if (fromPort != port)
		{
			drops.notFromControl++;
			return;
		}

		if (!NetV2_ReadCookie(data, size, handshakeId, cookie))
		{
			drops.notOurs++;
			return;
		}

		int const written = NetHandshakeA_WriteA1(&handshake, regionName, &relay, &drawn, NULL);

		if (written != kNetHandshakeAOk)
		{
			Fail(written);
			return;
		}

		for (size_t i = 0; i < kNetRelayV2A1Fragments; i++)
		{
			if (!NetV2_CutFragment(kNetV2MagicRelay, kNetV2KindA1, handshakeId, cookie, handshake.a1, sizeof(handshake.a1), i, a1[i]))
			{
				Fail(kNetHandshakeABadState);
				return;
			}
		}

		NetV2_AssemblyClear(&a2);
		step = kWaitA2;
		Phase(now);
		SendPhaseAgain();
	}

	// A2's datagrams, put together; then A3, and the sealed channel
	void HearA2(uint16_t fromPort, const uint8_t *data, size_t size, uint32_t now)
	{
		if (fromPort != port)
		{
			drops.notFromControl++;
			return;
		}

		NetV2Fragment f;
		static const uint8_t kNoCookie[kNetV2CookieBytes] = {};

		if (!NetV2_ReadFragment(kNetV2MagicRelay, data, size, &f) || f.kind != kNetV2KindA2 || memcmp(f.id, handshakeId, kNetV2IdBytes) != 0 ||
			f.count != kNetRelayV2A2Fragments || f.length != kNetHandshakeA2Bytes)
		{
			drops.notOurs++;
			return;
		}

		if (memcmp(f.cookie, kNoCookie, kNetV2CookieBytes) != 0)
		{
			Fail(kNetRelayV2BadCookie);
			return;
		}

		if (!NetV2_AssemblyAdd(&a2, &f))
		{
			drops.notOurs++;
			return;
		}

		if (!NetV2_AssemblyComplete(&a2))
		{
			return;
		}

		uint8_t a3[kNetHandshakeA3Bytes];
		NetHandshakeAKeys keys;
		int const read = NetHandshakeA_ReadA2(&handshake, a2.message, a2.length, a3, &keys, NULL);
		NetV2_AssemblyClear(&a2);

		if (read != kNetHandshakeAOk)
		{
			Fail(read);
			return;
		}

		uint8_t a3Datagram[kNetV2DatagramBytes];
		bool const cut = NetV2_CutFragment(kNetV2MagicRelay, kNetV2KindA3, handshakeId, cookie, a3, sizeof(a3), 0, a3Datagram);
		NetSealedControl_Start(&channel, &keys, now);
		NetHandshakeA_WipeKeys(&keys);
		NetCrypto_Wipe(a3, sizeof(a3));

		if (!cut)
		{
			Fail(kNetHandshakeABadState);
			return;
		}

		step = kReady;
		Queue(port, a3Datagram, sizeof(a3Datagram));
	}

	// A sealed message, from any of the relay's ports; anything else now (A2 sent again by the relay, say) is no longer needed
	void HearSealed(uint16_t fromPort, const uint8_t *data, size_t size, uint32_t now)
	{
		if (size != kNetSealedControlBytes)
		{
			drops.notOurs++;
			return;
		}

		uint8_t message[kNetSealedControlMessageBytes];
		int const result = NetSealedControl_Open(&channel, data, size, message, now);

		if (result == kNetSealedOver)
		{
			Fail(kNetSealedOver);
			return;
		}

		if (result != kNetSealedOk)
		{
			drops.refused++;
			return;
		}

		if (openedCount >= kNetRelayV2MostOpened)
		{
			drops.full++;
			NetCrypto_Wipe(message, sizeof(message));
			return;
		}

		opened[openedCount].fromPort = fromPort;
		memcpy(opened[openedCount].message, message, sizeof(message));
		openedCount++;
		NetCrypto_Wipe(message, sizeof(message));
	}

	int step;
	int why;
	char regionName[kNetHandshakeMostRegionLetters + 1];
	NetRelayStaticKeys relay;
	uint16_t port;
	uint8_t handshakeId[kNetV2IdBytes];
	uint8_t cookie[kNetV2CookieBytes];
	NetHandshakeADraws drawn;
	NetHandshakeA handshake;
	uint8_t a0[kNetV2DatagramBytes];
	uint8_t a1[kNetRelayV2A1Fragments][kNetV2DatagramBytes];
	NetV2Assembly a2;
	NetSealedControl channel;
	uint32_t phaseStarted;
	size_t resends;
	NetRelayV2Send sends[kNetRelayV2MostSends];
	size_t sendCount;
	NetRelayV2Opened opened[kNetRelayV2MostOpened];
	size_t openedCount;
	NetRelayV2Drops drops;
};

#endif
