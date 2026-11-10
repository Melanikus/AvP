// The test of the host's count of search questions in the log (3dc/net_search_log.h). Written 2026-11-08 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// - The header on its own: every line's words; nothing is counted or written on a machine that isn't answering searches (a joiner, or a
//   host before it starts or after it stops); the first question gets a line and the rest are only counted, whatever happened to them;
//   the minute line comes only after a minute, starts its counts again, and still comes with nothing in it; the closing line has the whole
//   game's counts; a count stops at its largest instead of going round; the network's clock going round doesn't stop the minute line;
//   and the longest line fits the buffer networking.cpp gives it.
// - By reading networking.cpp: a host starts the count only once its search port is open, and says so when it can't open it; every
//   packet on the port is counted, every question is counted with what happened to it, the minute line is asked for every time the port
//   is looked at, and the closing line comes before the port is closed; and none of those calls is given the asker's address.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this with tests/net_search_log_test.args, and the program gives back a non-zero result if any check failed.
// tests/takeouts/net_search_log_test.json proves each check can fail.

#include "net_search_log.h"

#include <string.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>

#include "source_slice.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static void FailBecause(std::string const &why)
{
	Check(false, why);
}

// A file of the game's, with its line ends written \n whatever the checkout made them. /FC in this test's settings makes __FILE__ this
// file's whole path, so the repository's top is found from it.
static std::string ReadSourceFile(char const *name)
{
	std::string path = __FILE__;
	std::string::size_type const tests = path.rfind("tests");

	if (tests == std::string::npos)
	{
		return "";
	}

	std::ifstream file((path.substr(0, tests) + name).c_str(), std::ios::binary);
	std::stringstream all;
	all << file.rdbuf();

	std::string text = all.str();
	text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
	return text;
}

static bool Has(std::string const &text, char const *words)
{
	return text.find(words) != std::string::npos;
}

static bool Same(char const *line, char const *words)
{
	return strcmp(line, words) == 0;
}

static std::string Between(std::string const &text, char const *from, char const *to)
{
	return SourceSlice(text, from, to, kSliceEndFromOneOn, FailBecause);
}

// Whether every line in text that calls one of the header's functions leaves out the asker's address (addr)
static bool NoCallGivesTheAddress(std::string const &text)
{
	std::stringstream lines(text);
	std::string line;
	int calls = 0;

	while (std::getline(lines, line))
	{
		if (line.find("NetSearchLog_") == std::string::npos)
		{
			continue;
		}

		calls++;

		if (line.find("addr") != std::string::npos)
		{
			return false;
		}
	}

	return calls > 0;
}

int main()
{
	printf("net_search_log_test: the host's count of search questions in the log\n");

	char line[320];
	NetSearchLog log = {};

	// A machine that isn't answering searches counts nothing and writes nothing
	NetSearchLog_Packet(&log);
	Check(!NetSearchLog_Question(&log, kNetSearchAnswered, 1000, line, sizeof(line)) && log.whileOpen.packets == 0 && log.whileOpen.questions == 0,
		"before a game answers searches, nothing is counted and no line is written");
	Check(!NetSearchLog_Minute(&log, 100000, line, sizeof(line)) && !NetSearchLog_Closed(&log, 100000, line, sizeof(line)),
		"and there's no minute line or closing line");

	// Opening
	NetSearchLog_Opened(&log, 5000, line, sizeof(line));
	Check(Same(line, "net: this game answers searches on UDP port 23514 from now"), std::string("the opening line: ") + line);
	Check(log.open && log.whileOpen.packets == 0 && !log.firstQuestionSaid, "it starts counting from nothing");

	// The first question, and the rest
	NetSearchLog_Packet(&log);
	bool const firstSaid = NetSearchLog_Question(&log, kNetSearchAnswered, 17345, line, sizeof(line));
	Check(firstSaid && Same(line, "net: the first search question came 12.3 s after this game began answering, and was answered"), std::string("the first question's line: ") + line);
	NetSearchLog_Packet(&log);
	Check(!NetSearchLog_Question(&log, kNetSearchTooMany, 18000, line, sizeof(line)), "a second question gets no line of its own");
	NetSearchLog_Packet(&log);
	Check(!NetSearchLog_Question(&log, kNetSearchNotSent, 19000, line, sizeof(line)), "nor a third");
	NetSearchLog_Packet(&log);   // a packet that wasn't a question
	Check(log.whileOpen.packets == 4 && log.whileOpen.questions == 3 && log.whileOpen.answered == 1 && log.whileOpen.tooMany == 1 && log.whileOpen.notSent == 1,
		"every packet is counted, and every question by what happened to it");

	// Each outcome goes in its own count. With one of each above, two counts swapped would still read 1 and 1, so here they differ: 2
	// turned away for asking too often, 1 not sent, 3 answered (Take-outs found the first form couldn't tell them apart).
	{
		NetSearchLog apart = {};
		NetSearchLog_Opened(&apart, 0, line, sizeof(line));
		NetSearchLog_Question(&apart, kNetSearchTooMany, 1, line, sizeof(line));
		NetSearchLog_Question(&apart, kNetSearchTooMany, 2, line, sizeof(line));
		NetSearchLog_Question(&apart, kNetSearchNotSent, 3, line, sizeof(line));

		for (int i = 0; i < 3; i++)
		{
			NetSearchLog_Question(&apart, kNetSearchAnswered, 4, line, sizeof(line));
		}

		Check(apart.whileOpen.tooMany == 2 && apart.whileOpen.notSent == 1 && apart.whileOpen.answered == 3 && apart.whileOpen.questions == 6 &&
			apart.sinceLastLine.tooMany == 2 && apart.sinceLastLine.notSent == 1 && apart.sinceLastLine.answered == 3,
			"each outcome is counted in its own count, in the minute's and the game's: 2 too many, 1 not sent, 3 answered");
	}

	// The minute line
	Check(!NetSearchLog_Minute(&log, 5000 + kNetSearchEveryMilliseconds - 1, line, sizeof(line)), "no minute line before a minute has gone");
	Check(NetSearchLog_Minute(&log, 5000 + kNetSearchEveryMilliseconds, line, sizeof(line)) &&
		Same(line, "net: searches in the last 60 s: 4 packets on the search port, 3 of them questions; 1 answered, 1 not (too many at once), 1 not sent"),
		std::string("the minute line: ") + line);
	Check(log.sinceLastLine.packets == 0 && log.sinceLastLine.questions == 0 && log.whileOpen.packets == 4, "the minute's counts start again, and the game's go on");
	Check(NetSearchLog_Minute(&log, 5000 + 2 * kNetSearchEveryMilliseconds, line, sizeof(line)) &&
		Same(line, "net: searches in the last 60 s: 0 packets on the search port, 0 of them questions; 0 answered, 0 not (too many at once), 0 not sent"),
		"a minute with nothing in it still gets its line, which says so");

	// Closing
	Check(NetSearchLog_Closed(&log, 605000, line, sizeof(line)) &&
		Same(line, "net: searches while this game was open, 600.0 s: 4 packets on the search port, 3 of them questions; 1 answered, 1 not (too many at once), 1 not sent"),
		std::string("the closing line: ") + line);
	Check(!log.open && !NetSearchLog_Closed(&log, 606000, line, sizeof(line)), "the closing line comes once");
	NetSearchLog_Packet(&log);
	Check(log.whileOpen.packets == 4 && !NetSearchLog_Minute(&log, 700000, line, sizeof(line)), "after closing, nothing is counted and no minute line comes");

	// What a first question that wasn't answered says
	NetSearchLog_Opened(&log, 0, line, sizeof(line));
	Check(NetSearchLog_Question(&log, kNetSearchTooMany, 1000, line, sizeof(line)) &&
		Same(line, "net: the first search question came 1.0 s after this game began answering, and wasn't answered (too many at once)"), std::string("too many: ") + line);
	NetSearchLog_Opened(&log, 0, line, sizeof(line));
	Check(log.whileOpen.questions == 0 && !log.firstQuestionSaid, "a new game starts its counts again");
	Check(NetSearchLog_Question(&log, kNetSearchNotSent, 2500, line, sizeof(line)) &&
		Same(line, "net: the first search question came 2.5 s after this game began answering, and wasn't answered (the network wouldn't take the answer)"),
		std::string("not sent: ") + line);

	// A count stops at its largest
	unsigned long long count = ~0ull;
	NetSearchLog_Add(&count);
	Check(count == ~0ull, "a count at its largest stays there instead of going round to 0");

	// The network's clock goes round after about 49 days: a minute is still a minute
	NetSearchLog_Opened(&log, 0xFFFFFFFFu - 999u, line, sizeof(line));
	Check(!NetSearchLog_Minute(&log, 58999, line, sizeof(line)) && NetSearchLog_Minute(&log, 59000, line, sizeof(line)), "a minute that runs over the clock's end still ends at a minute");

	// The longest line fits networking.cpp's buffer
	log.whileOpen.packets = log.whileOpen.questions = log.whileOpen.answered = log.whileOpen.tooMany = log.whileOpen.notSent = ~0ull;
	log.open = true;
	log.openedAt = 0;
	Check(NetSearchLog_Closed(&log, 0xFFFFFFFFu, line, sizeof(line)) && Has(line, "18446744073709551615 not sent"), "the longest closing line fits in 320 characters");

	// networking.cpp
	std::string const net = ReadSourceFile("3dc/networking.cpp");
	Check(Has(net, "#include \"net_search_log.h\"") && Has(net, "static_assert(kNetSearchPort == broadcastPort,"), "networking.cpp uses the header, and checks it names the same port");

	// Modified 2026-11-10 by Melanikus for the AvP port (#188): every host is HostGame's since Net_HostGame and Net_HostAtHome became its two
	// ways in, so that's where the search port opens (for a home game alone)
	std::string const hosting = Between(net, "static NetResult HostGame(", "\n}\n");
	Check(Has(hosting, "if (OpenDiscoverySocket())\n\t\t\t{\n\t\t\t\tchar line[160];\n\t\t\t\tNetSearchLog_Opened(&searchLog, enet_time_get(), line, sizeof(line));\n\t\t\t\tLogString(line);"),
		"a host starts counting only once its search port is open, and says so");
	Check(Has(hosting, "LogString(\"net: this game can't answer searches: UDP port \" + std::to_string(broadcastPort) + \" couldn't be opened, so a search won't find it\");"),
		"and says when it couldn't open it");

	std::string const answering = Between(net, "static void Net_CheckForSessionRequest()", "\n}\n");
	Check(Has(answering, "if (NetSearchLog_Minute(&searchLog, enet_time_get(), line, sizeof(line)))\n\t\t{\n\t\t\tLogString(line);"), "every look at the port asks for the minute line");
	Check(Has(answering, "if (len > 0) {\n\t\t\tCon_PrintDebugMessage(\"Got a packet of some sort..\");\n\t\t\tNetSearchLog_Packet(&searchLog);"), "every packet on the port is counted");
	Check(Has(answering, "if (NetSearchLog_Question(&searchLog, kNetSearchTooMany, enet_time_get(), line, sizeof(line)))\n\t\t\t\t{\n\t\t\t\t\tLogString(line);\n\t\t\t\t}\n\n\t\t\t\tcontinue;"),
		"a question turned away for asking too often is counted as that");
	Check(Has(answering, "int const ret = enet_socket_send(broadcastSocket, &addr, &buf, 1);\n\n\t\t\tif (NetSearchLog_Question(&searchLog, ret > 0 ? kNetSearchAnswered : kNetSearchNotSent, enet_time_get(), line, sizeof(line)))"),
		"an answered question is counted as answered only if the answer was sent");
	Check(NoCallGivesTheAddress(answering) && NoCallGivesTheAddress(hosting), "no call to the count is given the asker's address");

	std::string const leaving = Between(net, "void Net_Disconnect()", "\n}\n");
	std::string::size_type const closedAt = leaving.find("NetSearchLog_Closed(&searchLog, enet_time_get(), line, sizeof(line))");
	std::string::size_type const portClosedAt = leaving.find("CloseDiscoverySocket();");
	Check(closedAt != std::string::npos && portClosedAt != std::string::npos && closedAt < portClosedAt, "the closing line comes before the search port is closed");

	if (checksFailed)
	{
		printf("net_search_log_test: %d check(s) FAILED\n", checksFailed);
		return 1;
	}

	printf("net_search_log_test: all checks passed\n");
	return 0;
}
