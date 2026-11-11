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

// (THE WAYS ONLINE, the rows that asked for the one line's yes as they were pressed, went 2026-11-11: with one model for every line the
// page opens only from that line's row on the Online page. REMOVED-CODE.md has the row.)

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

// ---- THE PAGE'S LAYOUT. Added 2026-11-10 by Melanikus for the AvP port, from Third's Hudson pair on rc1 (PO saw the picture): the
// server's name was drawn in the big font at 60, on top of the page's own title at 70, and the owner's words ended with "play." alone on
// their last line. Now, on the menus' 640x480 grid, which is the same at every screen size:
// - the title, where RenderMenu draws every page's (70, the big font, 21 high, with a glow under it);
// - the server's name, on its own line under it in the small font (15 high), cut to the menus' edges by the font's own widths;
// - where the server is, and its key's two ends, a line each;
// - what its owner does and sees, wrapped to the narrowest width that keeps its lines (online_screens_page.h, OnlinePage_BalancedWidth),
//   so a last word never stands alone;
// and all of it above the page's rows, which RenderMenu puts in the middle (MENU_CENTREY, less half their height). Owner's words that
// wouldn't fit above them are cut to the lines that do: nothing is ever drawn into the rows.
int const kServerYesTitleTop = 70;      // menus.cpp, RenderMenu: every page's title, RenderMenuText(textPtr, MENU_CENTREX, 70, ...)
int const kServerYesBigLine = 21;       // the big font's height, MENU_FONT_HEIGHT (server_yes_page.cpp checks it when the game is built)
int const kServerYesSmallLine = 15;     // the small font's, HUD_FONT_HEIGHT
int const kServerYesUnderBig = 8;       // under a big line: its glow reaches below it
int const kServerYesUnderSmall = 4;     // between two small lines
int const kServerYesBeforeOwner = 12;   // before the owner's words, a paragraph of their own
int const kServerYesAboveRows = 15;     // a small line, at least, above the first row
int const kServerYesLeft = 30;          // the widest the words may be: the menus' own edges, MENU_LEFTXEDGE and MENU_RIGHTXEDGE
int const kServerYesRight = 610;

struct ServerYesLayout
{
	int nameTop;       // the server's name
	int whereTop;      // where it is
	int keyTop;        // its key's two ends
	int ownerTop;      // the owner's words' first line
	int ownerLines;    // how many of their lines are drawn: all of them, unless there's no room above the rows
	int bottom;        // under the last line drawn
};

// Where each part goes. ownerLines: how many lines the owner's words take at their width (SmallFontString_WrappedLines); rowsTop: where
// the page's first row is.
inline ServerYesLayout ServerYes_Layout(int ownerLines, int rowsTop)
{
	ServerYesLayout at;
	at.nameTop = kServerYesTitleTop + kServerYesBigLine + kServerYesUnderBig;
	at.whereTop = at.nameTop + kServerYesSmallLine + kServerYesUnderSmall;
	at.keyTop = at.whereTop + kServerYesSmallLine + kServerYesUnderSmall;
	at.ownerTop = at.keyTop + kServerYesSmallLine + kServerYesBeforeOwner;

	int room = (rowsTop - kServerYesAboveRows - at.ownerTop) / kServerYesSmallLine;
	room = room < 0 ? 0 : room;
	ownerLines = ownerLines < 0 ? 0 : ownerLines;
	at.ownerLines = ownerLines > room ? room : ownerLines;
	at.bottom = at.ownerTop + at.ownerLines * kServerYesSmallLine;
	return at;
}

// Where a box of this width starts, in the middle of the menus' edges: words wrapped narrower than the edges are centred. A width wider
// than the edges, or none, gets the edges themselves.
inline int ServerYes_BoxLeft(int width)
{
	int const widest = kServerYesRight - kServerYesLeft;
	return width <= 0 || width >= widest ? kServerYesLeft : kServerYesLeft + (widest - width) / 2;
}

#endif
