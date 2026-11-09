// The Connection check's game-free half: the facts read from Windows, turned into plain words (connection_check.h says what and why).
// Written 2026-11-09 by Melanikus for the AvP port.

#include "connection_check.h"

#include <stdio.h>
#include <string.h>

// The kinds of network whose Windows Firewall rules count: the connected ones where it's on. A firewall that wasn't read is taken to be
// on, so a rule that blocks is never let off for want of an answer.
static unsigned Active(const ConnectionFacts &f)
{
	unsigned const kinds = f.networksRead ? (f.kinds & kConnAllKinds) : 0;
	return f.firewallRead ? (f.firewallOn & kinds) : kinds;
}

// The kinds where AVPx can't get in: a rule against it, or "block everything coming in", where the firewall is on
static unsigned InBlocked(const ConnectionFacts &f)
{
	unsigned const rules = f.rulesRead ? f.avpxInBlocked : 0;
	unsigned const all = f.firewallRead ? f.inAllBlocked : 0;
	return Active(f) & (rules | all);
}

// The kinds where a rule stops AVPx going out (a block wins over any allow, as in Windows Firewall itself)
static unsigned OutBlockedByRule(const ConnectionFacts &f)
{
	return f.rulesRead ? (Active(f) & f.avpxOutBlocked) : 0;
}

// The kinds where the firewall stops every program going out unless a rule lets it, and none lets AVPx
static unsigned OutBlockedByDefault(const ConnectionFacts &f)
{
	unsigned const byDefault = f.firewallRead ? (Active(f) & f.outBlockedByDefault) : 0;
	return f.rulesRead ? (byDefault & ~f.avpxOutAllowed) : 0;
}

ConnectionVerdict ConnectionCheck_Verdict(const ConnectionFacts &f)
{
	// No network at all: nothing can be reached
	if (f.networksRead && f.connected <= 0)
	{
		return kConnBlocks;
	}

	if (OutBlockedByRule(f) || OutBlockedByDefault(f))
	{
		return kConnBlocks;
	}

	// The firewall stops everything going out by default, but its rules couldn't be read, so whether one lets AVPx is unknown
	bool const outUnknown = f.firewallRead && !f.rulesRead && (Active(f) & f.outBlockedByDefault) != 0;

	if ((f.othersRead && f.othersOn > 0) || InBlocked(f) || outUnknown)
	{
		return kConnMayBlock;
	}

	// What wasn't read is said on the page, but isn't taken as a problem
	return kConnFine;
}

// The kinds as words, in the order a player most often has them: "Public", "Public and Private", "Public, Private and Domain"
static void KindsWords(unsigned kinds, char *words, size_t size)
{
	const char *names[3];
	int n = 0;

	if (kinds & kConnPublic)
	{
		names[n++] = "Public";
	}

	if (kinds & kConnPrivate)
	{
		names[n++] = "Private";
	}

	if (kinds & kConnDomain)
	{
		names[n++] = "Domain";
	}

	if (n == 0)
	{
		_snprintf(words, size - 1, "%s", "none");
	}
	else if (n == 1)
	{
		_snprintf(words, size - 1, "%s", names[0]);
	}
	else if (n == 2)
	{
		_snprintf(words, size - 1, "%s and %s", names[0], names[1]);
	}
	else
	{
		_snprintf(words, size - 1, "%s, %s and %s", names[0], names[1], names[2]);
	}

	words[size - 1] = '\0';
}

// One line: this file's own fixed words before and after, with only kinds, counts or safe names between, and always ended. The format
// is always the one written here, never one that came from anywhere.
static void Line(char *line, const char *before, const char *between = "", const char *after = "")
{
	_snprintf(line, kConnLineBytes - 1, "%s%s%s", before, between, after);
	line[kConnLineBytes - 1] = '\0';
}

// The other firewalls' names, "A", "A and B", "A, B and C", then " and 2 more" for those only counted
static void OthersWords(const ConnectionFacts &f, char *words, size_t size)
{
	int const named = f.othersOn < kConnMostOthers ? f.othersOn : kConnMostOthers;
	size_t used = 0;
	words[0] = '\0';

	for (int i = 0; i < named && used + 1 < size; i++)
	{
		const char *between = i == 0 ? "" : (i == named - 1 && f.othersOn == named) ? " and " : ", ";
		int const wrote = _snprintf(words + used, size - used - 1, "%s%s", between, f.others[i]);
		used = wrote < 0 ? size - 1 : used + (size_t)wrote;
		words[size - 1] = '\0';
	}

	if (f.othersOn > named && used + 1 < size)
	{
		_snprintf(words + used, size - used - 1, " and %d more", f.othersOn - named);
		words[size - 1] = '\0';
	}
}

int ConnectionCheck_Lines(const ConnectionFacts &f, char lines[][kConnLineBytes], int most)
{
	char made[kConnMostLines][kConnLineBytes];
	char kinds[48];
	int n = 0;

	// The network
	if (!f.networksRead)
	{
		Line(made[n++], "Network: Windows didn't say which kind this is");
	}
	else if (f.connected <= 0)
	{
		Line(made[n++], "Network: none is connected, so AVPx can't reach anything");
	}
	else if ((f.kinds & kConnAllKinds) == 0)
	{
		Line(made[n++], "Network: connected, but Windows didn't say which kind");
	}
	else
	{
		KindsWords(f.kinds, kinds, sizeof(kinds));
		char count[24] = "";

		if (f.connected > 1)
		{
			_snprintf(count, sizeof(count) - 1, " (%d connected)", f.connected < 99 ? f.connected : 99);
			count[sizeof(count) - 1] = '\0';
		}

		Line(made[n++], "Network: ", kinds, count);
	}

	// Going out, which is all the relay needs
	if (!f.firewallRead)
	{
		Line(made[n++], "Windows Firewall: Windows didn't say how it's set");
	}
	else if (OutBlockedByRule(f))
	{
		KindsWords(OutBlockedByRule(f), kinds, sizeof(kinds));
		Line(made[n++], "Windows Firewall: a rule stops AVPx going out on ", kinds, " networks, so it can't reach the relay");
	}
	else if (OutBlockedByDefault(f))
	{
		KindsWords(OutBlockedByDefault(f), kinds, sizeof(kinds));
		Line(made[n++], "Windows Firewall: on ", kinds, " networks it stops programs going out unless a rule lets them, and none lets AVPx");
	}
	else if (f.networksRead && f.connected > 0 && Active(f) == 0)
	{
		Line(made[n++], "Windows Firewall: off on this network");
	}
	else if (!f.rulesRead)
	{
		Line(made[n++], "Windows Firewall: on, but its rules for AVPx couldn't be read");
	}
	else
	{
		Line(made[n++], "Windows Firewall: lets AVPx go out, which is all playing through the relay needs");
	}

	// Coming in, only for a LAN game at home, and only said when something stops it
	if (InBlocked(f))
	{
		KindsWords(InBlocked(f), kinds, sizeof(kinds));
		Line(made[n++], "On a LAN at home: Windows Firewall stops AVPx coming in on ", kinds,
			" networks, so others there can't join you. Through the relay it doesn't matter");
	}

	// Other firewalls
	if (!f.othersRead)
	{
		Line(made[n++], "Other firewalls: Windows didn't say");
	}
	else if (f.othersOn <= 0)
	{
		Line(made[n++], "Other firewalls: none that Windows knows of");
	}
	else
	{
		char names[kConnMostOthers * (kConnNameLetters + 2) + 24];
		OthersWords(f, names, sizeof(names));
		Line(made[n++], "Other firewalls on: ", names, ". AVPx can't see their rules: look there first if you can't connect");
	}

	int const given = n < most ? n : most;

	for (int i = 0; i < given; i++)
	{
		memcpy(lines[i], made[i], kConnLineBytes);
	}

	return given < 0 ? 0 : given;
}

void ConnectionCheck_LogLine(const ConnectionFacts &f, char *line, size_t size)
{
	if (!line || size == 0)
	{
		return;
	}

	char networks[64];
	char firewall[64];
	char outBlocked[48];
	char outAllowed[48];
	char inBlocked[48];
	char byDefault[48];
	char others[kConnMostOthers * (kConnNameLetters + 2) + 24];

	if (!f.networksRead)
	{
		_snprintf(networks, sizeof(networks) - 1, "%s", "not read");
	}
	else
	{
		char kinds[48];
		KindsWords(f.kinds, kinds, sizeof(kinds));
		_snprintf(networks, sizeof(networks) - 1, "%s (%d connected)", kinds, f.connected < 99 ? (f.connected < 0 ? 0 : f.connected) : 99);
	}

	networks[sizeof(networks) - 1] = '\0';

	if (!f.firewallRead)
	{
		_snprintf(firewall, sizeof(firewall) - 1, "%s", "not read");
	}
	else
	{
		char kinds[48];
		KindsWords(f.firewallOn, kinds, sizeof(kinds));
		_snprintf(firewall, sizeof(firewall) - 1, "on for %s networks", kinds);
	}

	firewall[sizeof(firewall) - 1] = '\0';
	KindsWords(f.firewallRead ? f.outBlockedByDefault : 0, byDefault, sizeof(byDefault));
	KindsWords(f.rulesRead ? f.avpxOutBlocked : 0, outBlocked, sizeof(outBlocked));
	KindsWords(f.rulesRead ? f.avpxOutAllowed : 0, outAllowed, sizeof(outAllowed));
	KindsWords(f.rulesRead ? f.avpxInBlocked : 0, inBlocked, sizeof(inBlocked));

	if (!f.othersRead)
	{
		_snprintf(others, sizeof(others) - 1, "%s", "not read");
		others[sizeof(others) - 1] = '\0';
	}
	else if (f.othersOn <= 0)
	{
		_snprintf(others, sizeof(others) - 1, "%s", "none");
		others[sizeof(others) - 1] = '\0';
	}
	else
	{
		OthersWords(f, others, sizeof(others));
	}

	_snprintf(line, size - 1, "connection check: networks %s; Windows Firewall %s, going out stopped by default on %s; AVPx's rules %s: out "
		"stopped on %s, out let through on %s, in stopped on %s; other firewalls on: %s",
		networks, firewall, byDefault, f.rulesRead ? "read" : "not read", outBlocked, outAllowed, inBlocked, others);
	line[size - 1] = '\0';
}

void ConnectionCheck_SafeName(const wchar_t *from, size_t mostChars, char *to, size_t toSize)
{
	if (!to || toSize == 0)
	{
		return;
	}

	size_t used = 0;

	for (size_t i = 0; from && i < mostChars && from[i] != 0 && used < kConnNameLetters && used + 1 < toSize; i++)
	{
		wchar_t const letter = from[i];
		to[used++] = (letter >= 32 && letter <= 126) ? (char)letter : '?';
	}

	to[used] = '\0';
}
