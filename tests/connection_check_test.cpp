// The test of the Connection check ("nobody blocked silently", 3dc/connection_check.h). Written 2026-11-09 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// 1. The verdict: what does stop AVPx going online (a rule against it going out, a kind of network that stops every program going out
//    with no rule letting AVPx, or no network at all), what may (another firewall, a LAN game's way in), and that what wasn't read is never
//    taken as either. A rule only counts on the kinds of network that are connected, and only where Windows Firewall is on.
// 2. The words, letter for letter, for each case, and that every line ends inside its room, the longest names too.
// 3. The log line: fixed words and counts only.
// 4. A product's name, which is another program's own words: cut, made printable, and never read past what it's given.
// 5. By reading its source (it needs Windows itself), that the Windows half only reads: it changes no setting and no rule, opens only
//    its two fixed Windows pages, and starts a read only when asked.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this file with connection_check.cpp, run from the top of the repository, where the source it reads is found
// from. The program gives back a non-zero result if anything failed.

#include <stdio.h>
#include <string.h>
#include <string>

#include "connection_check.h"

static int checksFailed = 0;

static void Check(bool passed, const char *what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what);

	if (!passed)
	{
		checksFailed++;
	}
}

// A machine where everything was read and nothing stops AVPx: one Private network, Windows Firewall on everywhere, no rule for AVPx, no
// other firewall
static ConnectionFacts AllWell()
{
	ConnectionFacts f = {};
	f.networksRead = true;
	f.connected = 1;
	f.kinds = kConnPrivate;
	f.firewallRead = true;
	f.firewallOn = kConnAllKinds;
	f.rulesRead = true;
	f.othersRead = true;
	return f;
}

static void TestTheVerdict()
{
	puts("The verdict");

	ConnectionFacts f = AllWell();
	Check(ConnectionCheck_Verdict(f) == kConnFine, "nothing found is nothing found");

	f = AllWell();
	f.connected = 0;
	f.kinds = 0;
	Check(ConnectionCheck_Verdict(f) == kConnBlocks, "no network connected stops it");

	// A rule against AVPx going out
	f = AllWell();
	f.kinds = kConnPublic;
	f.avpxOutBlocked = kConnPublic;
	Check(ConnectionCheck_Verdict(f) == kConnBlocks, "a rule that stops AVPx going out on this kind of network stops it");
	f.kinds = kConnPrivate;
	Check(ConnectionCheck_Verdict(f) == kConnFine, "but not on a kind of network that isn't connected");
	f.kinds = kConnPublic;
	f.avpxOutAllowed = kConnPublic;
	Check(ConnectionCheck_Verdict(f) == kConnBlocks, "a rule that blocks wins over one that allows, as in Windows Firewall itself");
	f.firewallOn = kConnPrivate;
	Check(ConnectionCheck_Verdict(f) == kConnFine, "and a rule counts only where Windows Firewall is on");
	f.firewallRead = false;
	Check(ConnectionCheck_Verdict(f) == kConnBlocks, "a firewall that wasn't read is taken as on, so a blocking rule still counts");

	// The kind of network's rule for every program
	f = AllWell();
	f.kinds = kConnPublic;
	f.outBlockedByDefault = kConnPublic;
	Check(ConnectionCheck_Verdict(f) == kConnBlocks, "a kind of network that stops programs going out, with no rule letting AVPx, stops it");
	f.avpxOutAllowed = kConnPublic;
	Check(ConnectionCheck_Verdict(f) == kConnFine, "and a rule letting AVPx out on it is enough");
	f.avpxOutAllowed = kConnPrivate;
	Check(ConnectionCheck_Verdict(f) == kConnBlocks, "only a rule for that kind of network");
	f.avpxOutAllowed = 0;
	f.rulesRead = false;
	Check(ConnectionCheck_Verdict(f) == kConnMayBlock, "with the rules not read, it may stop it: whether one lets AVPx is unknown");

	// Coming in, for a LAN game at home
	f = AllWell();
	f.avpxInBlocked = kConnPrivate;
	Check(ConnectionCheck_Verdict(f) == kConnMayBlock, "a rule that stops AVPx coming in may stop a LAN game");
	f = AllWell();
	f.inAllBlocked = kConnPrivate;
	Check(ConnectionCheck_Verdict(f) == kConnMayBlock, "and so may \"block everything coming in\"");
	f.firewallOn = kConnPublic;
	Check(ConnectionCheck_Verdict(f) == kConnFine, "but not where Windows Firewall is off");

	// Other firewalls
	f = AllWell();
	f.othersOn = 1;
	Check(ConnectionCheck_Verdict(f) == kConnMayBlock, "another firewall that's on may stop it");
	f.othersRead = false;
	Check(ConnectionCheck_Verdict(f) == kConnFine, "and one that couldn't be asked about isn't counted");

	// Nothing read at all
	f = ConnectionFacts();
	Check(ConnectionCheck_Verdict(f) == kConnFine, "what wasn't read is never taken as a problem");

	// The host page's line, for each verdict: none when nothing was found
	Check(strcmp(ConnectionCheck_HostWords(kConnBlocks), "Something on this PC stops AVPx going online: Multiplayer, Connection check says what") == 0 &&
		strcmp(ConnectionCheck_HostWords(kConnMayBlock), "Something on this PC may stop others reaching you: Multiplayer, Connection check says what") == 0 &&
		ConnectionCheck_HostWords(kConnFine)[0] == '\0' && ConnectionCheck_HostWords((ConnectionVerdict)99)[0] == '\0',
		"the host page's line says where to look, and is empty when there's nothing to say");
}

// The lines for the facts, as one text, a line end after each
static std::string Lines(const ConnectionFacts &f, int most = kConnMostLines)
{
	char lines[kConnMostLines][kConnLineBytes];
	memset(lines, 'x', sizeof(lines));
	int const n = ConnectionCheck_Lines(f, lines, most);
	std::string text;

	for (int i = 0; i < n; i++)
	{
		// Each must end inside its room, as a line of the screen's is read
		if (memchr(lines[i], '\0', kConnLineBytes) == NULL)
		{
			return "a line didn't end";
		}

		text += lines[i];
		text += "\n";
	}

	return text;
}

static void TestTheWords()
{
	puts("The words");

	ConnectionFacts f = AllWell();
	Check(Lines(f) == "Network: Private\n"
		"Windows Firewall: lets AVPx go out, which is all playing through the relay needs\n"
		"Other firewalls: none that Windows knows of\n", "all well, in three lines");

	f.connected = 2;
	f.kinds = kConnPublic | kConnPrivate;
	Check(Lines(f).rfind("Network: Public and Private (2 connected)\n", 0) == 0, "two networks, both kinds, and how many");
	f.connected = 3;
	f.kinds = kConnAllKinds;
	Check(Lines(f).rfind("Network: Public, Private and Domain (3 connected)\n", 0) == 0, "three kinds");

	f = AllWell();
	f.connected = 0;
	f.kinds = 0;
	Check(Lines(f).rfind("Network: none is connected, so AVPx can't reach anything\n", 0) == 0, "no network");
	f = AllWell();
	f.kinds = 0;
	Check(Lines(f).rfind("Network: connected, but Windows didn't say which kind\n", 0) == 0, "a network of no known kind");

	f = AllWell();
	f.kinds = kConnPublic;
	f.avpxOutBlocked = kConnPublic;
	Check(Lines(f).find("\nWindows Firewall: a rule stops AVPx going out on Public networks, so it can't reach the relay\n") != std::string::npos,
		"a rule against going out, and on which kind");

	f = AllWell();
	f.kinds = kConnPublic;
	f.outBlockedByDefault = kConnPublic;
	Check(Lines(f).find("\nWindows Firewall: on Public networks it stops programs going out unless a rule lets them, and none lets AVPx\n") !=
		std::string::npos, "a kind of network that stops every program going out");

	f = AllWell();
	f.firewallOn = kConnPublic;
	Check(Lines(f).find("\nWindows Firewall: off on this network\n") != std::string::npos, "the firewall off on the network that's connected");

	f = AllWell();
	f.rulesRead = false;
	Check(Lines(f).find("\nWindows Firewall: on, but its rules for AVPx couldn't be read\n") != std::string::npos, "rules that couldn't be read");

	f = AllWell();
	f.avpxInBlocked = kConnPrivate;
	Check(Lines(f) == "Network: Private\n"
		"Windows Firewall: lets AVPx go out, which is all playing through the relay needs\n"
		"On a LAN at home: Windows Firewall stops AVPx coming in on Private networks, so others there can't join you. Through the relay it "
		"doesn't matter\n"
		"Other firewalls: none that Windows knows of\n", "a LAN game's way in, on a line of its own, only when it's stopped");

	// Other firewalls, named
	f = AllWell();
	f.othersOn = 1;
	strcpy(f.others[0], "Norton");
	Check(Lines(f).find("\nOther firewalls on: Norton. AVPx can't see their rules: look there first if you can't connect\n") != std::string::npos,
		"one other firewall, by name");
	Check(Lines(f).find("connect\nIf one asks whether AvP_Classic.exe may send UDP, it's set to ask about every program: allow it for the addresses "
		"in servers.cfg, and nothing else\n") != std::string::npos, "and, right after, what to do if it asks about AVPx going out");
	f.othersOn = 2;
	strcpy(f.others[1], "ESET");
	Check(Lines(f).find("\nOther firewalls on: Norton and ESET. ") != std::string::npos, "two");
	f.othersOn = 3;
	strcpy(f.others[2], "Kaspersky");
	Check(Lines(f).find("\nOther firewalls on: Norton, ESET and Kaspersky. ") != std::string::npos, "three");
	f.othersOn = 5;
	Check(Lines(f).find("\nOther firewalls on: Norton, ESET, Kaspersky and 2 more. ") != std::string::npos, "and the rest only counted");

	// The longest names there can be still leave the line whole
	f = AllWell();
	f.othersOn = 3;

	for (int i = 0; i < 3; i++)
	{
		memset(f.others[i], 'N', kConnNameLetters);
		f.others[i][kConnNameLetters] = '\0';
	}

	std::string const longest = Lines(f);
	Check(longest.find("look there first if you can't connect\n") != std::string::npos, "three names of the most letters still fit, words and all");

	// What wasn't read
	f = ConnectionFacts();
	Check(Lines(f) == "Network: Windows didn't say which kind this is\n"
		"Windows Firewall: Windows didn't say how it's set\n"
		"Other firewalls: Windows didn't say\n", "nothing read, said so for each");

	// No more lines than asked for
	f = AllWell();
	f.avpxInBlocked = kConnPrivate;
	Check(Lines(f, 2) == "Network: Private\nWindows Firewall: lets AVPx go out, which is all playing through the relay needs\n", "no more lines than room for");
	Check(Lines(f, 0).empty() && Lines(f, -1).empty(), "and none for no room");
}

static void TestTheLogLine()
{
	puts("The log line");

	ConnectionFacts f = AllWell();
	f.othersOn = 1;
	strcpy(f.others[0], "Norton");
	char line[512];
	ConnectionCheck_LogLine(f, line, sizeof(line));
	Check(strcmp(line, "connection check: networks Private (1 connected); Windows Firewall on for Public, Private and Domain networks, going out "
		"stopped by default on none; AVPx's rules read: out stopped on none, out let through on none, in stopped on none; other firewalls on: "
		"Norton") == 0, "fixed words, the kinds, and the names already made safe");

	ConnectionCheck_LogLine(ConnectionFacts(), line, sizeof(line));
	Check(strcmp(line, "connection check: networks not read; Windows Firewall not read, going out stopped by default on none; AVPx's rules not "
		"read: out stopped on none, out let through on none, in stopped on none; other firewalls on: not read") == 0, "and says what wasn't read");

	char small[16];
	memset(small, 'x', sizeof(small));
	ConnectionCheck_LogLine(f, small, sizeof(small));
	Check(small[sizeof(small) - 1] == '\0' && strlen(small) == sizeof(small) - 1, "a line with too little room is cut, and ended");
}

static void TestSafeNames()
{
	puts("A product's name");

	char name[kConnNameLetters + 1];
	ConnectionCheck_SafeName(L"Norton 360", 100, name, sizeof(name));
	Check(strcmp(name, "Norton 360") == 0, "printable letters are kept as they are");
	ConnectionCheck_SafeName(L"Caf\x00e9 \x001b[2J\x2028", 100, name, sizeof(name));
	Check(strcmp(name, "Caf? ?[2J?") == 0, "anything but printable ASCII becomes ?: an accent, an escape, a line separator");
	ConnectionCheck_SafeName(L"0123456789012345678901234567890123456789", 100, name, sizeof(name));
	Check(strlen(name) == kConnNameLetters && memcmp(name, "01234567890123456789012345678901", kConnNameLetters) == 0, "cut to the most letters");
	char roomy[64];
	ConnectionCheck_SafeName(L"0123456789012345678901234567890123456789", 100, roomy, sizeof(roomy));
	Check(strlen(roomy) == kConnNameLetters, "even with more room than that");
	ConnectionCheck_SafeName(NULL, 100, name, sizeof(name));
	Check(name[0] == '\0', "no name is no letters");

	// Given a name with no end, only mostChars of it is read: the rest of this array is never looked at
	wchar_t const unended[4] = {L'A', L'B', L'C', L'D'};
	ConnectionCheck_SafeName(unended, 2, name, sizeof(name));
	Check(strcmp(name, "AB") == 0, "never read past the letters it's given");

	char tiny[3];
	memset(tiny, 'x', sizeof(tiny));
	ConnectionCheck_SafeName(L"Norton", 100, tiny, sizeof(tiny));
	Check(strcmp(tiny, "No") == 0, "a room too small is filled, and ended");
}

// A source file, as text, from the top of the repository, with its CRs taken out. The names are this file's own, never from outside.
static std::string ReadSource(const char *name)
{
	FILE *file = fopen(name, "rb");
	std::string text;

	if (!file)
	{
		return text;
	}

	char block[4096];
	size_t got = 0;

	while ((got = fread(block, 1, sizeof(block), file)) > 0)
	{
		for (size_t i = 0; i < got; i++)
		{
			if (block[i] != '\r')
			{
				text += block[i];
			}
		}
	}

	fclose(file);
	return text;
}

// The code alone: every // comment taken out to its line's end, so a "never" check reads code, never a comment that names what's never
// done. A // inside quotes (such as "windowsdefender://network") is part of the text, not a comment, so quotes are followed too, with
// their \ escapes.
static std::string CodeOnly(const std::string &text)
{
	std::string code;
	bool comment = false;
	char quote = 0;

	for (size_t i = 0; i < text.size(); i++)
	{
		char const c = text[i];

		if (comment)
		{
			if (c == '\n')
			{
				comment = false;
				code += c;
			}

			continue;
		}

		if (quote)
		{
			code += c;

			if (c == '\\' && i + 1 < text.size())
			{
				code += text[++i];
			}
			else if (c == quote || c == '\n')
			{
				quote = 0;
			}

			continue;
		}

		if (c == '/' && i + 1 < text.size() && text[i + 1] == '/')
		{
			comment = true;
			continue;
		}

		if (c == '"' || c == '\'')
		{
			quote = c;
		}

		code += c;
	}

	return code;
}

static size_t Count(const std::string &text, const char *piece)
{
	size_t n = 0;

	for (size_t at = text.find(piece); at != std::string::npos; at = text.find(piece, at + 1))
	{
		n++;
	}

	return n;
}

static void TestTheWindowsHalfOnlyReads()
{
	puts("The Windows half only reads (by reading its source)");

	std::string const source = ReadSource("3dc/win95/connection_check_win.cpp");
	std::string const code = CodeOnly(source);
	Check(code.size() > 4000, "the Windows half was found and read, from the top of the repository");

	// COM's way of changing anything is a put_ call, or a rule list's Add or Remove; none is there
	Check(Count(code, "->put_") == 0 && Count(code, "->Add(") == 0 && Count(code, "->Remove(") == 0 && Count(code, "->EnableRuleGroup") == 0 &&
		Count(code, "->RestoreLocalFirewallDefaults") == 0, "it changes no firewall setting and no rule");

	// The three things it asks, and nothing else is made
	Check(Count(code, "CoCreateInstance(") == 3 && Count(code, "CoCreateInstance(__uuidof(NetworkListManager),") == 1 &&
		Count(code, "CoCreateInstance(__uuidof(NetFwPolicy2),") == 1 && Count(code, "CoCreateInstance(__uuidof(WSCProductList),") == 1,
		"it asks only the Network List Manager, Windows Firewall's policy and the Security Center");

	// Only the two fixed pages, opened by Windows' own handlers
	Check(Count(code, "ShellExecute") == 2 && Count(code, "ShellExecuteW(NULL, L\"open\", L\"ms-settings:network-status\", NULL, NULL, SW_SHOWNORMAL);") == 1 &&
		Count(code, "ShellExecuteW(NULL, L\"open\", L\"windowsdefender://network\", NULL, NULL, SW_SHOWNORMAL);") == 1,
		"it opens only Windows' network settings and Windows Security's firewall page, by fixed addresses");

	// A read starts only when asked, and the log gets only the check's own line
	Check(Count(code, "CreateThread(") == 1 && Count(code, "LogString(") == 1 && Count(code, "ConnectionCheck_LogLine(facts, line, sizeof(line));\n\t\tLogString(line);") == 1,
		"a read starts only from ConnectionCheck_Start, and logs only the check's own line");

	// What it reads is bounded, and a rule list read only in part isn't taken as read
	Check(Count(code, "seen < kMostNetworks") == 1 && Count(code, "seen < kMostRules") == 1 && Count(code, "i < kMostProducts") == 1 &&
		Count(code, "f.rulesRead = ended;") == 1, "how much it reads is bounded, and rules only partly read count as not read");

	// Only this program's rules: compared with this program's own path; coming in, only UDP's
	Check(Count(code, "if (FAILED(rule->get_ApplicationName(&program)) || !IsThisProgram(program, ours))") == 1 &&
		Count(code, "(protocol == NET_FW_IP_PROTOCOL_UDP || protocol == NET_FW_IP_PROTOCOL_ANY)") == 1,
		"only rules for this program count, and coming in only those for UDP, which a LAN game uses");

	// Every name the Security Center gives goes through the safe name before anything else sees it
	Check(Count(code, "ConnectionCheck_SafeName(name, SysStringLen(name), f.others[f.othersOn], sizeof(f.others[f.othersOn]));") == 1 &&
		Count(code, "get_ProductName(") == 1, "a product's name is made safe as it's read, and read nowhere else");
}

// The text from the first start up to the end after it, or "" if either isn't there
static std::string Between(const std::string &text, const char *start, const char *end)
{
	size_t const from = text.find(start);
	size_t const to = from == std::string::npos ? std::string::npos : text.find(end, from + strlen(start));
	return to == std::string::npos ? std::string() : text.substr(from, to - from);
}

static void TestThePage()
{
	puts("The page, and the host page's line (by reading their source: they need the whole game)");

	std::string const data = CodeOnly(ReadSource("3dc/avp/win95/frontend/menu_data.cpp"));
	std::string const menus = CodeOnly(ReadSource("3dc/avp/win95/frontend/menus.cpp"));
	std::string const page = CodeOnly(ReadSource("3dc/avp/win95/frontend/connection_page.cpp"));
	Check(data.size() > 20000 && menus.size() > 100000 && page.size() > 2000, "the menus' sources and the page's were found and read");

	// A row on the Multiplayer page, always there, and the page's three rows
	std::string const multiplayer = Between(data, "static AVPMENU_ELEMENT AvPMenu_Multiplayer[]=", "\n};");
	size_t const rowAt = multiplayer.find("{AVPMENU_ELEMENT_GOTOMENU,\t{TEXTSTRING_CONNCHECK_ROW},\t{AVPMENU_CONNECTION_CHECK},");
	Check(rowAt != std::string::npos && rowAt < multiplayer.find("{AVPMENU_ELEMENT_ENDOFMENU}"),
		"the Multiplayer page has a Connection check row, before its end, so every player has it");
	std::string const rows = Between(data, "static AVPMENU_ELEMENT AvPMenu_ConnectionCheck[]=", "\n};");
	Check(Count(rows, "{AVPMENU_ELEMENT_CONNECTION_CHECK,\t{TEXTSTRING_CONNCHECK_AGAIN},\t{kConnRowAgain},") == 1 &&
		Count(rows, "{AVPMENU_ELEMENT_CONNECTION_CHECK,\t{TEXTSTRING_CONNCHECK_NETWORK},\t{kConnRowNetwork},") == 1 &&
		Count(rows, "{AVPMENU_ELEMENT_CONNECTION_CHECK,\t{TEXTSTRING_CONNCHECK_FIREWALL},\t{kConnRowFirewall},") == 1,
		"the page's rows: Check again, Windows network settings, Windows Firewall");

	// The page asks Windows as it opens and on Check again; its rows do what they say
	Check(Count(menus, "case AVPMENU_CONNECTION_CHECK:") == 1 && Count(Between(menus, "case AVPMENU_CONNECTION_CHECK:", "break;"), "ConnectionPage_Opened();") == 1 &&
		Count(page, "void ConnectionPage_Opened(void)\n{\n\tConnectionCheck_Start();\n}") == 1, "opening the page asks Windows");
	Check(Count(menus, "ConnectionPage_Pressed(elementPtr->MaxValue);") == 1 &&
		Count(page, "case kConnRowAgain:\n\t\t\tConnectionCheck_Start();") == 1 &&
		Count(page, "case kConnRowNetwork:\n\t\t\tConnectionCheck_OpenNetworkSettings();") == 1 &&
		Count(page, "case kConnRowFirewall:\n\t\t\tConnectionCheck_OpenFirewallSettings();") == 1, "and each row does what it says");
	Check(Count(menus, "ConnectionPage_DrawLines(y, y + AvPMenus.MenuHeight);") == 1, "the lines are drawn around the page's rows");

	// The host page's line: never while a join request's rows are up, when the check code is drawn there; and the host page asks
	// Windows at most once a minute
	Check(Count(menus, "if (AvPMenus.CurrentMenu == AVPMENU_COOP_HOST && !(answerRowsShown && AvPMenus.MenuElements == answerRows) &&\n\t\t"
		"ConnectionPage_HostLine(connectionLine, sizeof(connectionLine)))") == 1,
		"the host page's line is never drawn while a join request's rows are up, over the check code");
	Check(Count(page, "if (!hostAsked || (DWORD)(now - hostAskedAt) >= kHostCheckEveryMs)") == 1 && Count(page, "kHostCheckEveryMs = 60000;") == 1,
		"and the host page asks Windows at most once a minute");
}

int main()
{
	TestTheVerdict();
	TestTheWords();
	TestTheLogLine();
	TestSafeNames();
	TestTheWindowsHalfOnlyReads();
	TestThePage();

	if (checksFailed > 0)
	{
		printf("%d checks FAILED\n", checksFailed);
		return 1;
	}

	puts("All checks passed");
	return 0;
}
