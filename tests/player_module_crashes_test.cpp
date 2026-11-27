// The test for the crash fixes where the game read the player's module, playerPherModule, while it was none. Written 2026-11-26 by Melanikus
// (Drake) for the AvP port, on Bishop's card after XENA's run 195719 crashed in NPCGetMovementTarget (bh_ais.cpp, fixed on its own by Vasquez).
//
// WHY THE PLAYER'S MODULE CAN BE NONE. playerPherModule is the module the player is in, found again each frame (pheromone.cpp, module.cpp). When
// the player is in none, between two modules for a moment or outside the level, it is NULL (0), and anything that reads through it then
// crashes the game. The game's own GLOBALASSERTs don't stop that: a release build leaves them out. Every one of the 84 lines that name it was
// read for this (the sweep's report says which are safe, and why); these are the ones that read through it with nothing in the way, in code a
// player can reach, and one more found on the way: a module's name copied with no limit.
//
// WHAT IT TESTS
//
// Each crash needs a whole running game, with the player between modules and a console command typed. A small test can't do that, so this
// one reads the game's own source and fails if a fix is taken away or put after the read it guards, as tests/npc_crashes_test.cpp does. Each
// check says what a game without it does.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this, and the program gives back a non-zero result if any check failed. It needs no game files. /FC in its
// settings is how the source files are found.
#include <stdio.h>

#include <fstream>
#include <sstream>
#include <string>

#include "source_slice.h"

static int checksFailed = 0;

static void Check(bool good, const std::string &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// One source file, with Windows line ends made plain so the text below can say what it means
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

	std::string const text = all.str();
	std::string plain;
	plain.reserve(text.size());

	for (char const letter : text)
	{
		if (letter != '\r')
		{
			plain += letter;
		}
	}

	return plain;
}

// The text from one marker up to the next (source_slice.h). A marker that isn't there is a failed check of its own, named by the marker.
static std::string Between(std::string const &text, std::string const &from, std::string const &to)
{
	return SourceSlice(text, from, to, kSliceEndAfterTheStart, [](const std::string &why) { Check(false, why.c_str()); });
}

static bool Has(std::string const &text, std::string const &part)
{
	return text.find(part) != std::string::npos;
}

// Is the first one there, and earlier than the second (if the second is there at all)?
static bool ComesBefore(std::string const &text, std::string const &first, std::string const &second)
{
	std::string::size_type const a = text.find(first);
	std::string::size_type const b = text.find(second);

	return a != std::string::npos && (b == std::string::npos || a < b);
}

// ZONEALERT, a console command (cconvars.cpp, in every build): 0, or a number past the level's modules, means the player's own module
static void TestTheZoneAlert()
{
	puts("A zone alert typed at the console (bh_marine.cpp Console_ZoneAlert):");

	std::string const alert = Between(ReadSourceFile("3dc/avp/bh_marine.cpp"), "void Console_ZoneAlert(int input)", "\n}\n");

	Check(Has(alert, "if ((input<=0)||(input>=ModuleArraySize))"),
		"a number below 1 means the player's own module too: ZONEALERT -1 read the module list one place before its start");
	Check(ComesBefore(alert, "if (!target)", "target->m_index"),
		"and with no module to name (the player between modules, or an empty place in the list) nothing is alerted: it read target->m_index through 0");
}

// SHOWADJACENCIES, a console command (cconvars.cpp, in every build): each frame while it's on, it lists the player's module's neighbours
static void TestTheAdjacencies()
{
	puts("The adjacencies shown each frame (player.cpp ShowAdjacencies):");

	std::string const show = Between(ReadSourceFile("3dc/avp/player.cpp"), "void ShowAdjacencies(void) {", "Adjacencies TO Player's Module");

	Check(ComesBefore(show, "if (!playerPherModule || !playerPherModule->m_aimodule)", "playerPherModule->m_aimodule->m_module_ptrs"),
		"with the player in no module (or a module with no AI module) it stops before reading them: each frame it read them through 0");
}

// PAINTBALL, a console command (game_commands.cpp, in every build): each frame while it's on, a decal is drawn where the player aims
static void TestThePaintball()
{
	puts("The paintball decal drawn each frame (paintball.cpp PaintBallMode_DrawCurrentDecalAtTarget):");

	std::string const paint = Between(ReadSourceFile("3dc/avp/paintball.cpp"), "extern void PaintBallMode_DrawCurrentDecalAtTarget(void)", "\n}\n");

	Check(ComesBefore(paint, "if (!playerPherModule)", "playerPherModule->m_index"),
		"with the player in no module no decal is drawn: each frame it read the decal's module from the player's through 0");
}

// The camera's place for module linking (screenshot.cpp, a debug build's key): a module's name, which comes from the level file, then three
// places, into a line of 300 bytes
static void TestTheCameraLine()
{
	puts("The camera's place for module linking (win95/screenshot.cpp LogCameraPosForModuleLinking):");

	std::string const camera = Between(ReadSourceFile("3dc/avp/win95/screenshot.cpp"), "void LogCameraPosForModuleLinking()", "\n}\n");

	Check(!Has(camera, "strcpy("), "a module's name, from the level file, is never copied without its length known");
	Check(ComesBefore(camera, "> sizeof(output_buffer)", "avp_open_userfile"),
		"and its length is checked against the line's 300 bytes, before the file is opened: a name of about 236 letters or more wrote past them");
}

int main()
{
	TestTheZoneAlert();
	TestTheAdjacencies();
	TestThePaintball();
	TestTheCameraLine();

	puts(checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
