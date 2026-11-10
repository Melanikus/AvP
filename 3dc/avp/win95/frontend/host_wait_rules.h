// A host's wait for the server, and the line a failed host shows: their rules, with nothing of the game's in them, so a test can try every
// case (server_yes_page_test). Written 2026-11-10 by Melanikus for the AvP port (PO's rulings Q3 and Q4).
//
// WHY A HOST WAITS. Online, a game is hosted through the server servers.cfg names, and on a fresh start the server may not have answered
// yet: the list asks it as the Multiplayer page opens (relay_list.cpp). A host pressed before the answer (Start game, or Co-op campaign,
// whose page hosts as it opens) waits for it on the page it was pressed on, which says "Asking the server...", and hosts by itself once
// the server is ready. A server that can't be used stops the wait, and the page says why.
//
// IT NEVER WAITS FOREVER. Net_ServerState has no time limit of its own. Each of the list's calls has WinHTTP's 5 seconds a step
// (relay_list.cpp, CallChecked), but a server can keep one call going a byte at a time. So after kHostWaitMostMs the page gives up, says
// the server didn't answer, and hosts nothing. Esc goes back as on any page, and the wait ends with the page.
//
// THE WORDS. A failed host's line is the game's words for what failed, then why: "Couldn't host online: the relay server couldn't be
// reached." Both are the game's own fixed words. The why is one of networking.cpp's or relay_list.cpp's constants, never anything a
// server sent.

#ifndef HOST_WAIT_RULES_H
#define HOST_WAIT_RULES_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

// The longest a host waits for the server's first answer: 30 seconds, which language.cpp's words for giving up say too
uint32_t const kHostWaitMostMs = 30000;

// What one frame of the wait does
enum HostWaitStep
{
	kHostWaitGoOn,      // still asking, within the limit: wait
	kHostWaitHostNow,   // the server is ready, or the wait has nothing to wait for: host now, and the host says what's wrong if anything is
	kHostWaitCantUse,   // the server can't be used: stop, and say why
	kHostWaitGiveUp     // still asking at the limit: stop, say the server didn't answer, and host nothing
};

// One frame of the wait, from where the server stands (networking.h, Net_ServerState): asking, ready, or can't be used. now and startedAt
// are GetTickCount's milliseconds, so their difference is right across its wrap, every 49.7 days.
inline HostWaitStep HostWait_Step(bool asking, bool ready, bool cantUse, uint32_t startedAt, uint32_t now)
{
	if (ready)
	{
		return kHostWaitHostNow;
	}

	if (cantUse)
	{
		return kHostWaitCantUse;
	}

	if (!asking)
	{
		return kHostWaitHostNow;
	}

	return static_cast<uint32_t>(now - startedAt) >= kHostWaitMostMs ? kHostWaitGiveUp : kHostWaitGoOn;
}

// A failed host's line: what failed, then why, as "Couldn't host online: the relay server couldn't be reached." With no why, the words
// for what failed alone. Cut short at out's end, which always gets its 0.
inline void HostWait_FailedLine(const char *whatFailed, const char *why, char *out, size_t room)
{
	if (!out || room == 0)
	{
		return;
	}

	if (why && why[0])
	{
		snprintf(out, room, "%s %s.", whatFailed ? whatFailed : "", why);
	}
	else
	{
		snprintf(out, room, "%s", whatFailed ? whatFailed : "");
	}

	out[room - 1] = 0;
}

#endif
