// Tests the Predator's staff: its numbers, in 3dc/avp/staff.h, and that the game is wired to them.
// Written 2026-11-06 by Melanikus for the AvP port.
//
// Nothing here needs the game or its data. How the staff looks, how it feels and where a thrown one goes need a real run.

#include "avp/staff.h"

#include <stdio.h>

#include <fstream>
#include <sstream>
#include <string>

static int checks = 0;
static int failures = 0;

static void Check(bool good, const char *what)
{
	checks++;
	failures += good ? 0 : 1;
	printf("  %s  %s\n", good ? "ok" : "FAILED", what);
}

static std::string ReadSource(const char *name)
{
	const std::string path = __FILE__;
	const std::string::size_type tests = path.rfind("tests");
	if (tests == std::string::npos)
	{
		return "";
	}
	std::ifstream in((path.substr(0, tests) + name).c_str(), std::ios::binary);
	std::stringstream all;
	all << in.rdbuf();
	std::string text = all.str();
	std::string plain;
	for (char c : text)
	{
		if (c != '\r')
		{
			plain += c;
		}
	}
	return plain;
}

static bool Has(const std::string &text, const char *what)
{
	return text.find(what) != std::string::npos;
}

// The part of the table for one row: from its heading to the next row's heading
static std::string RowOf(const std::string &table, const char *heading, const char *nextHeading)
{
	const std::string::size_type from = table.find(heading);
	if (from == std::string::npos)
	{
		return "";
	}
	const std::string::size_type to = table.find(nextHeading, from);
	return table.substr(from, to == std::string::npos ? std::string::npos : to - from);
}

static void TestNumbers()
{
	Check(!Staff_UseSecondSwing(0), "an even number gives the first swing");
	Check(!Staff_UseSecondSwing(2), "another even number gives the first swing");
	Check(Staff_UseSecondSwing(1), "an odd number gives the second swing");
	Check(Staff_UseSecondSwing(3), "another odd number gives the second swing");
	Check(Staff_UseSecondSwing(-1), "an odd number below 0 gives the second swing");

	Check(kStaffThrowSpeed == 60000, "the speed of a throw");
	Check(kStaffFlightTime == 65536 * 10, "ten seconds in the air");
	Check(kStaffSwingTime == 0x17000, "the length of a swing");
	Check(kStaffThrowTime == 65536 * 2, "the length of the throwing move");
	Check(kStaffStayTime == 65536 * 20, "twenty seconds once it has landed");
}

static void TestFlight()
{
	Check(ThrownStaff_ReachesIt(0, 0), "something right there is reached, even before it has moved");
	Check(ThrownStaff_ReachesIt(9, 0), "and so is something just past its tip");
	Check(!ThrownStaff_ReachesIt(10, 0), "but not something further than that");
	Check(ThrownStaff_ReachesIt(1009, 1000), "a step of a metre reaches just past its own length");
	Check(!ThrownStaff_ReachesIt(1010, 1000), "and no further");
}

static void TestSavedNumbers()
{
	Check(ThrownStaff_SavedStageIsGood(kThrownStaffFlying, 1), "a flying staff with a moment left");
	Check(ThrownStaff_SavedStageIsGood(kThrownStaffFlying, kStaffFlightTime), "a flying staff with all its time left");
	Check(!ThrownStaff_SavedStageIsGood(kThrownStaffFlying, kStaffFlightTime + 1), "a flying staff can't have more than that");
	Check(!ThrownStaff_SavedStageIsGood(kThrownStaffFlying, kStaffStayTime), "not even as much as a landed one");
	Check(!ThrownStaff_SavedStageIsGood(kThrownStaffFlying, 0), "a flying staff with no time left");
	Check(ThrownStaff_SavedStageIsGood(kThrownStaffLanded, kStaffStayTime), "a landed staff with all its time left");
	Check(!ThrownStaff_SavedStageIsGood(kThrownStaffLanded, kStaffStayTime + 1), "a landed staff can't have more than that");
	Check(!ThrownStaff_SavedStageIsGood(kThrownStaffLanded, -1), "a landed staff with time below 0");
	Check(ThrownStaff_SavedStageIsGood(kThrownStaffInABody, kStaffStayTime), "a staff in a body with all its time left");
	Check(!ThrownStaff_SavedStageIsGood(kThrownStaffInABody, kStaffStayTime + 1), "a staff in a body can't have more than that");
	Check(!ThrownStaff_SavedStageIsGood(kThrownStaffInABody, 0), "a staff in a body with no time left");
	Check(!ThrownStaff_SavedStageIsGood(3, 100), "a stage after the last isn't one");
	Check(!ThrownStaff_SavedStageIsGood(-1, 100), "nor is one below the first");

	Check(ThrownStaff_SavedNumberIsGood(0), "a place at the middle of the world");
	Check(ThrownStaff_SavedNumberIsGood(999999), "a place near the edge");
	Check(!ThrownStaff_SavedNumberIsGood(1000000), "a place past the edge");
	Check(ThrownStaff_SavedNumberIsGood(-999999), "a place near the other edge");
	Check(!ThrownStaff_SavedNumberIsGood(-1000000), "a place past the other edge");
}

static void TestTable()
{
	const std::string table = ReadSource("3dc/avp/equipment.cpp");
	const std::string row = RowOf(table, "*   WEAPON_PRED_STAFF", "*   WEAPON_ALIEN_CLAW");
	const std::string ammo = RowOf(table, "/* AMMO_PRED_STAFF */", "/* AMMO_NPC_PRED_STAFF */");

	Check(!row.empty(), "the staff's row is there");
	Check(Has(row, "\"Staff_Hud\""), "its model's file");
	Check(Has(row, "\"Staff\""), "its model's name");
	Check(Has(row, "TEXTSTRING_INGAME_STAFF"), "its name on the screen");
	Check(Has(row, "PredStaff_Swing"), "primary fire swings it");
	Check(Has(row, "PredStaff_Throw"), "secondary fire throws it");
	Check(Has(row, "PredStaff_StartThrow"), "secondary fire can start");
	Check(Has(row, "PredStaff_SwapIn"), "it's brought up");
	Check(Has(row, "PredStaff_SwapOut"), "it's put away");
	Check(!Has(row, "StaffAttack_Basic"), "the old swing is gone");
	Check(Has(row, "65536*2/3, /* WEAPONSTATE_FIRING_PRIMARY */"), "a second and a half for each swing");
	Check(Has(row, "65536/2, /* WEAPONSTATE_FIRING_SECONDARY */"), "two seconds for each throw");

	Check(!ammo.empty(), "the staff's ammunition row is there");
	Check(Has(ammo, "0,20,20,0,0,0,"), "20 cutting and 20 penetrating, the user's ruling");
	Check(!Has(ammo, "0,120,0,0,0,0,"), "the old damage is gone");
	Check(Has(ammo, "4200,"), "it reaches 4200");
	Check(Has(ammo, "1*65536,"), "it holds one, which a throw needs");

	const std::string key = RowOf(table, "PredatorWeaponKey[] = {", "};");
	Check(Has(key, "WEAPON_PRED_STAFF"), "the Predator has a place for it");
}

static void TestWiring()
{
	const std::string staff = ReadSource("3dc/avp/weapon_staff.cpp");
	const std::string setup = ReadSource("3dc/avp/win95/npcsetup.cpp");
	const std::string cheat = ReadSource("3dc/avp/win95/cheat.cpp");
	const std::string text = ReadSource("3dc/avp/language.cpp");

	Check(Has(staff, "MeleeWeapon_180Degree_Front_Core(&ammo->MaxDamage[AvP.Difficulty], ONE_FIXED, ammo->MaxRange)"), "a swing uses the row's damage and reach");
	Check(Has(staff, "if (PlayersWeaponHModelController.keyframe_flags & 1)\n"), "a swing lands when the model says");
	Check(Has(staff, "Staff_UseSecondSwing(FastRandom())"), "a swing is chosen at random");
	Check(Has(staff, "weaponPtr->PrimaryRoundsRemaining > 0"), "a throw needs a staff");
	Check(Has(staff, "MakeObject(I_BehaviourThrownStaff, place)"), "a thrown staff is an object of its own");
	Check(Has(staff, "AllocateDynamicsBlock(DYNAMICS_TEMPLATE_GRENADE)"), "a thrown staff falls");
	Check(Has(staff, "dynPtr->DynamicsType = DYN_TYPE_NO_COLLISIONS;"), "nothing stops it in the air but what the line finds");
	Check(Has(staff, "FindPolygonInLineOfSight_TwoIgnores(&direction, &dynPtr->PrevPosition, 0, sbPtr->SBdptr, Player)"), "it looks along its own path, and can't hit the thrower");
	Check(Has(staff, "ThrownStaff_ReachesIt(LOS_Lambda, stepLength)"), "it only reacts to what it reaches this frame");
	Check(Has(staff, "dynPtr->PrevPosition = dynPtr->Position;\n}\n\n// One frame in the air"), "it keeps its own last place, so the line is one frame long, and so is the distance it adds");
	Check(Has(staff, "!ThrownStaff_ReachesIt(LOS_Lambda, stepLength))\n\t{\n\t\tThrownStaff_FlyOn(dynPtr);"), "when nothing stops it");
	Check(Has(staff, "flies on\", block->DistanceFlown);\n\t\t\tThrownStaff_FlyOn(dynPtr);"), "and when it cuts off a body part");
	Check(Has(staff, "TemplateAmmo[AMMO_PRED_RIFLE].MaxDamage[AvP.Difficulty]"), "it has the speargun's damage");
	Check(!Has(staff, "DYN_TYPE_NRBB_COLLISIONS"), "it never collides, because that let one fall 88 metres through the floor");
	Check(Has(staff, "block->RestHeight = LOS_ObjectHitPtr ? LOS_Point.vy : place.vy;"), "it finds the floor with a line, or stays put");
	Check(Has(staff, "if (dynPtr->GravityOn && dynPtr->Position.vy >= block->RestHeight)"), "and stops falling when it gets there");
	Check(Has(staff, "dynPtr->GravityOn = dynPtr->Position.vy < block->RestHeight ? 1 : 0;"), "and a save knows whether it's still falling");
	Check(Has(staff, "!ThrownStaff_SavedNumberIsGood(block->RestHeight)"), "and that height is checked");
	Check(Has(staff, "\"hit something of kind %d that isn't a body, and is gone\""), "the log says what kind of thing it hit");
	Check(Has(staff, "block->TimeLeft <= 0"), "its time runs out");
	Check(Has(staff, "if (!part || (part->flags & section_data_notreal))"), "it doesn't stay in a body part that's gone");
	Check(Has(staff, "if (!body)\n"), "or in a body that's gone");
	Check(Has(staff, "\"staff: thrown, heading %d %d %d\""), "the log says which way it went");
	Check(Has(staff, "\"staff: %s, after flying %d mm\""), "and what became of it, and how far it flew");
	Check(Has(staff, "\"staff: its time ran out, lying %d mm below where it hit\""), "and how far a landed one dropped");
	Check(Has(staff, "block->HeightItHitAt = point->vy;"), "measured from where it hit");
	Check(Has(staff, "header->size != sizeof(*block)"), "a save block of the wrong size is left alone");
	Check(Has(staff, "!ThrownStaff_SavedStageIsGood(block->State, block->TimeLeft)"), "its stage and time are checked");
	Check(Has(staff, "!ThrownStaff_SavedPlaceIsGood(&block->Position)"), "and its place");
	Check(Has(staff, "!ThrownStaff_SavedPlaceIsGood(&block->LinVelocity)"), "and its speed");
	Check(!Has(staff, "*sbPtr->DynPtr = block"), "and a file can't choose how it moves");

	const std::string kinds = ReadSource("3dc/avp/bh_types.cpp");
	Check(Has(kinds, "ThrownStaffBehaviour(sbptr);"), "the game runs it each frame");
	Check(Has(kinds, "GetLoadedShapeMSL(\"StaffPickup\") >= 0 ? \"StaffPickup\" : \"spear\""), "a missing model is safe");

	const std::string saves = ReadSource("3dc/avp/savegame.cpp");
	Check(Has(saves, "SaveStrategy_ThrownStaff(sbPtr);"), "it goes into a save");
	Check(Has(saves, "LoadStrategy_ThrownStaff(header);"), "and comes back from one");

	const std::string bolt = ReadSource("3dc/avp/bh_weapon.cpp");
	Check(!Has(bolt, "kThrownStaffMark"), "the spear bolt's code is stock's again");

	const std::string old = ReadSource("3dc/avp/weapons.cpp");
	Check(!Has(old, "void Staff_SwapIn("), "the old bringing up is gone");
	Check(!Has(old, "void Staff_SwapOut("), "the old putting away is gone");
	Check(!Has(old, "void Staff_Idle("), "the old idle is gone");
	Check(!Has(old, "void StaffAttack_Basic("), "the old swing is gone");
	Check(!Has(old, "PlayerStaff1"), "and what they used");
	Check(Has(old, "int Staff_Manager("), "NPC predators still have theirs");

	Check(Has(setup, "avp_huds/Staff_Hud.rif"), "the model is looked for");
	Check(Has(setup, "LoadedNPC tempnpc(\"Staff_Hud.rif\")"), "the model is loaded");
	Check(Has(setup, "LoadedNPC tempnpc(\"StaffPickup.rif\")"), "the thrown model is loaded");

	Check(Has(cheat, "wdPtr->WeaponIDNumber==WEAPON_PRED_STAFF"), "the test command knows the staff");
	Check(!Has(cheat, "if (wdPtr->WeaponIDNumber==WEAPON_PRED_STAFF) {\n\t\t\t\tcontinue;"), "and doesn't skip it");
	Check(Has(text, "\"Staff\""), "its name is in the game's text");
}

int main()
{
	TestNumbers();
	TestFlight();
	TestSavedNumbers();
	TestTable();
	TestWiring();

	printf("%d checks, %d failed\n", checks, failures);
	return failures == 0 ? 0 : 1;
}
