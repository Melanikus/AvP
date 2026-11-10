// Tests of protocol version 2's shared parts, the ones both handshakes and both sealed channels use: 3dc/net_v2_fragments.h (the 1,200-byte
// handshake datagrams and the cookie round), 3dc/net_v2_replay.h (the replay window) and 3dc/net_v2_direction.h (one direction's keys by
// epoch, and its nonces). Written 2026-11-09 by Melanikus for the AvP port.
//
// Against the relay's own bytes first: tests/vectors/crypto-v2.json, made by the relay's Java (tests/vectors/README.md). Its handshake A
// cut into datagrams must be the very datagrams the Java cut, and put back together to the very message; its nonces and epoch keys must
// come out the same; its refused datagrams must be refused. Then everything the file can't show: every way a fragment can be wrong, the
// window's every edge (the largest counter there is among them), and an epoch past what 4 bytes can name.

#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

#include "net_crypto.h"
#include "net_v2_fragments.h"
#include "net_v2_replay.h"
#include "net_v2_direction.h"
#include "json_reader.h"

static int partsChecksFailed = 0;

// Called PartsCheck, not Check: Snyk Code joins functions with the same name and signature across files, and many tests have a Check
static void PartsCheck(bool passed, const std::string &what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what.c_str());
	fflush(stdout);

	if (!passed)
	{
		partsChecksFailed++;
	}
}

typedef std::vector<uint8_t> Bytes;

// A field of the file, as bytes from its hex: empty if it isn't there or isn't hex, which the checks that use it then fail on
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

static bool AllZero(const uint8_t *bytes, size_t size)
{
	return std::all_of(bytes, bytes + size, [](uint8_t x) { return x == 0; });
}

// ----- The fragments, against the relay's own datagrams

static void TheDatagrams(const JsonValue &all)
{
	const JsonValue *const a = all.Member("handshakeA");
	const JsonValue *const d = all.Member("handshakeDatagrams");
	Bytes const id = HexOf(d, "id"), cookie = HexOf(d, "cookie");
	PartsCheck(id.size() == kNetV2IdBytes && cookie.size() == kNetV2CookieBytes, "the file's handshake id (8 bytes) and cookie (16 bytes)");

	if (id.size() != kNetV2IdBytes || cookie.size() != kNetV2CookieBytes)
	{
		return;
	}

	static const uint8_t kNoCookie[kNetV2CookieBytes] = {};

	struct Message
	{
		const char *name;
		uint8_t kind;
		const uint8_t *cookie;
		size_t count;
	};

	Message const messages[3] = { { "a1", kNetV2KindA1, cookie.data(), 5 }, { "a2", kNetV2KindA2, kNoCookie, 3 }, { "a3", kNetV2KindA3, cookie.data(), 1 } };

	for (const Message &m : messages)
	{
		Bytes const message = HexOf(a, m.name);
		const JsonValue *const list = d ? d->Member(m.name) : NULL;
		bool const shaped = !message.empty() && list && list->kind == kJsonList && list->items.size() == m.count;
		PartsCheck(shaped && NetV2_FragmentCount(message.size()) == m.count,
			std::string(m.name) + ": " + std::to_string(message.size()) + " bytes, " + std::to_string(m.count) + " datagrams in the file, as many as the rule says");

		if (!shaped)
		{
			continue;
		}

		NetV2Assembly assembly;
		NetV2_AssemblyClear(&assembly);
		bool allSame = true, allRead = true, allAdded = true;

		// Put back together from the last datagram to the first, so the order they come in is shown not to matter
		for (size_t k = 0; k < m.count; k++)
		{
			uint8_t made[kNetV2DatagramBytes];
			Bytes theirs;
			allSame = allSame && FromHex(list->items[k].text, &theirs) &&
				NetV2_CutFragment(kNetV2MagicRelay, m.kind, id.data(), m.cookie, message.data(), message.size(), k, made) && Same(theirs, made, sizeof(made));

			size_t const back = m.count - 1 - k;
			Bytes datagram;
			NetV2Fragment f;
			bool const read = FromHex(list->items[back].text, &datagram) && NetV2_ReadFragment(kNetV2MagicRelay, datagram.data(), datagram.size(), &f) &&
				f.kind == m.kind && f.index == back && f.count == m.count && f.length == message.size() && memcmp(f.id, id.data(), kNetV2IdBytes) == 0 &&
				memcmp(f.cookie, m.cookie, kNetV2CookieBytes) == 0;
			allRead = allRead && read;
			allAdded = allAdded && read && NetV2_AssemblyAdd(&assembly, &f);
		}

		PartsCheck(allSame, std::string(m.name) + ": cut here, each datagram is the relay's own, byte for byte");
		PartsCheck(allRead, std::string(m.name) + ": each of the relay's datagrams reads back as its fragment: kind, number, count, length, id and cookie");
		PartsCheck(allAdded && NetV2_AssemblyComplete(&assembly) && assembly.length == message.size() && Same(message, assembly.message, assembly.length),
			std::string(m.name) + ": put back together, from the last datagram to the first, it's the message exactly");
	}
}

// ----- The cookie round: A0 and A0'

static void TheCookieRound(const JsonValue &all)
{
	const JsonValue *const d = all.Member("handshakeDatagrams");
	Bytes const id = HexOf(d, "id"), cookie = HexOf(d, "cookie");

	if (id.size() != kNetV2IdBytes || cookie.size() != kNetV2CookieBytes)
	{
		PartsCheck(false, "the cookie round needs the file's id and cookie");
		return;
	}

	// A0: fragment 0 of 1 of a message of no bytes, with a zero cookie, padded with zeros (5.1 point 9), byte by byte
	uint8_t ask[kNetV2DatagramBytes];
	uint8_t want[kNetV2DatagramBytes] = {};
	memcpy(want, kNetV2MagicRelay, 4);
	want[4] = 2;
	want[5] = kNetV2KindCookieAsk;
	want[6] = 0;
	want[7] = 1;
	memcpy(want + 8, id.data(), kNetV2IdBytes);
	PartsCheck(NetV2_CookieAsk(id.data(), ask) && memcmp(ask, want, sizeof(want)) == 0,
		"A0, the COOKIE_ASK: 1,200 bytes, AVPR 2 kind 1, fragment 0 of 1, the id, a zero cookie, length 0 and nothing after");

	NetV2Fragment f;
	PartsCheck(NetV2_ReadFragment(kNetV2MagicRelay, ask, sizeof(ask), &f) && f.length == 0 && f.count == 1 && f.partBytes == 0,
		"and it reads back as one fragment of a message of no bytes");

	// A0': 'AVPR', 2, COOKIE, 0, 0, the id, the cookie, then 32 zeros
	uint8_t answer[kNetV2CookieAnswerBytes] = {};
	memcpy(answer, kNetV2MagicRelay, 4);
	answer[4] = 2;
	answer[5] = kNetV2KindCookie;
	memcpy(answer + 8, id.data(), kNetV2IdBytes);
	memcpy(answer + 16, cookie.data(), kNetV2CookieBytes);
	uint8_t got[kNetV2CookieBytes];
	PartsCheck(NetV2_ReadCookie(answer, sizeof(answer), id.data(), got) && memcmp(got, cookie.data(), kNetV2CookieBytes) == 0,
		"A0': the relay's 64-byte answer gives the cookie");

	// Every byte but the cookie's own is fixed: change any one, and it isn't an answer to us
	bool everyByteChecked = true;

	for (size_t i = 0; i < sizeof(answer); i++)
	{
		if (i >= 16 && i < 32)
		{
			continue;
		}

		uint8_t changed[kNetV2CookieAnswerBytes];
		memcpy(changed, answer, sizeof(changed));
		changed[i] ^= 0x01;
		memset(got, 0x5A, sizeof(got));
		everyByteChecked = everyByteChecked && !NetV2_ReadCookie(changed, sizeof(changed), id.data(), got) && AllZero(got, sizeof(got));
	}

	PartsCheck(everyByteChecked, "A0' with any of its 48 fixed bytes changed (the magic, version, kind, zeros, our id) is refused, the cookie wiped");
	PartsCheck(!NetV2_ReadCookie(answer, sizeof(answer) - 1, id.data(), got) && !NetV2_ReadCookie(answer, sizeof(answer) + 0, NULL, got) &&
		!NetV2_ReadCookie(NULL, sizeof(answer), id.data(), got), "A0' of 63 bytes, or with nothing to read or no id to match, is refused");

	uint8_t longer[kNetV2CookieAnswerBytes + 1] = {};
	memcpy(longer, answer, sizeof(answer));
	PartsCheck(!NetV2_ReadCookie(longer, sizeof(longer), id.data(), got), "and so is one of 65 bytes");
}

// ----- What the relay refuses, the game refuses too: the file's refused datagrams

static void TheRefusals(const JsonValue &all)
{
	const JsonValue *const list = all.Member("refusals");
	PartsCheck(list && list->kind == kJsonList && list->items.size() == 12, "the file's 12 refusals are there");

	if (!list || list->kind != kJsonList)
	{
		return;
	}

	int datagrams = 0;

	for (const JsonValue &r : list->items)
	{
		const JsonValue *const number = r.Member("refusal");
		const JsonValue *const what = TextMember(r, "what");
		Bytes const bytes = HexOf(&r, "bytes");

		if (!number || number->kind != kJsonNumber || !what)
		{
			PartsCheck(false, "a refusal without its number or its words");
			continue;
		}

		// The sealed control messages' (104 bytes, and the one cut to 103) are net_v2a_test's: they need the session's keys
		if (bytes.size() == 104 || bytes.size() == 103)
		{
			continue;
		}

		datagrams++;
		NetV2Fragment f;
		bool const read = NetV2_ReadFragment(kNetV2MagicRelay, bytes.data(), bytes.size(), &f);

		if (number->number == 4)
		{
			// An unknown kind is still a well-made fragment: it's refused by whoever expects a kind (the handshake), as kind 9 is none
			PartsCheck(read && f.kind == 9 && f.kind != kNetV2KindA2 && f.kind != kNetV2KindCookie,
				"refusal 4, " + what->text + ": it reads as a fragment of kind 9, which no step of handshake A takes");
		}
		else
		{
			PartsCheck(!read && AllZero(reinterpret_cast<const uint8_t *>(&f), sizeof(f)), "refusal " + std::to_string(number->number) + ", " + what->text + ": refused");
		}
	}

	// 8 datagrams (refusals 1, 2, 3, 4, and four of 6); the other 4 are sealed control messages (1, 8, 8, 9)
	PartsCheck(datagrams == 8, std::to_string(datagrams) + " of the 12 are handshake datagrams, all tried here");
}

// ----- Every other way a fragment can be wrong

static void TheFragmentRules()
{
	static const uint8_t kId[kNetV2IdBytes] = { 1, 2, 3, 4, 5, 6, 7, 8 };
	static const uint8_t kCookie[kNetV2CookieBytes] = { 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9 };
	std::vector<uint8_t> message(3249);

	for (size_t i = 0; i < message.size(); i++)
	{
		message[i] = static_cast<uint8_t>(i * 7 + 1);
	}

	uint8_t d[kNetV2DatagramBytes];
	NetV2Fragment f;
	PartsCheck(NetV2_CutFragment(kNetV2MagicRelay, kNetV2KindA1, kId, kCookie, message.data(), message.size(), 2, d) &&
		NetV2_ReadFragment(kNetV2MagicRelay, d, sizeof(d), &f) && f.partBytes == 3249 - 2 * 1164, "the last of 3 fragments carries what's left: 921 bytes");

	// Each header byte changed alone, where it can't still be read one honest way
	struct Change
	{
		size_t at;
		uint8_t to;
		const char *what;
	};

	Change const changes[] = {
		{ 0, 'X', "another magic" }, { 4, 3, "version 3" }, { 4, 1, "version 1" }, { 6, 3, "fragment 3 of 3" }, { 6, 4, "fragment 4 of 3" },
		{ 7, 0, "a count of 0" }, { 7, 5, "a count of 5" }, { 7, 6, "a count of 6, past the most" }, { 7, 2, "a count of 2 for 3,249 bytes" }, { 7, 4, "a count of 4 for 3,249 bytes" },
		{ 32, 0x31, "a length past the most (0x1231, 4,657)" }, { 33, 0xFF, "a length of 65,000 or so" }, { 34, 0x00, "fragment 2 starting at 0" },
		{ 35, 0x00, "fragment 2 starting at 0x48 (72)" },
	};

	for (const Change &c : changes)
	{
		uint8_t changed[kNetV2DatagramBytes];
		memcpy(changed, d, sizeof(changed));
		changed[c.at] = c.to;
		PartsCheck(!NetV2_ReadFragment(kNetV2MagicRelay, changed, sizeof(changed), &f), std::string("refused: ") + c.what);
	}

	uint8_t tail[kNetV2DatagramBytes];
	memcpy(tail, d, sizeof(tail));
	tail[kNetV2DatagramBytes - 1] = 1;
	PartsCheck(!NetV2_ReadFragment(kNetV2MagicRelay, tail, sizeof(tail), &f), "refused: a byte that isn't 0 after the last fragment's part (the very last byte)");
	PartsCheck(!NetV2_ReadFragment(kNetV2MagicEndToEnd, d, sizeof(d), &f), "refused: AVPR's datagram read as AVPE's (the magics never mix)");
	PartsCheck(!NetV2_ReadFragment(kNetV2MagicRelay, d, sizeof(d) - 1, &f) && !NetV2_ReadFragment(kNetV2MagicRelay, NULL, sizeof(d), &f),
		"refused: 1,199 bytes, or none at all");

	// What the protocol can't send isn't cut
	uint8_t out[kNetV2DatagramBytes];
	std::vector<uint8_t> tooLong(kNetV2MostMessageBytes + 1, 0x11);
	PartsCheck(!NetV2_CutFragment(kNetV2MagicRelay, kNetV2KindA1, kId, kCookie, tooLong.data(), tooLong.size(), 0, out) && AllZero(out, sizeof(out)),
		"a message of 5,821 bytes isn't cut, and nothing is left in the datagram");
	PartsCheck(!NetV2_CutFragment(kNetV2MagicRelay, kNetV2KindA1, kId, kCookie, message.data(), message.size(), 3, out),
		"nor fragment 3 of a message that has 3");
	PartsCheck(NetV2_CutFragment(kNetV2MagicRelay, kNetV2KindA1, kId, kCookie, tooLong.data(), kNetV2MostMessageBytes, 4, out),
		"5,820 bytes, the most, is 5 fragments (pqIK's A1 needs 5)");

	// Putting together: a fragment that doesn't belong changes nothing
	uint8_t d0[kNetV2DatagramBytes], d1[kNetV2DatagramBytes], other[kNetV2DatagramBytes];
	NetV2Fragment f0, f1, g;
	NetV2_CutFragment(kNetV2MagicRelay, kNetV2KindA1, kId, kCookie, message.data(), message.size(), 0, d0);
	NetV2_CutFragment(kNetV2MagicRelay, kNetV2KindA1, kId, kCookie, message.data(), message.size(), 1, d1);
	NetV2_ReadFragment(kNetV2MagicRelay, d0, sizeof(d0), &f0);
	NetV2_ReadFragment(kNetV2MagicRelay, d1, sizeof(d1), &f1);
	NetV2Assembly a;
	NetV2_AssemblyClear(&a);
	PartsCheck(NetV2_AssemblyAdd(&a, &f0) && NetV2_AssemblyAdd(&a, &f0) && !NetV2_AssemblyComplete(&a), "a fragment that comes twice, the same, is fine");

	static const uint8_t kOtherId[kNetV2IdBytes] = { 8, 7, 6, 5, 4, 3, 2, 1 };
	NetV2_CutFragment(kNetV2MagicRelay, kNetV2KindA1, kOtherId, kCookie, message.data(), message.size(), 1, other);
	NetV2_ReadFragment(kNetV2MagicRelay, other, sizeof(other), &g);
	PartsCheck(!NetV2_AssemblyAdd(&a, &g), "a fragment of another handshake's id doesn't belong");
	NetV2_CutFragment(kNetV2MagicRelay, kNetV2KindA2, kId, kCookie, message.data(), message.size(), 1, other);
	NetV2_ReadFragment(kNetV2MagicRelay, other, sizeof(other), &g);
	PartsCheck(!NetV2_AssemblyAdd(&a, &g), "nor one of another kind");
	NetV2_CutFragment(kNetV2MagicRelay, kNetV2KindA1, kId, kCookie, message.data(), message.size() - 1, 1, other);
	NetV2_ReadFragment(kNetV2MagicRelay, other, sizeof(other), &g);
	PartsCheck(!NetV2_AssemblyAdd(&a, &g), "nor one of a message one byte shorter");

	std::vector<uint8_t> changedMessage = message;
	changedMessage[0] ^= 0x01;
	NetV2_CutFragment(kNetV2MagicRelay, kNetV2KindA1, kId, kCookie, changedMessage.data(), changedMessage.size(), 0, other);
	NetV2_ReadFragment(kNetV2MagicRelay, other, sizeof(other), &g);
	PartsCheck(!NetV2_AssemblyAdd(&a, &g) && a.haveCount == 1 && a.message[0] == message[0], "fragment 0 again with other bytes is refused, and the first kept");
	PartsCheck(NetV2_AssemblyAdd(&a, &f1) && !NetV2_AssemblyComplete(&a), "fragment 1 is taken; 2 of 3 isn't the whole message");

	// A fragment made by hand, past what ReadFragment would ever give, can't write past the message
	NetV2Fragment bad = f1;
	bad.start = kNetV2MostMessageBytes - 10;
	PartsCheck(!NetV2_AssemblyAdd(&a, &bad), "a fragment made by hand to start anywhere else is refused, not copied");
	// Modified 2026-11-10 by Melanikus for the AvP port (Take-outs, #191): one claiming more than 1,164 bytes goes to an assembly of its own,
	// with a part that really holds 1,165, so its size is the one thing that can refuse it. Given to the assembly above, which has fragment
	// 1 already, it would also be refused as fragment 1 come again with other bytes, and the size rule could go unseen.
	NetV2Assembly fresh;
	NetV2_AssemblyClear(&fresh);
	std::vector<uint8_t> tooMuch(kNetV2PartBytes + 1, 0x33);
	bad = f1;
	bad.part = tooMuch.data();
	bad.partBytes = tooMuch.size();
	PartsCheck(!NetV2_AssemblyAdd(&fresh, &bad) && !fresh.started && fresh.haveCount == 0,
		"and so is one claiming more than 1,164 bytes, with nothing taken");
}

// ----- The replay window

static void TheReplayWindow()
{
	NetV2Replay r;
	NetV2_ReplayClear(&r);
	PartsCheck(NetV2_ReplayIsNew(&r, 0) && NetV2_ReplayIsNew(&r, 5000), "before anything, every counter is new");
	NetV2_ReplayTake(&r, 0);
	PartsCheck(!NetV2_ReplayIsNew(&r, 0) && NetV2_ReplayIsNew(&r, 1), "0 taken: 0 again is refused, 1 is new");
	NetV2_ReplayTake(&r, 10);
	PartsCheck(NetV2_ReplayIsNew(&r, 5) && !NetV2_ReplayIsNew(&r, 10) && !NetV2_ReplayIsNew(&r, 0), "10 taken: 5, late, is still new; 10 and 0 aren't");
	NetV2_ReplayTake(&r, 5);
	PartsCheck(!NetV2_ReplayIsNew(&r, 5), "5 taken late: now it's refused too");
	NetV2_ReplayTake(&r, 10 + 1023);
	PartsCheck(NetV2_ReplayIsNew(&r, 11) && !NetV2_ReplayIsNew(&r, 10), "at 1,033, 11 is the oldest the window holds, and 10 has fallen out of it");
	NetV2_ReplayTake(&r, 1033 + 5000);
	PartsCheck(!NetV2_ReplayIsNew(&r, 1033) && NetV2_ReplayIsNew(&r, 6033 - 1023) && !NetV2_ReplayIsNew(&r, 6033 - 1024),
		"a jump of 5,000: the whole window is new again from 5,010 up, and nothing below it");

	// The very top: the steps are counted, so the window moves to 2^64 - 1 and the call comes back
	NetV2_ReplayClear(&r);
	NetV2_ReplayTake(&r, UINT64_MAX - 5);
	NetV2_ReplayTake(&r, UINT64_MAX);
	PartsCheck(!NetV2_ReplayIsNew(&r, UINT64_MAX) && NetV2_ReplayIsNew(&r, UINT64_MAX - 1) && !NetV2_ReplayIsNew(&r, UINT64_MAX - 5),
		"2^64 - 1, 5 past the last: it's taken and the call returns; between them is still new");
}

// ----- One direction: nonces and epoch keys, against the relay's own

static void TheDirection(const JsonValue &all)
{
	const JsonValue *const nonces = all.Member("nonces");
	const JsonValue *const epochs = all.Member("epochKeys");
	// Modified 2026-11-09 by Melanikus for the AvP port (CRYPTO.md 5.1 point 3, the relay's file at 2b8dcd6): each epoch's key comes from a
	// 48-byte secret, and epoch 0 is in the file too, since it's made like the others
	PartsCheck(nonces && nonces->kind == kJsonList && nonces->items.size() == 6 && epochs && epochs->kind == kJsonList && epochs->items.size() == 4,
		"the file's 6 nonces and 4 epoch keys (0, 1, 2 and 4095) are there");

	if (!nonces || !epochs || nonces->kind != kJsonList || epochs->kind != kJsonList)
	{
		return;
	}

	static const uint8_t kSecret[kNetNoiseSecretBytes] = { 0x42 };
	int nonceRight = 0;

	for (const JsonValue &n : nonces->items)
	{
		Bytes const iv = HexOf(&n, "iv"), want = HexOf(&n, "nonce");
		const JsonValue *const counter = n.Member("n");
		NetV2Direction d;
		uint8_t key[kNetCryptoAesKeyBytes], nonce[kNetCryptoNonceBytes];

		if (iv.size() == kNetCryptoNonceBytes && counter && counter->kind == kJsonNumber)
		{
			NetV2_DirectionStart(&d, kSecret, iv.data());
			nonceRight += NetV2_DirectionFor(&d, counter->number, key, nonce) && Same(want, nonce, sizeof(nonce)) ? 1 : 0;
			NetV2_DirectionWipe(&d);
		}
	}

	PartsCheck(nonceRight == 6, std::to_string(nonceRight) + " of 6 nonces are the relay's: the IV, its last 8 bytes XORed with the counter, little-endian");

	int epochRight = 0;

	for (const JsonValue &e : epochs->items)
	{
		Bytes const secret = HexOf(&e, "secret"), want = HexOf(&e, "epochKey");
		const JsonValue *const epoch = e.Member("epoch");
		static const uint8_t kIv[kNetCryptoNonceBytes] = {};
		NetV2Direction d;
		uint8_t key[kNetCryptoAesKeyBytes], nonce[kNetCryptoNonceBytes];

		if (secret.size() == kNetNoiseSecretBytes && epoch && epoch->kind == kJsonNumber)
		{
			NetV2_DirectionStart(&d, secret.data(), kIv);
			uint64_t const first = epoch->number << kNetV2EpochShift;
			uint8_t lastKey[kNetCryptoAesKeyBytes];
			// Epoch 0 has no epoch before it, so its "another just before" is the next epoch's first key instead
			uint64_t const other = first == 0 ? first + (1u << kNetV2EpochShift) : first - 1;
			bool const right = NetV2_DirectionFor(&d, first, key, nonce) && Same(want, key, sizeof(key)) &&
				NetV2_DirectionFor(&d, first + (1u << kNetV2EpochShift) - 1, lastKey, nonce) && memcmp(key, lastKey, sizeof(key)) == 0 &&
				NetV2_DirectionFor(&d, other, lastKey, nonce) && memcmp(key, lastKey, sizeof(key)) != 0;
			epochRight += right ? 1 : 0;
			NetV2_DirectionWipe(&d);
		}
	}

	PartsCheck(epochRight == 4, std::to_string(epochRight) + " of 4 epoch keys are the relay's, the same key all through its epoch, another next to it");

	NetV2Direction d;
	uint8_t key[kNetCryptoAesKeyBytes], nonce[kNetCryptoNonceBytes];
	static const uint8_t kIv[kNetCryptoNonceBytes] = {};
	NetV2_DirectionStart(&d, kSecret, kIv);
	PartsCheck(NetV2_DirectionFor(&d, 0, key, nonce) && memcmp(key, kSecret, sizeof(key)) != 0,
		"epoch 0's key is made from the secret, and isn't the secret's own bytes: nothing seals with the secret itself");

	memset(key, 0x5A, sizeof(key));
	memset(nonce, 0x5A, sizeof(nonce));
	PartsCheck(!NetV2_DirectionFor(&d, (static_cast<uint64_t>(UINT32_MAX) + 1) << kNetV2EpochShift, key, nonce) && AllZero(key, sizeof(key)) &&
		AllZero(nonce, sizeof(nonce)), "an epoch past what 4 bytes can name is refused, with the key and the nonce wiped");

	NetV2_DirectionWipe(&d);
	PartsCheck(AllZero(reinterpret_cast<const uint8_t *>(&d), sizeof(d)), "wiping leaves nothing of the keys");
}

int main()
{
	std::string const text = ReadRepositoryFile("tests/vectors/crypto-v2.json");
	JsonReader reader(text);
	JsonValue all;
	bool const read = !text.empty() && reader.ReadAll(&all) && all.kind == kJsonObject;
	PartsCheck(read, "tests/vectors/crypto-v2.json: read");

	if (read)
	{
		TheDatagrams(all);
		TheCookieRound(all);
		TheRefusals(all);
		TheDirection(all);
	}

	TheFragmentRules();
	TheReplayWindow();

	if (partsChecksFailed > 0)
	{
		printf("%d check%s FAILED\n", partsChecksFailed, partsChecksFailed == 1 ? "" : "s");
		return 1;
	}

	printf("Every check passed\n");
	return 0;
}
