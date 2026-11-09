// The Connection check page, and its one line on the co-op host page (beta.2's "nobody blocked silently"). Written 2026-11-09 by
// Melanikus for the AvP port.
//
// WHAT THE PLAYER SEES. Multiplayer, then Connection check: what on this PC could stop AVPx playing online, in plain words (the network's
// kind, Windows Firewall going out, coming in for a LAN game at home, other firewalls), then three rows: Check again, Windows network
// settings and Windows Firewall, the last two opening Windows' own pages. The facts are read by connection_check_win.cpp, read only, on a
// thread of its own, when the page opens and on Check again; the words are connection_check.cpp's.
//
// ON THE HOST PAGE: one line, only when the last check found something that stops AVPx, or may, so a host who can't be reached is told
// why, and where to look. It's held back while a join request's rows are up, as the relay's news is: the check code is drawn there then.

#ifndef CONNECTION_PAGE_H
#define CONNECTION_PAGE_H

#include <stddef.h>

// The page's rows, each one's MaxValue in menu_data.cpp
enum ConnectionPageRow
{
	kConnRowAgain,      // Check again
	kConnRowNetwork,    // Windows' network settings
	kConnRowFirewall    // Windows Security's firewall page
};

// The page has opened: a check starts, unless one is going
void ConnectionPage_Opened(void);

// A row was pressed
void ConnectionPage_Pressed(int row);

// The check's lines, around the rows: above them from the top, and any that don't fit there under them. rowsTop and rowsBottom are
// where the rows are drawn.
void ConnectionPage_DrawLines(int rowsTop, int rowsBottom);

// The host page's line: true, with its words, when the last check found something that stops AVPx going online, or may. The first time
// it's asked, it starts a check (read only, on its own thread), so the line can come a moment after the page opens.
bool ConnectionPage_HostLine(char *line, size_t size);

#endif
