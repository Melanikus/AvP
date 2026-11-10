// The game's side of the relay (AVPx Dedicated Server): the messages it sends and hears, and what a host and a joiner do with them.
// Written 2026-11-08 by Melanikus for the AvP port.
// Modified 2026-11-08 by Melanikus for the AvP port: PROTOCOL.md 7014de1, a relay in each region: a message's pass, the rule that every field
// a kind of message doesn't use is 0, and the echoes a host times the relays with (NetRelayPicker).
// Modified 2026-11-08 by Melanikus for the AvP port: a pass time is at most 2^53 - 1, as the relay reads it (its shared test vectors).
//
// WHY THERE'S A RELAY. Many players can't be reached from the internet at all: their provider puts many homes behind one address
// (carrier-grade NAT), and no setting on their own router changes that. So a game can go through a relay, a server everyone can reach.
// There's a relay in each region of the world. The host opens a "room" on the one nearest it; each joiner gets a port of its own on that
// relay, and the relay passes the game's own packets between the two, unread and unchanged. How it all works, every message, field and limit:
// PROTOCOL.md in the AVPx-Dedicated-Server repository (at 7014de1, which this follows). That file is the single source both sides read.
//
// WHAT THIS FILE IS. Only the relays' own little messages, and three "state machines": the one that picks a relay (NetRelayPicker), the
// host's (NetRelayHost) and the joiner's (NetRelayJoiner). A state machine here is a small object that is told the time and what arrived,
// and says what to send. It never sends or reads anything itself. networking.cpp does that, from the game's own network socket, so a relay
// sees each player's game through the same door in their router that the game itself uses. Nothing here knows about the game, so
// tests/net_relay_test.cpp can try every case.
//
// EVERYTHING THAT ARRIVES IS HOSTILE (AvP Tao #3). A relay message is exactly kNetRelayMessageBytes long, and every byte has to be what
// PROTOCOL.md says, or it isn't one: NetRelay_Read says which rule it broke. A message is only listened to if it comes from where a relay
// sends such a message from (its control port), and one that names the room or the joiner has to carry the secret the relay gave it.
// Every list here has a fixed size, and every wait a time limit.
//
// A GAME'S LIFE, as the host sees it:
//
//      ECHO, 3 to each relay ------> each relay's control port, 100 ms apart
//      ECHO_BACK <------------------ the same 16 bytes: how long the round trip took. The nearest relay is the one to use.
//      HOST_OPEN ------------------> that relay's control port
//      COOKIE <--------------------- (proof we really are at this address)
//      HOST_OPEN, with the cookie ->
//      HOST_OPENED <---------------- the room's token (our secret), its code (8 letters, for the list and for typing) and a pass (the list's
//                                    proof that the room is real)
//      HOST_KEEP, every 15 s ------> (keeps the room, and our router's door to the relay, open)
//      HOST_KEPT <------------------ a fresh pass, each time, for the list
//      NEW_JOINER <----------------- a joiner is coming, on relay port P
//      HOST_HELLO, to port P ------> every second, until that joiner's game connects through port P
//      ... the game's own packets, through port P ...
//      HOST_CLOSE -----------------> when the game ends
//
// and as a joiner sees it, at the relay the list says the game is on:
//
//      JOIN, with the code --------> relay control port
//      COOKIE <---------------------
//      JOIN, with the cookie ------>
//      JOIN_PORT <------------------ its ticket (its secret) and its port P on the relay
//      JOINER_HELLO, to port P ----> every half second, until...
//      READY <---------------------- from port P: the host is there too. Now the game connects to the relay's port P, as if it were the host.

#ifndef NET_RELAY_H
#define NET_RELAY_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// THE MESSAGES (PROTOCOL.md, section 2)

size_t const kNetRelayMessageBytes = 64;      // every message, either way, is exactly this long: a relay never answers with more than it was sent
size_t const kNetRelaySecretBytes = 16;       // a token, a ticket, a cookie, or an echo's bytes
size_t const kNetRelayCodeLetters = 8;        // a game's code
size_t const kNetRelayPassBytes = 16;         // a pass
uint8_t const kNetRelayVersion = 1;
uint16_t const kNetRelayDefaultControlPort = 27500;

// The letters a code is made of: 32 of them, with no 0, 1, I or O, so a code can be read out loud without mistakes
static const char kNetRelayCodeAlphabet[] = "23456789ABCDEFGHJKLMNPQRSTUVWXYZ";

// The kinds of message (the "type" byte)
int const kNetRelayHostOpen = 1;       // host to the control port: give me a room (with a cookie, or 0 the first time)
int const kNetRelayHostOpened = 2;     // relay to host: the room's token and code, and a pass
int const kNetRelayHostKeep = 3;       // host to the control port: I'm still here (token)
int const kNetRelayHostKept = 4;       // relay to host: noted (token), how many joiners the room has now, and a fresh pass
int const kNetRelayHostClose = 5;      // host to the control port: the room is done (token)
int const kNetRelayJoin = 6;           // joiner to the control port: let me into this game (code; a cookie, or 0 the first time)
int const kNetRelayJoinPort = 7;       // relay to joiner: your ticket, and your port on the relay
int const kNetRelayNewJoiner = 8;      // relay to host: a joiner is coming, on this port (token)
int const kNetRelayHostHello = 9;      // host to a joiner's port: here I am (token, the port)
int const kNetRelayJoinerHello = 10;   // joiner to its port: here I am (ticket)
int const kNetRelayReady = 11;         // relay, from that port, to the joiner: both ends are here, start the game (ticket, the port)
int const kNetRelayRefused = 12;       // relay to whoever asked: no, and why (the number)
int const kNetRelayCookie = 13;        // relay to whoever asked: the cookie, to ask again with
int const kNetRelayEcho = 14;          // anyone to the control port: answer at once (16 bytes of the asker's, and a count)
int const kNetRelayEchoBack = 15;      // relay to the asker: the same 16 bytes and count, back
int const kNetRelayMostType = 15;

// One message, as its fields
struct NetRelayMessage
{
	int type;
	uint8_t secret[kNetRelaySecretBytes];   // all 0 where the kind of message has none
	char code[kNetRelayCodeLetters];        // all 0 where it has none; NOT a string: no 0 at its end
	uint16_t port;
	uint16_t number;
	uint8_t pass[kNetRelayPassBytes];       // all 0 but in HOST_OPENED and HOST_KEPT
	uint64_t passTime;                      // when the pass was made, in whole seconds since 1970; 0 where there's no pass
};

// Which fields each kind of message uses: PROTOCOL.md's table, row for row. Every other field of a message is 0.
int const kNetRelayUsesSecret = 1;
int const kNetRelayUsesCode = 2;
int const kNetRelayUsesPort = 4;
int const kNetRelayUsesNumber = 8;
int const kNetRelayUsesPass = 16;   // the pass and its time

inline int NetRelay_Uses(int type)
{
	switch (type)
	{
		case kNetRelayHostOpen: return kNetRelayUsesSecret;
		case kNetRelayHostOpened: return kNetRelayUsesSecret | kNetRelayUsesCode | kNetRelayUsesPass;
		case kNetRelayHostKeep: return kNetRelayUsesSecret;
		case kNetRelayHostKept: return kNetRelayUsesSecret | kNetRelayUsesNumber | kNetRelayUsesPass;
		case kNetRelayHostClose: return kNetRelayUsesSecret;
		case kNetRelayJoin: return kNetRelayUsesSecret | kNetRelayUsesCode;
		case kNetRelayJoinPort: return kNetRelayUsesSecret | kNetRelayUsesCode | kNetRelayUsesPort;
		case kNetRelayNewJoiner: return kNetRelayUsesSecret | kNetRelayUsesPort;
		case kNetRelayHostHello: return kNetRelayUsesSecret | kNetRelayUsesPort;
		case kNetRelayJoinerHello: return kNetRelayUsesSecret;
		case kNetRelayReady: return kNetRelayUsesSecret | kNetRelayUsesPort;
		case kNetRelayRefused: return kNetRelayUsesNumber;
		case kNetRelayCookie: return kNetRelayUsesSecret;
		case kNetRelayEcho: return kNetRelayUsesSecret | kNetRelayUsesNumber;
		case kNetRelayEchoBack: return kNetRelayUsesSecret | kNetRelayUsesNumber;
		default: return 0;
	}
}

// Why some bytes aren't a relay message (NetRelay_Read). 0 means they are one.
int const kNetRelayIsAMessage = 0;
int const kNetRelayNotMessageSize = 1;   // not exactly kNetRelayMessageBytes long
int const kNetRelayNotMagic = 2;         // doesn't start with AVPR
int const kNetRelayNotVersion = 3;       // another version of these messages
int const kNetRelayNotType = 4;          // a kind of message there isn't
int const kNetRelayNotZero = 5;          // a reserved or padding byte isn't 0
int const kNetRelayNotCode = 6;          // a code that's neither 8 code letters nor all 0
int const kNetRelayNotUnused = 7;        // a field its kind of message doesn't use isn't 0
int const kNetRelayNotPassTime = 8;      // a pass time past kNetRelayMostPassTime
int const kNetRelayNotKinds = 9;

// Whether a letter is one of the code's
inline bool NetRelay_IsCodeLetter(char c)
{
	return c != 0 && strchr(kNetRelayCodeAlphabet, c) != NULL;
}

// Whether 8 bytes are a code: every one a code letter
inline bool NetRelay_CodeIsGood(const char *code)
{
	if (!code)
	{
		return false;
	}

	for (size_t i = 0; i < kNetRelayCodeLetters; i++)
	{
		if (!NetRelay_IsCodeLetter(code[i]))
		{
			return false;
		}
	}

	return true;
}

// Whether all n bytes are 0
inline bool NetRelay_AllZero(const uint8_t *bytes, size_t n)
{
	for (size_t i = 0; i < n; i++)
	{
		if (bytes[i] != 0)
		{
			return false;
		}
	}

	return true;
}

// Where each field is in a message, byte by byte (PROTOCOL.md, section 2)
size_t const kNetRelayAtSecret = 8;
size_t const kNetRelayAtCode = 24;
size_t const kNetRelayAtPort = 32;
size_t const kNetRelayAtNumber = 34;
size_t const kNetRelayAtPass = 36;
size_t const kNetRelayAtPassTime = 52;
size_t const kNetRelayAtPadding = 60;

// The latest pass time there can be: 2^53 - 1 seconds. The list's pass time is a JSON number, and that's the most a JSON number holds
// exactly (PROTOCOL.md, section 4), so a later one could never be listed. A message with one isn't a message (PROTOCOL.md, section 2,
// "Not a message", since AVPx-Dedicated-Server c7d610f), so the relay and the game both refuse it, and the two read the same bytes the same
// way (the relay's test vectors, tests/vectors/relay-v1.json, have two such packets).
uint64_t const kNetRelayMostPassTime = 9007199254740991ull;

// Reads a relay message out of a packet. Gives back kNetRelayIsAMessage and fills out, or why the packet isn't one, and then out is left as
// it was. The layout: 0-3 AVPR, 4 the version, 5 the kind, 6-7 zero, 8-23 the secret, 24-31 the code, 32-33 the port and 34-35 the number
// (both low byte first), 36-51 the pass, 52-59 the pass time (low byte first), 60-63 zero. A field its kind doesn't use has to be 0, and a
// pass time can't be past kNetRelayMostPassTime. The rules are looked at in this order, the relay's own, so a packet that breaks two of
// them gets the same reason from both.
inline int NetRelay_Read(const uint8_t *data, size_t size, NetRelayMessage *out)
{
	if (!data || size != kNetRelayMessageBytes)
	{
		return kNetRelayNotMessageSize;
	}

	if (data[0] != 'A' || data[1] != 'V' || data[2] != 'P' || data[3] != 'R')
	{
		return kNetRelayNotMagic;
	}

	if (data[4] != kNetRelayVersion)
	{
		return kNetRelayNotVersion;
	}

	if (data[5] < 1 || data[5] > kNetRelayMostType)
	{
		return kNetRelayNotType;
	}

	if (data[6] != 0 || data[7] != 0 || !NetRelay_AllZero(data + kNetRelayAtPadding, kNetRelayMessageBytes - kNetRelayAtPadding))
	{
		return kNetRelayNotZero;
	}

	int const uses = NetRelay_Uses(data[5]);
	const char *const code = reinterpret_cast<const char *>(data + kNetRelayAtCode);
	bool const codeIsZero = NetRelay_AllZero(data + kNetRelayAtCode, kNetRelayCodeLetters);

	if (!(uses & kNetRelayUsesSecret) && !NetRelay_AllZero(data + kNetRelayAtSecret, kNetRelaySecretBytes))
	{
		return kNetRelayNotUnused;
	}

	if (!(uses & kNetRelayUsesCode) && !codeIsZero)
	{
		return kNetRelayNotUnused;
	}

	if (!(uses & kNetRelayUsesPort) && (data[kNetRelayAtPort] != 0 || data[kNetRelayAtPort + 1] != 0))
	{
		return kNetRelayNotUnused;
	}

	if (!(uses & kNetRelayUsesNumber) && (data[kNetRelayAtNumber] != 0 || data[kNetRelayAtNumber + 1] != 0))
	{
		return kNetRelayNotUnused;
	}

	if (!(uses & kNetRelayUsesPass) && !NetRelay_AllZero(data + kNetRelayAtPass, kNetRelayAtPadding - kNetRelayAtPass))
	{
		return kNetRelayNotUnused;
	}

	if (!codeIsZero && !NetRelay_CodeIsGood(code))
	{
		return kNetRelayNotCode;
	}

	uint64_t passTime = 0;

	for (size_t i = 0; i < 8; i++)
	{
		passTime |= static_cast<uint64_t>(data[kNetRelayAtPassTime + i]) << (8 * i);
	}

	if (passTime > kNetRelayMostPassTime)
	{
		return kNetRelayNotPassTime;
	}

	if (out)
	{
		out->type = data[5];
		memcpy(out->secret, data + kNetRelayAtSecret, kNetRelaySecretBytes);
		memcpy(out->code, code, kNetRelayCodeLetters);
		out->port = static_cast<uint16_t>(data[kNetRelayAtPort] | (data[kNetRelayAtPort + 1] << 8));
		out->number = static_cast<uint16_t>(data[kNetRelayAtNumber] | (data[kNetRelayAtNumber + 1] << 8));
		memcpy(out->pass, data + kNetRelayAtPass, kNetRelayPassBytes);
		out->passTime = passTime;
	}

	return kNetRelayIsAMessage;
}

// Writes a relay message into exactly kNetRelayMessageBytes bytes, every byte set: the ones the message doesn't use are 0
inline void NetRelay_Write(const NetRelayMessage &m, uint8_t *out)
{
	memset(out, 0, kNetRelayMessageBytes);
	out[0] = 'A';
	out[1] = 'V';
	out[2] = 'P';
	out[3] = 'R';
	out[4] = kNetRelayVersion;
	out[5] = static_cast<uint8_t>(m.type);
	memcpy(out + kNetRelayAtSecret, m.secret, kNetRelaySecretBytes);
	memcpy(out + kNetRelayAtCode, m.code, kNetRelayCodeLetters);
	out[kNetRelayAtPort] = static_cast<uint8_t>(m.port & 0xFF);
	out[kNetRelayAtPort + 1] = static_cast<uint8_t>(m.port >> 8);
	out[kNetRelayAtNumber] = static_cast<uint8_t>(m.number & 0xFF);
	out[kNetRelayAtNumber + 1] = static_cast<uint8_t>(m.number >> 8);
	memcpy(out + kNetRelayAtPass, m.pass, kNetRelayPassBytes);

	for (size_t i = 0; i < 8; i++)
	{
		out[kNetRelayAtPassTime + i] = static_cast<uint8_t>((m.passTime >> (8 * i)) & 0xFF);
	}
}

// A message with nothing in it but its kind
inline NetRelayMessage NetRelay_Message(int type)
{
	NetRelayMessage m;
	memset(&m, 0, sizeof(m));
	m.type = type;
	return m;
}

// Whether two secrets (or passes) are the same. Every byte is looked at whatever the first difference, so how long it takes says nothing of
// where they differ.
inline bool NetRelay_SameSecret(const uint8_t *a, const uint8_t *b)
{
	uint8_t differ = 0;

	for (size_t i = 0; i < kNetRelaySecretBytes; i++)
	{
		differ = static_cast<uint8_t>(differ | (a[i] ^ b[i]));
	}

	return differ == 0;
}

// WHY A ROOM WAS LOST, OR A JOIN FAILED. 1 to 4 are a relay's own reasons, as REFUSED gives them (PROTOCOL.md, section 2).
int const kNetRelayFine = 0;
int const kNetRelayRefusedNoGame = 1;         // no game has that code
int const kNetRelayRefusedFull = 2;           // the game is full
int const kNetRelayRefusedBusy = 3;           // the relay is too busy
int const kNetRelayRefusedTooMany = 4;        // too many games from this address
int const kNetRelayRefusedOther = 5;          // REFUSED with a reason this version doesn't know
int const kNetRelayNoAnswer = 6;              // the relay didn't answer in time
int const kNetRelayNoKeep = 7;                // the relay stopped answering the host's HOST_KEEP: its room has gone
int const kNetRelayNoReady = 8;               // the host never said hello on the joiner's port in time
int const kNetRelayClosed = 9;                // the host closed its room
int const kNetRelayNoRelay = 10;              // no relay answered the echoes, or there's no relay for that game's region
int const kNetRelayGivenUp = 11;              // the player gave up waiting for the join (the menu's wait hook, networking.h)

inline const char *NetRelay_WhyText(int why)
{
	switch (why)
	{
		case kNetRelayFine: return "fine";
		case kNetRelayRefusedNoGame: return "the relay knows no game with that code";
		case kNetRelayRefusedFull: return "that game is full";
		case kNetRelayRefusedBusy: return "the relay is too busy just now";
		case kNetRelayRefusedTooMany: return "the relay has too many games from this address";
		case kNetRelayRefusedOther: return "the relay said no, for a reason this version doesn't know";
		case kNetRelayNoAnswer: return "the relay didn't answer";
		case kNetRelayNoKeep: return "the relay stopped answering, so the room has gone";
		case kNetRelayNoReady: return "the game's host didn't answer through the relay";
		case kNetRelayClosed: return "the room was closed";
		case kNetRelayNoRelay: return "no relay could be reached";
		case kNetRelayGivenUp: return "the join was given up";
		default: return "no such reason";
	}
}

inline int NetRelay_RefusedWhy(uint16_t number)
{
	return number >= kNetRelayRefusedNoGame && number <= kNetRelayRefusedTooMany ? number : kNetRelayRefusedOther;
}

// WHAT A STATE MACHINE WANTS SENT: a message, to one of a relay's ports. relay says which relay, for the one that picks a relay
// (NetRelayPicker); the host and the joiner each talk to one relay only, and give 0.
struct NetRelaySend
{
	uint8_t relay;
	uint16_t port;
	uint8_t bytes[kNetRelayMessageBytes];
};

size_t const kNetRelayMostSends = 16;   // a state machine's sends wait here between TakeSends; past it, the newest are dropped, and counted

// THE TIMES, in milliseconds (PROTOCOL.md, sections 1, 2 and 5)
uint32_t const kNetRelayAskAgainMs = 1000;        // HOST_OPEN or JOIN, again, while the relay hasn't answered
uint32_t const kNetRelayGiveUpAskingMs = 10000;   // and no answer at all after this: give up
uint32_t const kNetRelayKeepEveryMs = 15000;      // the host's HOST_KEEP
uint32_t const kNetRelayKeptWithinMs = 45000;     // a relay ends a room unkept this long, so with no HOST_KEPT for this long it's gone
uint32_t const kNetRelayHostHelloEveryMs = 1000;  // the host's hello to a joiner's port, until that joiner's game connects
uint32_t const kNetRelayJoinerHelloEveryMs = 500; // the joiner's hello, until READY
uint32_t const kNetRelayWaitForReadyMs = 15000;   // a relay frees a joiner's port this long after JOIN_PORT if both haven't said hello
uint32_t const kNetRelayEchoEveryMs = 100;        // the echoes to each relay, this far apart
uint32_t const kNetRelayEchoAnswerWithinMs = 1000;// an echo answered later than this doesn't count
uint32_t const kNetRelayMostKeepMs = 9999;        // the most a keep's round trip is given as (NetRelayHost::KeepMs), the list's own limit on one

size_t const kNetRelayMostJoiners = 8;            // a room's joiners (PROTOCOL.md, section 5), and so the host's hellos at once
size_t const kNetRelayMostRelays = 16;            // the most relays the list names (PROTOCOL.md, section 5)
size_t const kNetRelayEchoes = 3;                 // echoes to each relay

// Why a message from a relay's control port was heard and then not used (each one is counted, by this reason, never with an address)
int const kNetRelayUsed = 0;
int const kNetRelayNotForNow = 1;      // a kind of message that isn't for this end, or not at this point of its life
int const kNetRelayWrongSecret = 2;    // it names a room, a joiner or an echo, with a secret that isn't ours
int const kNetRelayBadPort = 3;        // a port that can't be a joiner's: 0, or the control port itself
int const kNetRelayNoRoom = 4;         // NEW_JOINER with the host's hellos already full
int const kNetRelayNoPass = 5;         // HOST_OPENED or HOST_KEPT without a pass, which every one has
int const kNetRelayUnusedKinds = 6;

// THE ONE THAT PICKS A RELAY, before the host opens its room (PROTOCOL.md, section 2, Round trips): kNetRelayEchoes echoes to each relay,
// kNetRelayEchoEveryMs apart, each with 16 fresh random bytes; each relay's lowest round trip counts, a relay that doesn't answer within
// kNetRelayEchoAnswerWithinMs is left out, and the one with the lowest time wins. Only an answer from that relay's control port, carrying
// bytes this sent to that relay, counts: anything else is taken and counted.
class NetRelayPicker
{
public:
	NetRelayPicker() { Reset(); }

	void Reset()
	{
		count = 0;
		started = false;
		startedAt = 0;
		echoesSent = 0;
		memset(echoSentAt, 0, sizeof(echoSentAt));
		memset(controlPorts, 0, sizeof(controlPorts));
		memset(randomBytes, 0, sizeof(randomBytes));
		memset(answered, 0, sizeof(answered));
		memset(bestMs, 0, sizeof(bestMs));
		memset(hasTime, 0, sizeof(hasTime));
		sendCount = 0;
		sendsDropped = 0;
		memset(notMessages, 0, sizeof(notMessages));
		memset(unused, 0, sizeof(unused));
	}

	// Starts timing relayCount relays (at most kNetRelayMostRelays), each at its control port. random holds 16 fresh random bytes for each
	// echo, kNetRelayEchoes of them to each relay, relay by relay. The first echo to each goes at once. False, and nothing sent, for none.
	bool Start(size_t relayCount, const uint16_t *ports, const uint8_t *random, uint32_t now)
	{
		Reset();

		if (relayCount == 0 || relayCount > kNetRelayMostRelays || !ports || !random)
		{
			return false;
		}

		count = relayCount;
		memcpy(controlPorts, ports, count * sizeof(uint16_t));
		memcpy(randomBytes, random, count * kNetRelayEchoes * kNetRelaySecretBytes);
		started = true;
		startedAt = now;
		SendEchoes(now);
		return true;
	}

	// A packet from relay number relay's address, from its port fromPort. True if it was that relay's message, from its control port.
	bool Hear(size_t relay, uint16_t fromPort, const uint8_t *data, size_t size, uint32_t now)
	{
		if (!started || relay >= count || fromPort != controlPorts[relay])
		{
			return false;
		}

		NetRelayMessage m;
		int const why = NetRelay_Read(data, size, &m);

		if (why != kNetRelayIsAMessage)
		{
			notMessages[why]++;
			return true;
		}

		if (m.type != kNetRelayEchoBack)
		{
			return Unused(kNetRelayNotForNow);
		}

		// Its count says which echo it answers: 1 for the first. It has to be one sent already, not answered yet, with that echo's bytes.
		if (m.number < 1 || m.number > echoesSent || answered[relay][m.number - 1]
			|| !NetRelay_SameSecret(m.secret, randomBytes[relay][m.number - 1]))
		{
			return Unused(kNetRelayWrongSecret);
		}

		answered[relay][m.number - 1] = true;
		uint32_t const took = now - echoSentAt[m.number - 1];

		if (took <= kNetRelayEchoAnswerWithinMs && (!hasTime[relay] || took < bestMs[relay]))
		{
			bestMs[relay] = took;
			hasTime[relay] = true;
		}

		return true;
	}

	// The time has moved on: the next echoes, when it's their time
	void Tick(uint32_t now)
	{
		if (!started)
		{
			return;
		}

		while (echoesSent < kNetRelayEchoes && now - startedAt >= static_cast<uint32_t>(echoesSent) * kNetRelayEchoEveryMs)
		{
			SendEchoes(now);
		}
	}

	// Every echo sent, and the last one's time to be answered gone by
	bool Done(uint32_t now) const
	{
		return started && echoesSent == kNetRelayEchoes && now - echoSentAt[kNetRelayEchoes - 1] >= kNetRelayEchoAnswerWithinMs;
	}

	// The relay with the lowest round trip, or -1 if none answered in time; and that time, in ms
	int Best() const
	{
		int best = -1;

		for (size_t i = 0; i < count; i++)
		{
			if (hasTime[i] && (best < 0 || bestMs[i] < bestMs[best]))
			{
				best = static_cast<int>(i);
			}
		}

		return best;
	}

	uint32_t BestMs() const
	{
		int const best = Best();
		return best < 0 ? 0 : bestMs[best];
	}

	// One relay's own lowest round trip, in ms: false if it hasn't one (no echo answered in time, or no such relay). For the joiner's ping
	// column (networking.cpp, Net_JoinerMsToRelay), which times every relay.
	bool RelayMs(size_t relay, uint32_t *ms) const
	{
		if (relay >= count || !hasTime[relay] || !ms)
		{
			return false;
		}

		*ms = bestMs[relay];
		return true;
	}

	size_t TakeSends(NetRelaySend *out, size_t most)
	{
		size_t const n = sendCount < most ? sendCount : most;

		for (size_t i = 0; i < n; i++)
		{
			out[i] = sends[i];
		}

		for (size_t i = n; i < sendCount; i++)
		{
			sends[i - n] = sends[i];
		}

		sendCount -= n;
		return n;
	}

	uint32_t NotMessages(int why) const { return why >= 0 && why < kNetRelayNotKinds ? notMessages[why] : 0; }
	uint32_t UnusedMessages(int why) const { return why >= 0 && why < kNetRelayUnusedKinds ? unused[why] : 0; }
	uint32_t SendsDropped() const { return sendsDropped; }

private:
	// One echo to every relay: the next of kNetRelayEchoes. Its round trip is timed from now.
	void SendEchoes(uint32_t now)
	{
		echoSentAt[echoesSent] = now;

		for (size_t i = 0; i < count; i++)
		{
			NetRelayMessage m = NetRelay_Message(kNetRelayEcho);
			memcpy(m.secret, randomBytes[i][echoesSent], kNetRelaySecretBytes);
			m.number = static_cast<uint16_t>(echoesSent + 1);

			if (sendCount == kNetRelayMostSends)
			{
				sendsDropped++;
				continue;
			}

			sends[sendCount].relay = static_cast<uint8_t>(i);
			sends[sendCount].port = controlPorts[i];
			NetRelay_Write(m, sends[sendCount].bytes);
			sendCount++;
		}

		echoesSent++;
	}

	bool Unused(int why)
	{
		unused[why]++;
		return true;
	}

	size_t count;
	bool started;
	uint32_t startedAt;
	size_t echoesSent;
	uint32_t echoSentAt[kNetRelayEchoes];
	uint16_t controlPorts[kNetRelayMostRelays];
	uint8_t randomBytes[kNetRelayMostRelays][kNetRelayEchoes][kNetRelaySecretBytes];
	bool answered[kNetRelayMostRelays][kNetRelayEchoes];
	uint32_t bestMs[kNetRelayMostRelays];
	bool hasTime[kNetRelayMostRelays];
	NetRelaySend sends[kNetRelayMostSends];
	size_t sendCount;
	uint32_t sendsDropped;
	uint32_t notMessages[kNetRelayNotKinds];
	uint32_t unused[kNetRelayUnusedKinds];
};

// THE HOST. Start, then once a frame Hear each packet from the relay's address and Tick; take what to send with TakeSends.
class NetRelayHost
{
public:
	NetRelayHost() { Reset(); }

	void Reset()
	{
		state = kIdle;
		controlPort = 0;
		memset(token, 0, sizeof(token));
		memset(cookie, 0, sizeof(cookie));
		memset(code, 0, sizeof(code));
		memset(pass, 0, sizeof(pass));
		passTime = 0;
		passesHeard = 0;
		joiners = 0;
		lostWhy = kNetRelayFine;
		startedAt = askedAt = keptAt = keepAt = 0;
		keepWaiting = false;
		keepMs = -1;
		memset(hellos, 0, sizeof(hellos));
		sendCount = 0;
		memset(sends, 0, sizeof(sends));   // the messages still here hold the token and the cookie (added 2026-11-09 by Melanikus for the AvP port)
		sendsDropped = 0;
		memset(notMessages, 0, sizeof(notMessages));
		memset(unused, 0, sizeof(unused));
	}

	// Asks the relay for a room: HOST_OPEN to its control port
	void Start(uint16_t relayControlPort, uint32_t now)
	{
		Reset();
		controlPort = relayControlPort;
		state = kAsking;
		startedAt = now;
		Ask(now);
	}

	// A packet from the relay's address, from its port fromPort. True if it was one of the relay's messages for the host, and so isn't the
	// game's: the game's own packets, from joiners' ports, are never taken. Anything else from the control port is taken and counted, and
	// never answered.
	bool Hear(uint16_t fromPort, const uint8_t *data, size_t size, uint32_t now)
	{
		if (state == kIdle || fromPort != controlPort)
		{
			return false;
		}

		NetRelayMessage m;
		int const why = NetRelay_Read(data, size, &m);

		if (why != kNetRelayIsAMessage)
		{
			notMessages[why]++;
			return true;
		}

		switch (m.type)
		{
			case kNetRelayCookie:
				if (state != kAsking)
				{
					return Unused(kNetRelayNotForNow);
				}

				memcpy(cookie, m.secret, sizeof(cookie));
				Ask(now);
				return true;

			case kNetRelayHostOpened:
				if (state != kAsking)
				{
					return Unused(kNetRelayNotForNow);
				}

				// A room needs a real token and a real code, and comes with a pass
				if (NetRelay_AllZero(m.secret, kNetRelaySecretBytes) || !NetRelay_CodeIsGood(m.code))
				{
					return Unused(kNetRelayWrongSecret);
				}

				if (NetRelay_AllZero(m.pass, kNetRelayPassBytes) || m.passTime == 0)
				{
					return Unused(kNetRelayNoPass);
				}

				memcpy(token, m.secret, sizeof(token));
				memcpy(code, m.code, sizeof(code));
				KeepPass(m);
				state = kOpen;
				keptAt = keepAt = now;
				return true;

			case kNetRelayHostKept:
				if (state != kOpen)
				{
					return Unused(kNetRelayNotForNow);
				}

				if (!NetRelay_SameSecret(m.secret, token))
				{
					return Unused(kNetRelayWrongSecret);
				}

				if (NetRelay_AllZero(m.pass, kNetRelayPassBytes) || m.passTime == 0)
				{
					return Unused(kNetRelayNoPass);
				}

				keptAt = now;

				// The keep's round trip, from the HOST_KEEP this answers (KeepMs)
				if (keepWaiting)
				{
					uint32_t const took = now - keepAt;
					keepMs = took < 1 ? 1 : (took > kNetRelayMostKeepMs ? static_cast<int>(kNetRelayMostKeepMs) : static_cast<int>(took));
					keepWaiting = false;
				}

				joiners = m.number <= kNetRelayMostJoiners ? m.number : static_cast<int>(kNetRelayMostJoiners);
				KeepPass(m);
				return true;

			case kNetRelayNewJoiner:
				if (state != kOpen)
				{
					return Unused(kNetRelayNotForNow);
				}

				if (!NetRelay_SameSecret(m.secret, token))
				{
					return Unused(kNetRelayWrongSecret);
				}

				if (m.port == 0 || m.port == controlPort)
				{
					return Unused(kNetRelayBadPort);
				}

				return HelloTo(m.port, now) ? true : Unused(kNetRelayNoRoom);

			case kNetRelayRefused:
				if (state != kAsking && state != kOpen)
				{
					return Unused(kNetRelayNotForNow);
				}

				Lose(NetRelay_RefusedWhy(m.number));
				return true;

			default:
				return Unused(kNetRelayNotForNow);
		}
	}

	// The time has moved on: ask again, keep the room, say hello again, or give up
	void Tick(uint32_t now)
	{
		if (state == kAsking)
		{
			if (now - startedAt >= kNetRelayGiveUpAskingMs)
			{
				Lose(kNetRelayNoAnswer);
			}
			else if (now - askedAt >= kNetRelayAskAgainMs)
			{
				// The cookie is good for 10 to 20 s, so after a while a fresh one is asked for: the first ask again is without it
				memset(cookie, 0, sizeof(cookie));
				Ask(now);
			}

			return;
		}

		if (state != kOpen)
		{
			return;
		}

		if (now - keptAt >= kNetRelayKeptWithinMs)
		{
			Lose(kNetRelayNoKeep);
			return;
		}

		if (now - keepAt >= kNetRelayKeepEveryMs)
		{
			keepAt = now;
			keepWaiting = true;
			NetRelayMessage m = NetRelay_Message(kNetRelayHostKeep);
			memcpy(m.secret, token, sizeof(token));
			Queue(controlPort, m);
		}

		for (size_t i = 0; i < kNetRelayMostJoiners; i++)
		{
			Hello &h = hellos[i];

			if (h.port == 0)
			{
				continue;
			}

			if (now - h.since >= kNetRelayWaitForReadyMs)
			{
				h.port = 0;
			}
			else if (now - h.lastSent >= kNetRelayHostHelloEveryMs)
			{
				SendHello(h, now);
			}
		}
	}

	// That joiner's game has connected through its port: its hellos stop
	void JoinerConnected(uint16_t relayPort)
	{
		for (size_t i = 0; i < kNetRelayMostJoiners; i++)
		{
			if (hellos[i].port == relayPort)
			{
				hellos[i].port = 0;
			}
		}
	}

	// Added 2026-11-09 by Melanikus for the AvP port (protocol version 2, agreed with Primary): whether the relay has said a joiner is
	// coming on relayPort (NEW_JOINER), and that joiner's game hasn't connected yet. It's the one moment a host starts handshake B for a
	// port (networking.cpp): a datagram from any other port of the relay starts nothing. It only reads.
	bool NamedJoinerPort(uint16_t relayPort) const
	{
		if (relayPort == 0)
		{
			return false;
		}

		for (size_t i = 0; i < kNetRelayMostJoiners; i++)
		{
			if (hellos[i].port == relayPort)
			{
				return true;
			}
		}

		return false;
	}

	// The game is over: HOST_CLOSE, so the relay ends the room at once
	void Close()
	{
		if (state == kOpen)
		{
			NetRelayMessage m = NetRelay_Message(kNetRelayHostClose);
			memcpy(m.secret, token, sizeof(token));
			Queue(controlPort, m);
		}

		if (state != kIdle)
		{
			state = kLost;
			lostWhy = kNetRelayClosed;
		}
	}

	// What it wants sent, oldest first, up to most of them, written into out. Gives back how many; those are forgotten here.
	size_t TakeSends(NetRelaySend *out, size_t most)
	{
		size_t const n = sendCount < most ? sendCount : most;

		for (size_t i = 0; i < n; i++)
		{
			out[i] = sends[i];
		}

		for (size_t i = n; i < sendCount; i++)
		{
			sends[i - n] = sends[i];
		}

		sendCount -= n;
		return n;
	}

	bool Asking() const { return state == kAsking; }
	bool Open() const { return state == kOpen; }

	// What the list is given, once the room is open: the code's 8 letters (no 0 after), the newest pass and its time. False, with nothing
	// written, before. The room's token is never handed out: it's the host's proof to the relay alone.
	bool RoomForTheList(char *codeOut, uint8_t *passOut, uint64_t *passTimeOut) const
	{
		if (state != kOpen)
		{
			return false;
		}

		memcpy(codeOut, code, kNetRelayCodeLetters);
		memcpy(passOut, pass, kNetRelayPassBytes);
		*passTimeOut = passTime;
		return true;
	}

	uint32_t PassesHeard() const { return passesHeard; }   // goes up by one with each pass, so the list can be given each new one once
	int Joiners() const { return joiners; }                // as the relay last counted them
	int LostWhy() const { return lostWhy; }                // kNetRelayFine until the room is lost

	// Added 2026-11-09 by Melanikus for the AvP port: the round trip of the last keep, in ms, from a HOST_KEEP going to the HOST_KEPT that
	// answers it, for the line on the host's screen (Net_RelayPlayStateNow). -1 before the first. At most kNetRelayMostKeepMs. A keep lost on
	// the way is answered by none, so the next keep's answer is the one timed.
	int KeepMs() const { return keepMs; }

	size_t HellosWaiting() const
	{
		size_t n = 0;

		for (size_t i = 0; i < kNetRelayMostJoiners; i++)
		{
			n += hellos[i].port != 0 ? 1 : 0;
		}

		return n;
	}

	// The counts, for the log: packets from the control port that weren't messages, by NetRelay_Read's reason; messages not used, by
	// reason; and sends dropped for want of room
	uint32_t NotMessages(int why) const { return why >= 0 && why < kNetRelayNotKinds ? notMessages[why] : 0; }
	uint32_t UnusedMessages(int why) const { return why >= 0 && why < kNetRelayUnusedKinds ? unused[why] : 0; }
	uint32_t SendsDropped() const { return sendsDropped; }

private:
	enum State { kIdle, kAsking, kOpen, kLost };

	struct Hello
	{
		uint16_t port;      // 0 for none
		uint32_t since;     // when NEW_JOINER came
		uint32_t lastSent;
	};

	void KeepPass(const NetRelayMessage &m)
	{
		memcpy(pass, m.pass, sizeof(pass));
		passTime = m.passTime;
		passesHeard++;
	}

	void Ask(uint32_t now)
	{
		askedAt = now;
		NetRelayMessage m = NetRelay_Message(kNetRelayHostOpen);
		memcpy(m.secret, cookie, sizeof(cookie));
		Queue(controlPort, m);
	}

	// Starts saying hello on a joiner's port, or says it again if it's there already. False if there's no room for another.
	bool HelloTo(uint16_t port, uint32_t now)
	{
		Hello *free = NULL;

		for (size_t i = 0; i < kNetRelayMostJoiners; i++)
		{
			if (hellos[i].port == port)
			{
				SendHello(hellos[i], now);
				return true;
			}

			if (!free && hellos[i].port == 0)
			{
				free = &hellos[i];
			}
		}

		if (!free)
		{
			return false;
		}

		free->port = port;
		free->since = now;
		SendHello(*free, now);
		return true;
	}

	void SendHello(Hello &h, uint32_t now)
	{
		h.lastSent = now;
		NetRelayMessage m = NetRelay_Message(kNetRelayHostHello);
		memcpy(m.secret, token, sizeof(token));
		m.port = h.port;
		Queue(h.port, m);
	}

	void Lose(int why)
	{
		state = kLost;
		lostWhy = why;
		memset(hellos, 0, sizeof(hellos));
	}

	bool Unused(int why)
	{
		unused[why]++;
		return true;
	}

	void Queue(uint16_t port, const NetRelayMessage &m)
	{
		if (sendCount == kNetRelayMostSends)
		{
			sendsDropped++;
			return;
		}

		sends[sendCount].relay = 0;
		sends[sendCount].port = port;
		NetRelay_Write(m, sends[sendCount].bytes);
		sendCount++;
	}

	State state;
	uint16_t controlPort;
	uint8_t token[kNetRelaySecretBytes];
	uint8_t cookie[kNetRelaySecretBytes];
	char code[kNetRelayCodeLetters];
	uint8_t pass[kNetRelayPassBytes];
	uint64_t passTime;
	uint32_t passesHeard;
	int joiners;
	int lostWhy;
	uint32_t startedAt, askedAt, keptAt, keepAt;
	bool keepWaiting;   // a HOST_KEEP has gone, and no HOST_KEPT has come back for it yet
	int keepMs;         // the last keep's round trip (KeepMs)
	Hello hellos[kNetRelayMostJoiners];
	NetRelaySend sends[kNetRelayMostSends];
	size_t sendCount;
	uint32_t sendsDropped;
	uint32_t notMessages[kNetRelayNotKinds];
	uint32_t unused[kNetRelayUnusedKinds];
};

// THE JOINER. Start with the game's code, then Hear each packet from the relay's address and Tick, until Ready or Failed; take what to send
// with TakeSends. Once Ready, the game connects to the relay's Port().
class NetRelayJoiner
{
public:
	NetRelayJoiner() { Reset(); }

	void Reset()
	{
		state = kIdle;
		controlPort = 0;
		memset(code, 0, sizeof(code));
		memset(cookie, 0, sizeof(cookie));
		memset(ticket, 0, sizeof(ticket));
		port = 0;
		failedWhy = kNetRelayFine;
		startedAt = askedAt = portAt = helloAt = 0;
		sendCount = 0;
		memset(sends, 0, sizeof(sends));   // the messages still here hold the ticket and the cookie (added 2026-11-09 by Melanikus for the AvP port)
		sendsDropped = 0;
		memset(notMessages, 0, sizeof(notMessages));
		memset(unused, 0, sizeof(unused));
	}

	// Asks to join the game with this code (8 code letters, as the list gives it): JOIN to the relay's control port. False, and nothing
	// sent, for a code that isn't one.
	bool Start(uint16_t relayControlPort, const char *gameCode, uint32_t now)
	{
		Reset();

		if (!NetRelay_CodeIsGood(gameCode))
		{
			return false;
		}

		memcpy(code, gameCode, sizeof(code));
		controlPort = relayControlPort;
		state = kAsking;
		startedAt = now;
		Ask(now);
		return true;
	}

	// A packet from the relay's address, from its port fromPort. True if it was one of the relay's messages for this joiner. From its own
	// port, only a READY with its ticket is taken; anything else from there is the game's, once the game has connected.
	bool Hear(uint16_t fromPort, const uint8_t *data, size_t size, uint32_t now)
	{
		if (state == kIdle)
		{
			return false;
		}

		bool const fromControl = fromPort == controlPort;
		bool const fromItsPort = port != 0 && fromPort == port;

		if (!fromControl && !fromItsPort)
		{
			return false;
		}

		NetRelayMessage m;
		int const why = NetRelay_Read(data, size, &m);

		if (why != kNetRelayIsAMessage)
		{
			// On its own port, the game's packets come once it's connected: they're not the relay's, so they're not taken
			if (fromItsPort)
			{
				return false;
			}

			notMessages[why]++;
			return true;
		}

		if (fromItsPort)
		{
			if (m.type != kNetRelayReady)
			{
				return false;
			}

			if (!NetRelay_SameSecret(m.secret, ticket) || m.port != port)
			{
				return Unused(kNetRelayWrongSecret);
			}

			if (state == kHello)
			{
				state = kReady;
			}

			return true;
		}

		switch (m.type)
		{
			case kNetRelayCookie:
				if (state != kAsking)
				{
					return Unused(kNetRelayNotForNow);
				}

				memcpy(cookie, m.secret, sizeof(cookie));
				Ask(now);
				return true;

			case kNetRelayJoinPort:
				if (state != kAsking)
				{
					return Unused(kNetRelayNotForNow);
				}

				// The answer is for this game, with a real ticket and a port that can be a joiner's
				if (memcmp(m.code, code, sizeof(code)) != 0 || NetRelay_AllZero(m.secret, kNetRelaySecretBytes))
				{
					return Unused(kNetRelayWrongSecret);
				}

				if (m.port == 0 || m.port == controlPort)
				{
					return Unused(kNetRelayBadPort);
				}

				memcpy(ticket, m.secret, sizeof(ticket));
				port = m.port;
				portAt = now;
				state = kHello;
				SayHello(now);
				return true;

			case kNetRelayRefused:
				if (state != kAsking)
				{
					return Unused(kNetRelayNotForNow);
				}

				Fail(NetRelay_RefusedWhy(m.number));
				return true;

			default:
				return Unused(kNetRelayNotForNow);
		}
	}

	void Tick(uint32_t now)
	{
		if (state == kAsking)
		{
			if (now - startedAt >= kNetRelayGiveUpAskingMs)
			{
				Fail(kNetRelayNoAnswer);
			}
			else if (now - askedAt >= kNetRelayAskAgainMs)
			{
				memset(cookie, 0, sizeof(cookie));
				Ask(now);
			}
		}
		else if (state == kHello)
		{
			if (now - portAt >= kNetRelayWaitForReadyMs)
			{
				Fail(kNetRelayNoReady);
			}
			else if (now - helloAt >= kNetRelayJoinerHelloEveryMs)
			{
				SayHello(now);
			}
		}
	}

	size_t TakeSends(NetRelaySend *out, size_t most)
	{
		size_t const n = sendCount < most ? sendCount : most;

		for (size_t i = 0; i < n; i++)
		{
			out[i] = sends[i];
		}

		for (size_t i = n; i < sendCount; i++)
		{
			sends[i - n] = sends[i];
		}

		sendCount -= n;
		return n;
	}

	bool Ready() const { return state == kReady; }
	bool Failed() const { return state == kFailed; }
	bool Introducing() const { return state == kHello; }   // the relay has given it a port, and the two ends are saying hello
	int FailedWhy() const { return failedWhy; }
	uint16_t Port() const { return state == kReady ? port : 0; }   // the relay's port to connect the game to, once Ready

	uint32_t NotMessages(int why) const { return why >= 0 && why < kNetRelayNotKinds ? notMessages[why] : 0; }
	uint32_t UnusedMessages(int why) const { return why >= 0 && why < kNetRelayUnusedKinds ? unused[why] : 0; }
	uint32_t SendsDropped() const { return sendsDropped; }

private:
	enum State { kIdle, kAsking, kHello, kReady, kFailed };

	void Ask(uint32_t now)
	{
		askedAt = now;
		NetRelayMessage m = NetRelay_Message(kNetRelayJoin);
		memcpy(m.secret, cookie, sizeof(cookie));
		memcpy(m.code, code, sizeof(code));
		Queue(controlPort, m);
	}

	void SayHello(uint32_t now)
	{
		helloAt = now;
		NetRelayMessage m = NetRelay_Message(kNetRelayJoinerHello);
		memcpy(m.secret, ticket, sizeof(ticket));
		Queue(port, m);
	}

	void Fail(int why)
	{
		state = kFailed;
		failedWhy = why;
	}

	bool Unused(int why)
	{
		unused[why]++;
		return true;
	}

	void Queue(uint16_t to, const NetRelayMessage &m)
	{
		if (sendCount == kNetRelayMostSends)
		{
			sendsDropped++;
			return;
		}

		sends[sendCount].relay = 0;
		sends[sendCount].port = to;
		NetRelay_Write(m, sends[sendCount].bytes);
		sendCount++;
	}

	State state;
	uint16_t controlPort;
	char code[kNetRelayCodeLetters];
	uint8_t cookie[kNetRelaySecretBytes];
	uint8_t ticket[kNetRelaySecretBytes];
	uint16_t port;
	int failedWhy;
	uint32_t startedAt, askedAt, portAt, helloAt;
	NetRelaySend sends[kNetRelayMostSends];
	size_t sendCount;
	uint32_t sendsDropped;
	uint32_t notMessages[kNetRelayNotKinds];
	uint32_t unused[kNetRelayUnusedKinds];
};

#endif
