// The test of a relay's keys and their fingerprint (3dc/net_relay_keys.h). Written 2026-11-09 by Melanikus for the AvP port.
//
// WHAT IT TESTS:
// - the fingerprint of the relay keys in the relay's own test vectors (tests/vectors/crypto-v2.json, handshakeA.relayStatic) is all 48
//   bytes of SHA-384 over the words and the keys, and its first 16 are the vector the relay server's test was first held to;
// - one bit changed anywhere in the keys gives another fingerprint (modified 2026-11-10 by Melanikus for the AvP port, #175: the check that
//   held keys to the -dev setting RelayKey, and the setting's reader, are gone; the server's servers.cfg line proves the keys now);
// - (added 2026-11-10 by Melanikus for the AvP port, #175) a server's servers.cfg fingerprint (3dc/net_server_fingerprint.h): nyc3's known
//   answer, the same list key with the vectors' relay, and that a change to any byte of the list's key, the relay's keys or the line's
//   number is refused;
// - (the same) the list's key taken from a real certificate (3dc/net_server_key.h), byte for byte as another implementation reads it,
//   and PO's three cases: out of date with the right key is the line's server, in date with another key isn't, the right key under
//   another name is.
// It uses Windows' own SHA-384 and SHA-256 (CNG) and its certificate code (crypt32), so it's built and run on GitHub only, as the other
// cryptography tests are.
#include "net_relay_keys.h"
#include "net_server_fingerprint.h"
#include "net_server_key.h"

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

	// One bit changed anywhere in the keys: another id, so the line's fingerprint no longer matches them (net_server_fingerprint.h)
	size_t const places[] = { 0, 1, 96, kNetCryptoP384PublicBytes, kNetCryptoP384PublicBytes + 1, kNetCryptoP384PublicBytes + kNetPqKemPublicBytes - 1 };
	bool allDiffer = true;

	for (size_t const place : places)
	{
		uint8_t *const byte = place < kNetCryptoP384PublicBytes ? &keys.p384[place] : &keys.kem[place - kNetCryptoP384PublicBytes];
		uint8_t other[kNetRelayKeysFingerprintBytes];
		*byte ^= 0x01;
		allDiffer = allDiffer && NetRelayKeys_Fingerprint(&keys, other) && memcmp(other, fingerprint, sizeof(other)) != 0;
		*byte ^= 0x01;
	}

	Check(allDiffer, "keys with one bit changed, at the first, second and last byte of each key, have another id");
}

// Added 2026-11-10 by Melanikus for the AvP port (#175): a server's servers.cfg fingerprint. The known answers were worked out three ways
// apart from the game: Node's crypto, Windows' certutil over the same bytes, and the server's own Java (ServersLine, Primary, 2026-11-10).
static const char kNyc3ListKey[] = "3059301306072a8648ce3d020106082a8648ce3d030107034200046860ab37215b80bc818041605b4dc15b0f553ff8e0294ef67e0145059ab4fe9fb670e3b138d4fbef7c9a82ae095b32dc647e20fc0840c56c1edf1402f51515f8";
static const char kNyc3RelayId[] = "d4afea79e806fa341d350643a54b6964e532c591c0544be9ce93db4c752b9d89f668f026ffeb75a64f5049082499e445";
static const char kNyc3Fingerprint[] = "bb0e2958dade79033a90f34811c84837b404708cc004c5b1d37d9a26f28d663e";
static const char kNyc3KeyWithVectorRelay[] = "6f15c487060a467449128e470ae9f4ba5649b4e9598f42aeaea450fab880df30";

static void TestTheServersCfgFingerprint()
{
	printf("2. a server's servers.cfg fingerprint\n");
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

// Added 2026-11-10 by Melanikus for the AvP port (#175): the list's key from a real certificate (net_server_key.h), and PO's three cases:
// a certificate out of date with the right key is the right list, one in date with another key isn't, and the right key under another
// name is. The three test certificates were made for this test with Git for Windows' OpenSSL 3.5.9, from two P-256 keys made for it and
// thrown away after (nothing here signs anything):
//   openssl ecparam -name prime256v1 -genkey -noout -out a.key            (and b.key the same way)
//   openssl req -x509 -new -key a.key -subj "/CN=list.example" -not_before 20200101000000Z -not_after 20200201000000Z -outform DER -out a-expired.der
//   openssl req -x509 -new -key b.key -subj "/CN=list.example" -not_before 20260101000000Z -not_after 20460101000000Z -outform DER -out b-valid.der
//   openssl req -x509 -new -key a.key -subj "/CN=another.example" -not_before 20260101000000Z -not_after 20460101000000Z -outform DER -out a-othername.der
// Their keys and fingerprints (with the vectors' relay id, fc352802...) were worked out apart from Windows, with Node's own crypto.
// nyc3's is the certificate its list served on 2026-11-10, whose key is kNyc3ListKey.
static const char kExpiredWithKeyA[] =
	"3082018430820129a0030201020214767e9fa20fe803044d332c771bc284bb05492afc300a06082a8648ce3d04030230173115301306035504030c0c"
	"6c6973742e6578616d706c65301e170d3230303130313030303030305a170d3230303230313030303030305a30173115301306035504030c0c6c6973"
	"742e6578616d706c653059301306072a8648ce3d020106082a8648ce3d030107034200049121bc0a01024cb6da8fee145c4260fcdecb20d87fd603c9"
	"81fa0cab886d36312ca0fe29cf9102c3994d673faf962370fb8a519059edd80b6505b0c36697368ca3533051301d0603551d0e041604147ff6971f18"
	"e95d61ecb1fbb6e103be3c626a52ef301f0603551d230418301680147ff6971f18e95d61ecb1fbb6e103be3c626a52ef300f0603551d130101ff0405"
	"30030101ff300a06082a8648ce3d0403020349003046022100afdd4b54dce103f14eb3f740c308c6489f12391cbd6c99c7e1ad9826e635e62f022100"
	"89249eac4b3f371c591319dda719a413a9ee6216be36d0277f4559865417117c";
static const char kInDateWithKeyB[] =
	"3082018330820129a00302010202142d852aea44a9e4fdd4fa670ff678772cadf03871300a06082a8648ce3d04030230173115301306035504030c0c"
	"6c6973742e6578616d706c65301e170d3236303130313030303030305a170d3436303130313030303030305a30173115301306035504030c0c6c6973"
	"742e6578616d706c653059301306072a8648ce3d020106082a8648ce3d030107034200045fc8ffa261acc608195308269fd4ccd221609cac3e3f6629"
	"1ba97f4545a78366099872fbd78c0c8c20da89e259e5cc17cb9ffcbfe8027315fb737f30b73ab076a3533051301d0603551d0e041604144336e4b983"
	"3cb79af47aac7ed1f4e86b7fd0d1ba301f0603551d230418301680144336e4b9833cb79af47aac7ed1f4e86b7fd0d1ba300f0603551d130101ff0405"
	"30030101ff300a06082a8648ce3d040302034800304502203d23485615a8f82ab38ed5d2a8338497d5d6cad3288149b6fcbab04b8ea84cec022100ab"
	"650c68b7edbc4df5bc1761b6b0b472ad57eed058fce759dfb2c71047cfa0be";
static const char kOtherNameWithKeyA[] =
	"3082018a3082012fa00302010202143b09b8380f081a24ec78e5c671afb54d633471c3300a06082a8648ce3d040302301a3118301606035504030c0f"
	"616e6f746865722e6578616d706c65301e170d3236303130313030303030305a170d3436303130313030303030305a301a3118301606035504030c0f"
	"616e6f746865722e6578616d706c653059301306072a8648ce3d020106082a8648ce3d030107034200049121bc0a01024cb6da8fee145c4260fcdecb"
	"20d87fd603c981fa0cab886d36312ca0fe29cf9102c3994d673faf962370fb8a519059edd80b6505b0c36697368ca3533051301d0603551d0e041604"
	"147ff6971f18e95d61ecb1fbb6e103be3c626a52ef301f0603551d230418301680147ff6971f18e95d61ecb1fbb6e103be3c626a52ef300f0603551d"
	"130101ff040530030101ff300a06082a8648ce3d0403020349003046022100e1b3b8be39ff77bcecf9754bf5d34f1d0b5bdb77e5465f6ac9e6d2d5ea"
	"b72d2f022100e17050154a6d8cc394496273259e93e18d912653f4ff38d54a864ff075ae7711";
static const char kNyc3Certificate[] =
	"308201c83082016ea0030201020214781107462be3eed7e82440c077b00e2650fb8863300a06082a8648ce3d040302301f311d301b06035504030c14"
	"6e7963332e72656c61792e617670782e74657374301e170d3236313130393035313235355a170d3236313230393035313235355a301f311d301b0603"
	"5504030c146e7963332e72656c61792e617670782e746573743059301306072a8648ce3d020106082a8648ce3d030107034200046860ab37215b80bc"
	"818041605b4dc15b0f553ff8e0294ef67e0145059ab4fe9fb670e3b138d4fbef7c9a82ae095b32dc647e20fc0840c56c1edf1402f51515f8a3818730"
	"8184301d0603551d0e04160414517cd7f473ce72e3173f4c37b89023222708a80a301f0603551d23041830168014517cd7f473ce72e3173f4c37b890"
	"23222708a80a301f0603551d110418301682146e7963332e72656c61792e617670782e74657374300c0603551d130101ff0402300030130603551d25"
	"040c300a06082b06010505070301300a06082a8648ce3d040302034800304502200369232c0a1f991ee0c673029669d979d8ea09ef7d093b0bf77b1e"
	"e32a30e37002210097aba6e9085634d9a2cdf24beb2d515cf462b663e03cf844e51b95542b9a093d";
static const char kKeyAWithVectorRelay[] = "7379c24691eb7c3af1add347f1f9a2726d820d6bd0fd446755ea9148187f2fea";
static const char kKeyBWithVectorRelay[] = "b5c39025e8781d5719378af3ad9c9c772c51667011d93a0c423d0641852a4757";

// The key of a certificate given as hex, as the game takes it
static bool KeyOf(const char *certificateHex, std::vector<uint8_t> *key)
{
	std::vector<uint8_t> certificate;
	uint8_t room[kNetServerFingerprintMostKeyBytes];
	size_t size = 0;

	if (!FromHex(certificateHex, &certificate) || !NetServerKey_OfCertificate(certificate.data(), certificate.size(), room, sizeof(room), &size))
	{
		return false;
	}

	key->assign(room, room + size);
	return true;
}

static void TestTheListsKey()
{
	printf("3. the list's key, from the certificate it shows\n");
	std::vector<uint8_t> nyc3Key, expected, keyA, keyB, keyAOtherName;
	FromHex(kNyc3ListKey, &expected);
	Check(KeyOf(kNyc3Certificate, &nyc3Key) && nyc3Key == expected, "nyc3's certificate gives its 91-byte key, byte for byte as Node read it");
	Check(KeyOf(kExpiredWithKeyA, &keyA) && keyA.size() == 91, "a certificate out of date since 2020 still gives its key");
	Check(KeyOf(kOtherNameWithKeyA, &keyAOtherName) && keyAOtherName == keyA, "a certificate for another name with the same key gives the same key");
	Check(KeyOf(kInDateWithKeyB, &keyB) && keyB.size() == 91 && keyB != keyA, "and one in date with another key gives that other key");

	if (keyA.empty() || keyB.empty())
	{
		return;
	}

	// PO's three cases, through the check the game makes: a line made from key A, with the vectors' relay
	JsonValue file;
	std::string const all = ReadRepositoryFile("tests/vectors/crypto-v2.json");
	JsonReader reader(all);
	bool const read = !all.empty() && reader.ReadAll(&file) && file.kind == kJsonObject;
	const JsonValue *const a = read ? file.Member("handshakeA") : NULL;
	const JsonValue *const relay = a ? a->Member("relayStatic") : NULL;
	const JsonValue *const p384 = relay ? TextMember(*relay, "p384Public") : NULL;
	const JsonValue *const kem = relay ? TextMember(*relay, "mlkemPublic") : NULL;
	std::vector<uint8_t> p384Bytes, kemBytes, lineA, lineB;
	bool const haveKeys = p384 && kem && FromHex(p384->text, &p384Bytes) && FromHex(kem->text, &kemBytes) && p384Bytes.size() == kNetCryptoP384PublicBytes
		&& kemBytes.size() == kNetPqKemPublicBytes && FromHex(kKeyAWithVectorRelay, &lineA) && FromHex(kKeyBWithVectorRelay, &lineB);
	Check(haveKeys, "the vectors' relay keys, and the two lines' fingerprints, are read");

	if (!haveKeys)
	{
		return;
	}

	NetRelayStaticKeys keys;
	memcpy(keys.p384, p384Bytes.data(), sizeof(keys.p384));
	memcpy(keys.kem, kemBytes.data(), sizeof(keys.kem));
	Check(NetServerFingerprint_IsTheLines(keyA.data(), keyA.size(), &keys, lineA.data()), "an expired certificate with the right key: the line's server");
	Check(!NetServerFingerprint_IsTheLines(keyB.data(), keyB.size(), &keys, lineA.data()), "a certificate in date with the wrong key: not the line's server");
	Check(NetServerFingerprint_IsTheLines(keyAOtherName.data(), keyAOtherName.size(), &keys, lineA.data()), "the right key under another name: the line's server");
	Check(NetServerFingerprint_IsTheLines(keyB.data(), keyB.size(), &keys, lineB.data()), "(and key B is the line made from key B)");

	// What isn't a certificate gives no key
	std::vector<uint8_t> certificate;
	FromHex(kInDateWithKeyB, &certificate);
	uint8_t room[kNetServerFingerprintMostKeyBytes];
	size_t size = 99;
	memset(room, 0xA5, sizeof(room));
	Check(!NetServerKey_OfCertificate(certificate.data(), certificate.size() - 1, room, sizeof(room), &size) && size == 0 && AllZero(room, sizeof(room)),
		"a certificate a byte short gives no key, and leaves nothing behind");
	std::vector<uint8_t> broken = certificate;
	broken[0] = 0x31;
	Check(!NetServerKey_OfCertificate(broken.data(), broken.size(), room, sizeof(room), &size) && size == 0, "nor does one that doesn't start as DER's sequence");
	Check(!NetServerKey_OfCertificate(certificate.data(), certificate.size(), room, 90, &size) && size == 0 && AllZero(room, 90),
		"nor does a key with no room for all of it (91 bytes into 90)");
	Check(NetServerKey_OfCertificate(certificate.data(), certificate.size(), room, 91, &size) && size == 91, "but 91 bytes of room take it");
	std::vector<uint8_t> huge(kNetServerKeyMostCertificateBytes + 1, 0x30);
	Check(!NetServerKey_OfCertificate(huge.data(), huge.size(), room, sizeof(room), &size) && size == 0, "a certificate over 64 KB isn't read");
	Check(!NetServerKey_OfCertificate(NULL, 10, room, sizeof(room), &size) && !NetServerKey_OfCertificate(certificate.data(), certificate.size(), NULL, 91, &size)
		&& !NetServerKey_OfCertificate(certificate.data(), certificate.size(), room, sizeof(room), NULL), "no certificate, or nowhere to put the key, is refused");
}

int main()
{
	printf("A relay's keys and their fingerprint\n");
	TestTheFingerprint();
	TestTheServersCfgFingerprint();
	TestTheListsKey();

	if (checksFailed > 0)
	{
		printf("%d check%s FAILED\n", checksFailed, checksFailed == 1 ? "" : "s");
		return 1;
	}

	printf("Every check passed\n");
	return 0;
}
