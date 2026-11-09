// The heart of the relay's protocol version 2 handshakes: the running transcript and the keys made from it, as the Noise framework does
// it. Written 2026-11-09 by Melanikus for the AvP port.
//
// WHAT NOISE IS, in plain words. A handshake is a few messages, and both sides keep two things as it goes:
//   - h, a fingerprint of everything said so far (a SHA-384 "transcript hash"). Each message is mixed into it (MixHash), so if anyone
//     changed a single byte on the way, the two sides' h would differ from then on, and every later check would fail.
//   - ck, a "chaining key". Each secret the two sides agree on (an ECDH result, an ML-KEM secret) is mixed into it (MixKey), and each
//     mix also gives a fresh key, k, to seal the handshake's next words with. So every key after a secret depends on that secret, and
//     on every one before it: that's how the classical and the post-quantum secrets are both needed (the "hybrid", CRYPTO.md section 5).
// When the handshake is over, Split gives one key and nonce base for each direction, and h is no longer needed.
//
// This follows CRYPTO.md section 5 (in the AVPx-Dedicated-Server repository), itself the Noise framework's revision 34 (noiseprotocol.org)
// with SHA-384, AES-256-GCM and the post-quantum tokens of PQNoise. It's game-free, as net_relay.h is, so its test tries every case.
//
// THE BYTE-LEVEL CHOICES, all here in one place. CRYPTO.md (242e075) left these open; Third, who owns it, settled them on 2026-11-09,
// Primary's Java does the same, and Primary's crypto-v2 test vectors are the referee. Third is writing them into CRYPTO.md.
//   - MixKey: HKDF-SHA-384 (RFC 5869) with salt ck, the secret as its input, and no info, 80 bytes: the new ck is bytes 0-47, k 48-79.
//   - A nonce: the 12-byte IV, with its last 8 bytes XORed with n, little-endian (CRYPTO.md's nonce(n)). During a handshake the IV is 0.
//     (Noise's own AES-GCM puts n big-endian: version 2 differs on purpose.)
//   - Split: HKDF-SHA-384 with salt ck, no input and info "AVPX split v2", 120 bytes: the initiator-to-responder secret (48), the
//     responder-to-initiator secret (48), then the initiator-to-responder IV (12) and the responder-to-initiator IV (12).
//   - An epoch's key (CRYPTO.md section 9): for every epoch e, 0 included, HKDF-Expand(that direction's secret, "AVPX epoch v2" then e
//     as 4 bytes little-endian, 32 bytes). The IV stays Split's.
//   MODIFIED 2026-11-09 by Melanikus for the AvP port (CRYPTO.md 5.1 point 3, Third's a7a369f, from Secondary's reads): Split used to give
//   two 32-byte keys, and epoch 0 sealed with the very key every later epoch was made from: one key with two jobs. Now a direction's
//   secret only ever makes keys, and never seals anything itself. It's 48 bytes, the hash's own length, as RFC 5869 asks of a key for
//   HKDF-Expand.
//
// SECRETS. h isn't secret, but ck and k are: NetNoise_Wipe wipes all of it, and the handshakes call it the moment they're done.

#ifndef NET_NOISE_H
#define NET_NOISE_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "net_crypto.h"

// The protocol names (CRYPTO.md section 5): each handshake's h starts as the SHA-384 of its name, so the two can never be mixed up
// Handshake A, a game and its relay: pqIK since 2026-11-09 (server #24: the player's identity proves an approved player), so h starts
// apart from the old pqNK's, and a game of one can never finish a handshake with a relay of the other. (Modified 2026-11-09 by Melanikus
// for the AvP port.)
static const char kNetNoiseNameRelay[] = "AVPX_pqIK_P384+MLKEM1024_AESGCM_SHA384_v2";
static const char kNetNoiseNamePeers[] = "AVPX_pqXX_P384+MLKEM1024_AESGCM_SHA384_v2";   // handshake B, a joiner and its host

static const char kNetNoiseSplitInfo[] = "AVPX split v2";
static const char kNetNoiseEpochInfo[] = "AVPX epoch v2";

size_t const kNetNoiseMixKeyBytes = 80;      // MixKey's HKDF: the new ck (48), then k (32)
size_t const kNetNoiseSecretBytes = kNetCryptoHashBytes;   // a direction's secret: 48, SHA-384's length
size_t const kNetNoiseSplitBytes = 2 * kNetNoiseSecretBytes + 2 * kNetCryptoNonceBytes;   // Split's HKDF: two secrets, then two IVs: 120

struct NetNoiseState
{
	uint8_t h[kNetCryptoHashBytes];     // the transcript hash
	uint8_t ck[kNetCryptoHashBytes];    // the chaining key
	uint8_t k[kNetCryptoAesKeyBytes];   // the key the next words are sealed with, once there is one
	bool hasKey;                        // whether there's a k yet: before the first MixKey, words go unsealed (but still into h)
	uint64_t n;                         // how many words have been sealed with this k
};

// What a finished handshake gives: a secret and an IV for each direction. A secret never seals anything: each epoch's key is made from it
// (NetNoise_EpochKey).
struct NetNoiseSplit
{
	uint8_t initiatorToResponderSecret[kNetNoiseSecretBytes];
	uint8_t responderToInitiatorSecret[kNetNoiseSecretBytes];
	uint8_t initiatorToResponderIv[kNetCryptoNonceBytes];
	uint8_t responderToInitiatorIv[kNetCryptoNonceBytes];
};

inline void NetNoise_Wipe(NetNoiseState *s)
{
	NetCrypto_Wipe(s, sizeof(*s));
}

inline void NetNoise_WipeSplit(NetNoiseSplit *split)
{
	NetCrypto_Wipe(split, sizeof(*split));
}

// The nonce for the n-th word: the IV, with its last 8 bytes XORed with n, little-endian
inline void NetNoise_Nonce(const uint8_t iv[kNetCryptoNonceBytes], uint64_t n, uint8_t out[kNetCryptoNonceBytes])
{
	memcpy(out, iv, kNetCryptoNonceBytes);

	for (size_t i = 0; i < 8; i++)
	{
		out[4 + i] = static_cast<uint8_t>(out[4 + i] ^ ((n >> (8 * i)) & 0xFF));
	}
}

// The start: h is the SHA-384 of the protocol's name (its ASCII letters, with no 0 after), and ck begins the same. No key yet.
inline bool NetNoise_Start(NetNoiseState *s, const char *name)
{
	memset(s, 0, sizeof(*s));
	NetCryptoPiece const piece = { reinterpret_cast<const uint8_t *>(name), name ? strlen(name) : 0 };

	if (!name || !NetCrypto_Sha384(&piece, 1, s->h))
	{
		NetNoise_Wipe(s);
		return false;
	}

	memcpy(s->ck, s->h, sizeof(s->ck));
	return true;
}

// MixHash: h = SHA-384(h, then data)
inline bool NetNoise_MixHash(NetNoiseState *s, const uint8_t *data, size_t size)
{
	NetCryptoPiece const pieces[2] = { { s->h, sizeof(s->h) }, { data, size } };
	uint8_t next[kNetCryptoHashBytes];

	if (!NetCrypto_Sha384(pieces, 2, next))
	{
		return false;
	}

	memcpy(s->h, next, sizeof(next));
	return true;
}

// MixKey: a secret both sides now share goes into the chain, giving a new ck and a new k, whose count of sealed words starts again at 0
inline bool NetNoise_MixKey(NetNoiseState *s, const uint8_t *secret, size_t size)
{
	uint8_t both[kNetNoiseMixKeyBytes];

	if (!NetCrypto_HkdfSha384(s->ck, sizeof(s->ck), secret, size, NULL, 0, both, sizeof(both)))
	{
		return false;
	}

	memcpy(s->ck, both, kNetCryptoHashBytes);
	memcpy(s->k, both + kNetCryptoHashBytes, kNetCryptoAesKeyBytes);
	NetCrypto_Wipe(both, sizeof(both));
	s->hasKey = true;
	s->n = 0;
	return true;
}

// EncryptAndHash: the words, sealed with k if there's one (with h as what the tag also proves), then mixed into h as they went. Into out,
// which has room for outRoom bytes: size bytes, and a 16-byte tag after them once there's a key. *written says how many. text and out
// may be the same bytes.
inline bool NetNoise_EncryptAndHash(NetNoiseState *s, const uint8_t *text, size_t size, uint8_t *out, size_t outRoom, size_t *written)
{
	*written = 0;

	if (!s->hasKey)
	{
		if (outRoom < size || (size > 0 && (!text || !out)))
		{
			return false;
		}

		if (size > 0)
		{
			memmove(out, text, size);
		}

		*written = size;
		return NetNoise_MixHash(s, out, size);
	}

	// A key seals at most 2^64 - 1 words, as Noise says; a handshake never comes near
	if (outRoom < size + kNetCryptoTagBytes || s->n == UINT64_MAX || !out)
	{
		return false;
	}

	static const uint8_t kNoIv[kNetCryptoNonceBytes] = {};
	uint8_t nonce[kNetCryptoNonceBytes];
	NetNoise_Nonce(kNoIv, s->n, nonce);

	if (!NetCrypto_AesGcmSeal(s->k, nonce, s->h, sizeof(s->h), text, size, out, out + size))
	{
		return false;
	}

	s->n++;
	*written = size + kNetCryptoTagBytes;
	return NetNoise_MixHash(s, out, *written);
}

// DecryptAndHash: the other side's EncryptAndHash, backwards. False for a tag that fails, which ends the handshake. out has room for
// outRoom bytes; *written says how many. sealed and out may be the same bytes: h's next value is worked out from the sealed bytes first.
inline bool NetNoise_DecryptAndHash(NetNoiseState *s, const uint8_t *sealed, size_t size, uint8_t *out, size_t outRoom, size_t *written)
{
	*written = 0;

	if (size > 0 && !sealed)
	{
		return false;
	}

	NetCryptoPiece const pieces[2] = { { s->h, sizeof(s->h) }, { sealed, size } };
	uint8_t next[kNetCryptoHashBytes];

	if (!NetCrypto_Sha384(pieces, 2, next))
	{
		return false;
	}

	if (!s->hasKey)
	{
		if (outRoom < size || (size > 0 && !out))
		{
			return false;
		}

		if (size > 0)
		{
			memmove(out, sealed, size);
		}

		memcpy(s->h, next, sizeof(next));
		*written = size;
		return true;
	}

	if (size < kNetCryptoTagBytes || outRoom < size - kNetCryptoTagBytes || s->n == UINT64_MAX || (size > kNetCryptoTagBytes && !out))
	{
		return false;
	}

	size_t const textSize = size - kNetCryptoTagBytes;
	static const uint8_t kNoIv[kNetCryptoNonceBytes] = {};
	uint8_t nonce[kNetCryptoNonceBytes];
	uint8_t tag[kNetCryptoTagBytes];
	NetNoise_Nonce(kNoIv, s->n, nonce);
	memcpy(tag, sealed + textSize, sizeof(tag));

	if (!NetCrypto_AesGcmOpen(s->k, nonce, s->h, sizeof(s->h), sealed, textSize, tag, out))
	{
		return false;
	}

	s->n++;
	memcpy(s->h, next, sizeof(next));
	*written = textSize;
	return true;
}

// Split: the handshake is over, and gives one secret and IV for each direction, in CRYPTO.md's order (5.1 point 3)
inline bool NetNoise_Split(const NetNoiseState *s, NetNoiseSplit *out)
{
	uint8_t all[kNetNoiseSplitBytes];
	bool const ok = NetCrypto_HkdfSha384(s->ck, sizeof(s->ck), NULL, 0, reinterpret_cast<const uint8_t *>(kNetNoiseSplitInfo), strlen(kNetNoiseSplitInfo),
		all, sizeof(all));

	if (ok)
	{
		memcpy(out->initiatorToResponderSecret, all, kNetNoiseSecretBytes);
		memcpy(out->responderToInitiatorSecret, all + kNetNoiseSecretBytes, kNetNoiseSecretBytes);
		memcpy(out->initiatorToResponderIv, all + 2 * kNetNoiseSecretBytes, kNetCryptoNonceBytes);
		memcpy(out->responderToInitiatorIv, all + 2 * kNetNoiseSecretBytes + kNetCryptoNonceBytes, kNetCryptoNonceBytes);
	}
	else
	{
		NetNoise_WipeSplit(out);
	}

	NetCrypto_Wipe(all, sizeof(all));
	return ok;
}

// The key for one epoch of a direction (CRYPTO.md section 9): every epoch's, 0 included, is made the same way from the direction's secret,
// so nothing ever seals with the secret itself
inline bool NetNoise_EpochKey(const uint8_t secret[kNetNoiseSecretBytes], uint32_t epoch, uint8_t out[kNetCryptoAesKeyBytes])
{
	uint8_t info[sizeof(kNetNoiseEpochInfo) - 1 + 4];
	memcpy(info, kNetNoiseEpochInfo, sizeof(kNetNoiseEpochInfo) - 1);

	for (size_t i = 0; i < 4; i++)
	{
		info[sizeof(kNetNoiseEpochInfo) - 1 + i] = static_cast<uint8_t>((epoch >> (8 * i)) & 0xFF);
	}

	return NetCrypto_HkdfExpandSha384(secret, kNetNoiseSecretBytes, info, sizeof(info), out, kNetCryptoAesKeyBytes);
}

#endif
