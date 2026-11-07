// The test for four crash fixes in the NPC code. Written 2026-11-06 by Melanikus for the AvP port.
// Modified 2026-11-07 by Melanikus for the AvP port:
//   it also checks how a marine's glow is chosen when you play the alien
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

#include "npc_file_checks.h"

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
		Check(Has(made, "VerifyDeltaControllers(sbPtr);") && ComesBefore(made, "\"TorsoTwist\"", "VerifyDeltaControllers(sbPtr);"), name + ": the pointers to its moving parts are set once the parts exist, not on its first turn");
	}
}

// A sentry gun's pan and tilt pointers used to be set on its first turn, so anything that asked where it pointed before then read
// whatever the memory last held
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
		Check(Has(made, "Autogun_VerifyDeltaControllers(sbPtr);") && ComesBefore(made, "\"GunPan\"", "Autogun_VerifyDeltaControllers(sbPtr);"), name + ": the pointers to its pan and tilt are set once they exist, not on its first turn");
	}
}

// Making an alien, facehugger or queen can stop before its block exists (no room left for its movement, or bad level data), and
// RemoveBehaviourStrategy then cleans it up. Reading the missing block there would crash.
static void TestTheHalfMadeNpc()
{
	puts("An NPC whose making stopped before its block existed:");

	std::string const cleanup = Between(ReadSourceFile("3dc/avp/bh_types.cpp"), "void RemoveBehaviourStrategy(STRATEGYBLOCK* sbptr)", "case I_BehaviourNetGhost:");

	Check(Has(cleanup, "\t\t\t\tif (asb)\n\t\t\t\t{\n\t\t\t\t\tDispel_HModel(&asb->HModelController);"), "an alien's cleanup only reads its block if there is one");
	Check(Has(cleanup, "\t\t\t\tif(fhugStatusPointer->soundHandle!=SOUND_NOACTIVEINDEX) Sound_Stop(fhugStatusPointer->soundHandle);\n\t\t\t\tDispel_HModel(&fhugStatusPointer->HModelController);\n\t\t\t}"), "and a facehugger's");
	Check(Has(cleanup, "\t\t\t\t//stop any sound the queen is making\n\t\t\t\tif(queenStatusPointer->soundHandle!=SOUND_NOACTIVEINDEX) Sound_Stop(queenStatusPointer->soundHandle);\n\t\t\t}"), "and a queen's");
}

// NPC-WEAPON-NULL-1: a level or save file names an NPC's weapon by number, and a number that isn't on the list found nothing,
// which the code then read through. A file is hostile input, so each place now checks, and says in the log what it did instead.
static void TestTheWeaponFromAFile()
{
	puts("A marine's or predator's weapon named by a level or save file:");

	// The check itself, with a list of ten weapons numbered 0 to 9, as a save could give them
	Check(!NpcFile_WeaponNumberIsOnList(-1, 10), "a weapon number below the list is refused");
	Check(!NpcFile_WeaponNumberIsOnList(10, 10), "and the list's end mark itself");
	Check(!NpcFile_WeaponNumberIsOnList(2147483647, 10), "and the largest number a save can hold");
	Check(!NpcFile_WeaponNumberIsOnList(-2147483647 - 1, 10), "and the smallest");
	Check(NpcFile_WeaponNumberIsOnList(0, 10) && NpcFile_WeaponNumberIsOnList(9, 10), "but the first and the last weapon on it are kept");

	std::string const marine = ReadSourceFile("3dc/avp/bh_marine.cpp");
	std::string const predator = ReadSourceFile("3dc/avp/bh_predator.cpp");
	std::string const marineMade = Between(marine, "void InitMarineBehaviour(", "#if 0");
	std::string const marineLoaded = Between(marine, "void LoadStrategy_Marine(SAVE_BLOCK_STRATEGY_HEADER* header)", "void SaveStrategy_Marine(");
	std::string const predatorMade = Between(predator, "void InitPredatorBehaviour(", "void InitDormantPredatorBehaviour(");
	std::string const predatorLoaded = Between(predator, "void LoadStrategy_Predator(SAVE_BLOCK_STRATEGY_HEADER* header)", "void SaveStrategy_Predator(");

	Check(Has(marine, "LogString(std::string(\"npc: refused: \") + why);") && Has(predator, "LogString(std::string(\"npc: refused: \") + why);"), "a refusal is written in the log, with npc: refused: in front");

	Check(ComesBefore(marineMade, "if (!marineStatus->My_Weapon)", "marineStatus->My_Weapon->Android"), "a level's marine with a kind that isn't on the list is caught before his weapon is read");
	Check(Has(marineMade, "NoteRefusedFromAFile(\"a level or save named a kind of marine that isn't on the list, so he's an ordinary marine with a pulse rifle\");\n\t\t\tmarineStatus->My_Weapon=GetThisNPCMarineWeapon(MNPCW_PulseRifle);"), "and he's an ordinary marine, and the log says so in those words");

	Check(ComesBefore(marineLoaded, "if (!NpcFile_WeaponNumberIsOnList(block->weapon_id, MNPCW_End) || !GetThisNPCMarineWeapon(static_cast<enum marine_npc_weapons>(block->weapon_id)))", "sbPtr = FindSBWithName(header->SBname);"), "a save's marine with a weapon that isn't on the list is refused before anything is changed");
	Check(Has(marineLoaded, "NoteRefusedFromAFile(\"a save named a marine weapon that isn't on the list, so that marine's part of the save is left out\");\n\t\treturn;"), "and the log says so in those words");

	Check(ComesBefore(predatorMade, "if (!predatorStatus->Selected_Weapon)", "predatorStatus->Selected_Weapon->Riffname"), "a level's predator with a weapon that isn't on the list is caught before it's read");
	Check(Has(predatorMade, "NoteRefusedFromAFile(\"a level named a predator weapon that isn't on the list, so he has his wristblade\");"), "and he has his wristblade, and the log says so in those words");

	Check(ComesBefore(predatorLoaded, "if (!NpcFile_WeaponNumberIsOnList(block->weapon_id, PNPCW_End) || !GetThisNPCPredatorWeapon(static_cast<enum predator_npc_weapons>(block->weapon_id)))", "sbPtr = FindSBWithName(header->SBname);"), "a save's predator with a weapon that isn't on the list is refused before anything is changed");
	Check(Has(predatorLoaded, "NoteRefusedFromAFile(\"a save named a predator weapon that isn't on the list, so that predator's part of the save is left out\");\n\t\treturn;"), "and the log says so in those words");
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

// When you play the alien, every body glows in a colour that says what it is (DoAlienEnergyView in kshape.cpp). The
// marine's part of that choice used to carry on into the part for another player's stand-in, and read the marine's
// details as if they were a stand-in's. With today's numbers that happened to change nothing, which is luck, not a
// rule. Only a running game with a marine in view reaches these lines, so this reads the source.
static void TestTheMarineSeenByAnAlien()
{
	puts("A marine's glow when you play the alien:");

	std::string const glow = Between(ReadSourceFile("3dc/kshape.cpp"), "void DoAlienEnergyView(DISPLAYBLOCK *dispPtr)", "case I_BehaviourNetCorpse: {");
	std::string const marine = Between(glow, "case I_BehaviourSeal: {", "case I_BehaviourNetGhost: {");
	std::string const colour = "colour = MARINES_LIFEFORCE_GLOW_COLOUR;";
	std::string::size_type const colourAt = marine.find(colour);

	Check(colourAt != std::string::npos, "the marine's part still gives him the marine colour");
	Check(colourAt != std::string::npos && marine.find("break;", colourAt) != std::string::npos,
	      "and stops there, so his details aren't read as another player's stand-in's");
}

int main()
{
	TestTheXenoborg();
	TestTheXenoborgJustMade();
	TestTheSentryGunJustMade();
	TestTheHalfMadeNpc();
	TestTheWeaponFromAFile();
	TestTheEmptyModule();
	TestTheDeadQueen();
	TestTheQueenWithNoRoomToBeDrawn();
	TestTheMarineSeenByAnAlien();

	puts(checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
