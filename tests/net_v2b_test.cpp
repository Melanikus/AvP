// Tests of handshake B of protocol version 2 (3dc/net_handshake_b.h) and a player's identity (3dc/net_identity.h), the game's side.
// Written 2026-11-09 by Melanikus for the AvP port.
//
// WHAT'S CHECKED, and against what. The relay's repository has a reference handshake B written apart from this one, in Java
// (AVPx-Dedicated-Server, java/test/avpx/v2/HandshakeB.java), and its test vectors (tests/vectors/crypto-v2.json, copied byte for byte;
// the gate checks its sha256) hold one whole handshake B made by it from fixed keys and fixed randomness. So:
//   1. the two identities in the file: their ML-KEM public keys from their seeds, and their fingerprints; and a fingerprint's hex both ways;
//   2. the file's 23 steps, walked one by one with net_noise.h here in the test: every h and ck, every secret and key mixed, every
//      sealed piece. That checks the test's own reading of CRYPTO.md before it judges the game's code by it;
//   3. the game's NetHandshakeBJoiner and NetHandshakeBHost, given the file's keys and randomness, make B1 to B5 byte for byte, the same
//      keys both ways, the same transcript, and the check code 623782;
//   4. the file's 11 datagrams: each message cut by net_v2_fragments.h under 'AVPE' is exactly the file's, and reads back whole;
//   5. hostile messages, each refused for its own reason (CRYPTO.md section 16's number where there is one):
//      - a wrong size, a call out of turn, a bad start;
//      - B2 made with another host's keys under the listing's fingerprint (10), and B5 revealing another nJ (21), both with good
//        tags, made from the test's own walk;
//      - another region, port or fingerprint in the prologue (8);
//      - one bit changed in every byte of B1 to B5;
//   6. two handshakes with fresh keys and real randomness, from the game's own NetIdentity_Make: they agree, and differ from each other.
//
// The test's fixed randomness reaches mlkem-native's own "_derand" calls (keys from a seed, an encapsulation from its m) here alone: the
// game's code never has a way to be given fixed ML-KEM randomness (net_pq_native.cpp).

#include "net_handshake_b.h"
#include "net_identity.h"
#include "net_v2_fragments.h"

#include <mlkem-native/avpx_mlkem.h>

#include "json_reader.h"

#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

static int bChecksFailed = 0;

// Called BCheck, not Check: Snyk Code joins functions with the same name and signature across files, and many tests have a Check
static void BCheck(bool good, std::string const &what)
{
	printf("%s  %s\n", good ? "ok    " : "FAILED", what.c_str());
	fflush(stdout);
	bChecksFailed += good ? 0 : 1;
}

typedef std::vector<uint8_t> Bytes;

static bool Equal(const Bytes &a, const uint8_t *b, size_t size)
{
	return a.size() == size && (size == 0 || memcmp(a.data(), b, size) == 0);
}

static std::string ReasonName(int why)
{
	switch (why)
	{
		case kNetHandshakeBFine: return "fine";
		case kNetHandshakeBWrongSize: return "the wrong size (1)";
		case kNetHandshakeBBadTag: return "a tag that fails (8)";
		case kNetHandshakeBWrongHost: return "the wrong host (10)";
		case kNetHandshakeBBrokenPromise: return "nJ isn't what B1 promised (21)";
		case kNetHandshakeBNotAKey: return "not a key";
		case kNetHandshakeBOutOfTurn: return "out of turn";
		case kNetHandshakeBBadStart: return "a bad start";
		case kNetHandshakeBCouldnt: return "the cryptography failed";
		case kNetHandshakeBGivenUp: return "given up";
		default: return "reason " + std::to_string(why);
	}
}

// ---- The file

static JsonValue file;
static const JsonValue *b = NULL;   // the file's handshakeB

// A hex field of an object, as bytes; empty (and a failed check) if it's missing or isn't hex
static Bytes Hex(const JsonValue &object, const char *name)
{
	Bytes out;
	const JsonValue *const field = TextMember(object, name);

	if (!field || !FromHex(field->text, &out))
	{
		BCheck(false, std::string("the file has a hex field \"") + name + "\"");
		out.clear();
	}

	return out;
}

static Bytes BHex(const char *name)
{
	return Hex(*b, name);
}

static Bytes BHex(const char *object, const char *name)
{
	const JsonValue *const o = b->Member(object);
	return o ? Hex(*o, name) : Bytes();
}

static bool ReadTheFile()
{
	std::string const all = ReadRepositoryFile("tests/vectors/crypto-v2.json");
	JsonReader reader(all);

	if (all.empty() || !reader.ReadAll(&file) || file.kind != kJsonObject)
	{
		return false;
	}

	b = file.Member("handshakeB");
	return b && b->kind == kJsonObject;
}

static std::string Region()
{
	const JsonValue *const region = TextMember(*b, "region");
	return region ? region->text : std::string();
}

static uint16_t RelayPort()
{
	const JsonValue *const port = b->Member("relayPort");
	return port && port->kind == kJsonNumber && port->number <= 0xFFFF ? static_cast<uint16_t>(port->number) : 0;
}

// ---- Keys from the file, made with the libraries' fixed-randomness calls (test code only)

// An ML-KEM-1024 key pair from FIPS 203's d then z (the file's mlkemSeed, 64 bytes)
static bool KemPairFromSeed(const Bytes &seed, uint8_t publicOut[kNetPqKemPublicBytes], uint8_t privateOut[kNetPqKemPrivateBytes])
{
	return seed.size() == 64 && avpx_mlkem1024_keypair_derand(publicOut, privateOut, seed.data()) == 0;
}

// An encapsulation to publicKey from its m (32 bytes)
static bool EncapsulateFrom(const uint8_t publicKey[kNetPqKemPublicBytes], const Bytes &m, uint8_t ciphertextOut[kNetPqKemCiphertextBytes],
	uint8_t secretOut[kNetPqKemSecretBytes])
{
	return m.size() == 32 && avpx_mlkem1024_enc_derand(ciphertextOut, secretOut, publicKey, m.data()) == 0;
}

// A P-384 key from the file: its private number and its public key
static NetEcdhKey *EcdhFrom(const char *object)
{
	Bytes const priv = BHex(object, "p384Private");
	Bytes const pub = BHex(object, "p384Public");

	if (priv.size() != kNetCryptoP384ScalarBytes || pub.size() != kNetCryptoP384PublicBytes)
	{
		return NULL;
	}

	return NetCrypto_EcdhFromPrivate(priv.data(), pub.data());
}

// A whole identity from the file (its "host" or "joiner"), as NetIdentity_Make would make one, but from fixed keys
static bool IdentityFrom(const char *object, NetIdentity *out)
{
	memset(out, 0, sizeof(*out));
	Bytes const pub = BHex(object, "p384Public");
	out->p384 = EcdhFrom(object);

	if (!out->p384 || pub.size() != kNetCryptoP384PublicBytes)
	{
		NetIdentity_Free(out);
		return false;
	}

	memcpy(out->statics, pub.data(), kNetCryptoP384PublicBytes);

	if (!KemPairFromSeed(BHex(object, "mlkemSeed"), out->statics + kNetCryptoP384PublicBytes, out->kemPrivate)
		|| !NetIdentity_Fingerprint(NetIdentity_P384Public(out), NetIdentity_KemPublic(out), out->fingerprint))
	{
		NetIdentity_Free(out);
		return false;
	}

	out->made = true;
	return true;
}

static NetIdentity hostId, joinerId;

// ---- 1. The identities

static void TestIdentities()
{
	printf("\n1. The two identities in the file, and fingerprints\n");

	for (int which = 0; which < 2; which++)
	{
		const char *const name = which == 0 ? "host" : "joiner";
		NetIdentity *const id = which == 0 ? &hostId : &joinerId;
		bool const made = IdentityFrom(name, id);
		BCheck(made, std::string("the ") + name + "'s identity is made from the file's P-384 key and ML-KEM seed");

		if (!made)
		{
			continue;
		}

		BCheck(Equal(BHex(name, "mlkemPublic"), NetIdentity_KemPublic(id), kNetPqKemPublicBytes),
			std::string("its ML-KEM public key, from the seed, is the file's"));
		BCheck(Equal(BHex(name, "fingerprint"), id->fingerprint, kNetIdentityFingerprintBytes),
			std::string("its fingerprint (SHA-384 of \"AVPX-ID-2\" and both public keys, the first 16 bytes) is the file's"));

		char hex[kNetIdentityFingerprintHexBytes];
		NetIdentity_FingerprintHex(id->fingerprint, hex);
		const JsonValue *const written = TextMember(*b->Member(name), "fingerprint");
		BCheck(written && written->text == hex, std::string("and as 32 small hex digits it's written exactly as the file writes it: ") + hex);

		uint8_t back[kNetIdentityFingerprintBytes];
		BCheck(NetIdentity_FingerprintFromHex(hex, back) && memcmp(back, id->fingerprint, sizeof(back)) == 0, "and reads back to the same 16 bytes");
	}

	// A listing is input from outside the game: only exactly 32 small hex digits, with nothing after them
	uint8_t out[kNetIdentityFingerprintBytes];
	const char *const refused[] = {
		"11E6E2DFDA44451C8E4328B8A7CD50CA",     // capitals: the list never writes them
		"11e6e2dfda44451c8e4328b8a7cd50c",      // 31 digits
		"11e6e2dfda44451c8e4328b8a7cd50ca0",    // 33
		"11e6e2dfda44451c8e4328b8a7cd50cg",     // not a hex digit
		"11e6e2dfda44451c8e4328b8a7cd50c ",     // a space
		"",
	};

	for (size_t i = 0; i < sizeof(refused) / sizeof(refused[0]); i++)
	{
		memset(out, 0xAA, sizeof(out));
		bool const taken = NetIdentity_FingerprintFromHex(refused[i], out);
		BCheck(!taken && Equal(Bytes(sizeof(out), 0), out, sizeof(out)), std::string("a listing's fingerprint \"") + refused[i] + "\" is refused, and nothing of it kept");
	}

	BCheck(!NetIdentity_FingerprintFromHex(NULL, out), "and no fingerprint at all is refused");
}

// ---- 2. The steps, walked here with net_noise.h

// The walk's state after each of the file's steps, so the hostile tests can make messages with good tags from any point of it
static std::vector<NetNoiseState> walked;

static bool StepIs(size_t index, const NetNoiseState &noise)
{
	const JsonValue *const steps = b->Member("steps");

	if (!steps || steps->kind != kJsonList || index >= steps->items.size())
	{
		return false;
	}

	const JsonValue &step = steps->items[index];
	return Equal(Hex(step, "h"), noise.h, sizeof(noise.h)) && Equal(Hex(step, "ck"), noise.ck, sizeof(noise.ck));
}

static const JsonValue *Step(size_t index)
{
	const JsonValue *const steps = b->Member("steps");
	return steps && steps->kind == kJsonList && index < steps->items.size() ? &steps->items[index] : NULL;
}

static std::string StepName(size_t index)
{
	const JsonValue *const step = Step(index);
	const JsonValue *const name = step ? TextMember(*step, "step") : NULL;
	return name ? name->text : "step " + std::to_string(index);
}

static void Walked(NetNoiseState const &noise)
{
	size_t const index = walked.size();
	walked.push_back(noise);
	BCheck(StepIs(index, noise), "step " + std::to_string(index) + " gives the file's h and ck: " + StepName(index));
}

// A MixKey step: the secret mixed and the key made are the file's too
static void WalkedKey(NetNoiseState const &noise, const uint8_t *secret, size_t secretSize)
{
	const JsonValue *const step = Step(walked.size());
	BCheck(step && Equal(Hex(*step, "secret"), secret, secretSize) && Equal(Hex(*step, "k"), noise.k, sizeof(noise.k)),
		"step " + std::to_string(walked.size()) + "'s secret and k are the file's");
	Walked(noise);
}

// A sealing step: what it sent is the file's too
static void WalkedSent(NetNoiseState const &noise, const uint8_t *sent, size_t size)
{
	const JsonValue *const step = Step(walked.size());

	if (step && step->Member("sent"))
	{
		BCheck(Equal(Hex(*step, "sent"), sent, size), "step " + std::to_string(walked.size()) + " sent the file's bytes");
	}

	Walked(noise);
}

static void MixEcdh(NetNoiseState *noise, NetEcdhKey *mine, const Bytes &theirs)
{
	uint8_t secret[kNetCryptoP384ScalarBytes] = {};
	bool const agreed = theirs.size() == kNetCryptoP384PublicBytes && NetCrypto_EcdhAgree(mine, theirs.data(), secret)
		&& NetNoise_MixKey(noise, secret, sizeof(secret));
	BCheck(agreed, "an ECDH of the walk agrees, and is mixed");
	WalkedKey(*noise, secret, sizeof(secret));
}

static void MixSecret(NetNoiseState *noise, const Bytes &secret)
{
	BCheck(NetNoise_MixKey(noise, secret.data(), secret.size()), "an ML-KEM secret of the walk is mixed");
	WalkedKey(*noise, secret.data(), secret.size());
}

static void Hash(NetNoiseState *noise, const uint8_t *bytes, size_t size)
{
	BCheck(NetNoise_MixHash(noise, bytes, size), "a MixHash of the walk");
	Walked(*noise);
}

static void Seal(NetNoiseState *noise, const uint8_t *text, size_t size)
{
	std::vector<uint8_t> out(size + kNetCryptoTagBytes);
	size_t written = 0;
	static const uint8_t kNothing[1] = { 0 };
	BCheck(NetNoise_EncryptAndHash(noise, size > 0 ? text : kNothing, size, out.data(), out.size(), &written) && written == out.size(),
		"an EncryptAndHash of the walk");
	WalkedSent(*noise, out.data(), out.size());
}

static void TestSteps()
{
	printf("\n2. The file's 23 steps, walked one by one\n");
	NetNoiseState noise;
	std::string const region = Region();
	uint16_t const port = RelayPort();
	BCheck(region == "virginia" && port == 27507, "the file's handshake is for region \"virginia\", relay port 27507");

	BCheck(NetNoise_Start(&noise, kNetNoiseNamePeers), "start");
	Walked(noise);
	Bytes named(region.begin(), region.end());
	named.push_back(0);
	Hash(&noise, named.data(), named.size());
	uint8_t const portBytes[2] = { static_cast<uint8_t>(port & 0xFF), static_cast<uint8_t>(port >> 8) };
	Hash(&noise, portBytes, sizeof(portBytes));
	Hash(&noise, hostId.fingerprint, sizeof(hostId.fingerprint));

	// B1: e, and the promise
	Bytes const jeP384 = BHex("joinerEphemeral", "p384Public");
	Bytes const jeKem = BHex("joinerEphemeral", "mlkemPublic");
	Bytes const nJ = BHex("nJ");
	Bytes const commitment = BHex("commitment");
	uint8_t promise[kNetCryptoHashBytes] = {};
	NetCryptoPiece const piece = { nJ.data(), nJ.size() };
	BCheck(nJ.size() == 32 && NetCrypto_Sha384(&piece, 1, promise) && Equal(commitment, promise, sizeof(promise)),
		"the file's commitment is SHA-384 of its nJ, with nothing before or after");
	Hash(&noise, jeP384.data(), jeP384.size());
	Hash(&noise, jeKem.data(), jeKem.size());
	Hash(&noise, commitment.data(), commitment.size());

	// B2: e, ee, ekem, s, es, and a tag
	NetEcdhKey *const hostEphemeral = EcdhFrom("hostEphemeral");
	NetEcdhKey *const joinerEphemeral = EcdhFrom("joinerEphemeral");
	BCheck(hostEphemeral && joinerEphemeral, "the file's two fresh P-384 keys are taken");
	Bytes const reP384 = BHex("hostEphemeral", "p384Public");
	Hash(&noise, reP384.data(), reP384.size());
	MixEcdh(&noise, hostEphemeral, jeP384);

	// The test's own check of the file's encapsulations: each m gives its ciphertext and secret, and the secret decapsulates back
	uint8_t ekPublic[kNetPqKemPublicBytes], ekPrivate[kNetPqKemPrivateBytes];
	uint8_t ciphertext[kNetPqKemCiphertextBytes], secret[kNetPqKemSecretBytes], back[kNetPqKemSecretBytes];
	BCheck(KemPairFromSeed(BHex("joinerEphemeral", "mlkemSeed"), ekPublic, ekPrivate) && Equal(jeKem, ekPublic, sizeof(ekPublic)),
		"the joiner's fresh ML-KEM key, from its seed, is the file's");
	BCheck(EncapsulateFrom(ekPublic, BHex("ekem", "m"), ciphertext, secret) && Equal(BHex("ekem", "ciphertext"), ciphertext, sizeof(ciphertext))
		&& Equal(BHex("ekem", "secret"), secret, sizeof(secret)) && NetPq_KemDecapsulate(ekPrivate, ciphertext, back)
		&& memcmp(back, secret, sizeof(back)) == 0, "ekem: the file's m gives its ciphertext and secret, which decapsulates back");
	Hash(&noise, ciphertext, sizeof(ciphertext));
	MixSecret(&noise, BHex("ekem", "secret"));
	Seal(&noise, hostId.statics, sizeof(hostId.statics));
	MixEcdh(&noise, hostId.p384, jeP384);
	Seal(&noise, NULL, 0);

	// B3: skem, s, se, and a tag
	BCheck(EncapsulateFrom(NetIdentity_KemPublic(&hostId), BHex("skemToHost", "m"), ciphertext, secret)
		&& Equal(BHex("skemToHost", "ciphertext"), ciphertext, sizeof(ciphertext)) && Equal(BHex("skemToHost", "secret"), secret, sizeof(secret))
		&& NetPq_KemDecapsulate(hostId.kemPrivate, ciphertext, back) && memcmp(back, secret, sizeof(back)) == 0,
		"skem to the host: the file's m gives its ciphertext and secret, which the host's own key decapsulates back");
	Hash(&noise, ciphertext, sizeof(ciphertext));
	MixSecret(&noise, BHex("skemToHost", "secret"));
	Seal(&noise, joinerId.statics, sizeof(joinerId.statics));
	MixEcdh(&noise, joinerId.p384, reP384);
	Seal(&noise, NULL, 0);

	// B4: skem, then nH
	BCheck(EncapsulateFrom(NetIdentity_KemPublic(&joinerId), BHex("skemToJoiner", "m"), ciphertext, secret)
		&& Equal(BHex("skemToJoiner", "ciphertext"), ciphertext, sizeof(ciphertext)) && Equal(BHex("skemToJoiner", "secret"), secret, sizeof(secret))
		&& NetPq_KemDecapsulate(joinerId.kemPrivate, ciphertext, back) && memcmp(back, secret, sizeof(back)) == 0,
		"skem to the joiner: the same");
	Hash(&noise, ciphertext, sizeof(ciphertext));
	MixSecret(&noise, BHex("skemToJoiner", "secret"));
	Bytes const nH = BHex("nH");
	Seal(&noise, nH.data(), nH.size());

	// B5: nJ. Then h is the transcript, and Split gives the file's keys.
	Seal(&noise, nJ.data(), nJ.size());
	const JsonValue *const steps = b->Member("steps");
	BCheck(steps && walked.size() == steps->items.size(), "the walk took exactly the file's " + std::to_string(walked.size()) + " steps");
	BCheck(Equal(BHex("transcript"), noise.h, sizeof(noise.h)), "the transcript is h after B5 (5.1 point 15)");

	NetCrypto_EcdhFree(hostEphemeral);
	NetCrypto_EcdhFree(joinerEphemeral);
	NetCrypto_Wipe(ekPrivate, sizeof(ekPrivate));
}

// ---- 3. The game's handshake, from the file's keys and randomness

// Everything one run of B made, kept to compare and to break
struct Run
{
	uint8_t b1[kNetHandshakeBB1Bytes];
	uint8_t b2[kNetHandshakeBB2Bytes];
	uint8_t b3[kNetHandshakeBB3Bytes];
	uint8_t b4[kNetHandshakeBB4Bytes];
	uint8_t b5[kNetHandshakeBB5Bytes];
	NetHandshakeBKeys joinerKeys;
	NetHandshakeBKeys hostKeys;
};

// The joiner's WriteB1 with the file's draws, for the expected host's fingerprint, region and port given
static bool FileB1(NetHandshakeBJoiner *joiner, const uint8_t expected[kNetIdentityFingerprintBytes], const char *region, uint16_t port, uint8_t b1Out[kNetHandshakeBB1Bytes])
{
	uint8_t ekPublic[kNetPqKemPublicBytes], ekPrivate[kNetPqKemPrivateBytes];
	Bytes const nJ = BHex("nJ");
	Bytes const ePublic = BHex("joinerEphemeral", "p384Public");
	bool const drawn = KemPairFromSeed(BHex("joinerEphemeral", "mlkemSeed"), ekPublic, ekPrivate) && nJ.size() == 32
		&& ePublic.size() == kNetCryptoP384PublicBytes;
	bool const written = drawn && joiner->WriteB1(region, port, expected, &joinerId, EcdhFrom("joinerEphemeral"), ePublic.data(), ekPublic, ekPrivate,
		nJ.data(), b1Out);
	NetCrypto_Wipe(ekPrivate, sizeof(ekPrivate));
	return written;
}

// The host's ReadB1 and WriteB2 with the file's draws
static bool FileB2(NetHandshakeBHost *host, const char *region, uint16_t port, const uint8_t *b1, size_t size, uint8_t b2Out[kNetHandshakeBB2Bytes])
{
	if (!host->ReadB1(region, port, &hostId, b1, size))
	{
		return false;
	}

	uint8_t ciphertext[kNetPqKemCiphertextBytes], secret[kNetPqKemSecretBytes];
	Bytes const rePublic = BHex("hostEphemeral", "p384Public");

	if (!EncapsulateFrom(host->JoinerEphemeralKemPublic(), BHex("ekem", "m"), ciphertext, secret))
	{
		host->Abandon(kNetHandshakeBNotAKey);   // as the game does when FIPS 203 refuses the joiner's key
		return false;
	}

	bool const written = host->WriteB2(EcdhFrom("hostEphemeral"), rePublic.data(), ciphertext, secret, b2Out);
	NetCrypto_Wipe(secret, sizeof(secret));
	return written;
}

static bool FileB3(NetHandshakeBJoiner *joiner, const uint8_t *b2, size_t size, uint8_t b3Out[kNetHandshakeBB3Bytes])
{
	if (!joiner->ReadB2(b2, size))
	{
		return false;
	}

	uint8_t ciphertext[kNetPqKemCiphertextBytes], secret[kNetPqKemSecretBytes];

	if (!EncapsulateFrom(joiner->HostKemPublic(), BHex("skemToHost", "m"), ciphertext, secret))
	{
		joiner->Abandon(kNetHandshakeBNotAKey);
		return false;
	}

	bool const written = joiner->WriteB3(ciphertext, secret, b3Out);
	NetCrypto_Wipe(secret, sizeof(secret));
	return written;
}

static bool FileB4(NetHandshakeBHost *host, const uint8_t *b3, size_t size, uint8_t b4Out[kNetHandshakeBB4Bytes])
{
	if (!host->ReadB3(b3, size))
	{
		return false;
	}

	uint8_t ciphertext[kNetPqKemCiphertextBytes], secret[kNetPqKemSecretBytes];
	Bytes const nH = BHex("nH");

	if (nH.size() != 32 || !EncapsulateFrom(host->JoinerKemPublic(), BHex("skemToJoiner", "m"), ciphertext, secret))
	{
		host->Abandon(kNetHandshakeBNotAKey);
		return false;
	}

	bool const written = host->WriteB4(ciphertext, secret, nH.data(), b4Out);
	NetCrypto_Wipe(secret, sizeof(secret));
	return written;
}

static bool SameSplit(const NetNoiseSplit &one, const NetNoiseSplit &other)
{
	return memcmp(&one, &other, sizeof(one)) == 0;
}

static void TestTheGamesHandshake(Run *run)
{
	printf("\n3. The game's handshake B, from the file's keys and randomness\n");
	std::string const region = Region();
	uint16_t const port = RelayPort();
	NetHandshakeBJoiner joiner;
	NetHandshakeBHost host;

	BCheck(FileB1(&joiner, hostId.fingerprint, region.c_str(), port, run->b1) && Equal(BHex("b1"), run->b1, sizeof(run->b1)),
		"the joiner's B1 is the file's, byte for byte (1,713 bytes)");
	BCheck(FileB2(&host, region.c_str(), port, run->b1, sizeof(run->b1), run->b2) && Equal(BHex("b2"), run->b2, sizeof(run->b2)),
		"the host reads it, and its B2 is the file's (3,362 bytes)");
	BCheck(FileB3(&joiner, run->b2, sizeof(run->b2), run->b3) && Equal(BHex("b3"), run->b3, sizeof(run->b3)),
		"the joiner reads it (the host is the one the listing named), and its B3 is the file's (3,265 bytes)");
	BCheck(FileB4(&host, run->b3, sizeof(run->b3), run->b4) && Equal(BHex("b4"), run->b4, sizeof(run->b4)),
		"the host reads it, and its B4 is the file's (1,616 bytes)");
	BCheck(joiner.ReadB4(run->b4, sizeof(run->b4), run->b5, &run->joinerKeys) && Equal(BHex("b5"), run->b5, sizeof(run->b5)) && joiner.Done(),
		"the joiner reads it (the host's key confirmation), and its B5 is the file's (48 bytes)");
	BCheck(host.ReadB5(run->b5, sizeof(run->b5), &run->hostKeys) && host.Done(),
		"the host reads it: the joiner's key confirmation, and nJ is what B1 promised");

	NetNoiseSplit expected;
	Bytes const k1 = BHex("split", "keyJoinerToHost"), k2 = BHex("split", "keyHostToJoiner");
	Bytes const iv1 = BHex("split", "ivJoinerToHost"), iv2 = BHex("split", "ivHostToJoiner");
	bool const sized = k1.size() == 32 && k2.size() == 32 && iv1.size() == 12 && iv2.size() == 12;

	if (sized)
	{
		memcpy(expected.initiatorToResponderKey, k1.data(), 32);
		memcpy(expected.responderToInitiatorKey, k2.data(), 32);
		memcpy(expected.initiatorToResponderIv, iv1.data(), 12);
		memcpy(expected.responderToInitiatorIv, iv2.data(), 12);
	}

	BCheck(sized && SameSplit(expected, run->joinerKeys.split) && SameSplit(expected, run->hostKeys.split),
		"both sides have the file's keys and IVs, joiner-to-host as Split's \"initiator to responder\"");
	BCheck(Equal(BHex("transcript"), run->joinerKeys.transcript, kNetCryptoHashBytes) && Equal(BHex("transcript"), run->hostKeys.transcript, kNetCryptoHashBytes),
		"both have the file's transcript, h after B5");
	BCheck(memcmp(run->joinerKeys.theirFingerprint, hostId.fingerprint, kNetIdentityFingerprintBytes) == 0
		&& memcmp(run->hostKeys.theirFingerprint, joinerId.fingerprint, kNetIdentityFingerprintBytes) == 0,
		"the joiner knows the host's fingerprint, and the host the joiner's");

	// The code is made first, on its own line: in one call, the words could be put together before it was made
	char code[kNetHandshakeBCheckCodeBytes] = {};
	const JsonValue *const fileCode = TextMember(*b, "checkCode");
	bool const made = NetHandshakeB_CheckCode(run->joinerKeys.transcript, code);
	BCheck(made && fileCode && fileCode->text == code && std::string(code) == "623782", std::string("the check code is the file's, 623782: ") + code);

	// Every call after the end is out of turn, and gives nothing
	uint8_t b5[kNetHandshakeBB5Bytes];
	NetHandshakeBKeys keys;
	BCheck(!joiner.ReadB4(run->b4, sizeof(run->b4), b5, &keys) && joiner.Why() == kNetHandshakeBOutOfTurn
		&& Equal(Bytes(sizeof(b5), 0), b5, sizeof(b5)), "after the end, B4 again is out of turn, and nothing is made");
	BCheck(!host.ReadB5(run->b5, sizeof(run->b5), &keys) && host.Why() == kNetHandshakeBOutOfTurn, "and so is B5 again");
}

// The check code from 8 bytes: read little-endian, mod 1,000,000, 6 digits with 0s in front. Checked here on a transcript whose HKDF
// output the test works out itself, so a big-endian reading or a lost 0 in front can't pass.
static void TestCheckCodeForm()
{
	printf("\n   The check code's form\n");
	int leadingZeros = 0;

	for (uint8_t seed = 0; seed < 200; seed++)
	{
		uint8_t transcript[kNetCryptoHashBytes];
		memset(transcript, seed, sizeof(transcript));
		uint8_t eight[8];
		bool const derived = NetCrypto_HkdfSha384(transcript, sizeof(transcript), NULL, 0, reinterpret_cast<const uint8_t *>("AVPX check code v2"), 18, eight, 8);
		uint64_t n = 0;

		for (int i = 7; i >= 0; i--)
		{
			n = (n << 8) | eight[i];
		}

		char expected[16];
		snprintf(expected, sizeof(expected), "%06llu", static_cast<unsigned long long>(n % 1000000));
		char code[kNetHandshakeBCheckCodeBytes];

		if (!derived || !NetHandshakeB_CheckCode(transcript, code) || std::string(code) != expected)
		{
			BCheck(false, std::string("the check code of a transcript of all ") + std::to_string(seed) + " is " + expected);
			return;
		}

		leadingZeros += code[0] == '0' ? 1 : 0;
	}

	BCheck(true, "200 transcripts each give the 6 digits worked out here (little-endian, mod 1,000,000), "
		+ std::to_string(leadingZeros) + " of them with a 0 in front");
}

// ---- 4. The datagrams

static void TestDatagrams(const Run &run)
{
	printf("\n4. The file's datagrams: B1 to B5 cut under 'AVPE'\n");
	const JsonValue *const datagrams = b->Member("datagrams");
	Bytes const id = datagrams ? Hex(*datagrams, "id") : Bytes();
	static const uint8_t kNoCookie[kNetV2CookieBytes] = {};
	const uint8_t *const messages[5] = { run.b1, run.b2, run.b3, run.b4, run.b5 };
	size_t const sizes[5] = { sizeof(run.b1), sizeof(run.b2), sizeof(run.b3), sizeof(run.b4), sizeof(run.b5) };
	const char *const names[5] = { "b1", "b2", "b3", "b4", "b5" };
	size_t total = 0;

	for (size_t m = 0; m < 5 && id.size() == kNetV2IdBytes; m++)
	{
		const JsonValue *const list = datagrams->Member(names[m]);
		size_t const count = NetV2_FragmentCount(sizes[m]);
		bool same = list && list->kind == kJsonList && list->items.size() == count;
		NetV2Assembly assembly;
		NetV2_AssemblyClear(&assembly);
		uint8_t const kind = static_cast<uint8_t>(kNetV2KindB1 + m);

		for (size_t i = 0; same && i < count; i++)
		{
			uint8_t datagram[kNetV2DatagramBytes];
			Bytes expected;
			NetV2Fragment fragment;
			same = NetV2_CutFragment(kNetV2MagicEndToEnd, kind, id.data(), kNoCookie, messages[m], sizes[m], i, datagram)
				&& list->items[i].kind == kJsonText && FromHex(list->items[i].text, &expected) && Equal(expected, datagram, sizeof(datagram))
				&& NetV2_ReadFragment(kNetV2MagicEndToEnd, datagram, sizeof(datagram), &fragment) && fragment.kind == kind
				&& !NetV2_ReadFragment(kNetV2MagicRelay, datagram, sizeof(datagram), &fragment)
				&& NetV2_ReadFragment(kNetV2MagicEndToEnd, datagram, sizeof(datagram), &fragment) && NetV2_AssemblyAdd(&assembly, &fragment);
			total++;
		}

		same = same && NetV2_AssemblyComplete(&assembly) && assembly.length == sizes[m] && memcmp(assembly.message, messages[m], sizes[m]) == 0;
		BCheck(same, std::string(names[m]) + ": its " + std::to_string(count) + " datagram(s) of 1,200 bytes are the file's, aren't 'AVPR', and read back whole");
	}

	BCheck(total == 11, "11 datagrams in all, as the file has");
}

// ---- 5. Hostile messages

// A joiner whose B1 is written (the file's), ready for a B2
static bool JoinerAtB2(NetHandshakeBJoiner *joiner)
{
	uint8_t b1[kNetHandshakeBB1Bytes];
	return FileB1(joiner, hostId.fingerprint, Region().c_str(), RelayPort(), b1);
}

// A joiner whose B3 is written, ready for a B4
static bool JoinerAtB4(NetHandshakeBJoiner *joiner, const Run &run)
{
	uint8_t b3[kNetHandshakeBB3Bytes];
	return JoinerAtB2(joiner) && FileB3(joiner, run.b2, sizeof(run.b2), b3);
}

// A host whose B2 is written, ready for a B3
static bool HostAtB3(NetHandshakeBHost *host, const Run &run)
{
	uint8_t b2[kNetHandshakeBB2Bytes];
	return FileB2(host, Region().c_str(), RelayPort(), run.b1, sizeof(run.b1), b2);
}

// A host whose B4 is written, ready for a B5
static bool HostAtB5(NetHandshakeBHost *host, const Run &run)
{
	uint8_t b4[kNetHandshakeBB4Bytes];
	return HostAtB3(host, run) && FileB4(host, run.b3, sizeof(run.b3), b4);
}

static void TestWrongSizesAndTurns(const Run &run)
{
	printf("\n5a. The wrong sizes, out of turn, and bad starts\n");

	{
		NetHandshakeBJoiner joiner;
		BCheck(JoinerAtB2(&joiner) && !joiner.ReadB2(run.b2, sizeof(run.b2) - 1) && joiner.Why() == kNetHandshakeBWrongSize,
			"a B2 one byte short is the wrong size (1)");
		BCheck(joiner.HostKemPublic() == NULL, "and the joiner has no host key to encapsulate to");
		uint8_t b3[kNetHandshakeBB3Bytes];
		uint8_t ct[kNetPqKemCiphertextBytes] = {}, ss[kNetPqKemSecretBytes] = {};
		memset(b3, 0xAA, sizeof(b3));
		BCheck(!joiner.WriteB3(ct, ss, b3) && joiner.Why() == kNetHandshakeBOutOfTurn && Equal(Bytes(sizeof(b3), 0), b3, sizeof(b3)),
			"after a refusal, nothing more is made: B3 is out of turn, and its bytes are all 0");
	}
	{
		NetHandshakeBHost host;
		BCheck(!host.ReadB1(Region().c_str(), RelayPort(), &hostId, run.b1, sizeof(run.b1) + 1) && host.Why() == kNetHandshakeBWrongSize,
			"a B1 one byte long is the wrong size (1)");
	}
	{
		NetHandshakeBHost host;
		BCheck(HostAtB3(&host, run) && !host.ReadB3(run.b3, 0) && host.Why() == kNetHandshakeBWrongSize, "a B3 of no bytes is the wrong size (1)");
	}
	{
		NetHandshakeBJoiner joiner;
		uint8_t b5[kNetHandshakeBB5Bytes];
		NetHandshakeBKeys keys;
		BCheck(JoinerAtB4(&joiner, run) && !joiner.ReadB4(run.b4, sizeof(run.b4) - 16, b5, &keys) && joiner.Why() == kNetHandshakeBWrongSize,
			"a B4 short by its tag is the wrong size (1)");
	}
	{
		NetHandshakeBHost host;
		NetHandshakeBKeys keys;
		BCheck(HostAtB5(&host, run) && !host.ReadB5(run.b5, 32, &keys) && host.Why() == kNetHandshakeBWrongSize, "a B5 of nJ with no tag is the wrong size (1)");
	}
	{
		NetHandshakeBJoiner joiner;
		BCheck(!joiner.ReadB2(run.b2, sizeof(run.b2)) && joiner.Why() == kNetHandshakeBOutOfTurn, "a B2 before B1 was written is out of turn");
		NetHandshakeBHost host;
		BCheck(!host.ReadB3(run.b3, sizeof(run.b3)) && host.Why() == kNetHandshakeBOutOfTurn, "a B3 before B2 was written is out of turn");
	}

	// Bad starts: no identity, a region the list would never give, a port of 0
	uint8_t b1[kNetHandshakeBB1Bytes];
	const char *const regions[] = { "", "Virginia", "virginia ", "a-region-of-17-ch", "nyc3/../x" };

	for (size_t i = 0; i < sizeof(regions) / sizeof(regions[0]); i++)
	{
		NetHandshakeBJoiner joiner;
		BCheck(!FileB1(&joiner, hostId.fingerprint, regions[i], RelayPort(), b1) && joiner.Why() == kNetHandshakeBBadStart,
			std::string("the region \"") + regions[i] + "\" is a bad start");
		NetHandshakeBHost host;
		BCheck(!host.ReadB1(regions[i], RelayPort(), &hostId, run.b1, sizeof(run.b1)) && host.Why() == kNetHandshakeBBadStart,
			"for the host too");
	}

	{
		NetHandshakeBJoiner joiner;
		BCheck(!FileB1(&joiner, hostId.fingerprint, Region().c_str(), 0, b1) && joiner.Why() == kNetHandshakeBBadStart
			&& Equal(Bytes(sizeof(b1), 0), b1, sizeof(b1)), "a relay port of 0 is a bad start, and B1's bytes are all 0");
		NetHandshakeBHost host;
		NetIdentity none;
		memset(&none, 0, sizeof(none));
		BCheck(!host.ReadB1(Region().c_str(), RelayPort(), &none, run.b1, sizeof(run.b1)) && host.Why() == kNetHandshakeBBadStart,
			"an identity never made is a bad start");
		BCheck(!host.ReadB1(Region().c_str(), RelayPort(), &hostId, run.b1, sizeof(run.b1)) && host.Why() == kNetHandshakeBOutOfTurn,
			"and a handshake that has failed stays failed: a good B1 after it is out of turn");
		NetHandshakeBHost fresh;
		BCheck(!fresh.ReadB1(Region().c_str(), RelayPort(), NULL, run.b1, sizeof(run.b1)) && fresh.Why() == kNetHandshakeBBadStart, "and no identity at all is a bad start");
	}

	// A 16-letter region is the longest taken
	{
		NetHandshakeBJoiner joiner;
		BCheck(FileB1(&joiner, hostId.fingerprint, "abcdefghijklmn-0", RelayPort(), b1), "a region of 16 small letters, digits and - is taken");
	}
}

// B2 made from the walk with another identity's keys, under the listing's fingerprint: the man in the middle that the fingerprint check
// is there for. Its tags are good, because the walk has the joiner's own state; only the fingerprint can tell.
static void TestWrongHost(const Run &run)
{
	printf("\n5b. The wrong host (10), and the wrong prologue (8)\n");

	if (walked.size() < 11)
	{
		BCheck(false, "the walk reached B2's ekem");
		return;
	}

	NetNoiseState noise = walked[10];   // after "B2, ekem: MixKey(its secret)", the same on both sides
	uint8_t b2[kNetHandshakeBB2Bytes];
	memcpy(b2, run.b2, kNetCryptoP384PublicBytes + kNetPqKemCiphertextBytes);   // e and ekem as the real host sent them
	uint8_t *const sealedStatics = b2 + kNetCryptoP384PublicBytes + kNetPqKemCiphertextBytes;
	size_t written = 0;
	uint8_t secret[kNetCryptoP384ScalarBytes];
	Bytes const jeP384 = BHex("joinerEphemeral", "p384Public");

	// The impostor is the joiner's own identity: any identity but the listed host's does
	bool const made = NetNoise_EncryptAndHash(&noise, joinerId.statics, sizeof(joinerId.statics), sealedStatics, kNetHandshakeBSealedStaticsBytes, &written)
		&& NetCrypto_EcdhAgree(joinerId.p384, jeP384.data(), secret) && NetNoise_MixKey(&noise, secret, sizeof(secret))
		&& NetHandshakeB_SealNothing(&noise, sealedStatics + kNetHandshakeBSealedStaticsBytes);
	NetCrypto_Wipe(secret, sizeof(secret));

	NetHandshakeBJoiner joiner;
	BCheck(made && JoinerAtB2(&joiner) && !joiner.ReadB2(b2, sizeof(b2)) && joiner.Why() == kNetHandshakeBWrongHost,
		"B2 sealing another identity's keys, with good tags, is the wrong host (10): \"this isn't the host the list named\"");
	BCheck(joiner.HostKemPublic() == NULL, "and the joiner has nothing to answer it with");

	// The listing named another host (here, the joiner's own fingerprint): the prologues differ, so B2's first seal fails at the joiner
	{
		NetHandshakeBJoiner other;
		uint8_t b1[kNetHandshakeBB1Bytes];
		BCheck(FileB1(&other, joinerId.fingerprint, Region().c_str(), RelayPort(), b1) && !other.ReadB2(run.b2, sizeof(run.b2))
			&& other.Why() == kNetHandshakeBBadTag, "a joiner that expected another host's fingerprint refuses the real host's B2 at its tag (8)");
	}

	// Another region, another port: the same
	{
		NetHandshakeBJoiner other;
		uint8_t b1[kNetHandshakeBB1Bytes];
		BCheck(FileB1(&other, hostId.fingerprint, "paris", RelayPort(), b1) && !other.ReadB2(run.b2, sizeof(run.b2)) && other.Why() == kNetHandshakeBBadTag,
			"a joiner on another region's relay refuses it at its tag (8)");
	}
	{
		NetHandshakeBJoiner other;
		uint8_t b1[kNetHandshakeBB1Bytes];
		BCheck(FileB1(&other, hostId.fingerprint, Region().c_str(), static_cast<uint16_t>(RelayPort() + 1), b1) && !other.ReadB2(run.b2, sizeof(run.b2))
			&& other.Why() == kNetHandshakeBBadTag, "and so does one on another relay port (8)");
	}

	// And the host's side of the same: a B1 meant for another host's fingerprint gets a B2 its joiner can't take
	{
		NetHandshakeBJoiner other;
		NetHandshakeBHost host;
		uint8_t b1[kNetHandshakeBB1Bytes], b2Other[kNetHandshakeBB2Bytes];
		BCheck(FileB1(&other, joinerId.fingerprint, Region().c_str(), RelayPort(), b1) && FileB2(&host, Region().c_str(), RelayPort(), b1, sizeof(b1), b2Other)
			&& !other.ReadB2(b2Other, sizeof(b2Other)) && other.Why() == kNetHandshakeBBadTag,
			"a host answering a B1 meant for another host's fingerprint makes a B2 that joiner refuses (8)");
	}
}

// B5 made from the walk revealing another nJ: its tag is good, so only the promise in B1 can tell
static void TestBrokenPromise(const Run &run)
{
	printf("\n5c. The broken promise (21)\n");

	if (walked.size() < 22)
	{
		BCheck(false, "the walk reached B4");
		return;
	}

	NetNoiseState noise = walked[21];   // after "B4: EncryptAndHash(nH)", the same on both sides
	uint8_t other[kNetHandshakeBNonceBytes];
	Bytes const nJ = BHex("nJ");
	memcpy(other, nJ.data(), sizeof(other));
	other[0] ^= 0x01;
	uint8_t b5[kNetHandshakeBB5Bytes];
	size_t written = 0;
	bool const made = NetNoise_EncryptAndHash(&noise, other, sizeof(other), b5, sizeof(b5), &written) && written == sizeof(b5);

	NetHandshakeBHost host;
	NetHandshakeBKeys keys;
	memset(&keys, 0xAA, sizeof(keys));
	BCheck(made && HostAtB5(&host, run) && !host.ReadB5(b5, sizeof(b5), &keys) && host.Why() == kNetHandshakeBBrokenPromise,
		"B5 revealing another nJ, with a good tag, is refused: nJ isn't what B1 promised (21)");
	NetHandshakeBKeys none;
	memset(&none, 0, sizeof(none));
	BCheck(memcmp(&keys, &none, sizeof(keys)) == 0, "and no keys come of it: they're all 0");
}

// One bit changed in each byte of each message, at the side that reads it. Each must be refused, and for the reason its place gives.
static void TestEveryByte(const Run &run)
{
	printf("\n5d. One bit changed in every byte of B1 to B5\n");

	// B1, at the host. The host can't tell yet (B1 has no tag): the joiner, whose own h holds the real B1, refuses the B2 that comes
	// back. A changed P-384 key is refused at once (ee), and a changed ML-KEM key may be refused by FIPS 203's check when the host
	// encapsulates to it.
	{
		size_t refusedByJoiner = 0, refusedAsKey = 0, wrong = 0;

		for (size_t i = 0; i < sizeof(run.b1); i++)
		{
			uint8_t b1[kNetHandshakeBB1Bytes];
			memcpy(b1, run.b1, sizeof(b1));
			b1[i] ^= static_cast<uint8_t>(1u << (i % 8));
			NetHandshakeBHost host;
			uint8_t b2[kNetHandshakeBB2Bytes];

			if (!FileB2(&host, Region().c_str(), RelayPort(), b1, sizeof(b1), b2))
			{
				refusedAsKey += host.Why() == kNetHandshakeBNotAKey ? 1 : 0;
				wrong += host.Why() == kNetHandshakeBNotAKey ? 0 : 1;
				wrong += i >= kNetCryptoP384PublicBytes + kNetPqKemPublicBytes ? 1 : 0;   // the promise is never a key
				continue;
			}

			wrong += i < kNetCryptoP384PublicBytes ? 1 : 0;   // a changed P-384 key must never get as far as B2
			NetHandshakeBJoiner joiner;
			bool const refused = JoinerAtB2(&joiner) && !joiner.ReadB2(b2, sizeof(b2)) && joiner.Why() == kNetHandshakeBBadTag;
			refusedByJoiner += refused ? 1 : 0;
			wrong += refused ? 0 : 1;
		}

		BCheck(wrong == 0 && refusedAsKey + refusedByJoiner == sizeof(run.b1), "B1: all " + std::to_string(sizeof(run.b1)) + " refused, "
			+ std::to_string(refusedAsKey) + " at once as not a key, " + std::to_string(refusedByJoiner) + " by the joiner at B2's tag (8)");
	}

	// B2, at the joiner: its fresh key not a key; everything after it a tag that fails (a changed ciphertext gives another secret, as FIPS
	// 203's implicit rejection says, and the seal after it fails)
	{
		size_t wrong = 0;

		for (size_t i = 0; i < sizeof(run.b2); i++)
		{
			uint8_t b2[kNetHandshakeBB2Bytes];
			memcpy(b2, run.b2, sizeof(b2));
			b2[i] ^= static_cast<uint8_t>(1u << (i % 8));
			NetHandshakeBJoiner joiner;
			int const expected = i < kNetCryptoP384PublicBytes ? kNetHandshakeBNotAKey : kNetHandshakeBBadTag;
			bool const refused = JoinerAtB2(&joiner) && !joiner.ReadB2(b2, sizeof(b2)) && joiner.Why() == expected;

			if (!refused && wrong++ == 0)
			{
				BCheck(false, "B2 with byte " + std::to_string(i) + " changed is refused as " + ReasonName(expected) + ", not " + ReasonName(joiner.Why()));
			}
		}

		BCheck(wrong == 0, "B2: all " + std::to_string(sizeof(run.b2)) + " refused, the fresh key's bytes as not a key, the rest at a tag (8)");
	}

	// B3, at the host: a tag that fails, everywhere
	{
		size_t wrong = 0;

		for (size_t i = 0; i < sizeof(run.b3); i++)
		{
			uint8_t b3[kNetHandshakeBB3Bytes];
			memcpy(b3, run.b3, sizeof(b3));
			b3[i] ^= static_cast<uint8_t>(1u << (i % 8));
			NetHandshakeBHost host;
			bool const refused = HostAtB3(&host, run) && !host.ReadB3(b3, sizeof(b3)) && host.Why() == kNetHandshakeBBadTag;

			if (!refused && wrong++ == 0)
			{
				BCheck(false, "B3 with byte " + std::to_string(i) + " changed is refused at a tag, not as " + ReasonName(host.Why()));
			}
		}

		BCheck(wrong == 0, "B3: all " + std::to_string(sizeof(run.b3)) + " refused at a tag (8)");
	}

	// B4, at the joiner
	{
		size_t wrong = 0;

		for (size_t i = 0; i < sizeof(run.b4); i++)
		{
			uint8_t b4[kNetHandshakeBB4Bytes];
			memcpy(b4, run.b4, sizeof(b4));
			b4[i] ^= static_cast<uint8_t>(1u << (i % 8));
			NetHandshakeBJoiner joiner;
			uint8_t b5[kNetHandshakeBB5Bytes];
			NetHandshakeBKeys keys;
			bool const refused = JoinerAtB4(&joiner, run) && !joiner.ReadB4(b4, sizeof(b4), b5, &keys) && joiner.Why() == kNetHandshakeBBadTag;

			if (!refused && wrong++ == 0)
			{
				BCheck(false, "B4 with byte " + std::to_string(i) + " changed is refused at a tag, not as " + ReasonName(joiner.Why()));
			}
		}

		BCheck(wrong == 0, "B4: all " + std::to_string(sizeof(run.b4)) + " refused at a tag (8): the joiner sends no B5");
	}

	// B5, at the host
	{
		size_t wrong = 0;

		for (size_t i = 0; i < sizeof(run.b5); i++)
		{
			uint8_t b5[kNetHandshakeBB5Bytes];
			memcpy(b5, run.b5, sizeof(b5));
			b5[i] ^= static_cast<uint8_t>(1u << (i % 8));
			NetHandshakeBHost host;
			NetHandshakeBKeys keys;
			bool const refused = HostAtB5(&host, run) && !host.ReadB5(b5, sizeof(b5), &keys) && host.Why() == kNetHandshakeBBadTag;

			if (!refused && wrong++ == 0)
			{
				BCheck(false, "B5 with byte " + std::to_string(i) + " changed is refused at a tag, not as " + ReasonName(host.Why()));
			}
		}

		BCheck(wrong == 0, "B5: all " + std::to_string(sizeof(run.b5)) + " refused at a tag (8)");
	}
}

// ---- 6. Fresh keys and real randomness, as the game draws them

// One whole handshake B between two fresh identities, drawing as the game does. False if any step refused.
static bool FreshRun(const NetIdentity *host, const NetIdentity *joinerIdentity, NetHandshakeBKeys *joinerKeys, NetHandshakeBKeys *hostKeys)
{
	NetHandshakeBJoiner joiner;
	NetHandshakeBHost h;
	static uint8_t b1[kNetHandshakeBB1Bytes], b2[kNetHandshakeBB2Bytes], b3[kNetHandshakeBB3Bytes], b4[kNetHandshakeBB4Bytes], b5[kNetHandshakeBB5Bytes];
	uint8_t ePublic[kNetCryptoP384PublicBytes], ekPublic[kNetPqKemPublicBytes], ekPrivate[kNetPqKemPrivateBytes], nJ[32], nH[32];
	uint8_t ct[kNetPqKemCiphertextBytes], ss[kNetPqKemSecretBytes];

	// The joiner's draws, in 5.1 point 14's order: e's P-384 key, e's ML-KEM key, nJ
	NetEcdhKey *const e = NetCrypto_EcdhNew(ePublic);
	bool ok = e != NULL && NetPq_KemKeypair(ekPublic, ekPrivate) && NetCrypto_Random(nJ, sizeof(nJ))
		&& joiner.WriteB1("newyork", 40123, host->fingerprint, joinerIdentity, e, ePublic, ekPublic, ekPrivate, nJ, b1);
	NetCrypto_Wipe(ekPrivate, sizeof(ekPrivate));
	NetCrypto_Wipe(nJ, sizeof(nJ));

	// The host's: e's P-384 key, then its encapsulation to the joiner's fresh key
	ok = ok && h.ReadB1("newyork", 40123, host, b1, sizeof(b1));
	NetEcdhKey *const re = ok ? NetCrypto_EcdhNew(ePublic) : NULL;
	ok = ok && re != NULL && NetPq_KemEncapsulate(h.JoinerEphemeralKemPublic(), ct, ss) && h.WriteB2(re, ePublic, ct, ss, b2);

	if (!ok && re)
	{
		NetCrypto_EcdhFree(re);   // WriteB2 takes it only once it's called
	}

	// The joiner's: its encapsulation to the host's own key
	ok = ok && joiner.ReadB2(b2, sizeof(b2)) && NetPq_KemEncapsulate(joiner.HostKemPublic(), ct, ss) && joiner.WriteB3(ct, ss, b3);

	// The host's: its encapsulation to the joiner's own key, then nH
	ok = ok && h.ReadB3(b3, sizeof(b3)) && NetPq_KemEncapsulate(h.JoinerKemPublic(), ct, ss) && NetCrypto_Random(nH, sizeof(nH))
		&& h.WriteB4(ct, ss, nH, b4);
	ok = ok && joiner.ReadB4(b4, sizeof(b4), b5, joinerKeys) && h.ReadB5(b5, sizeof(b5), hostKeys);
	NetCrypto_Wipe(ss, sizeof(ss));
	NetCrypto_Wipe(nH, sizeof(nH));
	return ok;
}

static void TestFresh()
{
	printf("\n6. Fresh identities and real randomness\n");
	NetIdentity host, joiner;
	BCheck(NetIdentity_Make(&host) && NetIdentity_Make(&joiner), "two identities are made, as the game makes one each run");
	BCheck(memcmp(host.fingerprint, joiner.fingerprint, sizeof(host.fingerprint)) != 0 && memcmp(host.statics, joiner.statics, sizeof(host.statics)) != 0,
		"and they differ");

	NetHandshakeBKeys j1, h1, j2, h2;
	bool const first = FreshRun(&host, &joiner, &j1, &h1);
	bool const second = FreshRun(&host, &joiner, &j2, &h2);
	BCheck(first && second, "two whole handshakes B between them, drawing as the game does, both finish");
	BCheck(memcmp(&j1.split, &h1.split, sizeof(j1.split)) == 0 && memcmp(j1.transcript, h1.transcript, sizeof(j1.transcript)) == 0
		&& memcmp(&j2.split, &h2.split, sizeof(j2.split)) == 0 && memcmp(j2.transcript, h2.transcript, sizeof(j2.transcript)) == 0,
		"in each, the joiner and the host have the same keys and transcript");

	char code1[kNetHandshakeBCheckCodeBytes] = {}, code1h[kNetHandshakeBCheckCodeBytes] = {};
	bool const made = NetHandshakeB_CheckCode(j1.transcript, code1) && NetHandshakeB_CheckCode(h1.transcript, code1h);
	BCheck(made && std::string(code1) == code1h && strlen(code1) == 6, std::string("and both screens show the same check code: ") + code1);
	BCheck(memcmp(&j1.split, &j2.split, sizeof(j1.split)) != 0 && memcmp(j1.transcript, j2.transcript, sizeof(j1.transcript)) != 0,
		"the second handshake's keys and transcript aren't the first's: every handshake is fresh");

	NetHandshakeB_WipeKeys(&j1);
	NetHandshakeB_WipeKeys(&h1);
	NetHandshakeB_WipeKeys(&j2);
	NetHandshakeB_WipeKeys(&h2);
	NetIdentity_Free(&host);
	NetIdentity_Free(&joiner);
	NetIdentity none;
	memset(&none, 0, sizeof(none));
	BCheck(memcmp(&host, &none, sizeof(host)) == 0, "a freed identity is all 0, its CNG key destroyed");
}

int main()
{
	printf("Handshake B of protocol version 2, the game's side (net_handshake_b.h, net_identity.h)\n");
	bool const read = ReadTheFile();
	BCheck(read, "tests/vectors/crypto-v2.json is read, and has a handshakeB");
	BCheck(NetPq_Available(), "this test has the real post-quantum half (net_pq_native.cpp)");

	if (read)
	{
		static Run run;
		TestIdentities();

		if (hostId.made && joinerId.made)
		{
			TestSteps();
			TestTheGamesHandshake(&run);
			TestCheckCodeForm();
			TestDatagrams(run);
			TestWrongSizesAndTurns(run);
			TestWrongHost(run);
			TestBrokenPromise(run);
			TestEveryByte(run);
		}

		TestFresh();
		NetIdentity_Free(&hostId);
		NetIdentity_Free(&joinerId);
	}

	printf("\n%s: %d check(s) failed\n", bChecksFailed == 0 ? "PASSED" : "FAILED", bChecksFailed);
	return bChecksFailed == 0 ? 0 : 1;
}
