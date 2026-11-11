// Tests of handshake B of protocol version 2 (3dc/net_handshake_b.h) and a player's identity (3dc/net_identity.h), the game's side.
// Written 2026-11-09 by Melanikus for the AvP port.
//
// WHAT'S CHECKED, and against what. The relay's repository has a reference handshake B written apart from this one, in Java
// (AVPx-Dedicated-Server, java/test/avpx/v2/HandshakeB.java), and its test vectors (tests/vectors/crypto-v2.json, copied byte for byte;
// the gate checks its sha256) hold one whole handshake B made by it from fixed keys and fixed randomness. So:
//   1. the two identities in the file: their ML-KEM public keys from their seeds, their fingerprints and their whole ids (item 6); and the
//      hex of each, both ways;
//   2. the file's 23 steps, walked one by one with net_noise.h here in the test: every h and ck, every secret and key mixed, every
//      sealed piece. That checks the test's own reading of CRYPTO.md before it judges the game's code by it;
//   3. the game's NetHandshakeBJoiner and NetHandshakeBHost, given the file's keys and randomness, make B1 to B5 byte for byte, the same
//      keys both ways, the same transcript, and the check code 078265;
//   4. the file's 11 datagrams: each message cut by net_v2_fragments.h under 'AVPE' is exactly the file's, and reads back whole;
//   5. hostile messages, each refused for its own reason (CRYPTO.md section 16's number where there is one):
//      - a wrong size, a call out of turn, a bad start;
//      - B2 made with another host's keys under the listing's id (10), and B5 revealing another nJ (21), both with good
//        tags, made from the test's own walk;
//      - another region, port or host id in the prologue (8);
//      - one bit changed in every byte of B1 to B5;
//   6. two handshakes with fresh keys and real randomness, from the game's own NetIdentity_Make: they agree, and differ from each other;
//   7. sealed game datagrams (3dc/net_sealed_game.h, CRYPTO.md section 9) with the file's handshake B keys: each one exactly what the test
//      seals by hand from section 9's layout; both directions, and never one's own back; the smallest and the largest; epoch 1 from
//      counter 2^20; the replay window's edges; one bit changed in every byte; the 65,536th failed tag ending the session; 24 hours;
//      20,000 junk datagrams;
//   7b. the relay's own sealed game datagrams (the file's sealedGame, from the relay's Java; modified 2026-11-09 by Melanikus for the AvP
//      port): each is exactly what the test seals by hand, the other side opens it, and a new side seals the same at counters 0 and 1;
//   8. a joiner and a host (3dc/net_peer_v2.h) over a pretend wire that loses what each test says: the whole join, then game datagrams
//      both ways; the joiner's sends at exactly 0, 0.5, 1.5 and 3.5 s and its giving up at 7.5 s; B2, B4 and B5 lost once each, and the
//      host's answers sent again byte for byte; a joiner that expected another host; another handshake's datagrams, a cookie that isn't
//      0 and sealed datagrams before the keys, all dropped without harm; the host giving up on a joiner that goes quiet.
//
// The test's fixed randomness reaches mlkem-native's own "_derand" calls (keys from a seed, an encapsulation from its m) here alone: the
// game's code never has a way to be given fixed ML-KEM randomness (net_pq_native.cpp).

#include "net_handshake_b.h"
#include "net_identity.h"
#include "net_peer_v2.h"
#include "net_sealed_game.h"
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
		|| !NetIdentity_Fingerprint(NetIdentity_P384Public(out), NetIdentity_KemPublic(out), out->fingerprint)
		|| !NetIdentity_Id(NetIdentity_P384Public(out), NetIdentity_KemPublic(out), out->id))
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
	printf("\n1. The two identities in the file, their fingerprints and their whole ids\n");

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

		// Modified 2026-11-10 by Melanikus for the AvP port: the game no longer writes a fingerprint as hex (a listing names its host by the
		// whole id, SERVER-BLOCKERS item 6), so its writer went, and the file's text is read the one way the game reads one. That reader
		// takes exactly 32 small hex digits and nothing else, so this also proves the file writes them the way the list does.
		const JsonValue *const written = TextMember(*b->Member(name), "fingerprint");
		uint8_t back[kNetIdentityFingerprintBytes];
		BCheck(written && NetIdentity_FingerprintFromHex(written->text.c_str(), back) && memcmp(back, id->fingerprint, sizeof(back)) == 0,
			std::string("and the file's 32 small hex digits for it, read the game's one way, are the same 16 bytes"));

		// Added 2026-11-10 by Melanikus for the AvP port (SERVER-BLOCKERS item 6): the whole id, which the listing carries and B's prologue
		// mixes, is the file's, and the fingerprint is its first 16 bytes
		BCheck(Equal(BHex(name, "id"), id->id, kNetIdentityIdBytes),
			std::string("its whole id (SHA-384 of \"AVPX-ID-2\" and both public keys, all 48 bytes) is the file's"));
		BCheck(memcmp(id->id, id->fingerprint, kNetIdentityFingerprintBytes) == 0, "and its fingerprint is the id's first 16 bytes");

		char idHex[kNetIdentityIdHexBytes];
		NetIdentity_IdHex(id->id, idHex);
		const JsonValue *const writtenId = TextMember(*b->Member(name), "id");
		BCheck(writtenId && writtenId->text == idHex, std::string("and as 96 small hex digits the id is written exactly as the file writes it"));

		uint8_t idBack[kNetIdentityIdBytes];
		BCheck(NetIdentity_IdFromHex(idHex, idBack) && memcmp(idBack, id->id, sizeof(idBack)) == 0, "and reads back to the same 48 bytes");
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

	// The same for a listing's whole id: only exactly 96 small hex digits, with nothing after them (added 2026-11-10, item 6)
	std::string const good = "11e6e2dfda44451c8e4328b8a7cd50ca9591aaeac21b0cebf78a1c8f482b280d02677182138983bb42df6aa7513139ef";
	std::string capitals = good;
	capitals[0] = 'A';
	capitals[1] = 'B';
	std::string const refusedIds[] = {
		capitals,                                  // capitals: the list never writes them
		good.substr(0, 95),                        // 95 digits
		good + "0",                                // 97
		good.substr(0, 95) + "g",                  // not a hex digit
		good.substr(0, 95) + " ",                  // a space
		good.substr(0, 32),                        // a fingerprint where the id goes
		"",
	};
	uint8_t idOut[kNetIdentityIdBytes];

	for (size_t i = 0; i < sizeof(refusedIds) / sizeof(refusedIds[0]); i++)
	{
		memset(idOut, 0xAA, sizeof(idOut));
		bool const taken = NetIdentity_IdFromHex(refusedIds[i].c_str(), idOut);
		BCheck(!taken && Equal(Bytes(sizeof(idOut), 0), idOut, sizeof(idOut)),
			std::string("a listing's id of ") + std::to_string(refusedIds[i].size()) + " letters, not all small hex, or a fingerprint, is refused, and nothing of it kept");
	}

	BCheck(NetIdentity_IdFromHex(good.c_str(), idOut), "while the host's own 96 digits are taken");
	BCheck(!NetIdentity_IdFromHex(NULL, idOut), "and no id at all is refused");
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
	Hash(&noise, hostId.id, sizeof(hostId.id));   // the host's whole id since item 6 (2026-11-10)

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

// The joiner's WriteB1 with the file's draws, for the expected host's whole id, region and port given
static bool FileB1(NetHandshakeBJoiner *joiner, const uint8_t expected[kNetIdentityIdBytes], const char *region, uint16_t port, uint8_t b1Out[kNetHandshakeBB1Bytes])
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

	BCheck(FileB1(&joiner, hostId.id, region.c_str(), port, run->b1) && Equal(BHex("b1"), run->b1, sizeof(run->b1)),
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

	// Modified 2026-11-09 by Melanikus for the AvP port (CRYPTO.md 5.1 point 3, the relay's file at 2b8dcd6): a 48-byte secret each way
	NetNoiseSplit expected;
	Bytes const s1 = BHex("split", "secretJoinerToHost"), s2 = BHex("split", "secretHostToJoiner");
	Bytes const iv1 = BHex("split", "ivJoinerToHost"), iv2 = BHex("split", "ivHostToJoiner");
	bool const sized = s1.size() == kNetNoiseSecretBytes && s2.size() == kNetNoiseSecretBytes && iv1.size() == 12 && iv2.size() == 12;

	if (sized)
	{
		memcpy(expected.initiatorToResponderSecret, s1.data(), kNetNoiseSecretBytes);
		memcpy(expected.responderToInitiatorSecret, s2.data(), kNetNoiseSecretBytes);
		memcpy(expected.initiatorToResponderIv, iv1.data(), 12);
		memcpy(expected.responderToInitiatorIv, iv2.data(), 12);
	}

	BCheck(sized && SameSplit(expected, run->joinerKeys.split) && SameSplit(expected, run->hostKeys.split),
		"both sides have the file's secrets (48 bytes each) and IVs, joiner-to-host as Split's \"initiator to responder\"");
	BCheck(Equal(BHex("transcript"), run->joinerKeys.transcript, kNetCryptoHashBytes) && Equal(BHex("transcript"), run->hostKeys.transcript, kNetCryptoHashBytes),
		"both have the file's transcript, h after B5");
	BCheck(memcmp(run->joinerKeys.theirFingerprint, hostId.fingerprint, kNetIdentityFingerprintBytes) == 0
		&& memcmp(run->hostKeys.theirFingerprint, joinerId.fingerprint, kNetIdentityFingerprintBytes) == 0,
		"the joiner knows the host's fingerprint, and the host the joiner's");

	// The code is made first, on its own line: in one call, the words could be put together before it was made
	char code[kNetHandshakeBCheckCodeBytes] = {};
	const JsonValue *const fileCode = TextMember(*b, "checkCode");
	bool const made = NetHandshakeB_CheckCode(run->joinerKeys.transcript, code);
	// Modified 2026-11-10 by Melanikus for the AvP port: 078265, since B's prologue mixes the host's whole id (SERVER-BLOCKERS item 6,
	// 14fc5ddb), which changed the transcript and so the code. The file (the server's 2694c87 copy) has had it since then; this line was
	// left saying the code before it.
	BCheck(made && fileCode && fileCode->text == code && std::string(code) == "078265", std::string("the check code is the file's, 078265: ") + code);

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
	return FileB1(joiner, hostId.id, Region().c_str(), RelayPort(), b1);
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
		BCheck(!FileB1(&joiner, hostId.id, regions[i], RelayPort(), b1) && joiner.Why() == kNetHandshakeBBadStart,
			std::string("the region \"") + regions[i] + "\" is a bad start");
		NetHandshakeBHost host;
		BCheck(!host.ReadB1(regions[i], RelayPort(), &hostId, run.b1, sizeof(run.b1)) && host.Why() == kNetHandshakeBBadStart,
			"for the host too");
	}

	{
		NetHandshakeBJoiner joiner;
		BCheck(!FileB1(&joiner, hostId.id, Region().c_str(), 0, b1) && joiner.Why() == kNetHandshakeBBadStart
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
		BCheck(FileB1(&joiner, hostId.id, "abcdefghijklmn-0", RelayPort(), b1), "a region of 16 small letters, digits and - is taken");
	}
}

// B2 made from the walk with another identity's keys, under the listing's id: the man in the middle that the id check is there for. Its
// tags are good, because the walk has the joiner's own state; only the id can tell.
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

	// The listing named another host (here, the joiner's own id): the prologues differ, so B2's first seal fails at the joiner
	{
		NetHandshakeBJoiner other;
		uint8_t b1[kNetHandshakeBB1Bytes];
		BCheck(FileB1(&other, joinerId.id, Region().c_str(), RelayPort(), b1) && !other.ReadB2(run.b2, sizeof(run.b2))
			&& other.Why() == kNetHandshakeBBadTag, "a joiner that expected another host's id refuses the real host's B2 at its tag (8)");
	}

	// Another region, another port: the same
	{
		NetHandshakeBJoiner other;
		uint8_t b1[kNetHandshakeBB1Bytes];
		BCheck(FileB1(&other, hostId.id, "paris", RelayPort(), b1) && !other.ReadB2(run.b2, sizeof(run.b2)) && other.Why() == kNetHandshakeBBadTag,
			"a joiner on another region's relay refuses it at its tag (8)");
	}
	{
		NetHandshakeBJoiner other;
		uint8_t b1[kNetHandshakeBB1Bytes];
		BCheck(FileB1(&other, hostId.id, Region().c_str(), static_cast<uint16_t>(RelayPort() + 1), b1) && !other.ReadB2(run.b2, sizeof(run.b2))
			&& other.Why() == kNetHandshakeBBadTag, "and so does one on another relay port (8)");
	}

	// And the host's side of the same: a B1 meant for another host's id gets a B2 its joiner can't take
	{
		NetHandshakeBJoiner other;
		NetHandshakeBHost host;
		uint8_t b1[kNetHandshakeBB1Bytes], b2Other[kNetHandshakeBB2Bytes];
		BCheck(FileB1(&other, joinerId.id, Region().c_str(), RelayPort(), b1) && FileB2(&host, Region().c_str(), RelayPort(), b1, sizeof(b1), b2Other)
			&& !other.ReadB2(b2Other, sizeof(b2Other)) && other.Why() == kNetHandshakeBBadTag,
			"a host answering a B1 meant for another host's id makes a B2 that joiner refuses (8)");
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
		&& joiner.WriteB1("newyork", 40123, host->id, joinerIdentity, e, ePublic, ekPublic, ekPrivate, nJ, b1);
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

// ---- 7. Sealed game datagrams

// A datagram sealed by hand from section 9's table, with the direction's Split secret and IV: 'AVPG', the counter little-endian, the
// text sealed with the 12 bytes in front as the associated data, then the tag. The epoch's key (from the secret, epoch 0's too) and the
// nonce come straight from net_noise.h's NetNoise_EpochKey and NetNoise_Nonce, not from the class under test.
static Bytes SealByHand(const uint8_t secret[kNetNoiseSecretBytes], const uint8_t iv[kNetCryptoNonceBytes], uint64_t counter, const Bytes &text)
{
	Bytes out(kNetSealedGameOverheadBytes + text.size(), 0);
	memcpy(out.data(), "AVPG", 4);

	for (size_t i = 0; i < 8; i++)
	{
		out[4 + i] = static_cast<uint8_t>(counter >> (8 * i));
	}

	uint8_t epochKey[kNetCryptoAesKeyBytes], nonce[kNetCryptoNonceBytes];
	static const uint8_t kNothing[1] = { 0 };
	bool const sealed = NetNoise_EpochKey(secret, static_cast<uint32_t>(counter >> 20), epochKey) && (NetNoise_Nonce(iv, counter, nonce), true)
		&& NetCrypto_AesGcmSeal(epochKey, nonce, out.data(), 12, text.empty() ? kNothing : text.data(), text.size(), out.data() + 12,
			out.data() + 12 + text.size());
	return sealed ? out : Bytes();
}

static Bytes Pattern(size_t size, uint8_t start)
{
	Bytes out(size);

	for (size_t i = 0; i < size; i++)
	{
		out[i] = static_cast<uint8_t>(start + i * 7);
	}

	return out;
}

static std::string SealedName(int why)
{
	switch (why)
	{
		case kNetSealedGameFine: return "fine";
		case kNetSealedGameWrongSize: return "the wrong size (1)";
		case kNetSealedGameOtherMagic: return "another magic (2)";
		case kNetSealedGameBadTag: return "a tag that fails (8)";
		case kNetSealedGameSeenAlready: return "seen already (9)";
		case kNetSealedGameTooManyBad: return "too many bad packets (16)";
		case kNetSealedGameWornOut: return "worn out";
		case kNetSealedGameNotLive: return "no session";
		case kNetSealedGamePastTheEnd: return "past the session's end";
		default: return "reason " + std::to_string(why);
	}
}

// Opens bytes and says whether they were taken, and as what
static bool Opens(NetSealedGame *side, const Bytes &sealed, Bytes *text, uint32_t now)
{
	text->assign(sealed.size() >= kNetSealedGameOverheadBytes ? sealed.size() - kNetSealedGameOverheadBytes : 0, 0xAA);
	size_t written = 0;
	uint8_t spare[1];
	bool const taken = side->Open(sealed.empty() ? spare : sealed.data(), sealed.size(), text->empty() ? spare : text->data(), text->size(), &written, now);
	text->resize(written);
	return taken;
}

static void TestSealedGame(const Run &run)
{
	printf("\n7. Sealed game datagrams (section 9), with the file's handshake B keys\n");
	const NetNoiseSplit &split = run.joinerKeys.split;
	uint32_t const now = 1000;
	NetSealedGame joiner, host;
	joiner.Start(split, true, now);
	host.Start(split, false, now);
	BCheck(joiner.Live() && host.Live(), "both sides start a session from B's keys");

	// Each one exactly as sealed by hand, both ways, and opened by the other side
	bool same = true;
	bool openedAll = true;
	size_t const sizes[] = { 0, 1, 48, 1392, kNetSealedGameMostTextBytes };

	for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
	{
		for (int way = 0; way < 2; way++)
		{
			NetSealedGame &from = way == 0 ? joiner : host;
			NetSealedGame &to = way == 0 ? host : joiner;
			Bytes const text = Pattern(sizes[i], static_cast<uint8_t>(i + way));
			Bytes sealed(text.size() + kNetSealedGameOverheadBytes);
			size_t written = 0;
			uint64_t const counter = from.Sent();
			bool const made = from.Seal(text.empty() ? NULL : text.data(), text.size(), sealed.data(), sealed.size(), &written, now) && written == sealed.size();
			Bytes const byHand = way == 0 ? SealByHand(split.initiatorToResponderSecret, split.initiatorToResponderIv, counter, text)
				: SealByHand(split.responderToInitiatorSecret, split.responderToInitiatorIv, counter, text);
			same = same && made && sealed == byHand;
			Bytes back;
			openedAll = openedAll && Opens(&to, sealed, &back, now) && back == text;
		}
	}

	BCheck(same, "joiner to host and host to joiner, 0, 1, 48, 1,392 and 1,472 bytes: each datagram is exactly section 9's layout, sealed by hand");
	BCheck(openedAll, "and each opens on the other side to exactly what was sent, the 28-byte and the 1,500-byte included");

	// Never one's own back: a datagram the joiner sealed doesn't open as if the host had sent it
	{
		Bytes sealed(48 + kNetSealedGameOverheadBytes);
		size_t written = 0;
		Bytes const text = Pattern(48, 9);
		Bytes back;
		BCheck(joiner.Seal(text.data(), text.size(), sealed.data(), sealed.size(), &written, now) && !Opens(&joiner, sealed, &back, now)
			&& joiner.Why() == kNetSealedGameBadTag, "a datagram sent back to the side that sealed it fails its tag (8): each direction has its own key");
		BCheck(Opens(&host, sealed, &back, now) && back == text, "and the host still takes it");
		BCheck(!Opens(&host, sealed, &back, now) && host.Why() == kNetSealedGameSeenAlready, "but only once: the same datagram again is seen already (9)");
	}

	// Sizes
	{
		Bytes back;
		Bytes const short27(27, 0);
		Bytes const long1501(1501, 0);
		BCheck(!Opens(&host, short27, &back, now) && host.Why() == kNetSealedGameWrongSize, "27 bytes is the wrong size (1)");
		BCheck(!Opens(&host, long1501, &back, now) && host.Why() == kNetSealedGameWrongSize, "1,501 bytes is the wrong size (1)");
		Bytes const big = Pattern(kNetSealedGameMostTextBytes + 1, 1);
		Bytes room(big.size() + kNetSealedGameOverheadBytes, 0xAA);
		size_t written = 0;
		BCheck(!joiner.Seal(big.data(), big.size(), room.data(), room.size(), &written, now) && joiner.Why() == kNetSealedGameWrongSize
			&& written == 0 && room == Bytes(room.size(), 0), "a 1,473-byte datagram isn't sealed (it would be over 1,500), and nothing of it is left");
		Bytes const text = Pattern(10, 1);
		Bytes tight(text.size() + kNetSealedGameOverheadBytes - 1, 0xAA);
		BCheck(!joiner.Seal(text.data(), text.size(), tight.data(), tight.size(), &written, now) && joiner.Why() == kNetSealedGameWrongSize,
			"and no datagram is sealed into too little room");
	}

	// The counters past 2^20: epoch 1's key, from the counter alone. Made by hand, since sealing 2^20 datagrams would take too long here.
	{
		NetSealedGame fresh;
		fresh.Start(split, false, now);
		Bytes back;
		Bytes const text = Pattern(64, 3);
		uint64_t const last0 = (static_cast<uint64_t>(1) << 20) - 1;
		Bytes const inEpoch0 = SealByHand(split.initiatorToResponderSecret, split.initiatorToResponderIv, last0, text);
		Bytes const inEpoch1 = SealByHand(split.initiatorToResponderSecret, split.initiatorToResponderIv, last0 + 1, text);
		BCheck(Opens(&fresh, inEpoch0, &back, now) && back == text, "the last datagram of epoch 0 (counter 2^20 - 1) opens with epoch 0's key");
		BCheck(Opens(&fresh, inEpoch1, &back, now) && back == text, "the first of epoch 1 (counter 2^20) opens with epoch 1's key");

		// The same counter sealed with epoch 0's key doesn't: the key really changed. Modified 2026-11-09 by Melanikus for the AvP port:
		// epoch 0's key is made from the secret (CRYPTO.md 5.1 point 3), so it's made here too
		Bytes wrongKey = inEpoch1;
		uint8_t nonce[kNetCryptoNonceBytes], epoch0Key[kNetCryptoAesKeyBytes];
		NetNoise_Nonce(split.initiatorToResponderIv, last0 + 2, nonce);
		wrongKey[4] = static_cast<uint8_t>((last0 + 2) & 0xFF);
		wrongKey[5] = static_cast<uint8_t>(((last0 + 2) >> 8) & 0xFF);
		wrongKey[6] = static_cast<uint8_t>(((last0 + 2) >> 16) & 0xFF);
		BCheck(NetNoise_EpochKey(split.initiatorToResponderSecret, 0, epoch0Key)
			&& NetCrypto_AesGcmSeal(epoch0Key, nonce, wrongKey.data(), 12, text.data(), text.size(), wrongKey.data() + 12,
			wrongKey.data() + 12 + text.size()) && !Opens(&fresh, wrongKey, &back, now) && fresh.Why() == kNetSealedGameBadTag,
			"counter 2^20 + 1 sealed with epoch 0's key fails its tag (8)");
		NetCrypto_Wipe(epoch0Key, sizeof(epoch0Key));

		Bytes const past = SealByHand(split.initiatorToResponderSecret, split.initiatorToResponderIv, static_cast<uint64_t>(1) << 32, text);
		BCheck(!Opens(&fresh, past, &back, now) && fresh.Why() == kNetSealedGamePastTheEnd, "counter 2^32 is past a session's end, and refused");
	}

	// Added 2026-11-09 by Melanikus for the AvP port (CRYPTO.md 5.1 point 3): nothing seals with the secret itself. A datagram sealed with
	// the secret's own first 32 bytes, as epoch 0 was before, fails its tag; and it doesn't use its counter up, so the honest one opens.
	{
		NetSealedGame fresh;
		fresh.Start(split, false, now);
		Bytes back;
		Bytes const text = Pattern(16, 9);
		Bytes asBefore(kNetSealedGameOverheadBytes + text.size(), 0);
		memcpy(asBefore.data(), "AVPG", 4);
		uint8_t nonce[kNetCryptoNonceBytes];
		NetNoise_Nonce(split.initiatorToResponderIv, 0, nonce);
		BCheck(NetCrypto_AesGcmSeal(split.initiatorToResponderSecret, nonce, asBefore.data(), 12, text.data(), text.size(), asBefore.data() + 12,
			asBefore.data() + 12 + text.size()) && !Opens(&fresh, asBefore, &back, now) && fresh.Why() == kNetSealedGameBadTag,
			"a datagram sealed with the secret itself, as epoch 0 was before, fails its tag (8): the secret never seals");
		BCheck(Opens(&fresh, SealByHand(split.initiatorToResponderSecret, split.initiatorToResponderIv, 0, text), &back, now) && back == text,
			"and the honest counter 0, sealed with epoch 0's own key, still opens");
	}

	// The replay window: the highest taken, and the 1,023 below it, never more
	{
		NetSealedGame fresh;
		fresh.Start(split, false, now);
		Bytes back;
		Bytes const text = Pattern(8, 5);
		const uint8_t *const key = split.initiatorToResponderSecret;
		const uint8_t *const iv = split.initiatorToResponderIv;
		BCheck(Opens(&fresh, SealByHand(key, iv, 5000, text), &back, now), "counter 5,000 is taken first: datagrams may come in any order");
		BCheck(Opens(&fresh, SealByHand(key, iv, 5000 - 1023, text), &back, now), "then 3,977, the oldest still in the window");
		BCheck(!Opens(&fresh, SealByHand(key, iv, 5000 - 1024, text), &back, now) && fresh.Why() == kNetSealedGameSeenAlready,
			"but not 3,976, just below it (9)");
		BCheck(!Opens(&fresh, SealByHand(key, iv, 5000 - 1023, text), &back, now) && fresh.Why() == kNetSealedGameSeenAlready,
			"and 3,977 only once (9)");
		BCheck(Opens(&fresh, SealByHand(key, iv, 4999, text), &back, now), "4,999, late, is still new");

		// A forged datagram with an honest counter must not use the counter up
		Bytes forged = SealByHand(key, iv, 4998, text);
		forged[20] ^= 0x01;
		BCheck(!Opens(&fresh, forged, &back, now) && fresh.Why() == kNetSealedGameBadTag, "a forgery of 4,998 fails its tag (8)");
		BCheck(Opens(&fresh, SealByHand(key, iv, 4998, text), &back, now), "and the honest 4,998 is still taken after it: only a good tag marks a counter");
	}

	// One bit changed in every byte of a sealed datagram
	{
		Bytes const text = Pattern(100, 7);
		Bytes const good = SealByHand(split.initiatorToResponderSecret, split.initiatorToResponderIv, 77, text);
		size_t wrong = 0;

		for (size_t i = 0; i < good.size(); i++)
		{
			NetSealedGame fresh;
			fresh.Start(split, false, now);
			Bytes changed = good;
			changed[i] ^= static_cast<uint8_t>(1u << (i % 8));
			Bytes back;
			// The magic: another magic. The counter's high 4 bytes: a counter of 2^32 or more, past any session's end. Everything else,
			// the counter's low bytes included (another counter, so another nonce and another AAD): the tag.
			int const expected = i < 4 ? kNetSealedGameOtherMagic : (i >= 8 && i < 12) ? kNetSealedGamePastTheEnd : kNetSealedGameBadTag;
			bool const refused = !Opens(&fresh, changed, &back, now) && fresh.Why() == expected && back.empty();

			if (!refused && wrong++ == 0)
			{
				BCheck(false, "a sealed datagram with byte " + std::to_string(i) + " changed is refused as " + SealedName(expected) + ", not "
					+ SealedName(fresh.Why()));
			}
		}

		BCheck(wrong == 0, "every one of a datagram's " + std::to_string(good.size()) + " bytes changed is refused: the magic's as another magic (2), "
			"the counter's high 4 as past a session's end, the rest at the tag (8)");
	}

	// The 65,536th failed tag ends the session
	{
		NetSealedGame fresh;
		fresh.Start(split, false, now);
		Bytes forged = SealByHand(split.initiatorToResponderSecret, split.initiatorToResponderIv, 1, Pattern(4, 1));
		forged[forged.size() - 1] ^= 0x01;
		Bytes back;
		bool allBad = true;

		for (uint32_t i = 0; i + 1 < kNetSealedGameMostBadTags; i++)
		{
			allBad = allBad && !Opens(&fresh, forged, &back, now) && fresh.Why() == kNetSealedGameBadTag;
		}

		BCheck(allBad && fresh.Live() && fresh.BadTags() == kNetSealedGameMostBadTags - 1, "65,535 forgeries are refused (8), and the session goes on");
		BCheck(!Opens(&fresh, forged, &back, now) && !fresh.Live() && fresh.Ended() == kNetSealedGameTooManyBad,
			"the 65,536th ends it: too many bad packets (16)");
		Bytes const honest = SealByHand(split.initiatorToResponderSecret, split.initiatorToResponderIv, 2, Pattern(4, 1));
		BCheck(!Opens(&fresh, honest, &back, now) && fresh.Why() == kNetSealedGameNotLive, "and then even an honest datagram is refused: no session");
		Bytes room(4 + kNetSealedGameOverheadBytes);
		size_t written = 0;
		Bytes const text = Pattern(4, 1);
		BCheck(!fresh.Seal(text.data(), text.size(), room.data(), room.size(), &written, now) && fresh.Why() == kNetSealedGameNotLive,
			"and nothing can be sealed: there's no plaintext way out");
	}

	// 24 hours, and a clock that goes round past 2^32 milliseconds
	{
		NetSealedGame fresh;
		uint32_t const start = 0xFFFFF000u;   // 4,096 ms before the clock goes round
		fresh.Start(split, true, start);
		Bytes const text = Pattern(4, 2);
		Bytes room(text.size() + kNetSealedGameOverheadBytes);
		size_t written = 0;
		uint32_t const later = start + kNetSealedGameMostMs - 1;   // goes round: a small number
		BCheck(fresh.Seal(text.data(), text.size(), room.data(), room.size(), &written, later) && fresh.Live(),
			"a session started just before the clock went round still seals 1 ms before its 24 hours");
		BCheck(!fresh.Seal(text.data(), text.size(), room.data(), room.size(), &written, later + 1) && !fresh.Live()
			&& fresh.Ended() == kNetSealedGameWornOut, "and at 24 hours it ends, worn out: a new handshake must start");
	}

	// Before a session and after its end: nothing
	{
		NetSealedGame never;
		Bytes back;
		Bytes const good = SealByHand(split.initiatorToResponderSecret, split.initiatorToResponderIv, 0, Pattern(4, 1));
		BCheck(!Opens(&never, good, &back, now) && never.Why() == kNetSealedGameNotLive, "a side never started opens nothing");
		NetSealedGame ended;
		ended.Start(split, false, now);
		ended.End(kNetSealedGameFine);
		BCheck(!Opens(&ended, good, &back, now) && ended.Why() == kNetSealedGameNotLive, "and nor does one that has ended");
	}

	// Junk
	{
		NetSealedGame fresh;
		fresh.Start(split, false, now);
		uint32_t seed = 12345;
		size_t taken = 0;

		for (int i = 0; i < 20000; i++)
		{
			seed = seed * 1103515245u + 12345u;
			size_t const size = (seed >> 8) % 1601;
			Bytes junk(size);

			for (size_t j = 0; j < size; j++)
			{
				seed = seed * 1103515245u + 12345u;
				junk[j] = static_cast<uint8_t>(seed >> 16);
			}

			// A quarter of them start as a sealed datagram would, with a counter under 2^32, so they get as far as the tag
			if (size >= 12 && i % 4 == 0)
			{
				memcpy(junk.data(), "AVPG", 4);
				memset(junk.data() + 8, 0, 4);
			}

			Bytes back;
			taken += Opens(&fresh, junk, &back, now) ? 1 : 0;
		}

		BCheck(taken == 0, "20,000 junk datagrams of 0 to 1,600 bytes, a quarter starting 'AVPG': none taken, " + std::to_string(fresh.BadTags())
			+ " of them reaching a tag that failed");
	}

	joiner.End(kNetSealedGameFine);
	host.End(kNetSealedGameFine);
}

// Added 2026-11-09 by Melanikus for the AvP port: THE RELAY'S OWN SEALED GAME DATAGRAMS (crypto-v2.json's sealedGame, which the relay
// server's Java makes from the same handshake B's split, CRYPTO.md section 9). Until these came, the bytes this test sealed by hand were
// section 9's only reference here. Now each of the relay's is held to them three ways:
// - it's exactly what this test seals by hand;
// - the other side's NetSealedGame opens it to its message;
// - at counters 0 and 1, a new NetSealedGame seals the same bytes.
// So the game and the relay agree on section 9, not only the game with itself.
static void TestTheRelaysSealedGame(const Run &run)
{
	printf("\n7b. Sealed game datagrams against the relay's own (crypto-v2.json's sealedGame)\n");
	const JsonValue *const cases = file.Member("sealedGame");
	BCheck(cases && cases->kind == kJsonList && cases->items.size() == 6, "the file has the relay's 6 sealed game datagrams");

	if (!cases || cases->kind != kJsonList)
	{
		return;
	}

	const NetNoiseSplit &split = run.joinerKeys.split;
	uint32_t const now = 1000;
	NetSealedGame joinerSeals, hostSeals, hostOpens, joinerOpens;
	joinerSeals.Start(split, true, now);
	hostSeals.Start(split, false, now);
	hostOpens.Start(split, false, now);
	joinerOpens.Start(split, true, now);

	for (size_t i = 0; i < cases->items.size(); i++)
	{
		const JsonValue &c = cases->items[i];
		const JsonValue *const from = TextMember(c, "from");
		const JsonValue *const counter = c.Member("counter");
		const JsonValue *const epoch = c.Member("epoch");
		bool const fromJoiner = from && from->text == "joiner";
		uint64_t const n = counter && counter->kind == kJsonNumber ? counter->number : ~0ull;
		Bytes const message = Hex(c, "message");
		Bytes const sealed = Hex(c, "sealed");
		std::string const which = std::string(fromJoiner ? "the joiner's" : "the host's") + " at counter " + std::to_string(n);
		BCheck(from && (fromJoiner || from->text == "host") && epoch && epoch->kind == kJsonNumber && epoch->number == (n >> 20),
			which + ": its side is named, and its epoch is the counter's (2^20 to an epoch)");

		Bytes const byHand = fromJoiner ? SealByHand(split.initiatorToResponderSecret, split.initiatorToResponderIv, n, message)
			: SealByHand(split.responderToInitiatorSecret, split.responderToInitiatorIv, n, message);
		BCheck(!byHand.empty() && byHand == sealed, which + ": the relay's bytes are exactly what section 9 seals by hand");

		Bytes text;
		BCheck(Opens(fromJoiner ? &hostOpens : &joinerOpens, sealed, &text, now) && text == message, which + ": the other side opens it to its message");

		if (n <= 1)
		{
			NetSealedGame &side = fromJoiner ? joinerSeals : hostSeals;
			Bytes out(message.size() + kNetSealedGameOverheadBytes, 0xAA);
			size_t written = 0;
			uint8_t spare[1] = { 0 };
			bool const made = side.Seal(message.empty() ? spare : message.data(), message.size(), out.data(), out.size(), &written, now)
				&& written == out.size();
			BCheck(made && out == sealed, which + ": and a new NetSealedGame seals the same bytes");
		}
	}

	joinerSeals.End(kNetSealedGameFine);
	hostSeals.End(kNetSealedGameFine);
	hostOpens.End(kNetSealedGameFine);
	joinerOpens.End(kNetSealedGameFine);
}

// ---- 8. A joiner and a host over a pretend wire

static std::vector<Bytes> SendsOf(NetPeerJoiner *joiner)
{
	NetPeerSend sends[kNetPeerMostSends];
	size_t const n = joiner->TakeSends(sends, kNetPeerMostSends);
	std::vector<Bytes> out;

	for (size_t i = 0; i < n; i++)
	{
		out.push_back(Bytes(sends[i].bytes, sends[i].bytes + sends[i].size));
	}

	return out;
}

static std::vector<Bytes> SendsOf(NetPeerHostSide *host)
{
	NetPeerSend sends[kNetPeerMostSends];
	size_t const n = host->TakeSends(sends, kNetPeerMostSends);
	std::vector<Bytes> out;

	for (size_t i = 0; i < n; i++)
	{
		out.push_back(Bytes(sends[i].bytes, sends[i].bytes + sends[i].size));
	}

	return out;
}

// The kind of a handshake datagram (byte 5), or 0
static uint8_t KindOf(const std::vector<Bytes> &datagrams)
{
	return !datagrams.empty() && datagrams[0].size() > 5 ? datagrams[0][5] : 0;
}

static Bytes HearAt(NetPeerHostSide *host, const Bytes &datagram, uint32_t now)
{
	Bytes out(kNetSealedGameMostBytes);
	size_t written = 0;
	host->Hear(datagram.data(), datagram.size(), out.data(), out.size(), &written, now);
	out.resize(written);
	return out;
}

static Bytes HearAt(NetPeerJoiner *joiner, const Bytes &datagram, uint32_t now)
{
	Bytes out(kNetSealedGameMostBytes);
	size_t written = 0;
	joiner->Hear(datagram.data(), datagram.size(), out.data(), out.size(), &written, now);
	out.resize(written);
	return out;
}

// The wire: each step, the time goes on 10 ms, both sides tick, and what each sends is handed to the other, unless lose says to drop it.
// lose is given the kind of message (16 to 20) and how many of that kind went before. Stops once both are Ready, or one has failed, or
// at untilMs. Every message the host sent is kept, by kind, to compare its sends again.
struct Wire
{
	uint32_t now = 0;
	int sentOfKind[256] = {};
	std::vector<std::vector<Bytes> > hostSent[256];

	template <typename Lose>
	void Run(NetPeerJoiner *joiner, NetPeerHostSide *host, uint32_t untilMs, Lose lose)
	{
		for (; now <= untilMs; now += 10)
		{
			joiner->Tick(now);
			host->Tick(now);
			std::vector<Bytes> const fromJoiner = SendsOf(joiner);
			uint8_t const jKind = KindOf(fromJoiner);

			if (!fromJoiner.empty() && !lose(jKind, sentOfKind[jKind]++))
			{
				for (size_t i = 0; i < fromJoiner.size(); i++)
				{
					HearAt(host, fromJoiner[i], now);
				}
			}

			std::vector<Bytes> const fromHost = SendsOf(host);
			uint8_t const hKind = KindOf(fromHost);

			if (!fromHost.empty())
			{
				hostSent[hKind].push_back(fromHost);

				if (!lose(hKind, sentOfKind[hKind]++))
				{
					for (size_t i = 0; i < fromHost.size(); i++)
					{
						HearAt(joiner, fromHost[i], now);
					}
				}
			}

			if ((joiner->Ready() && host->Ready()) || joiner->Failed() || host->Failed())
			{
				return;
			}
		}
	}
};

static bool NoLoss(uint8_t, int)
{
	return false;
}

// Seals a game datagram on one side and opens it on the other
static bool GameDatagram(NetPeerJoiner *joiner, NetPeerHostSide *host, bool fromJoiner, const Bytes &text, uint32_t now)
{
	Bytes sealed(text.size() + kNetSealedGameOverheadBytes);
	size_t written = 0;
	bool const made = fromJoiner ? joiner->Seal(text.data(), text.size(), sealed.data(), sealed.size(), &written, now)
		: host->Seal(text.data(), text.size(), sealed.data(), sealed.size(), &written, now);

	if (!made || written != sealed.size() || memcmp(sealed.data(), "AVPG", 4) != 0)
	{
		return false;
	}

	Bytes const back = fromJoiner ? HearAt(host, sealed, now) : HearAt(joiner, sealed, now);
	return back == text;
}

static void TestPeers()
{
	printf("\n8. A joiner and a host over a pretend wire\n");
	NetIdentity hostI, joinerI;
	BCheck(NetIdentity_Make(&hostI) && NetIdentity_Make(&joinerI), "two fresh identities");
	const char *const region = "newyork";
	uint16_t const port = 40123;

	// The whole join, nothing lost
	{
		NetPeerJoiner joiner;
		NetPeerHostSide host;
		Wire wire;
		BCheck(host.Start(region, port, &hostI, 0) && joiner.Start(region, port, hostI.id, &joinerI, 0), "both sides start");
		BCheck(!joiner.Ready() && !host.Ready(), "and neither has keys yet");
		Bytes const early = Pattern(20, 1);
		Bytes room(early.size() + kNetSealedGameOverheadBytes);
		size_t written = 0;
		BCheck(!joiner.Seal(early.data(), early.size(), room.data(), room.size(), &written, 0) && written == 0,
			"the joiner seals nothing before B4: no game byte goes out before the keys");
		wire.Run(&joiner, &host, 2000, NoLoss);
		BCheck(joiner.Ready() && host.Ready(), "nothing lost: both have their keys after " + std::to_string(wire.now) + " ms");
		BCheck(strlen(joiner.CheckCode()) == 6 && strcmp(joiner.CheckCode(), host.CheckCode()) == 0,
			std::string("both screens show the same check code, ") + joiner.CheckCode());
		BCheck(memcmp(host.JoinerFingerprint(), joinerI.fingerprint, kNetIdentityFingerprintBytes) == 0, "the host knows the joiner's fingerprint");
		BCheck(GameDatagram(&joiner, &host, true, Pattern(100, 2), wire.now), "a game datagram from the joiner opens at the host");
		BCheck(GameDatagram(&joiner, &host, false, Pattern(1392, 3), wire.now), "and one of 1,392 bytes from the host at the joiner");

		// Having heard the host, the joiner sends B5 no more
		size_t later = 0;

		for (uint32_t t = wire.now; t < wire.now + 8000; t += 10)
		{
			joiner.Tick(t);
			later += SendsOf(&joiner).size();
		}

		BCheck(later == 0 && joiner.Ready(), "once the host's first sealed datagram has come, the joiner sends B5 no more, and stays ready");
		BCheck(host.Drops().notOurs == 0 && host.Drops().sealedEarly == 0 && joiner.Drops().notOurs == 0, "and nothing was dropped on the way");

		// Added 2026-11-09 by Melanikus for the AvP port (Snyk Code): ending a handshake wipes what came of it, the check code and the
		// joiner's fingerprint among them, on both sides
		joiner.Reset();
		host.Reset();
		bool wiped = !joiner.Ready() && !host.Ready() && host.Port() == 0;

		for (size_t i = 0; i < kNetHandshakeBCheckCodeBytes; i++)
		{
			wiped = wiped && joiner.CheckCode()[i] == 0 && host.CheckCode()[i] == 0;
		}

		for (size_t i = 0; i < kNetIdentityFingerprintBytes; i++)
		{
			wiped = wiped && host.JoinerFingerprint()[i] == 0;
		}

		BCheck(wiped, "ending the handshakes wipes their check codes, and the joiner's fingerprint the host kept");
	}

	// The joiner's sends when nothing answers: at 0, 0.5, 1.5 and 3.5 s, then it gives up at 7.5 s
	{
		NetPeerJoiner joiner;
		BCheck(joiner.Start(region, port, hostI.id, &joinerI, 0), "a joiner starts, and its host never answers");
		std::vector<uint32_t> times;
		uint32_t failedAt = 0;

		for (uint32_t t = 0; t <= 9000 && failedAt == 0; t++)
		{
			joiner.Tick(t);
			std::vector<Bytes> const sent = SendsOf(&joiner);

			if (!sent.empty())
			{
				times.push_back(t);
				BCheck(sent.size() == 2 && KindOf(sent) == kNetHandshakeBKindB1, "B1, in 2 datagrams of 1,200 bytes, at " + std::to_string(t) + " ms");
			}

			failedAt = joiner.Failed() ? t : 0;
		}

		BCheck(times == std::vector<uint32_t>({ 0, 500, 1500, 3500 }), "B1 went at exactly 0, 500, 1,500 and 3,500 ms");
		BCheck(failedAt == 7500 && joiner.Why() == kNetPeerNoAnswer, "and the joiner gave up at exactly 7,500 ms: the host doesn't answer (7)");
	}

	// A late frame doesn't push the later sends back: they're counted from the first
	{
		NetPeerJoiner joiner;
		joiner.Start(region, port, hostI.id, &joinerI, 0);
		SendsOf(&joiner);
		joiner.Tick(700);   // the first wait ended at 500, but the frame came at 700
		bool const second = !SendsOf(&joiner).empty();
		joiner.Tick(1499);
		bool const notYet = SendsOf(&joiner).empty();
		joiner.Tick(1500);
		BCheck(second && notYet && !SendsOf(&joiner).empty(), "a send due at 500 that goes at 700 still has the next at 1,500, not 1,700");
	}

	// B2 lost once: the joiner sends B1 again, and the host answers with the very same B2
	{
		NetPeerJoiner joiner;
		NetPeerHostSide host;
		Wire wire;
		host.Start(region, port, &hostI, 0);
		joiner.Start(region, port, hostI.id, &joinerI, 0);
		wire.Run(&joiner, &host, 3000, [](uint8_t kind, int before) { return kind == kNetHandshakeBKindB2 && before == 0; });
		BCheck(joiner.Ready() && host.Ready(), "B2 lost once: the join still finishes, at " + std::to_string(wire.now) + " ms");
		BCheck(wire.hostSent[kNetHandshakeBKindB2].size() == 2 && wire.hostSent[kNetHandshakeBKindB2][0] == wire.hostSent[kNetHandshakeBKindB2][1],
			"and the host's second B2 is the first, byte for byte, never a fresh one");
	}

	// B4 lost once: the joiner sends B3 again, and gets the same B4
	{
		NetPeerJoiner joiner;
		NetPeerHostSide host;
		Wire wire;
		host.Start(region, port, &hostI, 0);
		joiner.Start(region, port, hostI.id, &joinerI, 0);
		wire.Run(&joiner, &host, 3000, [](uint8_t kind, int before) { return kind == kNetHandshakeBKindB4 && before == 0; });
		BCheck(joiner.Ready() && host.Ready(), "B4 lost once: the join still finishes, at " + std::to_string(wire.now) + " ms");
		BCheck(wire.hostSent[kNetHandshakeBKindB4].size() == 2 && wire.hostSent[kNetHandshakeBKindB4][0] == wire.hostSent[kNetHandshakeBKindB4][1],
			"and the host's second B4 is the first, byte for byte");
		BCheck(strcmp(joiner.CheckCode(), host.CheckCode()) == 0, "and the check codes agree");
	}

	// B5 lost once: the host isn't ready, drops what the joiner seals, and is ready once B5 comes again
	{
		NetPeerJoiner joiner;
		NetPeerHostSide host;
		Wire wire;
		host.Start(region, port, &hostI, 0);
		joiner.Start(region, port, hostI.id, &joinerI, 0);
		wire.Run(&joiner, &host, 3000, [](uint8_t kind, int before) { return kind == kNetHandshakeBKindB5 && before == 0; });

		// The wire stops when one side is ready and the other isn't only at its end: run it on until the host has B5
		BCheck(joiner.Ready() && host.Ready(), "B5 lost once: the joiner sends it again on its schedule, and the host gets its keys, at "
			+ std::to_string(wire.now) + " ms");
		BCheck(wire.sentOfKind[kNetHandshakeBKindB5] >= 2, "B5 went twice");
	}
	{
		NetPeerJoiner joiner;
		NetPeerHostSide host;
		Wire wire;
		host.Start(region, port, &hostI, 0);
		joiner.Start(region, port, hostI.id, &joinerI, 0);
		wire.Run(&joiner, &host, 3000, [](uint8_t kind, int) { return kind == kNetHandshakeBKindB5; });
		BCheck(joiner.Ready() && !host.Ready(), "with every B5 lost the joiner has its keys and the host doesn't");
		BCheck(!GameDatagram(&joiner, &host, true, Pattern(30, 4), wire.now) && host.Drops().sealedEarly == 1,
			"so the joiner's game datagram is dropped at the host: sealed, but before the host has keys");
		Bytes room(30 + kNetSealedGameOverheadBytes);
		size_t written = 0;
		Bytes const text = Pattern(30, 4);
		BCheck(!host.Seal(text.data(), text.size(), room.data(), room.size(), &written, wire.now), "and the host seals nothing before B5");
	}

	// A joiner that expected another host: B2 fails at its tag, and nothing more is sent
	{
		NetPeerJoiner joiner;
		NetPeerHostSide host;
		Wire wire;
		host.Start(region, port, &hostI, 0);
		joiner.Start(region, port, joinerI.id, &joinerI, 0);   // the listing named another host's id
		wire.Run(&joiner, &host, 3000, NoLoss);
		BCheck(joiner.Failed() && joiner.Why() == kNetHandshakeBBadTag, std::string("a joiner that expected another host's id fails at B2's tag: ")
			+ ReasonName(joiner.Why()));
		size_t later = 0;

		for (uint32_t t = wire.now; t < wire.now + 9000; t += 10)
		{
			joiner.Tick(t);
			later += SendsOf(&joiner).size();
		}

		BCheck(later == 0, "and it sends nothing after that");

		for (uint32_t t = wire.now; t <= 20000; t += 10)
		{
			host.Tick(t);
		}

		BCheck(host.Failed() && host.Why() == kNetPeerNoAnswer, "the host gives up on it once its B2 has waited 10 s (7)");
	}

	// Hostile datagrams on the joiner's port while the join goes on: dropped, counted, and harmless
	{
		NetPeerJoiner joiner, other;
		NetPeerHostSide host;
		host.Start(region, port, &hostI, 0);
		joiner.Start(region, port, hostI.id, &joinerI, 0);
		other.Start(region, port, hostI.id, &joinerI, 0);
		std::vector<Bytes> const b1 = SendsOf(&joiner);
		std::vector<Bytes> const otherB1 = SendsOf(&other);

		for (size_t i = 0; i < b1.size(); i++)
		{
			HearAt(&host, b1[i], 0);
		}

		std::vector<Bytes> const b2 = SendsOf(&host);
		BCheck(KindOf(b2) == kNetHandshakeBKindB2, "the host answers the first joiner's B1");

		// Another handshake's B1 (another id) on the same port: not this handshake's
		uint32_t const before = host.Drops().notOurs;

		for (size_t i = 0; i < otherB1.size(); i++)
		{
			HearAt(&host, otherB1[i], 10);
		}

		BCheck(host.Drops().notOurs == before + otherB1.size() && SendsOf(&host).empty(), "another handshake's B1 on a busy port is dropped, unanswered");

		// A cookie that isn't 16 zeros
		Bytes cookied = b1[0];
		cookied[20] = 1;
		HearAt(&host, cookied, 10);
		BCheck(host.Drops().badCookie == 1 && SendsOf(&host).empty(), "a datagram whose cookie isn't 16 zeros is dropped (5)");

		// Sealed before the keys, junk, and the relay's own magic
		Bytes fakeSealed = Pattern(60, 9);
		memcpy(fakeSealed.data(), "AVPG", 4);
		HearAt(&host, fakeSealed, 10);
		Bytes junk = Pattern(1200, 5);
		HearAt(&host, junk, 10);
		Bytes relays = b1[0];
		memcpy(relays.data(), "AVPR", 4);
		HearAt(&host, relays, 10);
		Bytes shortOne(3, 0);
		HearAt(&host, shortOne, 10);
		BCheck(host.Drops().sealedEarly == 1 && host.Drops().notOurs == before + otherB1.size() + 3 && SendsOf(&host).empty(),
			"a sealed datagram before the keys, 1,200 bytes of junk, an 'AVPR' datagram and 3 bytes: all dropped, none answered");

		// And the join goes on as if none of it had come
		for (size_t i = 0; i < b2.size(); i++)
		{
			HearAt(&joiner, b2[i], 20);
		}

		Wire wire;
		wire.now = 20;
		wire.Run(&joiner, &host, 3000, NoLoss);
		BCheck(joiner.Ready() && host.Ready(), "and the join finishes as if none of them had come");
	}

	// A host whose joiner never sends B1 gives up at 10 s; and a joiner can start again after a failure
	{
		NetPeerHostSide host;
		host.Start(region, port, &hostI, 0);
		host.Tick(9990);
		bool const stillWaiting = !host.Failed();
		host.Tick(10000);
		BCheck(stillWaiting && host.Failed() && host.Why() == kNetPeerNoAnswer, "a host side with no B1 within 10 s gives up (7)");

		NetPeerJoiner joiner;
		NetPeerHostSide again;
		Wire wire;
		joiner.Start(region, port, joinerI.id, &joinerI, 0);
		again.Start(region, port, &hostI, 0);
		wire.Run(&joiner, &again, 3000, NoLoss);
		bool const failedFirst = joiner.Failed();
		Wire second;
		again.Start(region, port, &hostI, 0);
		BCheck(failedFirst && joiner.Start(region, port, hostI.id, &joinerI, 0), "a joiner that failed starts again");
		second.Run(&joiner, &again, 3000, NoLoss);
		BCheck(joiner.Ready() && again.Ready(), "and this time the join finishes");
	}

	BCheck(!NetPeerHostSide().Start("New York", port, &hostI, 0) && !NetPeerHostSide().Start(region, 0, &hostI, 0), "a host side won't start for a bad region or port 0");
	NetIdentity_Free(&hostI);
	NetIdentity_Free(&joinerI);
}

// Added 2026-11-09 by Melanikus for the AvP port (Primary's read of the wiring, finding 1): how often a host starts a handshake. Each start
// costs the host a B2, and a failed one frees its place, so a port the relay names starts at most 3, and the host at most 4 a second.
static void TestStartLimit()
{
	printf("\n9. How often a host starts a handshake\n");
	NetPeerStartLimit limit;
	uint32_t now = 1000;

	BCheck(!limit.Allow(0, now) && limit.Refused() == 1, "port 0 starts nothing");

	// One port: three starts, a second apart so the host's own limit never comes into it, then no more until it's named again
	bool three = true;

	for (int i = 0; i < 3; i++)
	{
		three = three && limit.Allow(40001, now);
		now += 1000;
	}

	BCheck(three, "a named port starts a handshake, and again, up to 3 times");
	BCheck(!limit.Allow(40001, now) && limit.Refused() == 2, "the 4th is refused, however long it waits");
	now += 60000;
	BCheck(!limit.Allow(40001, now), "even a minute later, while it's still the same naming");
	limit.Forget(40001);
	BCheck(limit.Allow(40001, now), "once the relay names it again, it starts afresh");
	limit.Forget(40001);
	now += 1000;

	// The host: 4 a second from all ports together, then the next second 4 more
	bool four = true;

	for (uint16_t port = 41001; port <= 41004; port++)
	{
		four = four && limit.Allow(port, now);
	}

	BCheck(four, "4 ports start one each in the same second");
	BCheck(!limit.Allow(41005, now + 999), "a 5th in that second is refused");
	BCheck(limit.Allow(41005, now + 1000), "and allowed a second later");

	// Room for 8 ports at once, a room's joiners: a 9th it can't count is refused, and nothing it counts is pushed out
	for (uint16_t port = 41001; port <= 41005; port++)
	{
		limit.Forget(port);
	}

	bool eight = true;
	now += 10000;

	for (uint16_t port = 42001; port <= 42008; port++)
	{
		now += 1000;
		eight = eight && limit.Allow(port, now);
	}

	now += 1000;
	BCheck(eight && !limit.Allow(42009, now), "8 ports are counted at once, and a 9th is refused rather than pushing one out");
	BCheck(limit.PortAt(0) == 42001 && limit.PortAt(7) == 42008 && limit.PortAt(8) == 0, "each counted port can be asked about, for the caller to forget");

	limit.Reset();
	BCheck(limit.Refused() == 0 && limit.Allow(42001, now) && limit.PortAt(1) == 0, "Reset forgets everything");
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
			TestSealedGame(run);
			TestTheRelaysSealedGame(run);
		}

		TestFresh();
		TestPeers();
		TestStartLimit();
		NetIdentity_Free(&hostId);
		NetIdentity_Free(&joinerId);
	}

	printf("\n%s: %d check(s) failed\n", bChecksFailed == 0 ? "PASSED" : "FAILED", bChecksFailed);
	return bChecksFailed == 0 ? 0 : 1;
}
