// The game's side of the relay (AVPx Dedicated Server): the messages it sends and hears, and what a host and a joiner do with them.
// Written 2026-11-08 by Melanikus for the AvP port.
//
// WHY THERE'S A RELAY. Many players can't be reached from the internet at all: their provider puts many homes behind one address
// (carrier-grade NAT), and no setting on their own router changes that. So a game can go through the relay, a server everyone can reach.
// The host opens a "room" there; each joiner gets a port of its own on the relay, and the relay passes the game's own packets between the
// two, unread and unchanged. How the relay works, every message, field and limit: PROTOCOL.md in the AVPx-Dedicated-Server repository
// (PROTOCOL.md at d75a5bd is what this follows). That file is the single source both sides read.
//
// WHAT THIS FILE IS. Only the relay's own little messages, and two "state machines": the host's (NetRelayHost) and the joiner's
// (NetRelayJoiner). A state machine here is a small object that is told the time and what arrived, and says what to send. It never sends or
// reads anything itself. networking.cpp does that, from the game's own network socket, so the relay sees each player's game through the same
// door in their router that the game itself uses. Nothing here knows about the game, so tests/net_relay_test.cpp can try every case.
//
// EVERYTHING THAT ARRIVES IS HOSTILE (AvP Tao #3). A relay message is exactly kNetRelayMessageBytes long, and every byte has to be what
// PROTOCOL.md says, or it isn't one: NetRelay_Read says which rule it broke. A message is only listened to if it comes from where the relay
// says such a message comes from (its control port), and one that names the room or the joiner has to carry the secret the relay gave it.
// Every list here has a fixed size, and every wait a time limit.
//
// A GAME'S LIFE, as the host sees it:
//
//      HOST_OPEN ------------------> relay control port
//      COOKIE <--------------------- (proof we really are at this address)
//      HOST_OPEN, with the cookie ->
//      HOST_OPENED <---------------- the room's token (our secret) and its code (8 letters, for the list and for typing)
//      HOST_KEEP, every 15 s ------> (keeps the room, and our router's door to the relay, open)
//      NEW_JOINER <----------------- a joiner is coming, on relay port P
//      HOST_HELLO, to port P ------> every second, until that joiner's game connects through port P
//      ... the game's own packets, through port P ...
//      HOST_CLOSE -----------------> when the game ends
//
// and as a joiner sees it:
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

size_t const kNetRelayMessageBytes = 64;      // every message, either way, is exactly this long: the relay never answers with more than it was sent
size_t const kNetRelaySecretBytes = 16;       // a token, a ticket or a cookie
size_t const kNetRelayCodeLetters = 8;        // a game's code
uint8_t const kNetRelayVersion = 1;
uint16_t const kNetRelayDefaultControlPort = 27500;

// The letters a code is made of: 32 of them, with no 0, 1, I or O, so a code can be read out loud without mistakes
static const char kNetRelayCodeAlphabet[] = "23456789ABCDEFGHJKLMNPQRSTUVWXYZ";

// The kinds of message (the "type" byte)
int const kNetRelayHostOpen = 1;       // host to the control port: give me a room (with a cookie, or 0 the first time)
int const kNetRelayHostOpened = 2;     // relay to host: the room's token and code
int const kNetRelayHostKeep = 3;       // host to the control port: I'm still here (token)
int const kNetRelayHostKept = 4;       // relay to host: noted (token), and how many joiners the room has now
int const kNetRelayHostClose = 5;      // host to the control port: the room is done (token)
int const kNetRelayJoin = 6;           // joiner to the control port: let me into this game (code; a cookie, or 0 the first time)
int const kNetRelayJoinPort = 7;       // relay to joiner: your ticket, and your port on the relay
int const kNetRelayNewJoiner = 8;      // relay to host: a joiner is coming, on this port (token)
int const kNetRelayHostHello = 9;      // host to a joiner's port: here I am (token, the port)
int const kNetRelayJoinerHello = 10;   // joiner to its port: here I am (ticket)
int const kNetRelayReady = 11;         // relay, from that port, to the joiner: both ends are here, start the game (ticket, the port)
int const kNetRelayRefused = 12;       // relay to whoever asked: no, and why (the number)
int const kNetRelayCookie = 13;        // relay to whoever asked: the cookie, to ask again with
int const kNetRelayMostType = 13;

// One message, as its fields
struct NetRelayMessage
{
	int type;
	uint8_t secret[kNetRelaySecretBytes];   // all 0 where the kind of message has none
	char code[kNetRelayCodeLetters];        // all 0 where it has none; NOT a string: no 0 at its end
	uint16_t port;
	uint16_t number;
};

// Why some bytes aren't a relay message (NetRelay_Read). 0 means they are one.
int const kNetRelayIsAMessage = 0;
int const kNetRelayNotMessageSize = 1;   // not exactly kNetRelayMessageBytes long
int const kNetRelayNotMagic = 2;         // doesn't start with AVPR
int const kNetRelayNotVersion = 3;       // another version of these messages
int const kNetRelayNotType = 4;          // a kind of message there isn't
int const kNetRelayNotZero = 5;          // a byte that must be 0 isn't
int const kNetRelayNotCode = 6;          // a code that's neither 8 code letters nor all 0
int const kNetRelayNotKinds = 7;

inline const char *NetRelay_NotAMessageWhy(int why)
{
	switch (why)
	{
		case kNetRelayIsAMessage: return "it is a relay message";
		case kNetRelayNotMessageSize: return "not exactly 64 bytes";
		case kNetRelayNotMagic: return "it doesn't start with AVPR";
		case kNetRelayNotVersion: return "another version of the relay's messages";
		case kNetRelayNotType: return "a kind of message the relay doesn't have";
		case kNetRelayNotZero: return "a byte that must be 0 isn't";
		case kNetRelayNotCode: return "its code isn't 8 code letters, nor all 0";
		default: return "no such reason";
	}
}

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

// Reads a relay message out of a packet. Gives back kNetRelayIsAMessage and fills out, or why the packet isn't one, and then out is left
// as it was. The layout, byte by byte, is PROTOCOL.md's: 0-3 AVPR, 4 the version, 5 the type, 6-7 zero, 8-23 the secret, 24-31 the
// code, 32-33 the port and 34-35 the number (both low byte first), 36-63 zero.
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

	if (data[6] != 0 || data[7] != 0 || !NetRelay_AllZero(data + 36, kNetRelayMessageBytes - 36))
	{
		return kNetRelayNotZero;
	}

	const char *const code = reinterpret_cast<const char *>(data + 24);

	if (!NetRelay_AllZero(data + 24, kNetRelayCodeLetters) && !NetRelay_CodeIsGood(code))
	{
		return kNetRelayNotCode;
	}

	if (out)
	{
		out->type = data[5];
		memcpy(out->secret, data + 8, kNetRelaySecretBytes);
		memcpy(out->code, code, kNetRelayCodeLetters);
		out->port = static_cast<uint16_t>(data[32] | (data[33] << 8));
		out->number = static_cast<uint16_t>(data[34] | (data[35] << 8));
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
	memcpy(out + 8, m.secret, kNetRelaySecretBytes);
	memcpy(out + 24, m.code, kNetRelayCodeLetters);
	out[32] = static_cast<uint8_t>(m.port & 0xFF);
	out[33] = static_cast<uint8_t>(m.port >> 8);
	out[34] = static_cast<uint8_t>(m.number & 0xFF);
	out[35] = static_cast<uint8_t>(m.number >> 8);
}

// A message with nothing in it but its kind
inline NetRelayMessage NetRelay_Message(int type)
{
	NetRelayMessage m;
	memset(&m, 0, sizeof(m));
	m.type = type;
	return m;
}

// Whether two secrets are the same. Every byte is looked at whatever the first difference, so how long it takes says nothing of where they
// differ.
inline bool NetRelay_SameSecret(const uint8_t *a, const uint8_t *b)
{
	uint8_t differ = 0;

	for (size_t i = 0; i < kNetRelaySecretBytes; i++)
	{
		differ = static_cast<uint8_t>(differ | (a[i] ^ b[i]));
	}

	return differ == 0;
}

// A token as the list's HTTPS calls take it: 32 hex digits, small letters, then a 0, so out holds 33
inline void NetRelay_SecretHex(const uint8_t *secret, char *out)
{
	static const char digits[] = "0123456789abcdef";

	for (size_t i = 0; i < kNetRelaySecretBytes; i++)
	{
		out[i * 2] = digits[secret[i] >> 4];
		out[i * 2 + 1] = digits[secret[i] & 0x0F];
	}

	out[kNetRelaySecretBytes * 2] = 0;
}

// A code a player typed or the list gave, as the relay's 8 letters: small letters are made capital, and spaces and dashes on either side
// or between are left out. False, with out untouched, unless exactly 8 code letters are left.
inline bool NetRelay_CodeFromText(const char *text, char *out)
{
	char code[kNetRelayCodeLetters];
	size_t letters = 0;

	for (size_t i = 0; text && text[i] != 0 && i < 64; i++)
	{
		char c = text[i];

		if (c == ' ' || c == '-')
		{
			continue;
		}

		if (c >= 'a' && c <= 'z')
		{
			c = static_cast<char>(c - 'a' + 'A');
		}

		if (letters == kNetRelayCodeLetters || !NetRelay_IsCodeLetter(c))
		{
			return false;
		}

		code[letters++] = c;
	}

	if (letters != kNetRelayCodeLetters)
	{
		return false;
	}

	memcpy(out, code, kNetRelayCodeLetters);
	return true;
}

// WHY A ROOM WAS LOST, OR A JOIN FAILED. 1 to 4 are the relay's own reasons, as REFUSED gives them (PROTOCOL.md, section 2).
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
		default: return "no such reason";
	}
}

inline int NetRelay_RefusedWhy(uint16_t number)
{
	return number >= kNetRelayRefusedNoGame && number <= kNetRelayRefusedTooMany ? number : kNetRelayRefusedOther;
}

// WHAT A STATE MACHINE WANTS SENT: a message, to the relay's address, at one of its ports
struct NetRelaySend
{
	uint16_t port;
	uint8_t bytes[kNetRelayMessageBytes];
};

size_t const kNetRelayMostSends = 16;   // a state machine's sends wait here between TakeSends; past it, the newest are dropped, and counted

// THE TIMES, in milliseconds (PROTOCOL.md, sections 1 and 5)
uint32_t const kNetRelayAskAgainMs = 1000;        // HOST_OPEN or JOIN, again, while the relay hasn't answered
uint32_t const kNetRelayGiveUpAskingMs = 10000;   // and no answer at all after this: give up
uint32_t const kNetRelayKeepEveryMs = 15000;      // the host's HOST_KEEP
uint32_t const kNetRelayKeptWithinMs = 45000;     // the relay ends a room unkept this long, so with no HOST_KEPT for this long it's gone
uint32_t const kNetRelayHostHelloEveryMs = 1000;  // the host's hello to a joiner's port, until that joiner's game connects
uint32_t const kNetRelayJoinerHelloEveryMs = 500; // the joiner's hello, until READY
uint32_t const kNetRelayWaitForReadyMs = 15000;   // the relay frees a joiner's port this long after JOIN_PORT if both haven't said hello

size_t const kNetRelayMostJoiners = 8;            // a room's joiners (PROTOCOL.md, section 5), and so the host's hellos at once

// Why a message from the relay's control port was heard and then not used (each one is counted, by this reason, never with an address)
int const kNetRelayUsed = 0;
int const kNetRelayNotForNow = 1;      // a kind of message that isn't for this end, or not at this point of its life
int const kNetRelayWrongSecret = 2;    // it names a room or a joiner, with a secret that isn't ours
int const kNetRelayBadPort = 3;        // a port that can't be a joiner's: 0, or the control port itself
int const kNetRelayNoRoom = 4;         // NEW_JOINER with the host's hellos already full
int const kNetRelayUnusedKinds = 5;

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
		joiners = 0;
		lostWhy = kNetRelayFine;
		startedAt = askedAt = keptAt = keepAt = 0;
		memset(hellos, 0, sizeof(hellos));
		sendCount = 0;
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

				// A room needs a real token and a real code
				if (NetRelay_AllZero(m.secret, kNetRelaySecretBytes) || !NetRelay_CodeIsGood(m.code))
				{
					return Unused(kNetRelayWrongSecret);
				}

				memcpy(token, m.secret, sizeof(token));
				memcpy(code, m.code, sizeof(code));
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

				keptAt = now;
				joiners = m.number <= kNetRelayMostJoiners ? m.number : static_cast<int>(kNetRelayMostJoiners);
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

	// The game is over: HOST_CLOSE, so the relay ends the room and its list entry at once
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

	// The room's token as 32 hex digits and its code as 8 letters with a 0 after, once it's open. False, with nothing written, before.
	bool Room(char *tokenHex, char *codeText) const
	{
		if (state != kOpen)
		{
			return false;
		}

		NetRelay_SecretHex(token, tokenHex);
		memcpy(codeText, code, kNetRelayCodeLetters);
		codeText[kNetRelayCodeLetters] = 0;
		return true;
	}

	// The same, as the relay gave them, for the list's calls: the token's 16 bytes, and the code's 8 letters with no 0 after. False, with
	// nothing written, before it's open.
	bool RoomSecret(uint8_t *tokenOut, char *codeOut) const
	{
		if (state != kOpen)
		{
			return false;
		}

		memcpy(tokenOut, token, kNetRelaySecretBytes);
		memcpy(codeOut, code, kNetRelayCodeLetters);
		return true;
	}

	int Joiners() const { return joiners; }                // as the relay last counted them
	int LostWhy() const { return lostWhy; }                // kNetRelayFine until the room is lost
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

		sends[sendCount].port = port;
		NetRelay_Write(m, sends[sendCount].bytes);
		sendCount++;
	}

	State state;
	uint16_t controlPort;
	uint8_t token[kNetRelaySecretBytes];
	uint8_t cookie[kNetRelaySecretBytes];
	char code[kNetRelayCodeLetters];
	int joiners;
	int lostWhy;
	uint32_t startedAt, askedAt, keptAt, keepAt;
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
		sendsDropped = 0;
		memset(notMessages, 0, sizeof(notMessages));
		memset(unused, 0, sizeof(unused));
	}

	// Asks to join the game with this code (8 code letters, NetRelay_CodeFromText): JOIN to the relay's control port. False, and nothing
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
