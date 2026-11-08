// Tests for the marine's shotgun. The first part tries the sums in 3dc/avp/shotgun.h, which know nothing about the game. The second part reads the game's source, for the things that only mean something where they join the game: its table rows, its firing and its loading. Written 2026-11-06 by Melanikus for the AvP port.
//
// WHAT IT DOESN'T TEST: nothing here fires a shot in the game, or draws the gun. That takes a run with the model file in a test copy.

#include "shotgun.h"

#include <fstream>
#include <sstream>
#include <stdio.h>
#include <string>

static int checksFailed = 0;

static void Check(bool good, const std::string &what)
{
	printf("%s  %s\n", good ? "ok    " : "FAILED", what.c_str());
	checksFailed += good ? 0 : 1;
}

// A turn as it would be if the angle hadn't wrapped round: below 0 for a turn the other way
static int AsSigned(int turn)
{
	return turn >= kAngleStepsInACircle / 2 ? turn - kAngleStepsInACircle : turn;
}

static void TestTheNumbers()
{
	Check(kShotgunPellets == 32, "a shot is 32 pellets");
	Check(kShotgunPelletsTight == 11 && kShotgunPelletsMiddle == 11 && kShotgunPelletsWide == 10, "11 tight, 11 wider and 10 widest");
	Check(kShotgunPelletImpact == 4 && kShotgunPelletPenetrating == 4, "each does impact 4 and penetrating 4");
	Check(kShotgunShells == 12, "the gun holds 12 shells");
	Check(kShotgunFiringSlowdown == 16384, "firing leaves him a quarter of his speed");

	int const rings[3] = { 32, 70, 100 };
	bool ringsRight = true;

	for (int pellet = 0; pellet < kShotgunPellets; pellet++)
	{
		int const ring = pellet < 11 ? 0 : pellet < 22 ? 1 : 2;

		ringsRight = ringsRight && Shotgun_SpreadOfPellet(pellet, false) == rings[ring];
	}

	Check(ringsRight, "the pellets go in three rings, each wider than the last");
	Check(Shotgun_SpreadOfPellet(-1, false) == 0, "a pellet before the first has no spread");
	Check(Shotgun_SpreadOfPellet(kShotgunPellets, false) == 0, "and so does one past the last");

	int const ringsOpen[3] = { 17, 35, 50 };
	bool openRight = true;

	for (int pellet = 0; pellet < kShotgunPellets; pellet++)
	{
		int const ring = pellet < 11 ? 0 : pellet < 22 ? 1 : 2;

		openRight = openRight && Shotgun_SpreadOfPellet(pellet, true) == ringsOpen[ring];
	}

	Check(openRight, "with the stock open the rings are about half as wide");
	Check(Shotgun_SpreadOfPellet(-1, true) == 0 && Shotgun_SpreadOfPellet(kShotgunPellets, true) == 0, "and a pellet that isn't one still has no spread");

	Check(Shotgun_ZoomTowards(1.0f, 0.65f, 0.05f) < 1.0f && Shotgun_ZoomTowards(1.0f, 0.65f, 0.05f) > 0.65f, "the zoom moves towards where it's going");
	Check(Shotgun_ZoomTowards(1.0f, 0.65f, 10.0f) == 0.65f, "and doesn't go past it going in, however long the frame");
	Check(Shotgun_ZoomTowards(0.65f, 1.0f, 10.0f) == 1.0f, "or going out");
	Check(Shotgun_ZoomTowards(0.65f, 0.65f, 0.05f) == 0.65f, "and stays when it's there");
}

static void TestTheTurns()
{
	for (int const spread : { 32, 70, 100 })
	{
		int lowest = 0, highest = 0;
		bool allThere[200] = { false };

		for (int random = 0; random < spread * 3; random++)
		{
			int const turn = AsSigned(Shotgun_PelletTurn(random, spread));

			lowest = turn < lowest ? turn : lowest;
			highest = turn > highest ? turn : highest;

			if (turn + spread / 2 >= 0 && turn + spread / 2 < 200)
			{
				allThere[turn + spread / 2] = true;
			}
		}

		bool everyOne = true;

		for (int i = 0; i < spread; i++)
		{
			everyOne = everyOne && allThere[i];
		}

		Check(lowest == -spread / 2, "a spread of " + std::to_string(spread) + " goes as far as half of it one way");
		Check(highest == spread - 1 - spread / 2, "and just under half of it the other");
		Check(everyOne, "and every step between comes up");
	}

	Check(Shotgun_PelletTurn(16, 32) == 0, "the middle of the spread is straight ahead");
	Check(Shotgun_PelletTurn(0, 32) == 4096 - 16, "a turn the other way wraps round, as the game's angles do");
	Check(Shotgun_PelletTurn(12345, 0) == 0, "no spread is no turn, and doesn't divide by 0");
	Check(Shotgun_PelletTurn(12345, -5) == 0, "and so is a spread below 0");

	int const fromBelowZero = AsSigned(Shotgun_PelletTurn(-1, 100));

	Check(fromBelowZero >= -50 && fromBelowZero < 50, "a random number below 0 stays inside the spread");
	Check(Shotgun_PelletTurn(0x7FFFFFFF, 100) >= 0 && Shotgun_PelletTurn(0x7FFFFFFF, 100) < 4096, "and the largest stays a real angle");
}

static void TestLoading()
{
	Check(Shotgun_ShellsThatGoIn(0, 0, 50) == 1, "one shell goes in as a loading state ends");
	Check(Shotgun_ShellsThatGoIn(-100, 0, 50) == 1, "and still one if the timer's gone a little past the end");
	Check(Shotgun_ShellsThatGoIn(-65536, 0, 50) == 2, "two if it's gone a whole state's length past");
	Check(Shotgun_ShellsThatGoIn(-65536 * 3 - 1, 0, 50) == 4, "and one more for each whole length after that");
	Check(Shotgun_ShellsThatGoIn(-65536 * 30, 10, 50) == 2, "never more than there's room for");
	Check(Shotgun_ShellsThatGoIn(-65536 * 30, 0, 3) == 3, "or more than he has");
	Check(Shotgun_ShellsThatGoIn(0, 0, 0) == 0, "none if he has none");
	Check(Shotgun_ShellsThatGoIn(0, 12, 50) == 0, "none if it's full");
	Check(Shotgun_ShellsThatGoIn(0, 500, 50) == 0, "none, and not a number below 0, if a saved game says it's fuller than full");
	Check(Shotgun_ShellsThatGoIn(0, 0, -5) == 0, "and none if he has fewer than none");
	Check(Shotgun_ShellsThatGoIn(-2147483647 - 1, 0, 50) == 12, "the lowest timer there is stays inside the gun");

	Check(!Shotgun_LoadingIsOver(3, 10, false), "loading goes on with room and shells left");
	Check(Shotgun_LoadingIsOver(12, 10, false), "it stops when the gun's full");
	Check(Shotgun_LoadingIsOver(3, 0, false), "or when he has no more");
	Check(Shotgun_LoadingIsOver(3, 10, true), "or when he presses fire");
	Check(!Shotgun_LoadingIsOver(0, 10, true), "but not until there's a shell to fire");
}

static void TestTheSavedState()
{
	ShotgunOwnState state = { 0, 0, 0 };

	Check(Shotgun_OwnStateIsGood(state), "all zeros is a good state");
	state.EmptyWhenLoadingBegan = 1;
	state.StopLoading = 1;
	Check(Shotgun_OwnStateIsGood(state), "so are ones");
	state.StopLoading = 2;
	Check(!Shotgun_OwnStateIsGood(state), "two isn't");
	state.StopLoading = 0;
	state.EmptyWhenLoadingBegan = -1;
	Check(!Shotgun_OwnStateIsGood(state), "and nor is a number below 0");
	state.EmptyWhenLoadingBegan = 0;
	state.StockOpen = 2;
	Check(!Shotgun_OwnStateIsGood(state), "and the stock is open or it isn't");
}

static std::string ReadSourceFile(char const *name)
{
	// /FC in this test's settings makes __FILE__ the whole path of this file, so everything else can be found from it
	std::string path = __FILE__;
	std::string::size_type const tests = path.rfind("tests");

	if (tests == std::string::npos)
	{
		return "";
	}

	std::ifstream file((path.substr(0, tests) + name).c_str(), std::ios::binary);
	std::stringstream all;
	all << file.rdbuf();

	// Files here have Windows' line ends, and what's looked for is written with plain ones
	std::string text = all.str(), plain;

	for (char const c : text)
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

static std::string Between(const std::string &text, const char *from, const char *to)
{
	std::string::size_type const start = text.find(from);
	std::string::size_type const end = start == std::string::npos ? start : text.find(to, start);

	return (start == std::string::npos || end == std::string::npos) ? std::string() : text.substr(start, end - start);
}

static void TestJoiningTheGame()
{
	std::string const table = ReadSourceFile("3dc/avp/equipment.cpp");
	std::string const gun = ReadSourceFile("3dc/avp/weapon_shotgun.cpp");
	std::string const row = Between(table, "* \tWEAPON_AUTOSHOTGUN   *", "* \tWEAPON_SMARTGUN   *");
	std::string const ammo = Between(table, "/* AMMO_PLAYER_SHOTGUN */", "/* AMMO_SNIPER */");
	std::string const firing = Between(gun, "int FireShotgun(PLAYER_WEAPON_DATA *weaponPtr)\n{", "\n}\n");

	// The ammunition. The computer's shotgun marines keep theirs.
	Check(Has(table, "/* AMMO_SHOTGUN */\n\t{\n\t\t20*65536,\n\t\t{\n\t\t\t{\n\t\t\t\t20,10,0,0,0,0,"), "the computer's shotgun marines keep their own ammunition");
	Check(Has(row, "AMMO_PLAYER_SHOTGUN,\n\t\t/* SecondaryAmmoID; */\n\t\tAMMO_NONE,"), "the player's shotgun uses the new row");
	Check(Has(ammo, "kShotgunShells*65536,"), "it holds as many as the sums say");

	std::string::size_type at = 0;
	int damageRows = 0;

	while ((at = ammo.find("kShotgunPelletImpact,0,kShotgunPelletPenetrating,0,0,0,", at)) != std::string::npos)
	{
		damageRows++;
		at++;
	}

	Check(damageRows == 4, "and each pellet does what the sums say, at all four difficulties");

	// The row
	Check(Has(row, "FireShotgun, /* FirePrimaryFunction */\n\t\tNULL, /* FireSecondaryFunction */"), "the row fires through its own function, with no secondary fire yet");
	Check(Has(row, "\"MelPyth_MarHUD\",\n\t\t/* HierarchyName */\n\t\t\"Shotgun\",\n\t\t/* InitialSequenceType */\n\t\t(int)HMSQT_MarineHUD,"), "and has its own model");
	Check(Has(row, "TEXTSTRING_INGAME_SHOTGUN,"), "and its own name");
	Check(Has(row, "65536*5/4, /* WEAPONSTATE_SWAPPING_IN */\n\t\t\t65536*5/4, /* WEAPONSTATE_SWAPPING_OUT */"), "it takes 0.8 of a second to bring up and to put away");
	Check(Has(row, "{ /* Encum_FirePrime */\n\t\t\tkShotgunFiringSlowdown, /* MovementMultiple */\n\t\t\tkShotgunFiringSlowdown, /* TurningMultiple */\n\t\t\tkShotgunFiringSlowdown, /* JumpingMultiple */"), "firing slows his moving, turning and jumping by the number that's named");
	Check(Has(row, "/* UseStateMovement :1; */\n\t\t0,"), "the model moves it, not the old sums for a gun with no model");
	Check(Has(row, "&Shotgun_Hooks,"), "the row points at its hooks");
	Check(Has(row, "65536*10/3, /* kShotgunState_Lift */\n\t\t\t65536*10/3, /* kShotgunState_LoadShell */\n\t\t\t65536*10/3, /* kShotgunState_Lower */\n\t\t\t65536*10/7, /* kShotgunState_Pump */"), "loading takes 0.3 of a second a step, and the pump 0.7");
	Check(Has(row, "Shotgun_Lift, /* kShotgunState_Lift */\n\t\t\tShotgun_LoadShell, /* kShotgunState_LoadShell */\n\t\t\tShotgun_Lower, /* kShotgunState_Lower */\n\t\t\tShotgun_Pump, /* kShotgunState_Pump */"), "and each has its function, in the same order");

	// Firing
	Check(Has(firing, "weaponPtr->PrimaryRoundsRemaining -= 65536;"), "one shot uses one shell");
	Check(Has(firing, "for (int pellet = 0; pellet < kShotgunPellets; pellet++)"), "and fires every pellet");
	Check(Has(firing, "int const spread = Shotgun_SpreadOfPellet(pellet, Own.StockOpen != 0);"), "each with the spread of its ring, narrower with the stock open");
	Check(Has(firing, "turn.EulerX = Shotgun_PelletTurn(FastRandom(), spread);") && Has(firing, "turn.EulerY = Shotgun_PelletTurn(FastRandom(), spread);"), "turned up or down and left or right by separate random numbers");
	Check(Has(firing, "VECTORCH direction = GunMuzzleDirectionInVS;"), "away from where the gun is pointing");
	Check(Has(firing, "FindPolygonInLineOfSight(&direction, &Global_VDB_Ptr->VDB_World, 1, Player);"), "and looks for what that pellet hits");
	Check(Has(firing, "HandleWeaponImpact(&LOS_Point, LOS_ObjectHitPtr->ObStrategyBlock, twPtr->PrimaryAmmoID, &direction, ONE_FIXED, section);"), "and each that hits does its own damage");
	Check(Has(firing, "if (section && section->my_controller != LOS_ObjectHitPtr->HModelControlBlock)\n\t\t\t{\n\t\t\t\tsection = NULL;"), "with a body part only if it belongs to what was hit");
	Check(Has(firing, "CurrentGameStats_WeaponFired(playerStatusPtr->SelectedWeaponSlot, 1);"), "a shot counts once in the statistics, however many pellets");
	Check(Has(firing, "CurrentGameStats_WeaponHit(playerStatusPtr->SelectedWeaponSlot, 1);"), "and it counts as a hit once");
	Check(firing.find("CurrentGameStats_WeaponHit") > firing.find("CurrentGameStats_WeaponFired"), "after all the pellets have gone, so accuracy can't pass 100%");

	// Loading
	std::string const ended = Between(gun, "static int Shotgun_StateEnded(", "\n}\n");

	Check(Has(ended, "case WEAPONSTATE_RELOAD_PRIMARY:\n\t\t\tShotgun_StartLoading(weaponPtr);"), "a long frame can't turn a reload into a whole magazine for one shell");
	Check(Has(ended, "weaponPtr->PrimaryRoundsRemaining += shellsIn << ONE_FIXED_SHIFT;\n\t\t\tweaponPtr->PrimaryMagazinesRemaining -= shellsIn;"), "a shell going in comes off his spares");
	Check(Has(ended, "if (Own.EmptyWhenLoadingBegan)\n\t\t\t{\n\t\t\t\tWeapon_EnterState(weaponPtr, kShotgunState_Pump);"), "the pump only follows a gun that was empty");
	Check(Has(ended, "default:\n\t\t\treturn 0;"), "and any other state goes the way it does for any gun");
	Check(Has(gun, "Own.EmptyWhenLoadingBegan = weaponPtr->PrimaryRoundsRemaining == 0 ? 1 : 0;"), "whether it was empty is noted as loading begins");
	Check(Has(gun, "Shotgun_SavedStateIsGood,\n\t1 // SparesAreSingleRounds\n};"), "a saved state is checked before it's used, and its spares are single shells");

	// The stock
	Check(Has(ended, "case WEAPONSTATE_UNREADYING:\n\t\t\tif (Own.StockOpen)\n\t\t\t{\n\t\t\t\tWeapon_EnterState(weaponPtr, kShotgunState_ZoomOutToPutAway);"), "putting the gun away closes the stock first");
	Check(Has(ended, "case kShotgunState_ClosingToPutAway:\n\t\t\tOwn.StockOpen = 0;\n\t\t\tWeapon_EnterState(weaponPtr, WEAPONSTATE_SWAPPING_OUT);"), "and then it goes");
	Check(Has(gun, "Weapon_EnterState(weaponPtr, Own.StockOpen ? kShotgunState_ZoomOut : kShotgunState_StockOpening);"), "secondary fire opens it, or closes it if it's open");
	Check(Has(gun, "Own.StockOpen ? kShotgunState_ZoomOutToLoad : kShotgunState_Lift"), "and loading zooms out first");
	Check(Has(gun, "Weapon_ThrowCase(\"ShotgunShell\", \"Eject\");"), "a spent shell comes out at the mark in the firing move");

	Check(Has(gun, "Shotgun_Play((enum MarineHUDSubSequences)kShotgunStockMove, ONE_FIXED, 0);"), "a model file with a move for the stock plays it");
	Check(Has(row, "Shotgun_StockMoves, /* kShotgunState_StockOpening */") && Has(row, "Shotgun_StockMoves, /* kShotgunState_StockClosing */") && Has(row, "Shotgun_StockMoves, /* kShotgunState_ClosingToPutAway */"), "whenever the stock is opening or closing");

	// The reload key has to start the reload where a state's function can see it has only just begun
	std::string const weapons = ReadSourceFile("3dc/avp/weapons.cpp");
	std::string::size_type const started = weapons.find("Weapon_StartReloadFromTheKey(playerStatusPtr);");
	std::string::size_type const timers = weapons.find("weaponPtr->StateTimeOutCounter-=MUL_FIXED(timeOutRate,NormalFrameTime);");
	std::string::size_type const functions = weapons.find("\tStateDependentMovement(playerStatusPtr,weaponPtr);");

	Check(started != std::string::npos && timers != std::string::npos && functions != std::string::npos && timers < started && started < functions, "the reload key's reload starts after the timers and before the state's function, or its move never plays");
	Check(Has(Between(weapons, "void Weapon_ReloadKeyPressed(void *playerStatus)\n{", "\n}\n"), "ReloadKeyIsWaiting = true;"), "and the key itself only asks");

	// Filling the gun with spares that are single shells takes what it holds, not one
	std::string const inventory = ReadSourceFile("3dc/avp/inventory.cpp");

	Check(Has(inventory, "if (twPtr->Hooks && twPtr->Hooks->SparesAreSingleRounds) {"), "filling up a gun asks how its spares are counted");
	Check(Has(inventory, "wdPtr->PrimaryRoundsRemaining = taken << ONE_FIXED_SHIFT;\n\t\t\t\twdPtr->PrimaryMagazinesRemaining -= taken;"), "and takes as many as it gives");
	Check(Has(inventory, "if (wdPtr->WeaponIDNumber == NULL_WEAPON) {\n\t\t\tcontinue;"), "an empty slot isn't looked up in the table");

	// The credit
	Check(Has(ReadSourceFile("3dc/avp/shotgun.h"), "MelPyth M5 by Mupp") && Has(gun, "MelPyth M5 by Mupp"), "where it came from is said");
}

int main()
{
	TestTheNumbers();
	TestTheTurns();
	TestLoading();
	TestTheSavedState();
	TestJoiningTheGame();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");

	return checksFailed ? 1 : 0;
}
