// The Connection check page, and its line on the co-op host page (connection_page.h says what the player sees). Written 2026-11-09 by
// Melanikus for the AvP port.

#include "3dc.h"
#include "inline.h"
#include "menus.h"
#include "menu_gfx.hpp"
#include "language.h"
#include "hud_layout.h"
#include "connection_check.h"
#include "connection_page.h"

#include <stdio.h>
#include <string.h>

extern char AAFontWidths[256];

namespace
{
	// Where the lines go: across the middle of the screen, from just under the page's title, a little gap between them
	const int kLinesLeft = 60;
	const int kLinesRight = 580;
	const int kLinesTop = 48;
	const int kLinesGap = 4;
	const int kScreenBottom = 470;
	const int kMostLinesHigh = 6;

	// The host page asks for a new check at most once a minute, so its line follows what the player changes without asking all the time
	const DWORD kHostCheckEveryMs = 60000;
	bool hostAsked = false;
	DWORD hostAskedAt = 0;

	// How many lines of the screen a line of words takes when it's wrapped at the page's width. Each letter's width is the small font's
	// own, at most 17 (as menu_gfx.cpp holds it), so the sum can't overflow; and one more line is counted for the words that move down
	// whole, so the room is never too small.
	int LinesHigh(const char *words)
	{
		int width = 0;

		for (const char *letter = words; *letter && width < 100000; letter++)
		{
			int const one = AAFontWidths[(unsigned char)*letter];
			width += one > 17 ? 17 : (one < 0 ? 0 : one);
		}

		int const across = kLinesRight - kLinesLeft;
		int lines = width / across + 1;

		if (lines > 1)
		{
			lines++;
		}

		return lines > kMostLinesHigh ? kMostLinesHigh : lines;
	}

	// One line of words, wrapped inside its own room from top; gives where the next line may start
	int DrawLine(char *words, int top, int high)
	{
		RECT area = {kLinesLeft, top, kLinesRight, top + high};
		int x = 0;
		int y = top;
		RenderSmallFontString_Wrapped(words, &area, ONE_FIXED, &x, &y);
		return top + high + kLinesGap;
	}
}

void ConnectionPage_Opened(void)
{
	ConnectionCheck_Start();
}

void ConnectionPage_Pressed(int row)
{
	switch (row)
	{
		case kConnRowAgain:
			ConnectionCheck_Start();
			break;
		case kConnRowNetwork:
			ConnectionCheck_OpenNetworkSettings();
			break;
		case kConnRowFirewall:
			ConnectionCheck_OpenFirewallSettings();
			break;
		default:
			break;
	}
}

void ConnectionPage_DrawLines(int rowsTop, int rowsBottom)
{
	ConnectionFacts facts;

	// While Windows is being asked, only that is said: the lines come when its answer does
	if (ConnectionCheck_Reading() || !ConnectionCheck_Result(&facts))
	{
		RenderSmallMenuText(GetTextString(TEXTSTRING_CONNCHECK_ASKING), MENU_CENTREX, kLinesTop + HUD_FONT_HEIGHT, ONE_FIXED,
			AVPMENUFORMAT_CENTREJUSTIFIED);
		return;
	}

	char lines[kConnMostLines][kConnLineBytes];
	int const count = ConnectionCheck_Lines(facts, lines, kConnMostLines);
	int top = kLinesTop;
	int i = 0;

	// Above the rows, as many as fit; then under them
	for (; i < count; i++)
	{
		int const high = LinesHigh(lines[i]) * HUD_FONT_HEIGHT;

		if (top + high > rowsTop - kLinesGap)
		{
			break;
		}

		top = DrawLine(lines[i], top, high);
	}

	top = rowsBottom + 2 * kLinesGap;

	for (; i < count; i++)
	{
		int const high = LinesHigh(lines[i]) * HUD_FONT_HEIGHT;

		if (top + high > kScreenBottom)
		{
			break;
		}

		top = DrawLine(lines[i], top, high);
	}
}

bool ConnectionPage_HostLine(char *line, size_t size)
{
	if (!line || size == 0)
	{
		return false;
	}

	line[0] = '\0';
	DWORD const now = GetTickCount();

	// Taking one time from the other works across GetTickCount going round to 0 after 49 days
	if (!hostAsked || (DWORD)(now - hostAskedAt) >= kHostCheckEveryMs)
	{
		hostAsked = true;
		hostAskedAt = now;
		ConnectionCheck_Start();
	}

	ConnectionFacts facts;

	if (!ConnectionCheck_Result(&facts))
	{
		return false;
	}

	const char *const words = ConnectionCheck_HostWords(ConnectionCheck_Verdict(facts));

	if (!words[0])
	{
		return false;
	}

	_snprintf(line, size - 1, "%s", words);
	line[size - 1] = '\0';
	return true;
}
