// Tests of 3dc/net_pq_native.cpp, the real post-quantum half of protocol version 2: that it does what net_pq.h promises, through
// Secondary's wrapper (pq_key_exchange.cpp, pq_signature_check.cpp) and the libraries under it. Written 2026-11-09 by Melanikus for the
// AvP port.
//
// The wrapper and the libraries are held to NIST's and Wycheproof's published answers by pq_mlkem_test and pq_mldsa_test. This test
// holds the translation in between to its own promises:
// 1. it says it's there, and names itself;
// 2. ML-KEM-1024: a key pair, an encapsulation and a decapsulation agree on one secret; a changed ciphertext gives another secret
//    (FIPS 203's implicit rejection), never a refusal; a public key FIPS 203 section 7.2 refuses is refused, with both outputs wiped;
//    a missing key is refused the same way; two key pairs are never the same;
// 3. ML-DSA-87: it checks with the EMPTY context (CRYPTO.md section 5.1 point 16). Every Wycheproof case with no context is taken or
//    refused as marked, and a good signature made with a context is refused, which shows the context passed really is empty.

#include "net_pq.h"

#include "crypto_kat_reader.h"

#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <string>
#include <vector>

static int nativeChecksFailed = 0;

// Called NativeCheck, not Check: Snyk Code joins functions with the same name and signature across files, and many tests have a Check
static void NativeCheck(bool good, std::string const &what)
{
	printf("%s  %s\n", good ? "ok    " : "FAILED", what.c_str());
	fflush(stdout);
	nativeChecksFailed += good ? 0 : 1;
}

typedef std::vector<uint8_t> Bytes;

static bool AllZero(uint8_t const *bytes, size_t size)
{
	return std::all_of(bytes, bytes + size, [](uint8_t x) { return x == 0; });
}

static std::string Text(KatCase const &one, char const *name)
{
	auto const found = one.find(name);
	return found == one.end() ? std::string() : found->second;
}

// 1. It's there
static void TestItSaysSo()
{
	NativeCheck(NetPq_Available(), "NetPq_Available() is true: this game has its post-quantum half");
	NativeCheck(NetPq_Name() != NULL && strstr(NetPq_Name(), "mlkem-native") != NULL && strstr(NetPq_Name(), "mldsa-native") != NULL,
	            "and NetPq_Name() names both libraries");
}

// 2. ML-KEM-1024
static void TestKem()
{
	static uint8_t publicKey[kNetPqKemPublicBytes], privateKey[kNetPqKemPrivateBytes];
	static uint8_t otherPublic[kNetPqKemPublicBytes], otherPrivate[kNetPqKemPrivateBytes];
	uint8_t ciphertext[kNetPqKemCiphertextBytes], sent[kNetPqKemSecretBytes], got[kNetPqKemSecretBytes];

	NativeCheck(NetPq_KemKeypair(publicKey, privateKey), "a key pair is made");
	NativeCheck(NetPq_KemKeypair(otherPublic, otherPrivate) && memcmp(publicKey, otherPublic, sizeof(publicKey)) != 0,
	            "and a second one isn't the same: the randomness is fresh");

	bool const made = NetPq_KemEncapsulate(publicKey, ciphertext, sent);
	NativeCheck(made && !AllZero(sent, sizeof(sent)), "a secret is encapsulated to the public key");
	NativeCheck(NetPq_KemDecapsulate(privateKey, ciphertext, got) && memcmp(sent, got, sizeof(got)) == 0,
	            "and the private key gets exactly that secret back");

	NativeCheck(NetPq_KemDecapsulate(otherPrivate, ciphertext, got) && memcmp(sent, got, sizeof(got)) != 0,
	            "another private key gets another secret, never this one, and isn't refused for it");

	ciphertext[100] ^= 0x01;
	NativeCheck(NetPq_KemDecapsulate(privateKey, ciphertext, got) && memcmp(sent, got, sizeof(got)) != 0,
	            "a ciphertext with one bit changed gives another secret, never a refusal (FIPS 203's implicit rejection)");

	// Every 12-bit number in a key of all 0xFF bytes is 4,095, past ML-KEM's 3,329: FIPS 203 section 7.2's check refuses it
	uint8_t bad[kNetPqKemPublicBytes];
	memset(bad, 0xFF, sizeof(bad));
	memset(ciphertext, 0x5A, sizeof(ciphertext));
	memset(sent, 0x5A, sizeof(sent));
	NativeCheck(!NetPq_KemEncapsulate(bad, ciphertext, sent) && AllZero(ciphertext, sizeof(ciphertext)) && AllZero(sent, sizeof(sent)),
	            "a public key FIPS 203 section 7.2 refuses is refused, and both outputs are wiped");

	memset(ciphertext, 0x5A, sizeof(ciphertext));
	memset(sent, 0x5A, sizeof(sent));
	NativeCheck(!NetPq_KemEncapsulate(NULL, ciphertext, sent) && AllZero(ciphertext, sizeof(ciphertext)) && AllZero(sent, sizeof(sent)),
	            "a missing public key is refused the same way");

	memset(got, 0x5A, sizeof(got));
	NativeCheck(!NetPq_KemDecapsulate(NULL, ciphertext, got) && AllZero(got, sizeof(got)), "so is a missing private key");
}

// 3. ML-DSA-87, with the empty context, against Wycheproof's cases
static void TestDsa(std::string const &top)
{
	std::string const name = "ml-dsa-87/wycheproof-verify.json";
	KatFile file;
	std::string why;

	if (!KatRead(top, name, file, why))
	{
		NativeCheck(false, why);
		return;
	}

	int noContext = 0, wrong = 0, withContext = 0;
	bool contextShown = false;

	for (KatCase const &one : file.cases)
	{
		Bytes pk, message, context, signature;

		if (!KatBytes(Text(one, "pk"), pk) || !KatBytes(Text(one, "msg"), message) || !KatBytes(Text(one, "context"), context) ||
		    !KatBytes(Text(one, "sig"), signature))
		{
			wrong++;
			continue;
		}

		std::string const result = Text(one, "result");
		bool const sized = pk.size() == kNetPqDsaPublicBytes && signature.size() == kNetPqDsaSignatureBytes;

		if (!context.empty())
		{
			withContext++;

			// A good signature made with a context mustn't check with none: the proof that the context given is the empty one
			if (result == "valid" && sized && !contextShown)
			{
				contextShown = true;
				NativeCheck(!NetPq_DsaVerify(pk.data(), message.empty() ? NULL : message.data(), message.size(), signature.data()),
				            "a good signature made with a context (case " + Text(one, "id") + ") doesn't check here: the context is empty");
			}

			continue;
		}

		noContext++;

		// net_pq.h takes the key and the signature at their fixed sizes, so a case of another size is the caller's to refuse: here it
		// must be marked invalid, and is counted as refused
		bool const said = sized && NetPq_DsaVerify(pk.data(), message.empty() ? NULL : message.data(), message.size(), signature.data());

		if (said != (result == "valid"))
		{
			wrong++;

			if (wrong <= 10)
			{
				NativeCheck(false, name + " case " + Text(one, "id") + " (" + Text(one, "flags") + "): " +
				                       (said ? "a signature that must be refused was taken" : "a good signature was refused"));
			}
		}
	}

	NativeCheck(noContext > 0 && wrong == 0, name + ": " + std::to_string(noContext - wrong) + " of " + std::to_string(noContext) +
	                                             " cases with no context are taken or refused, as marked");
	NativeCheck(contextShown, std::to_string(withContext) + " cases have a context: one good one was tried, and refused");
}

int main()
{
	std::string const top = KatRepositoryTop(__FILE__);
	NativeCheck(!top.empty(), "the repository's top folder is found from this file's own path");

	TestItSaysSo();
	TestKem();
	TestDsa(top);

	printf("%s\n", nativeChecksFailed ? "net_pq_native_test: FAILED" : "net_pq_native_test: every check passed");
	return nativeChecksFailed ? 1 : 0;
}
