// The test for 3dc/net_watch.h, the 10-second report's lines on frames and on figures that wait for news (issue #88). Written 2026-11-08
// by Melanikus for the AvP port.
//
// tests/build.ps1 compiles this, and the program gives back a non-zero result if any check failed. It uses no network and runs none of
// the game: the counting and the words are all in the header. The rows marked SOURCE read the game's files, to check each part of the
// game tells the report what the header says it does.

#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <climits>
#include <fstream>
#include <sstream>
#include <string>

#include "net_watch.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static void Frames()
{
	Check(NetFrameWatch_Milliseconds(65536) == 1000, "a second of frame time is 1000 ms");
	Check(NetFrameWatch_Milliseconds(65536 / 60) == 16, "a 60th of a second is 16 ms, rounded down");
	Check(NetFrameWatch_Milliseconds(2163) == 33, "just over 33 ms is 33");
	Check(NetFrameWatch_Milliseconds(0) == 0 && NetFrameWatch_Milliseconds(-5) == 0, "no time, or less, is 0");
	Check(NetFrameWatch_Milliseconds(INT_MAX) == 32767999, "the longest frame time doesn't go round");

	NetFrameWatch watch = {};
	NetFrameWatch_Note(&watch, 16);
	NetFrameWatch_Note(&watch, 40);
	NetFrameWatch_Note(&watch, 33);
	NetFrameWatch_Note(&watch, 34);
	Check(watch.frames == 4, "every frame is counted");
	Check(watch.slow == 2, "frames over 33 ms are slow, and one of exactly 33 isn't");
	Check(watch.slowestMs == 40, "the slowest is kept");

	char words[200];
	NetFrameWatch_Line(watch, 10, words, sizeof(words));
	Check(std::string(words) == "net: frames: 4 in the last 10 s, the slowest 40 ms, 2 slower than 33 ms", "the frames' line");

	// Filled first, so a line that didn't end itself would show: its last byte would still be a 'z'
	char small[16];
	memset(small, 'z', sizeof(small));
	NetFrameWatch_Line(watch, 10, small, sizeof(small));
	Check(small[sizeof(small) - 1] == '\0' && strlen(small) == sizeof(small) - 1, "a line in a small room is cut short and still ends within it");

	char untouched[4] = { 'x', 'x', 'x', 0 };
	NetFrameWatch_Line(watch, 10, untouched, 0);
	Check(untouched[0] == 'x', "with no room at all, nothing is written");
}

static void News()
{
	NetFigureNews news = {};
	unsigned int waited = 12345;

	Check(!NetFigureNews_Came(&news, 1000, &waited) && waited == 12345, "a figure's first news has nothing to wait from");
	Check(!NetFigureNews_Came(&news, 1050, &waited), "news 50 ms later isn't a wait");
	Check(!NetFigureNews_Came(&news, 1150, &waited), "nor is news exactly 100 ms later");
	Check(NetFigureNews_Came(&news, 1251, &waited) && waited == 101, "news 101 ms later is a wait of 101 ms");

	NetFigureNews round = {};
	NetFigureNews_Came(&round, 0xFFFFFFF0u, &waited);
	Check(NetFigureNews_Came(&round, 0x64u, &waited) && waited == 0x74u, "the clock going round still gives the right wait");
}

static void Figures()
{
	NetFigureWatch watch = {};
	NetFigureWatch_Note(&watch, 100, 9000, false);
	Check(watch.waits == 0 && watch.biggestJumpMm == 0, "a wait of 100 ms or less isn't counted, whatever the jump");

	NetFigureWatch_Note(&watch, 150, 2000, false);
	NetFigureWatch_Note(&watch, 740, 1449, true);
	NetFigureWatch_Note(&watch, 200, 3000, false);
	Check(watch.waits == 3, "each longer wait is counted");
	Check(watch.longestMs == 740 && watch.jumpAfterLongestMm == 1449 && watch.longestWasAPlayer, "the longest is kept, with its jump and whose it was");
	Check(watch.biggestJumpMm == 3000, "and the biggest jump after any wait");

	char words[200];
	NetFigureWatch_Line(watch, 10, words, sizeof(words));
	Check(std::string(words) == "net: figures: 3 went more than 100 ms with nothing new in the last 10 s; the longest 740 ms (a player's), then a jump of 1.4 m; the biggest jump after a wait 3.0 m",
		"the figures' line, with metres to a tenth");

	NetFigureWatch npc = {};
	NetFigureWatch_Note(&npc, 300, 1450, false);
	NetFigureWatch_Line(npc, 10, words, sizeof(words));
	Check(std::string(words) == "net: figures: 1 went more than 100 ms with nothing new in the last 10 s; the longest 300 ms (an NPC's), then a jump of 1.5 m; the biggest jump after a wait 1.5 m",
		"an NPC's, and a jump halfway between tenths rounds up");

	NetFigureWatch none = {};
	NetFigureWatch_Line(none, 9, words, sizeof(words));
	Check(std::string(words) == "net: figures: none went more than 100 ms with nothing new in the last 9 s", "and when none waited, it says so");
}

static std::string ReadSourceFile(char const *name)
{
	// /FC in this test's settings makes __FILE__ the whole path of this file, so the repository's top is found from it
	std::string path = __FILE__;
	std::string::size_type const tests = path.rfind("tests");

	if (tests == std::string::npos)
	{
		return "";
	}

	std::ifstream file((path.substr(0, tests) + name).c_str(), std::ios::binary);
	std::stringstream all;
	all << file.rdbuf();

	// A file has one kind of line ending or the other depending on how it was cloned, and these rows shouldn't care
	std::string text = all.str();
	text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
	return text;
}

static bool Has(std::string const &text, std::string const &words)
{
	return text.find(words) != std::string::npos;
}

static bool ComesBefore(std::string const &text, std::string const &first, std::string const &second)
{
	std::string::size_type const one = text.find(first);
	std::string::size_type const two = text.find(second);

	return one != std::string::npos && two != std::string::npos && one < two;
}

// The text from one marker up to the next one after it. Empty if either isn't there, so a row looking inside it fails instead of passing
// on nothing.
static std::string Between(std::string const &text, std::string const &from, std::string const &to)
{
	std::string::size_type const start = text.find(from);

	if (start == std::string::npos)
	{
		return "";
	}

	std::string::size_type const end = text.find(to, start + 1);

	return (end == std::string::npos) ? "" : text.substr(start, end - start);
}

static void TheGameUsesIt()
{
	std::string const net = ReadSourceFile("3dc/networking.cpp");
	Check(!net.empty(), "SOURCE: networking.cpp is read");
	Check(Has(net, "NetFrameWatch_Note(&netFrames, NetFrameWatch_Milliseconds(frameTime));"), "SOURCE: a frame's time is counted, in milliseconds");
	Check(Has(net, "NetFigureWatch_Note(&netFigures, waitedMs, jumpMm, aPlayer);"), "SOURCE: and so is a figure's wait");

	std::string const logging = Between(net, "static void LogConnectionNumbers()\n{", "\nstatic void LogOlderVersionsToldNo()");
	std::string const lines = Between(logging, "\tif (linesDue)\n\t{\n\t\tNetStats_BytesLine(", "\t\tnetStatsLastLines = now;");
	Check(Has(lines, "NetFrameWatch_Line(netFrames, seconds, words, sizeof(words));\n\t\tLogString(words);"), "SOURCE: the frames' line goes in the log with the 10-second lines");
	Check(Has(lines, "NetFigureWatch_Line(netFigures, seconds, words, sizeof(words));\n\t\tLogString(words);"), "SOURCE: and so does the figures' line");
	Check(Has(lines, "netFrames = NetFrameWatch();\n\t\tnetFigures = NetFigureWatch();"), "SOURCE: and both counts start again after them");
	Check(Has(Between(logging, "if (host != netStatsHost)", "return;"), "netFrames = NetFrameWatch();\n\t\tnetFigures = NetFigureWatch();"), "SOURCE: and with a new game's host");

	std::string const pldnet = ReadSourceFile("3dc/avp/win95/pldnet.cpp");
	Check(Has(pldnet, "\t\tPlayerGhostExtrapolation();\n#endif\n\t\t// For the 10-second report: how long the frame took (net_watch.h). This is the frame just played, so it counts frames of play only.\n\t\tNet_NoteFrame(RealFrameTime);"),
		"SOURCE: each frame of play tells the report its time, where the ghosts are moved");

	std::string const ghosts = ReadSourceFile("3dc/avp/win95/pldghost.cpp");
	std::string const update = Between(ghosts, "void UpdateGhost(STRATEGYBLOCK *sbPtr,VECTORCH *position,EULER *orientation,int sequence, int special)", "UpdateObjectTrails(sbPtr);");
	Check(ComesBefore(update, "Ghost_NewsCame(ghostData, &dynPtr->Position);", "dynPtr->Position = *position;"), "SOURCE: a player's news is noted before it moves the ghost");
	std::string const alien = Between(ghosts, "void UpdateAlienAIGhost(", "UpdateAlienAIGhostAnimSequence(");
	Check(ComesBefore(alien, "Ghost_NewsCame(ghostData, &dynPtr->Position);", "dynPtr->Position = *position;"), "SOURCE: so is an alien's, in the old alien games");

	std::string const each = Between(ghosts, "void PlayerGhostExtrapolation()", "if(UseExtrapolation)");
	Check(ComesBefore(each, "if(ghostData->onlyValidFar) continue;", "if (NetFigureNews_Came(&ghostData->news, timeGetTime(), &waitedMs))"), "SOURCE: once a frame, each ghost's news is looked at");
	Check(Has(each, "SubVector(&ghostData->placeBeforeNews, &moved);") && Has(each, "Net_NoteFigureWait(waitedMs, (unsigned int)Approximate3dMagnitude(&moved), aPlayer);"), "SOURCE: and a long wait goes to the report with how far the news moved it");
	Check(Has(each, "ghostData->newsThisFrame = 0;\n\t\t\t\t}"), "SOURCE: and the frame's news is done with");

	std::string const npcs = ReadSourceFile("3dc/avp/win95/npc_ghosts.cpp");
	std::string const apply = Between(npcs, "void NpcGhosts_ApplyState(", "CreateEulerMatrix(");
	Check(ComesBefore(apply, "Ghost_NewsCame(ghostData, &dynPtr->Position);", "dynPtr->Position = *position;"), "SOURCE: an NPC's news is noted before it moves the ghost");

	std::string const record = ReadSourceFile("3dc/avp/win95/pldghost.h");
	Check(Has(Between(record, "inline void Ghost_NewsCame(", "}\n}"), "if (!ghostData->newsThisFrame)\n\t{\n\t\tghostData->placeBeforeNews = *drawnAt;\n\t\tghostData->newsThisFrame = 1;"),
		"SOURCE: only the frame's first news keeps where the ghost was, so the jump is the whole frame's");
}

int main()
{
	Frames();
	News();
	Figures();
	TheGameUsesIt();

	if (checksFailed)
	{
		printf("FAILED: %d checks\n", checksFailed);
		return 1;
	}

	printf("all net watch checks passed\n");
	return 0;
}
