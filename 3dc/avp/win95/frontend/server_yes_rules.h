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
