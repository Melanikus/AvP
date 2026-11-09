// Protocol version 2's sealed control channel, the game's side: every one of the relay's small messages, sealed. Written 2026-11-09 by
// Melanikus for the AvP port.
//
// WHAT IT IS (CRYPTO.md section 7, in the AVPx-Dedicated-Server repository). Once handshake A is done (net_handshake_a.h), every control
// message of version 1 (the 64-byte HOST_OPEN, JOIN, the hellos and the rest, PROTOCOL.md) goes sealed, as exactly 104 bytes:
//   bytes 0-3     'AVPR'
//   byte  4       2, the version
//   byte  5       32, the kind: sealed
//   bytes 6-7     0
//   bytes 8-15    the relay's session id, from A2
//   bytes 16-23   the counter: 0, 1, 2, ... in each direction, 8 bytes little-endian
//   bytes 24-87   the 64-byte message, sealed (AES-256-GCM)
//   bytes 88-103  the tag
// Bytes 0-23 are what the tag also proves (the AAD). Questions and answers are the same size, so no answer is ever bigger than its
// question. Tokens, tickets, passes and codes are never seen on the path again.
//
// THE KEYS: the game is Noise's initiator, so it seals with Split's initiator-to-responder key and IV, and opens with the other two. One
// key seals at most 2^20 messages, an epoch (net_v2_direction.h). Nothing the relay sends is taken twice (net_v2_replay.h), and a counter
// is marked taken only once its tag has checked.
//
// WHEN A SESSION IS OVER (CRYPTO.md section 17): after 2^32 messages either way, 24 hours, or 65,536 tags that failed. Then it seals and
// opens nothing more, and the game starts a new handshake. The relay's Java does the same (java/src/avpx/v2/Channel.java).
//
// EVERYTHING THAT ARRIVES IS HOSTILE (AvP Tao 3): a sealed message is read only at its one size, every fixed byte checked, for this
// session's id only, before anything is spent on it.

#ifndef NET_SEALED_CONTROL_H
#define NET_SEALED_CONTROL_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "net_crypto.h"
#include "net_noise.h"
#include "net_v2_direction.h"
#include "net_v2_fragments.h"
#include "net_v2_replay.h"
#include "net_handshake_a.h"

size_t const kNetSealedControlBytes = 104;
size_t const kNetSealedControlMessageBytes = 64;   // version 1's message, as PROTOCOL.md has it
size_t const kNetSealedControlAadBytes = 24;
uint64_t const kNetSealedControlMostMessages = static_cast<uint64_t>(1) << 32;
uint32_t const kNetSealedControlMostMs = 24u * 60u * 60u * 1000u;   // 24 hours
uint64_t const kNetSealedControlMostFailedTags = 65536;

// Why a sealed message was refused: CRYPTO.md section 16's numbers
int const kNetSealedOk = 0;
int const kNetSealedWrongSize = 1;       // not 104 bytes
int const kNetSealedWrongShape = 2;      // another magic, version or kind, or bytes 6-7 not 0
int const kNetSealedNotOurs = 4;         // another session's id
int const kNetSealedTag = 8;             // the tag fails
int const kNetSealedReplayed = 9;        // a counter taken already, below the window, or past the most a session has
int const kNetSealedOver = 16;           // the session is over (its messages, its 24 hours, or its failed tags)

struct NetSealedControl
{
	uint8_t sessionId[kNetHandshakeSessionIdBytes];
	NetV2Direction out;        // the game's to the relay
	NetV2Direction in;         // the relay's to the game
	NetV2Replay window;        // the relay's counters taken
	uint64_t sent;             // the game's next counter
	uint64_t failedTags;
	uint32_t startedAt;        // milliseconds, the game's clock
	bool started;
	bool over;
};

inline void NetSealedControl_Wipe(NetSealedControl *c)
{
	NetCrypto_Wipe(c, sizeof(*c));
}

// A session from handshake A's keys, at now. The keys are copied; the caller wipes its own.
inline void NetSealedControl_Start(NetSealedControl *c, const NetHandshakeAKeys *keys, uint32_t now)
{
	NetSealedControl_Wipe(c);
	memcpy(c->sessionId, keys->sessionId, sizeof(c->sessionId));
	NetV2_DirectionStart(&c->out, keys->split.initiatorToResponderKey, keys->split.initiatorToResponderIv);
	NetV2_DirectionStart(&c->in, keys->split.responderToInitiatorKey, keys->split.responderToInitiatorIv);
	NetV2_ReplayClear(&c->window);
	c->startedAt = now;
	c->started = true;
}

// Whether this session is over: never started, ended, 24 hours old (the clock's difference, so it wraps safely), or its clock gone back
inline bool NetSealedControl_Over(NetSealedControl *c, uint32_t now)
{
	if (!c->started || c->over || static_cast<uint32_t>(now - c->startedAt) >= kNetSealedControlMostMs)
	{
		c->over = c->started;
		return true;
	}

	return false;
}

// Bytes 0-23 for a counter: the header every sealed message has, which its tag proves too
inline void NetSealedControl_Header(const uint8_t sessionId[kNetHandshakeSessionIdBytes], uint64_t counter, uint8_t out[kNetSealedControlAadBytes])
{
	memset(out, 0, kNetSealedControlAadBytes);
	memcpy(out, kNetV2MagicRelay, 4);
	out[4] = kNetV2Version;
	out[5] = kNetV2KindSealed;
	memcpy(out + 8, sessionId, kNetHandshakeSessionIdBytes);

	for (size_t i = 0; i < 8; i++)
	{
		out[16 + i] = static_cast<uint8_t>((counter >> (8 * i)) & 0xFF);
	}
}

// A 64-byte message sealed into out, 104 bytes, with the next counter. False, with out wiped, if the session is over (it then is over
// for good) or the cryptography failed.
inline bool NetSealedControl_Seal(NetSealedControl *c, const uint8_t message[kNetSealedControlMessageBytes], uint8_t out[kNetSealedControlBytes],
	uint32_t now)
{
	NetCrypto_Wipe(out, kNetSealedControlBytes);

	if (!message || NetSealedControl_Over(c, now) || c->sent >= kNetSealedControlMostMessages)
	{
		c->over = c->started;
		return false;
	}

	uint64_t const counter = c->sent;
	uint8_t key[kNetCryptoAesKeyBytes], nonce[kNetCryptoNonceBytes];
	NetSealedControl_Header(c->sessionId, counter, out);
	bool const sealed = NetV2_DirectionFor(&c->out, counter, key, nonce) &&
		NetCrypto_AesGcmSeal(key, nonce, out, kNetSealedControlAadBytes, message, kNetSealedControlMessageBytes, out + kNetSealedControlAadBytes,
			out + kNetSealedControlAadBytes + kNetSealedControlMessageBytes);
	NetCrypto_Wipe(key, sizeof(key));

	if (!sealed)
	{
		NetCrypto_Wipe(out, kNetSealedControlBytes);
		return false;
	}

	c->sent++;
	return true;
}

// The 64-byte message in a sealed one from the relay, into messageOut. Gives back kNetSealedOk, or why not, with messageOut wiped.
inline int NetSealedControl_Open(NetSealedControl *c, const uint8_t *d, size_t size, uint8_t messageOut[kNetSealedControlMessageBytes], uint32_t now)
{
	NetCrypto_Wipe(messageOut, kNetSealedControlMessageBytes);

	if (!d || size != kNetSealedControlBytes)
	{
		return kNetSealedWrongSize;
	}

	if (memcmp(d, kNetV2MagicRelay, 4) != 0 || d[4] != kNetV2Version || d[5] != kNetV2KindSealed || d[6] != 0 || d[7] != 0)
	{
		return kNetSealedWrongShape;
	}

	if (memcmp(d + 8, c->sessionId, kNetHandshakeSessionIdBytes) != 0)
	{
		return kNetSealedNotOurs;
	}

	if (NetSealedControl_Over(c, now))
	{
		return kNetSealedOver;
	}

	uint64_t counter = 0;

	for (size_t i = 0; i < 8; i++)
	{
		counter |= static_cast<uint64_t>(d[16 + i]) << (8 * i);
	}

	if (counter >= kNetSealedControlMostMessages || !NetV2_ReplayIsNew(&c->window, counter))
	{
		return kNetSealedReplayed;
	}

	uint8_t key[kNetCryptoAesKeyBytes], nonce[kNetCryptoNonceBytes], tag[kNetCryptoTagBytes];
	memcpy(tag, d + kNetSealedControlAadBytes + kNetSealedControlMessageBytes, sizeof(tag));
	bool const opened = NetV2_DirectionFor(&c->in, counter, key, nonce) &&
		NetCrypto_AesGcmOpen(key, nonce, d, kNetSealedControlAadBytes, d + kNetSealedControlAadBytes, kNetSealedControlMessageBytes, tag, messageOut);
	NetCrypto_Wipe(key, sizeof(key));

	if (!opened)
	{
		NetCrypto_Wipe(messageOut, kNetSealedControlMessageBytes);

		if (++c->failedTags >= kNetSealedControlMostFailedTags)
		{
			c->over = true;
		}

		return kNetSealedTag;
	}

	NetV2_ReplayTake(&c->window, counter);
	return kNetSealedOk;
}

#endif
