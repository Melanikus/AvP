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

int main()
{
	TestSlotNumbers();
	TestDynamics();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "all checks passed");
	return checksFailed ? 1 : 0;
}
