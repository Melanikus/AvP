// The host's count of the search questions it hears, in the log. Written 2026-11-08 by Melanikus for the AvP port.
//
// WHY: the user tried to join a game from the internet and the list said "no sessions found". A joiner finds games by asking on UDP
// port 23514: everyone on its own network at once (a broadcast, which never leaves that network), and the address the player typed, if
// there is one. A host answers each question with its game's description. Neither side wrote anything about it, so after a failed join
// nobody could tell whether the question ever reached the host, or reached it and went unanswered.
//
// WHAT IT WRITES, on the host only, and never an address (the user's ruling: "You can save times it took, no IPs, no usernames"):
//
//     net: this game answers searches on UDP port 23514 from now
//     net: the first search question came 12.3 s after this game began answering, and was answered
//     net: searches in the last 60 s: 3 packets on the search port, 3 of them questions; 3 answered, 0 not (too many at once), 0 not sent
//     net: searches while this game was open, 600.0 s: 9 packets on the search port, 9 of them questions; 9 answered, 0 not (too many
//          at once), 0 not sent
//
// The minute line comes every 60 seconds while the game is open, even when everything in it is 0: a 0 is what says nothing arrived. The
// last line comes when the game closes. Anyone can send packets to the port as fast as they like (AVP Tao 3), so nothing here writes a
// line for each one: the first question gets one line, and the rest are only counted. A count can't run over: it stops at its largest.
//
// The words: "packets" is everything that came to the port; "questions" are those the right size, with the game's marker, asking for a
// game's description; "answered" were sent an answer; "too many at once" weren't, because one address or everyone had asked too often
// that second (networking.cpp's limits); "not sent" were to be answered but the network wouldn't take the answer then.
//
// tests/net_search_log_test.cpp tries every line and count. networking.cpp calls these from Net_HostGame, Net_CheckForSessionRequest and
// Net_Disconnect.
#ifndef NET_SEARCH_LOG_H
#define NET_SEARCH_LOG_H

#include <stdio.h>
#include <stddef.h>

// The port a search asks on (networking.cpp's broadcastPort), and how often the counts are written, in milliseconds
static const unsigned int kNetSearchPort = 23514;
static const unsigned int kNetSearchEveryMilliseconds = 60000;

// What happened to one question
enum NetSearchOutcome
{
	kNetSearchAnswered,     // an answer was sent
	kNetSearchTooMany,      // not answered: too many questions that second, from its address or from everyone
	kNetSearchNotSent       // to be answered, but the network wouldn't take the answer then
};

struct NetSearchCounts
{
	unsigned long long packets;     // everything that came to the search port
	unsigned long long questions;   // of them, questions for a game's description
	unsigned long long answered;
	unsigned long long tooMany;
	unsigned long long notSent;
};

struct NetSearchLog
{
	bool open;                      // this machine is hosting a game that answers searches
	unsigned int openedAt;          // milliseconds, by the network's clock (enet_time_get)
	unsigned int lastLineAt;        // when the last minute line was written, or the game opened
	bool firstQuestionSaid;
	NetSearchCounts sinceLastLine;
	NetSearchCounts whileOpen;
};

// One more, stopping at the largest a count can hold
inline void NetSearchLog_Add(unsigned long long *count)
{
	if (*count != ~0ull)
	{
		(*count)++;
	}
}

inline void NetSearchLog_Zero(NetSearchCounts *counts)
{
	counts->packets = counts->questions = counts->answered = counts->tooMany = counts->notSent = 0;
}

// Milliseconds as seconds with one decimal, for a line: "12.3"
inline double NetSearchLog_Seconds(unsigned int milliseconds)
{
	return milliseconds / 1000.0;
}

// The game has started answering searches, at now. Gives the line that says so.
inline void NetSearchLog_Opened(NetSearchLog *log, unsigned int now, char *line, size_t size)
{
	log->open = true;
	log->openedAt = now;
	log->lastLineAt = now;
	log->firstQuestionSaid = false;
	NetSearchLog_Zero(&log->sinceLastLine);
	NetSearchLog_Zero(&log->whileOpen);
	snprintf(line, size, "net: this game answers searches on UDP port %u from now", kNetSearchPort);
}

// Something came to the search port
inline void NetSearchLog_Packet(NetSearchLog *log)
{
	if (!log->open)
	{
		return;
	}

	NetSearchLog_Add(&log->sinceLastLine.packets);
	NetSearchLog_Add(&log->whileOpen.packets);
}

// A question came, at now, and this happened to it. True, with the line, for the first question of the game; false for the rest, which
// are only counted.
inline bool NetSearchLog_Question(NetSearchLog *log, NetSearchOutcome outcome, unsigned int now, char *line, size_t size)
{
	if (!log->open)
	{
		return false;
	}

	NetSearchCounts *const both[2] = { &log->sinceLastLine, &log->whileOpen };

	for (NetSearchCounts *counts : both)
	{
		NetSearchLog_Add(&counts->questions);
		NetSearchLog_Add(outcome == kNetSearchAnswered ? &counts->answered : outcome == kNetSearchTooMany ? &counts->tooMany : &counts->notSent);
	}

	if (log->firstQuestionSaid)
	{
		return false;
	}

	log->firstQuestionSaid = true;
	char const *const what = outcome == kNetSearchAnswered ? "and was answered" : outcome == kNetSearchTooMany ? "and wasn't answered (too many at once)" :
		"and wasn't answered (the network wouldn't take the answer)";
	snprintf(line, size, "net: the first search question came %.1f s after this game began answering, %s", NetSearchLog_Seconds(now - log->openedAt), what);
	return true;
}

// The counts' words, after a line's start
inline void NetSearchLog_CountsLine(const char *start, const NetSearchCounts &c, char *line, size_t size)
{
	snprintf(line, size, "%s: %llu packets on the search port, %llu of them questions; %llu answered, %llu not (too many at once), %llu not sent", start,
		c.packets, c.questions, c.answered, c.tooMany, c.notSent);
}

// Called often: true, with the minute line, once kNetSearchEveryMilliseconds have gone since the last one. The counts start again then.
inline bool NetSearchLog_Minute(NetSearchLog *log, unsigned int now, char *line, size_t size)
{
	if (!log->open || now - log->lastLineAt < kNetSearchEveryMilliseconds)
	{
		return false;
	}

	char start[64];
	snprintf(start, sizeof(start), "net: searches in the last %u s", (now - log->lastLineAt) / 1000);
	NetSearchLog_CountsLine(start, log->sinceLastLine, line, size);
	log->lastLineAt = now;
	NetSearchLog_Zero(&log->sinceLastLine);
	return true;
}

// The game has stopped answering, at now: true, with the line for the whole game, if it was answering. It answers nothing after this.
inline bool NetSearchLog_Closed(NetSearchLog *log, unsigned int now, char *line, size_t size)
{
	if (!log->open)
	{
		return false;
	}

	char start[96];
	snprintf(start, sizeof(start), "net: searches while this game was open, %.1f s", NetSearchLog_Seconds(now - log->openedAt));
	NetSearchLog_CountsLine(start, log->whileOpen, line, size);
	log->open = false;
	return true;
}

#endif
