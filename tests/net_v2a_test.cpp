// Tests of 3dc/net_handshake_a.cpp, protocol version 2's handshake A on the game's side, against the relay's own bytes. Written 2026-11-09
// by Melanikus for the AvP port.
//
// tests/vectors/crypto-v2.json is the relay's Java's whole handshake A, from fixed seeds (tests/vectors/README.md). From the file's own
// inputs, the game's side must make exactly the file's A1, and h and ck after every one of the 16 steps; given the file's A2, exactly its
// A3, Split's four values, the session id and the transcript. Then what the file can't show: every byte of A2 changed, each refused for the
// reason it must be; every wrong size; calls out of order; regions the relay can't have; a relay key that isn't one; and the wiping.
//
// It links the post-quantum libraries (tests/net_v2_test_keys.h makes the file's ML-KEM keys from their seeds), so by the owner's rule it's
// built and run on GitHub, not on the team's own machines (2026-11-09: the antivirus held such a test there).

#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

#include "net_crypto.h"
#include "net_pq.h"
#include "net_handshake_a.h"
#include "net_sealed_control.h"
#include "net_relay_v2.h"
#include "net_v2_test_keys.h"
#include "json_reader.h"

static int aChecksFailed = 0;

// Called ACheck, not Check: Snyk Code joins functions with the same name and signature across files, and many tests have a Check
static void ACheck(bool passed, const std::string &what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what.c_str());
	fflush(stdout);

	if (!passed)
	{
		aChecksFailed++;
	}
}

typedef std::vector<uint8_t> Bytes;

static Bytes HexOf(const JsonValue *object, const char *name)
{
	Bytes out;
	const JsonValue *const field = object ? TextMember(*object, name) : NULL;

	if (!field || !FromHex(field->text, &out))
	{
		out.clear();
	}

	return out;
}

static bool Same(const Bytes &a, const uint8_t *b, size_t size)
{
	return a.size() == size && (size == 0 || memcmp(a.data(), b, size) == 0);
}

static bool AllZero(const void *bytes, size_t size)
{
	const uint8_t *const b = static_cast<const uint8_t *>(bytes);
	return std::all_of(b, b + size, [](uint8_t x) { return x == 0; });
}

// The file's handshake A, read once
struct Vectors
{
	std::string region;
	NetRelayStaticKeys relay;
	Bytes eP384Private, eP384Public, ekSeed, ekPublic, m, skemCiphertext, skemSecret;
	Bytes playerP384Private, playerP384Public, playerKemSeed, playerKemPublic;   // pqIK: the player's static keys
	Bytes a1, a2, a3, sessionId, transcript;
	Bytes keyGameToRelay, keyRelayToGame, ivGameToRelay, ivRelayToGame;
	std::vector<Bytes> h, ck;
};

static bool ReadVectors(const JsonValue &all, Vectors *v)
{
	const JsonValue *const a = all.Member("handshakeA");
	const JsonValue *const region = a ? TextMember(*a, "region") : NULL;
	const JsonValue *const relay = a ? a->Member("relayStatic") : NULL;
	const JsonValue *const game = a ? a->Member("gameEphemeral") : NULL;
	const JsonValue *const skem = a ? a->Member("skem") : NULL;
	const JsonValue *const player = a ? a->Member("player") : NULL;
	const JsonValue *const split = a ? a->Member("split") : NULL;
	const JsonValue *const steps = a ? a->Member("steps") : NULL;

	if (!a || !region || !relay || !game || !skem || !player || !split || !steps || steps->kind != kJsonList ||
		steps->items.size() != kNetHandshakeASteps)
	{
		return false;
	}

	v->playerP384Private = HexOf(player, "p384Private");
	v->playerP384Public = HexOf(player, "p384Public");
	v->playerKemSeed = HexOf(player, "mlkemSeed");
	v->playerKemPublic = HexOf(player, "mlkemPublic");

	v->region = region->text;
	Bytes const relayP384 = HexOf(relay, "p384Public"), relayKem = HexOf(relay, "mlkemPublic");

	if (relayP384.size() != sizeof(v->relay.p384) || relayKem.size() != sizeof(v->relay.kem))
	{
		return false;
	}

	memcpy(v->relay.p384, relayP384.data(), sizeof(v->relay.p384));
	memcpy(v->relay.kem, relayKem.data(), sizeof(v->relay.kem));
	v->eP384Private = HexOf(game, "p384Private");
	v->eP384Public = HexOf(game, "p384Public");
	v->ekSeed = HexOf(game, "mlkemSeed");
	v->ekPublic = HexOf(game, "mlkemPublic");
	v->m = HexOf(skem, "m");
	v->skemCiphertext = HexOf(skem, "ciphertext");
	v->skemSecret = HexOf(skem, "secret");
	v->a1 = HexOf(a, "a1");
	v->a2 = HexOf(a, "a2");
	v->a3 = HexOf(a, "a3");
	v->sessionId = HexOf(a, "sessionId");
	v->transcript = HexOf(a, "transcript");
	v->keyGameToRelay = HexOf(split, "keyGameToRelay");
	v->keyRelayToGame = HexOf(split, "keyRelayToGame");
	v->ivGameToRelay = HexOf(split, "ivGameToRelay");
	v->ivRelayToGame = HexOf(split, "ivRelayToGame");

	for (const JsonValue &step : steps->items)
	{
		v->h.push_back(HexOf(&step, "h"));
		v->ck.push_back(HexOf(&step, "ck"));
	}

	return v->eP384Private.size() == kNetCryptoP384ScalarBytes && v->ekSeed.size() == kNetV2TestKemSeedBytes &&
		v->m.size() == kNetV2TestKemMBytes && v->a1.size() == kNetHandshakeA1Bytes && v->a2.size() == kNetHandshakeA2Bytes &&
		v->a3.size() == kNetHandshakeA3Bytes;
}

// The file's player (pqIK): their static P-384 key from its number, their ML-KEM key pair from its seed, and their two public halves as
// A1's s sends them. Made once, in main, and borrowed by every handshake below, as the game's own identity is.
static NetEcdhKey *gPlayerKey = NULL;
static uint8_t gPlayerStatics[kNetHandshakeAStaticBytes];
static uint8_t gPlayerKemPrivate[kNetPqKemPrivateBytes];
static NetHandshakeAPlayer gPlayer;

static bool MakePlayer(const Vectors &v)
{
	uint8_t kemPublic[kNetPqKemPublicBytes];

	if (v.playerP384Public.size() != kNetCryptoP384PublicBytes || v.playerP384Private.size() != kNetCryptoP384ScalarBytes ||
		v.playerKemSeed.size() != kNetV2TestKemSeedBytes)
	{
		return false;
	}

	gPlayerKey = NetCrypto_EcdhFromPrivate(v.playerP384Private.data(), v.playerP384Public.data());

	if (!gPlayerKey || !NetV2Test_KemPairFromSeed(v.playerKemSeed.data(), kemPublic, gPlayerKemPrivate))
	{
		return false;
	}

	memcpy(gPlayerStatics, v.playerP384Public.data(), kNetCryptoP384PublicBytes);
	memcpy(gPlayerStatics + kNetCryptoP384PublicBytes, kemPublic, kNetPqKemPublicBytes);
	gPlayer.p384 = gPlayerKey;
	gPlayer.statics = gPlayerStatics;
	gPlayer.kemPrivate = gPlayerKemPrivate;
	return Same(v.playerKemPublic, kemPublic, sizeof(kemPublic));
}

// The file's own draws: e's P-384 key from its number, e's ML-KEM key from its seed, and the encapsulation from its m
static bool FileDraws(const Vectors &v, NetHandshakeADraws *d)
{
	memset(d, 0, sizeof(*d));

	if (v.eP384Public.size() != sizeof(d->ePublic))
	{
		return false;
	}

	memcpy(d->ePublic, v.eP384Public.data(), sizeof(d->ePublic));
	d->e = NetCrypto_EcdhFromPrivate(v.eP384Private.data(), d->ePublic);
	return d->e && NetV2Test_KemPairFromSeed(v.ekSeed.data(), d->ekPublic, d->ekPrivate) &&
		NetV2Test_KemEncapsulateFrom(v.relay.kem, v.m.data(), d->skemCiphertext, d->skemSecret);
}

// A handshake through A1, from the file's draws: what each hostile A2 below is read by
static bool FileA1(const Vectors &v, NetHandshakeA *hs)
{
	NetHandshakeADraws d;
	return FileDraws(v, &d) && NetHandshakeA_WriteA1(hs, v.region.c_str(), &v.relay, &gPlayer, &d, NULL) == kNetHandshakeAOk;
}

// ----- The relay's own bytes

static void TheVectors(const Vectors &v)
{
	NetHandshakeADraws d;
	bool const drawn = FileDraws(v, &d);
	ACheck(drawn, "the file's draws, made here: e's P-384 key from its number, e's ML-KEM key from its seed, skem from its m");
	ACheck(drawn && Same(v.ekPublic, d.ekPublic, sizeof(d.ekPublic)), "e's ML-KEM key, from its seed, is the file's");
	ACheck(drawn && Same(v.skemCiphertext, d.skemCiphertext, sizeof(d.skemCiphertext)) && Same(v.skemSecret, d.skemSecret, sizeof(d.skemSecret)),
		"skem's ciphertext and secret, from its m, are the file's");

	NetHandshakeA hs;
	NetHandshakeATrace trace;
	memset(&hs, 0, sizeof(hs));
	memset(&trace, 0, sizeof(trace));
	int const wrote = drawn ? NetHandshakeA_WriteA1(&hs, v.region.c_str(), &v.relay, &gPlayer, &d, &trace) : -1;
	ACheck(wrote == kNetHandshakeAOk, "A1 is made");
	ACheck(wrote == kNetHandshakeAOk && Same(v.a1, hs.a1, sizeof(hs.a1)), "A1 is the relay's own, byte for byte (4,930 bytes)");
	ACheck(AllZero(&d, sizeof(d)), "and the draws given are wiped, e's key handed over");

	uint8_t a3[kNetHandshakeA3Bytes];
	NetHandshakeAKeys keys;
	int const read = wrote == kNetHandshakeAOk ? NetHandshakeA_ReadA2(&hs, v.a2.data(), v.a2.size(), a3, &keys, &trace) : -1;
	ACheck(read == kNetHandshakeAOk, "the relay's A2 is read");
	ACheck(read == kNetHandshakeAOk && Same(v.a3, a3, sizeof(a3)), "A3 is the relay's own, byte for byte");
	ACheck(read == kNetHandshakeAOk && Same(v.sessionId, keys.sessionId, sizeof(keys.sessionId)), "the session id is the one A2 sealed");
	ACheck(read == kNetHandshakeAOk && Same(v.keyGameToRelay, keys.split.initiatorToResponderKey, kNetCryptoAesKeyBytes) &&
		Same(v.keyRelayToGame, keys.split.responderToInitiatorKey, kNetCryptoAesKeyBytes) &&
		Same(v.ivGameToRelay, keys.split.initiatorToResponderIv, kNetCryptoNonceBytes) &&
		Same(v.ivRelayToGame, keys.split.responderToInitiatorIv, kNetCryptoNonceBytes),
		"Split's two keys and two IVs are the relay's, the game being the initiator");
	ACheck(read == kNetHandshakeAOk && Same(v.transcript, keys.transcript, sizeof(keys.transcript)), "the transcript is h after A3, the relay's");

	int stepsRight = 0;

	for (size_t i = 0; i < kNetHandshakeASteps && i < trace.count; i++)
	{
		bool const right = Same(v.h[i], trace.h[i], kNetCryptoHashBytes) && Same(v.ck[i], trace.ck[i], kNetCryptoHashBytes);
		stepsRight += right ? 1 : 0;

		if (!right)
		{
			ACheck(false, "step " + std::to_string(i + 1) + ": h and ck aren't the relay's");
		}
	}

	ACheck(trace.count == kNetHandshakeASteps && stepsRight == static_cast<int>(kNetHandshakeASteps),
		std::to_string(stepsRight) + " of the 21 steps' h and ck are the relay's, in order");
	ACheck(AllZero(&hs.noise, sizeof(hs.noise)) && hs.e == NULL && AllZero(hs.ekPrivate, sizeof(hs.ekPrivate)) &&
		AllZero(&hs.player, sizeof(hs.player)), "the handshake's chain, e's keys, its ML-KEM private key and its hold on the player are wiped once it's done");
	NetCrypto_Wipe(&trace, sizeof(trace));
	NetHandshakeA_WipeKeys(&keys);
}

// ----- Every byte of A2 changed

static void EveryChangedA2(const Vectors &v)
{
	int refused = 0, rightReason = 0, wiped = 0, shown = 0;
	int const all = static_cast<int>(kNetHandshakeA2Bytes);

	for (int at = 0; at < all; at++)
	{
		NetHandshakeA hs;
		memset(&hs, 0, sizeof(hs));

		if (!FileA1(v, &hs))
		{
			ACheck(false, "a handshake for the changed A2s couldn't be started");
			return;
		}

		Bytes changed = v.a2;
		changed[static_cast<size_t>(at)] ^= static_cast<uint8_t>(1u << (at % 8));
		uint8_t a3[kNetHandshakeA3Bytes];
		NetHandshakeAKeys keys;
		memset(a3, 0x5A, sizeof(a3));
		int const why = NetHandshakeA_ReadA2(&hs, changed.data(), changed.size(), a3, &keys, NULL);

		// The relay's ephemeral key's bytes: a point no longer on the curve. Anything after: the seal doesn't open (a changed ciphertext
		// gives another secret, FIPS 203's implicit rejection, so it's the seal that fails then too).
		int const expected = at < static_cast<int>(kNetCryptoP384PublicBytes) ? kNetHandshakeABadKey : kNetHandshakeATag;
		refused += why != kNetHandshakeAOk ? 1 : 0;
		rightReason += why == expected ? 1 : 0;
		wiped += AllZero(a3, sizeof(a3)) && AllZero(&keys, sizeof(keys)) && AllZero(&hs.noise, sizeof(hs.noise)) ? 1 : 0;

		// The first few wrong ones are named; the counts below say how many in all
		if (why != expected && shown++ < 5)
		{
			ACheck(false, "A2's byte " + std::to_string(at) + " changed: refused with " + std::to_string(why) + ", not " + std::to_string(expected));
		}
	}

	ACheck(refused == all, std::to_string(refused) + " of A2's 3,257 bytes, each with a bit changed, are refused");
	ACheck(rightReason == all, std::to_string(rightReason) + " of them for the reason they must be: the first 97 as a key that isn't one, the rest at the seal");
	ACheck(wiped == all, "and every time, A3, the keys and the handshake's chain are left wiped");
}

// ----- What else can be wrong

static void TheRefusals(const Vectors &v)
{
	uint8_t a3[kNetHandshakeA3Bytes];
	NetHandshakeAKeys keys;

	size_t const sizes[] = { 0, kNetHandshakeA2Bytes - 1, kNetHandshakeA2Bytes + 1 };

	for (size_t size : sizes)
	{
		NetHandshakeA hs;
		memset(&hs, 0, sizeof(hs));
		Bytes longer = v.a2;
		longer.push_back(0);
		bool const started = FileA1(v, &hs);
		int const why = started ? NetHandshakeA_ReadA2(&hs, longer.data(), size, a3, &keys, NULL) : -1;
		ACheck(why == kNetHandshakeAWrongSize, "an A2 of " + std::to_string(size) + " bytes is refused as the wrong size (1)");
	}

	NetHandshakeA hs;
	memset(&hs, 0, sizeof(hs));
	bool const started = FileA1(v, &hs);
	ACheck(started && NetHandshakeA_ReadA2(&hs, NULL, kNetHandshakeA2Bytes, a3, &keys, NULL) == kNetHandshakeAWrongSize, "no A2 at all: the same");

	memset(&hs, 0, sizeof(hs));
	ACheck(NetHandshakeA_ReadA2(&hs, v.a2.data(), v.a2.size(), a3, &keys, NULL) == kNetHandshakeABadState, "A2 read before any A1: refused");

	memset(&hs, 0, sizeof(hs));
	bool const again = FileA1(v, &hs) && NetHandshakeA_ReadA2(&hs, v.a2.data(), v.a2.size(), a3, &keys, NULL) == kNetHandshakeAOk;
	ACheck(again && NetHandshakeA_ReadA2(&hs, v.a2.data(), v.a2.size(), a3, &keys, NULL) == kNetHandshakeABadState &&
		AllZero(a3, sizeof(a3)) && AllZero(&keys, sizeof(keys)), "A2 a second time, once done: refused, with nothing given");

	// Regions the relay can't have, and a good one that isn't the file's (whose A1 then differs from the file's)
	const char *const badRegions[] = { "", "Virginia", "new york", "abcdefghijklmnopq", "east\x01" };

	for (const char *region : badRegions)
	{
		NetHandshakeADraws d;
		memset(&hs, 0, sizeof(hs));
		bool const drawn = FileDraws(v, &d);
		ACheck(drawn && NetHandshakeA_WriteA1(&hs, region, &v.relay, &gPlayer, &d, NULL) == kNetHandshakeABadState && AllZero(&d, sizeof(d)),
			std::string("the region \"") + region + "\" is refused, and the draws wiped");
	}

	ACheck(NetHandshakeA_RegionIsGood("a-1") && NetHandshakeA_RegionIsGood("abcdefghijklmnop") && !NetHandshakeA_RegionIsGood(NULL),
		"1 to 16 small letters, digits and '-': taken");

	// A relay whose P-384 key isn't on the curve: refused at A1, as a key that isn't one
	{
		NetHandshakeADraws d;
		NetRelayStaticKeys broken = v.relay;
		broken.p384[50] ^= 0x01;
		memset(&hs, 0, sizeof(hs));
		bool const drawn = FileDraws(v, &d);
		ACheck(drawn && NetHandshakeA_WriteA1(&hs, v.region.c_str(), &broken, &gPlayer, &d, NULL) == kNetHandshakeABadKey && hs.e == NULL,
			"a relay P-384 key that isn't on the curve is refused at A1 (a key that isn't one), e's key destroyed");
	}

	// No draws at all
	memset(&hs, 0, sizeof(hs));
	ACheck(NetHandshakeA_WriteA1(&hs, v.region.c_str(), &v.relay, &gPlayer, NULL, NULL) == kNetHandshakeABadState, "A1 with no draws: refused");
}

// ----- The player (pqIK): proved by both static keys, and only borrowed

static void ThePlayer(const Vectors &v)
{
	uint8_t a3[kNetHandshakeA3Bytes];
	NetHandshakeAKeys keys;

	// The player's P-384 key, with another ML-KEM private key: A1 is the very same (ss holds, and s is the same public halves), but A2's
	// second encapsulation, to the player's static ML-KEM key, opens to another secret, so A2's seal fails. That's the post-quantum half
	// of the player's proof: someone who broke ECDH alone could make A1, but never read A2 or make A3.
	{
		uint8_t seed[kNetV2TestKemSeedBytes];
		uint8_t otherPublic[kNetPqKemPublicBytes];
		uint8_t otherPrivate[kNetPqKemPrivateBytes];
		memset(seed, 0x6b, sizeof(seed));
		bool const made = NetV2Test_KemPairFromSeed(seed, otherPublic, otherPrivate);
		NetHandshakeAPlayer half = gPlayer;
		half.kemPrivate = otherPrivate;
		NetHandshakeA hs;
		NetHandshakeADraws d;
		memset(&hs, 0, sizeof(hs));
		bool const wrote = made && FileDraws(v, &d) && NetHandshakeA_WriteA1(&hs, v.region.c_str(), &v.relay, &half, &d, NULL) == kNetHandshakeAOk;
		ACheck(wrote && Same(v.a1, hs.a1, sizeof(hs.a1)), "with another ML-KEM private key, A1 is the very same");
		ACheck(wrote && NetHandshakeA_ReadA2(&hs, v.a2.data(), v.a2.size(), a3, &keys, NULL) == kNetHandshakeATag && AllZero(a3, sizeof(a3)),
			"but the relay's A2 doesn't open (8): only the holder of the player's static ML-KEM key goes on");
		NetCrypto_Wipe(otherPrivate, sizeof(otherPrivate));
	}

	// No player, or one with a part missing: this game's mistake, refused before anything is made
	{
		NetHandshakeAPlayer const missing[] = {
			{ NULL, gPlayer.statics, gPlayer.kemPrivate },
			{ gPlayer.p384, NULL, gPlayer.kemPrivate },
			{ gPlayer.p384, gPlayer.statics, NULL } };
		bool refused = true;

		for (const NetHandshakeAPlayer &p : missing)
		{
			NetHandshakeA hs;
			NetHandshakeADraws d;
			memset(&hs, 0, sizeof(hs));
			refused = refused && FileDraws(v, &d) && NetHandshakeA_WriteA1(&hs, v.region.c_str(), &v.relay, &p, &d, NULL) == kNetHandshakeABadState &&
				AllZero(&d, sizeof(d));
		}

		NetHandshakeA hs;
		NetHandshakeADraws d;
		memset(&hs, 0, sizeof(hs));
		refused = refused && FileDraws(v, &d) && NetHandshakeA_WriteA1(&hs, v.region.c_str(), &v.relay, NULL, &d, NULL) == kNetHandshakeABadState;
		ACheck(refused, "no player, or a player with no P-384 key, no public halves or no ML-KEM key: refused (101), and the draws wiped");
	}

	// Borrowed, never freed: after every handshake above, the player's own key still works
	uint8_t agreed[kNetCryptoP384ScalarBytes];
	ACheck(NetCrypto_EcdhAgree(gPlayerKey, v.relay.p384, agreed), "the player's static key still works after the handshakes: it was only borrowed");
	NetCrypto_Wipe(agreed, sizeof(agreed));
}

// ----- The sealed control channel (net_sealed_control.h), under the file's handshake A keys

// The file's keys, as a finished handshake A gives them to the game
static NetHandshakeAKeys FileKeys(const Vectors &v)
{
	NetHandshakeAKeys keys;
	memset(&keys, 0, sizeof(keys));
	memcpy(keys.split.initiatorToResponderKey, v.keyGameToRelay.data(), kNetCryptoAesKeyBytes);
	memcpy(keys.split.responderToInitiatorKey, v.keyRelayToGame.data(), kNetCryptoAesKeyBytes);
	memcpy(keys.split.initiatorToResponderIv, v.ivGameToRelay.data(), kNetCryptoNonceBytes);
	memcpy(keys.split.responderToInitiatorIv, v.ivRelayToGame.data(), kNetCryptoNonceBytes);
	memcpy(keys.sessionId, v.sessionId.data(), kNetHandshakeSessionIdBytes);
	return keys;
}

// The relay's side of the same session: its two directions are the game's the other way round. Only for opening the game's own messages
// as the relay does, which is how the file writes its refusals down.
static void RelaySide(NetSealedControl *c)
{
	NetV2Direction const game = c->out;
	c->out = c->in;
	c->in = game;
}

static void TheSealedChannel(const JsonValue &all, const Vectors &v)
{
	if (v.keyGameToRelay.size() != kNetCryptoAesKeyBytes || v.ivGameToRelay.size() != kNetCryptoNonceBytes ||
		v.sessionId.size() != kNetHandshakeSessionIdBytes)
	{
		ACheck(false, "the file's split and session id, for the sealed channel");
		return;
	}

	NetHandshakeAKeys keys = FileKeys(v);
	const JsonValue *const sealed = all.Member("sealed");
	ACheck(sealed && sealed->kind == kJsonList && sealed->items.size() == 4, "the file's 4 sealed control messages are there");

	if (!sealed || sealed->kind != kJsonList)
	{
		return;
	}

	uint32_t const now = 1000;
	int right = 0;
	Bytes firstGame;

	for (const JsonValue &s : sealed->items)
	{
		const JsonValue *const from = TextMember(s, "from");
		const JsonValue *const counter = s.Member("counter");
		Bytes const message = HexOf(&s, "message"), want = HexOf(&s, "sealed");

		if (!from || !counter || counter->kind != kJsonNumber || message.size() != kNetSealedControlMessageBytes || want.size() != kNetSealedControlBytes)
		{
			ACheck(false, "a sealed message in the file isn't the shape of one");
			continue;
		}

		NetSealedControl c;
		NetSealedControl_Start(&c, &keys, now);

		if (from->text == "game")
		{
			// The game's: sealed here at the file's counter, to exactly the file's bytes
			c.sent = counter->number;
			uint8_t made[kNetSealedControlBytes];
			bool const ok = NetSealedControl_Seal(&c, message.data(), made, now) && Same(want, made, sizeof(made));
			right += ok ? 1 : 0;
			ACheck(ok, "the game's sealed message at counter " + std::to_string(counter->number) + " is the relay's own bytes");

			if (counter->number == 0)
			{
				firstGame = want;
			}
		}
		else
		{
			// The relay's: opened here to exactly the file's message
			uint8_t got[kNetSealedControlMessageBytes];
			bool const ok = NetSealedControl_Open(&c, want.data(), want.size(), got, now) == kNetSealedOk && Same(message, got, sizeof(got));
			right += ok ? 1 : 0;
			ACheck(ok, "the relay's sealed message at counter " + std::to_string(counter->number) + " opens to the file's message");
		}

		NetSealedControl_Wipe(&c);
	}

	ACheck(right == 4, std::to_string(right) + " of 4 sealed messages, the one in epoch 1 among them, are the relay's");

	if (firstGame.size() != kNetSealedControlBytes)
	{
		return;
	}

	// The file's refusals, as the relay refuses them: opening the game's first sealed message
	NetSealedControl relay;
	uint8_t got[kNetSealedControlMessageBytes];
	NetSealedControl_Start(&relay, &keys, now);
	RelaySide(&relay);
	Bytes tagChanged = firstGame, messageChanged = firstGame;
	tagChanged[100] ^= 0x01;
	messageChanged[30] ^= 0x01;
	ACheck(NetSealedControl_Open(&relay, firstGame.data(), kNetSealedControlBytes - 1, got, now) == kNetSealedWrongSize, "103 bytes: refused (1)");
	ACheck(NetSealedControl_Open(&relay, tagChanged.data(), tagChanged.size(), got, now) == kNetSealedTag && AllZero(got, sizeof(got)),
		"a bit of its tag changed: refused (8), nothing given");
	ACheck(NetSealedControl_Open(&relay, messageChanged.data(), messageChanged.size(), got, now) == kNetSealedTag, "a bit of its message changed: refused (8)");
	ACheck(NetSealedControl_Open(&relay, firstGame.data(), firstGame.size(), got, now) == kNetSealedOk, "as it is: opened");
	ACheck(NetSealedControl_Open(&relay, firstGame.data(), firstGame.size(), got, now) == kNetSealedReplayed, "the same again: refused (9)");

	// The file's own refused sealed messages, each by its number, as the relay opens them (refusal 9's after the first was taken)
	const JsonValue *const refusals = all.Member("refusals");
	int sealedRefusals = 0, sealedRight = 0;

	for (size_t i = 0; refusals && refusals->kind == kJsonList && i < refusals->items.size(); i++)
	{
		const JsonValue &r = refusals->items[i];
		const JsonValue *const number = r.Member("refusal");
		Bytes const bytes = HexOf(&r, "bytes");

		if (!number || number->kind != kJsonNumber || (bytes.size() != kNetSealedControlBytes && bytes.size() != kNetSealedControlBytes - 1))
		{
			continue;
		}

		sealedRefusals++;
		NetSealedControl fresh;
		NetSealedControl_Start(&fresh, &keys, now);
		RelaySide(&fresh);

		if (number->number == static_cast<uint64_t>(kNetSealedReplayed))
		{
			NetSealedControl_Open(&fresh, firstGame.data(), firstGame.size(), got, now);
		}

		int const why = NetSealedControl_Open(&fresh, bytes.data(), bytes.size(), got, now);
		sealedRight += static_cast<uint64_t>(why) == number->number ? 1 : 0;
		NetSealedControl_Wipe(&fresh);
	}

	ACheck(sealedRefusals == 4 && sealedRight == 4, std::to_string(sealedRight) + " of the file's " + std::to_string(sealedRefusals) +
		" refused sealed messages (1, 8, 8, 9) are refused for their own number");

	// What the file can't show
	Bytes other = firstGame;
	other[8] ^= 0x01;
	ACheck(NetSealedControl_Open(&relay, other.data(), other.size(), got, now) == kNetSealedNotOurs, "another session's id: refused as not ours");
	other = firstGame;
	other[6] = 1;
	ACheck(NetSealedControl_Open(&relay, other.data(), other.size(), got, now) == kNetSealedWrongShape, "byte 6 not 0: refused (2)");
	other = firstGame;
	other[5] = kNetV2KindA2;
	ACheck(NetSealedControl_Open(&relay, other.data(), other.size(), got, now) == kNetSealedWrongShape, "another kind: refused (2)");
	other = firstGame;
	other[20] = 1;   // counter 2^32
	ACheck(NetSealedControl_Open(&relay, other.data(), other.size(), got, now) == kNetSealedReplayed, "a counter of 2^32, past a session's most: refused (9)");

	// 65,536 failed tags end the session; then even a good one is refused
	NetSealedControl worn;
	NetSealedControl_Start(&worn, &keys, now);
	RelaySide(&worn);
	int failed = 0;

	for (uint64_t i = 0; i < kNetSealedControlMostFailedTags; i++)
	{
		failed += NetSealedControl_Open(&worn, tagChanged.data(), tagChanged.size(), got, now) == kNetSealedTag ? 1 : 0;
	}

	ACheck(failed == 65536 && NetSealedControl_Open(&worn, firstGame.data(), firstGame.size(), got, now) == kNetSealedOver,
		"after 65,536 failed tags the session is over (16), and a good message is refused too");

	// 24 hours end it, by the clock's difference, past the clock's own wrap
	NetSealedControl old;
	NetSealedControl_Start(&old, &keys, 0xFFFFFF00u);
	uint8_t made[kNetSealedControlBytes];
	uint8_t const message[kNetSealedControlMessageBytes] = { 1 };
	ACheck(NetSealedControl_Seal(&old, message, made, 0xFFFFFF00u + kNetSealedControlMostMs - 1), "a millisecond before 24 hours, across the clock's wrap: sealed");
	ACheck(!NetSealedControl_Seal(&old, message, made, 0xFFFFFF00u + kNetSealedControlMostMs) && AllZero(made, sizeof(made)),
		"at 24 hours: nothing sealed, and the session over");
	ACheck(!NetSealedControl_Seal(&old, message, made, 0xFFFFFF00u), "and over for good, whatever the clock says after");

	NetSealedControl_Wipe(&relay);
	NetSealedControl_Wipe(&worn);
	NetSealedControl_Wipe(&old);
	ACheck(AllZero(&relay, sizeof(relay)), "wiping leaves nothing of the keys");
	NetHandshakeA_WipeKeys(&keys);
}

// ----- The session (net_relay_v2.h): the cookie round, handshake A with its resends, the sealed channel, as the relay's own datagrams

uint16_t const kControl = 27500;

// A0', the relay's answer to our COOKIE_ASK: 'AVPR', 2, COOKIE, 0, 0, the id, the cookie, then zeros
static Bytes CookieAnswer(const Bytes &id, const Bytes &cookie)
{
	Bytes answer(kNetV2CookieAnswerBytes, 0);
	memcpy(answer.data(), kNetV2MagicRelay, 4);
	answer[4] = kNetV2Version;
	answer[5] = kNetV2KindCookie;
	memcpy(answer.data() + 8, id.data(), kNetV2IdBytes);
	memcpy(answer.data() + 16, cookie.data(), kNetV2CookieBytes);
	return answer;
}

static std::vector<Bytes> HexList(const JsonValue *object, const char *name)
{
	std::vector<Bytes> out;
	const JsonValue *const list = object ? object->Member(name) : NULL;

	for (size_t i = 0; list && list->kind == kJsonList && i < list->items.size(); i++)
	{
		Bytes b;
		out.push_back(list->items[i].kind == kJsonText && FromHex(list->items[i].text, &b) ? b : Bytes());
	}

	return out;
}

// A session started from the file's draws and id, at now
static bool FileSession(const Vectors &v, const Bytes &id, NetRelayV2Session *s, uint32_t now)
{
	NetHandshakeADraws d;
	return FileDraws(v, &d) && s->Start(v.region.c_str(), &v.relay, &gPlayer, kControl, id.data(), &d, now);
}

static void TheSession(const JsonValue &all, const Vectors &v)
{
	const JsonValue *const d = all.Member("handshakeDatagrams");
	Bytes const id = HexOf(d, "id"), cookie = HexOf(d, "cookie");
	std::vector<Bytes> const a1 = HexList(d, "a1"), a2 = HexList(d, "a2"), a3 = HexList(d, "a3");

	if (id.size() != kNetV2IdBytes || cookie.size() != kNetV2CookieBytes || a1.size() != kNetRelayV2A1Fragments || a2.size() != kNetRelayV2A2Fragments || a3.size() != 1)
	{
		ACheck(false, "the file's handshake datagrams, for the session");
		return;
	}

	Bytes const answer = CookieAnswer(id, cookie);
	NetRelayV2Send sent[kNetRelayV2MostSends];
	NetRelayV2Opened got[kNetRelayV2MostOpened];

	// The whole exchange, every byte the relay's
	{
		NetRelayV2Session s;
		uint8_t ask[kNetV2DatagramBytes];
		NetV2_CookieAsk(id.data(), ask);
		bool const started = FileSession(v, id, &s, 0);
		size_t const first = s.TakeSends(sent, kNetRelayV2MostSends);
		ACheck(started && first == 1 && sent[0].toPort == kControl && sent[0].size == kNetV2DatagramBytes && memcmp(sent[0].bytes, ask, sizeof(ask)) == 0,
			"the session's first send is A0, the COOKIE_ASK for its id, to the control port");

		s.Hear(kControl, answer.data(), answer.size(), 100);
		size_t const second = s.TakeSends(sent, kNetRelayV2MostSends);
		bool a1Right = second == kNetRelayV2A1Fragments;

		for (size_t i = 0; i < kNetRelayV2A1Fragments && a1Right; i++)
		{
			a1Right = sent[i].toPort == kControl && Same(a1[i], sent[i].bytes, sent[i].size);
		}

		ACheck(a1Right, "A0' brings A1 at once: its 5 datagrams, with the cookie, are the relay's own byte for byte");

		s.Hear(kControl, a2[1].data(), a2[1].size(), 200);
		s.Hear(kControl, a2[2].data(), a2[2].size(), 205);
		ACheck(!s.Ready() && s.TakeSends(sent, kNetRelayV2MostSends) == 0, "A2's second and third datagrams alone: nothing yet");
		s.Hear(kControl, a2[0].data(), a2[0].size(), 210);
		size_t const third = s.TakeSends(sent, kNetRelayV2MostSends);
		ACheck(s.Ready() && third == 1 && Same(a3[0], sent[0].bytes, sent[0].size), "the whole of A2, last datagram first: ready, and A3's datagram is the relay's own");

		// The first sealed message: the game's, at counter 0, as the file has it
		const JsonValue *const sealed = all.Member("sealed");
		const JsonValue *const game0 = sealed && sealed->kind == kJsonList && !sealed->items.empty() ? &sealed->items[0] : NULL;
		Bytes const message0 = HexOf(game0, "message"), sealed0 = HexOf(game0, "sealed");
		bool const sealedRight = message0.size() == kNetSealedControlMessageBytes && s.Seal(message0.data(), kControl, 300) &&
			s.TakeSends(sent, kNetRelayV2MostSends) == 1 && Same(sealed0, sent[0].bytes, sent[0].size);
		ACheck(sealedRight, "a version 1 message sealed by the session is the file's game message at counter 0");

		// The relay's, from a joiner's port: opened, and given with the port it came from
		const JsonValue *const relay0 = sealed && sealed->items.size() > 1 ? &sealed->items[1] : NULL;
		Bytes const relayMessage = HexOf(relay0, "message"), relaySealed = HexOf(relay0, "sealed");
		s.Hear(27503, relaySealed.data(), relaySealed.size(), 310);
		size_t const opened = s.TakeOpened(got, kNetRelayV2MostOpened);
		ACheck(opened == 1 && got[0].fromPort == 27503 && Same(relayMessage, got[0].message, sizeof(got[0].message)),
			"the relay's sealed message, from a joiner's port, is opened, with its port");
		s.Hear(27503, relaySealed.data(), relaySealed.size(), 320);
		ACheck(s.TakeOpened(got, kNetRelayV2MostOpened) == 0 && s.Drops().refused == 1, "the same again: refused, and counted");
		s.Hear(kControl, a2[0].data(), a2[0].size(), 330);
		ACheck(s.Ready() && s.Drops().notOurs == 1, "A2 sent again by the relay, once ready: dropped, and the session goes on");
		s.Wipe();
		ACheck(!s.Ready() && !s.Failed(), "wiped, it's nothing again");
	}

	// The timers: A0 again at 0.5, 1.5 and 3.5 s, and "the relay doesn't answer" at 7.5 s
	{
		NetRelayV2Session s;
		FileSession(v, id, &s, 1000);
		s.TakeSends(sent, kNetRelayV2MostSends);
		uint32_t const at[] = { 1499, 1500, 2499, 2500, 4499, 4500, 6000, 8499 };
		size_t const want[] = { 0, 1, 0, 1, 0, 1, 0, 0 };
		bool right = true;

		for (size_t i = 0; i < 8; i++)
		{
			s.Tick(at[i]);
			right = right && s.TakeSends(sent, kNetRelayV2MostSends) == want[i];
		}

		ACheck(right && !s.Failed(), "A0 goes again at 0.5, 1.5 and 3.5 s, and only then");
		s.Tick(8500);
		ACheck(s.Failed() && s.Why() == kNetRelayV2NoAnswer, "at 7.5 s it gives up: the relay doesn't answer (7)");
	}

	// A1's own timers, counted from A0'
	{
		NetRelayV2Session s;
		FileSession(v, id, &s, 0);
		s.Hear(kControl, answer.data(), answer.size(), 2000);
		s.TakeSends(sent, kNetRelayV2MostSends);
		s.Tick(2499);
		size_t const none = s.TakeSends(sent, kNetRelayV2MostSends);
		s.Tick(2500);
		size_t const again = s.TakeSends(sent, kNetRelayV2MostSends);
		ACheck(none == 0 && again == kNetRelayV2A1Fragments && Same(a1[0], sent[0].bytes, sent[0].size), "A1 goes again, all 5 datagrams, the same bytes, 0.5 s after A0'");
		s.Tick(9499);
		ACheck(!s.Failed(), "A1's own count: not given up at 7.5 s from the start");
		s.Tick(9500);
		ACheck(s.Failed() && s.Why() == kNetRelayV2NoAnswer, "but at 7.5 s from A1's first send");
	}

	// What isn't the relay's answer
	{
		NetRelayV2Session s;
		FileSession(v, id, &s, 0);
		s.TakeSends(sent, kNetRelayV2MostSends);
		s.Hear(27501, answer.data(), answer.size(), 10);
		Bytes otherId = id;
		otherId[0] ^= 0x01;
		Bytes const otherAnswer = CookieAnswer(otherId, cookie);
		s.Hear(kControl, otherAnswer.data(), otherAnswer.size(), 20);
		ACheck(s.TakeSends(sent, kNetRelayV2MostSends) == 0 && s.Drops().notFromControl == 1 && s.Drops().notOurs == 1,
			"A0' from another port, or for another handshake: dropped, counted, and no A1");

		s.Hear(kControl, answer.data(), answer.size(), 30);
		s.TakeSends(sent, kNetRelayV2MostSends);
		Bytes cookied = a2[0];
		cookied[16] = 1;
		s.Hear(kControl, cookied.data(), cookied.size(), 40);
		ACheck(s.Failed() && s.Why() == kNetRelayV2BadCookie, "an A2 datagram with a cookie that isn't 16 zeros ends it (5)");
	}

	{
		NetRelayV2Session s;
		FileSession(v, id, &s, 0);
		s.Hear(kControl, answer.data(), answer.size(), 10);
		Bytes changed = a2[1];
		// The second datagram carries A2's last 525 bytes at 36 to 560; the sealed session id is A2's last 24, so 537 to 560. 540 is in it.
		changed[540] ^= 0x01;
		s.Hear(kControl, a2[0].data(), a2[0].size(), 20);
		s.Hear(kControl, changed.data(), changed.size(), 30);
		ACheck(s.Failed() && s.Why() == kNetHandshakeATag, "an A2 whose seal doesn't open ends it for good (8)");
		ACheck(!s.Seal(changed.data(), kControl, 40), "and nothing can be sealed then");
	}
}

// ----- Picking the nearest relay by the cookie round (NetRelayV2Picker)

static void ThePicker()
{
	uint16_t const ports[2] = { 27500, 28500 };
	uint8_t ids[2][kNetRelayEchoes][kNetV2IdBytes];

	for (size_t r = 0; r < 2; r++)
	{
		for (size_t k = 0; k < kNetRelayEchoes; k++)
		{
			for (size_t b = 0; b < kNetV2IdBytes; b++)
			{
				ids[r][k][b] = static_cast<uint8_t>(r * 100 + k * 10 + b + 1);
			}
		}
	}

	Bytes const cookie(kNetV2CookieBytes, 0x77);
	NetRelayV2PickerSend sent[kNetRelayV2MostPickerSends];
	NetRelayV2Picker p;
	ACheck(p.Start(2, ports, &ids[0][0][0], 1000), "the picker starts for 2 relays");
	size_t const first = p.TakeSends(sent, kNetRelayV2MostPickerSends);
	uint8_t ask[kNetV2DatagramBytes];
	NetV2_CookieAsk(ids[1][0], ask);
	ACheck(first == 2 && sent[1].relay == 1 && sent[1].port == 28500 && memcmp(sent[1].bytes, ask, sizeof(ask)) == 0,
		"its first round is a COOKIE_ASK to each relay's control port, each with its own id");

	p.Tick(1099);
	ACheck(p.TakeSends(sent, kNetRelayV2MostPickerSends) == 0, "the next round not before 100 ms");
	p.Tick(1100);
	ACheck(p.TakeSends(sent, kNetRelayV2MostPickerSends) == 2, "and then, to both");
	p.Tick(1200);
	p.TakeSends(sent, kNetRelayV2MostPickerSends);

	// Relay 0 answers its first ask after 90 ms; relay 1 its second after 40 ms
	Bytes const answer00 = CookieAnswer(Bytes(ids[0][0], ids[0][0] + kNetV2IdBytes), cookie);
	Bytes const answer11 = CookieAnswer(Bytes(ids[1][1], ids[1][1] + kNetV2IdBytes), cookie);
	Bytes const wrongRelay = CookieAnswer(Bytes(ids[1][2], ids[1][2] + kNetV2IdBytes), cookie);
	ACheck(p.Hear(0, 27500, answer00.data(), answer00.size(), 1090), "relay 0's answer to its first ask is its own");
	ACheck(!p.Hear(0, 27501, answer00.data(), answer00.size(), 1091), "one from another of its ports isn't its control port's");
	p.Hear(0, 27500, wrongRelay.data(), wrongRelay.size(), 1092);
	ACheck(p.NotOurs() == 1, "an answer for relay 1's id, from relay 0, isn't taken");
	p.Hear(0, 27500, answer00.data(), answer00.size(), 1093);
	ACheck(p.NotOurs() == 2, "nor the same answer twice");
	ACheck(p.Hear(1, 28500, answer11.data(), answer11.size(), 1140), "relay 1's answer to its second ask");

	uint32_t ms = 0;
	ACheck(p.RelayMs(0, &ms) && ms == 90 && p.RelayMs(1, &ms) && ms == 40, "each relay's round trip: 90 and 40 ms");
	ACheck(!p.Done(2199) && p.Done(2200), "done once the last ask has had its second");
	ACheck(p.Best() == 1 && p.BestMs() == 40, "the nearest is relay 1, at 40 ms");

	NetRelayV2Picker late;
	late.Start(2, ports, &ids[0][0][0], 0);
	Bytes const answer10 = CookieAnswer(Bytes(ids[1][0], ids[1][0] + kNetV2IdBytes), cookie);
	late.Hear(1, 28500, answer10.data(), answer10.size(), 1001);
	ACheck(late.Best() == -1, "an answer later than 1 s counts for nothing");
	ACheck(!late.Start(0, ports, &ids[0][0][0], 0) && !late.Start(kNetRelayMostRelays + 1, ports, &ids[0][0][0], 0), "no relays, or too many: refused");
}

// ----- The game's real draws

static void TheRealDraws(const Vectors &v)
{
	NetHandshakeADraws one, two;
	bool const drawn = NetHandshakeA_Draw(&v.relay, &one) && NetHandshakeA_Draw(&v.relay, &two);
	ACheck(drawn, "the game's own draws are made, twice: a fresh P-384 key, ML-KEM key pair and encapsulation each time");
	ACheck(drawn && memcmp(one.ePublic, two.ePublic, sizeof(one.ePublic)) != 0 && memcmp(one.ekPublic, two.ekPublic, sizeof(one.ekPublic)) != 0 &&
		memcmp(one.skemSecret, two.skemSecret, sizeof(one.skemSecret)) != 0, "and the two are different, every part");

	NetHandshakeA hs;
	memset(&hs, 0, sizeof(hs));
	ACheck(drawn && NetHandshakeA_WriteA1(&hs, v.region.c_str(), &v.relay, &gPlayer, &one, NULL) == kNetHandshakeAOk && !Same(v.a1, hs.a1, sizeof(hs.a1)),
		"an A1 from them is made, and isn't the file's");
	NetHandshakeA_Wipe(&hs);
	NetHandshakeA_WipeDraws(&two);
	ACheck(AllZero(&hs, sizeof(hs)) && AllZero(&two, sizeof(two)), "wiping leaves nothing");

	NetRelayStaticKeys refusedKem = v.relay;
	memset(refusedKem.kem, 0xFF, sizeof(refusedKem.kem));
	NetHandshakeADraws three;
	ACheck(!NetHandshakeA_Draw(&refusedKem, &three) && AllZero(&three, sizeof(three)),
		"a relay ML-KEM key FIPS 203 section 7.2 refuses: no draws, nothing left");
}

int main()
{
	std::string const text = ReadRepositoryFile("tests/vectors/crypto-v2.json");
	JsonReader reader(text);
	JsonValue all;
	Vectors v;
	bool const read = !text.empty() && reader.ReadAll(&all) && all.kind == kJsonObject && ReadVectors(all, &v);
	ACheck(read, "tests/vectors/crypto-v2.json: handshake A read");
	ACheck(NetPq_Available(), "this test has the real post-quantum half (net_pq_native.cpp)");
	bool const player = read && MakePlayer(v);
	ACheck(player, "the file's player: their P-384 key from its number, their ML-KEM key from its seed, the file's public halves");

	if (read && player)
	{
		TheVectors(v);
		EveryChangedA2(v);
		TheRefusals(v);
		ThePlayer(v);
		TheSealedChannel(all, v);
		TheSession(all, v);
		TheRealDraws(v);
	}

	ThePicker();

	// The player is the test's own, as an identity is the game's: freed here, once, at the end
	NetCrypto_EcdhFree(gPlayerKey);
	NetCrypto_Wipe(gPlayerKemPrivate, sizeof(gPlayerKemPrivate));

	if (aChecksFailed > 0)
	{
		printf("%d check%s FAILED\n", aChecksFailed, aChecksFailed == 1 ? "" : "s");
		return 1;
	}

	printf("Every check passed\n");
	return 0;
}
