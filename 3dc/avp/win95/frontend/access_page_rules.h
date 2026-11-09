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

#endif
