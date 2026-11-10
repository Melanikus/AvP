// The test of a relay's keys and their fingerprint (3dc/net_relay_keys.h). Written 2026-11-09 by Melanikus for the AvP port.
//
// WHAT IT TESTS:
// - the fingerprint of the relay keys in the relay's own test vectors (tests/vectors/crypto-v2.json, handshakeA.relayStatic) is all 48
//   bytes of SHA-384 over the words and the keys, and its first 16 are the vector the relay server's test was first held to;
// - one bit changed anywhere in the keys gives another fingerprint;
// - the check takes the right fingerprint and no other, in all 48 bytes;
// - the -dev setting is read as a region and 96 hex digits, and nothing else;
// - (added 2026-11-10 by Melanikus for the AvP port, #175) a server's servers.cfg fingerprint (3dc/net_server_fingerprint.h): nyc3's known
//   answer, the same list key with the vectors' relay, and that a change to any byte of the list's key, the relay's keys or the line's
//   number is refused.
// It uses Windows' own SHA-384 and SHA-256 (CNG), so it's built and run on GitHub only, as the other cryptography tests are.
#include "net_relay_keys.h"
#include "net_server_fingerprint.h"

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

// Added 2026-11-10 by Melanikus for the AvP port (#175): a server's servers.cfg fingerprint. The known answers were worked out three ways
// apart from the game: Node's crypto, Windows' certutil over the same bytes, and the server's own Java (ServersLine, Primary, 2026-11-10).
static const char kNyc3ListKey[] = "3059301306072a8648ce3d020106082a8648ce3d030107034200046860ab37215b80bc818041605b4dc15b0f553ff8e0294ef67e0145059ab4fe9fb670e3b138d4fbef7c9a82ae095b32dc647e20fc0840c56c1edf1402f51515f8";
static const char kNyc3RelayId[] = "d4afea79e806fa341d350643a54b6964e532c591c0544be9ce93db4c752b9d89f668f026ffeb75a64f5049082499e445";
static const char kNyc3Fingerprint[] = "bb0e2958dade79033a90f34811c84837b404708cc004c5b1d37d9a26f28d663e";
static const char kNyc3KeyWithVectorRelay[] = "6f15c487060a467449128e470ae9f4ba5649b4e9598f42aeaea450fab880df30";

static void TestTheServersCfgFingerprint()
{
	printf("3. a server's servers.cfg fingerprint\n");
	std::vector<uint8_t> listKey, relayId, nyc3, withVector;
	bool const have = FromHex(kNyc3ListKey, &listKey) && listKey.size() == 91 && FromHex(kNyc3RelayId, &relayId) &&
		relayId.size() == kNetRelayKeysFingerprintBytes && FromHex(kNyc3Fingerprint, &nyc3) && FromHex(kNyc3KeyWithVectorRelay, &withVector);
	Check(have, "nyc3's list key (a P-256 SubjectPublicKeyInfo, 91 bytes) and its relay's id are read");

	if (!have)
	{
		return;
	}

	uint8_t made[kNetServerFingerprintBytes];
	Check(NetServerFingerprint_Make(listKey.data(), listKey.size(), relayId.data(), made) && HexOf(made, sizeof(made)) == kNyc3Fingerprint,
		"nyc3's fingerprint is SHA-256 over the words, a 0, its list key and its relay's id (it's " + HexOf(made, sizeof(made)) + ")");

	// One bit changed in the list key or the relay's id: another fingerprint
	bool allDiffer = true;

	for (size_t place : { static_cast<size_t>(0), static_cast<size_t>(1), listKey.size() - 1 })
	{
		listKey[place] ^= 0x01;
		allDiffer = allDiffer && NetServerFingerprint_Make(listKey.data(), listKey.size(), relayId.data(), made) && memcmp(made, nyc3.data(), sizeof(made)) != 0;
		listKey[place] ^= 0x01;
	}

	for (size_t place : { static_cast<size_t>(0), kNetRelayKeysFingerprintBytes - 1 })
	{
		relayId[place] ^= 0x01;
		allDiffer = allDiffer && NetServerFingerprint_Make(listKey.data(), listKey.size(), relayId.data(), made) && memcmp(made, nyc3.data(), sizeof(made)) != 0;
		relayId[place] ^= 0x01;
	}

	Check(allDiffer, "one bit changed at the first, second or last byte of the list key, or the first or last of the relay's id, gives another");
	Check(NetServerFingerprint_Make(listKey.data(), listKey.size() - 1, relayId.data(), made) && memcmp(made, nyc3.data(), sizeof(made)) != 0,
		"and so does the list key a byte short");

	// What's refused
	memset(made, 0xA5, sizeof(made));
	Check(!NetServerFingerprint_Make(NULL, 91, relayId.data(), made) && AllZero(made, sizeof(made)), "no list key is refused, with nothing left in the answer");
	memset(made, 0xA5, sizeof(made));
	Check(!NetServerFingerprint_Make(listKey.data(), 0, relayId.data(), made) && AllZero(made, sizeof(made)), "nor is an empty one");
	std::vector<uint8_t> huge(kNetServerFingerprintMostKeyBytes + 1, 0x30);
	memset(made, 0xA5, sizeof(made));
	Check(!NetServerFingerprint_Make(huge.data(), huge.size(), relayId.data(), made) && AllZero(made, sizeof(made)), "nor one over 4 KB");
	Check(NetServerFingerprint_Make(huge.data(), kNetServerFingerprintMostKeyBytes, relayId.data(), made), "but one of 4 KB is hashed");
	memset(made, 0xA5, sizeof(made));
	Check(!NetServerFingerprint_Make(listKey.data(), listKey.size(), NULL, made) && AllZero(made, sizeof(made)), "nor no relay id");

	// The check against a line: the vectors' relay keys, whose id the first part of this test proves, with nyc3's list key
	JsonValue file;
	std::string const all = ReadRepositoryFile("tests/vectors/crypto-v2.json");
	JsonReader reader(all);
	bool const read = !all.empty() && reader.ReadAll(&file) && file.kind == kJsonObject;
	const JsonValue *const a = read ? file.Member("handshakeA") : NULL;
	const JsonValue *const relay = a ? a->Member("relayStatic") : NULL;
	const JsonValue *const p384 = relay ? TextMember(*relay, "p384Public") : NULL;
	const JsonValue *const kem = relay ? TextMember(*relay, "mlkemPublic") : NULL;
	std::vector<uint8_t> p384Bytes, kemBytes;
	bool const haveKeys = p384 && kem && FromHex(p384->text, &p384Bytes) && FromHex(kem->text, &kemBytes) && p384Bytes.size() == kNetCryptoP384PublicBytes
		&& kemBytes.size() == kNetPqKemPublicBytes;
	Check(haveKeys, "the vectors' relay keys are read");

	if (!haveKeys)
	{
		return;
	}

	NetRelayStaticKeys keys;
	memcpy(keys.p384, p384Bytes.data(), sizeof(keys.p384));
	memcpy(keys.kem, kemBytes.data(), sizeof(keys.kem));
	Check(NetServerFingerprint_IsTheLines(listKey.data(), listKey.size(), &keys, withVector.data()),
		"nyc3's list key with the vectors' relay keys is the line's fingerprint 6f15c487...");
	Check(!NetServerFingerprint_IsTheLines(listKey.data(), listKey.size(), &keys, nyc3.data()), "and isn't nyc3's own: another relay, another server");

	// Any change, to the list key, either relay key or the line's number, and it isn't the line's
	bool allRefused = true;
	listKey[50] ^= 0x01;
	allRefused = allRefused && !NetServerFingerprint_IsTheLines(listKey.data(), listKey.size(), &keys, withVector.data());
	listKey[50] ^= 0x01;
	keys.p384[96] ^= 0x01;
	allRefused = allRefused && !NetServerFingerprint_IsTheLines(listKey.data(), listKey.size(), &keys, withVector.data());
	keys.p384[96] ^= 0x01;
	keys.kem[kNetPqKemPublicBytes - 1] ^= 0x01;
	allRefused = allRefused && !NetServerFingerprint_IsTheLines(listKey.data(), listKey.size(), &keys, withVector.data());
	keys.kem[kNetPqKemPublicBytes - 1] ^= 0x01;

	for (size_t i = 0; i < kNetServerFingerprintBytes; i++)
	{
		withVector[i] ^= 0x80;
		allRefused = allRefused && !NetServerFingerprint_IsTheLines(listKey.data(), listKey.size(), &keys, withVector.data());
		withVector[i] ^= 0x80;
	}

	Check(allRefused, "a bit changed in the list key, the relay's P-384 key, its ML-KEM key, or any of the line's 32 bytes, and it isn't the line's");
	Check(NetServerFingerprint_IsTheLines(listKey.data(), listKey.size(), &keys, withVector.data()), "and with every bit back, it is again");
	Check(!NetServerFingerprint_IsTheLines(NULL, 0, &keys, withVector.data()) && !NetServerFingerprint_IsTheLines(listKey.data(), listKey.size(), NULL, withVector.data())
		&& !NetServerFingerprint_IsTheLines(listKey.data(), listKey.size(), &keys, NULL), "no list key, no relay keys or no line is never a match");
}

int main()
{
	printf("A relay's keys and their fingerprint\n");
	TestTheFingerprint();
	TestTheSetting();
	TestTheServersCfgFingerprint();

	if (checksFailed > 0)
	{
		printf("%d check%s FAILED\n", checksFailed, checksFailed == 1 ? "" : "s");
		return 1;
	}

	printf("Every check passed\n");
	return 0;
}
