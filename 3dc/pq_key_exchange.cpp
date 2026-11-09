// The game's post-quantum key exchange, ML-KEM-1024. Written 2026-11-09 by Melanikus for the AvP port.
// pq_key_exchange.h says what it's for and why it's a wrapper.

#include "pq_key_exchange.h"

#include <mlkem-native/avpx_mlkem.h>

#include <windows.h>

// Our sizes are the library's, or this doesn't build
static_assert(kPqPublicKeyBytes == MLKEM1024_PUBLICKEYBYTES, "ML-KEM-1024's public key is 1,568 bytes");
static_assert(kPqSecretKeyBytes == MLKEM1024_SECRETKEYBYTES, "ML-KEM-1024's secret key is 3,168 bytes");
static_assert(kPqCiphertextBytes == MLKEM1024_CIPHERTEXTBYTES, "ML-KEM-1024's ciphertext is 1,568 bytes");
static_assert(kPqSharedSecretBytes == MLKEM_BYTES, "ML-KEM's shared secret is 32 bytes");

// Sets a buffer to zeros in a way the compiler can't leave out, even though nothing reads the buffer afterwards
static void Wipe(uint8_t *bytes, size_t length)
{
	if (bytes != NULL && length > 0)
	{
		SecureZeroMemory(bytes, length);
	}
}

// Whether a buffer is there and exactly the size wanted
static bool IsExactly(void const *bytes, size_t length, size_t wanted)
{
	return bytes != NULL && length == wanted;
}

bool Pq_NewKeyPair(uint8_t *publicKey, size_t publicKeyLength, uint8_t *secretKey, size_t secretKeyLength)
{
	if (IsExactly(publicKey, publicKeyLength, kPqPublicKeyBytes) && IsExactly(secretKey, secretKeyLength, kPqSecretKeyBytes))
	{
		if (avpx_mlkem1024_keypair(publicKey, secretKey) == 0)
		{
			return true;
		}
	}

	Wipe(publicKey, publicKeyLength);
	Wipe(secretKey, secretKeyLength);
	return false;
}

bool Pq_Encapsulate(uint8_t const *publicKey, size_t publicKeyLength, uint8_t *ciphertext, size_t ciphertextLength,
                    uint8_t *sharedSecret, size_t sharedSecretLength)
{
	if (IsExactly(publicKey, publicKeyLength, kPqPublicKeyBytes) && IsExactly(ciphertext, ciphertextLength, kPqCiphertextBytes) &&
	    IsExactly(sharedSecret, sharedSecretLength, kPqSharedSecretBytes))
	{
		if (avpx_mlkem1024_enc(ciphertext, sharedSecret, publicKey) == 0)
		{
			return true;
		}
	}

	Wipe(ciphertext, ciphertextLength);
	Wipe(sharedSecret, sharedSecretLength);
	return false;
}

bool Pq_Decapsulate(uint8_t const *ciphertext, size_t ciphertextLength, uint8_t const *secretKey, size_t secretKeyLength,
                    uint8_t *sharedSecret, size_t sharedSecretLength)
{
	if (IsExactly(ciphertext, ciphertextLength, kPqCiphertextBytes) && IsExactly(secretKey, secretKeyLength, kPqSecretKeyBytes) &&
	    IsExactly(sharedSecret, sharedSecretLength, kPqSharedSecretBytes))
	{
		if (avpx_mlkem1024_dec(sharedSecret, ciphertext, secretKey) == 0)
		{
			return true;
		}
	}

	Wipe(sharedSecret, sharedSecretLength);
	return false;
}
