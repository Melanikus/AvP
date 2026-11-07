// The test for four crash fixes in the NPC code. Written 2026-11-06 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// Each of the four needs a whole running game to happen: a xenoborg dying, a queen with nowhere to be drawn, an NPC
// outside every room. A small test can't do that, so this one can't show the crashes are gone. What it can do is read
// the game's own source and fail if a fix is taken away or put in the wrong order. Each check says what would happen
// in a game without it.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this, and the program gives back a non-zero result if any check failed. It needs no game
// files. /FC in its settings is how the source files are found. tests/takeouts/npc_crashes_test.json proves each check
// can fail, by breaking a copy of the source one way at a time.

#include <stdio.h>

#include <fstream>
#include <sstream>
#include <string>

static int checksFailed = 0;

static void Check(bool good, const std::string &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// One source file, with Windows line ends made plain so the text below can say what it means.
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

// The text from one mark up to the next, or nothing if either isn't there.
static std::string Between(std::string const &text, std::string const &from, std::string const &to)
{
	std::string::size_type const start = text.find(from);

	if (start == std::string::npos)
	{
		return "";
	}

	std::string::size_type const end = text.find(to, start + from.size());

	return end == std::string::npos ? "" : text.substr(start, end - start);
}

static bool Has(std::string const &text, std::string const &part)
{
	return text.find(part) != std::string::npos;
}

// Is the first one there, and earlier than the second (if the second is there at all)?
static bool ComesBefore(std::string const &text, std::string const &first, std::string const &second)
{
	std::string::size_type const a = text.find(first);

	return a != std::string::npos && a < text.find(second);
}

static void TestTheXenoborg()
{
	puts("A xenoborg that has just been killed:");

	std::string const hurt = Between(ReadSourceFile("3dc/avp/bh_xeno.cpp"), "void XenoborgIsDamaged(STRATEGYBLOCK *sbPtr,", "static void KillXeno(STRATEGYBLOCK *sbPtr,int wounds");
	std::string const kill = "KillXeno(sbPtr,wounds,damage,multiple,incoming);";

	Check(Has(hurt, kill), "the code that hurts it still kills it");

	// KillXeno can free the block, and the lines after this one read and write that block
	std::string const after = Between(hurt, kill, "}");

	Check(Has(after, "return;"), "and stops straight after, so nothing reads the memory KillXeno has freed");
}

// A level's switch can wake a xenoborg through RequestState (bh_types.cpp), and a test can make one and wake it at once.
// Either can come before the xenoborg's first turn, which is where the pointers to its moving parts used to be set.
static void TestTheXenoborgJustMade()
{
	puts("A xenoborg that has just been made:");

	std::string const xeno = ReadSourceFile("3dc/avp/bh_xeno.cpp");
	std::string const ways[2][2] = {
		{ "made during play", Between(xeno, "void CreateXenoborg(VECTORCH *Position,int type)\n{", "static void VerifyDeltaControllers(STRATEGYBLOCK *sbPtr) {") },
		{ "made when a level loads", Between(xeno, "void InitXenoborgBehaviour(void* bhdata, STRATEGYBLOCK *sbPtr)", "void XenoborgBehaviour(STRATEGYBLOCK *sbPtr)") },
	};

	Check(Has(xeno, "static void VerifyDeltaControllers(STRATEGYBLOCK *sbPtr);"), "the code that finds its moving parts can be used before it's written out");

	for (auto const &way : ways)
	{
		std::string const &made = way[1];
		std::string const name = way[0];

		Check(ComesBefore(made, "memset(xenoStatus, 0, sizeof(*xenoStatus));", "NPC_InitMovementData(&(xenoStatus->moveData));"), name + ": its block starts cleared, before anything in it is set");
		Check(ComesBefore(made, "\"TorsoTwist\"", "VerifyDeltaControllers(sbPtr);"), name + ": the pointers to its moving parts are set once the parts exist, not on its first turn");
	}
}

// A sentry gun's state is read for the network as soon as it exists, and its pan and tilt pointers used to be set on its first turn
static void TestTheSentryGunJustMade()
{
	puts("A sentry gun that has just been made:");

	std::string const gun = ReadSourceFile("3dc/avp/bh_agun.cpp");
	std::string const ways[2][2] = {
		{ "made during play", Between(gun, "void CreateSentrygun(VECTORCH *Position,int type)\n{", "void MakeSentrygunNear(STRATEGYBLOCK *sbPtr)") },
		{ "made when a level loads", Between(gun, "void AutoGunBehaveInit(void *bhdata,STRATEGYBLOCK *sbPtr) {", "void MakeSentrygunFar(STRATEGYBLOCK *sbPtr)") },
	};

	Check(Has(gun, "static void Autogun_VerifyDeltaControllers(STRATEGYBLOCK *sbPtr);"), "the code that finds its pan and tilt can be used before it's written out");

	for (auto const &way : ways)
	{
		std::string const &made = way[1];
		std::string const name = way[0];

		Check(ComesBefore(made, "memset(agunStatus, 0, sizeof(*agunStatus));", "/* Initialise xenoborg's stats */"), name + ": its block starts cleared, before anything in it is set");
		Check(ComesBefore(made, "\"GunPan\"", "Autogun_VerifyDeltaControllers(sbPtr);"), name + ": the pointers to its pan and tilt are set once they exist, not on its first turn");
	}
}

static void TestTheEmptyModule()
{
	puts("An NPC outside every room:");

	std::string const visible = Between(ReadSourceFile("3dc/module.cpp"), "int IsModuleVisibleFromModule(MODULE *source, MODULE *target) {", "while(");
	std::string const check = "if ((source==NULL)||(target==NULL)) return(0);";

	Check(Has(visible, check), "an empty room is checked for");
	Check(ComesBefore(visible, check, "source->"), "before the first read of the room, not after it");
	Check(ComesBefore(visible, check, "target->"), "and of the other room");
}

static void TestTheDeadQueen()
{
	puts("Whether a queen is dead:");

	std::string const dead = Between(ReadSourceFile("3dc/avp/bh_ais.cpp"), "int NPC_IsDead(STRATEGYBLOCK *sbPtr)", "case(I_BehaviourNetCorpse):");
	std::string const queen = Between(dead, "case(I_BehaviourQueenAlien):", "case(I_BehaviourPredatorAlien):");

	Check(Has(queen, "(QUEEN_STATUS_BLOCK *)(sbPtr->SBdataptr)"), "a queen is read as a queen");
	Check(!Has(queen, "PAQ_STATUS_BLOCK"), "not as a predator-alien, whose state sits where her foot's pointer is");
	Check(Has(queen, "queenStatusPointer->QueenState==QBS_Dead"), "and it's her own state that says she's dead");
	Check(Has(queen, "break;"), "and she doesn't run on into the predator-alien's lines");
}

static void TestTheQueenWithNoRoomToBeDrawn()
{
	puts("A queen when there's no room left to draw her:");

	std::string const near = Between(ReadSourceFile("3dc/avp/bh_queen.cpp"), "void MakeQueenNear(STRATEGYBLOCK *sbPtr)", "void MakeQueenFar(STRATEGYBLOCK *sbPtr)");
	std::string const leave = "if(dPtr==NULL) return;";

	Check(Has(near, leave), "she stays far, like every other NPC, and it tries again next frame");
	Check(ComesBefore(near, leave, "sbPtr->SBdptr = dPtr;"), "before anything is written through the empty pointer");
}

int main()
{
	TestTheXenoborg();
	TestTheXenoborgJustMade();
	TestTheSentryGunJustMade();
	TestTheEmptyModule();
	TestTheDeadQueen();
	TestTheQueenWithNoRoomToBeDrawn();

	puts(checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
