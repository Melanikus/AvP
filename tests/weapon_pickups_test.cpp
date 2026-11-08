// Tests how a map asks for a pickup of a new gun, in 3dc/avp/weapon_pickups.h, and that the game is wired to it.
// Written 2026-11-06 by Melanikus for the AvP port.
//
// No map has these pickups yet, so what a player sees when he walks over one needs a real map and a real run.

#include "avp/weapon_pickups.h"

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

static void TestNames()
{
	Check(PlacedPickup_FromName("WM79") == kPlacedPickup_M79, "the M79");
	Check(PlacedPickup_FromName("WSNIPER") == kPlacedPickup_SniperRifle, "the sniper rifle");
	Check(PlacedPickup_FromName("WREVOLVER") == kPlacedPickup_Revolver, "the revolver");
	Check(PlacedPickup_FromName("AM79") == kPlacedPickup_M79Grenades, "M79 grenades");
	Check(PlacedPickup_FromName("AM79FIRE") == kPlacedPickup_M79FireGrenades, "M79 fire grenades");
	Check(PlacedPickup_FromName("AM79FRAG") == kPlacedPickup_M79FragGrenades, "M79 frag grenades");
	Check(PlacedPickup_FromName("ASNIPER") == kPlacedPickup_SniperRounds, "sniper rounds");
	Check(PlacedPickup_FromName("AREVOLVER") == kPlacedPickup_RevolverRounds, "revolver rounds");

	// A map's objects need different names, so anything may follow the word
	Check(PlacedPickup_FromName("WM7901") == kPlacedPickup_M79, "the M79, with a number after");
	Check(PlacedPickup_FromName("AM79_B") == kPlacedPickup_M79Grenades, "grenades, with letters after");
	Check(PlacedPickup_FromName("AM79FIRE02") == kPlacedPickup_M79FireGrenades, "fire grenades, with a number after");
	Check(PlacedPickup_FromName("AM79FRAG02") == kPlacedPickup_M79FragGrenades, "frag grenades, with a number after");
	Check(PlacedPickup_FromName("AREVOLVER9") == kPlacedPickup_RevolverRounds, "revolver rounds, with a number after");

	Check(PlacedPickup_FromName(NULL) == kPlacedPickup_None, "no name at all");
	Check(PlacedPickup_FromName("") == kPlacedPickup_None, "an empty name");
	Check(PlacedPickup_FromName("W") == kPlacedPickup_None, "only the first letter");
	Check(PlacedPickup_FromName("A") == kPlacedPickup_None, "only the first letter of ammunition");
	Check(PlacedPickup_FromName("AM7") == kPlacedPickup_None, "a name cut short");
	Check(PlacedPickup_FromName("wm79") == kPlacedPickup_None, "small letters don't count");
	Check(PlacedPickup_FromName("BWM79") == kPlacedPickup_None, "the word has to come first");
	Check(PlacedPickup_FromName("WINDOW") == kPlacedPickup_None, "a real object's name from a stock map");
	Check(PlacedPickup_FromName("WIREY02") == kPlacedPickup_None, "another real object's name");
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
	const std::string setup = ReadSource("3dc/avp/win95/object_setup.cpp");
	const std::string inventory = ReadSource("3dc/avp/inventory.cpp");

	Check(Has(setup, "PlacedPickup_FromName(ob->object_data.o_name)"), "the map's object name is read");
	Check(Has(setup, "if (tdio->typeId == IOT_DataTape)"), "only for objects of the data tape type");
	Check(Has(setup, "if (tdio->typeId == IOT_IRGoggles)"), "IR goggles are the shotgun, as in the old mod");
	Check(Has(setup, "tdio->subType = AMMO_PLAYER_SHOTGUN;"), "ammunition number 1 is for the shotgun");

	Check(Has(inventory, "case AMMO_PLAYER_SHOTGUN:"), "shotgun shells can be picked up");
	Check(Has(inventory, "case AMMO_SNIPER:"), "sniper rounds can be picked up");
	Check(Has(inventory, "case AMMO_REVOLVER:"), "revolver rounds can be picked up");
	Check(Has(inventory, "case AMMO_M79_STANDARD:"), "M79 grenades can be picked up");
}

int main()
{
	TestNames();
	TestWiring();

	printf("%d checks, %d failed\n", checks, failures);
	return failures == 0 ? 0 : 1;
}
