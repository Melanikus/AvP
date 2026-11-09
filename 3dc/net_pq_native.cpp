// The real post-quantum half of protocol version 2 (net_pq.h): ML-KEM-1024 and ML-DSA-87, from the PQ Code Package's mlkem-native and
// mldsa-native v2.0.0, built from their source. Written 2026-11-09 by Melanikus for the AvP port.
//
// WHAT THIS IS. net_pq.h is what version 2 asks of a post-quantum half. Until the libraries had been read (CRYPTO.md section 3.1), the
// game was built with net_pq_none.cpp, which refuses everything. This file takes its place: the same calls, now done.
//
// HOW. It's only a translation: every call goes to Secondary's wrapper (pq_key_exchange.h and pq_signature_check.h), which is where the
// care is:
//   - every buffer is checked to be exactly its size before the library sees it;
//   - every answer of the library is looked at, and anything but "done" is a refusal;
//   - on a refusal, every output is wiped to 0.
// The wrapper's own tests (pq_mlkem_test, pq_mldsa_test) hold it to NIST's and Wycheproof's published answers. net_pq_native_test holds
// this file to what net_pq.h promises.
//
// THE RANDOMNESS. A key pair and an encapsulation draw their randomness inside the library, from Windows' own generator
// (BCryptGenRandom, avpx_mlkem_config.h), the same source as NetCrypto_Random. There's no call here that takes fixed randomness: test
// vectors reach the library's own "_derand" calls from the tests alone, so the game the players get can't be told to use any.
//
// ML-DSA'S CONTEXT. Always the empty one (net_pq.h, and CRYPTO.md section 5.1 point 16): the relay's Java can sign with no other.

#include "net_pq.h"

#include "pq_key_exchange.h"
#include "pq_signature_check.h"

// net_pq.h's sizes are the wrapper's, or this doesn't build
static_assert(kNetPqKemPublicBytes == kPqPublicKeyBytes, "an ML-KEM-1024 encapsulation key is 1,568 bytes");
static_assert(kNetPqKemPrivateBytes == kPqSecretKeyBytes, "an ML-KEM-1024 decapsulation key is 3,168 bytes");
static_assert(kNetPqKemCiphertextBytes == kPqCiphertextBytes, "an ML-KEM-1024 ciphertext is 1,568 bytes");
static_assert(kNetPqKemSecretBytes == kPqSharedSecretBytes, "ML-KEM's shared secret is 32 bytes");
static_assert(kNetPqDsaPublicBytes == kPqSignaturePublicKeyBytes, "an ML-DSA-87 public key is 2,592 bytes");
static_assert(kNetPqDsaSignatureBytes == kPqSignatureBytes, "an ML-DSA-87 signature is 4,627 bytes");

bool NetPq_Available()
{
	return true;
}

const char *NetPq_Name()
{
	return "mlkem-native and mldsa-native v2.0.0, built from source";
}

bool NetPq_KemKeypair(uint8_t publicOut[kNetPqKemPublicBytes], uint8_t privateOut[kNetPqKemPrivateBytes])
{
	return Pq_NewKeyPair(publicOut, kNetPqKemPublicBytes, privateOut, kNetPqKemPrivateBytes);
}

bool NetPq_KemEncapsulate(const uint8_t publicKey[kNetPqKemPublicBytes], uint8_t ciphertextOut[kNetPqKemCiphertextBytes],
	uint8_t secretOut[kNetPqKemSecretBytes])
{
	// The wrapper refuses a missing public key by its length check, and wipes both outputs
	return Pq_Encapsulate(publicKey, kNetPqKemPublicBytes, ciphertextOut, kNetPqKemCiphertextBytes, secretOut, kNetPqKemSecretBytes);
}

bool NetPq_KemDecapsulate(const uint8_t privateKey[kNetPqKemPrivateBytes], const uint8_t ciphertext[kNetPqKemCiphertextBytes],
	uint8_t secretOut[kNetPqKemSecretBytes])
{
	return Pq_Decapsulate(ciphertext, kNetPqKemCiphertextBytes, privateKey, kNetPqKemPrivateBytes, secretOut, kNetPqKemSecretBytes);
}

bool NetPq_DsaVerify(const uint8_t publicKey[kNetPqDsaPublicBytes], const uint8_t *message, size_t messageSize,
	const uint8_t signature[kNetPqDsaSignatureBytes])
{
	// The empty context: no bytes, and no pointer
	return Pq_SignatureIsValid(signature, kNetPqDsaSignatureBytes, message, messageSize, NULL, 0, publicKey, kNetPqDsaPublicBytes);
}
