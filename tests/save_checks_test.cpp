// A test of the checks on what a save file says.
// Written 2026-11-04 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// 3dc/avp/save_checks.cpp holds the checks that apply in many places. This test hands them made-up numbers, good and bad, and looks at what
// comes back. It reads no save file: there are none in this repository.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this file and 3dc/avp/save_checks.cpp, with the settings in tests/save_checks_test.args. The program prints a line
// for each check, and gives back a non-zero result if any check failed.

#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "3dc.h"
#include "dynblock.h"
#include "gamedef.h"
#include "strategy_def.h"
#include "env_info.h"
#include "equipment.h"
#include "vision.h"
#include "save_checks.h"

static int checksFailed = 0;

static void Check(bool passed, const char *what)
{
	printf("%s: %s\n", passed ? "ok    " : "FAILED", what);
	if (!passed)
	{
		checksFailed++;
	}
}

static void TestSlotNumbers()
{
	Check(Save_SlotNumbersAreValid(I_Marine, 0, I_Easy), "a marine's first level on easy");
	Check(Save_SlotNumbersAreValid(I_Predator, 0, I_Easy), "a predator's first level on easy");
	Check(Save_SlotNumbersAreValid(I_Alien, 0, I_Easy), "an alien's first level on easy");
	Check(Save_SlotNumbersAreValid(I_Marine, MAX_NO_OF_MARINE_EPISODES - 1, I_Impossible), "a marine's last level on impossible");
	Check(Save_SlotNumbersAreValid(I_Predator, MAX_NO_OF_PREDATOR_EPISODES - 1, I_Impossible), "a predator's last level on impossible");
	Check(Save_SlotNumbersAreValid(I_Alien, MAX_NO_OF_ALIEN_EPISODES - 1, I_Impossible), "an alien's last level on impossible");

	Check(!Save_SlotNumbersAreValid(3, 0, I_Easy), "no fourth species");
	Check(!Save_SlotNumbersAreValid(255, 0, I_Easy), "no species 255");
	Check(!Save_SlotNumbersAreValid(-1, 0, I_Easy), "no negative species");
	Check(!Save_SlotNumbersAreValid(I_Marine, MAX_NO_OF_MARINE_EPISODES, I_Easy), "one marine level past the end");
	Check(!Save_SlotNumbersAreValid(I_Predator, MAX_NO_OF_PREDATOR_EPISODES, I_Easy), "one predator level past the end");
	Check(!Save_SlotNumbersAreValid(I_Alien, MAX_NO_OF_ALIEN_EPISODES, I_Easy), "one alien level past the end");
	Check(!Save_SlotNumbersAreValid(I_Marine, -1, I_Easy), "no negative level");
	Check(!Save_SlotNumbersAreValid(I_Marine, 0, I_MaxDifficulties), "one difficulty past the end");
	Check(!Save_SlotNumbersAreValid(I_Marine, 0, -1), "no negative difficulty");
}

static void TestTableNumbers()
{
	Check(Save_WeaponSlotIsValid(WEAPON_SLOT_1), "the first weapon slot");
	Check(Save_WeaponSlotIsValid(MAX_NO_OF_WEAPON_SLOTS - 1), "the last weapon slot");
	Check(!Save_WeaponSlotIsValid(MAX_NO_OF_WEAPON_SLOTS), "one weapon slot past the end");
	Check(!Save_WeaponSlotIsValid(-1), "no negative weapon slot");

	Check(Save_WeaponStateIsValid(WEAPONSTATE_IDLE), "the first weapon state");
	Check(Save_WeaponStateIsValid(MAX_NO_OF_WEAPON_STATES - 1), "the last weapon state");
	Check(!Save_WeaponStateIsValid(MAX_NO_OF_WEAPON_STATES), "one weapon state past the end");
	Check(!Save_WeaponStateIsValid(-1), "no negative weapon state");

	Check(Save_VisionModeIsValid(VISION_MODE_NORMAL), "the first vision mode");
	Check(Save_VisionModeIsValid(NUMBER_OF_VISION_MODES - 1), "the last vision mode");
	Check(!Save_VisionModeIsValid(NUMBER_OF_VISION_MODES), "one vision mode past the end");
	Check(!Save_VisionModeIsValid(-1), "no negative vision mode");

	Check(Save_AmmoIsValid(0), "the first kind of ammunition");
	Check(Save_AmmoIsValid(AMMO_GRENADE), "the grenade launcher's first kind");
	Check(Save_AmmoIsValid(MAX_NO_OF_AMMO_TEMPLATES - 1), "the last kind of ammunition");
	Check(!Save_AmmoIsValid(MAX_NO_OF_AMMO_TEMPLATES), "one kind of ammunition past the end");
	Check(!Save_AmmoIsValid(AMMO_NONE), "no ammunition at all isn't a choice");
}

static void TestWeaponSwaps()
{
	Check(Save_SwapFitsWeaponState(WEAPONSTATE_UNREADYING, WEAPON_SLOT_1), "a weapon being put away, with a slot to go to");
	Check(Save_SwapFitsWeaponState(WEAPONSTATE_IDLE, WEAPON_FINISHED_SWAPPING), "a weapon in the hands, with no swap going on");
	Check(!Save_SwapFitsWeaponState(WEAPONSTATE_UNREADYING, WEAPON_FINISHED_SWAPPING), "a weapon being put away with no swap going on is refused");
}

static void TestListPlaces()
{
	Check(Save_PlaceInListOrNone(0, 3) == 0, "the first place in a list");
	Check(Save_PlaceInListOrNone(2, 3) == 2, "the last place in a list");
	Check(Save_PlaceInListOrNone(-1, 3) == -1, "none stays none");
	Check(Save_PlaceInListOrNone(3, 3) == -1, "one place past the end is none");
	Check(Save_PlaceInListOrNone(-2, 3) == -1, "a negative place is none");
	Check(Save_PlaceInListOrNone(0, 0) == -1, "an empty list has no first place");
	Check(Save_PlaceInListOrNone(0, -1) == -1, "a list that was never filled has no first place");
}

static void TestNames()
{
	char const name[] = "sound.wav";
	int const length = (int)sizeof(name);
	int const huge = 2147483647, negative = -1, zero = 0;
	char const noEnd[4] = { 'a', 'b', 'c', 'd' };
	int const four = 4;

	Check(Save_NameFillsBlock(20 + length, 20, &length, name), "a name that fills the block, end mark and all");
	Check(!Save_NameFillsBlock(19, 20, &length, name), "a block too small to hold the length isn't read");
	Check(!Save_NameFillsBlock(8, 20, 0, name), "a bare header isn't read at all");
	Check(!Save_NameFillsBlock(20 + length + 1, 20, &length, name), "a block with room left over");
	Check(!Save_NameFillsBlock(20 + length - 1, 20, &length, name), "a name longer than the block");
	// the byte before an empty name is an end mark, so a check that looked there would be fooled
	char const before[2] = { 0, 'a' };
	Check(!Save_NameFillsBlock(20, 20, &zero, &before[1]), "a name with nothing in it, not even its end mark");
	Check(!Save_NameFillsBlock(20, 20, &negative, name), "a negative length");
	Check(!Save_NameFillsBlock(20, 20, &huge, name), "a huge length");
	Check(!Save_NameFillsBlock(24, 20, &four, noEnd), "a name with no end mark");
}

static void TestMorphFrames()
{
	Check(Save_MorphFrameOrOwn(0, 7, 65536) == 0, "the first frame of a morph");
	Check(Save_MorphFrameOrOwn(65535, 7, 65536) == 65535, "the very end of the last frame");
	Check(Save_MorphFrameOrOwn(65536, 7, 65536) == 7, "one past the end keeps the door's own");
	Check(Save_MorphFrameOrOwn(-1, 7, 65536) == 7, "a negative frame keeps the door's own");
	Check(Save_MorphFrameOrOwn(2147483647, 7, 65536) == 7, "a huge frame keeps the door's own");
}

static void TestTextureSequences()
{
	// one entry of working space, two sequences and the empty entry at the end, then something that must never be reached
	int a = 1, b = 2, c = 3;
	void *list[5] = { 0, &a, &b, 0, &c };
	int const *animation = (int const *)list;

	Check(Save_TextureSequenceIsValid(animation, 0), "the first sequence");
	Check(Save_TextureSequenceIsValid(animation, 1), "the last sequence");
	Check(!Save_TextureSequenceIsValid(animation, 2), "one sequence past the end");
	Check(!Save_TextureSequenceIsValid(animation, 3), "a sequence beyond the empty entry");
	Check(!Save_TextureSequenceIsValid(animation, -1), "no negative sequence, which would be the working space");
	Check(!Save_TextureSequenceIsValid(0, 0), "no list at all");
}

static void TestRanges()
{
	Check(Save_NumberIsInRange(1, 1, 255), "the lowest number in a range");
	Check(Save_NumberIsInRange(255, 1, 255), "the highest number in a range");
	Check(!Save_NumberIsInRange(0, 1, 255), "one below a range");
	Check(!Save_NumberIsInRange(256, 1, 255), "one above a range");
}

static void MakeLive(DYNAMICSBLOCK &live)
{
	memset(&live, 0, sizeof(live));
	live.Mass = 100;
	live.DynamicsType = DYN_TYPE_SPHERE_COLLISIONS;
	live.ToppleForce = TOPPLE_FORCE_ALIEN;
}

static void TestDynamics()
{
	DYNAMICSBLOCK live, saved;

	// a real save
	MakeLive(live);
	memset(&saved, 0, sizeof(saved));
	saved.Position.vx = 1234;
	saved.Position.vy = -5678;
	saved.LinVelocity.vz = 99;
	saved.Mass = 30;
	saved.DynamicsType = DYN_TYPE_NRBB_COLLISIONS;
	saved.ToppleForce = TOPPLE_FORCE_FULL;
	saved.OrientEuler.EulerY = 4095;
	saved.GravityOn = 1;
	Save_RestoreDynamics(&live, &saved, kSaveHoldsWallsAndGravityOff);
	Check(live.Position.vx == 1234 && live.Position.vy == -5678 && live.LinVelocity.vz == 99, "where it is and how it moves come from the save");
	Check(live.Mass == 30, "a real mass comes from the save");
	Check(live.DynamicsType == DYN_TYPE_NRBB_COLLISIONS, "a real collision type comes from the save");
	Check(live.ToppleForce == TOPPLE_FORCE_FULL, "a real topple force comes from the save");
	Check(live.OrientEuler.EulerY == 4095, "an angle in range stays as it is");
	Check(live.GravityOn == 1, "the switches come from the save");

	// the first and last of each choice are real
	MakeLive(live);
	saved.DynamicsType = DYN_TYPE_NO_COLLISIONS;
	saved.ToppleForce = TOPPLE_FORCE_NONE;
	Save_RestoreDynamics(&live, &saved, kSaveHoldsWallsAndGravityOff);
	Check(live.DynamicsType == DYN_TYPE_NO_COLLISIONS, "the first collision type is real");
	Check(live.ToppleForce == TOPPLE_FORCE_NONE, "the first topple force is real");

	MakeLive(live);
	saved.DynamicsType = DYN_TYPE_CUBOID_COLLISIONS;
	Save_RestoreDynamics(&live, &saved, kSaveHoldsWallsAndGravityOff);
	Check(live.DynamicsType == DYN_TYPE_CUBOID_COLLISIONS, "the last collision type is real");

	// a crafted save
	MakeLive(live);
	memset(&saved, 0xFF, sizeof(saved));
	saved.Position.vx = 42;
	Save_RestoreDynamics(&live, &saved, kSaveHoldsWallsAndGravityOff);
	Check(live.CollisionReportPtr == 0, "an address in the save never gets in");
	Check(live.Mass == 100, "a mass of -1 keeps the object's own");
	Check(live.DynamicsType == DYN_TYPE_SPHERE_COLLISIONS, "a collision type of -1 keeps the object's own");
	Check(live.ToppleForce == TOPPLE_FORCE_ALIEN, "a topple force of -1 keeps the object's own");
	Check(live.Position.vx == 42, "the rest of a bad block still comes from the save");

	MakeLive(live);
	memset(&saved, 0, sizeof(saved));
	saved.Mass = 0;
	saved.DynamicsType = (enum DYN_TYPE)(DYN_TYPE_CUBOID_COLLISIONS + 1);
	saved.ToppleForce = (enum TOPPLE_FORCE)(TOPPLE_FORCE_FULL + 1);
	Save_RestoreDynamics(&live, &saved, kSaveHoldsWallsAndGravityOff);
	Check(live.Mass == 100, "a mass of 0 keeps the object's own");
	Check(live.DynamicsType == DYN_TYPE_SPHERE_COLLISIONS, "one collision type past the end keeps the object's own");
	Check(live.ToppleForce == TOPPLE_FORCE_ALIEN, "one topple force past the end keeps the object's own");

	MakeLive(live);
	memset(&saved, 0, sizeof(saved));
	saved.Mass = 30;
	saved.OrientEuler.EulerX = 4096;
	saved.OrientEuler.EulerY = -1;
	saved.OrientEuler.EulerZ = 4096 * 1000 + 7;
	saved.PrevOrientEuler.EulerX = -4096;
	saved.PrevOrientEuler.EulerY = 2147483647;
	saved.PrevOrientEuler.EulerZ = 5000;
	Save_RestoreDynamics(&live, &saved, kSaveHoldsWallsAndGravityOff);
	Check(live.OrientEuler.EulerX == 0, "an angle of 4096 is the same as 0");
	Check(live.OrientEuler.EulerY == 4095, "an angle of -1 is the same as 4095");
	Check(live.OrientEuler.EulerZ == 7, "an angle many turns round comes back in range");
	Check(live.PrevOrientEuler.EulerX == 0, "the last frame's angle too");
	Check(live.PrevOrientEuler.EulerY == 4095, "the last frame's huge angle comes back in range");
	Check(live.PrevOrientEuler.EulerZ == 904, "the last frame's third angle too");

	// nothing to write to
	Save_RestoreDynamics(0, &saved, kSaveHoldsWallsAndGravityOff);
	Check(true, "an object with no dynamics block is left alone");
}

// THE WALLS AND GRAVITY A SAVE CAN HOLD (save_checks.h). Added 2026-11-11 by Melanikus (Third) for the AvP port (#109, SAVE-HOSTILE-1): a
// save taken while an alien slid holds its walls and gravity off, and a save made to can hold either off for anything. Each kind keeps
// only what its own code can turn off, and the rest comes back as the object was made, counted as refused.
static void CheckRefusedBy(const char *check, int count, const char *what);
static void CheckNothingRefused(const char *what);

static void TestWallsAndGravity()
{
	DYNAMICSBLOCK live, saved;
	char words[200] = { 0 };

	// Made as 1999 makes an alien: the level's walls stop it, and gravity pulls it
	MakeLive(live);
	live.OnlyCollideWithObjects = 0;
	live.GravityOn = 1;
	memset(&saved, 0, sizeof(saved));
	saved.Mass = 30;
	saved.DynamicsType = DYN_TYPE_NRBB_COLLISIONS;
	saved.ToppleForce = TOPPLE_FORCE_FULL;
	saved.Position.vx = 777;
	saved.OnlyCollideWithObjects = 1;
	saved.GravityOn = 0;
	Save_StartCountingRefusals();

	// An alien saved mid-slide (or a save made so): it comes back with the level's walls and its gravity, where it was, and both are refused
	Save_RestoreDynamics(&live, &saved, kSaveHoldsNeitherOff);
	Check(live.OnlyCollideWithObjects == 0 && live.GravityOn == 1 && live.Position.vx == 777,
		"an alien saved with the level's walls and its gravity off comes back with both, where it was");
	Save_DescribeRefusals(words, sizeof(words));
	Check(strcmp(words, "save: 2 values refused, by the level's walls for an object 1, an object's gravity 1") == 0, "and both are counted as refused");
	Save_StartCountingRefusals();

	// The same save of a thrown flare: its gravity is as saved, as its own code turns it off, but never its walls
	MakeLive(live);
	live.GravityOn = 1;
	Save_RestoreDynamics(&live, &saved, kSaveHoldsGravityOff);
	Check(live.OnlyCollideWithObjects == 0 && live.GravityOn == 0, "a kind that turns its own gravity off keeps the gravity saved, but not the walls off");
	CheckRefusedBy("the level's walls for an object", 1, "and only the walls are refused");

	// The queen, climbing out of her pit: both as saved, nothing refused
	MakeLive(live);
	live.GravityOn = 1;
	Save_RestoreDynamics(&live, &saved, kSaveHoldsWallsAndGravityOff);
	Check(live.OnlyCollideWithObjects == 1 && live.GravityOn == 0, "the queen's walls and gravity are as saved, as she turns both off");
	CheckNothingRefused("and nothing is refused");

	// An alien's real save, walls and gravity on: as saved, nothing refused
	MakeLive(live);
	live.GravityOn = 1;
	saved.OnlyCollideWithObjects = 0;
	saved.GravityOn = 1;
	Save_RestoreDynamics(&live, &saved, kSaveHoldsNeitherOff);
	Check(live.OnlyCollideWithObjects == 0 && live.GravityOn == 1, "an alien saved as 1999 has it loads as it was");
	CheckNothingRefused("and nothing is refused");

	// Gravity alone off, for an alien: refused alone
	MakeLive(live);
	live.GravityOn = 1;
	saved.GravityOn = 0;
	Save_RestoreDynamics(&live, &saved, kSaveHoldsNeitherOff);
	Check(live.OnlyCollideWithObjects == 0 && live.GravityOn == 1, "an alien saved with only its gravity off has it back");
	CheckRefusedBy("an object's gravity", 1, "and only the gravity is refused");
}

// A good value has to count nothing, and a bad one has to count against the check that refused it and no other. If only the total were looked at, a later check could hide a missing one. The
// log's line names each check that refused something and how many times, so the whole line has to be just that one check, that many times.
static void CheckRefusedBy(const char *check, int count, const char *what)
{
	char words[200] = { 0 };

	Save_DescribeRefusals(words, sizeof(words));

	char wanted[120] = { 0 };

	snprintf(wanted, sizeof(wanted), "save: %d values refused, by %s %d", count, check, count);
	Check(strcmp(words, wanted) == 0, what);
	Save_StartCountingRefusals();
}

static void CheckNothingRefused(const char *what)
{
	char words[200] = { 0 };

	Save_DescribeRefusals(words, sizeof(words));
	Check(strcmp(words, "save: no values refused") == 0, what);
	Save_StartCountingRefusals();
}

static void TestCountingRefusals()
{
	Save_StartCountingRefusals();

	// everything a real save holds
	Save_WeaponSlotIsValid(0);
	Save_WeaponStateIsValid(0);
	Save_VisionModeIsValid(0);
	Save_AmmoIsValid(0);
	Save_SwapFitsWeaponState(WEAPONSTATE_UNREADYING, WEAPON_SLOT_1);
	Save_SwapFitsWeaponState(WEAPONSTATE_IDLE, WEAPON_FINISHED_SWAPPING);
	Save_PlaceInListOrNone(0, 3);
	Save_PlaceInListOrNone(-1, 3);
	Save_MorphFrameOrOwn(5, 1, 10);
	Save_NumberIsInRange(5, 1, 10);

	int const length = 4;
	char const goodName[4] = { 'a', 'b', 'c', 0 };

	Save_NameFillsBlock(14, 10, &length, goodName);

	void *const sequences[3] = { 0, (void *)goodName, 0 };

	Save_TextureSequenceIsValid((const int *)sequences, 0);

	DYNAMICSBLOCK live, saved;
	MakeLive(live);
	memset(&saved, 0, sizeof(saved));
	saved.Mass = 30;
	saved.DynamicsType = DYN_TYPE_NRBB_COLLISIONS;
	saved.ToppleForce = TOPPLE_FORCE_FULL;
	Save_RestoreDynamics(&live, &saved, kSaveHoldsWallsAndGravityOff);
	CheckNothingRefused("everything a real save holds counts nothing, and -1 for none is one of those");

	// each bad value, named
	Save_WeaponSlotIsValid(-1);
	CheckRefusedBy("a weapon slot", 1, "a bad weapon slot is counted against that check");
	Save_WeaponStateIsValid(MAX_NO_OF_WEAPON_STATES);
	CheckRefusedBy("a weapon's state", 1, "a bad weapon state");
	Save_VisionModeIsValid(NUMBER_OF_VISION_MODES);
	CheckRefusedBy("a vision mode", 1, "a bad vision mode");
	Save_AmmoIsValid(MAX_NO_OF_AMMO_TEMPLATES);
	CheckRefusedBy("a kind of ammunition", 1, "a bad kind of ammunition");
	Save_SwapFitsWeaponState(WEAPONSTATE_UNREADYING, WEAPON_FINISHED_SWAPPING);
	CheckRefusedBy("a weapon being put away with no slot to go to", 1, "a weapon being put away with no swap going on");
	Save_PlaceInListOrNone(3, 3);
	CheckRefusedBy("a place in a list", 1, "a place past the end of its list");
	Save_PlaceInListOrNone(-2, 3);
	CheckRefusedBy("a place in a list", 1, "a place below -1");
	Save_MorphFrameOrOwn(10, 1, 10);
	CheckRefusedBy("a morph's frame", 1, "a bad morph frame");
	Save_NumberIsInRange(11, 1, 10);
	CheckRefusedBy("a number out of its range", 1, "a number out of its range");
	Save_NameFillsBlock(9, 10, &length, goodName);
	CheckRefusedBy("a name in its block", 1, "a block too small for its name");
	Save_NameFillsBlock(15, 10, &length, goodName);
	CheckRefusedBy("a name in its block", 1, "a name that doesn't fill its block");
	Save_TextureSequenceIsValid(0, 0);
	CheckRefusedBy("a texture animation's sequence", 1, "a texture animation with no list");
	Save_TextureSequenceIsValid((const int *)sequences, 1);
	CheckRefusedBy("a texture animation's sequence", 1, "a sequence past the end of its list");

	MakeLive(live);
	memset(&saved, 0, sizeof(saved));
	saved.Mass = 0;
	saved.DynamicsType = DYN_TYPE_NRBB_COLLISIONS;
	saved.ToppleForce = TOPPLE_FORCE_FULL;
	Save_RestoreDynamics(&live, &saved, kSaveHoldsWallsAndGravityOff);
	CheckRefusedBy("a mass", 1, "a mass of nothing");

	MakeLive(live);
	saved.Mass = 30;
	saved.DynamicsType = (enum DYN_TYPE)9999;
	Save_RestoreDynamics(&live, &saved, kSaveHoldsWallsAndGravityOff);
	CheckRefusedBy("a kind of dynamics", 1, "a kind of dynamics that isn't one");

	MakeLive(live);
	saved.DynamicsType = DYN_TYPE_NRBB_COLLISIONS;
	saved.ToppleForce = (enum TOPPLE_FORCE)9999;
	Save_RestoreDynamics(&live, &saved, kSaveHoldsWallsAndGravityOff);
	CheckRefusedBy("a topple force", 1, "a topple force that isn't one");

	MakeLive(live);
	saved.ToppleForce = TOPPLE_FORCE_FULL;
	saved.CollisionReportPtr = (struct collisionreport *)&live;
	Save_RestoreDynamics(&live, &saved, kSaveHoldsWallsAndGravityOff);
	CheckRefusedBy("an address in a dynamics block", 1, "an address where a real save holds nothing");

	// several kinds in one line, and a line too small for them
	Save_WeaponSlotIsValid(-1);
	Save_WeaponSlotIsValid(-1);
	Save_VisionModeIsValid(-1);

	char words[200] = { 0 };

	Save_DescribeRefusals(words, sizeof(words));
	Check(strcmp(words, "save: 3 values refused, by a weapon slot 2, a vision mode 1") == 0, "the line adds up, and says how many each check refused");

	char tooSmall[20];

	memset(tooSmall, 'x', sizeof(tooSmall));
	Save_DescribeRefusals(tooSmall, 12);
	Check(tooSmall[11] == 0 && tooSmall[12] == 'x', "a line that doesn't fit is cut short, and writes nothing past its room");
	Save_StartCountingRefusals();

	// more kinds than there are places: the count stays right, and nothing goes past the end
	static char names[20][8];

	for (int i = 0; i < 20; i++)
	{
		snprintf(names[i], sizeof(names[i]), "kind%d", i);
		Save_NoteRefused(names[i]);
	}

	Save_DescribeRefusals(words, sizeof(words));
	Check(strncmp(words, "save: 20 values refused, by kind0 1", 35) == 0 && strstr(words, ", others 4") != 0, "more kinds than places still adds up");
	Save_StartCountingRefusals();
}

// A target found by a name from the save is kept only with a place in the world
static void TestTargets()
{
	STRATEGYBLOCK withPlace, withoutPlace;
	DYNAMICSBLOCK dynamics;
	memset(&withPlace, 0, sizeof(withPlace));
	memset(&withoutPlace, 0, sizeof(withoutPlace));
	memset(&dynamics, 0, sizeof(dynamics));
	withPlace.DynPtr = &dynamics;

	Save_StartCountingRefusals();
	Check(Save_TargetWithAPlace(&withPlace) == &withPlace, "a target with a place in the world is kept");
	Check(Save_TargetWithAPlace(0) == 0, "no target stays no target");
	CheckNothingRefused("and neither counts as refused");
	Check(Save_TargetWithAPlace(&withoutPlace) == 0, "a target with no place in the world, such as a placed sound, isn't kept");
	CheckRefusedBy("a target with no place in the world", 1, "and it counts as refused");
	Save_StartCountingRefusals();
}

// Reads a file of the game as text. /FC in this test's settings makes __FILE__ the whole path of this file, so the repository's top is found from it.
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

static int CountOf(std::string const &text, char const *words)
{
	int count = 0;

	for (std::string::size_type at = text.find(words); at != std::string::npos; at = text.find(words, at + 1))
	{
		count++;
	}

	return count;
}

// The loaders bring most of the game with them, so these read them as text. That shows each one asks. What happens in a real load needs a real save, and Hudson's saveat= and loadat= are for that.
static void TestTheLoadersAsk()
{
	std::string const avp = "3dc/avp/";
	char const *const usingDynamics[] = { "bh_agun", "bh_alien", "bh_corpse", "bh_debris", "bh_facehugger", "bh_marine", "bh_platform_lift", "bh_predator", "bh_queen", "bh_weapon", "bh_xeno", "bonusabilities", "player", "pvisible", "weapon_m79" };

	for (char const *const name : usingDynamics)
	{
		std::string const text = ReadSourceFile((avp + name + ".cpp").c_str());

		Check(Has(text, "Save_RestoreDynamics("), (std::string("a saved dynamics block goes through the check in ") + name).c_str());
		Check(!Has(text, "*sbPtr->DynPtr = block->dynamics"), (std::string("and nothing copies one straight over in ") + name).c_str());
	}

	Check(CountOf(ReadSourceFile("3dc/avp/bh_weapon.cpp"), "Save_RestoreDynamics(") == 13, "every thrown weapon's loader asks");
	Check(CountOf(ReadSourceFile("3dc/avp/bh_debris.cpp"), "Save_RestoreDynamics(") == 2, "and both of debris'");

	// THE WALLS AND GRAVITY A SAVE CAN HOLD (save_checks.h; added 2026-11-11 by Melanikus (Third) for the AvP port, #109, SAVE-HOSTILE-1):
	// each loader says what its kind's own code turns off. Only the queen can be saved with the level's walls off.
	struct Holds { const char *name; const char *says; int count; };
	Holds const holds[] = {
		{ "bh_agun", ", &block->dynamics, kSaveHoldsNeitherOff);", 1 }, { "bh_alien", ", &block->dynamics, kSaveHoldsNeitherOff);", 1 },
		{ "bh_corpse", ", &block->dynamics, kSaveHoldsNeitherOff);", 1 }, { "bh_debris", ", &block->dynamics, kSaveHoldsNeitherOff);", 2 },
		{ "bh_facehugger", ", &block->dynamics, kSaveHoldsGravityOff);", 1 }, { "bh_marine", ", &block->dynamics, kSaveHoldsGravityOff);", 1 },
		{ "bh_platform_lift", ", &block->dynamics, kSaveHoldsNeitherOff);", 1 }, { "bh_predator", ", &block->dynamics, kSaveHoldsNeitherOff);", 1 },
		{ "bh_queen", ", &block->dynamics, kSaveHoldsWallsAndGravityOff);", 1 }, { "bh_weapon", ", &block->dynamics, kSaveHoldsGravityOff);", 13 },
		{ "bh_xeno", ", &block->dynamics, kSaveHoldsNeitherOff);", 1 }, { "bonusabilities", ", &block->dynamics, kSaveHoldsNeitherOff);", 1 },
		{ "player", ", &block->dynamics, kSaveHoldsGravityOff);", 1 }, { "pvisible", ", &block->dynamics, kSaveHoldsGravityOff);", 1 },
		{ "weapon_m79", ", &block->dynamics, kSaveHoldsNeitherOff);", 1 } };
	int loaders = 0;

	for (Holds const &h : holds)
	{
		std::string const text = ReadSourceFile((avp + h.name + ".cpp").c_str());
		Check(CountOf(text, h.says) == h.count && CountOf(text, "Save_RestoreDynamics(") == h.count,
			(std::string("each of ") + h.name + "'s loaders says what of its walls and gravity its save can hold off").c_str());
		loaders += CountOf(text, "Save_RestoreDynamics(");
	}

	// Secondary's read of a23b233f, S1 (added 2026-11-11 by Melanikus (Third) for the AvP port, SAVE-HOSTILE-1): all 28 loaders of a saved
	// dynamics block ask, the M79 grenade's too, and no file of the game's own code copies a saved one straight over. Every file is read,
	// so a loader added later that copies one is found too.
	Check(loaders == 28, ("all 28 loaders of a saved dynamics block put it back through the check (" + std::to_string(loaders) + ")").c_str());
	std::string const here = __FILE__;
	std::string::size_type const testsAt = here.rfind("tests");
	size_t filesRead = 0;
	std::string plainCopies;

	for (char const *const top : { "3dc", "win32" })
	{
		std::filesystem::path const folder = std::filesystem::path(here.substr(0, testsAt)) / top;
		std::error_code missing;

		for (std::filesystem::recursive_directory_iterator walk(folder, missing), end; !missing && walk != end; walk.increment(missing))
		{
			std::string const ext = walk->path().extension().string();

			if (!walk->is_regular_file() || (ext != ".cpp" && ext != ".c" && ext != ".h"))
			{
				continue;
			}

			std::ifstream file(walk->path(), std::ios::binary);
			std::stringstream all;
			all << file.rdbuf();
			std::string const text = all.str();
			filesRead++;

			if (Has(text, "= block->dynamics;") || Has(text, "=block->dynamics;"))
			{
				plainCopies += " " + walk->path().filename().string();
			}
		}
	}

	Check(filesRead > 300 && plainCopies.empty(), ("no file of the game's own code copies a saved dynamics block straight over (" +
		std::to_string(filesRead) + " files read" + (plainCopies.empty() ? std::string() : ";" + plainCopies) + ")").c_str());

	Check(CountOf(ReadSourceFile("3dc/avp/save_checks.h"), "kSaveHoldsWallsAndGravityOff") == 2 && CountOf(ReadSourceFile("3dc/avp/bh_queen.cpp"), "kSaveHoldsWallsAndGravityOff") == 1,
		"only the queen's loader keeps a save's walls off");

	std::string const menus = ReadSourceFile("3dc/avp/win95/frontend/menus.cpp");
	Check(Has(menus, "if (!Save_SlotNumbersAreValid(block.Species, block.Episode, block.Difficulty))"), "a save slot's numbers are asked about when the menu lists it");

	std::string const player = ReadSourceFile("3dc/avp/player.cpp");
	Check(Has(player, "if(!Save_WeaponSlotIsValid(slot))"), "the player's weapon slot is asked about");
	Check(Has(player, "if(Save_WeaponStateIsValid(block->WeaponSlot[i].CurrentState))"), "and each weapon's state");
	Check(Has(player, "if(!Save_SwapFitsWeaponState(selectedWeapon->CurrentState,playerStatusPtr->SwapToWeaponSlot))"), "and whether it can go with the slot to swap to");
	Check(Has(player, "if(Save_VisionModeIsValid(block->CurrentVisionMode))"), "and the vision mode");
	Check(Has(player, "if(Save_AmmoIsValid(block->GrenadeLauncherData.SelectedAmmo))"), "and the grenade");

	Check(Has(ReadSourceFile("3dc/avp/bh_ais.cpp"), "*path = PathArray ? Save_PlaceInListOrNone(*path, PathArraySize) : -1;"), "a saved path has to be a real path");
	Check(Has(ReadSourceFile("3dc/avp/bh_ais.cpp"), "*stepnumber = (*path >= 0) ? Save_PlaceInListOrNone(*stepnumber, PathArray[*path].path_length) : -1;"), "and a real step on it");

	// What keeps bh_ais.cpp's asserts on a path and a step out of reach of a save, or of a level (tests/assert_limits_allowed.txt)
	std::string const marine = ReadSourceFile("3dc/avp/bh_marine.cpp");
	std::string const predator = ReadSourceFile("3dc/avp/bh_predator.cpp");
	Check(Has(marine, "\tNPC_CheckSavedPath(&marineStatusPointer->path, &marineStatusPointer->stepnumber);") && Has(predator, "\tNPC_CheckSavedPath(&predatorStatusPointer->path, &predatorStatusPointer->stepnumber);"), "a marine's and a predator's saved path and step both go through that check");
	Check(CountOf(marine, "if ((marineStatusPointer->stepnumber<0)||(marineStatusPointer->path<0)) {") == 2 && CountOf(predator, "if ((predatorStatusPointer->stepnumber<0)||(predatorStatusPointer->path<0)) {") == 2, "and each of the four places that walks a path goes off wandering first when there's no path or step");
	Check(Has(ReadSourceFile("3dc/avp/win95/projload.cpp"), "if(path_index<0 || path_index>=PathArraySize)\n\t{\n\t\treturn -1; //path doesn't exist"), "a level's path that isn't there gets no step");
	Check(Has(marine, "if(!PathCanBeFollowed(new_path)) return;"), "and a switch's path is asked about before a marine follows it");
	Check(Has(ReadSourceFile("3dc/avp/bh_corpse.cpp"), "Save_PlaceInListOrNone(block->hltable_index, numberOfTables)"), "a corpse's table has to be real");
	Check(Has(ReadSourceFile("3dc/avp/decal.cpp"), "if(Save_PlaceInListOrNone(saved_decal->ModuleIndex, ModuleArraySize) < 0)"), "a decal's module has to be real");
	Check(Has(ReadSourceFile("3dc/avp/bh_queen.cpp"), "Save_PlaceInListOrNone(block->CurrentQueenObject, NumQueenObjects)"), "and what the Queen holds");
	Check(Has(ReadSourceFile("3dc/avp/bh_generator.cpp"), "if(Save_NumberIsInRange(block->GenerationRate, 1, (60 * ONE_FIXED * 100) / 4))"), "a generator's rate has to be one it could reach");
	Check(Has(ReadSourceFile("3dc/avp/bh_generator.cpp"), "if(Save_NumberIsInRange(block->generatorNPCsPerMinute, GENSPERMINUTE_MIN, GENSPERMINUTE_MAX))"), "and its NPCs a minute");
	Check(Has(ReadSourceFile("3dc/avp/bh_predator.cpp"), "if(Save_NumberIsInRange(block->personalNumber, 0, PRED_MAXIDENTITY))"), "a predator's number has to pick one of its settings");
	Check(Has(ReadSourceFile("3dc/avp/bh_binary_switch.cpp"), "if(bs_bhv->bs_tac && Save_TextureSequenceIsValid(bs_bhv->bs_tac->tac_txarray, block->txanim_sequence))"), "a switch's sequence has to be its own");
	Check(Has(ReadSourceFile("3dc/avp/bh_link_switch.cpp"), "if(ls_bhv->ls_tac && Save_TextureSequenceIsValid(ls_bhv->ls_tac->tac_txarray, block->txanim_sequence))"), "and a linked switch's");
	Check(Has(ReadSourceFile("3dc/avp/bh_light.cpp"), "if (Save_TextureSequenceIsValid(tac->tac_txarray, sequence))"), "and a light's");
	Check(Has(ReadSourceFile("3dc/avp/bh_weapon.cpp"), "behav->Target = Save_TargetWithAPlace(behav->Target);"),
		"a saved disc's target has to have a place in the world to home on");
	Check(Has(ReadSourceFile("3dc/avp/bh_alien.cpp"), "alienStatusPointer->Target = Save_TargetWithAPlace(FindSBWithName(alienStatusPointer->Target_SBname));"),
		"and an alien's");
	Check(Has(ReadSourceFile("3dc/avp/bh_marine.cpp"), "marineStatusPointer->Target = Save_TargetWithAPlace(FindSBWithName(marineStatusPointer->Target_SBname));"),
		"and a marine's");
	Check(Has(ReadSourceFile("3dc/avp/bh_predator.cpp"), "predatorStatusPointer->Target = Save_TargetWithAPlace(FindSBWithName(predatorStatusPointer->Target_SBname));"),
		"and a predator's");
	Check(Has(ReadSourceFile("3dc/avp/bh_xeno.cpp"), "xenoStatusPointer->Target = Save_TargetWithAPlace(FindSBWithName(xenoStatusPointer->Target_SBname));"),
		"and a xenoborg's");
	Check(Has(ReadSourceFile("3dc/avp/bh_lift_door.cpp"), "Save_MorphFrameOrOwn(block->ObMorphCurrFrame,"), "a lift door's frame has to be real");
	Check(Has(ReadSourceFile("3dc/avp/bh_switch_door.cpp"), "Save_MorphFrameOrOwn(block->ObMorphCurrFrame,"), "and a switch door's");
	Check(Has(ReadSourceFile("3dc/avp/bh_types.cpp"), "Save_MorphFrameOrOwn(block->ObMorphCurrFrame,"), "and a proximity door's");
	Check(CountOf(ReadSourceFile("3dc/avp/psnd.cpp"), "if(!Save_NameFillsBlock(block->header.size, sizeof(*block), &block->fileNameLength, name)) return;") == 2, "a saved sound's name has to end inside its block, in both places a sound is loaded");

	std::string const savegame = ReadSourceFile("3dc/avp/savegame.cpp");
	Check(Has(savegame, "Save_StartCountingRefusals();"), "each load starts counting afresh");
	Check(Has(savegame, "Save_DescribeRefusals(refusals, sizeof(refusals));"), "and says what it refused");
	Check(Has(savegame, "if(sbPtr && sbPtr != Player->ObStrategyBlock)"), "a save can't take the player's block away");
}

// Added 2026-11-09 by Melanikus for the AvP port (#86): a load from an empty slot ends its request, so it doesn't wait (reading the save
// folder every frame) for the next save made there and load that unasked, as the 1999 code did. LoadSavedGame needs the whole game, so
// it's read as text.
static void TestAnEmptySlot()
{
	std::string const saves = ReadSourceFile("3dc/avp/savegame.cpp");
	size_t const start = saves.find("void LoadSavedGame()\n{");
	size_t const end = start == std::string::npos ? std::string::npos : saves.find("\n}\n", start);
	std::string const loading = end == std::string::npos ? std::string() : saves.substr(start, end - start);
	size_t const scan = loading.find("\tScanSaveSlots();\n");
	size_t const empty = loading.find("\tif(!save_slot->SlotUsed)\n\t{\n\t\tLogString(\"save: there's no save in that slot, so nothing is loaded\");\n"
		"\t\tLoadGameRequest = SAVELOAD_REQUEST_NONE;\n\t\treturn;\n\t}\n");
	Check(scan != std::string::npos && empty != std::string::npos && scan < empty,
		"a load from an empty slot ends its request, and says so, so it can't load the next save made there");
	Check(!loading.empty() && !Has(loading, "if(!save_slot->SlotUsed) return;"), "and nothing leaves an empty slot keeping the request, as 1999's line did");
}

int main()
{
	TestAnEmptySlot();
	TestSlotNumbers();
	TestTableNumbers();
	TestWeaponSwaps();
	TestListPlaces();
	TestNames();
	TestMorphFrames();
	TestTextureSequences();
	TestRanges();
	TestDynamics();
	TestCountingRefusals();
	TestWallsAndGravity();
	TestTargets();
	TestTheLoadersAsk();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "all checks passed");
	return checksFailed ? 1 : 0;
}
