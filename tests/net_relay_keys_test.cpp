// The test of a relay's keys and their fingerprint (3dc/net_relay_keys.h). Written 2026-11-09 by Melanikus for the AvP port.
//
// WHAT IT TESTS:
// - the fingerprint of the relay keys in the relay's own test vectors (tests/vectors/crypto-v2.json, handshakeA.relayStatic) is the one
//   the relay server's test is held to, fc352802066e547c261e37fe63f11486;
// - one bit changed anywhere in the keys gives another fingerprint;
// - the check takes the right fingerprint and no other;
// - the -dev setting is read as a region and 32 hex digits, and nothing else.
// It uses Windows' own SHA-384 (CNG), so it's built and run on GitHub only, as the other cryptography tests are.
#include "net_relay_keys.h"

#include "json_reader.h"

#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

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
	char hex[kNetIdentityFingerprintHexBytes];
	bool const made = NetRelayKeys_Fingerprint(&keys, fingerprint);
	NetIdentity_FingerprintHex(fingerprint, hex);
	Check(made && strcmp(hex, "fc352802066e547c261e37fe63f11486") == 0,
		std::string("its fingerprint is fc352802066e547c261e37fe63f11486, the one the server's test is held to (it's ") + hex + ")");
	Check(NetRelayKeys_AreThese(&keys, fingerprint), "and the check takes those keys for that fingerprint");

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

	bool otherRefused = true;

	for (size_t i = 0; i < kNetRelayKeysFingerprintBytes; i++)
	{
		fingerprint[i] ^= 0x80;
		otherRefused = otherRefused && !NetRelayKeys_AreThese(&keys, fingerprint);
		fingerprint[i] ^= 0x80;
	}

	Check(otherRefused && NetRelayKeys_AreThese(&keys, fingerprint), "nor are the keys taken for a fingerprint one bit away from theirs, in any of its 16 bytes");
	Check(!NetRelayKeys_AreThese(NULL, fingerprint) && !NetRelayKeys_AreThese(&keys, NULL), "and no keys, or no fingerprint, is never a match");
}

static void TestTheSetting()
{
	printf("2. the -dev setting, RelayKey=<region>:<32 hex digits>\n");
	char region[kRelayListRegionLetters + 1];
	uint8_t fingerprint[kNetRelayKeysFingerprintBytes];
	char hex[kNetIdentityFingerprintHexBytes];
	bool const read = NetRelayKeys_ReadSetting("newyork:fc352802066e547c261e37fe63f11486", region, fingerprint);
	NetIdentity_FingerprintHex(fingerprint, hex);
	Check(read && strcmp(region, "newyork") == 0 && strcmp(hex, "fc352802066e547c261e37fe63f11486") == 0, "a region and its relay's fingerprint are read");
	Check(NetRelayKeys_ReadSetting("a:00000000000000000000000000000001", region, fingerprint) && strcmp(region, "a") == 0 && fingerprint[15] == 1,
		"a region of one letter");
	Check(NetRelayKeys_ReadSetting("sixteen-letters1:fc352802066e547c261e37fe63f11486", region, fingerprint) && strcmp(region, "sixteen-letters1") == 0,
		"and one of 16, the most there is");

	const char *const bad[] = {
		"",
		"newyork",
		":fc352802066e547c261e37fe63f11486",
		"newyork:",
		"New York:fc352802066e547c261e37fe63f11486",
		"newyork:FC352802066E547C261E37FE63F11486",
		"newyork:fc352802066e547c261e37fe63f114860",
		"newyork:fc352802066e547c261e37fe63f1148",
		"newyork:fc352802066e547c261e37fe63f11486 ",
		" newyork:fc352802066e547c261e37fe63f11486",
		"seventeen-letters:fc352802066e547c261e37fe63f11486",
		"newyork:fc352802066e547c261e37fe63f1148g",
		"newyork::fc352802066e547c261e37fe63f11486",
		NULL
	};
	bool allRefused = true;

	for (const char *const setting : bad)
	{
		memset(region, 'x', sizeof(region));
		memset(fingerprint, 0xA5, sizeof(fingerprint));
		allRefused = allRefused && !NetRelayKeys_ReadSetting(setting, region, fingerprint) && region[0] == '\0' && AllZero(fingerprint, sizeof(fingerprint));
	}

	Check(allRefused, "nothing else is taken: no region, no digits, capitals, a digit too many or too few, a space, a region too long or not one, "
		"a letter that isn't a digit, two colons, no setting; and each leaves no region and a fingerprint of 0s");

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
