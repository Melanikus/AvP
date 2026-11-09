// A player's identity for protocol version 2: the keys that prove a host to its joiners, and a joiner to its host, in handshake B
// (net_handshake_b.h). Written 2026-11-09 by Melanikus for the AvP port.
//
// WHAT IT IS, in plain words. Every player has two key pairs: a classical one (ECDH on the P-384 curve) and a post-quantum one (ML-KEM-1024).
// The public halves are like a name badge anyone may see; the private halves never leave this game. In handshake B each side proves it
// holds its private halves, without showing them, by agreeing secrets with the other side's keys (CRYPTO.md section 8). The game never
// signs anything with these keys (CRYPTO.md section 4).
//
// A FINGERPRINT is a short name for the public halves: the first 16 bytes of SHA-384 of the 9 letters "AVPX-ID-2" (with nothing after
// them), then the P-384 public key (97 bytes) and the ML-KEM public key (1,568 bytes), exactly as they travel. It's written as 32 hex
// digits. The host's fingerprint goes in its listing, so a joiner knows which host to expect before it says a word (CRYPTO.md section 8).
//
// HOW LONG IT LIVES. For now an identity is made fresh each time the game runs, kept in memory only, and wiped when the game ends (PO's
// ruling, 2026-11-09). That's no weaker: the listing always carries this run's fingerprint. Keeping one identity from run to run, sealed
// with Windows' DPAPI (CRYPTO.md section 4.1), so friends see the same fingerprint, is a later card.
//
// It's game-free, as net_noise.h is: it uses only net_crypto.h and net_pq.h, so its test can try every case.

#ifndef NET_IDENTITY_H
#define NET_IDENTITY_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "net_crypto.h"
#include "net_pq.h"

size_t const kNetIdentityStaticBytes = kNetCryptoP384PublicBytes + kNetPqKemPublicBytes;   // both public halves, as `s` sends them: 1,665
size_t const kNetIdentityFingerprintBytes = 16;
size_t const kNetIdentityFingerprintHexBytes = 2 * kNetIdentityFingerprintBytes + 1;    // 32 hex digits and the 0 that ends the text

static const char kNetIdentityFingerprintWords[] = "AVPX-ID-2";

struct NetIdentity
{
	NetEcdhKey *p384;                                  // the private P-384 key; it lives inside CNG (net_crypto_cng.cpp)
	uint8_t kemPrivate[kNetPqKemPrivateBytes];         // the private ML-KEM key (its "decapsulation key"): a secret, wiped at the end
	uint8_t statics[kNetIdentityStaticBytes];          // the public halves: the P-384 key (97), then the ML-KEM key (1,568)
	uint8_t fingerprint[kNetIdentityFingerprintBytes];
	bool made;                                         // false until the identity is whole, and again once it's freed
};

// The public halves, one at a time
inline const uint8_t *NetIdentity_P384Public(const NetIdentity *id)
{
	return id->statics;
}

inline const uint8_t *NetIdentity_KemPublic(const NetIdentity *id)
{
	return id->statics + kNetCryptoP384PublicBytes;
}

// The fingerprint of two public halves, as CRYPTO.md section 8 says. False (with out wiped) only if SHA-384 itself failed.
inline bool NetIdentity_Fingerprint(const uint8_t p384Public[kNetCryptoP384PublicBytes], const uint8_t kemPublic[kNetPqKemPublicBytes],
	uint8_t out[kNetIdentityFingerprintBytes])
{
	NetCryptoPiece const pieces[3] = {
		{ reinterpret_cast<const uint8_t *>(kNetIdentityFingerprintWords), sizeof(kNetIdentityFingerprintWords) - 1 },
		{ p384Public, kNetCryptoP384PublicBytes },
		{ kemPublic, kNetPqKemPublicBytes },
	};
	uint8_t whole[kNetCryptoHashBytes];

	if (!NetCrypto_Sha384(pieces, 3, whole))
	{
		NetCrypto_Wipe(out, kNetIdentityFingerprintBytes);
		return false;
	}

	memcpy(out, whole, kNetIdentityFingerprintBytes);
	return true;
}

// A fingerprint as 32 small hex digits, as the list writes it, ended with a 0
inline void NetIdentity_FingerprintHex(const uint8_t fingerprint[kNetIdentityFingerprintBytes], char out[kNetIdentityFingerprintHexBytes])
{
	static const char kDigits[] = "0123456789abcdef";

	for (size_t i = 0; i < kNetIdentityFingerprintBytes; i++)
	{
		out[2 * i] = kDigits[fingerprint[i] >> 4];
		out[2 * i + 1] = kDigits[fingerprint[i] & 0x0F];
	}

	out[2 * kNetIdentityFingerprintBytes] = 0;
}

// A fingerprint from its 32 hex digits, as a listing gives it. Only small letters are taken, as the list writes them, and exactly 32
// digits with nothing after them: anything else is refused (false, with out wiped), because a listing is input from outside the game.
inline bool NetIdentity_FingerprintFromHex(const char *hex, uint8_t out[kNetIdentityFingerprintBytes])
{
	bool good = hex != NULL;

	for (size_t i = 0; good && i < 2 * kNetIdentityFingerprintBytes; i++)
	{
		good = (hex[i] >= '0' && hex[i] <= '9') || (hex[i] >= 'a' && hex[i] <= 'f');
	}

	good = good && hex[2 * kNetIdentityFingerprintBytes] == 0;

	if (!good)
	{
		NetCrypto_Wipe(out, kNetIdentityFingerprintBytes);
		return false;
	}

	for (size_t i = 0; i < kNetIdentityFingerprintBytes; i++)
	{
		char const high = hex[2 * i];
		char const low = hex[2 * i + 1];
		int const h = high <= '9' ? high - '0' : high - 'a' + 10;
		int const l = low <= '9' ? low - '0' : low - 'a' + 10;
		out[i] = static_cast<uint8_t>(h * 16 + l);
	}

	return true;
}

// Ends an identity: the P-384 key is destroyed inside CNG, and every byte of the rest is wiped. Safe to call twice, or on one never made.
inline void NetIdentity_Free(NetIdentity *id)
{
	NetCrypto_EcdhFree(id->p384);
	NetCrypto_Wipe(id, sizeof(*id));
}

// A new identity, from the operating system's randomness: what the game makes once each run. False if anything was refused, and then
// nothing of it is left.
inline bool NetIdentity_Make(NetIdentity *out)
{
	memset(out, 0, sizeof(*out));
	out->p384 = NetCrypto_EcdhNew(out->statics);

	bool const made = out->p384 != NULL
		&& NetPq_KemKeypair(out->statics + kNetCryptoP384PublicBytes, out->kemPrivate)
		&& NetIdentity_Fingerprint(NetIdentity_P384Public(out), NetIdentity_KemPublic(out), out->fingerprint);

	if (!made)
	{
		NetIdentity_Free(out);
		return false;
	}

	out->made = true;
	return true;
}

#endif
