// The test of a relay's keys and their fingerprint (3dc/net_relay_keys.h). Written 2026-11-09 by Melanikus for the AvP port.
//
// WHAT IT TESTS:
// - the fingerprint of the relay keys in the relay's own test vectors (tests/vectors/crypto-v2.json, handshakeA.relayStatic) is all 48
//   bytes of SHA-384 over the words and the keys, and its first 16 are the vector the relay server's test was first held to;
// - one bit changed anywhere in the keys gives another fingerprint;
// - the check takes the right fingerprint and no other, in all 48 bytes;
// - the -dev setting is read as a region and 96 hex digits, and nothing else.
// It uses Windows' own SHA-384 (CNG), so it's built and run on GitHub only, as the other cryptography tests are.
#include "net_relay_keys.h"

#include "json_reader.h"

#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

// The fingerprint of handshakeA.relayStatic: SHA-384 over "AVPX relay keys v2", its P-384 key and its ML-KEM key (1,683 bytes), worked out
// apart from the game (with node's crypto, 2026-11-09). Its first 32 digits are Primary's vector from the server's own test.
static const char kVectorFingerprint[] = "fc352802066e547c261e37fe63f114863debbc76b5b29575ab69af80c72eeb91dd9a9dddd8b38218b21df00b4bea5a54";

static int checksFailed = 0;

static void Check(bool good, const std::string &what)
{
	printf("%s  %s\n", good ? "ok    " : "FAILED", what.c_str());
	checksFailed += good ? 0 : 1;
}

static bool AllZero(const uint8_t *bytes, size_t size)
{
	uint8_t any = 0;

	for (size_t i = 0; i < size; i++)
	{
		any |= bytes[i];
	}

	return any == 0;
}

static std::string HexOf(const uint8_t *bytes, size_t size)
{
	static const char kDigits[] = "0123456789abcdef";
	std::string text;

	for (size_t i = 0; i < size; i++)
	{
		text += kDigits[bytes[i] >> 4];
		text += kDigits[bytes[i] & 15];
	}

	return text;
}

static void TestTheFingerprint()
{
	printf("1. the fingerprint of the relay's keys in its own test vectors\n");
	JsonValue file;
	std::string const all = ReadRepositoryFile("tests/vectors/crypto-v2.json");
	JsonReader reader(all);
	bool const read = !all.empty() && reader.ReadAll(&file) && file.kind == kJsonObject;
	const JsonValue *const a = read ? file.Member("handshakeA") : NULL;
	const JsonValue *const relay = a ? a->Member("relayStatic") : NULL;
	const JsonValue *const p384 = relay ? TextMember(*relay, "p384Public") : NULL;
	const JsonValue *const kem = relay ? TextMember(*relay, "mlkemPublic") : NULL;
	std::vector<uint8_t> p384Bytes, kemBytes;
	bool const have = p384 && kem && FromHex(p384->text, &p384Bytes) && FromHex(kem->text, &kemBytes) && p384Bytes.size() == kNetCryptoP384PublicBytes
		&& kemBytes.size() == kNetPqKemPublicBytes;
	Check(have, "the vectors' relay has a P-384 key of 97 bytes and an ML-KEM key of 1,568");

	if (!have)
	{
		return;
	}

	NetRelayStaticKeys keys;
	memcpy(keys.p384, p384Bytes.data(), sizeof(keys.p384));
	memcpy(keys.kem, kemBytes.data(), sizeof(keys.kem));
	uint8_t fingerprint[kNetRelayKeysFingerprintBytes];
	bool const made = NetRelayKeys_Fingerprint(&keys, fingerprint);
	std::string const hex = HexOf(fingerprint, sizeof(fingerprint));
	Check(made && hex == kVectorFingerprint, "its fingerprint is all 48 bytes of SHA-384 over the words and the keys (it's " + hex + ")");
	Check(hex.compare(0, 32, "fc352802066e547c261e37fe63f11486") == 0, "and its first 16 bytes are the vector the server's test was first held to");
	Check(NetRelayKeys_AreThese(&keys, fingerprint), "the check takes those keys for that fingerprint");

	// One bit changed anywhere in the keys: another fingerprint, which the check refuses
	size_t const places[] = { 0, 1, 96, kNetCryptoP384PublicBytes, kNetCryptoP384PublicBytes + 1, kNetCryptoP384PublicBytes + kNetPqKemPublicBytes - 1 };
	bool allRefused = true;

	for (size_t const place : places)
	{
		uint8_t *const byte = place < kNetCryptoP384PublicBytes ? &keys.p384[place] : &keys.kem[place - kNetCryptoP384PublicBytes];
		*byte ^= 0x01;
		allRefused = allRefused && !NetRelayKeys_AreThese(&keys, fingerprint);
		*byte ^= 0x01;
	}

	Check(allRefused, "keys with one bit changed, at the first, second and last byte of each key, aren't taken for that fingerprint");

	// A fingerprint one bit away, in any of its 48 bytes, isn't theirs: the whole of it is compared
	bool otherRefused = true;

	for (size_t i = 0; i < kNetRelayKeysFingerprintBytes; i++)
	{
		fingerprint[i] ^= 0x80;
		otherRefused = otherRefused && !NetRelayKeys_AreThese(&keys, fingerprint);
		fingerprint[i] ^= 0x80;
	}

	Check(otherRefused && NetRelayKeys_AreThese(&keys, fingerprint), "nor are the keys taken for a fingerprint one bit away from theirs, in any of its 48 bytes");
	Check(!NetRelayKeys_AreThese(NULL, fingerprint) && !NetRelayKeys_AreThese(&keys, NULL), "and no keys, or no fingerprint, is never a match");
}

static void TestTheSetting()
{
	printf("2. the -dev setting, RelayKey=<region>:<96 hex digits>\n");
	char region[kRelayListRegionLetters + 1];
	uint8_t fingerprint[kNetRelayKeysFingerprintBytes];
	std::string const good = kVectorFingerprint;
	bool const read = NetRelayKeys_ReadSetting(("newyork:" + good).c_str(), region, fingerprint);
	Check(read && strcmp(region, "newyork") == 0 && HexOf(fingerprint, sizeof(fingerprint)) == good, "a region and its relay's fingerprint are read");
	Check(NetRelayKeys_ReadSetting(("a:" + std::string(95, '0') + "1").c_str(), region, fingerprint) && strcmp(region, "a") == 0
		&& fingerprint[kNetRelayKeysFingerprintBytes - 1] == 1, "a region of one letter, and the last digit counts");
	Check(NetRelayKeys_ReadSetting(("sixteen-letters1:" + good).c_str(), region, fingerprint) && strcmp(region, "sixteen-letters1") == 0,
		"and one of 16, the most there is");

	std::string capitals = good;

	for (char &c : capitals)
	{
		c = (c >= 'a' && c <= 'f') ? static_cast<char>(c - 'a' + 'A') : c;
	}

	std::string const bad[] = {
		"",
		"newyork",
		":" + good,
		"newyork:",
		"New York:" + good,
		"newyork:" + capitals,
		"newyork:" + good + "0",
		"newyork:" + good.substr(1),
		"newyork:" + good.substr(0, 32),
		"newyork:" + good + " ",
		" newyork:" + good,
		"seventeen-letters:" + good,
		"newyork:" + good.substr(0, 95) + "g",
		"newyork::" + good,
	};
	bool allRefused = true;

	for (std::string const &setting : bad)
	{
		memset(region, 'x', sizeof(region));
		memset(fingerprint, 0xA5, sizeof(fingerprint));
		allRefused = allRefused && !NetRelayKeys_ReadSetting(setting.c_str(), region, fingerprint) && region[0] == '\0' && AllZero(fingerprint, sizeof(fingerprint));
	}

	memset(region, 'x', sizeof(region));
	allRefused = allRefused && !NetRelayKeys_ReadSetting(NULL, region, fingerprint) && region[0] == '\0';
	Check(allRefused, "nothing else is taken: no region, no digits, capitals, a digit too many or too few, the short 32 of before, a space, a "
		"region too long or not one, a letter that isn't a digit, two colons, no setting; and each leaves no region and a fingerprint of 0s");

	// A setting with no end in sight is read no further than the longest a setting could be
	char noEnd[kNetRelayKeysMostSettingLetters + 8];
	memset(noEnd, 'a', sizeof(noEnd));
	noEnd[sizeof(noEnd) - 1] = '\0';
	Check(!NetRelayKeys_ReadSetting(noEnd, region, fingerprint) && region[0] == '\0', "a setting longer than any can be is refused");
}

int main()
{
	printf("A relay's keys and their fingerprint\n");
	TestTheFingerprint();
	TestTheSetting();

	if (checksFailed > 0)
	{
		printf("%d check%s FAILED\n", checksFailed, checksFailed == 1 ? "" : "s");
		return 1;
	}

	printf("Every check passed\n");
	return 0;
}
