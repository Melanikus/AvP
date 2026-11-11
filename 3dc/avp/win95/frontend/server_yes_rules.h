// The yes page's rules, with nothing of the game's in them, so a test can try every one (server_yes_page_test). Written 2026-11-10 by
// Melanikus for the AvP port (#175's last part). server_yes_page.h says what the page is for.

#ifndef SERVER_YES_RULES_H
#define SERVER_YES_RULES_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>

// The page's two rows, told apart by their MaxValue in menu_data.cpp. No comes first, so an Enter pressed by chance sends nothing to the
// server. The numbers are the access page's own kind of row (AVPMENU_ELEMENT_ONLINE_ACCESS), kept far from access_page.h's.
enum
{
	kServerYesRowNo = 100,
	kServerYesRowYes = 101
};

// What a pressed row means: the yes, kept, and on to where the player was going; or back where they came from, with nothing sent; or
// nothing, for a row that isn't one of the two
enum ServerYesChoice
{
	kServerYesNothing,
	kServerYesAgreeAndGoOn,
	kServerYesGoBack
};

inline ServerYesChoice ServerYes_Choice(int row)
{
	if (row == kServerYesRowYes)
	{
		return kServerYesAgreeAndGoOn;
	}

	return row == kServerYesRowNo ? kServerYesGoBack : kServerYesNothing;
}

// THE WAYS ONLINE, and where a No takes each (PO's rulings, 2026-11-10). Added 2026-11-10 by Melanikus for the AvP port. The page is
// asked for as a way's row is pressed, never as a page opens, so a page can still open after a No without asking again:
// - the Online access page: it would send the player's request to the server, so it always asks; after a No, back to the Multiplayer page;
// - a host (Start game, Co-op campaign): it asks only when it hosts online, which is whenever servers.cfg names a server (with none,
//   nothing is hosted, so there's nothing to ask; modified 2026-11-10, the owner's ruling for beta.2). After a No, back to the
//   Multiplayer page;
// - Find games: it would read the server's list, so it always asks. After a No, back to the Multiplayer page too (modified 2026-11-10,
//   the owner's ruling for beta.2: "Everything over nyc3": there are no games on this network to show without it).
enum ServerYesWay
{
	kServerYesNotAWay,       // any other row: it goes where it always went
	kServerYesWayAccess,
	kServerYesWayHost,
	kServerYesWayFindGames
};

// Whether a way asks for the yes before it goes on. hostsOnline is whether a host would host online (it asks nothing of the others).
inline bool ServerYes_WayAsks(ServerYesWay way, bool hostsOnline)
{
	return way == kServerYesWayAccess || way == kServerYesWayFindGames || (way == kServerYesWayHost && hostsOnline);
}

// The line saying where the server is: its address as servers.cfg has it, then where that is, as "104.131.35.41, on the internet". Both
// come from the game (the address was read by servers_cfg.cpp's rules, written out again from its numbers; the place is the game's own
// words). Cut short at out's end, which always gets its 0.
inline void ServerYes_WhereLine(const char *address, const char *where, char *out, size_t room)
{
	if (!out || room == 0)
	{
		return;
	}

	snprintf(out, room, "%s, %s", address ? address : "", where ? where : "");
	out[room - 1] = 0;
}

// The line with the key's two ends, as "key bb0e2958 ... f28d663e": three dots, which the game's font has, between the fingerprint's
// first 8 digits and its last 8
inline void ServerYes_KeyLine(const char *keyWord, const char *keyStart, const char *keyEnd, char *out, size_t room)
{
	if (!out || room == 0)
	{
		return;
	}

	snprintf(out, room, "%s %s ... %s", keyWord ? keyWord : "", keyStart ? keyStart : "", keyEnd ? keyEnd : "");
	out[room - 1] = 0;
}

#endif
