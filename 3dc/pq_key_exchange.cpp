// The game's post-quantum key exchange, ML-KEM-1024. Written 2026-11-09 by Melanikus for the AvP port.
// pq_key_exchange.h says what it's for and why it's a wrapper.
// Modified 2026-11-09 by Melanikus for the AvP port (Fourth's cold read, findings 1 to 3): the sizes are held to the parameter set the
// library was built with, buffers that share a byte are refused, and a refusal never writes more into a buffer than ML-KEM-1024's own
// size for it.

#include "pq_key_exchange.h"

#include <mlkem-native/avpx_mlkem.h>

#include <windows.h>

// Our sizes are the library's, for the parameter set it was built with, or this doesn't build. The sizes named MLKEM1024_* wouldn't do
// for this: the library's header defines them for every set, so they'd match even a library built as ML-KEM-768.
static_assert(kPqPublicKeyBytes == MLKEM_PUBLICKEYBYTES(MLK_CONFIG_PARAMETER_SET), "ML-KEM-1024's public key is 1,568 bytes");
static_assert(kPqSecretKeyBytes == MLKEM_SECRETKEYBYTES(MLK_CONFIG_PARAMETER_SET), "ML-KEM-1024's secret key is 3,168 bytes");
static_assert(kPqCiphertextBytes == MLKEM_CIPHERTEXTBYTES(MLK_CONFIG_PARAMETER_SET), "ML-KEM-1024's ciphertext is 1,568 bytes");
static_assert(kPqSharedSecretBytes == MLKEM_BYTES, "ML-KEM's shared secret is 32 bytes");

// Sets the start of a buffer to zeros, in a way the compiler can't leave out even though nothing reads the buffer afterwards: length
// bytes, but never more than most, ML-KEM-1024's own size for that buffer. A refusal can be for a length that's wrong, and a length that's
// wrong may be bigger than the buffer really is, so this never writes more into a buffer than a call that worked would have.
static void Wipe(uint8_t *bytes, size_t length, size_t most)
{
	size_t const size = length < most ? length : most;

	if (bytes != NULL && size > 0)
	{
		SecureZeroMemory(bytes, size);
	}
}

// Whether a buffer is there and exactly the size wanted
static bool IsExactly(void const *bytes, size_t length, size_t wanted)
{
	return bytes != NULL && length == wanted;
}

// Whether two buffers share at least one byte. They're compared as plain numbers (uintptr_t), because C++ only promises what < means
// between two places in the same buffer. Only called once both lengths are known to be ML-KEM's own, so the sums can't go round.
static bool Overlap(void const *a, size_t aLength, void const *b, size_t bLength)
{
	uintptr_t const aStart = reinterpret_cast<uintptr_t>(a);
	uintptr_t const bStart = reinterpret_cast<uintptr_t>(b);
	return aStart < bStart + bLength && bStart < aStart + aLength;
}

bool Pq_NewKeyPair(uint8_t *publicKey, size_t publicKeyLength, uint8_t *secretKey, size_t secretKeyLength)
{
	if (IsExactly(publicKey, publicKeyLength, kPqPublicKeyBytes) && IsExactly(secretKey, secretKeyLength, kPqSecretKeyBytes))
	{
		// The library may write one while it still reads the other, so the two may share no byte (pq_key_exchange.h)
		bool const apart = !Overlap(publicKey, kPqPublicKeyBytes, secretKey, kPqSecretKeyBytes);

		if (apart && avpx_mlkem1024_keypair(publicKey, secretKey) == 0)
		{
			return true;
		}
	}

	Wipe(publicKey, publicKeyLength, kPqPublicKeyBytes);
	Wipe(secretKey, secretKeyLength, kPqSecretKeyBytes);
	return false;
}

bool Pq_Encapsulate(uint8_t const *publicKey, size_t publicKeyLength, uint8_t *ciphertext, size_t ciphertextLength,
                    uint8_t *sharedSecret, size_t sharedSecretLength)
{
	if (IsExactly(publicKey, publicKeyLength, kPqPublicKeyBytes) && IsExactly(ciphertext, ciphertextLength, kPqCiphertextBytes) &&
	    IsExactly(sharedSecret, sharedSecretLength, kPqSharedSecretBytes))
	{
		bool const apart = !Overlap(publicKey, kPqPublicKeyBytes, ciphertext, kPqCiphertextBytes) &&
		                   !Overlap(publicKey, kPqPublicKeyBytes, sharedSecret, kPqSharedSecretBytes) &&
		                   !Overlap(ciphertext, kPqCiphertextBytes, sharedSecret, kPqSharedSecretBytes);

		if (apart && avpx_mlkem1024_enc(ciphertext, sharedSecret, publicKey) == 0)
		{
			return true;
		}
	}

	Wipe(ciphertext, ciphertextLength, kPqCiphertextBytes);
	Wipe(sharedSecret, sharedSecretLength, kPqSharedSecretBytes);
	return false;
}

bool Pq_Decapsulate(uint8_t const *ciphertext, size_t ciphertextLength, uint8_t const *secretKey, size_t secretKeyLength,
                    uint8_t *sharedSecret, size_t sharedSecretLength)
{
	if (IsExactly(ciphertext, ciphertextLength, kPqCiphertextBytes) && IsExactly(secretKey, secretKeyLength, kPqSecretKeyBytes) &&
	    IsExactly(sharedSecret, sharedSecretLength, kPqSharedSecretBytes))
	{
		// The ciphertext and the secret key are only read, so they may share bytes with each other, but not with the secret written
		bool const apart = !Overlap(ciphertext, kPqCiphertextBytes, sharedSecret, kPqSharedSecretBytes) &&
		                   !Overlap(secretKey, kPqSecretKeyBytes, sharedSecret, kPqSharedSecretBytes);

		if (apart && avpx_mlkem1024_dec(sharedSecret, ciphertext, secretKey) == 0)
		{
			return true;
		}
	}

	Wipe(sharedSecret, sharedSecretLength, kPqSharedSecretBytes);
	return false;
}
