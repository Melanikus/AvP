// The test of the game's side of the relay (3dc/net_relay.h). Written 2026-11-08 by Melanikus for the AvP port.
// Modified 2026-11-08 by Melanikus for the AvP port: PROTOCOL.md 7014de1 (the pass, the rule that a field a kind doesn't use is 0, and the
// echoes that pick a relay); and two checks GitHub's run of dc83d6ee found wrong in this test, not in the code: the host's hellos were
// counted with its 15 s keep, which falls due at the same moment, and 64 bytes of A's fail the magic before the version.
// Modified 2026-11-08 by Melanikus for the AvP port: the relay's shared test vectors (tests/vectors/relay-v1.json), and a pass time's limit.
//
// WHAT IT TESTS
//
// The relays' messages: that each kind is written byte for byte as PROTOCOL.md says (AVPx-Dedicated-Server, 7014de1), that reading gives
// back what was written, and that anything that isn't exactly a message is refused, each with the reason NetRelay_Read gives for it. Then
// the three state machines: picking a relay by its round trip, the host's room and the joiner's way in: each step, every time limit, and what
// each does with a message that's wrong in some way (another room's token, another joiner's ticket, an echo's bytes it never sent, a port
// that can't be a joiner's, the wrong sender).
//
// And the relay's own test vectors: a file of exact bytes its repository makes from its own code (tests/vectors/relay-v1.json, copied
// byte for byte, with its sha256 checked by the gate). Every message in it has to read here to the fields it lists, and every packet it
// lists as refused has to be refused here for the same reason. So the game and the relay can't each pass their own tests and still read
// the same bytes differently.
//
// WHAT IT DOESN'T TEST
//
// The glue in networking.cpp that sends these from the game's own socket (tests/net_start_test.cpp tries some of it), and the relay itself
// (its own tests are in its repository). So nothing here sends a packet.

#include <stdio.h>
#include <string.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "net_relay.h"

static int checksFailed = 0;

static void Check(bool passed, char const *what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what);

	if (!passed)
	{
		checksFailed++;
	}
}

uint16_t const kControl = 27500;
uint16_t const kJoinerPort = 27501;

static const uint8_t kToken[kNetRelaySecretBytes] = { 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE };
static const uint8_t kOther[kNetRelaySecretBytes] = { 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFF };
static const uint8_t kCookie[kNetRelaySecretBytes] = { 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9 };
static const uint8_t kPass[kNetRelayPassBytes] = { 7, 6, 5, 4, 3, 2, 1, 0xF0, 7, 6, 5, 4, 3, 2, 1, 0xF1 };
static const uint8_t kNewerPass[kNetRelayPassBytes] = { 8, 6, 5, 4, 3, 2, 1, 0xF0, 7, 6, 5, 4, 3, 2, 1, 0xF1 };
uint64_t const kPassTime = 0x69101800u;   // a time in November 2025, in seconds since 1970, with a different byte in each of its lowest four
static const char kCode[] = "ABCD2345";
static const char kOtherCode[] = "ABCD2346";

// A message's bytes, with these fields
struct Bytes
{
	uint8_t b[kNetRelayMessageBytes];
};

static Bytes Make(int type, const uint8_t *secret, const char *code, uint16_t port, uint16_t number, const uint8_t *pass = NULL, uint64_t passTime = 0)
{
	NetRelayMessage m = NetRelay_Message(type);

	if (secret)
	{
		memcpy(m.secret, secret, kNetRelaySecretBytes);
	}

	if (code)
	{
		memcpy(m.code, code, kNetRelayCodeLetters);
	}

	if (pass)
	{
		memcpy(m.pass, pass, kNetRelayPassBytes);
	}

	m.port = port;
	m.number = number;
	m.passTime = passTime;
	Bytes out;
	NetRelay_Write(m, out.b);
	return out;
}

// The one message a state machine wants sent, read back. False if it wants none, or more than one.
template <typename End>
static bool OneSend(End &end, uint16_t *port, NetRelayMessage *m)
{
	NetRelaySend sends[kNetRelayMostSends];
	size_t const n = end.TakeSends(sends, kNetRelayMostSends);

	if (n != 1)
	{
		return false;
	}

	*port = sends[0].port;
	return NetRelay_Read(sends[0].bytes, kNetRelayMessageBytes, m) == kNetRelayIsAMessage;
}

template <typename End>
static size_t SendCount(End &end)
{
	NetRelaySend sends[kNetRelayMostSends];
	return end.TakeSends(sends, kNetRelayMostSends);
}

// How many of what an end wants sent are of this kind
template <typename End>
static size_t SendsOfKind(End &end, int type)
{
	NetRelaySend sends[kNetRelayMostSends];
	size_t const n = end.TakeSends(sends, kNetRelayMostSends);
	size_t found = 0;

	for (size_t i = 0; i < n; i++)
	{
		found += sends[i].bytes[5] == type ? 1 : 0;
	}

	return found;
}

static void TheMessages()
{
	printf("The relays' messages\n");

	Bytes const joinPort = Make(kNetRelayJoinPort, kToken, kCode, 27501, 0);
	Check(joinPort.b[0] == 'A' && joinPort.b[1] == 'V' && joinPort.b[2] == 'P' && joinPort.b[3] == 'R' && joinPort.b[4] == 1 && joinPort.b[5] == kNetRelayJoinPort,
		"a message starts AVPR, then the version, 1, then its kind");
	Check(joinPort.b[32] == 0x6D && joinPort.b[33] == 0x6B, "its port is written low byte first: 27501 is 6D 6B");
	Check(memcmp(joinPort.b + 8, kToken, 16) == 0 && memcmp(joinPort.b + 24, kCode, 8) == 0 && joinPort.b[6] == 0 && joinPort.b[7] == 0 && NetRelay_AllZero(joinPort.b + 34, 30),
		"the secret is at 8, the code at 24, and the rest is 0");

	NetRelayMessage m;
	Check(NetRelay_Read(joinPort.b, kNetRelayMessageBytes, &m) == kNetRelayIsAMessage && m.type == kNetRelayJoinPort && m.port == 27501 && m.number == 0
		&& memcmp(m.secret, kToken, 16) == 0 && memcmp(m.code, kCode, 8) == 0 && m.passTime == 0 && NetRelay_AllZero(m.pass, 16), "reading it gives back every field");

	Bytes const kept = Make(kNetRelayHostKept, kToken, NULL, 0, 0x0102, kPass, kPassTime);
	Check(kept.b[34] == 0x02 && kept.b[35] == 0x01 && memcmp(kept.b + 36, kPass, 16) == 0, "a count is written low byte first, and the pass is at 36");
	Check(kept.b[52] == 0x00 && kept.b[53] == 0x18 && kept.b[54] == 0x10 && kept.b[55] == 0x69 && kept.b[56] == 0 && kept.b[59] == 0 && NetRelay_AllZero(kept.b + 60, 4),
		"the pass time is 8 bytes at 52, low byte first, and 60-63 are 0");
	Check(NetRelay_Read(kept.b, kNetRelayMessageBytes, &m) == kNetRelayIsAMessage && m.number == 0x0102 && memcmp(m.pass, kPass, 16) == 0 && m.passTime == kPassTime,
		"and they read back");
	Bytes const farTime = Make(kNetRelayHostKept, kToken, NULL, 0, 0, kPass, 0x001F030405060708ull);
	Check(NetRelay_Read(farTime.b, kNetRelayMessageBytes, &m) == kNetRelayIsAMessage && m.passTime == 0x001F030405060708ull, "each byte of a pass time is its own");
	Bytes const latest = Make(kNetRelayHostKept, kToken, NULL, 0, 0, kPass, kNetRelayMostPassTime);
	Check(NetRelay_Read(latest.b, kNetRelayMessageBytes, &m) == kNetRelayIsAMessage && m.passTime == 9007199254740991ull,
		"the latest pass time there can be, 2^53 - 1, is read");
	Bytes const tooLate = Make(kNetRelayHostKept, kToken, NULL, 0, 0, kPass, kNetRelayMostPassTime + 1);
	Bytes const allOnes = Make(kNetRelayHostOpened, kToken, kCode, 0, 0, kPass, 0xFFFFFFFFFFFFFFFFull);
	Check(NetRelay_Read(tooLate.b, kNetRelayMessageBytes, &m) == kNetRelayNotPassTime && NetRelay_Read(allOnes.b, kNetRelayMessageBytes, &m) == kNetRelayNotPassTime,
		"one second later, or every bit set, is refused: the list could never take it, and the relay refuses it too");

	bool everyKind = true;

	for (int type = 1; type <= kNetRelayMostType; type++)
	{
		Bytes const one = Make(type, NULL, NULL, 0, 0);
		everyKind = everyKind && NetRelay_Read(one.b, kNetRelayMessageBytes, &m) == kNetRelayIsAMessage && m.type == type;
	}

	Check(everyKind, "each of the 15 kinds, with every field 0, is a message");

	// What isn't one, and why
	Check(NetRelay_Read(joinPort.b, 63, &m) == kNetRelayNotMessageSize && NetRelay_Read(joinPort.b, 0, &m) == kNetRelayNotMessageSize
		&& NetRelay_Read(NULL, 64, &m) == kNetRelayNotMessageSize, "63 bytes, none, or no packet at all: not exactly 64 bytes");
	uint8_t longer[65];
	memcpy(longer, joinPort.b, 64);
	longer[64] = 0;
	Check(NetRelay_Read(longer, 65, &m) == kNetRelayNotMessageSize, "65 bytes: not exactly 64");

	Bytes bad = joinPort;
	bad.b[3] = 'S';
	Check(NetRelay_Read(bad.b, 64, &m) == kNetRelayNotMagic, "another magic: not AVPR");
	bad = joinPort;
	bad.b[4] = 2;
	Check(NetRelay_Read(bad.b, 64, &m) == kNetRelayNotVersion, "version 2: another version");
	bad = joinPort;
	bad.b[5] = 0;
	Check(NetRelay_Read(bad.b, 64, &m) == kNetRelayNotType, "kind 0: no such kind");
	bad.b[5] = 16;
	Check(NetRelay_Read(bad.b, 64, &m) == kNetRelayNotType, "kind 16: no such kind");
	bad.b[5] = 255;
	Check(NetRelay_Read(bad.b, 64, &m) == kNetRelayNotType, "kind 255: no such kind");
	bad = joinPort;
	bad.b[7] = 1;
	Check(NetRelay_Read(bad.b, 64, &m) == kNetRelayNotZero, "a reserved byte that isn't 0");
	bad = kept;
	bad.b[60] = 1;
	Check(NetRelay_Read(bad.b, 64, &m) == kNetRelayNotZero, "the first padding byte not 0, even in a kind with a pass");
	bad = joinPort;
	bad.b[63] = 0x80;
	Check(NetRelay_Read(bad.b, 64, &m) == kNetRelayNotZero, "the last padding byte not 0");

	// A field a kind doesn't use (PROTOCOL.md's table)
	Bytes const withNumber = Make(kNetRelayJoinPort, kToken, kCode, 27501, 1);
	Check(NetRelay_Read(withNumber.b, 64, &m) == kNetRelayNotUnused, "JOIN_PORT with a number, which it doesn't have");
	Bytes const withPass = Make(kNetRelayJoinPort, kToken, kCode, 27501, 0, kPass, 0);
	Check(NetRelay_Read(withPass.b, 64, &m) == kNetRelayNotUnused, "JOIN_PORT with a pass");
	Bytes const withPassTime = Make(kNetRelayHostKeep, kToken, NULL, 0, 0, NULL, 5);
	Check(NetRelay_Read(withPassTime.b, 64, &m) == kNetRelayNotUnused, "HOST_KEEP with a pass time, and no pass");
	Bytes const refusedWithSecret = Make(kNetRelayRefused, kToken, NULL, 0, 1);
	Check(NetRelay_Read(refusedWithSecret.b, 64, &m) == kNetRelayNotUnused, "REFUSED with a secret, which it never has");
	Bytes const cookieWithCode = Make(kNetRelayCookie, kCookie, kCode, 0, 0);
	Check(NetRelay_Read(cookieWithCode.b, 64, &m) == kNetRelayNotUnused, "COOKIE with a code");
	Bytes const helloWithPort = Make(kNetRelayJoinerHello, kToken, NULL, 27501, 0);
	Check(NetRelay_Read(helloWithPort.b, 64, &m) == kNetRelayNotUnused, "JOINER_HELLO with a port, which only the host's hello has");
	Bytes const echoWithPort = Make(kNetRelayEchoBack, kToken, NULL, 1, 2);
	Check(NetRelay_Read(echoWithPort.b, 64, &m) == kNetRelayNotUnused, "ECHO_BACK with a port");
	Bytes const echo = Make(kNetRelayEchoBack, kToken, NULL, 0, 2);
	Check(NetRelay_Read(echo.b, 64, &m) == kNetRelayIsAMessage && m.number == 2, "but ECHO_BACK with its bytes and count is one");

	bad = Make(kNetRelayJoin, NULL, "ABCD234O", 0, 0);
	Check(NetRelay_Read(bad.b, 64, &m) == kNetRelayNotCode, "a code with O in it, which no code has");
	bad = Make(kNetRelayJoin, NULL, "abcd2345", 0, 0);
	Check(NetRelay_Read(bad.b, 64, &m) == kNetRelayNotCode, "a code in small letters");
	bad = Make(kNetRelayJoin, NULL, "ABCD234", 0, 0);   // 7 letters and a 0
	Check(NetRelay_Read(bad.b, 64, &m) == kNetRelayNotCode, "a code of 7 letters and a 0: neither a code nor all 0");

	NetRelayMessage untouched = NetRelay_Message(kNetRelayCookie);
	untouched.port = 77;
	Check(NetRelay_Read(bad.b, 64, &untouched) == kNetRelayNotCode && untouched.port == 77 && untouched.type == kNetRelayCookie,
		"a packet that isn't a message leaves what it was to be read into as it was");

	// Secrets
	Check(NetRelay_SameSecret(kToken, kToken) && !NetRelay_SameSecret(kToken, kOther), "two secrets differing in the last bit aren't the same");

	Check(NetRelay_RefusedWhy(2) == kNetRelayRefusedFull && NetRelay_RefusedWhy(0) == kNetRelayRefusedOther && NetRelay_RefusedWhy(5) == kNetRelayRefusedOther,
		"REFUSED's reasons 1 to 4 are kept, and any other is \"a reason this version doesn't know\"");
}

// 16 bytes for each echo to each relay, all different
static void EchoBytes(uint8_t *out, size_t relays)
{
	for (size_t i = 0; i < relays * kNetRelayEchoes * kNetRelaySecretBytes; i++)
	{
		out[i] = static_cast<uint8_t>(i * 7 + 3);
	}
}

static const uint8_t *EchoOf(const uint8_t *random, size_t relay, size_t echo)
{
	return random + (relay * kNetRelayEchoes + echo) * kNetRelaySecretBytes;
}

static void PickingARelay()
{
	printf("Picking a relay\n");
	uint16_t const ports[3] = { 27500, 27500, 27600 };
	uint8_t random[3 * kNetRelayEchoes * kNetRelaySecretBytes];
	EchoBytes(random, 3);

	NetRelayPicker picker;
	Check(!picker.Start(0, ports, random, 0) && SendCount(picker) == 0, "no relays to time: nothing starts, and nothing is sent");
	Check(!picker.Start(kNetRelayMostRelays + 1, ports, random, 0), "nor more than 16");

	Check(picker.Start(3, ports, random, 1000), "three relays to time");
	NetRelaySend sends[kNetRelayMostSends];
	size_t const first = picker.TakeSends(sends, kNetRelayMostSends);
	NetRelayMessage m;
	Check(first == 3 && sends[0].relay == 0 && sends[1].relay == 1 && sends[2].relay == 2 && sends[2].port == 27600
		&& NetRelay_Read(sends[0].bytes, 64, &m) == kNetRelayIsAMessage && m.type == kNetRelayEcho && m.number == 1 && memcmp(m.secret, EchoOf(random, 0, 0), 16) == 0,
		"the first echo goes to each at once, to its control port, with its own 16 bytes and the count 1");

	picker.Tick(1000 + kNetRelayEchoEveryMs - 1);
	Check(SendCount(picker) == 0, "nothing more within 100 ms");
	picker.Tick(1000 + kNetRelayEchoEveryMs);
	Check(SendCount(picker) == 3, "then the second echo to each");
	picker.Tick(1000 + 2 * kNetRelayEchoEveryMs);
	Check(SendCount(picker) == 3, "then the third");
	picker.Tick(1000 + 5 * kNetRelayEchoEveryMs);
	Check(SendCount(picker) == 0, "and no more");

	// Relay 0 answers its first echo after 80 ms and its second after 30; relay 2 its first after 40; relay 1 never
	Bytes const back00 = Make(kNetRelayEchoBack, EchoOf(random, 0, 0), NULL, 0, 1);
	Bytes const back01 = Make(kNetRelayEchoBack, EchoOf(random, 0, 1), NULL, 0, 2);
	Bytes const back20 = Make(kNetRelayEchoBack, EchoOf(random, 2, 0), NULL, 0, 1);
	Check(picker.Hear(0, 27500, back00.b, 64, 1080), "relay 0's answer to its first echo");
	Check(picker.Hear(0, 27500, back01.b, 64, 1130), "and to its second");
	Check(picker.Hear(2, 27600, back20.b, 64, 1040), "relay 2's answer to its first");
	Check(picker.Best() == 0 && picker.BestMs() == 30, "the lowest round trip counts: relay 0's 30 ms, its second echo's");

	Check(picker.Hear(2, 27600, back00.b, 64, 1050) && picker.UnusedMessages(kNetRelayWrongSecret) == 1, "an answer with another relay's bytes doesn't count");
	Bytes const wrongCount = Make(kNetRelayEchoBack, EchoOf(random, 2, 0), NULL, 0, 2);
	Check(picker.Hear(2, 27600, wrongCount.b, 64, 1050) && picker.UnusedMessages(kNetRelayWrongSecret) == 2, "nor one with its bytes and another echo's count");
	Bytes const countPastSent = Make(kNetRelayEchoBack, EchoOf(random, 2, 0), NULL, 0, 4);
	Check(picker.Hear(2, 27600, countPastSent.b, 64, 1050) && picker.UnusedMessages(kNetRelayWrongSecret) == 3, "nor a count past the third");
	Check(picker.Hear(2, 27600, back20.b, 64, 1041) && picker.UnusedMessages(kNetRelayWrongSecret) == 4 && picker.BestMs() == 30,
		"an answer sent again doesn't count again: relay 2 doesn't get 1 ms for it");
	Check(!picker.Hear(2, 27500, back20.b, 64, 1041), "from another port of that relay, it isn't the relay's at all");
	Check(!picker.Hear(5, 27500, back00.b, 64, 1041), "nor from a relay that isn't one of the three");
	Bytes const echoNotBack = Make(kNetRelayEcho, EchoOf(random, 1, 0), NULL, 0, 1);
	Check(picker.Hear(1, 27500, echoNotBack.b, 64, 1050) && picker.UnusedMessages(kNetRelayNotForNow) == 1, "an ECHO, not an ECHO_BACK, is taken and not used");
	uint8_t junk[64];
	memset(junk, 'A', sizeof(junk));
	Check(picker.Hear(1, 27500, junk, 64, 1050) && picker.NotMessages(kNetRelayNotMagic) == 1, "64 bytes that aren't a message are counted by why");

	Check(!picker.Done(1000 + 2 * kNetRelayEchoEveryMs + kNetRelayEchoAnswerWithinMs - 1), "it isn't done until a second after the last echo");
	Check(picker.Done(1000 + 2 * kNetRelayEchoEveryMs + kNetRelayEchoAnswerWithinMs), "and then it is");

	NetRelayPicker late;
	late.Start(1, ports, random, 0);
	SendCount(late);
	Check(late.Hear(0, 27500, back00.b, 64, kNetRelayEchoAnswerWithinMs + 1) && late.Best() == -1 && late.BestMs() == 0,
		"an answer later than a second doesn't count, and with none, there's no relay to use");

	NetRelayPicker many;
	uint16_t manyPorts[kNetRelayMostRelays];
	uint8_t manyRandom[kNetRelayMostRelays * kNetRelayEchoes * kNetRelaySecretBytes];

	for (size_t i = 0; i < kNetRelayMostRelays; i++)
	{
		manyPorts[i] = 27500;
	}

	EchoBytes(manyRandom, kNetRelayMostRelays);
	Check(many.Start(kNetRelayMostRelays, manyPorts, manyRandom, 0) && SendCount(many) == kNetRelayMostRelays && many.SendsDropped() == 0,
		"16 relays: one echo each, every one sent");
}

// A host whose room is open, at time 0
static NetRelayHost &OpenHost(NetRelayHost &host)
{
	host.Start(kControl, 0);
	SendCount(host);
	Bytes const opened = Make(kNetRelayHostOpened, kToken, kCode, 0, 0, kPass, kPassTime);
	host.Hear(kControl, opened.b, 64, 50);
	return host;
}

static void TheHost()
{
	printf("The host\n");
	NetRelayHost host;
	uint16_t port = 0;
	NetRelayMessage m;

	host.Start(kControl, 1000);
	Check(OneSend(host, &port, &m) && port == kControl && m.type == kNetRelayHostOpen && NetRelay_AllZero(m.secret, 16),
		"it asks the control port for a room, with no cookie the first time");
	Check(host.Asking() && !host.Open(), "and waits for the answer");

	Bytes const cookie = Make(kNetRelayCookie, kCookie, NULL, 0, 0);
	Check(!host.Hear(27600, cookie.b, 64, 1010) && SendCount(host) == 0, "a cookie from any port but the control port isn't the relay's: it's left for the game");
	Check(host.Hear(kControl, cookie.b, 64, 1010) && OneSend(host, &port, &m) && m.type == kNetRelayHostOpen && memcmp(m.secret, kCookie, 16) == 0,
		"given a cookie, it asks again with it");

	Bytes const zeroToken = Make(kNetRelayHostOpened, NULL, kCode, 0, 0, kPass, kPassTime);
	Check(host.Hear(kControl, zeroToken.b, 64, 1020) && host.Asking() && host.UnusedMessages(kNetRelayWrongSecret) == 1,
		"a room with a token of all 0 isn't one: it's taken, counted and not used");
	Bytes const noCode = Make(kNetRelayHostOpened, kToken, NULL, 0, 0, kPass, kPassTime);
	Check(host.Hear(kControl, noCode.b, 64, 1020) && host.Asking() && host.UnusedMessages(kNetRelayWrongSecret) == 2, "nor one with no code");
	Bytes const noPass = Make(kNetRelayHostOpened, kToken, kCode, 0, 0);
	Check(host.Hear(kControl, noPass.b, 64, 1020) && host.Asking() && host.UnusedMessages(kNetRelayNoPass) == 1, "nor one with no pass, which every HOST_OPENED has");
	Bytes const noPassTime = Make(kNetRelayHostOpened, kToken, kCode, 0, 0, kPass, 0);
	Check(host.Hear(kControl, noPassTime.b, 64, 1020) && host.Asking() && host.UnusedMessages(kNetRelayNoPass) == 2, "nor one whose pass has no time");

	Bytes const opened = Make(kNetRelayHostOpened, kToken, kCode, 0, 0, kPass, kPassTime);
	Check(host.Hear(kControl, opened.b, 64, 1030) && host.Open() && host.PassesHeard() == 1, "HOST_OPENED with a token, a code and a pass opens the room");
	char codeText[8];
	uint8_t pass[16];
	uint64_t passTime = 0;
	Check(host.RoomForTheList(codeText, pass, &passTime) && memcmp(codeText, kCode, 8) == 0 && memcmp(pass, kPass, 16) == 0 && passTime == kPassTime,
		"what the list is given: the code, the pass and its time (never the token)");
	Check(host.Hear(kControl, opened.b, 64, 1040) && host.UnusedMessages(kNetRelayNotForNow) == 1 && host.PassesHeard() == 1,
		"a second HOST_OPENED is taken and not used: the room stays the one it was");
	Check(host.Hear(kControl, cookie.b, 64, 1040) && SendCount(host) == 0 && host.UnusedMessages(kNetRelayNotForNow) == 2,
		"nor does a cookie make it ask again once the room is open");

	// Keeping the room
	host.Tick(1030 + kNetRelayKeepEveryMs - 1);
	Check(SendCount(host) == 0, "nothing before 15 s");
	host.Tick(1030 + kNetRelayKeepEveryMs);
	Check(OneSend(host, &port, &m) && port == kControl && m.type == kNetRelayHostKeep && memcmp(m.secret, kToken, 16) == 0,
		"after 15 s, HOST_KEEP with its token, to the control port");
	Check(host.KeepMs() == -1, "a keep has no round trip until it's answered (2026-11-09: the line on the host's screen)");

	Bytes const keptOther = Make(kNetRelayHostKept, kOther, NULL, 0, 3, kNewerPass, kPassTime + 15);
	Check(host.Hear(kControl, keptOther.b, 64, 1030 + 16000) && host.Joiners() == 0 && host.UnusedMessages(kNetRelayWrongSecret) == 3 && host.PassesHeard() == 1,
		"HOST_KEPT with another room's token is taken, counted and not used, pass and all");
	Bytes const keptNoPass = Make(kNetRelayHostKept, kToken, NULL, 0, 3);
	Check(host.Hear(kControl, keptNoPass.b, 64, 1030 + 16000) && host.Joiners() == 0 && host.UnusedMessages(kNetRelayNoPass) == 3,
		"HOST_KEPT with no pass is taken and not used: every one has a fresh pass");
	Check(host.KeepMs() == -1, "and a HOST_KEPT that isn't used times nothing");
	Bytes const kept = Make(kNetRelayHostKept, kToken, NULL, 0, 3, kNewerPass, kPassTime + 15);
	Check(host.Hear(kControl, kept.b, 64, 1030 + 16000) && host.Joiners() == 3 && host.PassesHeard() == 2, "HOST_KEPT with its token says how many joiners there are");
	Check(host.KeepMs() == 1000, "and times the keep it answers: 1,000 ms from HOST_KEEP to it");
	Check(host.RoomForTheList(codeText, pass, &passTime) && memcmp(pass, kNewerPass, 16) == 0 && passTime == kPassTime + 15, "and gives the newer pass, for the list");
	Bytes const keptMany = Make(kNetRelayHostKept, kToken, NULL, 0, 60000, kNewerPass, kPassTime + 30);
	Check(host.Hear(kControl, keptMany.b, 64, 1030 + 16000) && host.Joiners() == static_cast<int>(kNetRelayMostJoiners), "a count past 8 is taken as 8");
	Check(host.KeepMs() == 1000, "and a second HOST_KEPT for the same keep leaves its round trip as it was");

	host.Tick(1030 + 16000 + kNetRelayKeptWithinMs - 1);
	SendCount(host);
	Check(host.Open(), "a room kept within 45 s stays open");
	host.Tick(1030 + 16000 + kNetRelayKeptWithinMs);
	Check(!host.Open() && host.LostWhy() == kNetRelayNoKeep && !host.RoomForTheList(codeText, pass, &passTime),
		"with no HOST_KEPT for 45 s, the room is lost, says why, and there's nothing more for the list");

	// The joiners' hellos
	NetRelayHost joined;
	OpenHost(joined);
	Bytes const otherJoiner = Make(kNetRelayNewJoiner, kOther, NULL, kJoinerPort, 0);
	Check(joined.Hear(kControl, otherJoiner.b, 64, 100) && SendCount(joined) == 0 && joined.UnusedMessages(kNetRelayWrongSecret) == 1,
		"NEW_JOINER with another room's token: no hello");
	Bytes const zeroPort = Make(kNetRelayNewJoiner, kToken, NULL, 0, 0);
	Bytes const controlPort = Make(kNetRelayNewJoiner, kToken, NULL, kControl, 0);
	Check(joined.Hear(kControl, zeroPort.b, 64, 100) && joined.Hear(kControl, controlPort.b, 64, 100) && SendCount(joined) == 0
		&& joined.UnusedMessages(kNetRelayBadPort) == 2, "nor for port 0, nor for the control port itself");

	Bytes const newJoiner = Make(kNetRelayNewJoiner, kToken, NULL, kJoinerPort, 0);
	Check(joined.Hear(kControl, newJoiner.b, 64, 100) && OneSend(joined, &port, &m) && port == kJoinerPort && m.type == kNetRelayHostHello
		&& memcmp(m.secret, kToken, 16) == 0 && m.port == kJoinerPort, "NEW_JOINER: HOST_HELLO at once, to that port, with its token and the port");
	Check(!joined.Hear(kJoinerPort, newJoiner.b, 64, 100), "a NEW_JOINER from a joiner's port isn't the relay's: it's left for the game");
	joined.Tick(100 + kNetRelayHostHelloEveryMs - 1);
	Check(SendCount(joined) == 0, "no second hello within a second");
	joined.Tick(100 + kNetRelayHostHelloEveryMs);
	Check(OneSend(joined, &port, &m) && port == kJoinerPort && m.type == kNetRelayHostHello, "a second later, the hello again");
	joined.JoinerConnected(kJoinerPort);
	joined.Tick(100 + 3 * kNetRelayHostHelloEveryMs);
	Check(SendCount(joined) == 0 && joined.HellosWaiting() == 0, "once that joiner's game has connected, no more hellos");

	NetRelayHost unanswered;
	OpenHost(unanswered);
	unanswered.Hear(kControl, newJoiner.b, 64, 100);
	SendCount(unanswered);
	unanswered.Tick(100 + kNetRelayWaitForReadyMs);
	Check(SendsOfKind(unanswered, kNetRelayHostHello) == 0 && unanswered.HellosWaiting() == 0,
		"hellos to a joiner who never connects stop after 15 s, as the relay frees its port");

	NetRelayHost full;
	OpenHost(full);
	bool tookEight = true;

	for (uint16_t p = 0; p < kNetRelayMostJoiners; p++)
	{
		Bytes const one = Make(kNetRelayNewJoiner, kToken, NULL, static_cast<uint16_t>(kJoinerPort + p), 0);
		tookEight = tookEight && full.Hear(kControl, one.b, 64, 100);
	}

	SendCount(full);
	Bytes const ninth = Make(kNetRelayNewJoiner, kToken, NULL, static_cast<uint16_t>(kJoinerPort + 8), 0);
	Check(tookEight && full.HellosWaiting() == 8 && full.Hear(kControl, ninth.b, 64, 100) && SendCount(full) == 0 && full.UnusedMessages(kNetRelayNoRoom) == 1,
		"8 joiners' hellos at once, and a ninth is counted and not said");
	full.Hear(kControl, newJoiner.b, 64, 200);
	Check(SendCount(full) == 1 && full.HellosWaiting() == 8, "a NEW_JOINER again for a port it has already says hello again, and takes no more room");

	// Refusals and silence
	NetRelayHost refused;
	refused.Start(kControl, 0);
	SendCount(refused);
	Bytes const busy = Make(kNetRelayRefused, NULL, NULL, 0, 3);
	Check(refused.Hear(kControl, busy.b, 64, 10) && !refused.Open() && !refused.Asking() && refused.LostWhy() == kNetRelayRefusedBusy,
		"REFUSED, the relay too busy: no room, and that's the reason");
	Bytes const odd = Make(kNetRelayRefused, NULL, NULL, 0, 77);
	NetRelayHost refusedOdd;
	refusedOdd.Start(kControl, 0);
	refusedOdd.Hear(kControl, odd.b, 64, 10);
	Check(refusedOdd.LostWhy() == kNetRelayRefusedOther, "REFUSED with a reason it doesn't know: \"a reason this version doesn't know\"");

	NetRelayHost silent;
	silent.Start(kControl, 0);
	SendCount(silent);
	silent.Tick(kNetRelayAskAgainMs);
	Check(OneSend(silent, &port, &m) && m.type == kNetRelayHostOpen && NetRelay_AllZero(m.secret, 16), "with no answer, it asks again after a second, without the old cookie");
	silent.Tick(kNetRelayGiveUpAskingMs);
	Check(!silent.Asking() && silent.LostWhy() == kNetRelayNoAnswer, "and gives up after 10 s, saying the relay didn't answer");

	// 64 bytes of A: the second byte is already not AVPR's V
	uint8_t junk[64];
	memset(junk, 0x41, sizeof(junk));
	NetRelayHost junked;
	junked.Start(kControl, 0);
	Check(junked.Hear(kControl, junk, 64, 1) && junked.NotMessages(kNetRelayNotMagic) == 1 && junked.Asking(),
		"64 bytes from the control port that aren't a message are taken, counted by why, and change nothing");
	Check(junked.Hear(kControl, junk, 10, 1) && junked.NotMessages(kNetRelayNotMessageSize) == 1, "and so are 10 bytes");
	Check(!junked.Hear(kJoinerPort, junk, 10, 1), "but anything from another port is the game's");

	// Closing
	NetRelayHost closing;
	OpenHost(closing);
	closing.Close();
	Check(OneSend(closing, &port, &m) && port == kControl && m.type == kNetRelayHostClose && memcmp(m.secret, kToken, 16) == 0 && closing.LostWhy() == kNetRelayClosed,
		"closing sends HOST_CLOSE with its token, and the room is done");
	NetRelayHost neverOpened;
	neverOpened.Close();
	Check(SendCount(neverOpened) == 0 && neverOpened.LostWhy() == kNetRelayFine, "a host that never asked for a room sends nothing on closing");
	Check(!neverOpened.Hear(kControl, opened.b, 64, 0), "and hears nothing: it isn't using a relay");

	// The sends' room
	NetRelayHost flooded;
	flooded.Start(kControl, 0);

	for (size_t i = 0; i < kNetRelayMostSends + 4; i++)
	{
		flooded.Hear(kControl, cookie.b, 64, 1);
	}

	Check(SendCount(flooded) == kNetRelayMostSends && flooded.SendsDropped() == 5, "a flood of cookies queues at most 16 sends, and counts the rest as dropped");
}

// A joiner that has its port, at time 0
static NetRelayJoiner &JoinerWithPort(NetRelayJoiner &joiner)
{
	joiner.Start(kControl, kCode, 0);
	SendCount(joiner);
	Bytes const given = Make(kNetRelayJoinPort, kOther, kCode, kJoinerPort, 0);
	joiner.Hear(kControl, given.b, 64, 10);
	SendCount(joiner);
	return joiner;
}

static void TheJoiner()
{
	printf("The joiner\n");
	NetRelayJoiner joiner;
	uint16_t port = 0;
	NetRelayMessage m;

	Check(!joiner.Start(kControl, "ABCD234O", 0) && SendCount(joiner) == 0 && !joiner.Ready() && !joiner.Failed(), "a code with O in it: no join, and nothing sent");

	Check(joiner.Start(kControl, kCode, 1000) && OneSend(joiner, &port, &m) && port == kControl && m.type == kNetRelayJoin && memcmp(m.code, kCode, 8) == 0
		&& NetRelay_AllZero(m.secret, 16), "it asks the control port to join that game, with no cookie the first time");

	Bytes const cookie = Make(kNetRelayCookie, kCookie, NULL, 0, 0);
	Check(joiner.Hear(kControl, cookie.b, 64, 1010) && OneSend(joiner, &port, &m) && m.type == kNetRelayJoin && memcmp(m.secret, kCookie, 16) == 0
		&& memcmp(m.code, kCode, 8) == 0, "given a cookie, it asks again with it, and the code");

	Bytes const otherGame = Make(kNetRelayJoinPort, kOther, kOtherCode, kJoinerPort, 0);
	Check(joiner.Hear(kControl, otherGame.b, 64, 1020) && SendCount(joiner) == 0 && joiner.UnusedMessages(kNetRelayWrongSecret) == 1,
		"a port for another game's code isn't taken");
	Bytes const noTicket = Make(kNetRelayJoinPort, NULL, kCode, kJoinerPort, 0);
	Check(joiner.Hear(kControl, noTicket.b, 64, 1020) && SendCount(joiner) == 0 && joiner.UnusedMessages(kNetRelayWrongSecret) == 2,
		"nor one with a ticket of all 0");
	Bytes const controlAsPort = Make(kNetRelayJoinPort, kOther, kCode, kControl, 0);
	Check(joiner.Hear(kControl, controlAsPort.b, 64, 1020) && SendCount(joiner) == 0 && joiner.UnusedMessages(kNetRelayBadPort) == 1,
		"nor the control port itself as its port");

	Bytes const given = Make(kNetRelayJoinPort, kOther, kCode, kJoinerPort, 0);
	Check(!joiner.Hear(kJoinerPort, given.b, 64, 1030), "JOIN_PORT from a port it hasn't been given isn't the relay's: it's left alone");
	Check(joiner.Hear(kControl, given.b, 64, 1030) && OneSend(joiner, &port, &m) && port == kJoinerPort && m.type == kNetRelayJoinerHello
		&& memcmp(m.secret, kOther, 16) == 0, "JOIN_PORT: JOINER_HELLO at once, to its port, with its ticket");
	Check(!joiner.Ready() && joiner.Port() == 0, "not ready yet, so there's no port to connect to");

	joiner.Tick(1030 + kNetRelayJoinerHelloEveryMs - 1);
	Check(SendCount(joiner) == 0, "no second hello within half a second");
	joiner.Tick(1030 + kNetRelayJoinerHelloEveryMs);
	Check(OneSend(joiner, &port, &m) && port == kJoinerPort && m.type == kNetRelayJoinerHello, "half a second on, the hello again");

	Bytes const readyFromControl = Make(kNetRelayReady, kOther, NULL, kJoinerPort, 0);
	Check(joiner.Hear(kControl, readyFromControl.b, 64, 1600) && !joiner.Ready() && joiner.UnusedMessages(kNetRelayNotForNow) == 1,
		"READY from the control port isn't READY: it comes from the joiner's own port");
	Bytes const readyWrongTicket = Make(kNetRelayReady, kToken, NULL, kJoinerPort, 0);
	Check(joiner.Hear(kJoinerPort, readyWrongTicket.b, 64, 1600) && !joiner.Ready() && joiner.UnusedMessages(kNetRelayWrongSecret) == 3,
		"READY with another ticket: not ready");
	Bytes const readyWrongPort = Make(kNetRelayReady, kOther, NULL, static_cast<uint16_t>(kJoinerPort + 1), 0);
	Check(joiner.Hear(kJoinerPort, readyWrongPort.b, 64, 1600) && !joiner.Ready() && joiner.UnusedMessages(kNetRelayWrongSecret) == 4,
		"READY naming another port: not ready");

	Bytes const ready = Make(kNetRelayReady, kOther, NULL, kJoinerPort, 0);
	Check(joiner.Hear(kJoinerPort, ready.b, 64, 1600) && joiner.Ready() && joiner.Port() == kJoinerPort, "READY with its ticket, from its port: ready, to connect to that port");
	joiner.Tick(1600 + 2 * kNetRelayJoinerHelloEveryMs);
	Check(SendCount(joiner) == 0, "and its hellos stop");
	Check(joiner.Hear(kJoinerPort, ready.b, 64, 1700) && joiner.Ready(), "a READY sent again is taken, and changes nothing");

	uint8_t gamePacket[64];
	memset(gamePacket, 0x5A, sizeof(gamePacket));
	Check(!joiner.Hear(kJoinerPort, gamePacket, 64, 1800) && !joiner.Hear(kJoinerPort, gamePacket, 20, 1800) && joiner.NotMessages(kNetRelayNotMagic) == 0,
		"the game's own packets from its port, 64 bytes or not, are left for the game and not counted");
	Check(!joiner.Hear(27600, gamePacket, 64, 1800), "and anything from a port it wasn't given is not its");

	// Failing
	NetRelayJoiner noGame;
	noGame.Start(kControl, kCode, 0);
	SendCount(noGame);
	Bytes const refused = Make(kNetRelayRefused, NULL, NULL, 0, 1);
	Check(noGame.Hear(kControl, refused.b, 64, 10) && noGame.Failed() && noGame.FailedWhy() == kNetRelayRefusedNoGame
		&& strcmp(NetRelay_WhyText(noGame.FailedWhy()), "the relay knows no game with that code") == 0, "REFUSED, no such game: it fails, and says so in words");

	NetRelayJoiner noHost;
	JoinerWithPort(noHost);
	noHost.Tick(10 + kNetRelayWaitForReadyMs - 1);
	SendCount(noHost);
	Check(!noHost.Failed(), "still waiting for READY just under 15 s after its port came");
	noHost.Tick(10 + kNetRelayWaitForReadyMs);
	Check(noHost.Failed() && noHost.FailedWhy() == kNetRelayNoReady, "and at 15 s it fails: the host never answered on its port");
	Check(noHost.Hear(kJoinerPort, ready.b, 64, 20000) && !noHost.Ready(), "a READY after that changes nothing");

	NetRelayJoiner silent;
	silent.Start(kControl, kCode, 0);
	SendCount(silent);
	silent.Tick(kNetRelayAskAgainMs);
	Check(OneSend(silent, &port, &m) && m.type == kNetRelayJoin && memcmp(m.code, kCode, 8) == 0, "with no answer, it asks again after a second");
	silent.Tick(kNetRelayGiveUpAskingMs);
	Check(silent.Failed() && silent.FailedWhy() == kNetRelayNoAnswer, "and gives up after 10 s");

	NetRelayJoiner lateRefusal;
	JoinerWithPort(lateRefusal);
	Check(lateRefusal.Hear(kControl, refused.b, 64, 20) && !lateRefusal.Failed() && lateRefusal.UnusedMessages(kNetRelayNotForNow) == 1,
		"a REFUSED once it has its port is taken and not used: it's waiting for READY now");

	NetRelayJoiner idle;
	Check(!idle.Hear(kControl, cookie.b, 64, 0), "a joiner that never started hears nothing: it isn't using a relay");
}

// ---- The relay's test vectors (tests/vectors/relay-v1.json)
//
// The file is JSON. Only as much of JSON as the file uses is read here: objects, lists, texts (with \" and \\ the only escapes), whole
// numbers from 0 up, true, false and null. Anything else in it fails the reading, and so the test, as a damaged file should.

int const kJsonNothing = 0;     // null
int const kJsonTrueFalse = 1;
int const kJsonNumber = 2;
int const kJsonText = 3;
int const kJsonList = 4;
int const kJsonObject = 5;

struct JsonValue
{
	int kind = kJsonNothing;
	bool truth = false;
	uint64_t number = 0;
	std::string text;
	std::vector<JsonValue> items;     // a list's values, in order
	std::vector<std::string> names;   // an object's names, in the file's order...
	std::vector<JsonValue> values;    // ...and the value each one has

	// An object's value with this name, or NULL if it has none
	const JsonValue *Member(const char *name) const
	{
		for (size_t i = 0; i < names.size(); i++)
		{
			if (names[i] == name)
			{
				return &values[i];
			}
		}

		return NULL;
	}
};

class JsonReader
{
public:
	explicit JsonReader(const std::string &all) : text(all), at(0)
	{
	}

	// Reads the whole text as one value, with nothing after it but spaces and line ends. False if it isn't that.
	bool ReadAll(JsonValue *out)
	{
		if (!Value(out, 0))
		{
			return false;
		}

		Spaces();
		return at == text.size();
	}

private:
	// How deep lists and objects can be inside each other. The file goes 4 deep; this stops a damaged one from going on and on.
	static size_t const kMostDepth = 16;

	const std::string &text;
	size_t at;   // how far it has read

	void Spaces()
	{
		while (at < text.size() && (text[at] == ' ' || text[at] == '\n' || text[at] == '\r' || text[at] == '\t'))
		{
			at++;
		}
	}

	// Takes this one letter, after any spaces. False, taking nothing, if the next letter is another.
	bool Take(char c)
	{
		Spaces();

		if (at < text.size() && text[at] == c)
		{
			at++;
			return true;
		}

		return false;
	}

	// Takes this word if it's next
	bool Word(const char *word)
	{
		size_t const n = strlen(word);

		if (text.compare(at, n, word) == 0)
		{
			at += n;
			return true;
		}

		return false;
	}

	bool Text(std::string *out)
	{
		if (!Take('"'))
		{
			return false;
		}

		out->clear();

		while (at < text.size())
		{
			char const c = text[at++];

			if (c == '"')
			{
				return true;
			}

			// A line end or other control letter can't be inside a JSON text
			if (static_cast<unsigned char>(c) < 0x20)
			{
				return false;
			}

			if (c == '\\')
			{
				if (at >= text.size() || (text[at] != '"' && text[at] != '\\'))
				{
					return false;
				}

				*out += text[at++];
			}
			else
			{
				*out += c;
			}
		}

		return false;   // the text never ended
	}

	bool Number(uint64_t *out)
	{
		size_t const first = at;
		uint64_t n = 0;

		while (at < text.size() && text[at] >= '0' && text[at] <= '9')
		{
			uint64_t const digit = static_cast<uint64_t>(text[at] - '0');

			// One more digit would need more than 64 bits
			if (n > (UINT64_MAX - digit) / 10)
			{
				return false;
			}

			n = n * 10 + digit;
			at++;
		}

		// No digits at all, or a 0 in front of others, which JSON doesn't allow. A "-", "." or "e" after the digits isn't taken, and then
		// whatever reads next fails on it.
		if (at == first || (text[first] == '0' && at - first > 1))
		{
			return false;
		}

		*out = n;
		return true;
	}

	bool Value(JsonValue *out, size_t depth)
	{
		Spaces();

		if (at >= text.size() || depth > kMostDepth)
		{
			return false;
		}

		char const c = text[at];

		if (c == '{')
		{
			at++;
			out->kind = kJsonObject;

			if (Take('}'))
			{
				return true;
			}

			do
			{
				std::string name;
				JsonValue value;

				// A name given twice would leave which value counts to whoever reads it, so that's refused too
				if (!Text(&name) || out->Member(name.c_str()) || !Take(':') || !Value(&value, depth + 1))
				{
					return false;
				}

				out->names.push_back(name);
				out->values.push_back(value);
			} while (Take(','));

			return Take('}');
		}

		if (c == '[')
		{
			at++;
			out->kind = kJsonList;

			if (Take(']'))
			{
				return true;
			}

			do
			{
				JsonValue item;

				if (!Value(&item, depth + 1))
				{
					return false;
				}

				out->items.push_back(item);
			} while (Take(','));

			return Take(']');
		}

		if (c == '"')
		{
			out->kind = kJsonText;
			return Text(&out->text);
		}

		if (c >= '0' && c <= '9')
		{
			out->kind = kJsonNumber;
			return Number(&out->number);
		}

		if (Word("true"))
		{
			out->kind = kJsonTrueFalse;
			out->truth = true;
			return true;
		}

		if (Word("false"))
		{
			out->kind = kJsonTrueFalse;
			return true;
		}

		if (Word("null"))
		{
			out->kind = kJsonNothing;
			return true;
		}

		return false;
	}
};

// A whole file of this repository, byte for byte, found from this test's own place: /FC in its settings makes __FILE__ the whole path of
// this file. Empty if it can't be read.
static std::string ReadRepositoryFile(const char *name)
{
	std::string const here = __FILE__;
	size_t const tests = here.rfind("tests");

	if (tests == std::string::npos)
	{
		return "";
	}

	std::ifstream file((here.substr(0, tests) + name).c_str(), std::ios::binary);
	std::stringstream all;
	all << file.rdbuf();
	return all.str();
}

static int HexDigit(char c)
{
	if (c >= '0' && c <= '9')
	{
		return c - '0';
	}

	if (c >= 'a' && c <= 'f')
	{
		return c - 'a' + 10;
	}

	return -1;   // not a hex digit, or a capital one, which the file never writes
}

// Bytes from hex digits, two to a byte. False for an odd count, or anything that isn't a hex digit.
static bool FromHex(const std::string &hex, std::vector<uint8_t> *out)
{
	out->clear();

	if (hex.size() % 2 != 0)
	{
		return false;
	}

	for (size_t i = 0; i < hex.size(); i += 2)
	{
		int const high = HexDigit(hex[i]);
		int const low = HexDigit(hex[i + 1]);

		if (high < 0 || low < 0)
		{
			return false;
		}

		out->push_back(static_cast<uint8_t>(high * 16 + low));
	}

	return true;
}

// A field of n bytes as the file gives it: "" for one its kind doesn't use, which is all 0, or exactly n bytes in hex
static bool FieldBytes(const JsonValue *field, size_t n, uint8_t *out)
{
	if (!field || field->kind != kJsonText)
	{
		return false;
	}

	if (field->text.empty())
	{
		memset(out, 0, n);
		return true;
	}

	std::vector<uint8_t> bytes;

	if (!FromHex(field->text, &bytes) || bytes.size() != n)
	{
		return false;
	}

	memcpy(out, bytes.data(), n);
	return true;
}

// The code as the file gives it: "" for none, which is all 0, or its 8 letters as they are
static bool FieldCode(const JsonValue *field, char *out)
{
	if (!field || field->kind != kJsonText || (!field->text.empty() && field->text.size() != kNetRelayCodeLetters))
	{
		return false;
	}

	memset(out, 0, kNetRelayCodeLetters);
	memcpy(out, field->text.data(), field->text.size());
	return true;
}

static bool FieldNumber(const JsonValue *field, uint64_t most, uint64_t *out)
{
	if (!field || field->kind != kJsonNumber || field->number > most)
	{
		return false;
	}

	*out = field->number;
	return true;
}

// The message a vector's "fields" describe. False if one of its 7 fields is missing, there's one more, or a number is too big for its field.
static bool FromFields(const JsonValue *fields, NetRelayMessage *m)
{
	uint64_t type = 0, port = 0, number = 0, passTime = 0;

	if (!fields || fields->kind != kJsonObject || fields->names.size() != 7 || !FieldNumber(fields->Member("type"), 255, &type)
		|| !FieldNumber(fields->Member("port"), 65535, &port) || !FieldNumber(fields->Member("number"), 65535, &number)
		|| !FieldNumber(fields->Member("passTime"), UINT64_MAX, &passTime))
	{
		return false;
	}

	*m = NetRelay_Message(static_cast<int>(type));
	m->port = static_cast<uint16_t>(port);
	m->number = static_cast<uint16_t>(number);
	m->passTime = passTime;
	return FieldBytes(fields->Member("secret"), kNetRelaySecretBytes, m->secret) && FieldCode(fields->Member("code"), m->code)
		&& FieldBytes(fields->Member("pass"), kNetRelayPassBytes, m->pass);
}

static bool SameMessage(const NetRelayMessage &a, const NetRelayMessage &b)
{
	return a.type == b.type && memcmp(a.secret, b.secret, kNetRelaySecretBytes) == 0 && memcmp(a.code, b.code, kNetRelayCodeLetters) == 0
		&& a.port == b.port && a.number == b.number && memcmp(a.pass, b.pass, kNetRelayPassBytes) == 0 && a.passTime == b.passTime;
}

// The file's word for why some bytes aren't a message, as NetRelay_Read's reason. -1 for a word this test doesn't know, which fails it:
// a new word means the relay has a rule the game doesn't.
static int ReasonForWord(const std::string &why)
{
	static const struct
	{
		const char *word;
		int reason;
	} kWords[] = {
		{ "size", kNetRelayNotMessageSize }, { "magic", kNetRelayNotMagic }, { "version", kNetRelayNotVersion }, { "type", kNetRelayNotType },
		{ "zero", kNetRelayNotZero }, { "code", kNetRelayNotCode }, { "unused", kNetRelayNotUnused }, { "passtime", kNetRelayNotPassTime },
	};

	for (const auto &w : kWords)
	{
		if (why == w.word)
		{
			return w.reason;
		}
	}

	return -1;
}

// A vector's text field, or NULL if it hasn't one
static const JsonValue *TextMember(const JsonValue &vector, const char *name)
{
	const JsonValue *const field = vector.Member(name);
	return field && field->kind == kJsonText ? field : NULL;
}

static void TheSharedVectors()
{
	printf("The relay's own test vectors (tests/vectors/relay-v1.json)\n");

	std::string const all = ReadRepositoryFile("tests/vectors/relay-v1.json");
	JsonValue file;
	JsonReader reader(all);
	bool const readable = !all.empty() && reader.ReadAll(&file) && file.kind == kJsonObject;
	Check(readable, "the file is there, and is JSON this test can read");

	if (!readable)
	{
		return;
	}

	const JsonValue *const protocol = TextMember(file, "protocol");
	Check(protocol && protocol->text == "PROTOCOL.md as at 7014de1", "it was made for PROTOCOL.md as at 7014de1, the one net_relay.h follows");

	const JsonValue *const messages = file.Member("messages");
	const JsonValue *const refused = file.Member("refused");
	bool const lists = messages && messages->kind == kJsonList && refused && refused->kind == kJsonList;
	Check(lists && messages->items.size() == 24 && refused->items.size() == 81, "it has its 24 messages and its 81 refused packets");

	if (!lists)
	{
		return;
	}

	// Each message: read, it gives the fields the file lists; what was read, written again, gives the same bytes; and the file's fields
	// alone, written, give them too
	size_t readRight = 0, writtenRight = 0, madeRight = 0;
	bool kindSeen[kNetRelayMostType + 1] = {};

	for (const JsonValue &vector : messages->items)
	{
		const JsonValue *const name = TextMember(vector, "name");
		const JsonValue *const hex = TextMember(vector, "hex");
		std::vector<uint8_t> bytes;
		NetRelayMessage expected = NetRelay_Message(0);
		NetRelayMessage got = NetRelay_Message(0);
		bool const usable = name && hex && FromHex(hex->text, &bytes) && FromFields(vector.Member("fields"), &expected);
		bool const reads = usable && NetRelay_Read(bytes.data(), bytes.size(), &got) == kNetRelayIsAMessage && SameMessage(got, expected);
		bool writes = false, made = false;

		if (reads)
		{
			uint8_t again[kNetRelayMessageBytes];
			NetRelay_Write(got, again);
			writes = memcmp(again, bytes.data(), kNetRelayMessageBytes) == 0;
			kindSeen[got.type] = true;
		}

		if (usable && bytes.size() == kNetRelayMessageBytes)
		{
			uint8_t fresh[kNetRelayMessageBytes];
			NetRelay_Write(expected, fresh);
			made = memcmp(fresh, bytes.data(), kNetRelayMessageBytes) == 0;
		}

		readRight += reads ? 1 : 0;
		writtenRight += writes ? 1 : 0;
		madeRight += made ? 1 : 0;

		if (!reads || !writes || !made)
		{
			printf("    wrong here: %s\n", name ? name->text.c_str() : "(a message with no name)");
		}
	}

	Check(readRight == messages->items.size(), "every message reads to the fields the file lists for it");
	Check(writtenRight == messages->items.size(), "every message read, then written again, is the same 64 bytes");
	Check(madeRight == messages->items.size(), "and the file's fields alone, written, make those same 64 bytes");

	bool everyKind = true;

	for (int type = 1; type <= kNetRelayMostType; type++)
	{
		everyKind = everyKind && kindSeen[type];
	}

	Check(everyKind, "all 15 kinds of message are among them");

	// Each refused packet: refused, and for the reason the file gives
	size_t refusedRight = 0;
	bool reasonSeen[kNetRelayNotKinds] = {};

	for (const JsonValue &vector : refused->items)
	{
		const JsonValue *const name = TextMember(vector, "name");
		const JsonValue *const hex = TextMember(vector, "hex");
		const JsonValue *const why = TextMember(vector, "why");
		std::vector<uint8_t> bytes;
		NetRelayMessage untouched = NetRelay_Message(0);
		int const reason = why ? ReasonForWord(why->text) : -1;
		bool const right = name && hex && FromHex(hex->text, &bytes) && reason > 0 && NetRelay_Read(bytes.data(), bytes.size(), &untouched) == reason;

		if (right)
		{
			refusedRight++;
			reasonSeen[reason] = true;
		}
		else
		{
			printf("    wrong here: %s\n", name ? name->text.c_str() : "(a refused packet with no name)");
		}
	}

	Check(refusedRight == refused->items.size(), "every packet the file lists as refused is refused here, each for the same reason");

	bool everyReason = true;

	for (int reason = 1; reason < kNetRelayNotKinds; reason++)
	{
		everyReason = everyReason && reasonSeen[reason];
	}

	Check(everyReason, "and each reason NetRelay_Read can give is among them, the pass time's too");

	// The file's passes aren't tried here: the game never makes or checks one, it only hands the relay's on to the list. The relay's and
	// the list's own tests try them.
}

static void TheTimeWrapping()
{
	printf("The time going past 2^32 ms\n");
	uint32_t const nearTheEnd = 0xFFFFFF00u;
	NetRelayHost host;
	host.Start(kControl, nearTheEnd);
	SendCount(host);
	Bytes const opened = Make(kNetRelayHostOpened, kToken, kCode, 0, 0, kPass, kPassTime);
	host.Hear(kControl, opened.b, 64, nearTheEnd);
	host.Tick(nearTheEnd + kNetRelayKeepEveryMs);   // past 2^32: the sum comes round to a small number
	NetRelayMessage m;
	uint16_t port = 0;
	Check(OneSend(host, &port, &m) && m.type == kNetRelayHostKeep && host.Open(), "the keep comes 15 s on, though the game's clock came round in between");
}

int main()
{
	TheMessages();
	TheSharedVectors();
	PickingARelay();
	TheHost();
	TheJoiner();
	TheTimeWrapping();

	if (checksFailed > 0)
	{
		printf("%d check%s FAILED\n", checksFailed, checksFailed == 1 ? "" : "s");
		return 1;
	}

	printf("Every check passed\n");
	return 0;
}
