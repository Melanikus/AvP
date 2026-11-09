// The classical half of the relay's protocol version 2 cryptography: what the game asks for, and nothing more. Written 2026-11-09 by
// Melanikus for the AvP port.
//
// WHAT THIS IS. Version 2 (CRYPTO.md, in the AVPx-Dedicated-Server repository) seals everything the game says to the relay, and later
// everything a joiner and a host say to each other. It uses one fixed suite, with no choices (CRYPTO.md section 2): a classical half,
// here, and a post-quantum half (net_pq.h). Every key that seals anything depends on both halves, so a session holds unless both are
// broken. This file is the classical half:
//   - SHA-384 and HMAC-SHA-384, for the transcript and the keys;
//   - HKDF-SHA-384 (RFC 5869), written below over HMAC;
//   - ECDH on the P-384 curve, the classical key agreement;
//   - ECDSA P-384 signature checks (the game checks signatures and never makes one: CRYPTO.md section 4);
//   - AES-256-GCM, which seals a message and proves it wasn't changed;
//   - random bytes, from the operating system.
//
// HOW IT ANSWERS. Every call gives back true or false. False means refused, for any reason at all, and the caller ends what it was doing:
// "every failure ends the handshake" (CRYPTO.md). On false, every output has been wiped to 0, so half a result can never be used by
// mistake.
//
// WHERE IT'S DONE. Windows' own cryptography, CNG (bcrypt.dll), in net_crypto_cng.cpp: Microsoft's, in every Windows since 7, with no
// library of ours (AVP Tao 1). This file only declares the calls, so the code that uses them (net_noise.h and the handshakes) never needs
// Windows' headers, and its tests can try every case.
//
// SECRETS. Anything that held a key or a secret is wiped with NetCrypto_Wipe as soon as it's done with (CRYPTO.md section 9). Nothing here
// ever writes a key, a secret or an address to the log.

#ifndef NET_CRYPTO_H
#define NET_CRYPTO_H

#include <stddef.h>
#include <stdint.h>

// The sizes, in bytes (CRYPTO.md section 2)
size_t const kNetCryptoHashBytes = 48;          // SHA-384, and HMAC-SHA-384
size_t const kNetCryptoAesKeyBytes = 32;        // AES-256
size_t const kNetCryptoNonceBytes = 12;         // AES-GCM's nonce
size_t const kNetCryptoTagBytes = 16;           // AES-GCM's tag, the proof a sealed message wasn't changed
size_t const kNetCryptoP384PublicBytes = 97;    // a P-384 public key as it goes on the wire: 0x04, then X and Y, 48 bytes each, big-endian
size_t const kNetCryptoP384ScalarBytes = 48;    // a P-384 private key, or a shared secret (the agreed point's X, big-endian)
size_t const kNetCryptoEcdsaBytes = 96;         // an ECDSA P-384 signature: r then s, 48 bytes each, big-endian (IEEE P1363)
size_t const kNetCryptoMostHkdfBytes = 255 * 48;// the most HKDF-SHA-384 can give (RFC 5869 section 2.3)

// Wipes bytes so the compiler can't leave the wiping out: SecureZeroMemory, in net_crypto_cng.cpp
void NetCrypto_Wipe(void *bytes, size_t size);

// size random bytes from the operating system's own source (BCryptGenRandom). False, with out wiped, if it couldn't give them.
bool NetCrypto_Random(uint8_t *out, size_t size);

// Some bytes, one piece of a longer input. A hash or an HMAC of several pieces is of all of them one after another, as if they were one.
struct NetCryptoPiece
{
	const uint8_t *bytes;
	size_t size;
};

// SHA-384 of the pieces, one after another
bool NetCrypto_Sha384(const NetCryptoPiece *pieces, size_t count, uint8_t out[kNetCryptoHashBytes]);

// HMAC-SHA-384 with key (of any length, 0 included), of the pieces one after another
bool NetCrypto_HmacSha384(const uint8_t *key, size_t keySize, const NetCryptoPiece *pieces, size_t count, uint8_t out[kNetCryptoHashBytes]);

// A private P-384 key for key agreement. It lives inside CNG, and only net_crypto_cng.cpp knows what's in it.
struct NetEcdhKey;

// A new P-384 key of its own, made from the operating system's randomness. Gives back the key, and its public half in publicOut, or NULL
// (with publicOut wiped) if it couldn't. NetCrypto_EcdhFree it when done.
NetEcdhKey *NetCrypto_EcdhNew(uint8_t publicOut[kNetCryptoP384PublicBytes]);

// A P-384 key from fixed bytes, for test vectors only: a game in play always uses NetCrypto_EcdhNew. Its public half must be given too,
// and must be that private key's. NULL if CNG refuses either.
NetEcdhKey *NetCrypto_EcdhFromPrivate(const uint8_t privateKey[kNetCryptoP384ScalarBytes], const uint8_t publicKey[kNetCryptoP384PublicBytes]);

// The secret two P-384 keys agree on: the X of mine times theirs, big-endian, as Java and the other implementations give it. False (with
// secretOut wiped) for a public key that isn't one: the wrong first byte, a point not on the curve, or an answer of all 0.
bool NetCrypto_EcdhAgree(NetEcdhKey *mine, const uint8_t theirs[kNetCryptoP384PublicBytes], uint8_t secretOut[kNetCryptoP384ScalarBytes]);

// Destroys the key, inside CNG, and wipes what held it. NULL is allowed.
void NetCrypto_EcdhFree(NetEcdhKey *key);

// Whether signature is publicKey's ECDSA P-384 signature of hash, the SHA-384 of what was signed. CRYPTO.md section 12 says what's signed:
// the game passes SHA-384(D) here.
bool NetCrypto_EcdsaVerify(const uint8_t publicKey[kNetCryptoP384PublicBytes], const uint8_t hash[kNetCryptoHashBytes], const uint8_t signature[kNetCryptoEcdsaBytes]);

// AES-256-GCM. Seal: size bytes of text into out (also size bytes), and the tag. Open: the same backwards, and true only if the tag
// proves that the bytes, the aad and the nonce are exactly as sealed. On false, out is wiped. text and out may be the same bytes.
bool NetCrypto_AesGcmSeal(const uint8_t key[kNetCryptoAesKeyBytes], const uint8_t nonce[kNetCryptoNonceBytes], const uint8_t *aad, size_t aadSize,
	const uint8_t *text, size_t size, uint8_t *out, uint8_t tag[kNetCryptoTagBytes]);
bool NetCrypto_AesGcmOpen(const uint8_t key[kNetCryptoAesKeyBytes], const uint8_t nonce[kNetCryptoNonceBytes], const uint8_t *aad, size_t aadSize,
	const uint8_t *sealed, size_t size, const uint8_t tag[kNetCryptoTagBytes], uint8_t *out);

// HKDF-SHA-384 (RFC 5869), in its two steps. Each gives back false, with out wiped, if HMAC failed or more is asked than it can give.
//
// Expand: from a key that's already strong (prk), outSize bytes for one use, named by info. Block 1 is HMAC(prk, info, 1), block 2 is
// HMAC(prk, block 1, info, 2), and so on, the blocks one after another, cut to outSize. Version 2 also uses this alone, to give each epoch
// of a session its own key (CRYPTO.md section 9).
inline bool NetCrypto_HkdfExpandSha384(const uint8_t *prk, size_t prkSize, const uint8_t *info, size_t infoSize, uint8_t *out, size_t outSize)
{
	if (!out || outSize > kNetCryptoMostHkdfBytes || !prk || prkSize == 0)
	{
		return false;
	}

	uint8_t block[kNetCryptoHashBytes];
	size_t blockSize = 0;   // the first block follows nothing
	size_t done = 0;
	uint8_t number = 1;
	bool ok = true;

	while (ok && done < outSize)
	{
		NetCryptoPiece const pieces[3] = { { block, blockSize }, { info, infoSize }, { &number, 1 } };
		ok = NetCrypto_HmacSha384(prk, prkSize, pieces, 3, block);
		blockSize = sizeof(block);
		size_t const take = outSize - done < sizeof(block) ? outSize - done : sizeof(block);

		for (size_t i = 0; ok && i < take; i++)
		{
			out[done + i] = block[i];
		}

		done += take;
		number++;
	}

	NetCrypto_Wipe(block, sizeof(block));

	if (!ok)
	{
		NetCrypto_Wipe(out, outSize);
	}

	return ok;
}

// Extract then Expand: from secret (the "input keying material") and salt, a strong 48-byte key, PRK = HMAC(salt, secret), however lumpy
// the secret; then Expand from it. A salt of no bytes is 48 zeros (RFC 5869 section 2.2), which HMAC treats the same.
inline bool NetCrypto_HkdfSha384(const uint8_t *salt, size_t saltSize, const uint8_t *secret, size_t secretSize, const uint8_t *info, size_t infoSize,
	uint8_t *out, size_t outSize)
{
	if (!out || outSize > kNetCryptoMostHkdfBytes)
	{
		return false;
	}

	static const uint8_t kZeroSalt[kNetCryptoHashBytes] = {};
	uint8_t prk[kNetCryptoHashBytes];
	NetCryptoPiece const input = { secret, secretSize };
	bool const ok = NetCrypto_HmacSha384(saltSize > 0 ? salt : kZeroSalt, saltSize > 0 ? saltSize : sizeof(kZeroSalt), &input, 1, prk)
		&& NetCrypto_HkdfExpandSha384(prk, sizeof(prk), info, infoSize, out, outSize);
	NetCrypto_Wipe(prk, sizeof(prk));

	if (!ok)
	{
		NetCrypto_Wipe(out, outSize);
	}

	return ok;
}

#endif
