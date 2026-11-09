// Sealed game traffic of protocol version 2: every datagram the game's ENet sends between a joiner and its host, sealed end to end with
// the keys handshake B gave them (CRYPTO.md section 9, in the AVPx-Dedicated-Server repository). Written 2026-11-09 by Melanikus for the
// AvP port.
//
// WHAT IT DOES, in plain words. Once handshake B is done (net_handshake_b.h), the joiner and the host each have two keys, one for each
// direction. Every game datagram is then locked with the sender's key before it leaves, and unlocked by the other side when it comes in.
// The relay in the middle passes the locked bytes on and can't read or change them: a datagram changed by one bit, or sent again, or made
// up, fails to unlock and is thrown away, counted, and never answered.
//
// ONE SEALED DATAGRAM:
//   bytes 0-3             'AVPG'
//   bytes 4-11            the counter: 0 for the first datagram in this direction, then 1, 2, ..., 8 bytes little-endian
//   bytes 12 to end - 16  the ENet datagram, sealed (AES-256-GCM: the same length as it was)
//   the last 16 bytes     the tag, the proof that nothing above was changed
// The 12 bytes in front are the "associated data": not hidden, but covered by the tag. So 28 bytes are added to each datagram, and the
// whole is at most 1,500 bytes, the most the relay passes on: ENet's own datagrams are at most 1,392 (its default MTU), 1,420 sealed.
//
// THE KEYS (net_v2_direction.h, Primary's): each direction's key changes every 2^20 datagrams (an "epoch"), with no message, at the same
// counter on both sides, so no key ever seals more than 2^20. The nonce is the direction's IV with the counter mixed in, so no two
// datagrams are ever sealed with the same nonce and key. The joiner sends with Split's "initiator to responder" key, and the host with
// "responder to initiator": the joiner is handshake B's initiator.
//
// THE LIMITS (CRYPTO.md section 17):
//   - a datagram already taken, or older than the last 1,024, is refused (refusal 9): the replay window (net_v2_replay.h, Primary's),
//     which marks a counter taken only after its tag has checked;
//   - after 65,536 tags that fail, the session ends ("too many bad packets", refusal 16), so nobody can keep trying forgeries forever;
//   - after 2^32 datagrams either way, or 24 hours, the session ends, and a new handshake starts. At 60 datagrams a second the counter
//     limit is two years away, so in practice it's the 24 hours.
//
// NO PLAINTEXT, EVER. There's no way to send a datagram unsealed, or to take one in unsealed: before Start, after End, and after any limit,
// Seal and Open refuse everything. There's no switch to turn sealing off.
//
// It's game-free, as net_noise.h is: the caller gives the time, in milliseconds from any clock that only goes forward, and does the
// sending and receiving. Its test is tests/net_v2b_test.cpp. Nothing here writes anything to the log.

#ifndef NET_SEALED_GAME_H
#define NET_SEALED_GAME_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "net_crypto.h"
#include "net_noise.h"
#include "net_v2_direction.h"
#include "net_v2_replay.h"

static const uint8_t kNetSealedGameMagic[4] = { 'A', 'V', 'P', 'G' };

size_t const kNetSealedGameHeaderBytes = 12;                                                    // 'AVPG' and the counter
size_t const kNetSealedGameOverheadBytes = kNetSealedGameHeaderBytes + kNetCryptoTagBytes;     // 28
size_t const kNetSealedGameMostBytes = 1500;                                                    // the most the relay passes on
size_t const kNetSealedGameMostTextBytes = kNetSealedGameMostBytes - kNetSealedGameOverheadBytes;   // 1,472
uint64_t const kNetSealedGameMostDatagrams = static_cast<uint64_t>(1) << 32;                   // either way, in one session
uint32_t const kNetSealedGameMostMs = 24u * 60u * 60u * 1000u;                                  // 24 hours
uint32_t const kNetSealedGameMostBadTags = 65536;

// Why a datagram was refused, or why the session ended. The numbers up to 16 are CRYPTO.md section 16's; it has none for the others.
int const kNetSealedGameFine = 0;
int const kNetSealedGameWrongSize = 1;        // under 28 bytes, or over 1,500
int const kNetSealedGameOtherMagic = 2;       // not 'AVPG'
int const kNetSealedGameBadTag = 8;
int const kNetSealedGameSeenAlready = 9;      // a counter taken already, or below the window
int const kNetSealedGameTooManyBad = 16;      // 65,536 tags failed: the session is over ("the connection was attacked, and closed")
int const kNetSealedGameWornOut = 105;        // 2^32 datagrams or 24 hours: the session is over, and a new handshake starts
int const kNetSealedGameNotLive = 106;        // no session: never started, or ended
int const kNetSealedGamePastTheEnd = 107;     // a counter of 2^32 or more: past a session's last datagram, which no honest side sends
int const kNetSealedGameCouldnt = 103;        // the cryptography itself failed

class NetSealedGame
{
public:
	NetSealedGame()
	{
		Clear();
	}

	~NetSealedGame()
	{
		End(kNetSealedGameNotLive);
	}

	NetSealedGame(const NetSealedGame &) = delete;   // it holds keys, so it's never copied
	NetSealedGame &operator=(const NetSealedGame &) = delete;

	// A session starts with handshake B's keys: joiner says which side this is. now is the time in milliseconds. The caller wipes its
	// own copy of split.
	void Start(const NetNoiseSplit &split, bool joiner, uint32_t now)
	{
		Clear();
		NetV2_DirectionStart(&sending, joiner ? split.initiatorToResponderKey : split.responderToInitiatorKey,
			joiner ? split.initiatorToResponderIv : split.responderToInitiatorIv);
		NetV2_DirectionStart(&receiving, joiner ? split.responderToInitiatorKey : split.initiatorToResponderKey,
			joiner ? split.responderToInitiatorIv : split.initiatorToResponderIv);
		started = now;
		live = true;
	}

	// Seals one of ENet's datagrams (size bytes, at most 1,472) into out, which has room for outRoom bytes: size + 28 of them.
	// False (with out wiped) if there's no session, it's past a limit (and so ends), or there's no room.
	bool Seal(const uint8_t *datagram, size_t size, uint8_t *out, size_t outRoom, size_t *written, uint32_t now)
	{
		*written = 0;

		if (!out || outRoom < size + kNetSealedGameOverheadBytes)
		{
			return Refuse(kNetSealedGameWrongSize, out, outRoom);
		}

		if (!live)
		{
			return Refuse(kNetSealedGameNotLive, out, outRoom);
		}

		if (size > kNetSealedGameMostTextBytes || (size > 0 && !datagram))
		{
			return Refuse(kNetSealedGameWrongSize, out, outRoom);
		}

		if (WornOut(now) || sent >= kNetSealedGameMostDatagrams)
		{
			End(kNetSealedGameWornOut);
			return Refuse(kNetSealedGameWornOut, out, outRoom);
		}

		uint64_t const counter = sent;
		memcpy(out, kNetSealedGameMagic, 4);

		for (size_t i = 0; i < 8; i++)
		{
			out[4 + i] = static_cast<uint8_t>((counter >> (8 * i)) & 0xFF);
		}

		uint8_t key[kNetCryptoAesKeyBytes], nonce[kNetCryptoNonceBytes];
		static const uint8_t kNothing[1] = { 0 };
		bool const sealed = NetV2_DirectionFor(&sending, counter, key, nonce)
			&& NetCrypto_AesGcmSeal(key, nonce, out, kNetSealedGameHeaderBytes, size > 0 ? datagram : kNothing, size, out + kNetSealedGameHeaderBytes,
				out + kNetSealedGameHeaderBytes + size);
		NetCrypto_Wipe(key, sizeof(key));

		if (!sealed)
		{
			return Refuse(kNetSealedGameCouldnt, out, outRoom);
		}

		sent++;
		*written = size + kNetSealedGameOverheadBytes;
		return true;
	}

	// Opens a datagram that came in (size bytes) into out, which has room for outRoom bytes: size - 28 of them, the ENet datagram. False
	// (with out wiped) for anything refused: Why() says why. Nothing refused changes anything but the counts, except the 65,536th tag that
	// fails, which ends the session.
	bool Open(const uint8_t *sealed, size_t size, uint8_t *out, size_t outRoom, size_t *written, uint32_t now)
	{
		*written = 0;

		if (!live)
		{
			return Refuse(kNetSealedGameNotLive, out, outRoom);
		}

		if (WornOut(now))
		{
			End(kNetSealedGameWornOut);
			return Refuse(kNetSealedGameWornOut, out, outRoom);
		}

		if (!sealed || size < kNetSealedGameOverheadBytes || size > kNetSealedGameMostBytes)
		{
			return Refuse(kNetSealedGameWrongSize, out, outRoom);
		}

		if (memcmp(sealed, kNetSealedGameMagic, 4) != 0)
		{
			return Refuse(kNetSealedGameOtherMagic, out, outRoom);
		}

		size_t const textSize = size - kNetSealedGameOverheadBytes;

		if (outRoom < textSize || (textSize > 0 && !out))
		{
			return Refuse(kNetSealedGameWrongSize, out, outRoom);
		}

		uint64_t counter = 0;

		for (size_t i = 8; i > 0; i--)
		{
			counter = (counter << 8) | sealed[4 + i - 1];
		}

		// Past the session's last datagram: no honest sender sends one (it ends its session first)
		if (counter >= kNetSealedGameMostDatagrams)
		{
			return Refuse(kNetSealedGamePastTheEnd, out, outRoom);
		}

		if (!NetV2_ReplayIsNew(&window, counter))
		{
			seenAlready++;
			return Refuse(kNetSealedGameSeenAlready, out, outRoom);
		}

		uint8_t key[kNetCryptoAesKeyBytes], nonce[kNetCryptoNonceBytes];
		uint8_t spare[1];
		bool const keyed = NetV2_DirectionFor(&receiving, counter, key, nonce);
		bool const opened = keyed && NetCrypto_AesGcmOpen(key, nonce, sealed, kNetSealedGameHeaderBytes, sealed + kNetSealedGameHeaderBytes, textSize,
			sealed + kNetSealedGameHeaderBytes + textSize, textSize > 0 ? out : spare);
		NetCrypto_Wipe(key, sizeof(key));

		if (!keyed)
		{
			return Refuse(kNetSealedGameCouldnt, out, outRoom);
		}

		if (!opened)
		{
			badTags++;

			if (badTags >= kNetSealedGameMostBadTags)
			{
				End(kNetSealedGameTooManyBad);
			}

			return Refuse(kNetSealedGameBadTag, out, outRoom);
		}

		NetV2_ReplayTake(&window, counter);
		*written = textSize;
		return true;
	}

	// The session is over: both keys wiped. reason is kept for Why(), which then says why it ended.
	void End(int reason)
	{
		NetV2_DirectionWipe(&sending);
		NetV2_DirectionWipe(&receiving);
		NetV2_ReplayClear(&window);

		if (live)
		{
			ended = reason;
		}

		live = false;
	}

	bool Live() const
	{
		return live;
	}

	// Why the last datagram was refused, or kNetSealedGameFine if the last one was taken
	int Why() const
	{
		return why;
	}

	// Why the session ended, once it has (kNetSealedGameFine while it's live)
	int Ended() const
	{
		return ended;
	}

	uint64_t Sent() const
	{
		return sent;
	}

	uint32_t BadTags() const
	{
		return badTags;
	}

	uint64_t SeenAlready() const
	{
		return seenAlready;
	}

private:
	void Clear()
	{
		NetV2_DirectionWipe(&sending);
		NetV2_DirectionWipe(&receiving);
		NetV2_ReplayClear(&window);
		live = false;
		started = 0;
		sent = 0;
		badTags = 0;
		seenAlready = 0;
		why = kNetSealedGameFine;
		ended = kNetSealedGameFine;
	}

	bool WornOut(uint32_t now) const
	{
		return static_cast<uint32_t>(now - started) >= kNetSealedGameMostMs;
	}

	bool Refuse(int reason, uint8_t *out, size_t outRoom)
	{
		if (out && outRoom > 0)
		{
			NetCrypto_Wipe(out, outRoom);
		}

		why = reason;
		return false;
	}

	NetV2Direction sending;
	NetV2Direction receiving;
	NetV2Replay window;
	bool live;
	uint32_t started;
	uint64_t sent;           // the next counter to send
	uint32_t badTags;
	uint64_t seenAlready;
	int why;
	int ended;
};

#endif
