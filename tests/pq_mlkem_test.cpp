// Tests of the game's post-quantum key exchange, ML-KEM-1024: 3dc/pq_key_exchange.cpp, and the library under it, mlkem-native, as
// vcpkg builds it for the game (vcpkg-overlay-ports/mlkem-native). Written 2026-11-09 by Melanikus for the AvP port.
//
// The library's upstream checks its code with proofs, but those are for 64-bit builds by other compilers, not for this one: 32-bit,
// Microsoft's compiler, with our settings. So this runs the published answers on our build itself:
// 1. the library's own known answers (its test/test_vectors/expected_test_vectors.h, from the same release, installed with it);
// 2. every ML-KEM-1024 case NIST published for FIPS 203 (ACVP keyGen and encapDecap);
// 3. every one of Wycheproof's ML-KEM-1024 cases, made to catch the mistakes implementations are known to make;
// 4. what the wrapper adds: the sizes it refuses, the library's refusals it passes on, and the wiping on a refusal.
// The cases are in tests/crypto-kat (its README.md says where from). A case the game's code can't take is counted and named.
//
// The wrapper's own make-a-key-pair refusal can't be reached from here: it needs Windows' random generator to fail, or the library's
// check of a new key pair to. The code that turns either into a refusal is the same as for encapsulating, which is tested.

#include "pq_key_exchange.h"

#include <mlkem-native/avpx_mlkem.h>
#include <upstream-tests/expected_test_vectors.h>   // from vcpkg_installed's share/mlkem-native, which only this test's .args names

#include "crypto_kat_reader.h"

#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

static int kemChecksFailed = 0;

// Called KemCheck, not Check: Snyk Code joins functions with the same name and signature across files, and many tests have a Check
static void KemCheck(bool good, std::string const &what)
{
	printf("%s  %s\n", good ? "ok    " : "FAILED", what.c_str());
	fflush(stdout);
	kemChecksFailed += good ? 0 : 1;
}

typedef std::vector<uint8_t> Bytes;

static bool AllAre(uint8_t const *bytes, size_t length, uint8_t value)
{
	for (size_t i = 0; i < length; i++)
	{
		if (bytes[i] != value)
		{
			return false;
		}
	}

	return true;
}

// A refusal's wipe, in a buffer of room bytes set to 0xAA before (Fourth's cold read, finding 1): zeros for the length the call was given,
// but never past ML-KEM-1024's own size for that buffer (most), and everything after that still 0xAA
static bool WipedJustSo(uint8_t const *bytes, size_t room, size_t given, size_t most)
{
	size_t const wiped = given < most ? given : most;
	return wiped <= room && AllAre(bytes, wiped, 0) && AllAre(bytes + wiped, room - wiped, 0xAA);
}

static bool Same(Bytes const &a, uint8_t const *b, size_t length)
{
	return a.size() == length && memcmp(a.data(), b, length) == 0;
}

// One file's cases, tried one by one. Each failed case is named (the first ten of a file), and the file gets one line at the end.
struct KatRun
{
	std::string file;
	int tried = 0;
	int failed = 0;
	int notForTheGame = 0;

	void Fail(KatCase const &one, std::string const &why)
	{
		failed++;

		if (failed <= 10)
		{
			auto const id = one.find("id");
			KemCheck(false, file + " case " + (id == one.end() ? std::string("?") : id->second) + ": " + why);
		}
	}

	void End(std::string const &what)
	{
		std::string line = file + ": " + std::to_string(tried - failed) + " of " + std::to_string(tried) + " cases " + what;

		if (notForTheGame > 0)
		{
			line += ", and " + std::to_string(notForTheGame) + " not for the game (counted above)";
		}

		KemCheck(tried > 0 && failed == 0, line);
	}
};

// A case's field as bytes. False, and the case failed with why, if it's missing or isn't hex.
static bool Field(KatRun &run, KatCase const &one, char const *name, Bytes &bytes)
{
	auto const found = one.find(name);

	if (found == one.end() || !KatBytes(found->second, bytes))
	{
		run.Fail(one, std::string("its \"") + name + "\" is missing or isn't hex");
		return false;
	}

	return true;
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
	KemCheck(read, read ? "read tests/crypto-kat/" + name + ", " + std::to_string(file.cases.size()) + " cases" : why);
	return read;
}

// Decapsulating through the wrapper, with the library asked the same, so both are seen to give k
static void DecapsulateBoth(KatRun &run, KatCase const &one, Bytes const &dk, Bytes const &c, Bytes const &k)
{
	uint8_t secret[kPqSharedSecretBytes];

	if (!Pq_Decapsulate(c.data(), c.size(), dk.data(), dk.size(), secret, sizeof secret) || !Same(k, secret, sizeof secret))
	{
		run.Fail(one, "Pq_Decapsulate didn't give exactly k");
		return;
	}

	if (avpx_mlkem1024_dec(secret, c.data(), dk.data()) != 0 || !Same(k, secret, sizeof secret))
	{
		run.Fail(one, "the library's dec didn't give exactly k");
	}
}

// A public key that must be refused: by the library's own check, by encapsulating to it, and by the wrapper, which wipes its outputs
static void RefusedPublicKey(KatRun &run, KatCase const &one, Bytes const &ek)
{
	uint8_t ciphertext[kPqCiphertextBytes];
	uint8_t secret[kPqSharedSecretBytes];
	memset(ciphertext, 0xAA, sizeof ciphertext);
	memset(secret, 0xAA, sizeof secret);

	if (Pq_Encapsulate(ek.data(), ek.size(), ciphertext, sizeof ciphertext, secret, sizeof secret))
	{
		run.Fail(one, "Pq_Encapsulate took a public key it must refuse");
		return;
	}

	if (!AllAre(ciphertext, sizeof ciphertext, 0) || !AllAre(secret, sizeof secret, 0))
	{
		run.Fail(one, "Pq_Encapsulate refused, but didn't wipe the ciphertext and the secret");
		return;
	}

	if (ek.size() == kPqPublicKeyBytes)
	{
		uint8_t coins[MLKEM_SYMBYTES] = {0};

		if (avpx_mlkem1024_check_pk(ek.data()) == 0 || avpx_mlkem1024_enc_derand(ciphertext, secret, ek.data(), coins) == 0)
		{
			run.Fail(one, "the library took a public key it must refuse");
		}
	}
}

// 1. The library's own known answers
static void TestTheLibrarysOwnAnswers()
{
	uint8_t coins[2 * MLKEM_SYMBYTES];
	memcpy(coins, test_vector_d, MLKEM_SYMBYTES);
	memcpy(coins + MLKEM_SYMBYTES, test_vector_z, MLKEM_SYMBYTES);

	uint8_t publicKey[kPqPublicKeyBytes];
	uint8_t secretKey[kPqSecretKeyBytes];
	bool const made = avpx_mlkem1024_keypair_derand(publicKey, secretKey, coins) == 0;
	KemCheck(made && memcmp(publicKey, test_vector_pk, sizeof publicKey) == 0 && memcmp(secretKey, test_vector_sk, sizeof secretKey) == 0,
	         "the library's own answer: a key pair from its fixed seed is exactly its public and secret key");

	uint8_t ciphertext[kPqCiphertextBytes];
	uint8_t secret[kPqSharedSecretBytes];
	bool const sent = avpx_mlkem1024_enc_derand(ciphertext, secret, test_vector_pk, test_vector_m) == 0;
	KemCheck(sent && memcmp(ciphertext, test_vector_ct, sizeof ciphertext) == 0 && memcmp(secret, test_vector_ss, sizeof secret) == 0,
	         "the library's own answer: encapsulating to its key with its fixed seed is exactly its ciphertext and secret");

	uint8_t back[kPqSharedSecretBytes];
	KemCheck(Pq_Decapsulate(test_vector_ct, sizeof test_vector_ct, test_vector_sk, sizeof test_vector_sk, back, sizeof back) &&
	             memcmp(back, test_vector_ss, sizeof back) == 0,
	         "the library's own answer: Pq_Decapsulate gets its secret back from its ciphertext");
}

// 2. NIST's ACVP keyGen: KeyGen_internal(d, z) gives exactly ek and dk
static void TestNistKeyGen(std::string const &top)
{
	KatFile file;

	if (!ReadKat(top, "ml-kem-1024/nist-acvp-keygen.json", file))
	{
		return;
	}

	KatRun run;
	run.file = "nist-acvp-keygen";

	for (KatCase const &one : file.cases)
	{
		run.tried++;
		Bytes d, z, ek, dk;

		if (!Field(run, one, "d", d) || !Field(run, one, "z", z) || !Field(run, one, "ek", ek) || !Field(run, one, "dk", dk))
		{
			continue;
		}

		if (d.size() != MLKEM_SYMBYTES || z.size() != MLKEM_SYMBYTES || Text(one, "result") != "valid")
		{
			run.Fail(one, "isn't a 32-byte d and z, marked valid");
			continue;
		}

		uint8_t coins[2 * MLKEM_SYMBYTES];
		memcpy(coins, d.data(), MLKEM_SYMBYTES);
		memcpy(coins + MLKEM_SYMBYTES, z.data(), MLKEM_SYMBYTES);
		uint8_t publicKey[kPqPublicKeyBytes];
		uint8_t secretKey[kPqSecretKeyBytes];

		if (avpx_mlkem1024_keypair_derand(publicKey, secretKey, coins) != 0 || !Same(ek, publicKey, sizeof publicKey) ||
		    !Same(dk, secretKey, sizeof secretKey))
		{
			run.Fail(one, "the key pair from d and z isn't exactly ek and dk");
		}
	}

	run.End("make exactly ek and dk from d and z");
}

// 2. NIST's ACVP encapDecap: its four kinds of case
static void TestNistEncapDecap(std::string const &top)
{
	KatFile file;

	if (!ReadKat(top, "ml-kem-1024/nist-acvp-encapdecap.json", file))
	{
		return;
	}

	KatRun run;
	run.file = "nist-acvp-encapdecap";

	for (KatCase const &one : file.cases)
	{
		run.tried++;
		std::string const kind = Text(one, "function");
		bool const valid = Text(one, "result") == "valid";
		Bytes ek, dk, m, c, k;

		if (kind == "encapsulation")
		{
			if (!Field(run, one, "ek", ek) || !Field(run, one, "m", m) || !Field(run, one, "c", c) || !Field(run, one, "k", k))
			{
				continue;
			}

			uint8_t ciphertext[kPqCiphertextBytes];
			uint8_t secret[kPqSharedSecretBytes];

			if (!valid || ek.size() != kPqPublicKeyBytes || m.size() != MLKEM_SYMBYTES ||
			    avpx_mlkem1024_enc_derand(ciphertext, secret, ek.data(), m.data()) != 0 || !Same(c, ciphertext, sizeof ciphertext) ||
			    !Same(k, secret, sizeof secret))
			{
				run.Fail(one, "encapsulating to ek with m isn't exactly c and k");
			}
		}
		else if (kind == "decapsulation")
		{
			if (Field(run, one, "dk", dk) && Field(run, one, "c", c) && Field(run, one, "k", k))
			{
				if (!valid)
				{
					run.Fail(one, "a decapsulation case marked invalid, which NIST doesn't make");
					continue;
				}

				DecapsulateBoth(run, one, dk, c, k);
			}
		}
		else if (kind == "encapsulationKeyCheck")
		{
			if (!Field(run, one, "ek", ek))
			{
				continue;
			}

			if (valid)
			{
				uint8_t ciphertext[kPqCiphertextBytes];
				uint8_t secret[kPqSharedSecretBytes];

				if (avpx_mlkem1024_check_pk(ek.data()) != 0 ||
				    !Pq_Encapsulate(ek.data(), ek.size(), ciphertext, sizeof ciphertext, secret, sizeof secret))
				{
					run.Fail(one, "a good public key was refused");
				}
			}
			else
			{
				RefusedPublicKey(run, one, ek);
			}
		}
		else if (kind == "decapsulationKeyCheck")
		{
			if (!Field(run, one, "dk", dk))
			{
				continue;
			}

			// Any ciphertext does: what's asked is whether the secret key is taken
			uint8_t const ciphertext[kPqCiphertextBytes] = {0};
			uint8_t secret[kPqSharedSecretBytes];
			memset(secret, 0xAA, sizeof secret);
			bool const libraryTakes = dk.size() == kPqSecretKeyBytes && avpx_mlkem1024_check_sk(dk.data()) == 0;
			bool const wrapperTakes = Pq_Decapsulate(ciphertext, sizeof ciphertext, dk.data(), dk.size(), secret, sizeof secret);

			if (libraryTakes != valid || wrapperTakes != valid)
			{
				run.Fail(one, valid ? "a good secret key was refused" : "a secret key that must be refused was taken");
			}
			else if (!valid && !AllAre(secret, sizeof secret, 0))
			{
				run.Fail(one, "Pq_Decapsulate refused the secret key, but didn't wipe the secret");
			}
		}
		else
		{
			run.Fail(one, "a kind of case this test doesn't know: \"" + kind + "\"");
		}
	}

	run.End("give exactly their answer: encapsulation, decapsulation, and both key checks");
}

// 3. Wycheproof's key pairs from a seed (d then z)
static void TestWycheproofKeyGen(std::string const &top)
{
	KatFile file;

	if (!ReadKat(top, "ml-kem-1024/wycheproof-keygen-seed.json", file))
	{
		return;
	}

	KatRun run;
	run.file = "wycheproof-keygen-seed";

	for (KatCase const &one : file.cases)
	{
		run.tried++;
		Bytes seed, ek, dk;

		if (!Field(run, one, "seed", seed) || !Field(run, one, "ek", ek) || !Field(run, one, "dk", dk))
		{
			continue;
		}

		uint8_t publicKey[kPqPublicKeyBytes];
		uint8_t secretKey[kPqSecretKeyBytes];

		if (Text(one, "result") != "valid" || seed.size() != 2 * MLKEM_SYMBYTES ||
		    avpx_mlkem1024_keypair_derand(publicKey, secretKey, seed.data()) != 0 || !Same(ek, publicKey, sizeof publicKey) ||
		    !Same(dk, secretKey, sizeof secretKey))
		{
			run.Fail(one, "the key pair from the seed isn't exactly ek and dk");
		}
	}

	run.End("make exactly ek and dk from the seed");
}

// 3. Wycheproof's encapsulations: good keys give exactly c and K, and every bad key (a number in it past q, or the wrong length) is refused
static void TestWycheproofEncaps(std::string const &top)
{
	KatFile file;

	if (!ReadKat(top, "ml-kem-1024/wycheproof-encaps.json", file))
	{
		return;
	}

	KatRun run;
	run.file = "wycheproof-encaps";

	for (KatCase const &one : file.cases)
	{
		run.tried++;
		Bytes ek, m, c, k;

		if (!Field(run, one, "ek", ek) || !Field(run, one, "m", m) || !Field(run, one, "c", c) || !Field(run, one, "K", k))
		{
			continue;
		}

		std::string const result = Text(one, "result");

		if (result == "valid")
		{
			uint8_t ciphertext[kPqCiphertextBytes];
			uint8_t secret[kPqSharedSecretBytes];

			if (ek.size() != kPqPublicKeyBytes || m.size() != MLKEM_SYMBYTES ||
			    avpx_mlkem1024_enc_derand(ciphertext, secret, ek.data(), m.data()) != 0 || !Same(c, ciphertext, sizeof ciphertext) ||
			    !Same(k, secret, sizeof secret))
			{
				run.Fail(one, "encapsulating to ek with m isn't exactly c and K");
			}
		}
		else if (result == "invalid")
		{
			RefusedPublicKey(run, one, ek);
		}
		else
		{
			run.Fail(one, "a result that's neither valid nor invalid");
		}
	}

	run.End("give exactly c and K, or are refused, as marked");
}

// 3. Wycheproof's decapsulations, from a key pair made from a seed: changed ciphertexts give the implicit rejection's secret, exactly, and
// ciphertexts of the wrong length are refused
static void TestWycheproofDecaps(std::string const &top)
{
	KatFile file;

	if (!ReadKat(top, "ml-kem-1024/wycheproof-decaps.json", file))
	{
		return;
	}

	KatRun run;
	run.file = "wycheproof-decaps";
	int wrongSeeds = 0;

	for (KatCase const &one : file.cases)
	{
		run.tried++;
		Bytes seed, ek, c, k;

		if (!Field(run, one, "seed", seed) || !Field(run, one, "ek", ek) || !Field(run, one, "c", c) || !Field(run, one, "K", k))
		{
			continue;
		}

		bool const valid = Text(one, "result") == "valid";

		// The game never makes a key from a seed it's given, so a seed of the wrong length is nothing the game could be handed
		if (seed.size() != 2 * MLKEM_SYMBYTES)
		{
			if (valid)
			{
				run.Fail(one, "a seed of the wrong length, marked valid");
			}
			else
			{
				run.tried--;
				run.notForTheGame++;
				wrongSeeds++;
			}

			continue;
		}

		uint8_t publicKey[kPqPublicKeyBytes];
		uint8_t secretKey[kPqSecretKeyBytes];

		if (avpx_mlkem1024_keypair_derand(publicKey, secretKey, seed.data()) != 0 || (!ek.empty() && !Same(ek, publicKey, sizeof publicKey)))
		{
			run.Fail(one, "the key pair from the seed isn't the case's ek");
			continue;
		}

		Bytes const dk(secretKey, secretKey + sizeof secretKey);

		if (valid)
		{
			DecapsulateBoth(run, one, dk, c, k);
		}
		else
		{
			uint8_t secret[kPqSharedSecretBytes];
			memset(secret, 0xAA, sizeof secret);

			if (Pq_Decapsulate(c.data(), c.size(), dk.data(), dk.size(), secret, sizeof secret) || !AllAre(secret, sizeof secret, 0))
			{
				run.Fail(one, "a ciphertext that must be refused was taken, or the secret wasn't wiped");
			}
		}
	}

	// Counted against the file's own number of them, so a case can't slip in here by mistake
	KemCheck(wrongSeeds == 20, "wycheproof-decaps: " + std::to_string(wrongSeeds) +
	                               " cases give a seed of the wrong length, which the game never takes (it never makes a key from a given seed); "
	                               "the file has 20");
	run.End("give exactly K, or are refused, as marked");
}

// 3. Wycheproof's decapsulations with a secret key given whole: the wrong lengths, and secret keys that fail FIPS 203's own check
static void TestWycheproofDecapsChecked(std::string const &top)
{
	KatFile file;

	if (!ReadKat(top, "ml-kem-1024/wycheproof-decaps-checked.json", file))
	{
		return;
	}

	KatRun run;
	run.file = "wycheproof-decaps-checked";

	for (KatCase const &one : file.cases)
	{
		run.tried++;
		Bytes dk, c, k;

		if (!Field(run, one, "dk", dk) || !Field(run, one, "c", c) || !Field(run, one, "K", k))
		{
			continue;
		}

		if (Text(one, "result") == "valid")
		{
			DecapsulateBoth(run, one, dk, c, k);
		}
		else
		{
			uint8_t secret[kPqSharedSecretBytes];
			memset(secret, 0xAA, sizeof secret);

			if (Pq_Decapsulate(c.data(), c.size(), dk.data(), dk.size(), secret, sizeof secret) || !AllAre(secret, sizeof secret, 0))
			{
				run.Fail(one, "a case that must be refused was taken, or the secret wasn't wiped");
			}
		}
	}

	run.End("give exactly K, or are refused, as marked");
}

// 4. The wrapper on its own: a whole exchange, fresh randomness every time, and each size refused with every output wiped
static void TestTheWrapper()
{
	uint8_t publicKey[kPqPublicKeyBytes + 1];
	uint8_t secretKey[kPqSecretKeyBytes + 1];
	uint8_t ciphertext[kPqCiphertextBytes + 1];
	uint8_t sent[kPqSharedSecretBytes + 1];
	uint8_t back[kPqSharedSecretBytes + 1];

	bool const made = Pq_NewKeyPair(publicKey, kPqPublicKeyBytes, secretKey, kPqSecretKeyBytes);
	KemCheck(made && !AllAre(publicKey, kPqPublicKeyBytes, 0) && !AllAre(secretKey, kPqSecretKeyBytes, 0), "Pq_NewKeyPair makes a key pair");

	bool const encapsulated = Pq_Encapsulate(publicKey, kPqPublicKeyBytes, ciphertext, kPqCiphertextBytes, sent, kPqSharedSecretBytes);
	bool const decapsulated = Pq_Decapsulate(ciphertext, kPqCiphertextBytes, secretKey, kPqSecretKeyBytes, back, kPqSharedSecretBytes);
	KemCheck(encapsulated && decapsulated && memcmp(sent, back, kPqSharedSecretBytes) == 0 && !AllAre(sent, kPqSharedSecretBytes, 0),
	         "and a secret encapsulated to it comes back the same from its ciphertext");

	// Fresh randomness: the second of each is different
	uint8_t publicKey2[kPqPublicKeyBytes];
	uint8_t secretKey2[kPqSecretKeyBytes];
	uint8_t ciphertext2[kPqCiphertextBytes];
	uint8_t sent2[kPqSharedSecretBytes];
	KemCheck(Pq_NewKeyPair(publicKey2, sizeof publicKey2, secretKey2, sizeof secretKey2) &&
	             memcmp(publicKey, publicKey2, sizeof publicKey2) != 0 && memcmp(secretKey, secretKey2, sizeof secretKey2) != 0,
	         "a second key pair is a different one (Windows' random generator is used)");
	KemCheck(Pq_Encapsulate(publicKey, kPqPublicKeyBytes, ciphertext2, sizeof ciphertext2, sent2, sizeof sent2) &&
	             memcmp(ciphertext, ciphertext2, sizeof ciphertext2) != 0 && memcmp(sent, sent2, sizeof sent2) != 0,
	         "a second secret encapsulated to the same key is a different one");

	// A changed ciphertext isn't refused (ML-KEM's implicit rejection), and gives a different secret
	ciphertext[100] ^= 0x01;
	KemCheck(Pq_Decapsulate(ciphertext, kPqCiphertextBytes, secretKey, kPqSecretKeyBytes, back, kPqSharedSecretBytes) &&
	             memcmp(sent, back, kPqSharedSecretBytes) != 0,
	         "a changed ciphertext gives a different secret, as ML-KEM's design has it");
	ciphertext[100] ^= 0x01;

	// A secret key whose own record of its public key (bytes 3,104 to 3,135, FIPS 203 section 7.3) is wrong is refused
	secretKey[3110] ^= 0x01;
	memset(back, 0xAA, sizeof back);
	KemCheck(!Pq_Decapsulate(ciphertext, kPqCiphertextBytes, secretKey, kPqSecretKeyBytes, back, kPqSharedSecretBytes) &&
	             AllAre(back, kPqSharedSecretBytes, 0),
	         "a damaged secret key is refused, and the secret wiped");
	secretKey[3110] ^= 0x01;

	// Every size one short and one long, and every buffer missing: refused, and every output wiped for the length it was given
	struct Size
	{
		size_t publicKey, secretKey, ciphertext, secret;
		bool missing;
		char const *what;
	};
	Size const sizes[] = {
		{kPqPublicKeyBytes - 1, kPqSecretKeyBytes, kPqCiphertextBytes, kPqSharedSecretBytes, false, "a public key one byte short"},
		{kPqPublicKeyBytes + 1, kPqSecretKeyBytes, kPqCiphertextBytes, kPqSharedSecretBytes, false, "a public key one byte long"},
		{kPqPublicKeyBytes, kPqSecretKeyBytes - 1, kPqCiphertextBytes, kPqSharedSecretBytes, false, "a secret key one byte short"},
		{kPqPublicKeyBytes, kPqSecretKeyBytes + 1, kPqCiphertextBytes, kPqSharedSecretBytes, false, "a secret key one byte long"},
		{kPqPublicKeyBytes, kPqSecretKeyBytes, kPqCiphertextBytes - 1, kPqSharedSecretBytes, false, "a ciphertext one byte short"},
		{kPqPublicKeyBytes, kPqSecretKeyBytes, kPqCiphertextBytes + 1, kPqSharedSecretBytes, false, "a ciphertext one byte long"},
		{kPqPublicKeyBytes, kPqSecretKeyBytes, kPqCiphertextBytes, kPqSharedSecretBytes - 1, false, "room for a secret one byte short"},
		{kPqPublicKeyBytes, kPqSecretKeyBytes, kPqCiphertextBytes, kPqSharedSecretBytes + 1, false, "room for a secret one byte long"},
	};

	for (Size const &size : sizes)
	{
		bool const keyPairSizes = size.publicKey != kPqPublicKeyBytes || size.secretKey != kPqSecretKeyBytes;
		bool const encapsulateSizes = size.publicKey != kPqPublicKeyBytes || size.ciphertext != kPqCiphertextBytes || size.secret != kPqSharedSecretBytes;
		bool const decapsulateSizes = size.ciphertext != kPqCiphertextBytes || size.secretKey != kPqSecretKeyBytes || size.secret != kPqSharedSecretBytes;

		if (keyPairSizes)
		{
			uint8_t newPublic[kPqPublicKeyBytes + 1];
			uint8_t newSecret[kPqSecretKeyBytes + 1];
			memset(newPublic, 0xAA, sizeof newPublic);
			memset(newSecret, 0xAA, sizeof newSecret);
			KemCheck(!Pq_NewKeyPair(newPublic, size.publicKey, newSecret, size.secretKey) &&
			             WipedJustSo(newPublic, sizeof newPublic, size.publicKey, kPqPublicKeyBytes) &&
			             WipedJustSo(newSecret, sizeof newSecret, size.secretKey, kPqSecretKeyBytes),
			         std::string("Pq_NewKeyPair refuses ") + size.what + ", and wipes both, never past ML-KEM's own sizes");
		}

		if (encapsulateSizes)
		{
			memset(ciphertext, 0xAA, sizeof ciphertext);
			memset(sent, 0xAA, sizeof sent);
			KemCheck(!Pq_Encapsulate(publicKey, size.publicKey, ciphertext, size.ciphertext, sent, size.secret) &&
			             WipedJustSo(ciphertext, sizeof ciphertext, size.ciphertext, kPqCiphertextBytes) &&
			             WipedJustSo(sent, sizeof sent, size.secret, kPqSharedSecretBytes),
			         std::string("Pq_Encapsulate refuses ") + size.what + ", and wipes the ciphertext and the secret, never past ML-KEM's own sizes");
		}

		if (decapsulateSizes)
		{
			uint8_t const anyCiphertext[kPqCiphertextBytes + 1] = {0};
			memset(back, 0xAA, sizeof back);
			KemCheck(!Pq_Decapsulate(anyCiphertext, size.ciphertext, secretKey, size.secretKey, back, size.secret) &&
			             WipedJustSo(back, sizeof back, size.secret, kPqSharedSecretBytes),
			         std::string("Pq_Decapsulate refuses ") + size.what + ", and wipes the secret, never past ML-KEM's own size");
		}
	}

	uint8_t const anyCiphertext[kPqCiphertextBytes] = {0};
	KemCheck(!Pq_NewKeyPair(NULL, kPqPublicKeyBytes, secretKey2, sizeof secretKey2) &&
	             !Pq_NewKeyPair(publicKey2, sizeof publicKey2, NULL, kPqSecretKeyBytes),
	         "Pq_NewKeyPair refuses a missing buffer");
	KemCheck(!Pq_Encapsulate(NULL, kPqPublicKeyBytes, ciphertext2, sizeof ciphertext2, sent2, sizeof sent2) &&
	             !Pq_Encapsulate(publicKey, kPqPublicKeyBytes, NULL, kPqCiphertextBytes, sent2, sizeof sent2) &&
	             !Pq_Encapsulate(publicKey, kPqPublicKeyBytes, ciphertext2, sizeof ciphertext2, NULL, kPqSharedSecretBytes),
	         "Pq_Encapsulate refuses a missing buffer");
	KemCheck(!Pq_Decapsulate(NULL, kPqCiphertextBytes, secretKey, kPqSecretKeyBytes, back, kPqSharedSecretBytes) &&
	             !Pq_Decapsulate(anyCiphertext, sizeof anyCiphertext, NULL, kPqSecretKeyBytes, back, kPqSharedSecretBytes) &&
	             !Pq_Decapsulate(anyCiphertext, sizeof anyCiphertext, secretKey, kPqSecretKeyBytes, NULL, kPqSharedSecretBytes),
	         "Pq_Decapsulate refuses a missing buffer");

	// Buffers that share a byte are refused before the library sees them; buffers that only touch are fine (Fourth's cold read, finding
	// 3). Each case is set up in one buffer of its own, big enough that every wipe stays inside it.
	static uint8_t pair[kPqPublicKeyBytes + kPqSecretKeyBytes];
	KemCheck(!Pq_NewKeyPair(pair, kPqPublicKeyBytes, pair + kPqPublicKeyBytes - 1, kPqSecretKeyBytes),
	         "Pq_NewKeyPair refuses a public key and a secret key that share a byte");
	KemCheck(Pq_NewKeyPair(pair, kPqPublicKeyBytes, pair + kPqPublicKeyBytes, kPqSecretKeyBytes),
	         "and makes a key pair into two that only touch");

	static uint8_t sending[kPqPublicKeyBytes + kPqCiphertextBytes + kPqSharedSecretBytes];
	uint8_t *const sendingCiphertext = sending + kPqPublicKeyBytes;
	uint8_t *const sendingSecret = sendingCiphertext + kPqCiphertextBytes;
	memcpy(sending, publicKey, kPqPublicKeyBytes);
	KemCheck(!Pq_Encapsulate(sending, kPqPublicKeyBytes, sendingCiphertext - 1, kPqCiphertextBytes, sendingSecret, kPqSharedSecretBytes),
	         "Pq_Encapsulate refuses a ciphertext that shares a byte with the public key");
	memcpy(sending, publicKey, kPqPublicKeyBytes);
	KemCheck(!Pq_Encapsulate(sending, kPqPublicKeyBytes, sendingCiphertext, kPqCiphertextBytes, sendingSecret - 1, kPqSharedSecretBytes),
	         "Pq_Encapsulate refuses a secret that shares a byte with the ciphertext");
	memcpy(sending, publicKey, kPqPublicKeyBytes);
	KemCheck(!Pq_Encapsulate(sending, kPqPublicKeyBytes, sendingCiphertext, kPqCiphertextBytes, sending + 8, kPqSharedSecretBytes),
	         "Pq_Encapsulate refuses a secret that shares bytes with the public key");
	memcpy(sending, publicKey, kPqPublicKeyBytes);
	KemCheck(Pq_Encapsulate(sending, kPqPublicKeyBytes, sendingCiphertext, kPqCiphertextBytes, sendingSecret, kPqSharedSecretBytes),
	         "and encapsulates into buffers that only touch");

	// That ciphertext, and the secret it carries, for the decapsulations below (the checks above it wrote over the earlier ones)
	static uint8_t opening[kPqCiphertextBytes + kPqSecretKeyBytes + kPqSharedSecretBytes];
	uint8_t *const openingKey = opening + kPqCiphertextBytes;
	uint8_t *const openingSecret = openingKey + kPqSecretKeyBytes;
	uint8_t expected[kPqSharedSecretBytes];
	memcpy(expected, sendingSecret, kPqSharedSecretBytes);
	memcpy(opening, sendingCiphertext, kPqCiphertextBytes);
	memcpy(openingKey, secretKey, kPqSecretKeyBytes);
	KemCheck(!Pq_Decapsulate(opening, kPqCiphertextBytes, openingKey, kPqSecretKeyBytes, openingSecret - 1, kPqSharedSecretBytes),
	         "Pq_Decapsulate refuses a secret that shares a byte with the secret key");
	memcpy(openingKey, secretKey, kPqSecretKeyBytes);
	KemCheck(!Pq_Decapsulate(opening, kPqCiphertextBytes, openingKey, kPqSecretKeyBytes, opening + 8, kPqSharedSecretBytes),
	         "Pq_Decapsulate refuses a secret that shares bytes with the ciphertext");
	memcpy(opening, sendingCiphertext, kPqCiphertextBytes);
	KemCheck(Pq_Decapsulate(opening, kPqCiphertextBytes, openingKey, kPqSecretKeyBytes, openingSecret, kPqSharedSecretBytes) &&
	             memcmp(openingSecret, expected, kPqSharedSecretBytes) == 0,
	         "and decapsulates into a secret that only touches the secret key, the same secret that was sent");
}

// The reader of tests/crypto-kat refuses a file that isn't the shape (tests/crypto-kat/README.md promises it), so a damaged file
// fails this test instead of losing cases. pq_mldsa_test reads with the same tests/crypto_kat_reader.h, so this is for both.
static void TestTheReader()
{
	struct Sample
	{
		char const *text;
		bool good;
		char const *what;
	};
	Sample const samples[] = {
		{"{\"primitive\":\"p\",\"source\":\"s\",\"about\":\"a\",\"count\":1,\"cases\":[{\"id\":\"1\",\"result\":\"valid\"}]}\r\n", true, "a file of the shape, with a Windows line end"},
		{"{\"primitive\":\"p\",\"source\":\"s\",\"about\":\"a\",\"count\":2,\"cases\":[{\"id\":\"1\"}]}", false, "a count that isn't the number of cases"},
		{"{\"primitive\":\"p\",\"source\":\"s\",\"about\":\"a\",\"count\":1,\"cases\":[{\"id\":\"1\"}]} x", false, "more after the end"},
		{"{\"primitive\":\"p\",\"source\":\"s\",\"about\":\"a\",\"count\":1,\"cases\":[{\"id\":\"1\",\"id\":\"2\"}]}", false, "a name twice in a case"},
		{"{\"primitive\":\"p\",\"primitive\":\"q\",\"source\":\"s\",\"about\":\"a\",\"count\":0,\"cases\":[]}", false, "a name twice in the file"},
		{"{\"primitive\":\"p\",\"source\":\"s\",\"about\":\"a\",\"count\":0,\"cases\":[],\"extra\":\"x\"}", false, "a sixth name"},
		{"{\"primitive\":\"p\",\"source\":\"s\",\"count\":0,\"cases\":[]}", false, "a name missing"},
		{"{\"primitive\":\"p\\n\",\"source\":\"s\",\"about\":\"a\",\"count\":0,\"cases\":[]}", false, "an escape other than \\\" and \\\\"},
		{"{\"primitive\":\"p\xC3\xA9\",\"source\":\"s\",\"about\":\"a\",\"count\":0,\"cases\":[]}", false, "a character that isn't printable ASCII"},
		{"{\"primitive\":\"p\",\"source\":\"s\",\"about\":\"a\",\"count\":01,\"cases\":[{}]}", false, "a number that starts with 0"},
		{"{\"primitive\":\"p\",\"source\":\"s\",\"about\":\"a\",\"count\":1,\"cases\":[{\"id\":1}]}", false, "a case value that isn't a text"},
		{"{\"primitive\":\"p\",\"source\":\"s\",\"about\":\"a\",\"count\":0,\"cases\":[]", false, "a file cut short"},
	};

	for (Sample const &sample : samples)
	{
		std::string const text = sample.text;
		KatParser parser(text);
		KatFile file;
		bool const read = parser.ReadFile(file);
		KemCheck(read == sample.good, std::string("the known-answer reader ") + (sample.good ? "reads " : "refuses ") + sample.what +
		                                  (read ? "" : " (" + parser.Why() + ")"));
	}

	Bytes bytes;
	KemCheck(KatBytes("00ff7a", bytes) && bytes.size() == 3 && bytes[1] == 0xFF && !KatBytes("0", bytes) && !KatBytes("0A", bytes) &&
	             !KatBytes("zz", bytes),
	         "and its hex: small letters, two to a byte, nothing else");
}

// The settings the library was built with, as its installed header says (the same file the build read)
static void TestTheSettings()
{
	KemCheck(MLK_CONFIG_PARAMETER_SET == 1024, "the library is ML-KEM-1024");
#if defined(MLK_CONFIG_KEYGEN_PCT)
	KemCheck(true, "the library checks every new key pair before giving it back (MLK_CONFIG_KEYGEN_PCT)");
#else
	KemCheck(false, "the library checks every new key pair before giving it back (MLK_CONFIG_KEYGEN_PCT)");
#endif
#if defined(MLK_CONFIG_NO_ASM)
	KemCheck(true, "the library is built from its C alone (MLK_CONFIG_NO_ASM)");
#else
	KemCheck(false, "the library is built from its C alone (MLK_CONFIG_NO_ASM)");
#endif
}

int main()
{
	std::string const top = KatRepositoryTop(__FILE__);
	KemCheck(!top.empty(), "the repository's top folder is found from this file's own path");

	TestTheReader();
	TestTheSettings();
	TestTheLibrarysOwnAnswers();
	TestNistKeyGen(top);
	TestNistEncapDecap(top);
	TestWycheproofKeyGen(top);
	TestWycheproofEncaps(top);
	TestWycheproofDecaps(top);
	TestWycheproofDecapsChecked(top);
	TestTheWrapper();

	printf("%s\n", kemChecksFailed ? "SOME CHECKS FAILED" : "The ML-KEM-1024 test passed");
	return kemChecksFailed ? 1 : 0;
}
