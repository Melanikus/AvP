// Checking the post-quantum half of a signature, ML-DSA-87. Written 2026-11-09 by Melanikus for the AvP port.
// pq_signature_check.h says what it's for and why it's a wrapper.

#include "pq_signature_check.h"

#include <mldsa-native/avpx_mldsa.h>

// Our sizes are the library's, or this doesn't build
static_assert(kPqSignatureBytes == MLDSA87_BYTES, "an ML-DSA-87 signature is 4,627 bytes");
static_assert(kPqSignaturePublicKeyBytes == MLDSA87_PUBLICKEYBYTES, "an ML-DSA-87 public key is 2,592 bytes");

bool Pq_SignatureIsValid(uint8_t const *signature, size_t signatureLength, uint8_t const *message, size_t messageLength,
                         uint8_t const *context, size_t contextLength, uint8_t const *publicKey, size_t publicKeyLength)
{
	if (signature == NULL || signatureLength != kPqSignatureBytes || publicKey == NULL || publicKeyLength != kPqSignaturePublicKeyBytes)
	{
		return false;
	}

	// Empty is fine, and may be NULL. Bytes that aren't there are not.
	if ((message == NULL && messageLength != 0) || (context == NULL && contextLength != 0))
	{
		return false;
	}

	// A context longer than FIPS 204's 255 bytes is refused by the library itself (MLD_ERR_INVALID_ARG), and Wycheproof's cases of it
	// show that it is (tests/pq_mldsa_test.cpp). It isn't checked here as well: a check that can be taken out with no test noticing is
	// a check nobody can show works.
	return avpx_mldsa87_verify(signature, message, messageLength, context, contextLength, publicKey) == 0;
}
