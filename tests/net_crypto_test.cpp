// The test of protocol version 2's cryptography on the game's side: the classical half on Windows' CNG (3dc/net_crypto_cng.cpp), the
// post-quantum stand-in (3dc/net_pq_none.cpp), and the Noise state built on them (3dc/net_noise.h). Written 2026-11-09 by Melanikus for
// the AvP port.
//
// WHAT IT TESTS
//
// Every primitive against the published known answers in tests/vectors/crypto-kat/ (copied byte for byte from the relay's repository;
// COPIED.md says what and why): NIST's CAVP and ACVP cases and Wycheproof's edge cases, for SHA-384 (two FIPS 180-4 examples), HMAC-SHA-384,
// HKDF-SHA-384, AES-256-GCM, ECDH on P-384 and ECDSA P-384 with SHA-384. Each file's "valid" cases have to give exactly its answer; its
// "invalid" ones have to be refused (a tag, a point or a signature that's wrong) or give something else (a NIST answer that's wrong on
// purpose). Then the post-quantum stand-in refuses every call, and the Noise state does what CRYPTO.md section 5 says on both sides.
//
// WHAT IT DOESN'T TEST
//
// The handshakes themselves, which come next and will be checked against Primary's crypto-v2 vectors; and ML-KEM and ML-DSA, whose known
// answers come with their library.

#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

#include "net_crypto.h"
#include "net_pq.h"
#include "net_noise.h"
#include "json_reader.h"

static int checksFailed = 0;

static void Check(bool passed, const std::string &what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what.c_str());

	if (!passed)
	{
		checksFailed++;
	}
}

// ---- Reading a file of known answers

// One file's cases, read and checked for its shape: an object with its primitive, its count, and that many cases. False if it can't be
// read; then the test says which file.
static bool LoadCases(const char *file, JsonValue *all, const JsonValue **cases)
{
	std::string const text = ReadRepositoryFile(file);
	JsonReader reader(text);
	const JsonValue *count = NULL;
	bool const read = !text.empty() && reader.ReadAll(all) && all->kind == kJsonObject && (*cases = all->Member("cases")) != NULL
		&& (*cases)->kind == kJsonList && (count = all->Member("count")) != NULL && count->kind == kJsonNumber && count->number == (*cases)->items.size()
		&& !(*cases)->items.empty();
	Check(read, std::string(file) + ": read, with as many cases as it says");
	return read;
}

// A case's bytes, from its hex ("" is no bytes). False, with the case counted as wrong, if the field isn't there or isn't hex.
static bool Bytes(const JsonValue &c, const char *name, std::vector<uint8_t> *out)
{
	const JsonValue *const field = TextMember(c, name);
	return field && FromHex(field->text, out);
}

static bool Number(const JsonValue &c, const char *name, uint64_t *out)
{
	const JsonValue *const field = c.Member(name);

	if (!field || field->kind != kJsonNumber)
	{
		return false;
	}

	*out = field->number;
	return true;
}

static std::string Result(const JsonValue &c)
{
	const JsonValue *const field = TextMember(c, "result");
	return field ? field->text : std::string();
}

// How a file's cases went: how many of them did as the file says, and the first few that didn't, by their id
struct Tally
{
	size_t right = 0;
	size_t all = 0;
	int shown = 0;

	void Case(bool asTheFileSays, const JsonValue &c)
	{
		all++;
		right += asTheFileSays ? 1 : 0;

		if (!asTheFileSays && shown < 5)
		{
			const JsonValue *const id = TextMember(c, "id");
			printf("    wrong here: %s\n", id ? id->text.c_str() : "(a case with no id)");
			shown++;
		}
	}

	void Say(const char *file)
	{
		Check(right == all, std::string(file) + ": " + std::to_string(right) + " of " + std::to_string(all) + " cases as the file says");
	}
};

static const uint8_t *Data(const std::vector<uint8_t> &bytes)
{
	return bytes.empty() ? NULL : bytes.data();
}

// ---- SHA-384 and HMAC-SHA-384

static void TheHashes()
{
	printf("SHA-384 and HMAC-SHA-384\n");

	// FIPS 180-4's own examples (NIST, "Examples with Intermediate Values": SHA-384 of "abc", and of nothing)
	std::vector<uint8_t> abc, empty;
	FromHex("cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7", &abc);
	FromHex("38b060a751ac96384cd9327eb1b1e36a21fdb71114be07434c0cc7bf63f6e1da274edebfe76f65fbd51ad2f14898b95b", &empty);
	uint8_t out[kNetCryptoHashBytes];
	NetCryptoPiece const abcPiece = { reinterpret_cast<const uint8_t *>("abc"), 3 };
	Check(NetCrypto_Sha384(&abcPiece, 1, out) && memcmp(out, abc.data(), sizeof(out)) == 0, "SHA-384 of \"abc\" is FIPS 180-4's");
	Check(NetCrypto_Sha384(NULL, 0, out) && memcmp(out, empty.data(), sizeof(out)) == 0, "and of nothing");

	// The same bytes in pieces give the same hash as in one
	NetCryptoPiece const pieces[3] = { { reinterpret_cast<const uint8_t *>("a"), 1 }, { NULL, 0 }, { reinterpret_cast<const uint8_t *>("bc"), 2 } };
	Check(NetCrypto_Sha384(pieces, 3, out) && memcmp(out, abc.data(), sizeof(out)) == 0, "and of \"a\", nothing, then \"bc\", one after another");

	JsonValue file;
	const JsonValue *cases = NULL;
	const char *const name = "tests/vectors/crypto-kat/hmac-sha384/wycheproof.json";

	if (LoadCases(name, &file, &cases))
	{
		Tally tally;

		for (const JsonValue &c : cases->items)
		{
			std::vector<uint8_t> key, msg, tag;
			uint64_t tagBits = 0;
			bool const usable = Bytes(c, "key", &key) && Bytes(c, "msg", &msg) && Bytes(c, "tag", &tag) && Number(c, "tagSize", &tagBits)
				&& tagBits % 8 == 0 && tagBits / 8 == tag.size() && tag.size() <= kNetCryptoHashBytes;
			NetCryptoPiece const piece = { Data(msg), msg.size() };
			bool const same = usable && NetCrypto_HmacSha384(Data(key), key.size(), &piece, 1, out) && memcmp(out, tag.data(), tag.size()) == 0;
			tally.Case(usable && same == (Result(c) == "valid"), c);
		}

		tally.Say(name);
	}
}

// ---- HKDF-SHA-384

static void TheKeyDerivation()
{
	printf("HKDF-SHA-384\n");
	const char *const files[2] = { "tests/vectors/crypto-kat/hkdf-sha384/wycheproof.json", "tests/vectors/crypto-kat/hkdf-sha384/nist-acvp-kda.json" };

	for (const char *const name : files)
	{
		JsonValue file;
		const JsonValue *cases = NULL;

		if (!LoadCases(name, &file, &cases))
		{
			continue;
		}

		Tally tally;

		for (const JsonValue &c : cases->items)
		{
			std::vector<uint8_t> ikm, salt, info, okm;
			uint64_t size = 0;
			bool const usable = Bytes(c, "ikm", &ikm) && Bytes(c, "salt", &salt) && Bytes(c, "info", &info) && Bytes(c, "okm", &okm) && Number(c, "size", &size);
			bool const valid = Result(c) == "valid";

			// Wycheproof's invalid ones ask for more than HKDF can give: HKDF must refuse. NIST's say an answer is wrong: ours must differ.
			std::vector<uint8_t> out(static_cast<size_t>(size < 1000000 ? size : 1));
			bool const gave = usable && size < 1000000 && NetCrypto_HkdfSha384(Data(salt), salt.size(), Data(ikm), ikm.size(), Data(info), info.size(),
				out.empty() ? NULL : out.data(), out.size());
			bool const same = gave && out == okm;
			bool const asSaid = valid ? same : (okm.empty() ? !gave : !same);
			tally.Case(usable && asSaid, c);
		}

		tally.Say(name);
	}

	uint8_t out[16];
	Check(!NetCrypto_HkdfSha384(NULL, 0, NULL, 0, NULL, 0, NULL, 16) && !NetCrypto_HkdfExpandSha384(NULL, 0, NULL, 0, out, sizeof(out)),
		"nowhere to put the key, or no key to expand from, is refused");
}

// ---- AES-256-GCM

static void TheSeal()
{
	printf("AES-256-GCM\n");

	struct GcmFile
	{
		const char *name;
		const char *text;   // the plaintext's field: NIST's "pt", Wycheproof's "msg"
		bool sealToo;       // whether a valid case is also sealed again, to its exact bytes and tag
	};

	const GcmFile files[3] = {
		{ "tests/vectors/crypto-kat/aes-256-gcm/nist-cavp-encrypt.json", "pt", true },
		{ "tests/vectors/crypto-kat/aes-256-gcm/nist-cavp-decrypt.json", "pt", false },
		{ "tests/vectors/crypto-kat/aes-256-gcm/wycheproof.json", "msg", true },
	};

	for (const GcmFile &f : files)
	{
		JsonValue file;
		const JsonValue *cases = NULL;

		if (!LoadCases(f.name, &file, &cases))
		{
			continue;
		}

		Tally tally;

		for (const JsonValue &c : cases->items)
		{
			std::vector<uint8_t> key, iv, aad, text, ct, tag;
			bool const usable = Bytes(c, "key", &key) && Bytes(c, "iv", &iv) && Bytes(c, "aad", &aad) && Bytes(c, f.text, &text) && Bytes(c, "ct", &ct)
				&& Bytes(c, "tag", &tag) && key.size() == kNetCryptoAesKeyBytes && iv.size() == kNetCryptoNonceBytes && tag.size() == kNetCryptoTagBytes;
			bool const valid = Result(c) == "valid";

			// Opening: a valid case gives its plaintext; an invalid one is refused, with nothing left in its place
			std::vector<uint8_t> opened(ct.size(), 0xAA);
			bool const opens = usable && NetCrypto_AesGcmOpen(key.data(), iv.data(), Data(aad), aad.size(), Data(ct), ct.size(), tag.data(),
				opened.empty() ? NULL : opened.data());
			bool asSaid = valid ? (opens && opened == text) : (!opens && std::all_of(opened.begin(), opened.end(), [](uint8_t b) { return b == 0; }));

			// Sealing a valid case's plaintext again gives its exact bytes and tag
			if (asSaid && valid && f.sealToo)
			{
				std::vector<uint8_t> sealed(text.size());
				uint8_t madeTag[kNetCryptoTagBytes];
				asSaid = NetCrypto_AesGcmSeal(key.data(), iv.data(), Data(aad), aad.size(), Data(text), text.size(), sealed.empty() ? NULL : sealed.data(), madeTag)
					&& sealed == ct && memcmp(madeTag, tag.data(), sizeof(madeTag)) == 0;
			}

			tally.Case(usable && asSaid, c);
		}

		tally.Say(f.name);
	}
}

// ---- ECDH on P-384

static void TheKeyAgreement()
{
	printf("ECDH on P-384\n");

	// NIST's: our private key and its public half, the other side's public key, and the secret they agree on
	{
		JsonValue file;
		const JsonValue *cases = NULL;
		const char *const name = "tests/vectors/crypto-kat/ecdh-p384/nist-cavp.json";

		if (LoadCases(name, &file, &cases))
		{
			Tally tally;

			for (const JsonValue &c : cases->items)
			{
				std::vector<uint8_t> ours, ourPublic, theirs, shared;
				bool const usable = Bytes(c, "private", &ours) && Bytes(c, "ourPublic", &ourPublic) && Bytes(c, "public", &theirs) && Bytes(c, "shared", &shared)
					&& ours.size() == kNetCryptoP384ScalarBytes && ourPublic.size() == kNetCryptoP384PublicBytes && theirs.size() == kNetCryptoP384PublicBytes
					&& shared.size() == kNetCryptoP384ScalarBytes;
				NetEcdhKey *const key = usable ? NetCrypto_EcdhFromPrivate(ours.data(), ourPublic.data()) : NULL;
				uint8_t secret[kNetCryptoP384ScalarBytes];
				bool const agreed = key && NetCrypto_EcdhAgree(key, theirs.data(), secret) && memcmp(secret, shared.data(), sizeof(secret)) == 0;
				NetCrypto_EcdhFree(key);
				tally.Case(agreed, c);
			}

			tally.Say(name);
		}
	}

	// The other side's point, checked: Wycheproof's points and NIST's public-key checks, each against a fresh key of ours. A point the file
	// calls valid is agreed with; one it calls invalid, or that isn't 97 bytes (compressed, or too long), is refused.
	const char *const pointFiles[2] = { "tests/vectors/crypto-kat/ecdh-p384/wycheproof-ecpoint.json", "tests/vectors/crypto-kat/ecdh-p384/nist-cavp-pkv.json" };
	uint8_t ourPublic[kNetCryptoP384PublicBytes];
	NetEcdhKey *const fresh = NetCrypto_EcdhNew(ourPublic);
	Check(fresh != NULL && ourPublic[0] == 0x04, "a fresh P-384 key of our own, with its public half as 04, X, Y");

	for (const char *const name : pointFiles)
	{
		JsonValue file;
		const JsonValue *cases = NULL;

		if (!fresh || !LoadCases(name, &file, &cases))
		{
			continue;
		}

		Tally tally;

		for (const JsonValue &c : cases->items)
		{
			std::vector<uint8_t> theirs;
			uint8_t secret[kNetCryptoP384ScalarBytes];
			bool const usable = Bytes(c, "public", &theirs);
			bool const agreed = usable && theirs.size() == kNetCryptoP384PublicBytes && NetCrypto_EcdhAgree(fresh, theirs.data(), secret);
			std::string const result = Result(c);

			// "acceptable" points are compressed ones, which version 2 never sends: refused here, as the file allows
			bool const asSaid = result == "valid" ? agreed : !agreed;
			tally.Case(usable && asSaid, c);
		}

		tally.Say(name);
	}

	// Two fresh keys agree with each other, and an all-zero secret or a wrong first byte is refused
	uint8_t otherPublic[kNetCryptoP384PublicBytes];
	NetEcdhKey *const other = NetCrypto_EcdhNew(otherPublic);
	uint8_t a[kNetCryptoP384ScalarBytes], b[kNetCryptoP384ScalarBytes];
	Check(fresh && other && NetCrypto_EcdhAgree(fresh, otherPublic, a) && NetCrypto_EcdhAgree(other, ourPublic, b) && memcmp(a, b, sizeof(a)) == 0,
		"two fresh keys agree on the same secret, each from the other's public half");
	uint8_t wrongFirst[kNetCryptoP384PublicBytes];
	memcpy(wrongFirst, otherPublic, sizeof(wrongFirst));
	wrongFirst[0] = 0x05;
	memset(b, 0x77, sizeof(b));
	Check(fresh && !NetCrypto_EcdhAgree(fresh, wrongFirst, b) && std::all_of(b, b + sizeof(b), [](uint8_t x) { return x == 0; }),
		"a public key that doesn't start with 04 is refused, with nothing left of a secret");
	NetCrypto_EcdhFree(other);
	NetCrypto_EcdhFree(fresh);
}

// ---- ECDSA P-384 with SHA-384

static void TheSignatures()
{
	printf("ECDSA P-384 with SHA-384\n");
	const char *const files[3] = {
		"tests/vectors/crypto-kat/ecdsa-p384-sha384/nist-cavp-sigver.json",
		"tests/vectors/crypto-kat/ecdsa-p384-sha384/nist-cavp-siggen.json",
		"tests/vectors/crypto-kat/ecdsa-p384-sha384/wycheproof-p1363.json",
	};

	for (const char *const name : files)
	{
		JsonValue file;
		const JsonValue *cases = NULL;

		if (!LoadCases(name, &file, &cases))
		{
			continue;
		}

		Tally tally;

		for (const JsonValue &c : cases->items)
		{
			std::vector<uint8_t> key, msg, sig;
			bool const usable = Bytes(c, "public", &key) && Bytes(c, "msg", &msg) && Bytes(c, "sig", &sig);
			uint8_t hash[kNetCryptoHashBytes];
			NetCryptoPiece const piece = { Data(msg), msg.size() };

			// A signature that isn't exactly 96 bytes, or a key that isn't 97, is refused before CNG is asked (CRYPTO.md section 12)
			bool const checks = usable && key.size() == kNetCryptoP384PublicBytes && sig.size() == kNetCryptoEcdsaBytes && NetCrypto_Sha384(&piece, 1, hash)
				&& NetCrypto_EcdsaVerify(key.data(), hash, sig.data());
			tally.Case(usable && checks == (Result(c) == "valid"), c);
		}

		tally.Say(name);
	}
}

// ---- The post-quantum stand-in

static void ThePostQuantumStandIn()
{
	printf("The post-quantum half, as built: the stand-in\n");
	Check(!NetPq_Available() && NetPq_Name() && strstr(NetPq_Name(), "none") == NetPq_Name(), "it says it isn't there");

	static uint8_t publicKey[kNetPqKemPublicBytes], privateKey[kNetPqKemPrivateBytes], ciphertext[kNetPqKemCiphertextBytes];
	uint8_t secret[kNetPqKemSecretBytes];
	memset(publicKey, 0x5A, sizeof(publicKey));
	memset(privateKey, 0x5A, sizeof(privateKey));
	memset(ciphertext, 0x5A, sizeof(ciphertext));
	memset(secret, 0x5A, sizeof(secret));
	bool const keypair = NetPq_KemKeypair(publicKey, privateKey);
	bool const zeroed = std::all_of(publicKey, publicKey + sizeof(publicKey), [](uint8_t x) { return x == 0; })
		&& std::all_of(privateKey, privateKey + sizeof(privateKey), [](uint8_t x) { return x == 0; });
	Check(!keypair && zeroed, "a key pair is refused, with nothing left in the keys' room");

	memset(publicKey, 0x5A, sizeof(publicKey));
	bool const encapsulated = NetPq_KemEncapsulate(publicKey, ciphertext, secret);
	Check(!encapsulated && std::all_of(secret, secret + sizeof(secret), [](uint8_t x) { return x == 0; })
		&& std::all_of(ciphertext, ciphertext + sizeof(ciphertext), [](uint8_t x) { return x == 0; }), "encapsulating is refused, and nothing is left of a secret");

	memset(secret, 0x5A, sizeof(secret));
	Check(!NetPq_KemDecapsulate(privateKey, ciphertext, secret) && std::all_of(secret, secret + sizeof(secret), [](uint8_t x) { return x == 0; }),
		"decapsulating is refused too");

	static uint8_t dsaKey[kNetPqDsaPublicBytes], dsaSig[kNetPqDsaSignatureBytes];
	Check(!NetPq_DsaVerify(dsaKey, reinterpret_cast<const uint8_t *>("m"), 1, dsaSig), "and no signature ever checks");
}

// ---- The Noise state (CRYPTO.md section 5)

static void TheNoiseState()
{
	printf("The Noise state\n");
	NetNoiseState a, b;
	Check(NetNoise_Start(&a, kNetNoiseNameRelay) && NetNoise_Start(&b, kNetNoiseNameRelay) && memcmp(a.h, b.h, sizeof(a.h)) == 0
		&& memcmp(a.ck, a.h, sizeof(a.ck)) == 0 && !a.hasKey, "both sides start alike: h is the name's SHA-384, ck the same, and no key");

	NetNoiseState other;
	Check(NetNoise_Start(&other, kNetNoiseNamePeers) && memcmp(other.h, a.h, sizeof(a.h)) != 0, "and the other handshake's name starts somewhere else");

	static const uint8_t prologue[] = { 'l', 'o', 'c', 'a', 'l', 0 };
	Check(NetNoise_MixHash(&a, prologue, sizeof(prologue)) && NetNoise_MixHash(&b, prologue, sizeof(prologue)) && memcmp(a.h, b.h, sizeof(a.h)) == 0,
		"the same words mixed in on both sides keep h the same");

	// Before any key, words go as they are, but into h all the same
	uint8_t sent[64], got[64];
	size_t sentSize = 0, gotSize = 0;
	Check(NetNoise_EncryptAndHash(&a, prologue, sizeof(prologue), sent, sizeof(sent), &sentSize) && sentSize == sizeof(prologue)
		&& memcmp(sent, prologue, sizeof(prologue)) == 0 && NetNoise_DecryptAndHash(&b, sent, sentSize, got, sizeof(got), &gotSize) && gotSize == sizeof(prologue)
		&& memcmp(a.h, b.h, sizeof(a.h)) == 0, "with no key yet, words go unsealed, and both sides' h still agree");

	static const uint8_t secret[48] = { 1, 2, 3 };
	Check(NetNoise_MixKey(&a, secret, sizeof(secret)) && NetNoise_MixKey(&b, secret, sizeof(secret)) && a.hasKey && memcmp(a.k, b.k, sizeof(a.k)) == 0
		&& memcmp(a.ck, b.ck, sizeof(a.ck)) == 0, "the same secret mixed in gives both sides the same new key and chain");

	static const uint8_t words[] = { 'h', 'e', 'l', 'l', 'o' };
	Check(NetNoise_EncryptAndHash(&a, words, sizeof(words), sent, sizeof(sent), &sentSize) && sentSize == sizeof(words) + kNetCryptoTagBytes
		&& memcmp(sent, words, sizeof(words)) != 0, "with a key, words are sealed, and carry a 16-byte tag");

	NetNoiseState tampered = b;
	uint8_t changed[64];
	memcpy(changed, sent, sentSize);
	changed[0] ^= 1;
	Check(!NetNoise_DecryptAndHash(&tampered, changed, sentSize, got, sizeof(got), &gotSize), "one bit changed on the way, and the other side refuses them");
	Check(NetNoise_DecryptAndHash(&b, sent, sentSize, got, sizeof(got), &gotSize) && gotSize == sizeof(words) && memcmp(got, words, sizeof(words)) == 0
		&& memcmp(a.h, b.h, sizeof(a.h)) == 0, "as they came, they open, and both sides' h still agree");

	NetNoiseSplit splitA, splitB;
	Check(NetNoise_Split(&a, &splitA) && NetNoise_Split(&b, &splitB) && memcmp(&splitA, &splitB, sizeof(splitA)) == 0
		&& memcmp(splitA.initiatorToResponderKey, splitA.responderToInitiatorKey, kNetCryptoAesKeyBytes) != 0,
		"the split gives both sides the same two keys, and a different one each way");

	uint8_t epoch0[kNetCryptoAesKeyBytes], epoch1[kNetCryptoAesKeyBytes];
	Check(NetNoise_EpochKey(splitA.initiatorToResponderKey, 0, epoch0) && memcmp(epoch0, splitA.initiatorToResponderKey, sizeof(epoch0)) == 0
		&& NetNoise_EpochKey(splitA.initiatorToResponderKey, 1, epoch1) && memcmp(epoch1, epoch0, sizeof(epoch1)) != 0,
		"epoch 0 is the split's own key, and epoch 1 is another");

	NetNoise_Wipe(&a);
	Check(std::all_of(a.k, a.k + sizeof(a.k), [](uint8_t x) { return x == 0; }) && !a.hasKey, "wiping leaves nothing of the keys");
	NetNoise_Wipe(&b);
	NetNoise_Wipe(&tampered);
	NetNoise_Wipe(&other);
	NetNoise_WipeSplit(&splitA);
	NetNoise_WipeSplit(&splitB);
}

int main()
{
	TheHashes();
	TheKeyDerivation();
	TheSeal();
	TheKeyAgreement();
	TheSignatures();
	ThePostQuantumStandIn();
	TheNoiseState();

	if (checksFailed > 0)
	{
		printf("%d check%s FAILED\n", checksFailed, checksFailed == 1 ? "" : "s");
		return 1;
	}

	printf("Every check passed\n");
	return 0;
}
