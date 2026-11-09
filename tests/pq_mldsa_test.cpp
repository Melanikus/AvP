// Tests of the game's post-quantum signature check, ML-DSA-87: 3dc/pq_signature_check.cpp, and the library under it, mldsa-native, as
// vcpkg builds it for the game (vcpkg-overlay-ports/mldsa-native). Written 2026-11-09 by Melanikus for the AvP port.
//
// The library's upstream checks its code with proofs, but those are for 64-bit builds by other compilers, not for this one: 32-bit,
// Microsoft's compiler, with our settings. So this runs the published answers on our build itself:
// 1. the library's own known answer (its test/test_vectors/expected_test_vectors.h, from the same release, installed with it);
// 2. every ML-DSA-87 signature check NIST published for FIPS 204 (ACVP sigVer, pure ML-DSA through the external interface);
// 3. every one of Wycheproof's ML-DSA-87 cases: good signatures at the edges, and bad ones of every kind it knows (hints badly encoded,
//    numbers too big, wrong lengths, a context over 255 bytes);
// 4. what the wrapper adds: the sizes it refuses, and missing buffers.
// The cases are in tests/crypto-kat (its README.md says where from). NIST's keyGen and sigGen cases need key making and signing, which
// the game's library is built without: they're read, counted and named, not run.

#include "pq_signature_check.h"

#include <mldsa-native/avpx_mldsa.h>
#include <mldsa-native/upstream-tests/expected_test_vectors.h>

#include "crypto_kat_reader.h"

#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

static int dsaChecksFailed = 0;

// Called DsaCheck, not Check: Snyk Code joins functions with the same name and signature across files, and many tests have a Check
static void DsaCheck(bool good, std::string const &what)
{
	printf("%s  %s\n", good ? "ok    " : "FAILED", what.c_str());
	fflush(stdout);
	dsaChecksFailed += good ? 0 : 1;
}

typedef std::vector<uint8_t> Bytes;

// An empty field is given as NULL, so the wrapper's "may be NULL when empty" is tried on every case that has one
static uint8_t const *DataOrNull(Bytes const &bytes)
{
	return bytes.empty() ? NULL : bytes.data();
}

static std::string Text(KatCase const &one, char const *name)
{
	auto const found = one.find(name);
	return found == one.end() ? std::string() : found->second;
}

static bool ReadKat(std::string const &top, std::string const &name, KatFile &file)
{
	std::string why;
	bool const read = KatRead(top, name, file, why);
	DsaCheck(read, read ? "read tests/crypto-kat/" + name + ", " + std::to_string(file.cases.size()) + " cases" : why);
	return read;
}

// Runs every case of a verify file through the wrapper. pkName, messageName and signatureName are the file's own names for them.
static void TestVerifyFile(std::string const &top, std::string const &name, char const *pkName, char const *messageName, char const *signatureName)
{
	KatFile file;

	if (!ReadKat(top, name, file))
	{
		return;
	}

	int tried = 0, failed = 0;

	for (KatCase const &one : file.cases)
	{
		tried++;
		Bytes pk, message, context, signature;
		bool const read = KatBytes(Text(one, pkName), pk) && KatBytes(Text(one, messageName), message) &&
		                  KatBytes(Text(one, "context"), context) && KatBytes(Text(one, signatureName), signature);
		std::string const result = Text(one, "result");
		std::string why;

		if (!read || (result != "valid" && result != "invalid"))
		{
			why = "a field is missing or isn't hex, or the result is neither valid nor invalid";
		}
		else
		{
			bool const said = Pq_SignatureIsValid(DataOrNull(signature), signature.size(), DataOrNull(message), message.size(),
			                                      DataOrNull(context), context.size(), DataOrNull(pk), pk.size());

			if (said != (result == "valid"))
			{
				why = said ? "a signature that must be refused was taken" : "a good signature was refused";
			}
		}

		if (!why.empty())
		{
			failed++;

			if (failed <= 10)
			{
				DsaCheck(false, name + " case " + Text(one, "id") + " (" + Text(one, "flags") + "): " + why);
			}
		}
	}

	DsaCheck(tried > 0 && failed == 0, name + ": " + std::to_string(tried - failed) + " of " + std::to_string(tried) +
	                                       " cases are taken or refused, as marked");
}

// NIST's cases for what the game's library can't do: read, so a damaged file still fails, then counted and named
static void CountNotForTheGame(std::string const &top, std::string const &name, size_t expected, char const *why)
{
	KatFile file;

	if (ReadKat(top, name, file))
	{
		DsaCheck(file.cases.size() == expected, name + ": " + std::to_string(file.cases.size()) + " cases not for the game, " + why +
		                                            " (the file has " + std::to_string(expected) + ")");
	}
}

// 1. The library's own known answer: its signature of its message, with its context, by its key
static void TestTheLibrarysOwnAnswer()
{
	uint8_t const *const message = reinterpret_cast<uint8_t const *>(TEST_VECTOR_MSG);
	uint8_t const *const context = reinterpret_cast<uint8_t const *>(TEST_VECTOR_CTX);

	DsaCheck(Pq_SignatureIsValid(test_vector_sig, sizeof test_vector_sig, message, TEST_VECTOR_MSG_LEN, context, TEST_VECTOR_CTX_LEN,
	                             test_vector_pk, sizeof test_vector_pk),
	         "the library's own answer: its signature checks");

	Bytes changed(test_vector_sig, test_vector_sig + sizeof test_vector_sig);
	changed[10] ^= 0x01;
	DsaCheck(!Pq_SignatureIsValid(changed.data(), changed.size(), message, TEST_VECTOR_MSG_LEN, context, TEST_VECTOR_CTX_LEN,
	                              test_vector_pk, sizeof test_vector_pk),
	         "with one bit of the signature changed, it doesn't");

	DsaCheck(!Pq_SignatureIsValid(test_vector_sig, sizeof test_vector_sig, message, TEST_VECTOR_MSG_LEN - 1, context, TEST_VECTOR_CTX_LEN,
	                              test_vector_pk, sizeof test_vector_pk),
	         "for a message one byte shorter, it doesn't");

	DsaCheck(!Pq_SignatureIsValid(test_vector_sig, sizeof test_vector_sig, message, TEST_VECTOR_MSG_LEN, NULL, 0, test_vector_pk,
	                              sizeof test_vector_pk),
	         "with no context, it doesn't: the context is part of what was signed");
}

// 4. The wrapper on its own: sizes one off, missing buffers, with the library's own good signature, so only that one thing is wrong
static void TestTheWrapper()
{
	uint8_t const *const message = reinterpret_cast<uint8_t const *>(TEST_VECTOR_MSG);
	uint8_t const *const context = reinterpret_cast<uint8_t const *>(TEST_VECTOR_CTX);

	// One byte more than each, so a length one long is still inside the buffer, with the good bytes first
	Bytes signature(test_vector_sig, test_vector_sig + sizeof test_vector_sig);
	signature.push_back(0);
	Bytes pk(test_vector_pk, test_vector_pk + sizeof test_vector_pk);
	pk.push_back(0);

	DsaCheck(!Pq_SignatureIsValid(signature.data(), kPqSignatureBytes + 1, message, TEST_VECTOR_MSG_LEN, context, TEST_VECTOR_CTX_LEN,
	                              pk.data(), kPqSignaturePublicKeyBytes),
	         "a good signature with one byte more is refused");
	DsaCheck(!Pq_SignatureIsValid(signature.data(), kPqSignatureBytes - 1, message, TEST_VECTOR_MSG_LEN, context, TEST_VECTOR_CTX_LEN,
	                              pk.data(), kPqSignaturePublicKeyBytes),
	         "a signature one byte short is refused");
	DsaCheck(!Pq_SignatureIsValid(signature.data(), kPqSignatureBytes, message, TEST_VECTOR_MSG_LEN, context, TEST_VECTOR_CTX_LEN,
	                              pk.data(), kPqSignaturePublicKeyBytes + 1),
	         "a good key with one byte more is refused");
	DsaCheck(!Pq_SignatureIsValid(signature.data(), kPqSignatureBytes, message, TEST_VECTOR_MSG_LEN, context, TEST_VECTOR_CTX_LEN,
	                              pk.data(), kPqSignaturePublicKeyBytes - 1),
	         "a key one byte short is refused");

	DsaCheck(!Pq_SignatureIsValid(NULL, kPqSignatureBytes, message, TEST_VECTOR_MSG_LEN, context, TEST_VECTOR_CTX_LEN, pk.data(),
	                              kPqSignaturePublicKeyBytes),
	         "a missing signature is refused");
	DsaCheck(!Pq_SignatureIsValid(signature.data(), kPqSignatureBytes, message, TEST_VECTOR_MSG_LEN, context, TEST_VECTOR_CTX_LEN, NULL,
	                              kPqSignaturePublicKeyBytes),
	         "a missing key is refused");
	DsaCheck(!Pq_SignatureIsValid(signature.data(), kPqSignatureBytes, NULL, TEST_VECTOR_MSG_LEN, context, TEST_VECTOR_CTX_LEN, pk.data(),
	                              kPqSignaturePublicKeyBytes),
	         "a missing message that claims to have bytes is refused");
	DsaCheck(!Pq_SignatureIsValid(signature.data(), kPqSignatureBytes, message, TEST_VECTOR_MSG_LEN, NULL, TEST_VECTOR_CTX_LEN, pk.data(),
	                              kPqSignaturePublicKeyBytes),
	         "a missing context that claims to have bytes is refused");

	// And with everything right, the same buffers give yes, so each refusal above was for its one change
	DsaCheck(Pq_SignatureIsValid(signature.data(), kPqSignatureBytes, message, TEST_VECTOR_MSG_LEN, context, TEST_VECTOR_CTX_LEN, pk.data(),
	                             kPqSignaturePublicKeyBytes),
	         "and the same buffers at their right sizes check");
}

// The settings the library was built with, as its installed header says (the same file the build read)
static void TestTheSettings()
{
	DsaCheck(MLD_CONFIG_PARAMETER_SET == 87, "the library is ML-DSA-87");
#if defined(MLD_CONFIG_NO_SIGN_API) && defined(MLD_CONFIG_NO_KEYPAIR_API) && defined(MLD_CONFIG_NO_RANDOMIZED_API)
	DsaCheck(true, "the library only checks signatures: no key making, no signing, nothing random");
#else
	DsaCheck(false, "the library only checks signatures: no key making, no signing, nothing random");
#endif
#if defined(MLD_CONFIG_REDUCE_RAM)
	DsaCheck(MLD_TOTAL_ALLOC_87_VERIFY == 12512, "the library uses the smaller memory: 12,512 bytes to check a signature");
#else
	DsaCheck(false, "the library uses the smaller memory (MLD_CONFIG_REDUCE_RAM)");
#endif
#if defined(MLD_CONFIG_NO_ASM)
	DsaCheck(true, "the library is built from its C alone (MLD_CONFIG_NO_ASM)");
#else
	DsaCheck(false, "the library is built from its C alone (MLD_CONFIG_NO_ASM)");
#endif
}

int main()
{
	std::string const top = KatRepositoryTop(__FILE__);
	DsaCheck(!top.empty(), "the repository's top folder is found from this file's own path");

	TestTheSettings();
	TestTheLibrarysOwnAnswer();
	TestVerifyFile(top, "ml-dsa-87/nist-acvp-sigver.json", "pk", "message", "signature");
	TestVerifyFile(top, "ml-dsa-87/wycheproof-verify.json", "pk", "msg", "sig");
	CountNotForTheGame(top, "ml-dsa-87/nist-acvp-keygen.json", 25, "because they make keys, and the game's library makes none");
	CountNotForTheGame(top, "ml-dsa-87/nist-acvp-siggen.json", 30, "because they sign, and the game's library signs nothing");
	TestTheWrapper();

	printf("%s\n", dsaChecksFailed ? "SOME CHECKS FAILED" : "The ML-DSA-87 test passed");
	return dsaChecksFailed ? 1 : 0;
}
