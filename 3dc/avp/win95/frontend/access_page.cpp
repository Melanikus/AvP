// The Online access pages (server #24). access_page.h says what they're for, and why they're only there with -dev. Written 2026-11-09 by
// Melanikus for the AvP port.

#include "3dc.h"
#include "inline.h"
#include "module.h"
#include "menus.h"
#include "menu_gfx.hpp"
#include "language.h"
#include "user_profile.h"
#include "relay_list.h"
#include "net_identity_run.h"   // this run's identity, from networking.cpp (Fifth's)
#include "access_page_rules.h"
#include "access_page.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>

extern int DevMode;

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
		stateLine = Relay_ServerName()[0] ? TEXTSTRING_ACCESS_CANT_REACH : TEXTSTRING_ACCESS_NO_SERVER;
		return;
	}

	memcpy(askedFor, ourFingerprint, sizeof(askedFor));
	sentThisRun = true;
	calledThisRun = true;
	onItsWay = true;
	lastCallAt = Now();
	stateLine = TEXTSTRING_ACCESS_SENDING;
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
	if (!DevMode)
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

	SetTheRows(rows);
	return true;
}

bool AccessPage_QuestionOpened(void)
{
	return DevMode != 0;
}

void AccessPage_Frame(AVPMENU_ELEMENT *rows)
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

void AccessPage_DrawTop(int rowsBottom)
{
	int x = 0;
	int y = 0;

	if (!sentThisRun)
	{
		// Before anything is sent: what Request access does, in plain words, above the rows (the owner's spec). Two lines of the text
		// table, which holds at most 199 characters a line, made one paragraph here.
		char paragraph[420];
		snprintf(paragraph, sizeof(paragraph), "%s %s", GetTextString(TEXTSTRING_ACCESS_EXPLAIN), GetTextString(TEXTSTRING_ACCESS_EXPLAIN_MORE));
		RECT area = { 60, 56, 580, 170 };
		RenderSmallFontString_Wrapped(paragraph, &area, ONE_FIXED, &x, &y);
	}
	else if (ourCode[0])
	{
		// After: the code, large, and what to do with it
		RenderSmallMenuText(GetTextString(TEXTSTRING_ACCESS_YOUR_CODE), MENU_CENTREX, 56, ONE_FIXED, AVPMENUFORMAT_CENTREJUSTIFIED);
		RenderMenuText(ourCode, MENU_CENTREX, 76, ONE_FIXED, AVPMENUFORMAT_CENTREJUSTIFIED);
		RECT area = { 60, 118, 580, 170 };
		RenderSmallFontString_Wrapped(GetTextString(TEXTSTRING_ACCESS_TELL_US), &area, ONE_FIXED, &x, &y);
	}

	if (stateLine != TEXTSTRING_BLANK)
	{
		RenderSmallMenuText(GetTextString(stateLine), MENU_CENTREX, rowsBottom + 16, ONE_FIXED, AVPMENUFORMAT_CENTREJUSTIFIED);
	}
}

void AccessPage_DrawQuestion(void)
{
	int x = 0;
	int y = 0;
	RECT area = { 60, 56, 580, 170 };
	RenderSmallFontString_Wrapped(GetTextString(TEXTSTRING_ACCESS_NEWID_EXPLAIN), &area, ONE_FIXED, &x, &y);
}
