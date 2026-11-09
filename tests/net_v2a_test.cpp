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
	const JsonValue *const split = a ? a->Member("split") : NULL;
	const JsonValue *const steps = a ? a->Member("steps") : NULL;

	if (!a || !region || !relay || !game || !skem || !split || !steps || steps->kind != kJsonList || steps->items.size() != kNetHandshakeASteps)
	{
		return false;
	}

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
	return FileDraws(v, &d) && NetHandshakeA_WriteA1(hs, v.region.c_str(), &v.relay, &d, NULL) == kNetHandshakeAOk;
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
	int const wrote = drawn ? NetHandshakeA_WriteA1(&hs, v.region.c_str(), &v.relay, &d, &trace) : -1;
	ACheck(wrote == kNetHandshakeAOk, "A1 is made");
	ACheck(wrote == kNetHandshakeAOk && Same(v.a1, hs.a1, sizeof(hs.a1)), "A1 is the relay's own, byte for byte (3,249 bytes)");
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
		std::to_string(stepsRight) + " of the 16 steps' h and ck are the relay's, in order");
	ACheck(AllZero(&hs.noise, sizeof(hs.noise)) && hs.e == NULL && AllZero(hs.ekPrivate, sizeof(hs.ekPrivate)),
		"the handshake's chain, e's keys and its ML-KEM private key are wiped once it's done");
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

	ACheck(refused == all, std::to_string(refused) + " of A2's 1,689 bytes, each with a bit changed, are refused");
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
		ACheck(drawn && NetHandshakeA_WriteA1(&hs, region, &v.relay, &d, NULL) == kNetHandshakeABadState && AllZero(&d, sizeof(d)),
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
		ACheck(drawn && NetHandshakeA_WriteA1(&hs, v.region.c_str(), &broken, &d, NULL) == kNetHandshakeABadKey && hs.e == NULL,
			"a relay P-384 key that isn't on the curve is refused at A1 (a key that isn't one), e's key destroyed");
	}

	// No draws at all
	memset(&hs, 0, sizeof(hs));
	ACheck(NetHandshakeA_WriteA1(&hs, v.region.c_str(), &v.relay, NULL, NULL) == kNetHandshakeABadState, "A1 with no draws: refused");
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
	ACheck(drawn && NetHandshakeA_WriteA1(&hs, v.region.c_str(), &v.relay, &one, NULL) == kNetHandshakeAOk && !Same(v.a1, hs.a1, sizeof(hs.a1)),
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

	if (read)
	{
		TheVectors(v);
		EveryChangedA2(v);
		TheRefusals(v);
		TheSealedChannel(all, v);
		TheRealDraws(v);
	}

	if (aChecksFailed > 0)
	{
		printf("%d check%s FAILED\n", aChecksFailed, aChecksFailed == 1 ? "" : "s");
		return 1;
	}

	printf("Every check passed\n");
	return 0;
}
