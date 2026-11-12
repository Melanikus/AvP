// Tests the sums behind the M79, in 3dc/avp/m79.h, and that the game is wired to them. Written 2026-11-06 by Melanikus for the AvP port.
//
// Nothing here needs the game or its data. How the gun looks and sounds, and what its grenades do, need a real run.

#include "avp/m79.h"

#include <stdio.h>
#include <string.h>

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

static M79OwnState Empty()
{
	M79OwnState state;
	memset(&state, 0, sizeof(state));
	return state;
}

static void TestChoosingAKind()
{
	M79OwnState state = Empty();

	Check(M79_NextKindWithAmmo(state, kM79Standard) == -1, "no ammunition at all: nothing to go to");

	state.Spares[kM79Frag] = 3;
	Check(M79_NextKindWithAmmo(state, kM79Standard) == kM79Frag, "skips a kind with none");
	Check(M79_NextKindWithAmmo(state, kM79Frag) == -1, "only this kind left: stays on it");

	state.Spares[kM79Fire] = 1;
	Check(M79_NextKindWithAmmo(state, kM79Standard) == kM79Fire, "the next kind in order");
	Check(M79_NextKindWithAmmo(state, kM79Fire) == kM79Frag, "and the one after");
	Check(M79_NextKindWithAmmo(state, kM79Frag) == kM79Fire, "going round past the end");

	M79OwnState loadedOnly = Empty();
	loadedOnly.RoundsInGun[kM79Fire] = 1;
	Check(M79_NextKindWithAmmo(loadedOnly, kM79Standard) == kM79Fire, "a loaded grenade counts as ammunition");

	Check(M79_NextKindWithAmmo(state, -1) == -1, "a kind below the list");
	Check(M79_NextKindWithAmmo(state, kM79Kinds) == -1, "a kind past the list");
	Check(!M79_KindHasAmmo(state, kM79Kinds), "asking about a kind past the list");
	Check(!M79_KindHasAmmo(state, -1), "asking about a kind below the list");
}

static void TestTapping()
{
	M79OwnState state = Empty();

	Check(!M79_TapChangeKey(state), "a tap with nothing else to change to does nothing");
	Check(state.ChangeIsWaiting == 0, "and nothing is left waiting");

	state.Spares[kM79Fire] = 2;
	state.Spares[kM79Frag] = 2;
	Check(M79_TapChangeKey(state), "the first tap");
	Check(state.ChangeIsWaiting == 1, "the change waits");
	Check(state.KindToChangeTo == kM79Fire, "it's going to the next kind");
	Check(state.ChangeCountdown == kM79ChangeDelay, "half a second");
	Check(state.Kind == kM79Standard, "the gun hasn't changed yet");

	state.ChangeCountdown = 100;
	Check(M79_TapChangeKey(state), "a second tap");
	Check(state.KindToChangeTo == kM79Frag, "carries on from where the first reached");
	Check(state.ChangeCountdown == kM79ChangeDelay, "and starts the half second again");

	M79OwnState alone = Empty();
	alone.Kind = kM79Standard;
	alone.Spares[kM79Standard] = 2;
	alone.Spares[kM79Fire] = 2;
	Check(M79_TapChangeKey(alone), "two kinds: the first tap");
	Check(M79_TapChangeKey(alone), "two kinds: the second tap");
	Check(alone.KindToChangeTo == kM79Standard, "two kinds: comes back to where it started");
}

static void TestChanging()
{
	M79OwnState state = Empty();
	state.Kind = kM79Standard;
	state.RoundsInGun[kM79Standard] = 1;
	state.Spares[kM79Standard] = 4;
	state.Spares[kM79Fire] = 2;
	state.ChangeIsWaiting = 1;
	state.ChangeCountdown = 7;

	Check(M79_ChangeKind(state, kM79Fire), "a change to another kind happens");
	Check(state.Kind == kM79Fire, "the kind is the new one");
	Check(state.Spares[kM79Standard] == 5, "the loaded grenade went back with its spares");
	Check(state.RoundsInGun[kM79Standard] == 0, "and left the gun");
	Check(state.RoundsInGun[kM79Fire] == 1, "one of the new kind went in");
	Check(state.Spares[kM79Fire] == 1, "taken from its spares");
	Check(state.GunWasEmptyAtChange == 0, "the gun had a grenade in it");
	Check(state.ChangeIsWaiting == 0 && state.ChangeCountdown == 0, "nothing's waiting any more");

	Check(!M79_ChangeKind(state, kM79Fire), "a change to the same kind does nothing");
	Check(state.RoundsInGun[kM79Fire] == 1, "and leaves the grenade where it is");
	Check(!M79_ChangeKind(state, kM79Kinds), "a kind past the list");
	Check(!M79_ChangeKind(state, -1), "a kind below the list");
	Check(state.Kind == kM79Fire, "and the kind stays");

	M79OwnState spent = Empty();
	spent.Kind = kM79Standard;
	spent.Spares[kM79Standard] = 1;
	spent.Spares[kM79Frag] = 1;
	Check(M79_ChangeKind(spent, kM79Frag), "a change from an empty gun");
	Check(spent.GunWasEmptyAtChange == 1, "remembers the gun was empty");
	Check(spent.Spares[kM79Standard] == 1, "and adds nothing to the old kind");

	M79OwnState full = Empty();
	full.Kind = kM79Standard;
	full.RoundsInGun[kM79Standard] = 1;
	full.Spares[kM79Standard] = kM79MostSpares;
	full.Spares[kM79Fire] = 1;
	M79_ChangeKind(full, kM79Fire);
	Check(full.Spares[kM79Standard] == kM79MostSpares, "going back to a full count doesn't go over");

	M79OwnState alreadyLoaded = Empty();
	alreadyLoaded.Kind = kM79Standard;
	alreadyLoaded.RoundsInGun[kM79Fire] = 1;
	alreadyLoaded.Spares[kM79Fire] = 3;
	M79_ChangeKind(alreadyLoaded, kM79Fire);
	Check(alreadyLoaded.RoundsInGun[kM79Fire] == 1 && alreadyLoaded.Spares[kM79Fire] == 3, "a kind already holding a grenade doesn't load another");

	M79OwnState noneLeft = Empty();
	noneLeft.Kind = kM79Standard;
	M79_ChangeKind(noneLeft, kM79Frag);
	Check(noneLeft.RoundsInGun[kM79Frag] == 0 && noneLeft.Spares[kM79Frag] == 0, "a kind with none left stays empty");
}

static void TestPickups()
{
	M79OwnState state = Empty();

	Check(M79_AddPickup(state, kM79Fire), "a box of ammunition");
	Check(state.Spares[kM79Fire] == kM79SparesPerPickup, "holds five");
	Check(state.Spares[kM79Standard] == 0, "and goes only to its own kind");

	state.Spares[kM79Fire] = kM79MostSpares - 2;
	Check(M79_AddPickup(state, kM79Fire), "a box when nearly full");
	Check(state.Spares[kM79Fire] == kM79MostSpares, "stops at the most he can carry");

	Check(!M79_AddPickup(state, kM79Fire), "a box when already full stays where it is");
	Check(!M79_AddPickup(state, kM79Kinds), "a kind past the list");
	Check(!M79_AddPickup(state, -1), "a kind below the list");
}

static M79OwnState AGoodState()
{
	M79OwnState state = Empty();
	state.Kind = kM79Fire;
	state.RoundsInGun[kM79Fire] = 1;
	state.Spares[kM79Standard] = kM79MostSpares;
	state.ChangeIsWaiting = 1;
	state.KindToChangeTo = kM79Frag;
	state.ChangeCountdown = kM79ChangeDelay;
	state.GunWasEmptyAtChange = 1;
	return state;
}

// One number changed in a good state, and the check has to refuse it for that number alone
static void TestSavedStates()
{
	Check(M79_OwnStateIsGood(AGoodState()), "a good state");
	Check(M79_OwnStateIsGood(Empty()), "a state with nothing in it");

	M79OwnState bad;

	bad = AGoodState(); bad.Kind = kM79Kinds; Check(!M79_OwnStateIsGood(bad), "a kind past the list");
	bad = AGoodState(); bad.Kind = -1; Check(!M79_OwnStateIsGood(bad), "a kind below the list");
	bad = AGoodState(); bad.KindToChangeTo = kM79Kinds; Check(!M79_OwnStateIsGood(bad), "a kind to change to past the list");
	bad = AGoodState(); bad.KindToChangeTo = -1; Check(!M79_OwnStateIsGood(bad), "a kind to change to below the list");
	bad = AGoodState(); bad.ChangeIsWaiting = 2; Check(!M79_OwnStateIsGood(bad), "waiting is only 0 or 1");
	bad = AGoodState(); bad.ChangeIsWaiting = -1; Check(!M79_OwnStateIsGood(bad), "waiting below 0");
	bad = AGoodState(); bad.GunWasEmptyAtChange = 2; Check(!M79_OwnStateIsGood(bad), "empty is only 0 or 1");
	bad = AGoodState(); bad.GunWasEmptyAtChange = -1; Check(!M79_OwnStateIsGood(bad), "empty below 0");
	bad = AGoodState(); bad.ChangeCountdown = kM79ChangeDelay + 1; Check(!M79_OwnStateIsGood(bad), "a countdown over half a second");
	bad = AGoodState(); bad.ChangeCountdown = -1; Check(!M79_OwnStateIsGood(bad), "a countdown below 0");

	for (int kind = 0; kind < kM79Kinds; kind++)
	{
		bad = AGoodState(); bad.RoundsInGun[kind] = 2; Check(!M79_OwnStateIsGood(bad), "two grenades in a gun that holds one");
		bad = AGoodState(); bad.RoundsInGun[kind] = -1; Check(!M79_OwnStateIsGood(bad), "rounds below 0");
		bad = AGoodState(); bad.Spares[kind] = kM79MostSpares + 1; Check(!M79_OwnStateIsGood(bad), "more spares than he can carry");
		bad = AGoodState(); bad.Spares[kind] = -1; Check(!M79_OwnStateIsGood(bad), "spares below 0");
	}
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
	return all.str();
}

static bool Has(const std::string &text, const char *what)
{
	return text.find(what) != std::string::npos;
}

// The sums mean nothing if the game doesn't use them, so read the game's own source.
static void TestWiring()
{
	const std::string gun = ReadSource("3dc/avp/weapon_m79.cpp");
	const std::string row = ReadSource("3dc/avp/equipment.cpp");
	const std::string types = ReadSource("3dc/avp/bh_types.cpp");
	const std::string save = ReadSource("3dc/avp/savegame.cpp");
	const std::string inventory = ReadSource("3dc/avp/inventory.cpp");
	const std::string cheat = ReadSource("3dc/avp/win95/cheat.cpp");
	const std::string hud = ReadSource("3dc/avp/hud.cpp");

	Check(Has(gun, "M79_OwnStateIsGood(*(const M79OwnState *)state)"), "a saved state is checked before it's used");
	Check(Has(gun, "M79_TapChangeKey(Own)"), "the key goes through the sums");
	Check(Has(gun, "M79_ChangeKind(Own, Own.KindToChangeTo)"), "the change goes through the sums");
	Check(Has(gun, "M79_AddPickup(Own, kind)"), "a pickup goes through the sums");
	Check(Has(row, "&M79_Hooks"), "the gun's row points at its hooks");
	Check(Has(row, "FireM79"), "the gun's row fires it");
	Check(Has(row, "\"M79\""), "the gun's row names its model");
	Check(Has(types, "M79GrenadeBehaviour(sbptr)"), "the grenades run");
	Check(Has(save, "LoadStrategy_M79Grenade(header)"), "a grenade in the air loads");
	Check(Has(save, "SaveStrategy_M79Grenade(sbPtr)"), "a grenade in the air saves");
	Check(Has(inventory, "M79_TakeAmmo("), "ammunition can be picked up");
	Check(Has(cheat, "M79_FillEveryKind(wdPtr)"), "the test command fills every kind");
	Check(Has(hud, "twPtr->Hooks->DrawAmmoPanel(playerStatusPtr, weaponPtr);"), "a gun can draw its own ammunition panel");
	Check(Has(gun, "M79_DrawList\r\n};") || Has(gun, "M79_DrawList\n};"), "the list is the M79's ammunition panel");
	Check(Has(gun, "ScreenDescriptorBlock.SDB_Width - margin"), "the list is at the right of the screen");

	// A timer read from a save must be held in range, or a grenade could wait forever
	Check(Has(gun, "block->counter > 2 * ONE_FIXED ? 2 * ONE_FIXED"), "a grenade's timer from a save is held in range");
	// The saved dynamics block goes through the same checks as every object's (Save_RestoreDynamics, save_checks.h), which never let a
	// pointer from a save in (modified 2026-11-11 by Melanikus (Third) for the AvP port, SAVE-HOSTILE-1)
	Check(Has(gun, "Save_RestoreDynamics(sbPtr->DynPtr, &block->dynamics, kSaveHoldsNeitherOff);"), "a saved grenade's dynamics block goes through the checks, so a pointer from a save is never used");
}

int main()
{
	TestChoosingAKind();
	TestTapping();
	TestChanging();
	TestPickups();
	TestSavedStates();
	TestWiring();

	printf("%d checks, %d failed\n", checks, failures);
	return failures == 0 ? 0 : 1;
}
