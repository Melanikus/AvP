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

#include "3dc.h"
#include "dynblock.h"
#include "gamedef.h"
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
	Save_RestoreDynamics(&live, &saved);
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
	Save_RestoreDynamics(&live, &saved);
	Check(live.DynamicsType == DYN_TYPE_NO_COLLISIONS, "the first collision type is real");
	Check(live.ToppleForce == TOPPLE_FORCE_NONE, "the first topple force is real");

	MakeLive(live);
	saved.DynamicsType = DYN_TYPE_CUBOID_COLLISIONS;
	Save_RestoreDynamics(&live, &saved);
	Check(live.DynamicsType == DYN_TYPE_CUBOID_COLLISIONS, "the last collision type is real");

	// a crafted save
	MakeLive(live);
	memset(&saved, 0xFF, sizeof(saved));
	saved.Position.vx = 42;
	Save_RestoreDynamics(&live, &saved);
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
	Save_RestoreDynamics(&live, &saved);
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
	Save_RestoreDynamics(&live, &saved);
	Check(live.OrientEuler.EulerX == 0, "an angle of 4096 is the same as 0");
	Check(live.OrientEuler.EulerY == 4095, "an angle of -1 is the same as 4095");
	Check(live.OrientEuler.EulerZ == 7, "an angle many turns round comes back in range");
	Check(live.PrevOrientEuler.EulerX == 0, "the last frame's angle too");
	Check(live.PrevOrientEuler.EulerY == 4095, "the last frame's huge angle comes back in range");
	Check(live.PrevOrientEuler.EulerZ == 904, "the last frame's third angle too");

	// nothing to write to
	Save_RestoreDynamics(0, &saved);
	Check(true, "an object with no dynamics block is left alone");
}

// A good value has to count nothing, and a bad one has to count against the check that refused it and no other. If only the total were looked at, a later check could hide a missing one.
static void CheckRefusedBy(const char *check, int count, const char *what)
{
	char words[200] = { 0 };

	Save_DescribeRefusals(words, sizeof(words));

	char wanted[120] = { 0 };

	snprintf(wanted, sizeof(wanted), "save: %d values refused, by %s %d", count, check, count);
	Check(Save_RefusalsBy(check) == count && strcmp(words, wanted) == 0, what);
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
	Save_RestoreDynamics(&live, &saved);
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
	Save_RestoreDynamics(&live, &saved);
	CheckRefusedBy("a mass", 1, "a mass of nothing");

	MakeLive(live);
	saved.Mass = 30;
	saved.DynamicsType = (enum DYN_TYPE)9999;
	Save_RestoreDynamics(&live, &saved);
	CheckRefusedBy("a kind of dynamics", 1, "a kind of dynamics that isn't one");

	MakeLive(live);
	saved.DynamicsType = DYN_TYPE_NRBB_COLLISIONS;
	saved.ToppleForce = (enum TOPPLE_FORCE)9999;
	Save_RestoreDynamics(&live, &saved);
	CheckRefusedBy("a topple force", 1, "a topple force that isn't one");

	MakeLive(live);
	saved.ToppleForce = TOPPLE_FORCE_FULL;
	saved.CollisionReportPtr = (struct collisionreport *)&live;
	Save_RestoreDynamics(&live, &saved);
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

int main()
{
	TestSlotNumbers();
	TestTableNumbers();
	TestListPlaces();
	TestNames();
	TestMorphFrames();
	TestTextureSequences();
	TestRanges();
	TestDynamics();
	TestCountingRefusals();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "all checks passed");
	return checksFailed ? 1 : 0;
}
