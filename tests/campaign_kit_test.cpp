// The test of what a player keeps between the levels of a co-op campaign. Written 2026-11-07 by Melanikus for the AvP port. Credit: Melanikus and
// Bobblen (the kept kit follows Bobblen's avp-forever, rewritten).
//
// WHAT IT TESTS
//
// 3dc/avp/campaign_kit.h: the rule itself, with made-up kits. And that the game's own files use it the way the rule says, read from
// their source, because a player's real kit needs a whole game around it.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this with the settings in tests/campaign_kit_test.args, and the program gives back a non-zero result if
// any check failed. It needs no game files. tests/takeouts/campaign_kit_test.json proves each check can fail.

// Modified 2026-11-08 by Melanikus for the AvP port:
//   its Between is the shared one in source_slice.h, so a marker that isn't there is a failed check of its own (SOURCE-SLICE-1, #128)
#include <stdio.h>
#include <string.h>

#include <fstream>
#include <sstream>
#include <string>

#include "campaign_kit.h"
#include "source_slice.h"

static int checksFailed = 0;

static void Check(bool good, const std::string &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// A kit with these guns in its first slots, all empty and not carried
static CampaignKit EmptyKit(int guns)
{
	CampaignKit kit;

	memset(&kit, 0, sizeof(kit));
	kit.guns = guns;

	for (int i = 0; i < guns; i++)
	{
		kit.gun[i].weapon = 100 + i;
	}

	return kit;
}

static void TestKeepingAGun()
{
	puts("Keeping a gun:");

	CampaignKit level = EmptyKit(2);
	CampaignKit kept = EmptyKit(2);

	kept.gun[1].possessed = 1;
	kept.gun[1].primaryRounds = 12;
	kept.gun[1].primaryMagazines = 3;
	kept.gun[1].secondaryRounds = 4;
	kept.gun[1].secondaryMagazines = 2;
	kept.gun[1].leftoverRounds = 7;

	CampaignKit_Keep(level, kept);

	Check(level.gun[1].possessed == 1, "a gun carried out of the last level is still carried");
	Check(level.gun[1].primaryRounds == 12 && level.gun[1].primaryMagazines == 3, "with its rounds and magazines");
	Check(level.gun[1].secondaryRounds == 4 && level.gun[1].secondaryMagazines == 2, "and its second kind of ammunition");
	Check(level.gun[1].leftoverRounds == 7, "and the rounds left over from reloading part way through");
	Check(level.gun[0].possessed == 0, "and a gun nobody had or gave stays away");
}

static void TestTheLargerOfEach()
{
	puts("The larger of each:");

	CampaignKit level = EmptyKit(3);
	CampaignKit kept = EmptyKit(3);

	// The level gives a gun they never had
	level.gun[0].possessed = 1;
	level.gun[0].primaryRounds = 50;
	level.gun[0].primaryMagazines = 5;

	// They have more rounds in the gun than the level gives, and fewer magazines. And the other way round for the second kind.
	level.gun[1].possessed = 1;
	level.gun[1].primaryRounds = 99;
	level.gun[1].primaryMagazines = 1;
	level.gun[1].secondaryRounds = 1;
	level.gun[1].secondaryMagazines = 9;
	level.gun[1].leftoverRounds = 8;
	kept.gun[1].possessed = 1;
	kept.gun[1].primaryRounds = 10;
	kept.gun[1].primaryMagazines = 4;
	kept.gun[1].secondaryRounds = 6;
	kept.gun[1].secondaryMagazines = 2;
	kept.gun[1].leftoverRounds = 3;

	// The level gives this one, and they'd lost it
	level.gun[2].possessed = 1;
	kept.gun[2].possessed = 0;

	CampaignKit_Keep(level, kept);

	Check(level.gun[0].possessed == 1 && level.gun[0].primaryRounds == 50 && level.gun[0].primaryMagazines == 5, "a gun the new level adds arrives, with what the level gives");
	Check(level.gun[1].primaryRounds == 99, "the level's rounds where they're more");
	Check(level.gun[1].primaryMagazines == 4, "their magazines where they're more");
	Check(level.gun[1].secondaryRounds == 6, "their second rounds where they're more");
	Check(level.gun[1].secondaryMagazines == 9, "the level's second magazines where they're more");
	Check(level.gun[1].leftoverRounds == 8, "the level's left-over rounds where they're more");
	Check(level.gun[2].possessed == 1, "a gun the level gives isn't taken away because they'd lost it");
}

static void TestGunsNotToMix()
{
	puts("What isn't kept:");

	CampaignKit level = EmptyKit(2);
	CampaignKit kept = EmptyKit(2);

	level.gun[0].possessed = -1;
	kept.gun[0].possessed = 1;
	kept.gun[0].primaryRounds = 12;

	level.gun[1].weapon = 999;
	kept.gun[1].possessed = 1;
	kept.gun[1].primaryRounds = 12;

	CampaignKit_Keep(level, kept);

	Check(level.gun[0].possessed == -1 && level.gun[0].primaryRounds == 0, "a gun this level doesn't allow stays away, with its ammunition");
	Check(level.gun[1].possessed == 0 && level.gun[1].primaryRounds == 0, "a slot holding another gun now doesn't take the old gun's numbers");

	CampaignKit shorter = EmptyKit(1);
	CampaignKit longer = EmptyKit(2);

	longer.gun[1].possessed = 1;
	longer.gun[1].primaryRounds = 12;
	// A slot past the end of the level's list can still hold a number, from before. It's the kept one's here, so only the list's length can
	// keep it out.
	shorter.gun[1].weapon = 101;
	CampaignKit_Keep(shorter, longer);
	Check(shorter.gun[1].possessed == 0 && shorter.gun[1].primaryRounds == 0, "a slot past the end of the level's list is left alone");

	CampaignKit longerLevel = EmptyKit(2);
	CampaignKit shorterKept = EmptyKit(1);

	shorterKept.gun[1].weapon = 101;
	shorterKept.gun[1].possessed = 1;
	CampaignKit_Keep(longerLevel, shorterKept);
	Check(longerLevel.gun[1].possessed == 0, "and so is a slot past the end of what was kept");
}

static void TestTheLauncher()
{
	puts("The grenade launcher's four kinds:");

	CampaignKit level = EmptyKit(0);
	CampaignKit kept = EmptyKit(0);

	for (int kind = 0; kind < kCampaignKitLauncherKinds; kind++)
	{
		level.launcherRounds[kind] = 1;
		level.launcherMagazines[kind] = 9;
		kept.launcherRounds[kind] = 6 + kind;
		kept.launcherMagazines[kind] = kind;
	}

	CampaignKit_Keep(level, kept);

	for (int kind = 0; kind < kCampaignKitLauncherKinds; kind++)
	{
		Check(level.launcherRounds[kind] == (unsigned int)(6 + kind), "kind " + std::to_string(kind) + ": their rounds where they're more");
		Check(level.launcherMagazines[kind] == 9, "kind " + std::to_string(kind) + ": the level's magazines where they're more");
	}

	// And the other way round: the magazines they kept are more, so only the line that keeps them can give them back
	for (int kind = 0; kind < kCampaignKitLauncherKinds; kind++)
	{
		level.launcherMagazines[kind] = 1;
		kept.launcherMagazines[kind] = 5 + kind;
	}

	CampaignKit_Keep(level, kept);

	for (int kind = 0; kind < kCampaignKitLauncherKinds; kind++)
	{
		Check(level.launcherMagazines[kind] == (unsigned int)(5 + kind), "kind " + std::to_string(kind) + ": their magazines where they're more");
	}
}

static void TestWhoGetsAKit()
{
	puts("Who gets a kit back:");

	CampaignKit const kit = EmptyKit(1);
	CampaignKitNote note;

	memset(&note, 0, sizeof(note));
	Check(!CampaignKit_IsFor(note, true, 1, 0), "nothing noted yet means no kit, even for the first level of the first campaign");

	CampaignKit_Note(note, true, 1, 4, kit);
	Check(note.noted && note.campaign == 1 && note.place == 4, "the kit is noted with the level it's for");
	Check(CampaignKit_IsFor(note, true, 1, 4), "a player alive at the end gets it for that level");
	Check(CampaignKit_IsFor(note, true, 1, 4), "and again if that level starts over, because everyone died");
	Check(!CampaignKit_IsFor(note, false, 1, 4), "not with Reset chosen");
	Check(!CampaignKit_IsFor(note, true, 2, 4), "not in another campaign");
	Check(!CampaignKit_IsFor(note, true, 1, 5), "not on a later level");
	Check(!CampaignKit_IsFor(note, true, 1, 3), "not on an earlier level");

	CampaignKit_Note(note, false, 1, 4, kit);
	Check(!CampaignKit_IsFor(note, true, 1, 4), "a player dead at the end gets the level's own kit");
}

static std::string ReadSourceFile(const std::string &name)
{
	std::ifstream file(name, std::ios::binary);
	std::stringstream all;

	all << file.rdbuf();

	std::string text = all.str();
	std::string withoutReturns;

	for (size_t i = 0; i < text.size(); i++)
	{
		if (text[i] != '\r')
		{
			withoutReturns += text[i];
		}
	}

	if (withoutReturns.empty())
	{
		printf("  FAILED  the test couldn't read %s\n", name.c_str());
		checksFailed++;
	}

	return withoutReturns;
}

static bool Has(const std::string &text, const std::string &what)
{
	return text.find(what) != std::string::npos;
}

// The text from one marker up to the next (source_slice.h). A marker that isn't there is a failed check of its own, named by the marker.
static std::string Between(const std::string &text, const std::string &from, const std::string &to)
{
	return SourceSlice(text, from, to, kSliceEndAfterTheStart, [](const std::string &why) { Check(false, why.c_str()); });
}

static void TestTheGameUsesIt()
{
	puts("The game's own files:");

	std::string const glue = ReadSourceFile("3dc/avp/campaign_kit.cpp");
	std::string const inventory = ReadSourceFile("3dc/avp/inventory.cpp");
	std::string const net = ReadSourceFile("3dc/avp/win95/pldnet.cpp");

	Check(Has(glue, "static_assert(MAX_NO_OF_WEAPON_SLOTS <= kCampaignKitMostGuns"), "the build stops if a species can ever have more slots than a kit holds");
	Check(Has(glue, "Campaign_PlayerIsAliveAtTheEnd()"), "a dead player is noted as dead");
	Check(Has(glue, "CampaignKit_IsFor(note, netGameData.coopKeepKit != 0, netGameData.coopCampaign, netGameData.coopCampaignLevel)"), "a kit is only given with Keep chosen, for this level of this campaign");
	Check(Has(glue, "Guns_GiveBackOwnStates()"), "guns that keep ammunition of their own get it back");
	Check(Has(glue, "LoadAllWeapons(playerStatusPtr)"), "a gun kept empty with magazines left starts loaded, as a level's own guns do");

	// It has to be the last thing, or what the level gives would land on top of it. Both a level's start and a start after everyone died come here.
	std::string const inventoryEnd = Between(inventory, "LoadAllWeapons(PlayerStatusPtr);\n", "\n}\n");

	Check(Has(inventoryEnd, "Campaign_GiveKeptKit(playerStatusPtr);"), "the kit is given after everything the level gives");

	std::string const goToPlace = Between(net, "static void Coop_GoToPlace(int place)\n{", "\n}\n");

	Check(goToPlace.find("Campaign_NoteKit(place);") != std::string::npos && goToPlace.find("Campaign_NoteKit(place);") < goToPlace.find("TurnOffMultiplayerObserveMode"),
	      "the kit is noted as a level ends, before watching someone else is turned off");
	Check(Has(goToPlace, "netGameData.myGameState == NGS_Playing"), "and only if a level was really being played");

	Check(Has(Between(net, "void InitAVPNetGameForHost(int species, int gamestyle, int level)\n{", "\n}\n"), "Campaign_ForgetKit();"), "a new game hosted starts with no kit");
	Check(Has(Between(net, "void InitAVPNetGameForJoin(void)\n{", "\n}\n"), "Campaign_ForgetKit();"), "and so does a new game joined");
	Check(Has(Between(net, "void InitAVPNetGameForJoin(void)\n{", "\n}\n"), "netGameData.coopKeepKit = 0;"), "and a joiner keeps nothing until the host says so");
	Check(Has(Between(net, "void EndAVPNetGame(void)\n{", "\n}\n"), "netGameData.coopKeepKit = 0;"), "and a game's end forgets the host's choice");
}

int main()
{
	TestKeepingAGun();
	TestTheLargerOfEach();
	TestGunsNotToMix();
	TestTheLauncher();
	TestWhoGetsAKit();
	TestTheGameUsesIt();

	puts(checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
