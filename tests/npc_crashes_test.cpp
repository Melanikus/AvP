// The test for four crash fixes in the NPC code. Written 2026-11-06 by Melanikus for the AvP port. More were added since,
// each in its own Test... function below: the weapon a file names, and a character whose model isn't loaded (2026-11-07).
// Modified 2026-11-07 by Melanikus for the AvP port:
//   it also checks how a marine's glow is chosen when you play the alien
// Modified 2026-11-08 by Melanikus for the AvP port:
//   and that facehuggers and the queen glow in the alien colour;
//   and that an alien coming for its target checks it has one before it reads where it is;
//   and that a dormant predator that wakes is still kept until the level ends
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

// MODEL-NULL-1. A character's model comes from a file the level loads, and which files those are is worked out from
// the level's own lists. If the two don't agree, the model isn't there, and the old code made the character anyway:
// Create_HModel read through nothing. Bobblen's fork of this game, avp-forever, crashed that way, making civilians in
// skirmish (his commit 207704e), which is how this was found. Every place that makes a character from a level has to
// check, and before it checks, nothing the clean-up frees or stops may be left as whatever the memory last held.
struct ModelMaker
{
	char const *file;          // the source file
	char const *from;          // where the function that makes it starts
	char const *to;            // where the next function starts
	char const *check;         // its check, which has to come before its model is made
	char const *made;          // where its model is made
	char const *noneFirst[4];  // what has to be set to none before the check, so taking it out frees and stops nothing
	char const *says;
};

static void TestTheCharacterWithNoModel()
{
	puts("A character whose model isn't loaded:");

	std::string const types = ReadSourceFile("3dc/avp/bh_types.cpp");

	Check(Has(types, "if (root)\n\t{\n\t\treturn true;\n\t}"), "a model that's there passes");
	Check(Has(types, "LogString(std::string(\"npc: refused: \") + who + \" needs the model \" + modelName + \" from \" + fileName + \", which isn't loaded, so it isn't made\");\n\treturn false;"), "and one that isn't is refused, and the log names the model and its file");
	Check(Has(Between(types, "case I_BehaviourPlacedHierarchy:", "case I_BehaviourPlacedLight:"), "if (!sbptr->SBdataptr && !memoryInitialisationFailure)\n\t\t\t{\n\t\t\t\tRemoveBehaviourStrategy(sbptr);\n\t\t\t}"), "a placed hierarchy that comes back with nothing is taken out, because everything that works it reads its block");

	ModelMaker const makers[] =
	{
		{ "3dc/avp/bh_marine.cpp", "void InitMarineBehaviour(", "void InitSealBehaviour(",
			"if (!NpcModelIsLoaded(root_section, \"a marine the level places\", marineStatus->My_Weapon->Riffname, marineStatus->My_Weapon->HierarchyName))\n\t\t{\n\t\t\tRemoveBehaviourStrategy(sbPtr);\n\t\t\treturn;\n\t\t}",
			"Create_HModel(&marineStatus->HModelController,root_section);",
			{ "marineStatus->HModelController.section_data=NULL;", "marineStatus->HModelController.Deltas=NULL;", "marineStatus->myGunFlash = (DISPLAYBLOCK *)0;", "marineStatus->soundHandle = SOUND_NOACTIVEINDEX;" },
			"a marine the level places" },
		{ "3dc/avp/bh_marine.cpp", "void CreateMarineDynamic(", "void SetMarineElevation(",
			"if (!NpcModelIsLoaded(root_section, \"a marine or civilian a generator makes\", marineStatus->My_Weapon->Riffname, marineStatus->My_Weapon->HierarchyName))\n\t\t{\n\t\t\tRemoveBehaviourStrategy(sbPtr);\n\t\t\treturn;\n\t\t}",
			"Create_HModel(&marineStatus->HModelController,root_section);",
			{ "marineStatus->HModelController.section_data=NULL;", "marineStatus->HModelController.Deltas=NULL;", "marineStatus->myGunFlash = (DISPLAYBLOCK *)0;", "marineStatus->soundHandle = SOUND_NOACTIVEINDEX;" },
			"a marine or civilian a generator makes" },
		{ "3dc/avp/bh_alien.cpp", "void CreateAlienDynamic(", "void InitAlienBehaviour(",
			"if (!NpcModelIsLoaded(root_section, \"an alien a generator makes\", modelFile, modelName))\n\t\t\t{\n\t\t\t\tRemoveBehaviourStrategy(sbPtr);\n\t\t\t\treturn;\n\t\t\t}",
			"Create_HModel(&alienStatus->HModelController,root_section);",
			{ "alienStatus->HModelController.section_data=NULL;", "alienStatus->HModelController.Deltas=NULL;", 0, 0 },
			"an alien a generator makes" },
		{ "3dc/avp/bh_alien.cpp", "void InitAlienBehaviour(", "static BOOL PlayerIsDeadAndNoLivingNetghosts(",
			"if (!NpcModelIsLoaded(root_section, \"an alien the level places\", modelFile, modelName))\n\t\t\t{\n\t\t\t\tRemoveBehaviourStrategy(sbPtr);\n\t\t\t\treturn;\n\t\t\t}",
			"Create_HModel(&alienStatus->HModelController,root_section);",
			{ "alienStatus->HModelController.section_data=NULL;", "alienStatus->HModelController.Deltas=NULL;", 0, 0 },
			"an alien the level places" },
		{ "3dc/avp/bh_facehugger.cpp", "void InitFacehuggerBehaviour(", "void FacehuggerBehaviour(",
			"if (!NpcModelIsLoaded(root_section, \"a facehugger the level places\", \"hnpchugger\", \"Template\"))\n\t\t{\n\t\t\tRemoveBehaviourStrategy(sbPtr);\n\t\t\treturn;\n\t\t}",
			"Create_HModel(&facehuggerStatus->HModelController,root_section);",
			{ "facehuggerStatus->HModelController.section_data=NULL;", "facehuggerStatus->HModelController.Deltas=NULL;", "facehuggerStatus->soundHandle = SOUND_NOACTIVEINDEX;", "facehuggerStatus->soundHandle2 = SOUND_NOACTIVEINDEX;" },
			"a facehugger the level places" },
		{ "3dc/avp/bh_predator.cpp", "void InitPredatorBehaviour(", "void InitDormantPredatorBehaviour(",
			"if (!NpcModelIsLoaded(root_section, \"a predator the level places\", predatorStatus->Selected_Weapon->Riffname, predatorStatus->Selected_Weapon->HierarchyName))\n                {\n                        RemoveBehaviourStrategy(sbPtr);\n                        return;\n                }",
			"Create_HModel(&predatorStatus->HModelController,root_section);",
			{ "predatorStatus->HModelController.section_data=NULL;", "predatorStatus->HModelController.Deltas=NULL;", "predatorStatus->soundHandle = SOUND_NOACTIVEINDEX;", 0 },
			"a predator the level places" },
		{ "3dc/avp/bh_queen.cpp", "void InitQueenBehaviour(", "static BOOL QueenPlayingStunAnimation(",
			"if (!NpcModelIsLoaded(root_section, \"a queen the level places\", \"queen\", \"Template\"))\n\t\t{\n\t\t\tRemoveBehaviourStrategy(sbPtr);\n\t\t\treturn;\n\t\t}",
			"Create_HModel(&queenStatus->HModelController,root_section);",
			{ "queenStatus->HModelController.section_data=NULL;", "queenStatus->HModelController.Deltas=NULL;", "queenStatus->soundHandle=SOUND_NOACTIVEINDEX;", 0 },
			"a queen the level places" },
		{ "3dc/avp/bh_xeno.cpp", "void InitXenoborgBehaviour(", "void XenoborgBehaviour(",
			"if (!NpcModelIsLoaded(root_section, \"a xenoborg the level places\", \"hnpc_xenoborg\", \"xenobasic\"))\n\t\t{\n\t\t\tRemoveBehaviourStrategy(sbPtr);\n\t\t\treturn;\n\t\t}",
			"Create_HModel(&xenoStatus->HModelController,root_section);",
			{ "xenoStatus->HModelController.section_data=NULL;", "xenoStatus->HModelController.Deltas=NULL;", "xenoStatus->soundHandle1=SOUND_NOACTIVEINDEX;", "xenoStatus->torso_whirr=SOUND_NOACTIVEINDEX;" },
			"a xenoborg the level places" },
		{ "3dc/avp/bh_placed_hierarchy.cpp", "void* PlacedHierarchyBehaveInit(", "void MakePlacedHierarchyNear(",
			"if (!NpcModelIsLoaded(root_section, \"a placed hierarchy the level names\", ph_tt->file_name, ph_tt->hier_name))\n\t{\n\t\tDeallocateMem(ph_bhv);\n\t\treturn ((void *)NULL);\n\t}",
			"Create_HModel(&ph_bhv->HModelController,root_section);",
			{ 0, 0, 0, 0 },
			"a placed hierarchy whose file and model the level names" },
	};

	for (ModelMaker const &maker : makers)
	{
		std::string const made = Between(ReadSourceFile(maker.file), maker.from, maker.to);
		bool noneFirst = true;

		for (char const *line : maker.noneFirst)
		{
			if (line && !ComesBefore(made, line, maker.check))
			{
				noneFirst = false;
			}
		}

		Check(Has(made, maker.made) && ComesBefore(made, maker.check, maker.made), std::string(maker.says) + ": refused before its model is made");
		Check(noneFirst, std::string(maker.says) + ": and nothing its clean-up frees or stops is left as old memory");
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

// When you play the alien, every body glows in a colour that says what it is (DoAlienEnergyView in kshape.cpp). The
// marine's part of that choice used to carry on into the part for another player's stand-in, and read the marine's
// details as if they were a stand-in's. With today's numbers that happened to change nothing, which is luck, not a
// rule. Only a running game with a marine in view reaches these lines, so this reads the source.
static void TestTheMarineSeenByAnAlien()
{
	puts("A marine's glow when you play the alien:");

	std::string const glow = Between(ReadSourceFile("3dc/kshape.cpp"), "void DoAlienEnergyView(DISPLAYBLOCK *dispPtr)\n{", "case I_BehaviourNetCorpse: {");
	std::string const marine = Between(glow, "case I_BehaviourSeal: {", "case I_BehaviourNetGhost: {");
	std::string const colour = "colour = MARINES_LIFEFORCE_GLOW_COLOUR;";
	std::string::size_type const colourAt = marine.find(colour);

	Check(colourAt != std::string::npos, "the marine's part still gives him the marine colour");
	Check(colourAt != std::string::npos && marine.find("break;", colourAt) != std::string::npos,
	      "and stops there, so his details aren't read as another player's stand-in's");
}

// The same choice for the other aliens. The facehugger and the queen had no line in it, so they glowed in the colour every body
// starts with, the marine's; the user's ruling, 2026-11-07, is that they get the alien colour. That holds for one another machine
// runs in a network game, and for a part blown off either, the way it already did for the alien. The predator's own view that
// picks out aliens has always counted both as aliens (kshape.cpp, VISION_MODE_PRED_SEEALIENS). Found through Bobblen's
// avp-forever (commit 12e3cd1), rewritten. Only a running game with one in view reaches these lines, so this reads the source.
static void TestTheAliensSeenByAnAlien()
{
	puts("A facehugger's and the queen's glow when you play the alien:");

	std::string const choice = Between(ReadSourceFile("3dc/kshape.cpp"), "void DoAlienEnergyView(DISPLAYBLOCK *dispPtr)\n{", "ProveHModel(controllerPtr, dispPtr);");
	std::string const colour = "colour = ALIENS_LIFEFORCE_GLOW_COLOUR;";

	// Each part runs from where it starts up to the first line that gives the alien colour, so what's in it leads there
	std::string const creatures = Between(choice, "case I_BehaviourAlien:", "break;");
	std::string const standIns = Between(choice, "case I_BehaviourNetGhost: {", colour);
	std::string const parts = Between(choice, "case I_BehaviourHierarchicalFragment: {", colour);

	Check(!choice.empty() && Has(creatures, colour), "the choice can be read, and an alien still gets the alien colour");
	Check(Has(creatures, "case I_BehaviourFaceHugger:"), "so does a facehugger");
	Check(Has(creatures, "case I_BehaviourQueenAlien:"), "and the queen");
	Check(Has(standIns, "ghostDataPtr->type == I_BehaviourFaceHugger"), "and a facehugger another machine runs");
	Check(Has(standIns, "ghostDataPtr->type == I_BehaviourQueenAlien"), "and a queen another machine runs");
	Check(Has(parts, "debrisDataPtr->Type == I_BehaviourFaceHugger"), "and a part blown off a facehugger");
	Check(Has(parts, "debrisDataPtr->Type == I_BehaviourQueenAlien"), "and a part blown off the queen");
}

// A ghost's data block is new memory: it's cleared so no field holds what was there before, and that has to wait until the block is
// known to be there, or clearing it is the first thing to go wrong
static void TestTheGhostJustMade()
{
	puts("A ghost that has just been made:");

	std::string const ghosts = ReadSourceFile("3dc/avp/win95/pldghost.cpp");
	std::string const made = "NETGHOSTDATABLOCK *ghostData = (NETGHOSTDATABLOCK*)AllocateMem(sizeof(NETGHOSTDATABLOCK));";

	Check(Has(ghosts, made + "\n\t\tif(!ghostData)\n"), "its block is checked as soon as it's asked for");
	Check(ComesBefore(ghosts, made + "\n\t\tif(!ghostData)\n", "memset(ghostData, 0, sizeof(NETGHOSTDATABLOCK));"), "and only then cleared");
}

// An alien coming for its target (AlienNearState_Approach) can find it has none, and then it goes hunting. It used to measure how far
// away the target was first, through the empty pointer, which crashed the game. Only a running game with an alien that has lost its
// target reaches these lines, so this reads the source.
static void TestTheAlienWithNoTarget()
{
	puts("An alien coming for a target it no longer has:");

	std::string const approach = Between(ReadSourceFile("3dc/avp/bh_near.cpp"), "static void AlienNearState_Approach(STRATEGYBLOCK *sbPtr)\n{", "/* Update sequence! */");
	std::string const check = "if (alienStatusPointer->Target==NULL) {";
	std::string const read = "alienStatusPointer->Target->DynPtr";

	Check(Has(approach, check), "it checks it has a target");
	Check(Has(approach, read), "and measures how far away it is, at the start");
	Check(ComesBefore(approach, check, read), "but only after the check, so an alien with none never reads through the empty pointer");
}

// Everything placed in a level is marked to be kept until the level ends, because switches, tracks and other things in the level keep
// hold of it, even after it's gone. A dormant predator that wakes has its block started afresh, which clears that mark, so it has to be
// put back. Without it, once the predator's body has gone its block is handed to the next thing made, while the switches still hold it.
static void TestTheWokenPredator()
{
	puts("A dormant predator that wakes:");

	std::string const setup = ReadSourceFile("3dc/avp/win95/object_setup.cpp");
	std::string const blocks = ReadSourceFile("3dc/avp/strategy_def.cpp");
	std::string const predator = ReadSourceFile("3dc/avp/bh_predator.cpp");

	std::string const placed = Between(setup, "void create_strategies_from_list ()", "\n}\n");
	std::string const destroyed = Between(blocks, "int DestroyActiveStrategyBlock(STRATEGYBLOCK* sb)", "\n}\n");
	std::string const afresh = Between(blocks, "void InitialiseSBValues(STRATEGYBLOCK* sptr)", "\n}\n");
	std::string const wake = Between(predator, "void ActivateDormantPredator(STRATEGYBLOCK* sbPtr)", "\n}\n");

	Check(Has(placed, "\t\tsbPtr->SBflags.preserve_until_end_of_level=1;"), "everything placed in a level is marked to be kept until it ends");
	Check(Has(destroyed, "\t\t\t\tif(!sb->SBflags.preserve_until_end_of_level)\n\t\t\t\t{\n\t\t\t\t\tDeallocateStrategyBlock(sb);"),
		"a block with the mark isn't handed back for something else when it's gone");
	Check(Has(afresh, "\tsptr->SBflags.preserve_until_end_of_level = 0;"), "starting a block afresh clears the mark, which is why the predator puts it back");
	Check(ComesBefore(wake, "bool const keptUntilTheLevelEnds = sbPtr->SBflags.preserve_until_end_of_level != 0;", "InitialiseSBValues(sbPtr);"),
		"the woken predator's mark is read before its block starts afresh");
	Check(ComesBefore(wake, "InitialiseSBValues(sbPtr);\n        sbPtr->SBflags.preserve_until_end_of_level = keptUntilTheLevelEnds ? 1 : 0;\n",
		"EnableBehaviourType(sbPtr,I_BehaviourPredator ,&toolsData );"), "and put back straight after, before it becomes a predator");
}

int main()
{
	TestTheXenoborg();
	TestTheXenoborgJustMade();
	TestTheSentryGunJustMade();
	TestTheHalfMadeNpc();
	TestTheWeaponFromAFile();
	TestTheCharacterWithNoModel();
	TestTheGhostJustMade();
	TestTheEmptyModule();
	TestTheDeadQueen();
	TestTheQueenWithNoRoomToBeDrawn();
	TestTheMarineSeenByAnAlien();
	TestTheAliensSeenByAnAlien();
	TestTheAlienWithNoTarget();
	TestTheWokenPredator();

	puts(checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
