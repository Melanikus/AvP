// The Connection check: what on this PC could stop AVPx playing online, said in plain words, so nobody is blocked without being told why
// ("nobody blocked silently", beta.2's goal: players anywhere, through the relay, with the least hurdle). Written 2026-11-09 by Melanikus
// for the AvP port.
//
// WHY THESE THINGS. Through the relay the game only ever goes OUT: to the list (HTTPS) and to the relay (UDP). So Windows needs no rule
// to let it in, and what can still stop it without a word is:
//   - a Windows Firewall rule that stops AVPx going out, or a kind of network whose rule for everything is to stop going out;
//   - another firewall (Norton's, say), whose rules Windows can't show us;
//   - on a LAN game at home (the owner's ruling, 2026-11-09: plain LAN stays, at home only), a rule that stops AVPx coming in. Pressing
//     Cancel once on Windows' "Allow access?" question leaves just such a rule.
//
// TWO HALVES. This file, and connection_check.cpp, are game-free: they turn the facts into words, and connection_check_test tries every
// case. What reads the facts from Windows is win95/connection_check_win.cpp, read only, without admin, on a thread of its own, and
// nothing it reads ever leaves the PC: the words go on the screen, and one line of fixed words goes in the log.

#ifndef CONNECTION_CHECK_H
#define CONNECTION_CHECK_H

#include <stddef.h>

// The kinds of network, as bits, with the same values Windows Firewall gives its profiles (NET_FW_PROFILE2_DOMAIN, _PRIVATE and _PUBLIC
// in netfw.h), so the facts read from it need no change
enum
{
	kConnDomain = 1,
	kConnPrivate = 2,
	kConnPublic = 4,
	kConnAllKinds = kConnDomain | kConnPrivate | kConnPublic
};

enum
{
	kConnMostOthers = 3,       // other firewalls kept by name; any more are only counted (3 names of the most letters fit in a line)
	kConnNameLetters = 32,     // the most letters of a product's name that are kept
	kConnLineBytes = 200,      // the most a line of the check's words takes, its 0 too
	kConnMostLines = 5         // network, going out, coming in, other firewalls, and what to do when one asks
};

// What was read. Each "Read" says whether Windows answered that part at all: a part it didn't answer says so in the words, and is never
// taken as good or bad. Each set of bits is the kinds of network for which it's so.
struct ConnectionFacts
{
	// The connected networks (Windows' Network List Manager): how many, and their kinds. Never their names, which can be personal.
	bool networksRead;
	int connected;
	unsigned kinds;

	// Windows Firewall, for each kind: on; its rule for everything is to stop programs going out; it blocks everything coming in
	bool firewallRead;
	unsigned firewallOn;
	unsigned outBlockedByDefault;
	unsigned inAllBlocked;

	// Windows Firewall's own rules for this program, the ones that are switched on: those that stop it going out, let it out, or stop it
	// coming in, by the kinds they're for
	bool rulesRead;
	unsigned avpxOutBlocked;
	unsigned avpxOutAllowed;
	unsigned avpxInBlocked;

	// Other firewalls Windows knows of (its Security Center) that are on: how many, and the first kConnMostOthers by name, each made safe
	// by ConnectionCheck_SafeName
	bool othersRead;
	int othersOn;
	char others[kConnMostOthers][kConnNameLetters + 1];
};

// How bad it is, for the host page's one line: nothing found; something that may stop it (another firewall, or a LAN game's way in);
// or something that does stop it going online (a rule, the kind's own rule, or no network at all)
enum ConnectionVerdict
{
	kConnFine,
	kConnMayBlock,
	kConnBlocks
};

ConnectionVerdict ConnectionCheck_Verdict(const ConnectionFacts &facts);

// The check's lines, in order: the network, going out, coming in (only when something stops it), other firewalls, and, when there are
// any, what to do if one asks about AVPx going out (a firewall set to ask about every program does: the owner's own, 2026-11-09, asked
// whether AVP_Classic.exe may send UDP; Windows Firewall as it comes never asks about going out, and another firewall's mode can't be
// read). Each is fixed words with only the kinds of network and the safe names put in. Gives how many lines it wrote, at most `most`.
int ConnectionCheck_Lines(const ConnectionFacts &facts, char lines[][kConnLineBytes], int most);

// One line for the log: the same facts in fixed words and counts. No address, no network's name and no path is ever in it.
void ConnectionCheck_LogLine(const ConnectionFacts &facts, char *line, size_t size);

// A product's name, as Windows gives it (another program's own words, so hostile: AVP Tao 3), made into what may be drawn and logged:
// at most kConnNameLetters letters, each printable ASCII (32 to 126) as it is and anything else as ?. Reads at most mostChars of from,
// stopping at its 0. to always ends with a 0.
void ConnectionCheck_SafeName(const wchar_t *from, size_t mostChars, char *to, size_t toSize);

// ---- The Windows half (win95/connection_check_win.cpp)

// Starts a read on a thread of its own, unless one is going already
void ConnectionCheck_Start(void);

// True once a read has finished, with what it found in facts. False before the first one ends.
bool ConnectionCheck_Result(ConnectionFacts *facts);

// Whether a read is going on now
bool ConnectionCheck_Reading(void);

// Windows' own pages, opened by Windows' own handlers for two fixed addresses: its network settings, and Windows Security's firewall page
void ConnectionCheck_OpenNetworkSettings(void);
void ConnectionCheck_OpenFirewallSettings(void);

#endif
