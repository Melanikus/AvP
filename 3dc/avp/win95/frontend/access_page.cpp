// The Online access pages (server #24). access_page.h says what they're for, and why they're only there when servers.cfg names a server.
// Written 2026-11-09 by Melanikus for the AvP port.

#include "3dc.h"
#include "inline.h"
#include "module.h"
#include "menus.h"
#include "menu_gfx.hpp"
#include "hud_layout.h"     // HUD_FONT_HEIGHT, the small font's height
#include "language.h"
#include "user_profile.h"
#include "relay_list.h"
#include "net_identity_run.h"   // this run's identity, from networking.cpp (Fifth's)
#include "access_page_rules.h"
#include "access_page.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>

// The names the screens make safe before drawing them (online_screens.cpp): a letter the game's font doesn't have shows as ?
void OnlineScreens_SafeName(const char *from, size_t mostBytes, char *to, size_t toSize);

char AccessPageName[kRelayAccessNameMostBytes + 1];

// What the pages know. Only the game's main thread uses these.
static bool nameStarted = false;      // the profile's name has been put in the name row, once this run
static bool sentThisRun = false;      // the player has sent a request this run, for this identity
static bool calledThisRun = false;    // a call has gone to the list this run, for any identity: the list's limit is per address
static bool onItsWay = false;         // a call is on its way, and its answer hasn't come
static uint32_t lastCallAt = 0;       // when the last call went (GetTickCount)
static TEXTSTRING_ID stateLine = TEXTSTRING_BLANK;   // the line under the rows: what the server said, or why nothing was sent
static bool needsANewIdentity = false;   // the server said refused or revoked, which stays so: asking again would only use up calls
static RelayAccessAnswer lastAnswer = kRelayAccessWaiting;

static bool codeWasShown = false;     // the code was on the page the frame before (AccessPage_ChoosesCopy)
static bool chooseCopy = false;       // the next frame chooses Copy access code for the player

static bool haveIdentity = false;
static uint8_t ourStatics[kNetIdentityStaticBytes];
static uint8_t ourFingerprint[kNetIdentityFingerprintBytes];
static uint8_t askedFor[kNetIdentityFingerprintBytes];   // the identity the call on its way is about
static char ourCode[kRelayAccessCodeTextBytes];

static uint32_t Now(void)
{
	return (uint32_t)GetTickCount();
}

// This run's identity, for the request and its code: only its public halves. With none, the line under the rows says why. A kept one
// that couldn't be used waits for the player's yes on the question page: nothing here replaces it.
static void TakeTheIdentity(void)
{
	haveIdentity = Net_OurIdentity(ourStatics, ourFingerprint);

	if (haveIdentity)
	{
		RelayAccess_Code(ourFingerprint, ourCode);
		return;
	}

	ourCode[0] = '\0';
	NetIdentityStoreResult const kept = Net_KeptIdentityResult();
	bool const keptButUnusable = kept == kNetIdentityStoreRefused || kept == kNetIdentityStoreCantUnseal || kept == kNetIdentityStoreCantRead;
	stateLine = keptButUnusable ? TEXTSTRING_ACCESS_KEPT_UNUSABLE : TEXTSTRING_ACCESS_NO_IDENTITY;
}

// Sends the request (relay_list.cpp's thread does the call). Only ever from a pressed row, or from the page opening once a request has
// been sent this run (AccessPage_MayAskByItself).
static void Ask(void)
{
	if (!RelayList_AskForAccess(AccessPageName, ourStatics, ourFingerprint))
	{
		// Modified 2026-11-10 by Melanikus for the AvP port: with no servers.cfg line, nothing is sent (relay_list.cpp), and the page says so,
		// rather than leave the player waiting on a request that never went
		stateLine = !Relay_ServerName()[0] ? TEXTSTRING_ACCESS_NO_SERVER : TEXTSTRING_ACCESS_CANT_REACH;
		return;
	}

	memcpy(askedFor, ourFingerprint, sizeof(askedFor));
	sentThisRun = true;
	calledThisRun = true;
	onItsWay = true;
	lastCallAt = Now();
	stateLine = TEXTSTRING_ACCESS_SENDING;
}

// Whether the code is on the page: once a request has been sent this run, for an identity this PC has
static bool CodeShown(void)
{
	return sentThisRun && ourCode[0] != '\0';
}

// Copy access code's place among the page's rows, or -1 on a page without it
static int CopyRowOf(const AVPMENU_ELEMENT *rows)
{
	for (int at = 0; rows && rows[at].ElementID != AVPMENU_ELEMENT_ENDOFMENU; at++)
	{
		if (rows[at].ElementID == AVPMENU_ELEMENT_ONLINE_ACCESS && rows[at].MaxValue == kAccessRowCopy)
		{
			return at;
		}
	}

	return -1;
}

// Once a request has been sent, its row asks again, and says so
static void SetTheRows(AVPMENU_ELEMENT *rows)
{
	for (AVPMENU_ELEMENT *row = rows; row && row->ElementID != AVPMENU_ELEMENT_ENDOFMENU; row++)
	{
		if (row->ElementID == AVPMENU_ELEMENT_ONLINE_ACCESS && row->MaxValue == kAccessRowRequest)
		{
			row->TextDescription = sentThisRun ? TEXTSTRING_ACCESS_CHECK : TEXTSTRING_ACCESS_REQUEST;
			row->HelpString = sentThisRun ? TEXTSTRING_ACCESS_CHECK_HELP : TEXTSTRING_ACCESS_REQUEST_HELP;
		}
	}
}

// The code on the clipboard, as text. Windows owns the memory once it has taken it, and frees it itself.
static bool CopyToClipboard(const char *text)
{
	size_t const size = strlen(text) + 1;
	HGLOBAL const memory = GlobalAlloc(GMEM_MOVEABLE, size);

	if (!memory)
	{
		return false;
	}

	void *const at = GlobalLock(memory);

	if (!at)
	{
		GlobalFree(memory);
		return false;
	}

	memcpy(at, text, size);
	GlobalUnlock(memory);

	if (!OpenClipboard(NULL))
	{
		GlobalFree(memory);
		return false;
	}

	bool const put = EmptyClipboard() && SetClipboardData(CF_TEXT, memory) != NULL;
	CloseClipboard();

	if (!put)
	{
		GlobalFree(memory);
	}

	return put;
}

bool AccessPage_Opened(AVPMENU_ELEMENT *rows)
{
	// Modified 2026-11-10 by Melanikus for the AvP port (beta.2): the page is for the server servers.cfg names, so with none it isn't there
	if (!Relay_ServerName()[0])
	{
		return false;
	}

	if (!nameStarted)
	{
		RelayAccess_NameFromProfile(UserProfilePtr ? UserProfilePtr->Name : NULL, sizeof(UserProfilePtr->Name), AccessPageName);
		nameStarted = true;

		// Modified 2026-11-09 by Melanikus for the AvP port (Secondary's dry run of beta.2, T2.2): a letter the rule won't take is left out
		// of the name to send, and the page says so, rather than drop it without a word
		if (stateLine == TEXTSTRING_BLANK && RelayAccess_NameLeftLettersOut(UserProfilePtr ? UserProfilePtr->Name : NULL, sizeof(UserProfilePtr->Name)))
		{
			stateLine = TEXTSTRING_ACCESS_NAME_LEFT_OUT;
		}
	}

	TakeTheIdentity();

	// PO's ruling, 2026-11-09: once the player has sent a request this run, opening the page asks again by itself, since that sends
	// nothing new, only the same name and key. Never before a request has been sent, and never within a minute of the last call.
	if (haveIdentity && !needsANewIdentity && AccessPage_MayAskByItself(sentThisRun, onItsWay, Now(), lastCallAt))
	{
		Ask();
	}

	// With the code on the page, the page opens on Copy access code (access_page_rules.h, AccessPage_ChoosesCopy)
	chooseCopy = AccessPage_ChoosesCopy(true, codeWasShown, CodeShown());
	codeWasShown = CodeShown();

	SetTheRows(rows);
	return true;
}

bool AccessPage_QuestionOpened(void)
{
	return Relay_ServerName()[0] != 0;
}

int AccessPage_Frame(AVPMENU_ELEMENT *rows)
{
	bool answered = false;
	RelayAccessAnswer answer = kRelayAccessWaiting;
	const char *why = NULL;

	if (RelayList_AccessCame(&answered, &answer, &why))
	{
		onItsWay = false;

		// An answer about an identity this PC no longer has (a new one was made while it was on its way) says nothing about this one
		if (haveIdentity && memcmp(askedFor, ourFingerprint, sizeof(askedFor)) == 0)
		{
			stateLine = answered ? AccessPage_AnswerText(answer) : TEXTSTRING_ACCESS_CANT_REACH;

			if (answered)
			{
				lastAnswer = answer;
				needsANewIdentity = AccessPage_NeedsANewIdentity(answer);
			}
		}
	}

	SetTheRows(rows);

	// The moment a press first puts the code on the page (Request access sent), the page chooses Copy access code for the player
	if (AccessPage_ChoosesCopy(false, codeWasShown, CodeShown()))
	{
		chooseCopy = true;
	}

	codeWasShown = CodeShown();

	int const copyRow = chooseCopy ? CopyRowOf(rows) : -1;

	if (copyRow >= 0)
	{
		chooseCopy = false;
	}

	return copyRow;
}

bool AccessPage_Pressed(int row, enum AVPMENU_ID *page)
{
	switch (row)
	{
		case kAccessRowRequest:
		{
			if (!haveIdentity)
			{
				TakeTheIdentity();

				if (!haveIdentity)
				{
					return false;
				}
			}

			// Refused or revoked stays so for this identity: the answer is shown again, and nothing is sent
			if (needsANewIdentity)
			{
				stateLine = AccessPage_AnswerText(lastAnswer);
				return false;
			}

			RelayAccessNameRefusal const refusal = RelayAccess_NameRefusal(AccessPageName);

			if (refusal != kRelayAccessNameGood)
			{
				stateLine = AccessPage_NameText(refusal);
				return false;
			}

			switch (AccessPage_PressedAsk(calledThisRun, onItsWay, Now(), lastCallAt))
			{
				case kAccessPageAskSends: Ask(); break;
				case kAccessPageAskOnItsWay: break;
				case kAccessPageAskWaitAMinute: stateLine = TEXTSTRING_ACCESS_WAIT_A_MINUTE; break;
			}

			return false;
		}

		case kAccessRowCopy:
		{
			if (!haveIdentity)
			{
				TakeTheIdentity();
			}

			if (haveIdentity)
			{
				stateLine = CopyToClipboard(ourCode) ? TEXTSTRING_ACCESS_COPIED : TEXTSTRING_ACCESS_NOT_COPIED;
			}

			return false;
		}

		case kAccessRowNewNo:
		{
			*page = AVPMENU_ONLINE_ACCESS;
			return true;
		}

		case kAccessRowNewYes:
		{
			bool const made = Net_MakeNewIdentity();

			// A request sent belonged to the old identity, and so did its answer: the new one hasn't sent any. The list's minute between
			// calls still holds.
			sentThisRun = false;
			needsANewIdentity = false;
			TakeTheIdentity();

			if (haveIdentity)
			{
				stateLine = made ? TEXTSTRING_ACCESS_NEWID_MADE : TEXTSTRING_ACCESS_NEWID_FAILED;
			}

			*page = AVPMENU_ONLINE_ACCESS;
			return true;
		}
	}

	return false;
}

// access_page_rules.h's copies of the menus' numbers must be the menus' own: it can't include menus.h, so these checks are here, where
// the game is built
static_assert(kAccessPageTop == MENU_TOPYEDGE, "the access page starts where every page's rows may start, MENU_TOPYEDGE");
static_assert(kAccessPageBottom == MENU_BOTTOMYEDGE, "the access page ends where every page's rows must end, MENU_BOTTOMYEDGE");
static_assert(kAccessPageBigLine == MENU_FONT_HEIGHT, "the big font's height is MENU_FONT_HEIGHT");
static_assert(kAccessPageSmallLine == HUD_FONT_HEIGHT, "the small font's height is HUD_FONT_HEIGHT");

// How many lines a text takes in the words' box: the same width above the rows and under them
static int LinesOf(const char *text)
{
	return text ? SmallFontString_WrappedLines(text, kAccessPageWordsRight - kAccessPageWordsLeft) : 0;
}

// A text drawn into its place: a box exactly as tall as the lines given, so RenderSmallFontString_Wrapped starts it at its top. One line
// is centred, as that routine does; more are lined up on the left.
static void DrawWrapped(char *text, int top, int lines)
{
	if (!text || lines <= 0)
	{
		return;
	}

	int x = 0;
	int y = 0;
	RECT area = { kAccessPageWordsLeft, top, kAccessPageWordsRight, top + lines * kAccessPageSmallLine };
	RenderSmallFontString_Wrapped(text, &area, ONE_FIXED, &x, &y);
}

// Modified 2026-11-10 by Melanikus for the AvP port, after the owner's picture: the title, "Your access code" and the code were drawn on
// top of each other, and an empty gap sat above the rows. The page now lays itself out under the title, top to bottom, by
// AccessPage_Layout (access_page_rules.h), and gives back where the rows start.
int AccessPage_DrawTop(int menuHeight, int rows)
{
	int const rowsHeight = AccessPage_RowsHeight(menuHeight, rows);
	char paragraph[420];
	char *words = NULL;
	bool codeShown = false;

	if (!sentThisRun)
	{
		// Before anything is sent: what Request access does, in plain words, above the rows (the owner's spec). Lines of the text table,
		// which holds at most 199 characters a line, made one paragraph here, with the server's name as servers.cfg gives it (modified
		// 2026-11-10 by Melanikus for the AvP port: the server is the line's, no longer always Mel's). The name comes from a file from
		// outside the game, so it's made safe before it's drawn, and it's only ever a %s's argument, never the format.
		char server[kServersCfgNameLetters + 1];
		OnlineScreens_SafeName(Relay_ServerName(), kServersCfgNameLetters, server, sizeof(server));
		snprintf(paragraph, sizeof(paragraph), "%s %s %s %s", GetTextString(TEXTSTRING_ACCESS_EXPLAIN_ASKS), server,
			GetTextString(TEXTSTRING_ACCESS_EXPLAIN), GetTextString(TEXTSTRING_ACCESS_EXPLAIN_MORE));
		words = paragraph;
	}
	else if (ourCode[0])
	{
		// After: the code, large, and what to do with it
		words = GetTextString(TEXTSTRING_ACCESS_TELL_US);
		codeShown = true;
	}

	// What the server said, or why nothing was sent, under the rows: wrapped to the words' width (the owner, 2026-11-10: as one line it
	// ran almost from one edge of the screen to the other)
	char *under = stateLine != TEXTSTRING_BLANK ? GetTextString(stateLine) : NULL;
	AccessPageLayout const at = AccessPage_Layout(codeShown, LinesOf(words), rowsHeight, LinesOf(under));

	if (codeShown)
	{
		RenderSmallMenuText(GetTextString(TEXTSTRING_ACCESS_YOUR_CODE), MENU_CENTREX, at.labelTop, ONE_FIXED, AVPMENUFORMAT_CENTREJUSTIFIED);
		RenderMenuText(ourCode, MENU_CENTREX, at.codeTop, ONE_FIXED, AVPMENUFORMAT_CENTREJUSTIFIED);
	}

	DrawWrapped(words, at.wordsTop, at.wordsLines);
	DrawWrapped(under, at.underTop, at.underLines);
	return at.rowsTop;
}

// The question before a new identity: its words under the title, and its two rows right under them, by the same layout
int AccessPage_DrawQuestion(int menuHeight, int rows)
{
	char *words = GetTextString(TEXTSTRING_ACCESS_NEWID_EXPLAIN);
	AccessPageLayout const at = AccessPage_Layout(false, LinesOf(words), AccessPage_RowsHeight(menuHeight, rows), 0);
	DrawWrapped(words, at.wordsTop, at.wordsLines);
	return at.rowsTop;
}

bool AccessPage_CallOnItsWay(void)
{
	return onItsWay;
}
