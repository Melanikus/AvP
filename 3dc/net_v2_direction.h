// One direction of a version 2 session: its secret from Split, its IV, and the key for each epoch. Written 2026-11-09 by Melanikus for the
// AvP port.
//
// A finished handshake gives each direction a secret and an IV (net_noise.h, Split). Every message one side sends has a counter: 0, 1,
// 2, ... in that direction. Two things come from the counter:
//   - the nonce: the IV with its last 8 bytes XORed with the counter, little-endian (NetNoise_Nonce), so no nonce ever seals twice;
//   - the key: one key seals at most 2^20 messages, an "epoch" (CRYPTO.md section 9, and 5.1 point 8). Every epoch's key, epoch 0's
//     too, is HKDF-Expand(the direction's secret, "AVPX epoch v2" || e as 4 bytes little-endian, 32) (NetNoise_EpochKey). The secret
//     itself never seals anything (modified 2026-11-09 by Melanikus for the AvP port: epoch 0 used to seal with Split's key itself).
// This keeps the key for the epoch now in use, and makes the next only when a counter reaches it. The sealed control channel
// (net_sealed_control.h) and sealed game datagrams (net_sealed_game.h) each hold two of these, one per direction.
//
// SECRETS. The secret and the epoch's key are both secret: NetV2_DirectionWipe wipes them, and a session wipes both of its directions the
// moment it ends.

#ifndef NET_V2_DIRECTION_H
#define NET_V2_DIRECTION_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "net_crypto.h"
#include "net_noise.h"

uint32_t const kNetV2EpochShift = 20;   // an epoch is 2^20 messages

struct NetV2Direction
{
	uint8_t secret[kNetNoiseSecretBytes];   // Split's for this direction: it only makes keys
	uint8_t iv[kNetCryptoNonceBytes];
	uint8_t epochKey[kNetCryptoAesKeyBytes];
	uint64_t epoch;          // which epoch epochKey is for, once hasEpochKey
	bool hasEpochKey;
};

inline void NetV2_DirectionWipe(NetV2Direction *d)
{
	NetCrypto_Wipe(d, sizeof(*d));
}

// A direction from Split's secret and IV for it
inline void NetV2_DirectionStart(NetV2Direction *d, const uint8_t secret[kNetNoiseSecretBytes], const uint8_t iv[kNetCryptoNonceBytes])
{
	NetV2_DirectionWipe(d);
	memcpy(d->secret, secret, kNetNoiseSecretBytes);
	memcpy(d->iv, iv, kNetCryptoNonceBytes);
}

// The key and the nonce for counter. False (with both wiped) only if the epoch's key can't be made, or for an epoch past what 4 bytes
// can name, which no session reaches: it ends at 2^32 messages (CRYPTO.md section 17), 4,096 epochs.
inline bool NetV2_DirectionFor(NetV2Direction *d, uint64_t counter, uint8_t keyOut[kNetCryptoAesKeyBytes], uint8_t nonceOut[kNetCryptoNonceBytes])
{
	uint64_t const epoch = counter >> kNetV2EpochShift;

	if (!d->hasEpochKey || d->epoch != epoch)
	{
		NetCrypto_Wipe(d->epochKey, sizeof(d->epochKey));
		d->hasEpochKey = false;

		if (epoch > UINT32_MAX || !NetNoise_EpochKey(d->secret, static_cast<uint32_t>(epoch), d->epochKey))
		{
			NetCrypto_Wipe(d->epochKey, sizeof(d->epochKey));
			NetCrypto_Wipe(keyOut, kNetCryptoAesKeyBytes);
			NetCrypto_Wipe(nonceOut, kNetCryptoNonceBytes);
			return false;
		}

		d->epoch = epoch;
		d->hasEpochKey = true;
	}

	memcpy(keyOut, d->epochKey, kNetCryptoAesKeyBytes);
	NetNoise_Nonce(d->iv, counter, nonceOut);
	return true;
}

#endif
