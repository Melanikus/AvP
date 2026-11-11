// A player's identity for protocol version 2: the keys that prove a host to its joiners, and a joiner to its host, in handshake B
// (net_handshake_b.h). Written 2026-11-09 by Melanikus for the AvP port.
//
// WHAT IT IS, in plain words. Every player has two key pairs: a classical one (ECDH on the P-384 curve) and a post-quantum one (ML-KEM-1024).
// The public halves are like a name badge anyone may see; the private halves never leave this game. In handshake B each side proves it
// holds its private halves, without showing them, by agreeing secrets with the other side's keys (CRYPTO.md section 8). The game never
// signs anything with these keys (CRYPTO.md section 4).
//
// THE ID is the whole name of the public halves: SHA-384 of the 9 letters "AVPX-ID-2" (with nothing after them), then the P-384 public
// key (97 bytes) and the ML-KEM public key (1,568 bytes), exactly as they travel: 48 bytes, written as 96 hex digits. The host's id goes
// in its listing, so a joiner knows which host to expect before it says a word, and handshake B's prologue mixes it (CRYPTO.md section 8,
// and 5.1 point 17). A FINGERPRINT is the id's first 16 bytes, written as 32 hex digits: the short name people read, and what the access
// code and an approval are made from. (Modified 2026-11-10 by Melanikus for the AvP port, SERVER-BLOCKERS item 6: until then the listing
// and handshake B used the fingerprint, a key pair to match which takes about 2^64 tries for a quantum search, below the rest of the
// design; the whole id takes far more: CRYPTO.md section 8.)
//
// HOW LONG IT LIVES. It's kept from run to run, in the player's save folder, with its private halves sealed by Windows' DPAPI for that
// Windows user only (CRYPTO.md section 4.1; net_identity_file.h and net_identity_store.cpp, #21). So friends, and a server that has
// approved a player, see the same fingerprint every time. A new one is made only when there's none yet, or the player asks for one, or
// confirms one after the kept one couldn't be read. In memory it's wiped when the game ends. (Modified 2026-11-09 by Melanikus for the
// AvP port: until then, one was made fresh each run, PO's ruling while nothing kept it.)
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
size_t const kNetIdentityIdBytes = kNetCryptoHashBytes;                                  // the whole id: SHA-384's 48 bytes
size_t const kNetIdentityIdHexBytes = 2 * kNetIdentityIdBytes + 1;                       // 96 hex digits and the 0 that ends the text
static_assert(kNetIdentityFingerprintBytes <= kNetIdentityIdBytes, "a fingerprint is the id's first bytes");

static const char kNetIdentityFingerprintWords[] = "AVPX-ID-2";

struct NetIdentity
{
	NetEcdhKey *p384;                                  // the private P-384 key; it lives inside CNG (net_crypto_cng.cpp)
	uint8_t kemPrivate[kNetPqKemPrivateBytes];         // the private ML-KEM key (its "decapsulation key"): a secret, wiped at the end
	uint8_t statics[kNetIdentityStaticBytes];          // the public halves: the P-384 key (97), then the ML-KEM key (1,568)
	uint8_t fingerprint[kNetIdentityFingerprintBytes];   // the id's first 16 bytes
	uint8_t id[kNetIdentityIdBytes];                   // the whole id (above), which the listing carries and handshake B mixes
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

// The whole id of two public halves, as CRYPTO.md section 8 says: the SHA-384 the fingerprint is the start of. False (with out wiped)
// only if SHA-384 itself failed.
inline bool NetIdentity_Id(const uint8_t p384Public[kNetCryptoP384PublicBytes], const uint8_t kemPublic[kNetPqKemPublicBytes],
	uint8_t out[kNetIdentityIdBytes])
{
	NetCryptoPiece const pieces[3] = {
		{ reinterpret_cast<const uint8_t *>(kNetIdentityFingerprintWords), sizeof(kNetIdentityFingerprintWords) - 1 },
		{ p384Public, kNetCryptoP384PublicBytes },
		{ kemPublic, kNetPqKemPublicBytes },
	};

	if (!NetCrypto_Sha384(pieces, 3, out))
	{
		NetCrypto_Wipe(out, kNetIdentityIdBytes);
		return false;
	}

	return true;
}

// An id as 96 small hex digits, as the list writes it, ended with a 0
inline void NetIdentity_IdHex(const uint8_t id[kNetIdentityIdBytes], char out[kNetIdentityIdHexBytes])
{
	static const char kDigits[] = "0123456789abcdef";

	for (size_t i = 0; i < kNetIdentityIdBytes; i++)
	{
		out[2 * i] = kDigits[id[i] >> 4];
		out[2 * i + 1] = kDigits[id[i] & 0x0F];
	}

	out[2 * kNetIdentityIdBytes] = 0;
}

// An id from its 96 hex digits, as a listing gives it. Only small letters are taken, as the list writes them, and exactly 96 digits with
// nothing after them: anything else is refused (false, with out wiped), because a listing is input from outside the game.
inline bool NetIdentity_IdFromHex(const char *hex, uint8_t out[kNetIdentityIdBytes])
{
	bool good = hex != NULL;

	for (size_t i = 0; good && i < 2 * kNetIdentityIdBytes; i++)
	{
		good = (hex[i] >= '0' && hex[i] <= '9') || (hex[i] >= 'a' && hex[i] <= 'f');
	}

	good = good && hex[2 * kNetIdentityIdBytes] == 0;

	if (!good)
	{
		NetCrypto_Wipe(out, kNetIdentityIdBytes);
		return false;
	}

	for (size_t i = 0; i < kNetIdentityIdBytes; i++)
	{
		char const high = hex[2 * i];
		char const low = hex[2 * i + 1];
		int const h = high <= '9' ? high - '0' : high - 'a' + 10;
		int const l = low <= '9' ? low - '0' : low - 'a' + 10;
		out[i] = static_cast<uint8_t>(h * 16 + l);
	}

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
		&& NetIdentity_Fingerprint(NetIdentity_P384Public(out), NetIdentity_KemPublic(out), out->fingerprint)
		&& NetIdentity_Id(NetIdentity_P384Public(out), NetIdentity_KemPublic(out), out->id);

	if (!made)
	{
		NetIdentity_Free(out);
		return false;
	}

	out->made = true;
	return true;
}

// ---- An identity kept between runs (#21). Added 2026-11-09 by Melanikus for the AvP port.

// Whether two secrets are the same, looking at every byte whatever it finds, so the time taken says nothing about where they differ
inline bool NetIdentity_SameSecret(const uint8_t *a, const uint8_t *b, size_t size)
{
	uint8_t differ = 0;

	for (size_t i = 0; i < size; i++)
	{
		differ |= (uint8_t)(a[i] ^ b[i]);
	}

	return differ == 0;
}

// Whether an identity's private halves really belong to its public ones, by using each pair once, as a handshake would.
// - P-384: a new key of its own agrees a secret with the public half, and the private half agrees one with the new key's public half. The
//   two secrets are the same only if the private half is that public half's.
// - ML-KEM: a secret sealed to the public half has to open, with the private half, to the same secret. A wrong private half doesn't refuse
//   (ML-KEM never does): it opens to a different secret.
// CNG doesn't promise to check that a private key it's given matches the public half given with it, which is why this does. Every secret
// is wiped.
inline bool NetIdentity_HalvesMatch(NetIdentity *id)
{
	uint8_t otherPublic[kNetCryptoP384PublicBytes];
	uint8_t one[kNetCryptoP384ScalarBytes];
	uint8_t two[kNetCryptoP384ScalarBytes];
	NetEcdhKey *const other = NetCrypto_EcdhNew(otherPublic);

	bool ok = other != NULL
		&& NetCrypto_EcdhAgree(other, NetIdentity_P384Public(id), one)
		&& NetCrypto_EcdhAgree(id->p384, otherPublic, two)
		&& NetIdentity_SameSecret(one, two, sizeof(one));

	NetCrypto_EcdhFree(other);
	NetCrypto_Wipe(one, sizeof(one));
	NetCrypto_Wipe(two, sizeof(two));

	uint8_t ciphertext[kNetPqKemCiphertextBytes];
	uint8_t sealed[kNetPqKemSecretBytes];
	uint8_t opened[kNetPqKemSecretBytes];

	ok = ok
		&& NetPq_KemEncapsulate(NetIdentity_KemPublic(id), ciphertext, sealed)
		&& NetPq_KemDecapsulate(id->kemPrivate, ciphertext, opened)
		&& NetIdentity_SameSecret(sealed, opened, sizeof(sealed));

	NetCrypto_Wipe(sealed, sizeof(sealed));
	NetCrypto_Wipe(opened, sizeof(opened));
	return ok;
}

// An identity from what was kept: its public halves, and its private halves just unsealed (net_identity_store.cpp). The private halves have
// to belong to the public ones (NetIdentity_HalvesMatch), and its fingerprint is worked out again from the public halves, never read. False,
// with nothing left of it (NetIdentity_Free, which wipes the private halves too), on any refusal: a kept identity that doesn't hold
// together is never used.
inline bool NetIdentity_FromKept(const uint8_t statics[kNetIdentityStaticBytes], const uint8_t p384Private[kNetCryptoP384ScalarBytes],
	const uint8_t kemPrivate[kNetPqKemPrivateBytes], NetIdentity *out)
{
	if (!out)
	{
		return false;
	}

	memset(out, 0, sizeof(*out));

	if (!statics || !p384Private || !kemPrivate)
	{
		return false;
	}

	memcpy(out->statics, statics, kNetIdentityStaticBytes);
	memcpy(out->kemPrivate, kemPrivate, kNetPqKemPrivateBytes);
	out->p384 = NetCrypto_EcdhFromPrivate(p384Private, NetIdentity_P384Public(out));

	bool const made = out->p384 != NULL
		&& NetIdentity_HalvesMatch(out)
		&& NetIdentity_Fingerprint(NetIdentity_P384Public(out), NetIdentity_KemPublic(out), out->fingerprint)
		&& NetIdentity_Id(NetIdentity_P384Public(out), NetIdentity_KemPublic(out), out->id);

	if (!made)
	{
		NetIdentity_Free(out);
		return false;
	}

	out->made = true;
	return true;
}

// The P-384 private number of an identity, for sealing it to keep (net_identity_store.cpp): nothing else may ask for it. False, with out
// wiped, for an identity that isn't whole. The caller wipes out once it's sealed. The ML-KEM private half is id->kemPrivate already.
inline bool NetIdentity_ExportP384Private(const NetIdentity *id, uint8_t out[kNetCryptoP384ScalarBytes])
{
	if (!out)
	{
		return false;
	}

	if (!id || !id->made)
	{
		NetCrypto_Wipe(out, kNetCryptoP384ScalarBytes);
		return false;
	}

	return NetCrypto_EcdhExportPrivate(id->p384, out);
}

#endif
