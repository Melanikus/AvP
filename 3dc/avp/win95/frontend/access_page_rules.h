// The Online access page's rules: when it may ask the list, and which words it shows for each answer (server #24). Written 2026-11-09 by
// Melanikus for the AvP port.
//
// access_page.cpp is the page itself, which needs the game. These are only its decisions, game-free, so access_page_test can try each one.
//
// WHEN THE PAGE ASKS THE LIST. Never before the player has pressed Request access: nothing is sent until then (the owner's ruling). Once
// the player has sent a request this run, opening the page again may ask by itself, since it sends nothing new: the same name and key
// (PO's ruling, 2026-11-09). The list takes one request a minute from an address, and the owner tests on two PCs behind one address, so
// the page never asks again within a minute of its last call, by itself or when a row is pressed, and never while a call is on its way.

#ifndef ACCESS_PAGE_RULES_H
#define ACCESS_PAGE_RULES_H

#include <stdint.h>

#include "langenum.h"
#include "relay_list_text.h"

// The least time between two calls, in ms: the list's own limit for one address
uint32_t const kAccessPageLeastMsBetweenCalls = 60000;

// Whether a minute has gone since the last call. The clock is GetTickCount's, which goes round to 0 after 49 days: taking one time from the
// other works across that, as long as the two are less than 49 days apart.
inline bool AccessPage_MinuteHasGone(uint32_t now, uint32_t lastCallAt)
{
	return (uint32_t)(now - lastCallAt) >= kAccessPageLeastMsBetweenCalls;
}

// Whether the page may ask as it opens, by itself: only after a request has been sent this run, with none on its way, and a minute after
// the last call
inline bool AccessPage_MayAskByItself(bool sentThisRun, bool onItsWay, uint32_t now, uint32_t lastCallAt)
{
	return sentThisRun && !onItsWay && AccessPage_MinuteHasGone(now, lastCallAt);
}

// What pressing Request access (or Check again) does
enum AccessPageAsk
{
	kAccessPageAskSends,        // the request goes
	kAccessPageAskOnItsWay,     // one is on its way already: nothing more is sent
	kAccessPageAskWaitAMinute   // the last call was less than a minute ago: nothing is sent, and the page says to wait
};

// The first call of the run may go at once. After that, a minute after the last.
inline AccessPageAsk AccessPage_PressedAsk(bool calledThisRun, bool onItsWay, uint32_t now, uint32_t lastCallAt)
{
	if (onItsWay)
	{
		return kAccessPageAskOnItsWay;
	}

	if (calledThisRun && !AccessPage_MinuteHasGone(now, lastCallAt))
	{
		return kAccessPageAskWaitAMinute;
	}

	return kAccessPageAskSends;
}

// The page's words for what the list said
inline TEXTSTRING_ID AccessPage_AnswerText(RelayAccessAnswer answer)
{
	switch (answer)
	{
		case kRelayAccessWaiting: return TEXTSTRING_ACCESS_WAITING;
		case kRelayAccessApproved: return TEXTSTRING_ACCESS_APPROVED;
		case kRelayAccessRefused: return TEXTSTRING_ACCESS_REFUSED;
		case kRelayAccessRevoked: return TEXTSTRING_ACCESS_REVOKED;
		case kRelayAccessBadName:
		case kRelayAccessBadKey:
		case kRelayAccessBadShape: return TEXTSTRING_ACCESS_NOT_TAKEN;
		case kRelayAccessBusy: return TEXTSTRING_ACCESS_BUSY;
		case kRelayAccessClosed: return TEXTSTRING_ACCESS_CLOSED;
	}

	return TEXTSTRING_ACCESS_CANT_REACH;
}

// The page's words for a name that can't be sent. TEXTSTRING_BLANK for a good one.
inline TEXTSTRING_ID AccessPage_NameText(RelayAccessNameRefusal why)
{
	switch (why)
	{
		case kRelayAccessNameGood: return TEXTSTRING_BLANK;
		case kRelayAccessNameEmpty: return TEXTSTRING_ACCESS_NAME_EMPTY;
		case kRelayAccessNameTooLong: return TEXTSTRING_ACCESS_NAME_TOO_LONG;
		case kRelayAccessNameBadLetter: return TEXTSTRING_ACCESS_NAME_BAD_LETTER;
		case kRelayAccessNameSpaceAtAnEnd: return TEXTSTRING_ACCESS_NAME_SPACE_AT_END;
	}

	return TEXTSTRING_ACCESS_NAME_BAD_LETTER;
}

// Whether an answer means the request is done with for this identity: refused and revoked stay so, and only a new identity can ask again
inline bool AccessPage_NeedsANewIdentity(RelayAccessAnswer answer)
{
	return answer == kRelayAccessRefused || answer == kRelayAccessRevoked;
}

// WHERE THE PAGE'S PARTS GO. Added 2026-11-10 by Melanikus for the AvP port, after the owner's pictures: first the page's title, "Your
// access code" and the code were drawn on top of each other, so the code couldn't be read, with an empty gap above the rows; then, once
// that was fixed, "we have space for some more padding".
//
// The menus are drawn on a grid 640 wide and 480 high, stretched to fill the screen, so these numbers are the same at every screen size,
// 16:9 or 4:3. Every page's title is drawn at y 70, in the big font, 21 high (menus.cpp), and a page's own rows start no higher than
// MENU_TOPYEDGE, 130 (menus.h). So this page lives between 130 and MENU_BOTTOMYEDGE, 390, where the black band with the help line
// starts, and goes down in this order:
//   1. "Your access code", in the small font, and the code itself, in the big font: only once a request has been sent;
//   2. the words: what Request access does, before a request is sent; what to do with the code, after;
//   3. the rows, a little further apart than the menus' own;
//   4. the lines under the rows: what the server said, or why nothing was sent, wrapped to the words' width (as one line it ran almost
//      from one edge of the screen to the other).
// Each gap has a least size: a small line under the code and above the rows, 16 above the lines under the rows, as it always was. Room
// is kept for at least two lines under the rows, so the rows don't move when the server's answer changes. Words too long for the room
// left are cut to the lines that fit, rather than run into the rows. Whatever room is left then is shared out evenly between the gaps
// (under the title, under the code, above the rows, above the lines under them), up to kAccessPageMostSpare each.
//
// The menus' numbers are copied here because this file can't include menus.h (access_page_test uses it without the game):
// access_page.cpp checks each copy against the menus' own when the game is built.
int const kAccessPageTitleTop = 70;        // every page's title (menus.cpp, RenderMenu: RenderMenuText(textPtr, MENU_CENTREX, 70, ...))
int const kAccessPageBigLine = 21;         // the big font's height: MENU_FONT_HEIGHT
int const kAccessPageSmallLine = 15;       // the small font's: HUD_FONT_HEIGHT
int const kAccessPageTop = 130;            // MENU_TOPYEDGE
int const kAccessPageBottom = 390;         // MENU_BOTTOMYEDGE
int const kAccessPageCodeGap = 4;          // between "Your access code" and the code
int const kAccessPageLeastWordsGap = 15;   // under the code, a small line at least: the code's glow reaches below its line
int const kAccessPageLeastRowsGap = 15;    // above the first row, a small line at least
int const kAccessPageLeastUnderGap = 16;   // between the last row and the lines under the rows, as it always was
int const kAccessPageUnderKept = 2;        // lines kept under the rows however few there are, so the rows stay put
int const kAccessPageMostSpare = 20;       // the most of the room left that one gap is given
int const kAccessPageRowsApart = 5;        // added between two rows: the menus' small rows are 19 apart, these 24
int const kAccessPageWordsLeft = 60;       // the words' box, left and right, as it always was
int const kAccessPageWordsRight = 580;

// How bright a row that isn't chosen is drawn on these pages, out of 100. The menus' own is 25 (BRIGHTNESS_OF_DARKENED_ELEMENT is a
// quarter of ONE_FIXED): too faint against the starfield, where the owner looked for "Copy access code" and missed it. The chosen row is
// 100, so these stay clearly dimmer than it.
int const kAccessPageRowNotChosenOutOf100 = 60;

struct AccessPageLayout
{
	int labelTop;      // "Your access code", or -1 when the code isn't shown
	int codeTop;       // the code, or -1
	int wordsTop;      // the words' first line
	int wordsLines;    // how many of their lines are drawn: all of them, unless there's no room
	int rowsTop;       // the first row
	int underTop;      // the lines under the rows
	int underLines;    // how many of them are drawn: all of them, unless even the rows don't leave room
	int spare;         // the room each gap was given beyond its least
};

// How tall the rows are, with kAccessPageRowsApart between each two. menuHeight is the menus' MenuHeight, rows how many rows there are.
inline int AccessPage_RowsHeight(int menuHeight, int rows)
{
	return (menuHeight > 0 ? menuHeight : 0) + (rows > 1 ? (rows - 1) * kAccessPageRowsApart : 0);
}

// The page's parts, top to bottom. codeShown: whether the code is drawn. wordsLines: how many lines the words take
// (SmallFontString_WrappedLines). rowsHeight: the rows' height (AccessPage_RowsHeight). underLines: how many lines the words under the
// rows take, wrapped to the same width (0 when there are none).
inline AccessPageLayout AccessPage_Layout(bool codeShown, int wordsLines, int rowsHeight, int underLines)
{
	AccessPageLayout at;

	if (rowsHeight < 0)
	{
		rowsHeight = 0;
	}

	if (underLines < 0)
	{
		underLines = 0;
	}

	// Everything but the words, with every gap at its least
	int const keptUnder = underLines > kAccessPageUnderKept ? underLines : kAccessPageUnderKept;
	int const codeHeight = codeShown ? kAccessPageSmallLine + kAccessPageCodeGap + kAccessPageBigLine + kAccessPageLeastWordsGap : 0;
	int const least = codeHeight + kAccessPageLeastRowsGap + rowsHeight + kAccessPageLeastUnderGap + keptUnder * kAccessPageSmallLine;

	// The words get what's left, in whole lines
	int const room = kAccessPageBottom - kAccessPageTop - least;
	int const mostLines = room > 0 ? room / kAccessPageSmallLine : 0;
	at.wordsLines = wordsLines < 0 ? 0 : (wordsLines > mostLines ? mostLines : wordsLines);

	// And what's left after them is shared out evenly between the gaps
	int const gaps = codeShown ? 4 : 3;
	int const left = room - at.wordsLines * kAccessPageSmallLine;
	int const share = left > 0 ? left / gaps : 0;
	at.spare = share > kAccessPageMostSpare ? kAccessPageMostSpare : share;

	int y = kAccessPageTop + at.spare;

	if (codeShown)
	{
		at.labelTop = y;
		y += kAccessPageSmallLine + kAccessPageCodeGap;
		at.codeTop = y;
		y += kAccessPageBigLine + kAccessPageLeastWordsGap + at.spare;
	}
	else
	{
		at.labelTop = -1;
		at.codeTop = -1;
	}

	at.wordsTop = y;
	at.rowsTop = y + at.wordsLines * kAccessPageSmallLine + kAccessPageLeastRowsGap + at.spare;
	at.underTop = at.rowsTop + rowsHeight + kAccessPageLeastUnderGap + at.spare;

	// Only rows too many for the page could leave no room under them: then the lines under them are cut too
	int const underRoom = (kAccessPageBottom - at.underTop) / kAccessPageSmallLine;
	at.underLines = underLines > underRoom ? (underRoom > 0 ? underRoom : 0) : underLines;
	return at;
}

// Whether the page chooses "Copy access code" for the player (PO for the owner, 2026-11-10: "They have to use arrows to get it?"; the
// menus are worked with the keyboard alone, as in 1999). It does as the page opens with the code on it, and the moment a press first
// puts the code there, so a single Enter copies it. Otherwise the row the player is on stays, and the arrows move from it as always.
// opening: the page is opening. codeWasShown: the code was on the page the frame before. codeShown: it's on the page now.
inline bool AccessPage_ChoosesCopy(bool opening, bool codeWasShown, bool codeShown)
{
	return codeShown && (opening || !codeWasShown);
}

#endif
