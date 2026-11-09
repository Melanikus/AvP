// The classical half of protocol version 2's cryptography (net_crypto.h), done by Windows' own CNG (bcrypt.dll). Written 2026-11-09 by
// Melanikus for the AvP port.
//
// Every call opens what it needs from CNG, uses it and closes it again, so nothing is shared between calls or threads. That's slower than
// keeping things open, which doesn't matter for a handshake or the relay's few control messages. Sealing every game packet (CRYPTO.md
// section 9) will keep its key open instead.
//
// Every CNG answer is checked, and any failure gives back false with every output wiped (net_crypto.h, HOW IT ANSWERS). Every buffer that
// held a key or a secret is wiped before it's let go of, and every CNG key is destroyed.
//
// TWO THINGS ABOUT CNG TO KNOW:
//   - The secret two P-384 keys agree on comes out of CNG with its bytes in the reverse order (little-endian), when asked for it raw
//     (BCRYPT_KDF_RAW_SECRET). Java, Node and the standard all give it big-endian, so it's turned round here.
//   - A public key goes into CNG with a small header (BCRYPT_ECCKEY_BLOB: which curve and kind, and the size of X), then X, then Y. On the
//     wire it's 0x04, X, Y. CNG checks that the point is on the curve when it's imported; tests/net_crypto_test.cpp proves it refuses one
//     that isn't, and NetCrypto_EcdhAgree also refuses an agreed secret of all 0.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>

#include <string.h>

#include <new>

#include "net_crypto.h"

// CNG's own library, which comes with Windows
#pragma comment(lib, "bcrypt.lib")

// The game is built for Windows 8 (_WIN32_WINNT 0x0602), and bcrypt.h names this only for 8.1 and later. Windows has had it since 8.1,
// and online play needs Windows 10 anyway, so its value, from bcrypt.h, is written here.
#ifndef BCRYPT_KDF_RAW_SECRET
#define BCRYPT_KDF_RAW_SECRET L"TRUNCATE"
#endif

#ifndef NT_SUCCESS
#define NT_SUCCESS(status) (((NTSTATUS)(status)) >= 0)
#endif

namespace
{
	// A P-384 public key as CNG takes and gives it: its header, then X, then Y
	struct EccPublicBlob
	{
		BCRYPT_ECCKEY_BLOB header;
		uint8_t xy[2 * kNetCryptoP384ScalarBytes];
	};

	// And a private key: its header, X, Y, then the private number d
	struct EccPrivateBlob
	{
		BCRYPT_ECCKEY_BLOB header;
		uint8_t xyd[3 * kNetCryptoP384ScalarBytes];
	};

	// A public key from the wire into CNG's form. False if it doesn't start with 0x04, the only form version 2 uses.
	bool ToBlob(const uint8_t wire[kNetCryptoP384PublicBytes], ULONG magic, EccPublicBlob *blob)
	{
		if (!wire || wire[0] != 0x04)
		{
			return false;
		}

		blob->header.dwMagic = magic;
		blob->header.cbKey = static_cast<ULONG>(kNetCryptoP384ScalarBytes);
		memcpy(blob->xy, wire + 1, sizeof(blob->xy));
		return true;
	}

	// A public key out of CNG, onto the wire's form
	bool FromKey(BCRYPT_KEY_HANDLE key, ULONG magic, uint8_t wire[kNetCryptoP384PublicBytes])
	{
		EccPublicBlob blob;
		ULONG got = 0;

		if (!NT_SUCCESS(BCryptExportKey(key, NULL, BCRYPT_ECCPUBLIC_BLOB, reinterpret_cast<PUCHAR>(&blob), sizeof(blob), &got, 0))
			|| got != sizeof(blob) || blob.header.dwMagic != magic || blob.header.cbKey != kNetCryptoP384ScalarBytes)
		{
			return false;
		}

		wire[0] = 0x04;
		memcpy(wire + 1, blob.xy, sizeof(blob.xy));
		return true;
	}

	bool AllZero(const uint8_t *bytes, size_t size)
	{
		uint8_t any = 0;

		for (size_t i = 0; i < size; i++)
		{
			any |= bytes[i];
		}

		return any == 0;
	}

	// A hash, or an HMAC with key, of the pieces one after another
	bool Hash(bool hmac, const uint8_t *key, size_t keySize, const NetCryptoPiece *pieces, size_t count, uint8_t out[kNetCryptoHashBytes])
	{
		BCRYPT_ALG_HANDLE algorithm = NULL;
		BCRYPT_HASH_HANDLE hash = NULL;
		bool ok = out != NULL && (count == 0 || pieces != NULL) && (!hmac || keySize == 0 || key != NULL) && keySize <= 0xFFFFFFFFu;

		// CNG wants a key's bytes even when there are none; one byte that's never read stands in for them
		uint8_t const none = 0;

		ok = ok && NT_SUCCESS(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA384_ALGORITHM, NULL, hmac ? BCRYPT_ALG_HANDLE_HMAC_FLAG : 0));
		ok = ok && NT_SUCCESS(BCryptCreateHash(algorithm, &hash, NULL, 0, hmac ? const_cast<PUCHAR>(keySize > 0 ? key : &none) : NULL,
			hmac ? static_cast<ULONG>(keySize) : 0, 0));

		for (size_t i = 0; ok && i < count; i++)
		{
			if (pieces[i].size == 0)
			{
				continue;
			}

			ok = pieces[i].bytes != NULL && pieces[i].size <= 0xFFFFFFFFu
				&& NT_SUCCESS(BCryptHashData(hash, const_cast<PUCHAR>(pieces[i].bytes), static_cast<ULONG>(pieces[i].size), 0));
		}

		ok = ok && NT_SUCCESS(BCryptFinishHash(hash, out, static_cast<ULONG>(kNetCryptoHashBytes), 0));

		if (hash)
		{
			BCryptDestroyHash(hash);
		}

		if (algorithm)
		{
			BCryptCloseAlgorithmProvider(algorithm, 0);
		}

		if (!ok && out)
		{
			NetCrypto_Wipe(out, kNetCryptoHashBytes);
		}

		return ok;
	}

	// AES-256-GCM, either way. Its key lives only for this call.
	bool AesGcm(bool seal, const uint8_t key[kNetCryptoAesKeyBytes], const uint8_t nonce[kNetCryptoNonceBytes], const uint8_t *aad, size_t aadSize,
		const uint8_t *in, size_t size, uint8_t *out, uint8_t tag[kNetCryptoTagBytes])
	{
		if (!key || !nonce || !tag || (aadSize > 0 && !aad) || (size > 0 && (!in || !out)) || aadSize > 0xFFFFFFFFu || size > 0xFFFFFFFFu)
		{
			return false;
		}

		BCRYPT_ALG_HANDLE algorithm = NULL;
		BCRYPT_KEY_HANDLE handle = NULL;
		bool ok = NT_SUCCESS(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_AES_ALGORITHM, NULL, 0));
		ok = ok && NT_SUCCESS(BCryptSetProperty(algorithm, BCRYPT_CHAINING_MODE, reinterpret_cast<PUCHAR>(const_cast<wchar_t *>(BCRYPT_CHAIN_MODE_GCM)),
			sizeof(BCRYPT_CHAIN_MODE_GCM), 0));
		ok = ok && NT_SUCCESS(BCryptGenerateSymmetricKey(algorithm, &handle, NULL, 0, const_cast<PUCHAR>(key), static_cast<ULONG>(kNetCryptoAesKeyBytes), 0));

		// The nonce, the aad and the tag go in this; the text itself goes through BCryptEncrypt or BCryptDecrypt
		BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
		BCRYPT_INIT_AUTH_MODE_INFO(info);
		info.pbNonce = const_cast<PUCHAR>(nonce);
		info.cbNonce = static_cast<ULONG>(kNetCryptoNonceBytes);
		info.pbAuthData = aadSize > 0 ? const_cast<PUCHAR>(aad) : NULL;
		info.cbAuthData = static_cast<ULONG>(aadSize);
		info.pbTag = tag;
		info.cbTag = static_cast<ULONG>(kNetCryptoTagBytes);

		// An empty text is sealed too: then only the tag comes out. CNG still wants somewhere to point, so one byte stands in.
		uint8_t spare = 0;
		PUCHAR const from = size > 0 ? const_cast<PUCHAR>(in) : &spare;
		PUCHAR const to = size > 0 ? out : &spare;
		ULONG done = 0;

		if (ok)
		{
			NTSTATUS const status = seal ? BCryptEncrypt(handle, from, static_cast<ULONG>(size), &info, NULL, 0, to, static_cast<ULONG>(size), &done, 0)
				: BCryptDecrypt(handle, from, static_cast<ULONG>(size), &info, NULL, 0, to, static_cast<ULONG>(size), &done, 0);
			ok = NT_SUCCESS(status) && done == size;
		}

		if (handle)
		{
			BCryptDestroyKey(handle);
		}

		if (algorithm)
		{
			BCryptCloseAlgorithmProvider(algorithm, 0);
		}

		if (!ok)
		{
			if (size > 0 && out)
			{
				NetCrypto_Wipe(out, size);
			}

			if (seal)
			{
				NetCrypto_Wipe(tag, kNetCryptoTagBytes);
			}
		}

		return ok;
	}
}

// A P-384 key inside CNG, with the algorithm it was made by, which is closed only after the key is destroyed
struct NetEcdhKey
{
	BCRYPT_ALG_HANDLE algorithm;
	BCRYPT_KEY_HANDLE key;
};

void NetCrypto_Wipe(void *bytes, size_t size)
{
	if (bytes && size > 0)
	{
		SecureZeroMemory(bytes, size);
	}
}

bool NetCrypto_Random(uint8_t *out, size_t size)
{
	if (!out || size > 0xFFFFFFFFu)
	{
		return false;
	}

	if (size == 0)
	{
		return true;
	}

	if (!NT_SUCCESS(BCryptGenRandom(NULL, out, static_cast<ULONG>(size), BCRYPT_USE_SYSTEM_PREFERRED_RNG)))
	{
		NetCrypto_Wipe(out, size);
		return false;
	}

	return true;
}

bool NetCrypto_Sha384(const NetCryptoPiece *pieces, size_t count, uint8_t out[kNetCryptoHashBytes])
{
	return Hash(false, NULL, 0, pieces, count, out);
}

bool NetCrypto_HmacSha384(const uint8_t *key, size_t keySize, const NetCryptoPiece *pieces, size_t count, uint8_t out[kNetCryptoHashBytes])
{
	return Hash(true, key, keySize, pieces, count, out);
}

NetEcdhKey *NetCrypto_EcdhNew(uint8_t publicOut[kNetCryptoP384PublicBytes])
{
	if (!publicOut)
	{
		return NULL;
	}

	NetEcdhKey made = { NULL, NULL };
	bool ok = NT_SUCCESS(BCryptOpenAlgorithmProvider(&made.algorithm, BCRYPT_ECDH_P384_ALGORITHM, NULL, 0));
	ok = ok && NT_SUCCESS(BCryptGenerateKeyPair(made.algorithm, &made.key, 384, 0));
	ok = ok && NT_SUCCESS(BCryptFinalizeKeyPair(made.key, 0));
	ok = ok && FromKey(made.key, BCRYPT_ECDH_PUBLIC_P384_MAGIC, publicOut);
	NetEcdhKey *const kept = ok ? new (std::nothrow) NetEcdhKey(made) : NULL;

	if (!kept)
	{
		NetCrypto_Wipe(publicOut, kNetCryptoP384PublicBytes);

		if (made.key)
		{
			BCryptDestroyKey(made.key);
		}

		if (made.algorithm)
		{
			BCryptCloseAlgorithmProvider(made.algorithm, 0);
		}
	}

	return kept;
}

NetEcdhKey *NetCrypto_EcdhFromPrivate(const uint8_t privateKey[kNetCryptoP384ScalarBytes], const uint8_t publicKey[kNetCryptoP384PublicBytes])
{
	if (!privateKey || !publicKey || publicKey[0] != 0x04)
	{
		return NULL;
	}

	EccPrivateBlob blob;
	blob.header.dwMagic = BCRYPT_ECDH_PRIVATE_P384_MAGIC;
	blob.header.cbKey = static_cast<ULONG>(kNetCryptoP384ScalarBytes);
	memcpy(blob.xyd, publicKey + 1, 2 * kNetCryptoP384ScalarBytes);
	memcpy(blob.xyd + 2 * kNetCryptoP384ScalarBytes, privateKey, kNetCryptoP384ScalarBytes);

	NetEcdhKey made = { NULL, NULL };
	bool ok = NT_SUCCESS(BCryptOpenAlgorithmProvider(&made.algorithm, BCRYPT_ECDH_P384_ALGORITHM, NULL, 0));
	ok = ok && NT_SUCCESS(BCryptImportKeyPair(made.algorithm, NULL, BCRYPT_ECCPRIVATE_BLOB, &made.key, reinterpret_cast<PUCHAR>(&blob), sizeof(blob), 0));
	NetCrypto_Wipe(&blob, sizeof(blob));
	NetEcdhKey *const kept = ok ? new (std::nothrow) NetEcdhKey(made) : NULL;

	if (!kept)
	{
		if (made.key)
		{
			BCryptDestroyKey(made.key);
		}

		if (made.algorithm)
		{
			BCryptCloseAlgorithmProvider(made.algorithm, 0);
		}
	}

	return kept;
}

// Added 2026-11-09 by Melanikus for the AvP port (#21): see net_crypto.h. A key BCryptGenerateKeyPair made can be exported this way; the
// blob is X, Y and then d, and only d is given back.
bool NetCrypto_EcdhExportPrivate(NetEcdhKey *key, uint8_t privateOut[kNetCryptoP384ScalarBytes])
{
	if (!privateOut)
	{
		return false;
	}

	EccPrivateBlob blob;
	ULONG got = 0;
	bool const ok = key && key->key &&
		NT_SUCCESS(BCryptExportKey(key->key, NULL, BCRYPT_ECCPRIVATE_BLOB, reinterpret_cast<PUCHAR>(&blob), sizeof(blob), &got, 0)) &&
		got == sizeof(blob) && blob.header.dwMagic == BCRYPT_ECDH_PRIVATE_P384_MAGIC && blob.header.cbKey == kNetCryptoP384ScalarBytes;

	if (ok)
	{
		memcpy(privateOut, blob.xyd + 2 * kNetCryptoP384ScalarBytes, kNetCryptoP384ScalarBytes);
	}
	else
	{
		NetCrypto_Wipe(privateOut, kNetCryptoP384ScalarBytes);
	}

	NetCrypto_Wipe(&blob, sizeof(blob));
	return ok;
}

bool NetCrypto_EcdhAgree(NetEcdhKey *mine, const uint8_t theirs[kNetCryptoP384PublicBytes], uint8_t secretOut[kNetCryptoP384ScalarBytes])
{
	if (!secretOut)
	{
		return false;
	}

	EccPublicBlob blob;
	BCRYPT_KEY_HANDLE their = NULL;
	BCRYPT_SECRET_HANDLE secret = NULL;
	uint8_t raw[kNetCryptoP384ScalarBytes];
	ULONG got = 0;
	bool ok = mine != NULL && ToBlob(theirs, BCRYPT_ECDH_PUBLIC_P384_MAGIC, &blob);

	// The import is where CNG checks that their point is on the curve
	ok = ok && NT_SUCCESS(BCryptImportKeyPair(mine->algorithm, NULL, BCRYPT_ECCPUBLIC_BLOB, &their, reinterpret_cast<PUCHAR>(&blob), sizeof(blob), 0));
	ok = ok && NT_SUCCESS(BCryptSecretAgreement(mine->key, their, &secret, 0));
	ok = ok && NT_SUCCESS(BCryptDeriveKey(secret, BCRYPT_KDF_RAW_SECRET, NULL, raw, sizeof(raw), &got, 0)) && got == sizeof(raw);

	// CNG gives the secret little-endian: turned round, it's the X the standard and the other implementations give
	for (size_t i = 0; ok && i < kNetCryptoP384ScalarBytes; i++)
	{
		secretOut[i] = raw[kNetCryptoP384ScalarBytes - 1 - i];
	}

	ok = ok && !AllZero(secretOut, kNetCryptoP384ScalarBytes);
	NetCrypto_Wipe(raw, sizeof(raw));

	if (secret)
	{
		BCryptDestroySecret(secret);
	}

	if (their)
	{
		BCryptDestroyKey(their);
	}

	if (!ok)
	{
		NetCrypto_Wipe(secretOut, kNetCryptoP384ScalarBytes);
	}

	return ok;
}

void NetCrypto_EcdhFree(NetEcdhKey *key)
{
	if (!key)
	{
		return;
	}

	if (key->key)
	{
		BCryptDestroyKey(key->key);
	}

	if (key->algorithm)
	{
		BCryptCloseAlgorithmProvider(key->algorithm, 0);
	}

	NetCrypto_Wipe(key, sizeof(*key));
	delete key;
}

bool NetCrypto_AesGcmSeal(const uint8_t key[kNetCryptoAesKeyBytes], const uint8_t nonce[kNetCryptoNonceBytes], const uint8_t *aad, size_t aadSize,
	const uint8_t *text, size_t size, uint8_t *out, uint8_t tag[kNetCryptoTagBytes])
{
	return AesGcm(true, key, nonce, aad, aadSize, text, size, out, tag);
}

bool NetCrypto_AesGcmOpen(const uint8_t key[kNetCryptoAesKeyBytes], const uint8_t nonce[kNetCryptoNonceBytes], const uint8_t *aad, size_t aadSize,
	const uint8_t *sealed, size_t size, const uint8_t tag[kNetCryptoTagBytes], uint8_t *out)
{
	// CNG writes the tag it's given into its own record: a copy is handed over, so the caller's stays as it was
	uint8_t given[kNetCryptoTagBytes];

	if (!tag)
	{
		return false;
	}

	memcpy(given, tag, sizeof(given));
	bool const ok = AesGcm(false, key, nonce, aad, aadSize, sealed, size, out, given);
	NetCrypto_Wipe(given, sizeof(given));
	return ok;
}
