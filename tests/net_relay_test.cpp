// The test of the game's side of the relay (3dc/net_relay.h). Written 2026-11-08 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// The relay's messages: that each kind is written byte for byte as PROTOCOL.md says (AVPx-Dedicated-Server, d75a5bd), that reading gives
// back what was written, and that anything that isn't exactly a message is refused, each with the reason NetRelay_Read gives for it. Then the
// host's and the joiner's state machines: each step of a game's life, every time limit, and what each does with a message that's wrong in
// some way (another room's token, another joiner's ticket, a port that can't be a joiner's, the wrong sender).
//
// WHAT IT DOESN'T TEST
//
// The glue in networking.cpp that sends these from the game's own socket, and the relay itself (its own tests are in its repository). So this
// shows the two ends do the right thing with what they're given, and nothing here sends a packet.

#include <stdio.h>
#include <string.h>

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
static const char kCode[] = "ABCD2345";
static const char kOtherCode[] = "ABCD2346";

// A message's bytes, with these fields
struct Bytes
{
	uint8_t b[kNetRelayMessageBytes];
};

static Bytes Make(int type, const uint8_t *secret, const char *code, uint16_t port, uint16_t number)
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

	m.port = port;
	m.number = number;
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

static void TheMessages()
{
	printf("The relay's messages\n");

	Bytes const open = Make(kNetRelayJoinPort, kToken, kCode, 27501, 0x0102);
	Check(open.b[0] == 'A' && open.b[1] == 'V' && open.b[2] == 'P' && open.b[3] == 'R' && open.b[4] == 1 && open.b[5] == kNetRelayJoinPort,
		"a message starts AVPR, then the version, 1, then its kind");
	Check(open.b[32] == 0x6D && open.b[33] == 0x6B && open.b[34] == 0x02 && open.b[35] == 0x01, "its port and its number are written low byte first: 27501 is 6D 6B");
	Check(memcmp(open.b + 8, kToken, 16) == 0 && memcmp(open.b + 24, kCode, 8) == 0 && open.b[6] == 0 && open.b[7] == 0 && NetRelay_AllZero(open.b + 36, 28),
		"the secret is at 8, the code at 24, and the rest is 0");

	NetRelayMessage m;
	Check(NetRelay_Read(open.b, kNetRelayMessageBytes, &m) == kNetRelayIsAMessage && m.type == kNetRelayJoinPort && m.port == 27501 && m.number == 0x0102
		&& memcmp(m.secret, kToken, 16) == 0 && memcmp(m.code, kCode, 8) == 0, "reading it gives back every field");

	bool everyKind = true;

	for (int type = 1; type <= kNetRelayMostType; type++)
	{
		Bytes const one = Make(type, NULL, NULL, 0, 0);
		everyKind = everyKind && NetRelay_Read(one.b, kNetRelayMessageBytes, &m) == kNetRelayIsAMessage && m.type == type;
	}

	Check(everyKind, "each of the 13 kinds is a message");

	// What isn't one, and why
	Check(NetRelay_Read(open.b, 63, &m) == kNetRelayNotMessageSize && NetRelay_Read(open.b, 0, &m) == kNetRelayNotMessageSize
		&& NetRelay_Read(NULL, 64, &m) == kNetRelayNotMessageSize, "63 bytes, none, or no packet at all: not exactly 64 bytes");
	uint8_t longer[65];
	memcpy(longer, open.b, 64);
	longer[64] = 0;
	Check(NetRelay_Read(longer, 65, &m) == kNetRelayNotMessageSize, "65 bytes: not exactly 64");

	Bytes bad = open;
	bad.b[3] = 'S';
	Check(NetRelay_Read(bad.b, 64, &m) == kNetRelayNotMagic, "another magic: not AVPR");
	bad = open;
	bad.b[4] = 2;
	Check(NetRelay_Read(bad.b, 64, &m) == kNetRelayNotVersion, "version 2: another version");
	bad = open;
	bad.b[5] = 0;
	Check(NetRelay_Read(bad.b, 64, &m) == kNetRelayNotType, "kind 0: no such kind");
	bad.b[5] = 14;
	Check(NetRelay_Read(bad.b, 64, &m) == kNetRelayNotType, "kind 14: no such kind");
	bad.b[5] = 255;
	Check(NetRelay_Read(bad.b, 64, &m) == kNetRelayNotType, "kind 255: no such kind");
	bad = open;
	bad.b[7] = 1;
	Check(NetRelay_Read(bad.b, 64, &m) == kNetRelayNotZero, "a reserved byte that isn't 0");
	bad = open;
	bad.b[36] = 1;
	Check(NetRelay_Read(bad.b, 64, &m) == kNetRelayNotZero, "the first padding byte not 0");
	bad = open;
	bad.b[63] = 0x80;
	Check(NetRelay_Read(bad.b, 64, &m) == kNetRelayNotZero, "the last padding byte not 0");

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

	Check(strcmp(NetRelay_NotAMessageWhy(kNetRelayNotMagic), "it doesn't start with AVPR") == 0 && strcmp(NetRelay_NotAMessageWhy(99), "no such reason") == 0,
		"each reason has its words, and a number that isn't one says so");

	// Codes and secrets
	char code[8];
	Check(NetRelay_CodeFromText("abcd-2345", code) && memcmp(code, kCode, 8) == 0, "a typed code in small letters with a dash is the code");
	Check(NetRelay_CodeFromText(" ABCD 2345 ", code) && memcmp(code, kCode, 8) == 0, "and with spaces round it and in it");
	char kept[8] = { 'K', 'K', 'K', 'K', 'K', 'K', 'K', 'K' };
	Check(!NetRelay_CodeFromText("ABCD234", kept) && !NetRelay_CodeFromText("ABCD23456", kept) && !NetRelay_CodeFromText("ABCD2340", kept)
		&& !NetRelay_CodeFromText("", kept) && !NetRelay_CodeFromText(NULL, kept) && kept[0] == 'K',
		"7 letters, 9, a 0 in it, none, or no text at all: not a code, and nothing is written");

	char hex[33];
	NetRelay_SecretHex(kToken, hex);
	Check(strcmp(hex, "0123456789abcdef1032547698badcfe") == 0, "a token as the list takes it: 32 hex digits, small letters");
	Check(NetRelay_SameSecret(kToken, kToken) && !NetRelay_SameSecret(kToken, kOther), "two secrets differing in the last bit aren't the same");

	Check(NetRelay_RefusedWhy(2) == kNetRelayRefusedFull && NetRelay_RefusedWhy(0) == kNetRelayRefusedOther && NetRelay_RefusedWhy(5) == kNetRelayRefusedOther,
		"REFUSED's reasons 1 to 4 are kept, and any other is \"a reason this version doesn't know\"");
}

// A host whose room is open, at time 0
static NetRelayHost &OpenHost(NetRelayHost &host)
{
	host.Start(kControl, 0);
	SendCount(host);
	Bytes const opened = Make(kNetRelayHostOpened, kToken, kCode, 0, 0);
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

	Bytes const zeroToken = Make(kNetRelayHostOpened, NULL, kCode, 0, 0);
	Check(host.Hear(kControl, zeroToken.b, 64, 1020) && host.Asking() && host.UnusedMessages(kNetRelayWrongSecret) == 1,
		"a room with a token of all 0 isn't one: it's taken, counted and not used");
	Bytes const noCode = Make(kNetRelayHostOpened, kToken, NULL, 0, 0);
	Check(host.Hear(kControl, noCode.b, 64, 1020) && host.Asking() && host.UnusedMessages(kNetRelayWrongSecret) == 2, "nor one with no code");

	Bytes const opened = Make(kNetRelayHostOpened, kToken, kCode, 0, 0);
	Check(host.Hear(kControl, opened.b, 64, 1030) && host.Open(), "HOST_OPENED with a token and a code opens the room");
	char tokenHex[33], codeText[9];
	Check(host.Room(tokenHex, codeText) && strcmp(tokenHex, "0123456789abcdef1032547698badcfe") == 0 && strcmp(codeText, kCode) == 0,
		"the room's token and code are given, for the list");
	uint8_t tokenBytes[16];
	char codeBytes[8];
	Check(host.RoomSecret(tokenBytes, codeBytes) && memcmp(tokenBytes, kToken, 16) == 0 && memcmp(codeBytes, kCode, 8) == 0, "and as the relay gave them, for the list's calls");
	Check(host.Hear(kControl, opened.b, 64, 1040) && host.UnusedMessages(kNetRelayNotForNow) == 1 && host.Room(tokenHex, codeText) && strcmp(codeText, kCode) == 0,
		"a second HOST_OPENED is taken and not used: the room stays the one it was");
	Check(host.Hear(kControl, cookie.b, 64, 1040) && SendCount(host) == 0 && host.UnusedMessages(kNetRelayNotForNow) == 2,
		"nor does a cookie make it ask again once the room is open");

	// Keeping the room
	host.Tick(1030 + kNetRelayKeepEveryMs - 1);
	Check(SendCount(host) == 0, "nothing before 15 s");
	host.Tick(1030 + kNetRelayKeepEveryMs);
	Check(OneSend(host, &port, &m) && port == kControl && m.type == kNetRelayHostKeep && memcmp(m.secret, kToken, 16) == 0,
		"after 15 s, HOST_KEEP with its token, to the control port");

	Bytes const keptOther = Make(kNetRelayHostKept, kOther, NULL, 0, 3);
	Check(host.Hear(kControl, keptOther.b, 64, 1030 + 16000) && host.Joiners() == 0 && host.UnusedMessages(kNetRelayWrongSecret) == 3,
		"HOST_KEPT with another room's token is taken, counted and not used");
	Bytes const kept = Make(kNetRelayHostKept, kToken, NULL, 0, 3);
	Check(host.Hear(kControl, kept.b, 64, 1030 + 16000) && host.Joiners() == 3, "HOST_KEPT with its token says how many joiners there are");
	Bytes const keptMany = Make(kNetRelayHostKept, kToken, NULL, 0, 60000);
	Check(host.Hear(kControl, keptMany.b, 64, 1030 + 16000) && host.Joiners() == static_cast<int>(kNetRelayMostJoiners), "a count past 8 is taken as 8");

	host.Tick(1030 + 16000 + kNetRelayKeptWithinMs - 1);
	SendCount(host);
	Check(host.Open(), "a room kept within 45 s stays open");
	host.Tick(1030 + 16000 + kNetRelayKeptWithinMs);
	Check(!host.Open() && host.LostWhy() == kNetRelayNoKeep, "with no HOST_KEPT for 45 s, the room is lost, and says why");

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
	Check(SendCount(unanswered) == 0 && unanswered.HellosWaiting() == 0, "hellos to a joiner who never connects stop after 15 s, as the relay frees its port");

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

	uint8_t junk[64];
	memset(junk, 0x41, sizeof(junk));
	NetRelayHost junked;
	junked.Start(kControl, 0);
	Check(junked.Hear(kControl, junk, 64, 1) && junked.NotMessages(kNetRelayNotVersion) == 1 && junked.Asking(),
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
	Check(!neverOpened.Hear(kControl, opened.b, 64, 0), "and hears nothing: it isn't using the relay");

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
	Check(!idle.Hear(kControl, cookie.b, 64, 0), "a joiner that never started hears nothing: it isn't using the relay");
}

static void TheTimeWrapping()
{
	printf("The time going past 2^32 ms\n");
	uint32_t const nearTheEnd = 0xFFFFFF00u;
	NetRelayHost host;
	host.Start(kControl, nearTheEnd);
	SendCount(host);
	Bytes const opened = Make(kNetRelayHostOpened, kToken, kCode, 0, 0);
	host.Hear(kControl, opened.b, 64, nearTheEnd);
	host.Tick(nearTheEnd + kNetRelayKeepEveryMs);   // past 2^32: the sum comes round to a small number
	NetRelayMessage m;
	uint16_t port = 0;
	Check(OneSend(host, &port, &m) && m.type == kNetRelayHostKeep && host.Open(), "the keep comes 15 s on, though the game's clock came round in between");
}

int main()
{
	TheMessages();
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
