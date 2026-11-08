// The test for 3dc/avp/hudson_plan.cpp, which reads what a Hudson run has been asked to do from the command line. Written 2026-11-04 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// The command line comes from outside the game, so nothing on it is believed. Each bad thing here has to be refused for its own reason, and the test names that reason, because a later check could otherwise
// catch what an earlier one missed, and hide that it's missing. And a normal game has to stay a normal game: without both -dev and -hudson, each as a word on its own, no run is asked for.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this, and the program gives back a non-zero result if any check failed. It needs no game files, and it only runs the code that reads the command line.
//
// The last part reads a few of the game's source files, for what can't be run here: that a run in the wrong folder ends before anything is written, that its settings work in a new folder, and that it loads what the
// menus load. Each of those went wrong once. /FC in this test's settings makes __FILE__ the whole path of this file, which is how the source files are found.

#include <stdio.h>
#include <string.h>

#include <fstream>
#include <sstream>
#include <string>

#include "hudson_plan.h"

static int checksFailed = 0;

static void Check(bool good, const char *what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what);

	if (!good)
	{
		checksFailed++;
	}
}

static void Refused(const char *commandLine, const char *reason)
{
	HudsonPlan plan;
	const char *const got = HudsonPlan_Read(commandLine, &plan);
	char words[300];

	_snprintf(words, sizeof(words) - 1, "[%s] is refused: %s", commandLine, reason);
	words[sizeof(words) - 1] = 0;
	Check(got != 0 && strcmp(got, reason) == 0, words);
}

static void TestWhoAsked()
{
	Check(HudsonPlan_IsAskedFor("-dev -hudson level=derelict frames=1"), "both words ask for a run");
	Check(HudsonPlan_IsAskedFor("-hudson level=derelict frames=1 -dev"), "in either order");
	Check(HudsonPlan_IsAskedFor("-w\t-dev\t-hudson"), "with tabs, and other switches beside them");

	Check(!HudsonPlan_IsAskedFor(""), "a normal game asks for no run");
	Check(!HudsonPlan_IsAskedFor(0), "nor does no command line at all");
	Check(!HudsonPlan_IsAskedFor("-w -dev"), "-dev alone asks for no run");
	Check(!HudsonPlan_IsAskedFor("-hudson level=derelict frames=1"), "-hudson without -dev asks for no run");
	Check(!HudsonPlan_IsAskedFor("-dev -hudsonx"), "a longer word isn't -hudson");
	Check(!HudsonPlan_IsAskedFor("-dev x-hudson"), "nor is a word that ends with it");
	Check(!HudsonPlan_IsAskedFor("-developer -hudson"), "a longer word isn't -dev");
}

static void TestGoodPlans()
{
	HudsonPlan plan;

	Check(HudsonPlan_Read("-dev -hudson level=derelict frames=300", &plan) == 0, "the least a run can ask for");
	Check(strcmp(plan.level, "derelict") == 0 && plan.frames == 300, "it reads the level and the frames");
	Check(plan.seconds == kHudsonDefaultSeconds && plan.idleSeconds == kHudsonDefaultIdleSeconds, "and what isn't asked for has its default");

	Check(HudsonPlan_Read("-hudson frames=5 idle=0 seconds=10 level=stat101_m -dev -w", &plan) == 0, "any order, stopping at the next switch");
	Check(strcmp(plan.level, "stat101_m") == 0 && plan.frames == 5 && plan.idleSeconds == 0 && plan.seconds == 10, "and each goes where it should");

	Check(HudsonPlan_Read("-hudson level=invasion_p frames=5 gun=7 species=predator", &plan) == 0 && plan.playAsPredator && plan.gunSlot == 7, "a gun run can play as a Predator");
	Check(HudsonPlan_Read("-hudson level=invasion frames=5 gun=7", &plan) == 0 && !plan.playAsPredator, "and a gun run is a marine unless it asks");
	Check(HudsonPlan_Read("-hudson level=invasion frames=5 gun=7 gunalien=yes", &plan) == 0 && plan.gunAlien, "gunalien=yes gives a gun run something to hit");
	Check(HudsonPlan_Read("-hudson level=invasion frames=5 gun=10 gunmarine=yes", &plan) == 0 && plan.gunMarine, "gunmarine=yes gives a gun run a marine to look at");
	Check(HudsonPlan_Read("-hudson level=invasion frames=5 gun=7", &plan) == 0 && !plan.gunMarine, "and a gun run has none unless it says so");
	Check(HudsonPlan_Read("-hudson level=invasion frames=5 gun=7", &plan) == 0 && !plan.gunAlien, "and a gun run has nothing to hit unless it says so");
	Check(HudsonPlan_Read("-hudson level=invasion frames=5 gun=1 class=shotgun", &plan) == 0 && plan.classSlider == 11, "class=shotgun is the shotgun's place on the class menu");
	Check(HudsonPlan_Read("-hudson level=invasion frames=5 gun=12 class=revolver", &plan) == 0 && plan.classSlider == 12, "class=revolver is the revolver's");
	Check(HudsonPlan_Read("-hudson level=invasion frames=5 gun=13 class=m79", &plan) == 0 && plan.classSlider == 13, "class=m79 is the M79's");
	Check(HudsonPlan_Read("-hudson level=invasion frames=5 gun=7", &plan) == 0 && plan.classSlider == 0, "and a gun run has no class unless it says so");

	Check(HudsonPlan_Read("-hudson level=a frames=1000000 seconds=14400 idle=3600", &plan) == 0, "the highest each can be");
	Check(HudsonPlan_Read("-hudson   level=a\t frames=1  ", &plan) == 0, "extra spaces and tabs are nothing");

	// A longer word that holds -hudson comes first in both of these, and what follows it isn't for a run. What's read has to be what follows the word itself.
	Check(HudsonPlan_Read("-hudsonx level=wrong frames=9 -hudson level=right frames=1", &plan) == 0 && strcmp(plan.level, "right") == 0 && plan.frames == 1, "a word that starts with -hudson isn't where the plan is");
	Check(HudsonPlan_Read("x-hudson level=wrong frames=9 -hudson level=right frames=1", &plan) == 0 && strcmp(plan.level, "right") == 0 && plan.frames == 1, "nor is a word that ends with it");

	char longest[kHudsonLongestLevelName + 1];
	char line[128];

	memset(longest, 'a', kHudsonLongestLevelName);
	longest[kHudsonLongestLevelName] = 0;
	sprintf(line, "-hudson level=%s frames=1", longest);
	Check(HudsonPlan_Read(line, &plan) == 0 && strcmp(plan.level, longest) == 0, "the longest name there's room for");
}

static void TestBadPlans()
{
	Refused("-dev -hudson", kHudsonRefusedNoLevel);
	Refused("-hudson frames=1", kHudsonRefusedNoLevel);
	Refused("-hudson level= frames=1", kHudsonRefusedNoLevel);
	Refused("-hudson level=a", kHudsonRefusedNoFrames);
	Refused("-hudson level=a -w frames=1", kHudsonRefusedNoFrames);

	Refused("-hudson derelict frames=1", kHudsonRefusedNotNameAndValue);
	Refused("-hudson level=a frames=1 extra", kHudsonRefusedNotNameAndValue);
	Refused("-hudson level=a frames=1 weather=3", kHudsonRefusedUnknownName);
	Refused("-hudson =a frames=1", kHudsonRefusedUnknownName);
	Refused("-hudson LEVEL=a frames=1", kHudsonRefusedUnknownName);
	Refused("-hudson levelx=a frames=1", kHudsonRefusedUnknownName);

	Refused("-hudson level=a level=b frames=1", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 frames=2", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 seconds=10 seconds=20", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 idle=1 idle=2", kHudsonRefusedAskedTwice);

	Refused("-hudson level=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa frames=1", kHudsonRefusedLevelTooLong);
	Refused("-hudson level=..\\secret frames=1", kHudsonRefusedLevelCharacter);
	Refused("-hudson level=a/b frames=1", kHudsonRefusedLevelCharacter);
	Refused("-hudson level=a.rif frames=1", kHudsonRefusedLevelCharacter);
	Refused("-hudson level=Derelict.rif frames=1", kHudsonRefusedLevelCharacter);
	Refused("-hudson level=avp_rifs\\Derelict frames=1", kHudsonRefusedLevelCharacter);
	Refused("-hudson level=Custom/Derelict frames=1", kHudsonRefusedLevelCharacter);
	Refused("-hudson level=a=b frames=1", kHudsonRefusedLevelCharacter);
	Refused("-hudson level=a%s frames=1", kHudsonRefusedLevelCharacter);

	Refused("-hudson level=a frames=", kHudsonRefusedFramesNotANumber);
	Refused("-hudson level=a frames=-1", kHudsonRefusedFramesNotANumber);
	Refused("-hudson level=a frames=+1", kHudsonRefusedFramesNotANumber);
	Refused("-hudson level=a frames=1x", kHudsonRefusedFramesNotANumber);
	Refused("-hudson level=a frames=0x10", kHudsonRefusedFramesNotANumber);
	Refused("-hudson level=a frames=1234567890", kHudsonRefusedFramesNotANumber);
	Refused("-hudson level=a frames=99999999999999999999", kHudsonRefusedFramesNotANumber);
	Refused("-hudson level=a frames=0", kHudsonRefusedFramesOutOfRange);
	Refused("-hudson level=a frames=1000001", kHudsonRefusedFramesOutOfRange);

	Refused("-hudson level=a frames=1 seconds=x", kHudsonRefusedSecondsNotANumber);
	Refused("-hudson level=a frames=1 seconds=9", kHudsonRefusedSecondsOutOfRange);
	Refused("-hudson level=a frames=1 seconds=14401", kHudsonRefusedSecondsOutOfRange);

	Refused("-hudson level=a frames=1 idle=x", kHudsonRefusedIdleNotANumber);
	Refused("-hudson level=a frames=1 idle=3601", kHudsonRefusedIdleOutOfRange);

	Refused("-hudson level=a frames=1 rooms=x", kHudsonRefusedRoomsNotANumber);
	Refused("-hudson level=a frames=1 rooms=-1", kHudsonRefusedRoomsNotANumber);
	Refused("-hudson level=a frames=1 rooms=1001", kHudsonRefusedRoomsOutOfRange);
	Refused("-hudson level=a frames=1 machine=nobody", kHudsonRefusedMachineUnknown);
	Refused("-hudson level=a frames=1 machine=shared show=no", kHudsonRefusedShowNotYes);
	Refused("-hudson level=a frames=1 rooms=1 lighting=relief vision=maybe", kHudsonRefusedVisionUnknown);
	Refused("-hudson level=a frames=1 rooms=1 lighting=relief vision=check break=light", kHudsonRefusedBreakUnknown);
	Refused("-hudson level=a frames=1 rooms=1 lamp=dim", kHudsonRefusedLampUnknown);
	Refused("-hudson level=a frames=1 rooms=1 lamp=", kHudsonRefusedLampUnknown);
	Refused("-hudson level=a frames=1 rooms=1 lamp=on lamp=off", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 rooms=1 lampkind=glow", kHudsonRefusedLampKindUnknown);
	Refused("-hudson level=a frames=1 rooms=1 lampkind=real lampkind=real", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 rooms=1 lampkind=real lampat=hat", kHudsonRefusedLampAtUnknown);
	Refused("-hudson level=a frames=1 rooms=1 lampkind=real lampat=wide lampat=wide", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 rooms=1 lampat=wide", kHudsonRefusedLampWordsWithoutReal);
	Refused("-hudson level=a frames=1 rooms=1 lighting=relief vision=check lamp=on", kHudsonRefusedLampSpoilsComparing);
	Refused("-hudson level=a frames=1 rooms=1 lighting=relief vision=check lamp=blink", kHudsonRefusedLampSpoilsComparing);
	Refused("-hudson level=a frames=1 rooms=1 views=3 wall=hive01 wallroom=hole compare=relief record=pictures lighting=relief lamp=on", kHudsonRefusedLampSpoilsComparing);
	Refused("-hudson level=a frames=1 rooms=1 lampbeam=yes", kHudsonRefusedUnknownName);
	Refused("-hudson level=a frames=1 rooms=1 record=video", kHudsonRefusedVideoMeasuresTime);
	Refused("-hudson level=a frames=1 rooms=1 machine=alone record=video", kHudsonRefusedVideoMeasuresTime);
	Refused("-hudson level=a frames=1 machine=shared net=both", kHudsonRefusedNetUnknown);
	Refused("-hudson level=a frames=1 machine=shared net=", kHudsonRefusedNetUnknown);
	Refused("-hudson level=a frames=1 machine=shared net=host net=host", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 net=host", kHudsonRefusedNetMeasuresTime);
	Refused("-hudson level=a frames=1 machine=alone net=join", kHudsonRefusedNetMeasuresTime);
	Refused("-hudson level=a frames=1 rooms=1 machine=shared net=host", kHudsonRefusedNetNotAlone);
	Refused("-hudson level=a frames=1 rooms=1 machine=shared net=join", kHudsonRefusedNetNotAlone);
	Refused("-hudson level=a frames=1 machine=shared net=host strategies=list", kHudsonRefusedNetNotAlone);
	Refused("-hudson level=a frames=1 rooms=1 machine=shared film=no break=video", kHudsonRefusedBreakVideoWithoutVideo);
	Refused("-hudson level=a frames=1 rooms=1 machine=shared film=yes", kHudsonRefusedFilmUnknown);
	Refused("-hudson level=a frames=1 rooms=1 machine=shared film=maybe", kHudsonRefusedFilmUnknown);
	Refused("-hudson level=a frames=1 rooms=1 machine=shared film=", kHudsonRefusedFilmUnknown);
	Refused("-hudson level=a frames=1 rooms=1 machine=shared film=no film=no", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 rooms=1 machine=shared record=video film=no", kHudsonRefusedFilmBothWays);
	Refused("-hudson level=a frames=1 rooms=1 machine=shared record=numbers break=video", kHudsonRefusedBreakVideoWithoutVideo);
	Refused("-hudson level=a frames=1 rooms=1 lighting=relief vision=check bright=no", kHudsonRefusedBrightNotYes);
	Refused("-hudson level=a frames=1 rooms=1 lighting=relief vision=check bright=yes bright=yes", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 rooms=1 lighting=relief bright=yes", kHudsonRefusedBrightWithoutVision);
	Refused("-hudson level=a frames=1 record=pictures", kHudsonRefusedPicturesWithoutRooms);
	Refused("-hudson level=a frames=1 gun=0", kHudsonRefusedGunNotASlot);
	Refused("-hudson level=a frames=1 gun=16", kHudsonRefusedGunNotASlot);
	Refused("-hudson level=a frames=1 gun=x", kHudsonRefusedGunNotASlot);
	Refused("-hudson level=a frames=1 gun=10 gun=10", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 rooms=1 gun=10", kHudsonRefusedGunWithAMarine);
	Refused("-hudson level=a frames=1 gun=7 species=queen", kHudsonRefusedSpeciesNotPredator);
	Refused("-hudson level=a frames=1 gun=7 species=alien", kHudsonRefusedAlienWithGun);
	Refused("-hudson level=a frames=1 species=alien", kHudsonRefusedSpeciesWithoutGun);
	Refused("-hudson level=a frames=1 meets=no", kHudsonRefusedMeetsNotYes);
	Refused("-hudson level=a frames=1 meets=yes meets=yes", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 listplace=x", kHudsonRefusedListPlaceNotANumber);
	Refused("-hudson level=a frames=1 listplace=-1", kHudsonRefusedListPlaceNotANumber);
	Refused("-hudson level=a frames=1 listplace=1000", kHudsonRefusedListPlaceNotANumber);
	Refused("-hudson level=a frames=1 listplace=13 listplace=13", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 listplace=13 species=alien", kHudsonRefusedListPlaceWithSpecies);
	Refused("-hudson level=a frames=1 listplace=13 species=predator", kHudsonRefusedListPlaceWithSpecies);
	Refused("-hudson level=a frames=1 listplace=13 gun=10 class=shotgun", kHudsonRefusedListPlaceWithClass);
	Refused("-hudson level=a frames=1 machine=shared listplace=13 net=host", kHudsonRefusedListPlaceWithNet);
	Refused("-hudson level=a frames=1 dieat=0", kHudsonRefusedDieAtNotANumber);
	Refused("-hudson level=a frames=1 dieat=3601", kHudsonRefusedDieAtNotANumber);
	Refused("-hudson level=a frames=1 dieat=x", kHudsonRefusedDieAtNotANumber);
	Refused("-hudson level=a frames=1 levelfilms=no", kHudsonRefusedLevelFilmsNotYes);
	Refused("-hudson level=a frames=1 levelfilms=yes levelfilms=yes", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 levelfilms=yes filmskip=0", kHudsonRefusedFilmSkipNotANumber);
	Refused("-hudson level=a frames=1 levelfilms=yes filmskip=3601", kHudsonRefusedFilmSkipNotANumber);
	Refused("-hudson level=a frames=1 levelfilms=yes filmskip=x", kHudsonRefusedFilmSkipNotANumber);
	Refused("-hudson level=a frames=1 levelfilms=yes filmskip=1 filmskip=1", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 filmskip=5", kHudsonRefusedFilmSkipWithoutFilms);
	Refused("-hudson level=a frames=1 screen=left", kHudsonRefusedScreenWord);
	Refused("-hudson level=a frames=1 screen=main screen=main", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 windowsof=0", kHudsonRefusedWindowsOf);
	Refused("-hudson level=a frames=1 windowsof=5", kHudsonRefusedWindowsOf);
	Refused("-hudson level=a frames=1 windowsof=x", kHudsonRefusedWindowsOf);
	Refused("-hudson level=a frames=1 windowsof=2 windowsof=2", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 windowslot=4 windowsof=4", kHudsonRefusedWindowSlot);
	Refused("-hudson level=a frames=1 windowslot=x windowsof=4", kHudsonRefusedWindowSlot);
	Refused("-hudson level=a frames=1 windowslot=-1 windowsof=4", kHudsonRefusedWindowSlot);
	Refused("-hudson level=a frames=1 windowslot=2 windowsof=2", kHudsonRefusedWindowSlot);
	Refused("-hudson level=a frames=1 windowslot=1", kHudsonRefusedWindowSlot);
	Refused("-hudson level=a frames=1 windowslot=0 windowslot=0", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 screen=main windowsof=2", kHudsonRefusedWindowPartsOnMain);
	Refused("-hudson level=a frames=1 screen=main windowslot=0", kHudsonRefusedWindowPartsOnMain);
	Refused("-hudson level=a frames=1 screen=main windowsof=2 windowbig=1", kHudsonRefusedWindowPartsOnMain);
	Refused("-hudson level=a frames=1 windowsof=2 windowbig=2", kHudsonRefusedWindowBig);
	Refused("-hudson level=a frames=1 windowsof=2 windowbig=-1", kHudsonRefusedWindowBig);
	Refused("-hudson level=a frames=1 windowsof=2 windowbig=x", kHudsonRefusedWindowBig);
	Refused("-hudson level=a frames=1 windowsof=3 windowbig=1", kHudsonRefusedWindowBig);
	Refused("-hudson level=a frames=1 windowbig=0", kHudsonRefusedWindowBig);
	Refused("-hudson level=a frames=1 windowsof=2 windowbig=1 windowbig=1", kHudsonRefusedAskedTwice);
	Refused("-hudson level=invasion frames=5 machine=shared net=host coop=yes siege=", kHudsonRefusedSiegeEmpty);
	Refused("-hudson level=invasion frames=5 machine=shared net=host coop=yes siege=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", kHudsonRefusedSiegeTooLong);
	Refused("-hudson level=invasion frames=5 machine=shared net=host coop=yes siege=a.b", kHudsonRefusedSiegeCharacter);
	Refused("-hudson level=invasion frames=5 machine=shared net=host coop=yes siege=a siege=a", kHudsonRefusedAskedTwice);
	Refused("-hudson level=invasion frames=5 siege=base", kHudsonRefusedSiegeNeedsAPair);
	Refused("-hudson level=invasion frames=5 machine=shared net=host siege=base", kHudsonRefusedSiegeNeedsAPair);
	Refused("-hudson level=invasion frames=5 machine=shared net=host coop=yes botfight=marine siege=base", kHudsonRefusedSiegeNeedsAPair);
	Refused("-hudson level=invasion frames=5 machine=shared net=host coop=yes siege=a siegefrom=", kHudsonRefusedSiegeFromEmpty);
	Refused("-hudson level=invasion frames=5 machine=shared net=host coop=yes siege=a siegefrom=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", kHudsonRefusedSiegeFromTooLong);
	Refused("-hudson level=invasion frames=5 machine=shared net=host coop=yes siege=a siegefrom=a.b", kHudsonRefusedSiegeFromCharacter);
	Refused("-hudson level=invasion frames=5 machine=shared net=host coop=yes siege=a siegefrom=a siegefrom=a", kHudsonRefusedAskedTwice);
	Refused("-hudson level=invasion frames=5 machine=shared net=host coop=yes siegefrom=a", kHudsonRefusedSiegeFromWithoutSiege);
	{
		HudsonPlan plan;

		Check(HudsonPlan_Read("-hudson level=derelict frames=5 custommap=My_map", &plan) == 0 && strcmp(plan.customMap, "My_map") == 0, "custommap= keeps a name a custom map can have");
	}

	Refused("-hudson level=derelict frames=5 custommap=", kHudsonRefusedCustomMapEmpty);
	Refused("-hudson level=derelict frames=5 custommap=..\\Maps", kHudsonRefusedCustomMapName);
	Refused("-hudson level=derelict frames=5 custommap=a.b", kHudsonRefusedCustomMapName);
	Refused("-hudson level=derelict frames=5 custommap=CON", kHudsonRefusedCustomMapName);
	Refused("-hudson level=derelict frames=5 custommap=1234567890123456789012345678901234567890", kHudsonRefusedCustomMapName);
	Refused("-hudson level=derelict frames=5 custommap=a custommap=b", kHudsonRefusedAskedTwice);
	Refused("-hudson level=invasion frames=5 machine=shared net=host coop=yes siege=a siegefar=", kHudsonRefusedSiegeFarEmpty);
	Refused("-hudson level=invasion frames=5 machine=shared net=host coop=yes siege=a siegefar=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", kHudsonRefusedSiegeFarTooLong);
	Refused("-hudson level=invasion frames=5 machine=shared net=host coop=yes siege=a siegefar=a.b", kHudsonRefusedSiegeFarCharacter);
	Refused("-hudson level=invasion frames=5 machine=shared net=host coop=yes siege=a siegefar=a siegefar=a", kHudsonRefusedAskedTwice);
	Refused("-hudson level=invasion frames=5 machine=shared net=host coop=yes siegefar=a", kHudsonRefusedSiegeFarWithoutSiege);
	Refused("-hudson level=invasion frames=5 machine=shared net=host coop=yes look=maybe", kHudsonRefusedLookNotYesOrNo);
	Refused("-hudson level=invasion frames=5 machine=shared net=host coop=yes look=yes look=no", kHudsonRefusedAskedTwice);
	Refused("-hudson level=invasion frames=5 look=yes", kHudsonRefusedLookNeedsAPair);
	Refused("-hudson level=invasion frames=5 machine=shared net=host look=no", kHudsonRefusedLookNeedsAPair);
	Refused("-hudson level=invasion frames=5 machine=shared net=host coop=yes coopwalk=yes look=yes", kHudsonRefusedLookNeedsAPair);
	Refused("-hudson level=invasion frames=5 machine=shared net=host coop=yes hostile=yes look=yes", kHudsonRefusedLookNeedsAPair);
	Refused("-hudson level=a frames=1 dieat=5 dieat=5", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 difficulty=impossible", kHudsonRefusedDifficultyUnknown);
	Refused("-hudson level=a frames=1 difficulty=", kHudsonRefusedDifficultyUnknown);
	Refused("-hudson level=a frames=1 difficulty=hard difficulty=hard", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 coopend=5", kHudsonRefusedCoopWithoutNet);
	{
		HudsonPlan plan;
		Check(HudsonPlan_Read("-hudson level=a frames=1 listplace=13", &plan) == 0 && plan.listPlaceAsked && plan.listPlace == 13 && plan.meets, "listplace= is read, and it says what he meets");
		Check(HudsonPlan_Read("-hudson level=a frames=1 meets=yes species=alien", &plan) == 0 && plan.playAsAlien && plan.meets && !plan.listPlaceAsked, "an alien can be asked for, to say what he meets");
		Check(HudsonPlan_Read("-hudson level=a frames=1", &plan) == 0 && plan.difficulty == 1 && plan.dieAtSeconds == 0, "Medium, and he doesn't die, unless asked");
		Check(HudsonPlan_Read("-hudson level=a frames=1 difficulty=easy", &plan) == 0 && plan.difficulty == 0, "easy is the game's 0");
		Check(HudsonPlan_Read("-hudson level=a frames=1 difficulty=medium", &plan) == 0 && plan.difficulty == 1, "medium is 1");
		Check(HudsonPlan_Read("-hudson level=a frames=1 difficulty=hard", &plan) == 0 && plan.difficulty == 2, "hard is 2");
		Check(HudsonPlan_Read("-hudson level=a frames=1 dieat=12", &plan) == 0 && plan.dieAtSeconds == 12, "dieat= is read");
		Check(HudsonPlan_Read("-hudson level=a frames=1", &plan) == 0 && !plan.levelFilms && plan.filmSkipSeconds == 0, "a run skips a level's films unless asked");
		Check(HudsonPlan_Read("-hudson level=a frames=1 levelfilms=yes filmskip=7", &plan) == 0 && plan.levelFilms && plan.filmSkipSeconds == 7, "levelfilms= and filmskip= are read");
		Check(HudsonPlan_Read("-hudson level=a frames=1", &plan) == 0 && !plan.lookAllTheTime, "a run that isn't a pair doesn't go on looking");
		Check(HudsonPlan_Read("-hudson level=invasion frames=5 machine=shared net=host", &plan) == 0 && !plan.lookAllTheTime, "nor does a pair with no campaign");
		Check(HudsonPlan_Read("-hudson level=invasion frames=5 machine=shared net=join coop=yes", &plan) == 0 && !plan.lookAllTheTime, "a pair with no fight only has its smoke test");
		Check(HudsonPlan_Read("-hudson level=invasion frames=5 machine=shared net=host coop=yes botfight=marine coopwalk=yes", &plan) == 0 && !plan.lookAllTheTime, "a walk turns him itself");
		Check(HudsonPlan_Read("-hudson level=invasion frames=5 machine=shared net=host coop=yes botfight=marine", &plan) == 0 && plan.lookAllTheTime, "a fight goes on looking");
		Check(HudsonPlan_Read("-hudson level=invasion frames=5 machine=shared net=host coop=yes botfight=marine look=no", &plan) == 0 && !plan.lookAllTheTime, "unless it's told not to");
		Check(HudsonPlan_Read("-hudson level=invasion frames=5 machine=shared net=host coop=yes look=yes", &plan) == 0 && plan.lookAllTheTime, "and any pair on the campaign can be told to");
		{
			HudsonSmokeFacts facts;
			const char *why = "";

			memset(&facts, 0, sizeof(facts));
			Check(HudsonPlan_SmokeVerdict(facts, &why) == kHudsonSmokeWaiting, "the smoke test waits for a body that hasn't come");
			facts.millisecondsWaiting = kHudsonSmokeMostWaitForABody - 1;
			Check(HudsonPlan_SmokeVerdict(facts, &why) == kHudsonSmokeWaiting, "as long as a joiner can be loading");
			facts.millisecondsWaiting = kHudsonSmokeMostWaitForABody;
			Check(HudsonPlan_SmokeVerdict(facts, &why) == kHudsonSmokeFailed && strcmp(why, "the other player's body never came") == 0, "and no longer");
			facts.bodyHere = true;
			facts.facing = true;
			facts.drawn = true;
			facts.wayClear = true;
			facts.millisecondsSeen = kHudsonSmokeLeastLook - 1;
			Check(HudsonPlan_SmokeVerdict(facts, &why) == kHudsonSmokeWaiting, "they look at each other for a while, even if it's all there at once");
			facts.millisecondsSeen = kHudsonSmokeLeastLook;
			Check(HudsonPlan_SmokeVerdict(facts, &why) == kHudsonSmokePassed && why[0] == 0, "and then it passes");
			facts.facing = false;
			Check(HudsonPlan_SmokeVerdict(facts, &why) == kHudsonSmokeWaiting, "it's given time for what isn't true yet");
			facts.millisecondsSeen = kHudsonSmokeMostLook;
			Check(HudsonPlan_SmokeVerdict(facts, &why) == kHudsonSmokeFailed && strcmp(why, "he never came to face the other player") == 0, "but not for ever, and it says why");
			facts.facing = true;
			facts.drawn = false;
			Check(HudsonPlan_SmokeVerdict(facts, &why) == kHudsonSmokeFailed && strcmp(why, "the other player's body never came up on the screen") == 0, "a body that isn't drawn fails it");
			facts.drawn = true;
			facts.wayClear = false;
			Check(HudsonPlan_SmokeVerdict(facts, &why) == kHudsonSmokeFailed && strcmp(why, "something stands between them") == 0, "and so does a wall between them");
			facts.wayClear = true;
			facts.dead = true;
			facts.millisecondsSeen = 1;
			Check(HudsonPlan_SmokeVerdict(facts, &why) == kHudsonSmokeFailed && strcmp(why, "he died before it was done") == 0, "dying fails it at once");
		}
		Check(HudsonPlan_Read("-hudson level=a frames=1", &plan) == 0 && !plan.windowOnMainScreen && plan.windowSlot == 0 && plan.windowsTogether == 1, "a run's window goes on the second screen unless it says otherwise, alone there");
		Check(HudsonPlan_Read("-hudson level=a frames=1 screen=main", &plan) == 0 && plan.windowOnMainScreen, "screen=main keeps it on the main screen");
		Check(HudsonPlan_Read("-hudson level=a frames=1 screen=second", &plan) == 0 && !plan.windowOnMainScreen, "and screen=second is the same as saying nothing");
		Check(HudsonPlan_Read("-hudson level=a frames=1 windowslot=3 windowsof=4", &plan) == 0 && plan.windowSlot == 3 && plan.windowsTogether == 4, "windowslot= and windowsof= are read");
		{
			int left = 0, top = 0, width = 1936, height = 1119;

			HudsonPlan_WindowPlace(-2560, 0, 1707, 1019, 0, 2, &left, &top, &width, &height);
			Check(left == -2560 && top == 0 && width == 853 && height == 493, "the host of a pair has the left half, made smaller and the same shape");
			width = 1936;
			height = 1119;
			HudsonPlan_WindowPlace(-2560, 0, 1707, 1019, 1, 2, &left, &top, &width, &height);
			Check(left == -1707 && top == 0 && width == 853 && height == 493, "and the joiner has the right half");
			width = 1936;
			height = 1119;
			HudsonPlan_WindowPlace(-2560, 0, 1707, 1019, 2, 3, &left, &top, &width, &height);
			Check(left == -2560 && top == 509 && width == 853 && height == 493, "a third game goes below the first");
			width = 1936;
			height = 1119;
			HudsonPlan_WindowPlace(-2560, 0, 1707, 1019, 3, 4, &left, &top, &width, &height);
			Check(left == -1707 && top == 509, "and a fourth beside it");
			width = 1296;
			height = 759;
			HudsonPlan_WindowPlace(-2560, 0, 1707, 1019, 0, 1, &left, &top, &width, &height);
			Check(left == -2560 && top == 0 && width == 1296 && height == 759, "a window that fits stays the size it is");
			width = 1000;
			height = 1000;
			HudsonPlan_WindowPlace(0, 0, 1707, 509, 0, 1, &left, &top, &width, &height);
			Check(width == 509 && height == 509, "if it's too tall, its height decides");
			width = -5;
			height = -5;
			HudsonPlan_WindowPlace(-2560, 0, 1707, 1019, 1, 2, &left, &top, &width, &height);
			Check(width == 853 && height == 1019, "a window that doesn't know its size yet has the whole of its part");
		}
		Check(HudsonPlan_Read("-hudson level=a frames=1 windowslot=1 windowsof=2 windowbig=1", &plan) == 0 && plan.windowBig == 1, "windowbig= is read");
		Check(HudsonPlan_Read("-hudson level=a frames=1", &plan) == 0 && plan.windowBig == -1, "and without it, neither window is the big one");
		{
			int left = 0, top = 0, width = 0, height = 0;

			HudsonPlan_WindowPart(-2560, 0, 1707, 1019, true, &left, &top, &width, &height);
			Check(left == -2560 && top == 0 && width == 1280 && height == 1019, "the big part is the left three quarters, the whole height");
			HudsonPlan_WindowPart(-2560, 0, 1707, 1019, false, &left, &top, &width, &height);
			Check(left == -1280 && top == 0 && width == 427 && height == 1019, "and the small part is the rest, beside it");

			int windowLeft = 0, windowTop = 0, windowWidth = 1936, windowHeight = 1119;

			HudsonPlan_WindowPart(-2560, 0, 1707, 1019, true, &left, &top, &width, &height);
			HudsonPlan_WindowPlace(left, top, width, height, 0, 1, &windowLeft, &windowTop, &windowWidth, &windowHeight);
			Check(windowLeft == -2560 && windowWidth == 1280 && windowHeight == 739, "a 1920x1080 window in the big part is made 1280 wide on this screen, which is about 1920 real pixels");
			windowWidth = 2576;
			windowHeight = 1479;
			HudsonPlan_WindowPlace(left, top, width, height, 0, 1, &windowLeft, &windowTop, &windowWidth, &windowHeight);
			Check(windowLeft == -2560 && windowWidth == 1280 && windowHeight == 734, "and so is a 2560x1440 one, which goes on drawing at its own size");
			windowWidth = 1296;
			windowHeight = 759;
			HudsonPlan_WindowPart(-2560, 0, 1707, 1019, false, &left, &top, &width, &height);
			HudsonPlan_WindowPlace(left, top, width, height, 0, 1, &windowLeft, &windowTop, &windowWidth, &windowHeight);
			Check(windowLeft == -1280 && windowWidth == 427 && windowHeight == 250, "and a 1280x720 window in the small part is a quarter of the screen wide");
		}
		Check(HudsonPlan_Read("-hudson level=kens-co-op frames=5 machine=shared net=join coop=yes siege=base", &plan) == 0 && strcmp(plan.siegeRoom, "base") == 0, "siege= is read");
		Check(plan.noFriendlyFire, "and in a siege their shots are for the enemy");
		Check(HudsonPlan_PlayersCannotDie(plan), "and its players can't be hurt");
		Check(HudsonPlan_Read("-hudson level=a frames=1", &plan) == 0 && !HudsonPlan_PlayersCannotDie(plan), "but a plain run's players can");
		Check(HudsonPlan_Read("-hudson level=invasion frames=5 machine=shared net=host coop=yes botfight=marine", &plan) == 0 && !HudsonPlan_PlayersCannotDie(plan), "and so can a pair's without a siege");
		{
			HudsonPlan nothingAsked;

			memset(&nothingAsked, 0, sizeof(nothingAsked));
			Check(!HudsonPlan_PlayersCannotDie(nothingAsked), "and a plan nobody filled in, as a game that isn't a run has, never turns a blow away");
		}
		Check(HudsonPlan_Read("-hudson level=kens-co-op frames=5 machine=shared net=join coop=yes siege=base siegefrom=canyon_01", &plan) == 0 && strcmp(plan.siegeFrom, "canyon_01") == 0, "siegefrom= is read");
		Check(HudsonPlan_Read("-hudson level=kens-co-op frames=5 machine=shared net=join coop=yes siege=base siegefar=canyon_02", &plan) == 0 && strcmp(plan.siegeFar, "canyon_02") == 0, "siegefar= is read");
		Check(!plan.lookAllTheTime, "and it aims for itself, so nothing else looks");
		{
			static const int kWide = 200, kTall = 40;
			static unsigned char picture[kWide * kTall * 4 + 8];
			int const pitch = kWide * 4;
			unsigned char *const at = picture;
			#define PIXEL_IS_LIT(x, y) (at[(y) * pitch + (x) * 4] == 255)

			HudsonPlan_StampNumber(picture, pitch, kWide, kTall, 0);
			Check(PIXEL_IS_LIT(8, 8) && PIXEL_IS_LIT(16, 8) && !PIXEL_IS_LIT(12, 12), "a stamped 0 has its corners lit, and its middle isn't");
			HudsonPlan_StampNumber(picture, pitch, kWide, kTall, 1000000000UL);
			Check(!PIXEL_IS_LIT(8, 8) && PIXEL_IS_LIT(12, 8), "the highest figure of a number comes first: a 1 is its middle column");
			HudsonPlan_StampNumber(picture, pitch, kWide, kTall, 9UL);
			Check(PIXEL_IS_LIT(152, 12) && !PIXEL_IS_LIT(156, 12) && PIXEL_IS_LIT(160, 12), "and the lowest comes last: a 9's second row has a gap in it");
			#undef PIXEL_IS_LIT

			memset(picture, 77, sizeof(picture));
			HudsonPlan_StampNumber(picture, 40, 10, 10, 1234567890UL);
			Check(picture[9 * 40 + 9 * 4 + 4] == 77 && picture[8 * 40 + 8 * 4] != 77, "it only writes where the picture has room, across");
			Check(picture[10 * 40 + 8 * 4] == 77, "and down");
			{
				unsigned long readBack = 0;

				memset(picture, 77, sizeof(picture));
				HudsonPlan_StampNumber(picture, pitch, kWide, kTall, 1762043219UL);
				Check(HudsonPlan_ReadStampedNumber(picture, pitch, kWide, kTall, &readBack) && readBack == 1762043219UL, "a stamped clock reads back as it was written");

				// A film blurs it: every lit pixel a little darker and every dark one a little lighter still reads
				for (size_t i = 0; i < sizeof(picture); i++)
				{
					picture[i] = picture[i] > 127 ? (unsigned char)200 : (unsigned char)60;
				}

				Check(HudsonPlan_ReadStampedNumber(picture, pitch, kWide, kTall, &readBack) && readBack == 1762043219UL, "and still does after a film has blurred it");
				memset(picture, 0, sizeof(picture));
				Check(!HudsonPlan_ReadStampedNumber(picture, pitch, kWide, kTall, &readBack), "a picture with no clock in its corner doesn't read as one");
				// A clock really there, but the picture is said to be smaller than it is: reading it would go past the picture's edge
				HudsonPlan_StampNumber(picture, pitch, kWide, kTall, 1762043219UL);
				Check(!HudsonPlan_ReadStampedNumber(picture, pitch, 50, 10, &readBack), "nor does one too small to hold it");
			}
			memset(picture, 77, sizeof(picture));
			HudsonPlan_StampNumber(picture, 39, 10, 10, 0);
			Check(picture[8 * 39 + 8 * 4] == 77, "and a picture whose rows are too short for its width isn't touched");
		}
		Check(HudsonPlan_LookStep(0, 100, 23) == 13, "a look moves an eighth of what's left, and one more");
		Check(HudsonPlan_LookStep(0, 1024, 23) == 23, "but never more than this frame's most");
		Check(HudsonPlan_LookStep(4090, 10, 100) == 4093, "it goes the short way round, across the top of the circle");
		Check(HudsonPlan_LookStep(10, 4090, 100) == 7, "and back the other way");
		Check(HudsonPlan_LookStep(0, 2049, 23) == 4073, "just past halfway is shorter going backwards");
		Check(HudsonPlan_LookStep(100, 100, 23) == 100, "and it stays still once it's there");
		Check(HudsonPlan_Read("-hudson level=a frames=1 listplace=13 coopend=5", &plan) == 0 && plan.coopEndSeconds == 5, "and a skirmish from the level row can reach a level's end");
	}
	Refused("-hudson level=a frames=1 gun=7 species=", kHudsonRefusedSpeciesNotPredator);
	Refused("-hudson level=a frames=1 gun=7 species=Predator", kHudsonRefusedSpeciesNotPredator);
	Refused("-hudson level=a frames=1 gun=7 species=predator species=predator", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 species=predator", kHudsonRefusedSpeciesWithoutGun);
	Refused("-hudson level=a frames=1 gun=7 gunalien=no", kHudsonRefusedGunAlienNotYes);
	Refused("-hudson level=a frames=1 gun=7 gunalien=", kHudsonRefusedGunAlienNotYes);
	Refused("-hudson level=a frames=1 gun=7 gunalien=yes gunalien=yes", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 gunalien=yes", kHudsonRefusedGunAlienWithoutGun);
	Refused("-hudson level=a frames=1 gun=7 gunmarine=no", kHudsonRefusedGunMarineNotYes);
	Refused("-hudson level=a frames=1 gun=7 gunmarine=", kHudsonRefusedGunMarineNotYes);
	Refused("-hudson level=a frames=1 gun=7 gunmarine=yes gunmarine=yes", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 gunmarine=yes", kHudsonRefusedGunMarineWithoutGun);
	Refused("-hudson level=a frames=1 gun=1 class=sniper", kHudsonRefusedClassNotOne);
	Refused("-hudson level=a frames=1 gun=1 class=", kHudsonRefusedClassNotOne);
	Refused("-hudson level=a frames=1 gun=1 class=Shotgun", kHudsonRefusedClassNotOne);
	Refused("-hudson level=a frames=1 gun=1 class=shotgun class=shotgun", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 class=shotgun", kHudsonRefusedClassWithoutGun);
	Refused("-hudson level=a frames=1 gun=7 species=predator class=m79", kHudsonRefusedClassAsPredator);
	Refused("-hudson level=a frames=1 rooms=1 flare=no", kHudsonRefusedFlareNotYes);
	Refused("-hudson level=a frames=1 rooms=1 flare=", kHudsonRefusedFlareNotYes);
	Refused("-hudson level=a frames=1 rooms=1 flare=yes flare=yes", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 rooms=1 lighting=relief vision=check flare=yes", kHudsonRefusedFlareSpoilsComparing);
	Refused("-hudson level=a frames=1 rooms=1 views=3 wall=hive01 wallroom=hole compare=relief record=pictures lighting=relief flare=yes", kHudsonRefusedFlareSpoilsComparing);
	Refused("-hudson level=a frames=1 flare=yes", kHudsonRefusedFlareWithoutHolds);
	Refused("-hudson level=a frames=1 rooms=1 combat=alien flare=yes", kHudsonRefusedFlareWithoutHolds);
	Refused("-hudson level=a frames=1 rooms=1 nav=3 flare=yes", kHudsonRefusedFlareWithoutHolds);
	Refused("-hudson level=a frames=1 rooms=1 route=a flare=yes", kHudsonRefusedFlareWithoutHolds);
	Refused("-hudson level=a frames=1 rooms=1 strategy=a flare=yes", kHudsonRefusedFlareWithoutHolds);
	Refused("-hudson level=a frames=1 rooms=1 strategies=list flare=yes", kHudsonRefusedFlareWithoutHolds);
	Refused("-hudson level=a frames=1 rooms=1 lighting=relief vision=check break=prisms break=prisms", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 rooms=1 lighting=relief break=prisms", kHudsonRefusedBreakWithoutVision);
	Refused("-hudson level=a frames=1 rooms=1 lighting=relief vision=check vision=check", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 lighting=relief vision=check", kHudsonRefusedVisionWithoutRooms);
	Refused("-hudson level=a frames=1 rooms=1 vision=check", kHudsonRefusedVisionWithoutPictures);
	Refused("-hudson level=a frames=1 rooms=1 lighting=gouraud vision=check", kHudsonRefusedVisionWithoutPictures);
	Refused("-hudson level=a frames=1 rooms=1 lighting=relief record=pictures compare=relief vision=check", kHudsonRefusedVisionWithoutViews);
	Refused("-hudson level=a frames=1 rooms=1 lighting=relief strategy=a vision=check", kHudsonRefusedVisionWithoutViews);
	Refused("-hudson level=a frames=1 rooms=1 lighting=relief nav=3 vision=check", kHudsonRefusedVisionWithoutViews);
	Refused("-hudson level=a frames=1 rooms=1 lighting=relief route=a vision=check", kHudsonRefusedVisionWithoutViews);
	Refused("-hudson level=a frames=1 size=", kHudsonRefusedSizeUnknown);
	Refused("-hudson level=a frames=1 size=1x1", kHudsonRefusedSizeUnknown);
	Refused("-hudson level=a frames=1 size=1920", kHudsonRefusedSizeUnknown);
	Refused("-hudson level=a frames=1 size=1920x1080x", kHudsonRefusedSizeUnknown);
	Refused("-hudson level=a frames=1 size=-1920x1080", kHudsonRefusedSizeUnknown);
	Refused("-hudson level=a frames=1 size=99999999999999x1", kHudsonRefusedSizeUnknown);
	Refused("-hudson level=a frames=1 size=1920x1080 size=1920x1080", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 machine=shared show=yes show=yes", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 show=yes", kHudsonRefusedShowWithoutSharing);
	Refused("-hudson level=a frames=1 machine=alone show=yes", kHudsonRefusedShowWithoutSharing);
	Refused("-hudson level=a frames=1 rooms=2 strategy=a", kHudsonRefusedOneJobAtATime);
	Refused("-hudson level=a frames=1 rooms=1 strategy=a combat=alien", kHudsonRefusedOneJobAtATime);
	Refused("-hudson level=a frames=1 rooms=1 strategy=a nav=3", kHudsonRefusedOneJobAtATime);
	Refused("-hudson level=a frames=1 rooms=1 combat=alien nav=3", kHudsonRefusedOneJobAtATime);
	Refused("-hudson level=a frames=1 rooms=1 combat=alien route=a", kHudsonRefusedOneJobAtATime);
	Refused("-hudson level=a frames=1 rooms=1 strategy=a route=a", kHudsonRefusedOneJobAtATime);
	Refused("-hudson level=a frames=1 rooms=1 holdseconds=x", kHudsonRefusedHoldSecondsNotANumber);
	Refused("-hudson level=a frames=1 rooms=1 holdseconds=0", kHudsonRefusedHoldSecondsOutOfRange);
	Refused("-hudson level=a frames=1 rooms=1 holdseconds=3601", kHudsonRefusedHoldSecondsOutOfRange);
	Refused("-hudson level=a frames=1 rooms=1 holdseconds=3 holdseconds=3", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 rooms=1 hold=100 holdseconds=3", kHudsonRefusedHoldTwoWays);
	Refused("-hudson level=a frames=1 holdseconds=3", kHudsonRefusedHoldWithoutRooms);
	Refused("-hudson level=a frames=1 rooms=2 tourfrom=", kHudsonRefusedTourFromEmpty);
	Refused("-hudson level=a frames=1 rooms=2 tourfrom=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", kHudsonRefusedTourFromTooLong);
	Refused("-hudson level=a frames=1 rooms=2 tourfrom=a.b", kHudsonRefusedTourFromCharacter);
	Refused("-hudson level=a frames=1 rooms=2 tourfrom=a tourfrom=a", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 rooms=1 tourfrom=a", kHudsonRefusedTourFromWithoutATour);
	Refused("-hudson level=a frames=1 steam=no", kHudsonRefusedSteamNotYes);
	Refused("-hudson level=a frames=1 steam=yes steam=yes", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 machine=shared machine=shared", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 rooms=2 wall=a", kHudsonRefusedWallInManyRooms);
	Refused("-hudson level=a frames=1 rooms=1 rooms=1", kHudsonRefusedAskedTwice);

	Refused("-hudson level=a frames=1 rooms=1 views=x", kHudsonRefusedViewsNotANumber);
	Refused("-hudson level=a frames=1 rooms=1 views=0", kHudsonRefusedViewsOutOfRange);
	Refused("-hudson level=a frames=1 rooms=1 views=9", kHudsonRefusedViewsOutOfRange);
	Refused("-hudson level=a frames=1 rooms=1 views=2 views=2", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 views=2", kHudsonRefusedViewsWithoutRooms);
	Refused("-hudson level=a frames=1 rooms=0 views=2", kHudsonRefusedViewsWithoutRooms);

	Refused("-hudson level=a frames=1 rooms=1 hold=", kHudsonRefusedHoldNotANumber);
	Refused("-hudson level=a frames=1 rooms=1 hold=x", kHudsonRefusedHoldNotANumber);
	Refused("-hudson level=a frames=1 rooms=1 hold=29", kHudsonRefusedHoldOutOfRange);
	Refused("-hudson level=a frames=1 rooms=1 hold=100001", kHudsonRefusedHoldOutOfRange);
	Refused("-hudson level=a frames=1 rooms=1 nav=x", kHudsonRefusedNavNotANumber);
	Refused("-hudson level=a frames=1 rooms=1 nav=", kHudsonRefusedNavNotANumber);
	Refused("-hudson level=a frames=1 rooms=1 nav=0", kHudsonRefusedNavOutOfRange);
	Refused("-hudson level=a frames=1 rooms=1 nav=1001", kHudsonRefusedNavOutOfRange);
	Refused("-hudson level=a frames=1 rooms=1 nav=1 nav=1", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 nav=1", kHudsonRefusedNavWithoutOneRoom);
	Refused("-hudson level=a frames=1 rooms=2 nav=1", kHudsonRefusedNavWithoutOneRoom);
	Refused("-hudson level=a frames=1 rooms=1 nav=1 wall=hive01", kHudsonRefusedNavWithWall);
	Refused("-hudson level=a frames=1 rooms=1 route=", kHudsonRefusedRouteEmpty);
	Refused("-hudson level=a frames=1 rooms=1 route=123456789012345678901234567890123", kHudsonRefusedRouteTooLong);
	Refused("-hudson level=a frames=1 rooms=1 route=..\\up", kHudsonRefusedRouteCharacter);
	Refused("-hudson level=a frames=1 rooms=1 route=a/b", kHudsonRefusedRouteCharacter);
	Refused("-hudson level=a frames=1 rooms=1 route=a.txt", kHudsonRefusedRouteCharacter);
	Refused("-hudson level=a frames=1 rooms=1 route=a route=b", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 route=a", kHudsonRefusedRouteWithoutOneRoom);
	Refused("-hudson level=a frames=1 rooms=2 route=a", kHudsonRefusedRouteWithoutOneRoom);
	Refused("-hudson level=a frames=1 rooms=1 route=a wall=hive01", kHudsonRefusedRouteWithWall);
	Refused("-hudson level=a frames=1 rooms=1 nav=1 pace=", kHudsonRefusedPaceUnknown);
	Refused("-hudson level=a frames=1 rooms=1 nav=1 pace=crawl", kHudsonRefusedPaceUnknown);
	Refused("-hudson level=a frames=1 rooms=1 nav=1 pace=runs", kHudsonRefusedPaceUnknown);
	Refused("-hudson level=a frames=1 rooms=1 nav=1 pace=run pace=walk", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 rooms=1 pace=run", kHudsonRefusedPaceWithoutWalking);
	Refused("-hudson level=a frames=1 rooms=1 nav=1 framehold=on", kHudsonRefusedFrameHoldNotOff);
	Refused("-hudson level=a frames=1 rooms=1 nav=1 framehold=", kHudsonRefusedFrameHoldNotOff);
	Refused("-hudson level=a frames=1 rooms=1 nav=1 framehold=off framehold=off", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 rooms=1 framehold=off", kHudsonRefusedFrameHoldWithoutWalking);
	{
		HudsonPlan holdPlan;

		Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 nav=1", &holdPlan) == 0 && !holdPlan.frameHoldOff, "a walk is held at 60 frames a second unless it's told otherwise");
		Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 route=a framehold=off", &holdPlan) == 0 && holdPlan.frameHoldOff, "and it can be told otherwise");
	}
	{
		HudsonPlan pacePlan;

		Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 nav=1", &pacePlan) == 0 && !pacePlan.paceRun, "he walks unless he's told otherwise");
		Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 nav=1 pace=walk", &pacePlan) == 0 && !pacePlan.paceRun, "and when he's told to walk");
		Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 route=a pace=run", &pacePlan) == 0 && pacePlan.paceRun, "and he runs when he's told to");
	}
	{
		HudsonPlan routePlan;

		Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 route=12345678901234567890123456789012", &routePlan) == 0 && strcmp(routePlan.route, "12345678901234567890123456789012") == 0, "a route's name as long as it can be is read whole");
		Check(routePlan.navModules == kHudsonMostNavModules, "and a route with no nav= goes on until it ends");
		Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 route=derelict_a-1 nav=3", &routePlan) == 0 && strcmp(routePlan.route, "derelict_a-1") == 0 && routePlan.navModules == 3, "a route with nav= stops sooner if it says so");
		Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 nav=20 route=b lighting=relief shadows=on machine=shared", &routePlan) == 0, "and all of that goes with a mode and shadows, sharing the machine");
		Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1", &routePlan) == 0 && routePlan.route[0] == 0 && routePlan.navModules == 0, "nothing asked for, he doesn't walk and has no route");
	}
	Refused("-hudson level=a frames=1 rooms=1 hold=600 hold=600", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 hold=600", kHudsonRefusedHoldWithoutRooms);
	Refused("-hudson level=a frames=1 rooms=0 hold=600", kHudsonRefusedHoldWithoutRooms);

	Refused("-hudson level=a frames=1 lighting=", kHudsonRefusedLightingUnknown);
	Refused("-hudson level=a frames=1 lighting=classic", kHudsonRefusedLightingUnknown);
	Refused("-hudson level=a frames=1 lighting=NORMALS", kHudsonRefusedLightingUnknown);
	Refused("-hudson level=a frames=1 lighting=normal", kHudsonRefusedLightingUnknown);
	Refused("-hudson level=a frames=1 lighting=normalsx", kHudsonRefusedLightingUnknown);
	Refused("-hudson level=a frames=1 lighting=normals lighting=normals", kHudsonRefusedAskedTwice);

	Refused("-hudson level=a frames=1 shadows=", kHudsonRefusedShadowsUnknown);
	Refused("-hudson level=a frames=1 shadows=1", kHudsonRefusedShadowsUnknown);
	Refused("-hudson level=a frames=1 shadows=ON", kHudsonRefusedShadowsUnknown);
	Refused("-hudson level=a frames=1 shadows=on shadows=off", kHudsonRefusedAskedTwice);

	Refused("-hudson level=a frames=1 rooms=1 record=", kHudsonRefusedRecordUnknown);
	Refused("-hudson level=a frames=1 rooms=1 record=flicker", kHudsonRefusedRecordUnknown);
	Refused("-hudson level=a frames=1 rooms=1 record=numbers record=numbers", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 record=pictures", kHudsonRefusedPicturesWithoutRooms);
	Refused("-hudson level=a frames=1 rooms=0 record=pictures", kHudsonRefusedPicturesWithoutRooms);
}

static void TestMarinePlans()
{
	HudsonPlan plan;

	Check(HudsonPlan_Read("-hudson level=a frames=1", &plan) == 0 && plan.rooms == 0 && plan.views == kHudsonDefaultViews, "no rooms asked for means no marine");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=0", &plan) == 0 && plan.rooms == 0, "and so does asking for none");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1", &plan) == 0 && plan.rooms == 1 && plan.views == kHudsonDefaultViews, "one room, looking the default number of ways");
	Check(HudsonPlan_Read("-hudson level=a frames=1 views=1 rooms=1", &plan) == 0 && plan.rooms == 1 && plan.views == 1, "in either order, and the fewest views");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 views=8", &plan) == 0 && plan.views == 8, "and the most");

	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1", &plan) == 0 && plan.holdFrames == kHudsonDefaultHoldFrames && plan.holdFrames == 300, "a hold is as long as one of the game's reports, unless it's asked for");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 hold=600", &plan) == 0 && plan.holdFrames == 600, "hold= says how long");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 hold=30", &plan) == 0 && plan.holdFrames == 30, "down to the shortest");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 hold=100000", &plan) == 0 && plan.holdFrames == 100000, "and up to the longest");

	Check(HudsonPlan_Read("-hudson level=a frames=1", &plan) == 0 && plan.lighting == kHudsonLightingAsItIs && plan.shadows == kHudsonShadowsAsTheyAre && !plan.pictures, "asking for nothing leaves the mode as it is, with no pictures");
	Check(HudsonPlan_Read("-hudson level=a frames=1 lighting=gouraud", &plan) == 0 && plan.lighting == kHudsonLightingGouraud, "lighting= reads gouraud");
	Check(HudsonPlan_Read("-hudson level=a frames=1 lighting=perpixel", &plan) == 0 && plan.lighting == kHudsonLightingPerPixel, "and perpixel");
	Check(HudsonPlan_Read("-hudson level=a frames=1 lighting=normals", &plan) == 0 && plan.lighting == kHudsonLightingNormals, "and normals");
	Check(HudsonPlan_Read("-hudson level=a frames=1 lighting=relief", &plan) == 0 && plan.lighting == kHudsonLightingRelief, "and relief");
	Check(HudsonPlan_Read("-hudson level=a frames=1 shadows=on", &plan) == 0 && plan.shadows == kHudsonShadowsOn, "shadows= reads on");
	Check(HudsonPlan_Read("-hudson level=a frames=1 shadows=off", &plan) == 0 && plan.shadows == kHudsonShadowsOff, "and off");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 record=pictures", &plan) == 0 && plan.pictures, "record= reads pictures");
	Check(HudsonPlan_Read("-hudson level=a frames=1 record=numbers", &plan) == 0 && !plan.pictures, "and numbers, which needs no rooms");

	int x = 0, z = 0;

	HudsonPlan_ViewDirection(0, 2, &x, &z);
	Check(x == 0 && z == kHudsonViewDistance, "the first view looks along z");
	HudsonPlan_ViewDirection(1, 2, &x, &z);
	Check(x == 0 && z == -kHudsonViewDistance, "and the second of two is the other way");
	HudsonPlan_ViewDirection(1, 4, &x, &z);
	Check(x == kHudsonViewDistance && z == 0, "the second of four is a quarter of the way round");
	HudsonPlan_ViewDirection(3, 4, &x, &z);
	Check(x == -kHudsonViewDistance && z == 0, "and the fourth is three quarters");
	HudsonPlan_ViewDirection(1, 8, &x, &z);
	Check(x == 2121 && z == 2121, "the second of eight is half way between, the same distance away");
	HudsonPlan_ViewDirection(2, 7, &x, &z);
	Check(x == 2925 && z == -668, "and each is rounded to the nearest whole number, up or down");

	Check(HudsonPlan_IsFacing(0, 65536, 0, 3000), "facing the same way, whatever the lengths");
	Check(HudsonPlan_IsFacing(1000, 65536, 0, 3000), "a little off still counts");
	Check(!HudsonPlan_IsFacing(4000, 65536, 0, 3000), "three and a half degrees off doesn't");
	Check(!HudsonPlan_IsFacing(0, -65536, 0, 3000), "nor does the opposite way");
	Check(!HudsonPlan_IsFacing(65536, 0, 0, 3000), "nor a quarter turn");
	Check(!HudsonPlan_IsFacing(0, 0, 0, 3000), "having no direction isn't facing anywhere");
	Check(!HudsonPlan_IsFacing(0, 65536, 0, 0), "and nor is being asked to face nowhere");
}

static void TestWallPlans()
{
	HudsonPlan plan;

	Check(HudsonPlan_Read("-hudson level=a frames=1", &plan) == 0 && plan.wall[0] == 0 && plan.wallRank == 1 && !plan.compareRelief, "no wall and no comparing unless they're asked for");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 wall=Hive01", &plan) == 0 && strcmp(plan.wall, "Hive01") == 0 && plan.wallRank == 1, "wall= reads the picture's name, and the biggest wall is the first");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 wall=a_b-9 wallrank=1000", &plan) == 0 && strcmp(plan.wall, "a_b-9") == 0 && plan.wallRank == 1000, "and wallrank= says which, up to the most");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 wall=a", &plan) == 0 && plan.wallRoom[0] == 0, "every room, unless one is asked for");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 wall=a wallroom=JOCKEY-01_x", &plan) == 0 && strcmp(plan.wallRoom, "JOCKEY-01_x") == 0, "wallroom= reads a room's name");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 record=pictures compare=relief", &plan) == 0 && plan.compareRelief, "compare= reads relief");

	char longest[kHudsonLongestWallName + 1];
	char line[128];

	memset(longest, 'a', kHudsonLongestWallName);
	longest[kHudsonLongestWallName] = 0;
	sprintf(line, "-hudson level=a frames=1 rooms=1 wall=%s", longest);
	Check(HudsonPlan_Read(line, &plan) == 0 && strcmp(plan.wall, longest) == 0, "the longest name there's room for");

	sprintf(line, "-hudson level=a frames=1 rooms=1 wall=%sa", longest);
	Refused(line, kHudsonRefusedWallTooLong);
	Refused("-hudson level=a frames=1 rooms=1 wall=", kHudsonRefusedWallEmpty);
	Refused("-hudson level=a frames=1 rooms=1 strategies=all", kHudsonRefusedStrategiesUnknown);
	Refused("-hudson level=a frames=1 strategies=list", kHudsonRefusedStrategiesWithoutRooms);
	Refused("-hudson level=a frames=1 rooms=1 strategy=", kHudsonRefusedStrategyEmpty);
	Refused("-hudson level=a frames=1 rooms=1 strategy=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", kHudsonRefusedStrategyTooLong);
	Refused("-hudson level=a frames=1 rooms=1 strategy=a.b", kHudsonRefusedStrategyCharacter);
	Refused("-hudson level=a frames=1 rooms=1 strategy=a%s", kHudsonRefusedStrategyCharacter);
	Refused("-hudson level=a frames=1 strategy=a", kHudsonRefusedStrategyWithoutRooms);
	Refused("-hudson level=a frames=1 rooms=1 strategy=a wall=b", kHudsonRefusedStrategyWithWall);
	Refused("-hudson level=a frames=1 rooms=1 strategy=a strategyseconds=x", kHudsonRefusedStrategySecondsNotANumber);
	Refused("-hudson level=a frames=1 rooms=1 strategy=a strategyseconds=0", kHudsonRefusedStrategySecondsOutOfRange);
	Refused("-hudson level=a frames=1 rooms=1 strategy=a strategyseconds=301", kHudsonRefusedStrategySecondsOutOfRange);
	Refused("-hudson level=a frames=1 rooms=1 strategyseconds=5", kHudsonRefusedStrategySecondsWithoutStrategy);
	Refused("-hudson level=a frames=1 rooms=1 strategy=a strategy=b", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 rooms=1 strategy=a strategyseconds=5 strategyseconds=6", kHudsonRefusedAskedTwice);
	{
		HudsonPlan strategyPlan;
		Check(HudsonPlan_Read("-hudson level=a frames=1", &strategyPlan) == 0 && strategyPlan.strategy[0] == 0 && strategyPlan.strategyLook[0] == 0 && !strategyPlan.listStrategies && strategyPlan.strategySeconds == 12, "no strategy, unless one is asked for, and 12 seconds to watch one");
		Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 strategies=list", &strategyPlan) == 0 && strategyPlan.listStrategies, "strategies=list asks for the list");
		Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 strategy=DR-a_9 strategylook=sw_1 strategyseconds=300", &strategyPlan) == 0 && strcmp(strategyPlan.strategy, "DR-a_9") == 0 && strcmp(strategyPlan.strategyLook, "sw_1") == 0 && strategyPlan.strategySeconds == 300, "strategy=, strategylook= and the most seconds are all read");
		Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 strategy=a strategyseconds=1", &strategyPlan) == 0 && strategyPlan.strategySeconds == 1, "and the fewest seconds");
	}
	Refused("-hudson level=a frames=1 rooms=1 strategy=a strategylook=", kHudsonRefusedStrategyLookEmpty);
	Refused("-hudson level=a frames=1 rooms=1 strategy=a strategylook=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", kHudsonRefusedStrategyLookTooLong);
	Refused("-hudson level=a frames=1 rooms=1 strategy=a strategylook=a.b", kHudsonRefusedStrategyLookCharacter);
	Refused("-hudson level=a frames=1 rooms=1 strategylook=a", kHudsonRefusedStrategyLookWithoutStrategy);
	Refused("-hudson level=a frames=1 rooms=1 strategy=a strategylook=b strategylook=c", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 rooms=1 strategies=list strategies=list", kHudsonRefusedAskedTwice);
	Check(HudsonPlan_NameMatches("DR-VAENT01", "dr-vaent01"), "a strategy's name matches in capitals or not");
	Check(HudsonPlan_NameMatches("V timer2 airlock 1", "V_timer2_airlock_1"), "a _ in the word stands for a space in the name");
	Check(!HudsonPlan_NameMatches("DR-VAENT01", "DR-VAENT0"), "a word that stops early doesn't match");
	Check(!HudsonPlan_NameMatches("DR-VAENT0", "DR-VAENT01"), "a word that goes on doesn't match");
	Check(!HudsonPlan_NameMatches("V timer", "V-timer"), "a - in the word doesn't stand for a space");
	Refused("-hudson level=a frames=1 rooms=1 wall=a.b", kHudsonRefusedWallCharacter);
	Refused("-hudson level=a frames=1 rooms=1 wall=a/b", kHudsonRefusedWallCharacter);
	Refused("-hudson level=a frames=1 rooms=1 wall=a\\b", kHudsonRefusedWallCharacter);
	Refused("-hudson level=a frames=1 rooms=1 wall=a%s", kHudsonRefusedWallCharacter);
	Refused("-hudson level=a frames=1 rooms=1 wall=a wall=b", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 wall=a", kHudsonRefusedWallWithoutRooms);
	Refused("-hudson level=a frames=1 rooms=0 wall=a", kHudsonRefusedWallWithoutRooms);
	Refused("-hudson level=a frames=1 rooms=1 wall=a wallrank=x", kHudsonRefusedWallRankNotANumber);
	Refused("-hudson level=a frames=1 rooms=1 wall=a wallrank=0", kHudsonRefusedWallRankOutOfRange);
	Refused("-hudson level=a frames=1 rooms=1 wall=a wallrank=1001", kHudsonRefusedWallRankOutOfRange);
	Refused("-hudson level=a frames=1 rooms=1 wall=a wallrank=1 wallrank=1", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 rooms=1 wallrank=1", kHudsonRefusedWallRankWithoutWall);

	sprintf(line, "-hudson level=a frames=1 rooms=1 wall=a wallroom=%s", longest);
	Check(HudsonPlan_Read(line, &plan) == 0 && strcmp(plan.wallRoom, longest) == 0, "the longest room name there's room for");
	sprintf(line, "-hudson level=a frames=1 rooms=1 wall=a wallroom=%sa", longest);
	Refused(line, kHudsonRefusedWallRoomTooLong);
	Refused("-hudson level=a frames=1 rooms=1 wall=a wallroom=", kHudsonRefusedWallRoomEmpty);
	Refused("-hudson level=a frames=1 rooms=1 wall=a wallroom=a.b", kHudsonRefusedWallRoomCharacter);
	Refused("-hudson level=a frames=1 rooms=1 wall=a wallroom=a/b", kHudsonRefusedWallRoomCharacter);
	Refused("-hudson level=a frames=1 rooms=1 wall=a wallroom=a%s", kHudsonRefusedWallRoomCharacter);
	Refused("-hudson level=a frames=1 rooms=1 wall=a wallroom=a wallroom=a", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 rooms=1 wallroom=a", kHudsonRefusedWallRoomWithoutWall);
	Refused("-hudson level=a frames=1 rooms=1 record=pictures compare=", kHudsonRefusedCompareUnknown);
	Refused("-hudson level=a frames=1 rooms=1 record=pictures compare=normals", kHudsonRefusedCompareUnknown);
	Refused("-hudson level=a frames=1 rooms=1 record=pictures compare=relief compare=relief", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 rooms=1 compare=relief", kHudsonRefusedCompareWithoutPictures);
	Refused("-hudson level=a frames=1 rooms=1 record=numbers compare=relief", kHudsonRefusedCompareWithoutPictures);

	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1", &plan) == 0 && !plan.combatAlien, "no fight unless it's asked for");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 combat=alien", &plan) == 0 && plan.combatAlien, "combat= reads alien");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 views=1 combat=alien record=pictures", &plan) == 0 && plan.combatAlien && plan.pictures, "with one view and pictures as well");
	Refused("-hudson level=a frames=1 rooms=1 combat=", kHudsonRefusedCombatUnknown);
	Refused("-hudson level=a frames=1 rooms=1 combat=predator", kHudsonRefusedCombatUnknown);
	Refused("-hudson level=a frames=1 rooms=1 combat=alien combat=alien", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 combat=alien", kHudsonRefusedCombatWithoutRooms);
	Refused("-hudson level=a frames=1 rooms=1 views=2 combat=alien", kHudsonRefusedCombatNotAlone);
	Refused("-hudson level=a frames=1 rooms=1 hold=30 combat=alien", kHudsonRefusedCombatNotAlone);
	Refused("-hudson level=a frames=1 rooms=1 wall=hive01 combat=alien", kHudsonRefusedCombatNotAlone);
	Refused("-hudson level=a frames=1 rooms=1 record=pictures compare=relief combat=alien", kHudsonRefusedCombatNotAlone);
	Refused("-hudson level=a frames=1 rooms=1 holdseconds=3 combat=alien", kHudsonRefusedCombatNotAlone);
	Refused("-hudson level=a frames=1 rooms=2 combat=alien", kHudsonRefusedCombatNotAlone);

	// watch=alien: a fight he doesn't take part in, for filming the way an alien takes to him
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 combat=alien", &plan) == 0 && !plan.watchAlien && !plan.holdFire && !plan.alienAtSaid && !plan.standAtSaid, "a fight is watched only if it asks");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 combat=alien watch=alien", &plan) == 0 && plan.watchAlien && !plan.holdFire, "watch=alien goes with a fight");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 combat=alien watch=alien holdfire=yes alienat=-43880,5490,-310070 standat=-50790,5490,-267890", &plan) == 0
		&& plan.holdFire && plan.alienAtSaid && plan.alienAtX == -43880 && plan.alienAtY == 5490 && plan.alienAtZ == -310070
		&& plan.standAtSaid && plan.standAtX == -50790 && plan.standAtY == 5490 && plan.standAtZ == -267890, "with its two places, in the level's own millimetres, below zero too");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 combat=alien alienat=0,-1,2", &plan) == 0 && plan.alienAtSaid && plan.alienAtX == 0 && plan.alienAtY == -1 && plan.alienAtZ == 2, "and a place can go with a fight that isn't watched");
	Refused("-hudson level=a frames=1 rooms=1 combat=alien watch=marine", kHudsonRefusedWatchUnknown);
	Refused("-hudson level=a frames=1 rooms=1 combat=alien watch=", kHudsonRefusedWatchUnknown);
	Refused("-hudson level=a frames=1 rooms=1 combat=alien watch=alien holdfire=no", kHudsonRefusedHoldFireNotYes);
	Refused("-hudson level=a frames=1 rooms=1 combat=alien holdfire=yes", kHudsonRefusedHoldFireWithoutWatch);
	Refused("-hudson level=a frames=1 rooms=1 watch=alien", kHudsonRefusedWatchWordsWithoutCombat);
	Refused("-hudson level=a frames=1 rooms=1 holdfire=yes", kHudsonRefusedWatchWordsWithoutCombat);
	Refused("-hudson level=a frames=1 rooms=1 alienat=1,2,3", kHudsonRefusedWatchWordsWithoutCombat);
	Refused("-hudson level=a frames=1 rooms=1 standat=1,2,3", kHudsonRefusedWatchWordsWithoutCombat);
	Refused("-hudson level=a frames=1 rooms=1 combat=alien watch=alien aliens=2", kHudsonRefusedWatchWithAliens);
	Refused("-hudson level=a frames=1 rooms=1 combat=alien alienat=1,2", kHudsonRefusedPlaceNotThreeNumbers);
	Refused("-hudson level=a frames=1 rooms=1 combat=alien alienat=1,2,3,4", kHudsonRefusedPlaceNotThreeNumbers);
	Refused("-hudson level=a frames=1 rooms=1 combat=alien standat=1,,3", kHudsonRefusedPlaceNotThreeNumbers);
	Refused("-hudson level=a frames=1 rooms=1 combat=alien standat=1,2,3x", kHudsonRefusedPlaceNotThreeNumbers);
	Refused("-hudson level=a frames=1 rooms=1 combat=alien standat=1,2,9999999999", kHudsonRefusedPlaceNotThreeNumbers);
	Refused("-hudson level=a frames=1 rooms=1 combat=alien standat=+1,2,3", kHudsonRefusedPlaceNotThreeNumbers);
	Refused("-hudson level=a frames=1 rooms=1 combat=alien watch=alien watch=alien", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 rooms=1 combat=alien watch=alien holdfire=yes holdfire=yes", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 rooms=1 combat=alien alienat=1,2,3 alienat=1,2,3", kHudsonRefusedAskedTwice);
	// watchfrom=: where a watched fight is filmed from. eyes is his own, as a player standing there would see the alien come
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 combat=alien watch=alien", &plan) == 0 && !plan.watchFromEyes, "a watched fight is filmed from beside the alien unless it says");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 combat=alien watch=alien watchfrom=side", &plan) == 0 && plan.watchAlien && !plan.watchFromEyes, "and watchfrom=side says that");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 combat=alien watch=alien holdfire=yes watchfrom=eyes", &plan) == 0 && plan.watchAlien && plan.holdFire && plan.watchFromEyes, "watchfrom=eyes films it from his own eyes");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 combat=alien watch=alien watchfrom=eyes", &plan) == 0 && !plan.lampOn && !plan.lampBlink, "and a watched fight has no lamp unless it asks, wherever it is filmed from");
	Refused("-hudson level=a frames=1 rooms=1 combat=alien watch=alien watchfrom=above", kHudsonRefusedWatchFromUnknown);
	Refused("-hudson level=a frames=1 rooms=1 combat=alien watch=alien watchfrom=", kHudsonRefusedWatchFromUnknown);
	Refused("-hudson level=a frames=1 rooms=1 combat=alien watch=alien watchfrom=Eyes", kHudsonRefusedWatchFromUnknown);
	Refused("-hudson level=a frames=1 rooms=1 combat=alien watchfrom=eyes", kHudsonRefusedWatchFromWithoutWatch);
	Refused("-hudson level=a frames=1 rooms=1 combat=alien watchfrom=side", kHudsonRefusedWatchFromWithoutWatch);
	Refused("-hudson level=a frames=1 rooms=1 watchfrom=eyes", kHudsonRefusedWatchWordsWithoutCombat);
	Refused("-hudson level=a frames=1 rooms=1 combat=alien watch=alien watchfrom=eyes watchfrom=side", kHudsonRefusedAskedTwice);
	// together=: a watched fight with several aliens at once
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 combat=alien watch=alien together=5", &plan) == 0 && plan.watchTogether == 5, "together=5 has five aliens at once");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 combat=alien watch=alien together=8", &plan) == 0 && plan.watchTogether == 8, "and eight is the most");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 combat=alien watch=alien", &plan) == 0 && plan.watchTogether == 0, "a run that doesn't say has one");
	Refused("-hudson level=a frames=1 rooms=1 combat=alien watch=alien together=1", kHudsonRefusedTogetherNotANumber);
	Refused("-hudson level=a frames=1 rooms=1 combat=alien watch=alien together=9", kHudsonRefusedTogetherNotANumber);
	Refused("-hudson level=a frames=1 rooms=1 combat=alien watch=alien together=x", kHudsonRefusedTogetherNotANumber);
	Refused("-hudson level=a frames=1 rooms=1 combat=alien watch=alien together=", kHudsonRefusedTogetherNotANumber);
	Refused("-hudson level=a frames=1 rooms=1 combat=alien together=3", kHudsonRefusedTogetherWithoutWatch);
	Refused("-hudson level=a frames=1 rooms=1 together=3", kHudsonRefusedWatchWordsWithoutCombat);
	Refused("-hudson level=a frames=1 rooms=1 combat=alien watch=alien together=3 together=3", kHudsonRefusedAskedTwice);
	// The user's alien films have the lamp on at the biggest spot it has: lampsize=
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 lamp=on lampsize=0.35", &plan) == 0 && plan.lampOn && plan.lampSizeHundredths == 35, "lampsize=0.35 is the most the spot goes to");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 lamp=on lampsize=0.05", &plan) == 0 && plan.lampSizeHundredths == 5, "and 0.05 the least");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 lamp=blink lampsize=0.2", &plan) == 0 && plan.lampSizeHundredths == 20, "one digit after the dot is tenths, and a blinking lamp has a size too");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 lamp=on", &plan) == 0 && plan.lampSizeHundredths == 0, "without it the spot is as built");
	Refused("-hudson level=a frames=1 rooms=1 lamp=on lampsize=0.36", kHudsonRefusedLampSizeNotANumber);
	Refused("-hudson level=a frames=1 rooms=1 lamp=on lampsize=0.04", kHudsonRefusedLampSizeNotANumber);
	Refused("-hudson level=a frames=1 rooms=1 lamp=on lampsize=1", kHudsonRefusedLampSizeNotANumber);
	Refused("-hudson level=a frames=1 rooms=1 lamp=on lampsize=0,35", kHudsonRefusedLampSizeNotANumber);
	Refused("-hudson level=a frames=1 rooms=1 lamp=on lampsize=.35", kHudsonRefusedLampSizeNotANumber);
	Refused("-hudson level=a frames=1 rooms=1 lamp=on lampsize=0.355", kHudsonRefusedLampSizeNotANumber);
	Refused("-hudson level=a frames=1 rooms=1 lamp=on lampsize=0.3x", kHudsonRefusedLampSizeNotANumber);
	Refused("-hudson level=a frames=1 rooms=1 lamp=on lampsize=", kHudsonRefusedLampSizeNotANumber);
	Refused("-hudson level=a frames=1 rooms=1 lampsize=0.35", kHudsonRefusedLampSizeWithoutLamp);
	Refused("-hudson level=a frames=1 rooms=1 lamp=off lampsize=0.35", kHudsonRefusedLampSizeWithoutLamp);
	Refused("-hudson level=a frames=1 rooms=1 lamp=on lampsize=0.35 lampsize=0.35", kHudsonRefusedAskedTwice);
	// #126: follow= names one thing in the level whose place and state the log says once a second
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 follow=hugger1", &plan) == 0 && strcmp(plan.follow, "hugger1") == 0, "follow= names one thing to follow");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 follow=locker_area_switch", &plan) == 0 && strcmp(plan.follow, "locker_area_switch") == 0, "with _ for a space in its name");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 follow=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", &plan) == 0 && strlen(plan.follow) == 59, "up to 59 letters");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1", &plan) == 0 && plan.follow[0] == 0, "and a run that doesn't say follows nothing");
	Refused("-hudson level=a frames=1 rooms=1 follow=", kHudsonRefusedFollowName);
	Refused("-hudson level=a frames=1 rooms=1 follow=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", kHudsonRefusedFollowName);
	Refused("-hudson level=a frames=1 rooms=1 follow=hug.ger", kHudsonRefusedFollowName);
	Refused("-hudson level=a frames=1 rooms=1 follow=hug/ger", kHudsonRefusedFollowName);
	Refused("-hudson level=a frames=1 rooms=1 follow=hugger1 follow=hugger1", kHudsonRefusedAskedTwice);
	// #126: a walk, as a player walks, through a walk-in switch, and where he looks once he's there
	Check(HudsonPlan_Read("-hudson level=a frames=1 dieat=5 walkfrom=-45280,5363,8000 walkto=-45280,5363,11000 lookat=-51005,4671,17506", &plan) == 0 && plan.walkSaid && plan.lookAtSaid
		&& plan.walkFromX == -45280 && plan.walkFromY == 5363 && plan.walkFromZ == 8000 && plan.walkToX == -45280 && plan.walkToY == 5363 && plan.walkToZ == 11000
		&& plan.lookAtX == -51005 && plan.lookAtY == 4671 && plan.lookAtZ == 17506 && plan.dieAtSeconds == 5, "a walk from one place to another, and a place to look at after it, in the level's own millimetres");
	Check(HudsonPlan_Read("-hudson level=a frames=1 walkfrom=1,2,3 walkto=4,5,6", &plan) == 0 && plan.walkSaid && !plan.lookAtSaid, "a walk doesn't need a place to look at");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1", &plan) == 0 && !plan.walkSaid && !plan.lookAtSaid, "and a run that doesn't say has no walk");
	Refused("-hudson level=a frames=1 walkfrom=1,2,3", kHudsonRefusedWalkNeedsBoth);
	Refused("-hudson level=a frames=1 walkto=1,2,3", kHudsonRefusedWalkNeedsBoth);
	Refused("-hudson level=a frames=1 lookat=1,2,3", kHudsonRefusedLookAtWithoutWalk);
	Refused("-hudson level=a frames=1 walkfrom=1,2 walkto=1,2,3", kHudsonRefusedWalkPlaceNotThreeNumbers);
	Refused("-hudson level=a frames=1 walkfrom=1,2,3 walkto=1,2,3,4", kHudsonRefusedWalkPlaceNotThreeNumbers);
	Refused("-hudson level=a frames=1 walkfrom=1,2,3 walkto=4,5,6 lookat=x,y,z", kHudsonRefusedWalkPlaceNotThreeNumbers);
	Refused("-hudson level=a frames=1 walkfrom=1,2,3 walkfrom=1,2,3 walkto=4,5,6", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 walkfrom=1,2,3 walkto=4,5,6 walkto=4,5,6", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 rooms=1 combat=alien walkfrom=1,2,3 walkto=4,5,6", kHudsonRefusedWalkNotAlone);
	Refused("-hudson level=a frames=1 rooms=1 walkfrom=1,2,3 walkto=4,5,6", kHudsonRefusedWalkNotAlone);
	Check(HudsonPlan_Read("-hudson level=a frames=1 machine=shared record=pictures walkfrom=1,2,3 walkto=4,5,6", &plan) == 0 && plan.pictures && plan.walkSaid, "a walk can ask for its pictures without rooms=");
	Refused("-hudson level=a frames=1 machine=shared record=pictures walkfrom=1,2,3", kHudsonRefusedPicturesWithoutRooms);
	Refused("-hudson level=a frames=1 rooms=1 combat=alien standat=1,2,3 standat=1,2,3", kHudsonRefusedAskedTwice);

	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 combat=alien", &plan) == 0 && plan.combatAliens == 1, "one alien unless it says");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 combat=alien aliens=5", &plan) == 0 && plan.combatAliens == 5, "aliens= says how many");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 combat=alien aliens=50", &plan) == 0 && plan.combatAliens == 50, "up to the most");
	Refused("-hudson level=a frames=1 rooms=1 combat=alien aliens=", kHudsonRefusedAliensNotANumber);
	Refused("-hudson level=a frames=1 rooms=1 combat=alien aliens=five", kHudsonRefusedAliensNotANumber);
	Refused("-hudson level=a frames=1 rooms=1 combat=alien aliens=0", kHudsonRefusedAliensOutOfRange);
	Refused("-hudson level=a frames=1 rooms=1 combat=alien aliens=51", kHudsonRefusedAliensOutOfRange);
	Refused("-hudson level=a frames=1 rooms=1 combat=alien aliens=2 aliens=2", kHudsonRefusedAskedTwice);
	Refused("-hudson level=a frames=1 rooms=1 aliens=2", kHudsonRefusedAliensWithoutCombat);
}

static void TestPicturesNames()
{
	Check(HudsonPlan_PictureIsNamed("graphics/Envrnmts/Derelict/Hive01.RIM", "hive01"), "a picture is known by the last part of its path, in capitals or not");
	Check(HudsonPlan_PictureIsNamed("graphics\\Envrnmts\\Derelict\\HIVE01.rim", "Hive01"), "whichever way its slashes go");
	Check(HudsonPlan_PictureIsNamed("Hive01", "hive01"), "with or without a folder or an ending");
	Check(!HudsonPlan_PictureIsNamed("graphics/Hive011.RIM", "hive01"), "a longer name isn't it");
	Check(!HudsonPlan_PictureIsNamed("graphics/Hive0.RIM", "hive01"), "nor is a shorter one");
	Check(!HudsonPlan_PictureIsNamed("hive01/other.rim", "hive01"), "and a folder of that name doesn't count");
	Check(!HudsonPlan_PictureIsNamed("graphics/Hive01_Normal.rim", "hive01"), "nor does its map");
	Check(!HudsonPlan_PictureIsNamed(0, "hive01") && !HudsonPlan_PictureIsNamed("a", 0) && !HudsonPlan_PictureIsNamed("a", ""), "and nothing matches nothing");
}

static void TestWallSpots()
{
	// a wall 4 metres wide and 3 tall, from x = 0, at z = 0, facing along z. y goes up as it gets smaller, as in the game
	int const wall[4][3] = { { 0, -3000, 0 }, { 4000, -3000, 0 }, { 4000, 0, 0 }, { 0, 0, 0 } };
	HudsonWallSpot spot;

	Check(HudsonPlan_WallSpot(wall, 4, 0.0, 0.0, 65536.0, &spot), "an upright polygon is a wall");
	Check(spot.wide == 4000.0 && spot.tall == 3000.0, "it knows how wide it is along the wall, and how tall");
	Check(spot.size == 12000000.0, "its size is one of those times the other");
	Check(spot.x == 2000 && spot.y == -1500 && spot.z == kHudsonWallDistance, "he stands straight out from its middle, on the side it faces");
	Check(spot.towardsX == 0 && spot.towardsZ == -kHudsonViewDistance, "and the way to the wall is back along that");

	Check(HudsonPlan_WallSpot(wall, 4, 0.0, 0.0, -65536.0, &spot) && spot.z == -kHudsonWallDistance && spot.towardsZ == kHudsonViewDistance, "facing the other way, he stands on the other side");

	int const sideWall[4][3] = { { 0, -3000, 0 }, { 0, -3000, 4000 }, { 0, 0, 4000 }, { 0, 0, 0 } };

	Check(HudsonPlan_WallSpot(sideWall, 4, 65536.0, 0.0, 0.0, &spot) && spot.x == kHudsonWallDistance && spot.z == 2000 && spot.size == 12000000.0, "a wall along z is measured along z");

	Check(HudsonPlan_WallSpot(wall, 4, 0.0, 0.4848, 0.8746, &spot), "one leaning 29 degrees is still a wall");
	Check(!HudsonPlan_WallSpot(wall, 4, 0.0, 0.5150, 0.8572, &spot), "one leaning 31 degrees isn't");
	Check(HudsonPlan_WallSpot(wall, 4, 0.0, -0.4848, 0.8746, &spot), "and it's the same leaning the other way");
	Check(!HudsonPlan_WallSpot(wall, 4, 0.0, -0.5150, 0.8572, &spot), "both ways");
	Check(!HudsonPlan_WallSpot(wall, 4, 0.0, 65536.0, 65536.0, &spot), "one leaning 45 degrees isn't");
	Check(!HudsonPlan_WallSpot(wall, 4, 0.0, 65536.0, 0.0, &spot), "a floor isn't");
	Check(!HudsonPlan_WallSpot(wall, 4, 0.0, -65536.0, 0.0, &spot), "a ceiling isn't");
	Check(!HudsonPlan_WallSpot(wall, 4, 0.0, 0.0, 0.0, &spot), "something facing no way at all isn't");

	// wide and tall enough, so only the number of corners can be what refuses it
	int const twoCorners[2][3] = { { 0, -3000, 0 }, { 4000, 0, 0 } };

	Check(!HudsonPlan_WallSpot(twoCorners, 2, 0.0, 0.0, 65536.0, &spot), "and two corners don't make a polygon");

	int const narrow[4][3] = { { 0, -3000, 0 }, { 499, -3000, 0 }, { 499, 0, 0 }, { 0, 0, 0 } };
	int const justWideEnough[4][3] = { { 0, -3000, 0 }, { 500, -3000, 0 }, { 500, 0, 0 }, { 0, 0, 0 } };
	int const low[4][3] = { { 0, -499, 0 }, { 4000, -499, 0 }, { 4000, 0, 0 }, { 0, 0, 0 } };
	int const justTallEnough[4][3] = { { 0, -500, 0 }, { 4000, -500, 0 }, { 4000, 0, 0 }, { 0, 0, 0 } };

	Check(!HudsonPlan_WallSpot(narrow, 4, 0.0, 0.0, 65536.0, &spot), "too narrow to judge anything by");
	Check(HudsonPlan_WallSpot(justWideEnough, 4, 0.0, 0.0, 65536.0, &spot), "half a metre wide is enough");
	Check(!HudsonPlan_WallSpot(low, 4, 0.0, 0.0, 65536.0, &spot), "too low");
	Check(HudsonPlan_WallSpot(justTallEnough, 4, 0.0, 0.0, 65536.0, &spot), "half a metre tall is enough");
}

static void TestWallViews()
{
	int x = 0, z = 0;

	Check(HudsonPlan_WallViewDegrees(0) == 0 && HudsonPlan_WallViewStep(0) == 0, "the first view looks straight at the wall");
	Check(HudsonPlan_WallViewDegrees(1) == 35 && HudsonPlan_WallViewStep(1) == 0, "the second is turned, from the same place");
	Check(HudsonPlan_WallViewDegrees(2) == 0 && HudsonPlan_WallViewStep(2) == 1000, "the third looks straight at it from a metre along it");
	Check(HudsonPlan_WallViewDegrees(3) == -35 && HudsonPlan_WallViewStep(3) == 0, "then turned the other way");
	Check(HudsonPlan_WallViewDegrees(4) == 0 && HudsonPlan_WallViewStep(4) == -1000, "and a step the other way");
	Check(HudsonPlan_WallViewDegrees(7) == 0 && HudsonPlan_WallViewStep(7) == 2000, "and there's one for as many views as a run can ask for");
	Check(kHudsonMostViews == 8, "which is 8");
	Check(HudsonPlan_WallViewDegrees(-1) == 0 && HudsonPlan_WallViewStep(-1) == 0 && HudsonPlan_WallViewDegrees(8) == 0 && HudsonPlan_WallViewStep(8) == 0, "and a number past either end can't read past the list");

	HudsonPlan_WallViewDirection(0, 0, -3000, &x, &z);
	Check(x == 0 && z == -3000, "straight at it is the way to the wall, exactly");

	HudsonPlan_WallViewDirection(1, 0, -3000, &x, &z);
	Check(x == -1721 && z == -2457, "turned 35 degrees it's still the same length, and still towards the wall");

	HudsonPlan_WallViewDirection(3, 0, -3000, &x, &z);
	Check(x == 1721 && z == -2457, "and the other way is the same on the other side");

	HudsonPlan_WallViewDirection(2, 0, -3000, &x, &z);
	Check(x == 0 && z == -3000, "from the step it faces the same way as the first, so the two can be compared");

	HudsonPlan_WallViewPlace(0, 0, -3000, &x, &z);
	Check(x == 0 && z == 0, "the first view is from where he first stood");

	HudsonPlan_WallViewPlace(2, 0, -3000, &x, &z);
	Check(x == -1000 && z == 0, "the step is along the wall, so he stays the same distance from it");

	HudsonPlan_WallViewPlace(4, 0, -3000, &x, &z);
	Check(x == 1000 && z == 0, "and the other step goes the other way");

	HudsonPlan_WallViewPlace(2, 3000, 0, &x, &z);
	Check(x == 0 && z == -1000, "whichever way the wall runs");

	HudsonPlan_WallViewPlace(2, 2121, 2121, &x, &z);
	Check(x == 707 && z == -707, "and it's a metre on a slant too");

	HudsonPlan_WallViewPlace(2, 0, 0, &x, &z);
	Check(x == 0 && z == 0, "with no way to the wall there's no step");

	Check(HudsonPlan_OnePictureCovers(4540.0, 64.0, 128.0) == 9080.0, "a wall 4.54 m wide showing half a picture means one picture covers 9.08 m");
	Check(HudsonPlan_OnePictureCovers(4540.0, 0.0, 128.0) == 0.0, "and a wall showing none of it says 0, and doesn't divide by nothing");
}

static void TestTheWaysOfDrawing()
{
	int const count = HudsonPlan_CompareCaseCount();
	std::string names;
	bool allDifferent = true;

	for (int i = 0; i < count; i++)
	{
		std::string const name = std::string("[") + HudsonPlan_CompareCase(i).name + "]";

		allDifferent = allDifferent && names.find(name) == std::string::npos;
		names += name;
	}

	Check(count == 9 && allDifferent, "there are 9 ways of drawing a view, each with a name of its own");

	// Each of these is what the comparison afterwards relies on: tools/hudson/compare-pictures.ps1 compares them by name.
	struct Expected { const char *name; int lighting; bool heightFlipped; bool tiltFlipped; bool noDepth; };

	static const Expected kExpected[] =
	{
		{ "gouraud",       kHudsonLightingGouraud,  false, false, false },
		{ "perpixel",      kHudsonLightingPerPixel, false, false, false },
		{ "normals",       kHudsonLightingNormals,  false, false, false },
		{ "relief",        kHudsonLightingRelief,   false, false, false },
		{ "relief-flip",   kHudsonLightingRelief,   true,  false, false },
		{ "relief-tilt",   kHudsonLightingRelief,   false, true,  false },
		{ "relief-depth0", kHudsonLightingRelief,   false, false, true  },
		{ "relief-again",  kHudsonLightingRelief,   false, false, false },
		{ "normals-again", kHudsonLightingNormals,  false, false, false }
	};

	bool allRight = count == (int)(sizeof(kExpected) / sizeof(kExpected[0]));

	for (int i = 0; allRight && i < count; i++)
	{
		HudsonCompareCase const &drawing = HudsonPlan_CompareCase(i);
		Expected const &wanted = kExpected[i];

		allRight = strcmp(drawing.name, wanted.name) == 0 && drawing.lighting == wanted.lighting && drawing.heightFlipped == wanted.heightFlipped && drawing.tiltFlipped == wanted.tiltFlipped && drawing.noDepth == wanted.noDepth;
	}

	Check(allRight, "and each is exactly what its name says");
	Check(strcmp(HudsonPlan_CompareCase(-1).name, "gouraud") == 0 && strcmp(HudsonPlan_CompareCase(count).name, "gouraud") == 0, "and a number past either end can't read past the list");
}

static void TestFolders()
{
	Check(!HudsonPlan_FolderIsRefused("C:\\Users\\someone\\source\\AvP-run-hudson\\AvPx.exe"), "a test copy of the game is fine");
	Check(HudsonPlan_FolderIsRefused("C:\\Program Files (x86)\\Steam\\steamapps\\common\\Aliens versus Predator Classic\\AvPx.exe"), "the Steam folder isn't");
	Check(HudsonPlan_FolderIsRefused("D:\\SteamApps\\common\\AvP\\AvPx.exe"), "whatever its capitals");
	Check(HudsonPlan_FolderIsRefused(0), "and a folder nobody can find isn't either");

	char const *const steam = "C:\\Program Files (x86)\\Steam\\steamapps\\common\\AvP\\bin\\AvP_Classic.exe";
	char const *const testCopy = "C:\\Users\\someone\\source\\AvP-run-tour\\AvPx.exe";
	HudsonPlan without, with;

	char const *const readWithout = HudsonPlan_Read("-hudson level=a frames=1", &without);
	char const *const readWith = HudsonPlan_Read("-hudson level=a frames=1 steam=yes", &with);

	Check(readWithout == 0 && !without.steamFolderAllowed, "a run isn't let into a Steam folder unless it asks");
	Check(readWith == 0 && with.steamFolderAllowed, "and steam=yes asks");
	Check(!HudsonPlan_FolderIsAllowed(steam, readWithout, without), "without the word, a Steam folder stays refused");
	Check(HudsonPlan_FolderIsAllowed(steam, readWith, with), "with it, a Steam folder is allowed");
	Check(!HudsonPlan_FolderIsAllowed(steam, kHudsonRefusedNoLevel, with), "but not if anything else on the command line was refused");
	Check(!HudsonPlan_FolderIsAllowed(0, readWith, with), "and a folder nobody can find never is");
	Check(HudsonPlan_FolderIsAllowed(testCopy, readWithout, without), "a test copy needs no word");
	Check(HudsonPlan_FolderIsAllowed(testCopy, kHudsonRefusedNoLevel, without), "and a refused command line in one still gets as far as saying why");
}

static std::string ReadSourceFile(const char *name)
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

	// Windows line endings are taken out, because a copy of the repository can have them or not.
	std::string text = all.str();
	std::string::size_type place = 0;

	while ((place = text.find('\r', place)) != std::string::npos)
	{
		text.erase(place, 1);
	}

	return text;
}

// The text from one marker up to the next one after it. Empty if either isn't there, so a check looking inside it fails instead of passing on nothing.
static std::string Between(const std::string &text, const char *from, const char *to)
{
	std::string::size_type const start = text.find(from);

	if (start == std::string::npos)
	{
		return "";
	}

	std::string::size_type const end = text.find(to, start);

	return (end == std::string::npos) ? "" : text.substr(start, end - start);
}

static bool Has(const std::string &text, const char *words)
{
	return text.find(words) != std::string::npos;
}

// How many times these words are in the text
static int CountOf(const std::string &text, const char *words)
{
	int count = 0;

	for (size_t at = text.find(words); at != std::string::npos; at = text.find(words, at + 1))
	{
		count++;
	}

	return count;
}

// Both are there, and the first comes first
static bool ComesBefore(const std::string &text, const char *first, const char *second)
{
	std::string::size_type const one = text.find(first);
	std::string::size_type const two = text.find(second);

	return one != std::string::npos && two != std::string::npos && one < two;
}

static void TestTheRealGame()
{
	std::string const winmain = ReadSourceFile("3dc/avp/win95/winmain.cpp");
	std::string const hudson = ReadSourceFile("3dc/avp/hudson.cpp");
	std::string const menus = ReadSourceFile("3dc/avp/win95/frontend/menus.cpp");

	std::string const reading = Between(hudson, "void Hudson_ReadCommandLine(const char *commandLine)", "int Hudson_CheckBeforeStarting()");

	Check(ComesBefore(winmain, "Hudson_ReadCommandLine(lpCmdLine);", "ClearLog();"), "the game finds out it's a run before the log starts");
	Check(ComesBefore(reading, "HudsonPlan_IsAskedFor", "HudsonPlan_FolderIsAllowed"), "only a run has its folder looked at");
	Check(ComesBefore(reading, "HudsonPlan_FolderIsAllowed", "TerminateProcess"), "and the wrong folder ends the game there");
	Check(ComesBefore(reading, "TerminateProcess", "HudsonRunIsOn = true"), "before anything can take this for a run and use its folder");
	Check(Has(reading, "kHudsonExitWrongFolder"), "with its own exit code");
	Check(!reading.empty() && !Has(reading, "WriteResult") && !Has(reading, "GetSaveFolderPath") && !Has(reading, "LogString") && !Has(reading, "LogErrorString") && !Has(reading, "fopen"), "and nothing is written on the way out");
	Check(ComesBefore(reading, "HudsonPlan_IsAskedFor", "SetErrorMode("), "only a run tells Windows not to ask anyone about errors");
	Check(Has(reading, "SEM_NOGPFAULTERRORBOX") && Has(reading, "SEM_FAILCRITICALERRORS") && Has(reading, "SEM_NOOPENFILEERRORBOX"), "and that covers all three kinds of box");

	std::string const video = Between(hudson, "void Hudson_SetVideoSettings()", "int Hudson_MarineEpisode()");

	Check(ComesBefore(video, "Config_GetInt(\"[VideoMode]\", \"Width\"", "Config_SetInt(\"[VideoMode]\", \"Width\""), "in a new folder the width is put there, then set");
	Check(ComesBefore(video, "Config_GetInt(\"[VideoMode]\", \"Height\"", "Config_SetInt(\"[VideoMode]\", \"Height\""), "and the height");
	Check(ComesBefore(video, "Config_GetBool(\"[VideoMode]\", \"UseVSync\"", "Config_SetBool(\"[VideoMode]\", \"UseVSync\""), "and waiting for the screen");
	Check(Has(hudson, "size=%ux%u vsync=%s") && Has(hudson, "d3d.d3dpp.BackBufferWidth, d3d.d3dpp.BackBufferHeight"), "and the result says what the graphics card really got");

	std::string const window = ReadSourceFile("3dc/avp/win95/win_proj.cpp");

	Check(Has(window, "Hudson_StaysOutOfTheWay() ? SW_SHOWNOACTIVATE : (HudsonRunIsOn ? SW_SHOW : nCmdShow)"), "a run's window opens without asking for the focus");
	Check(Has(window, "Hudson_StaysOutOfTheWay() ? WS_EX_NOACTIVATE : 0"), "and can't be activated, which a normal game's can");
	Check(Has(window, "SWP_SHOWWINDOW | (Hudson_StaysOutOfTheWay() ? (SWP_NOACTIVATE | SWP_NOZORDER) : 0)"), "and sizing it doesn't take the focus or bring it to the front, as it does in a normal game");

	std::string const writing = Between(hudson, "static void WriteResult(const char *howItEnded, int code)", "static void EndAtOnce(");

	Check(ComesBefore(writing, "code == kHudsonExitFinished", "LogString(std::string(\"hudson: \") + line)"), "a run that finished logs an ordinary line");
	Check(Has(Between(writing, "else\n", "\n}\n"), "LogErrorString"), "and any other ending logs an error, which goes to the disk at once");

	Check(Has(Between(winmain, "Music_Init();", "\n\t}\n"), "LoadVorbisTrackList();"), "a run doesn't look for music it won't play, and so doesn't log that it's missing");

	std::string const settings = ReadSourceFile("tools/hudson/AliensVsPredator.cfg");
	std::string const script = ReadSourceFile("tools/hudson/run-hudson.ps1");

	Check(Has(settings, "Width = 1280") && Has(settings, "Height = 720") && Has(settings, "UseVSync = false"), "the known settings are the size and the waiting a run uses");
	Check(Has(script, "Clear-HudsonRunFolder $folder"), "the script clears what the last run left");
	Check(ComesBefore(script, "Clear-HudsonRunFolder $folder", "Copy-Item -LiteralPath $settings"), "before it puts the known settings there");
	Check(ComesBefore(script, "Copy-Item -LiteralPath $settings", "Start-Process"), "before the game starts");
	Check(ComesBefore(script, "$folder -match 'steamapps'", "Clear-HudsonRunFolder $folder"), "and only after refusing a Steam folder");
	Check(Has(script, "if ($refusal) { Stop-Here"), "and if it can't clear them, the game doesn't start");
	Check(Has(script, "settings_sha256="), "and it records which settings they were");

	std::string const camera = Between(ReadSourceFile("3dc/avp/view.cpp"), "void UpdateCamera(void)", "void AVPGetInViewVolumeList");

	Check(ComesBefore(camera, "Global_VDB_Ptr->VDB_Mat = dptr_s->ObMat;", "if (HudsonMarineIsOn)"), "the camera takes the player's place first, as it always has");
	Check(ComesBefore(camera, "Hudson_PointTheCamera(&Global_VDB_Ptr->VDB_World, &Global_VDB_Ptr->VDB_Mat);", "MatrixMultiply(&Global_VDB_Ptr->VDB_Mat"), "and only a run with a marine moves it, before anything else reads it");

	std::string const game = ReadSourceFile("3dc/avp/game.cpp");

	std::string const stoppedInARun = Between(game, "if (!HudsonMarineIsOn || HudsonWorldIsLive)", "\n\t}\n");

	Check(Has(stoppedInARun, "DoHive();") && Has(stoppedInARun, "DoSquad();"), "only a run with a marine stops the hive and the squad");

	std::string const behaviours = Between(ReadSourceFile("3dc/avp/bh_types.cpp"), "void ExecuteBehaviour(STRATEGYBLOCK* sbptr)", "switch(sbptr->I_SBtype)");

	Check(Has(behaviours, "if (HudsonMarineIsOn && Hudson_KeepsStill(sbptr))"), "and only such a run keeps anyone still");

	std::string const keepsStill = Between(hudson, "bool Hudson_KeepsStill(STRATEGYBLOCK *block)", "void Hudson_EndOfFrame()");

	Check(Has(keepsStill, "case I_BehaviourAlien:") && Has(keepsStill, "case I_BehaviourPredator:") && Has(keepsStill, "case I_BehaviourGenerator:") && Has(keepsStill, "default:\n\t\t\treturn false;"), "it's the people and what makes them, and nothing else");
	Check(Has(keepsStill, "NAME_ISEQUAL(block->SBname, marineName)"), "and not the marine himself");

	std::string const marineCode = ReadSourceFile("3dc/avp/bh_marine.cpp");

	Check(Has(Between(hudson, "if (plan.combatAlien)\n\t\t{\n\t\t\tHudsonWorldIsLive = true;", "\n\t\t}\n"), "ForceLoad_Alien = TRUE;"), "only a run that asked for a fight has the world running, and it loads the alien it needs");
	Check(Has(keepsStill, "if (HudsonWorldIsLive)\n\t{\n\t\treturn false;"), "and then nobody's kept still");
	Check(Has(Between(hudson, "static void MakeTheMarine(LONG frame)", "VECTORCH place"), "EndTheLevel(\"combat= is refused in a network game\", kHudsonExitBadArguments);\n\t\treturn;"), "a network game refuses a fight, before anything is made");
	Check(Has(hudson, "part->current_damage.SB_H_flags.Indestructable = 1;") && Has(Between(hudson, "static void WatchTheFight(STRATEGYBLOCK *marine, LONG frame)", "framesInThisStage++"), "MakeEveryPartUnbreakable(status->HModelController.section_data);"), "he lasts, part by part, because the flag on his whole body doesn't stop a blow to a part");
	Check(Has(hudson, "block->DynPtr->DynamicsType = DYN_TYPE_NO_COLLISIONS;") && Has(ReadSourceFile("3dc/avp/los.cpp"), "if(sbPtr->DynPtr->DynamicsType == DYN_TYPE_NO_COLLISIONS)"), "and the player can't stand in the way of a look or a shot: the game's look along a line counts him unless he has no collisions");
	Check(Has(Between(hudson, "static const char *HowTheFightRanOut()", "static void WatchTheMarine"), "he never took the alien as a target"), "a marine who never targets it is said in as many words, because that's the finding");
	Check(Has(Between(hudson, "static void WatchTheFight(STRATEGYBLOCK *marine, LONG frame)", "static const char *HowTheFightRanOut()"), "if (!hurtThisFrame)\n\t\t{\n\t\t\tInterlockedIncrement(&timesItWasHurt);"), "the blow that kills it counts, when it's no longer an alien to be seen being hurt");

	// SOURCE: what a watched fight does in the game. These only show the lines are there; a run shows what they do.
	{
		std::string const watchCode = ReadSourceFile("3dc/avp/hudson.cpp");
		std::string const marineCode = ReadSourceFile("3dc/avp/bh_marine.cpp");

		Check(Has(marineCode, "if (HudsonMarineIsOn && (!HudsonWorldIsLive || HudsonMarineHoldsFire)) {"), "SOURCE: a marine holding his fire takes nothing as a target");
		Check(Has(Between(marineCode, "void WarnMarineOfAttack(STRATEGYBLOCK *marine,STRATEGYBLOCK *attacker) {", "/* Test? */"), "if (HudsonMarineIsOn && HudsonMarineHoldsFire) {\n\t\treturn;"), "SOURCE: not even an alien charging him, which would make it his target without the filter");
		Check(Has(watchCode, "HudsonMarineHoldsFire = plan.holdFire;"), "SOURCE: and holdfire= is what tells him to");
		Check(Has(Between(watchCode, "static void MakeTheAlien(STRATEGYBLOCK *marine, LONG frame)", "\n}\n"), "spot.vy = plan.alienAtY - kAlienStartsAboveTheFloor;"), "SOURCE: alienat= makes the alien just above the place");
		Check(Has(Between(watchCode, "static void MakeTheAlien(STRATEGYBLOCK *marine, LONG frame)", "\n}\n"), "alienat= isn't in any of this level's rooms"), "SOURCE: and refuses a place in no room");
		Check(Has(watchCode, "standat= isn't in any of this level's rooms"), "SOURCE: so does standat=");
		Check(ComesBefore(Between(watchCode, "static void WatchTheFight(STRATEGYBLOCK *marine, LONG frame)", "static const char *HowTheFightRanOut()"), "if (plan.watchAlien)\n\t{\n\t\tif (!WatchTheAlien(marine, alien, frame)", "kMillisecondsBeforeAFightHasStalled"), "SOURCE: a watched fight is never taken away as stalled");
		Check(Has(watchCode, "bool const besideTheAlien = plan.watchAlien && !fromHisEyes && PutTheCameraBesideTheAlien(marine, place, turn);"), "SOURCE: the camera goes beside a watched alien");
		Check(Has(watchCode, "bool const fromHisEyes = plan.watchAlien && plan.watchFromEyes;"), "SOURCE: unless watchfrom=eyes keeps it at his eyes");
		Check(Has(watchCode, "if (fromHisEyes)\n\t{\n\t\tLookFromHisEyesAtTheAlien(marine, place, turn);"), "SOURCE: and turns them to the alien");
		Check(Has(watchCode, "if (plan.combatAlien && !besideTheAlien && !fromHisEyes)"), "SOURCE: and doesn't move it behind him");
		Check(Has(Between(watchCode, "static void LookFromHisEyesAtTheAlien(const STRATEGYBLOCK *marine, VECTORCH *place, MATRIXCH *turn)", "\n}\n"), "int const eyeHeight = CollisionExtents[CE_MARINE].StandingTop + 200;"), "SOURCE: looking from where the game will put his eyes, not his feet");
		Check(Has(Between(watchCode, "static void LookFromHisEyesAtTheAlien(const STRATEGYBLOCK *marine, VECTORCH *place, MATRIXCH *turn)", "\n}\n"), "MakeMatrixFromDirection(&look, turn);"), "SOURCE: and the look is what turns the camera");
		Check(Has(Between(watchCode, "static void LookFromHisEyesAtTheAlien(const STRATEGYBLOCK *marine, VECTORCH *place, MATRIXCH *turn)", "\n}\n"), "if (distance > 0 && ModuleFromPosition(&inFrontOfHim, marine->containingModule))\n\t\t{\n\t\t\t*place = inFrontOfHim;"), "SOURCE: from a little in front of them, so that his own helmet and gun aren't in the way, but only inside a room");
		Check(Has(Between(watchCode, "static void LookFromHisEyesAtTheAlien(const STRATEGYBLOCK *marine, VECTORCH *place, MATRIXCH *turn)", "\n}\n"), "LOS_Lambda - kCameraOffAWall < distance"), "SOURCE: and short of a wall");
		Check(Has(Between(watchCode, "static void LookFromHisEyesAtTheAlien(const STRATEGYBLOCK *marine, VECTORCH *place, MATRIXCH *turn)", "\n}\n"), "int distance = Approximate3dMagnitude(&ahead) / 2;"), "SOURCE: and never past half the way to the alien");
		Check(Has(watchCode, "%d mm above what's below it, animation %s %d, crouched %d, %d mm from him, his feet at y %d"), "SOURCE: each second says how high the alien is, what it's playing and where his feet are");
		Check(Has(watchCode, "HeightAboveWhatsBelow(alien), AlienAnimationWord(status->HModelController.Sequence_Type), status->HModelController.Sub_Sequence, status->IAmCrouched,"), "SOURCE: from the alien's own animation and crouch");
		Check(Has(Between(watchCode, "static int HeightAboveWhatsBelow(const STRATEGYBLOCK *alien)", "\n}\n"), "VECTORCH down = { 0, ONE_FIXED, 0 };"), "SOURCE: the height is measured straight down, where y gets bigger");
		Check(Has(watchCode, "if (plan.pictures)\n\t\t{\n\t\t\tchar what[32] = { 0 };"), "SOURCE: record=pictures takes a picture each second of a watch");
		Check(Has(watchCode, "_snprintf(what, sizeof(what) - 1, \"watch-second-%03d\", seconds);\n\t\t\tAskForAFightPicture(what);"), "SOURCE: named by its second");
		Check(ComesBefore(watchCode, "hudson: a run starts: level %s", "hudson: the shoulder lamp is off in this run, as it is unless a run asks"), "SOURCE: every run's log says whether the lamp is on, as it starts");
		Check(Has(ReadSourceFile("tools/hudson/run-hudson.ps1"), "if ($WatchFrom) { $arguments += \" watchfrom=$WatchFrom\" }"), "SOURCE: the script can ask for watchfrom=");
		Check(Has(watchCode, "ShoulderLamp_SetSpotSize(plan.lampSizeHundredths / 100.0);"), "SOURCE: lampsize= sets the lamp's spot");
		Check(Has(watchCode, "int const lampSize = (int)(ShoulderLamp_SpotSize() * 100.0 + 0.5);"), "SOURCE: and the result says the size the lamp really had, in hundredths, the same in any language");
		Check(Has(watchCode, "\tDieIfItIsTime();\n\tFollowTheNamedThing();"), "SOURCE: follow= is looked at every frame");
		Check(Has(Between(watchCode, "static void FollowTheNamedThing()", "\n}\n"), "if (lastSaid != 0 && now - lastSaid < 1000)"), "SOURCE: and said once a second");
		Check(Has(Between(watchCode, "static void FollowTheNamedThing()", "\n}\n"), "STRATEGYBLOCK const *const thing = HudsonStrategies_Find(plan.follow);"), "SOURCE: and found afresh each time, because a restart makes it again");
		Check(Has(watchCode, "if (HudsonRunIsOn && plan.walkSaid && !levelHasEnded)\n\t{\n\t\tWalkHim((PLAYER_STATUS *)playerStatus);"), "SOURCE: a walk is the player's own input");
		Check(Has(Between(watchCode, "static void WalkHim(PLAYER_STATUS *status)", "\n}\n"), "\t\tstatus->Mvt_InputRequests.Flags.Rqst_Forward = 1;\n\t\tstatus->Mvt_MotionIncrement = ONE_FIXED;"), "SOURCE: facing walkto=, with the forward key, as a person walks: the request and the motion it sets");
		Check(Has(Between(watchCode, "static void WalkHim(PLAYER_STATUS *status)", "\n}\n"), "Hudson_FaceThePlace(status, to);"), "SOURCE: and he turns to walkto= every frame");
		Check(Has(Between(watchCode, "static void WalkHim(PLAYER_STATUS *status)", "\n}\n"), "bool const there = away <= kWalkCloseEnough;"), "SOURCE: until he's close to it");
		Check(Has(Between(watchCode, "static void WalkHim(PLAYER_STATUS *status)", "\n}\n"), "if (walkStage == kWalkDone || walkMayStartAt == 0 || now < walkMayStartAt"), "SOURCE: only once it may start");
		Check(Has(Between(watchCode, "static void WalkHim(PLAYER_STATUS *status)", "\n}\n"), "me->SBDamageBlock.Health <= 0 || !status->IsAlive)"), "SOURCE: and he's alive, which a co-op restart makes him later than single player does");
		Check(Has(Between(watchCode, "bool Hudson_PressesAKeyAfterHeDied()", "\n}\n"), "if (pressedThisTime || now - sawHimOutAt < 3000)"), "SOURCE: one key press each time he goes out");
		Check(Has(Between(watchCode, "bool Hudson_PressesAKeyAfterHeDied()", "\n}\n"), "if (lastAsked == 0 || now - lastAsked > 1000)\n\t{\n\t\tsawHimOutAt = now;\n\t\tpressedThisTime = false;"), "SOURCE: and a gap in the asking is a new time out");
		Check(Has(Between(watchCode, "static void WalkHim(PLAYER_STATUS *status)", "\n}\n"), "if (walkMayStartAt == 0 && plan.dieAtSeconds == 0)"), "SOURCE: which is soon after the start without dieat=");
		Check(Has(watchCode, "if (plan.walkSaid && plan.dieAtSeconds != 0)\n\t{\n\t\twalkMayStartAt = GetTickCount() + 3000;"), "SOURCE: and after the restart with it");
		Check(Has(Between(watchCode, "static void WalkHim(PLAYER_STATUS *status)", "\n}\n"), "_snprintf(walkPictureName, sizeof(walkPictureName) - 1, \"2s\");"), "SOURCE: a picture 2 seconds after he gets there");
		Check(Has(Between(watchCode, "static void WalkHim(PLAYER_STATUS *status)", "\n}\n"), "_snprintf(walkPictureName, sizeof(walkPictureName) - 1, \"5s\");"), "SOURCE: and 5");
		Check(Has(watchCode, "else if (plan.walkSaid && walkPictureName[0])\n\t{\n\t\t_snprintf(name, sizeof(name) - 1, \"walk-%s.png\", walkPictureName);"), "SOURCE: named for the walk, and only the walk's");
		Check(Has(Between(watchCode, "static void WalkHim(PLAYER_STATUS *status)", "\n}\n"), "EndTheLevel(\"the walk is done, and he has looked\", kHudsonExitFinished);"), "SOURCE: and then the run ends");
		Check(Has(watchCode, "if (!plan.walkSaid)\n\t\t\t{\n\t\t\t\tendAfterLookingAt"), "SOURCE: meets= doesn't end a walk's run before the walk");
		Check(Has(ReadSourceFile("3dc/avp/hudson_strategies.cpp"), "\t\tcase I_BehaviourFaceHugger:\n\t\t{\n\t\t\tstatic const char *const states[] = { \"approaching\", \"attacking\", \"waiting\", \"avoiding\", \"dying\", \"floating\", \"jumping\", \"about to jump\" };"), "SOURCE: a facehugger's state is said in words");
		Check(Has(ReadSourceFile("3dc/avp/bh_facehugger.h"), "\tFHNS_Approach,\n\tFHNS_Attack,\n\tFHNS_Wait,\n\tFHNS_Avoidance,\n\tFHNS_Dying,\n\tFHNS_Floating,\n\tFHNS_Jumping,\n\tFHNS_AboutToJump,"), "SOURCE: in the order the game has them");
		Check(Has(ReadSourceFile("tools/hudson/run-hudson.ps1"), "if ($LampSize) { $arguments += \" lampsize=$LampSize\" }"), "SOURCE: the script can ask for lampsize=");
		Check(Has(watchCode, "if (plan.watchTogether > 1)\n\t{\n\t\tMakeTheOtherAliens(spot, frame);"), "SOURCE: together= makes the others with the first");
		Check(Has(Between(watchCode, "static void MakeTheOtherAliens(const VECTORCH &first, LONG frame)", "\n}\n"), "if (!ModuleFromPosition(&spot, (MODULE *)0))"), "SOURCE: only where there's a room");
		Check(Has(Between(watchCode, "static void MakeTheOtherAliens(const VECTORCH &first, LONG frame)", "\n}\n"), "for (int i = 0; i < plan.watchTogether - 1 && i < kHudsonMostTogether - 1; i++)"), "SOURCE: and never more than there are names for");
		Check(Has(watchCode, "		WatchTheOtherAliens(marine, seconds);"), "SOURCE: and follows each of them once a second");
		Check(Has(ReadSourceFile("tools/hudson/run-hudson.ps1"), "if ($Together -gt 0) { $arguments += \" together=$Together\" }"), "SOURCE: the script can ask for together=");
		Check(Has(ReadSourceFile("3dc/avp/win95/pldnet.cpp"), "if (DebouncedGotAnyKey || timeIsUp || Hudson_PressesAKeyAfterHeDied())"), "SOURCE: a host who is out alone starts the level again after dieat=, as single player does");
		Check(Has(ReadSourceFile("tools/hudson/run-hudson.ps1"), "if ($Follow) { $arguments += \" follow=$Follow\" }"), "SOURCE: and follow=");
		Check(Has(ReadSourceFile("tools/hudson/run-hudson.ps1"), "if ($WalkFrom) { $arguments += \" walkfrom=$($WalkFrom -replace 'm', '-')\" }"), "SOURCE: and a walk");
		Check(Has(watchCode, "place->vy -= CollisionExtents[CE_MARINE].StandingTop + 200;"), "SOURCE: and allows for the eye height the game adds afterwards");
		Check(Has(watchCode, "if (plan.watchAlien && watchFirstBlowAt == 0 && marineStage == kMarineFighting)"), "SOURCE: its first blow on him is noted");
		Check(Has(watchCode, "if (watchFirstBlowAt != 0 && now - watchFirstBlowAt >= kMillisecondsAfterTheKill)"), "SOURCE: and ends the run a few seconds later");
	}
	Check(Has(Between(hudson, "static void WatchTheFight(STRATEGYBLOCK *marine, LONG frame)", "static const char *HowTheFightRanOut()"), "if (aliensMade < plan.combatAliens)"), "the next alien comes until there have been as many as were asked for");
	Check(Has(Between(hudson, "static void WatchTheFight(STRATEGYBLOCK *marine, LONG frame)", "static const char *HowTheFightRanOut()"), "status->Courage = ONE_FIXED << 1;"), "he keeps his courage, because someone who can't die would otherwise only ever try to run");
	Check(Has(Between(hudson, "static void WatchTheFight(STRATEGYBLOCK *marine, LONG frame)", "static const char *HowTheFightRanOut()"), "if (GetTickCount() - alienLastHurtAt >= kMillisecondsBeforeAFightHasStalled)"), "a fight where the alien isn't being hurt moves on, so a run can't hang on it");
	Check(ComesBefore(Between(hudson, "if (GetTickCount() - alienLastHurtAt >= kMillisecondsBeforeAFightHasStalled)", "static const char *HowTheFightRanOut()"), "status->Target = NULL;", "DestroyAnyStrategyBlock(alien);"), "and nothing is left pointing at the alien it takes away");
	Check(Has(hudson, "killed=%ld/%d stalled=%ld enemy=%s"), "and the result line counts those, so it can't be mistaken for a win");
	Check(Has(Between(marineCode, "void MarineIsDamaged(STRATEGYBLOCK *sbPtr", "LOCALASSERT(sbPtr);"), "if (HudsonWorldIsLive) {\n\t\tHudson_HeWasHit(sbPtr, damage->Id);"), "a blow on a marine is told to a run with a fight, and only to that");
	Check(Has(Between(hudson, "void Hudson_HeWasHit(STRATEGYBLOCK *block, int kind)", "static const char *HowTheFightRanOut()"), "NAME_ISEQUAL(block->SBname, marineName)"), "and only a blow on him counts, not on a marine the level placed");
	Check(Has(hudson, "CreateMarineBot(&place, MNPCW_PulseRifle + 1);"), "he's sure to have a rifle: CreateMarineBot counts from 1, and 0 means a random weapon");
	Check(Has(marineCode, "if ( (weapon<=0)||(weapon>(int)MNPCW_End)) {") && Has(marineCode, "marineStatus->My_Weapon=GetThisNPCMarineWeapon(weapon-1);"), "which is what that plus 1 is for, so if the game's rule ever changes this says so");
	Check(ComesBefore(Between(hudson, "void Hudson_HeWasHit(STRATEGYBLOCK *block, int kind)", "static const char *HowTheFightRanOut()"), "if (kind == AMMO_ALIEN_FRAG)", "InterlockedIncrement(&blowsOnHim)"), "acid is counted apart, before it could count as a blow, because a drop counts every frame it touches him");
	Check(Has(hudson, "blows=%ld acid=%ld killed="), "and the result line says both");
	Check(Has(Between(marineCode, "int Marine_TargetFilter(STRATEGYBLOCK *candidate) {", "switch (candidate->I_SBtype)"), "if (HudsonMarineIsOn && (!HudsonWorldIsLive || HudsonMarineHoldsFire))"), "with nobody fighting back, a marine has nothing to aim at");
	Check(Has(Between(marineCode, "STRATEGYBLOCK *CreateMarineBot(VECTORCH *Position, int weapon)\n{", "void Marine_GuardAndFace"), "return sbPtr;"), "CreateMarineBot gives back who it made");
	Check(!Has(Between(marineCode, "STRATEGYBLOCK *CreateMarineBot(VECTORCH *Position, int weapon)\n{", "void Marine_GuardAndFace"), "return;"), "and every way out of it gives back something");

	std::string const observer = Between(ReadSourceFile("3dc/avp/hud.cpp"), "if (Observer)", "return;");

	Check(ComesBefore(observer, "GADGET_Render();", "gunLayer = false;"), "an Observer's world isn't drawn as if it were the gun, and what's drawn before that keeps the flag it always had");

	std::string const started = Between(hudson, "int Hudson_CheckBeforeStarting()", "bool Hudson_RunFolder(");

	Check(Has(started, "if (plan.rooms > 0)") && ComesBefore(started, "if (plan.rooms > 0)", "HudsonMarineIsOn = true;"), "a marine only comes from asking for rooms");
	Check(Has(started, "ForceLoad_Marine = TRUE;"), "and his models get loaded, which a level might not do");
	Check(Has(started, "Observer = 1;"), "and the player steps aside");
	Check(Has(hudson, "rooms_asked=%d rooms_reached=%ld views_done=%ld"), "and the result says how far he got");

	std::string const render = ReadSourceFile("3dc/d3d_render.cpp");
	std::string const frameEnd = Between(render, "void ThisFramesRenderingHasFinished()", "Tex_CheckMemoryUsage();");

	Check(ComesBefore(frameEnd, "R_EndScene();", "Hudson_TakePictureIfDue();"), "a picture is taken when the frame is finished");
	Check(Has(frameEnd, "if (HudsonRunIsOn)\n\t\t{\n\t\t\tHudson_TakePictureIfDue();"), "and only in a run");

	std::string const picture = Between(hudson, "void Hudson_TakePictureIfDue()", "void Hudson_PointTheCamera");

	std::string const saving = Between(hudson, "static bool SaveTheScreen(std::string const &path)", "void Hudson_SaveThePage(");

	Check(ComesBefore(picture, "if (!pictureIsDue)", "SaveTheScreen(path)"), "only when one is due");
	Check(ComesBefore(picture, "pictureIsDue = false;", "SaveTheScreen(path)"), "and only once");
	Check(Has(saving, "D3DXIFF_PNG"), "as a file that loses nothing");
	Check(Has(picture, "\"room%ld-view%d.png\""), "named for its view, so that two runs can be compared");
	Check(Has(saving, "copy->Release();") && Has(saving, "screen->Release();"), "and nothing is left held");
	Check(Has(hudson, "if (plan.pictures && !pictureTakenThisHold && (plan.holdSeconds > 0 ? GetTickCount() - holdStartedAt >= (DWORD)plan.holdSeconds * 500 : framesInThisStage == plan.holdFrames / 2))"), "and only a run that asked for pictures has one due, in the middle of a hold");

	Check(Has(hudson, "pictures=%ld%s lighting=%s shadows=%s static_frames=%ld/%ld"), "the result says how many pictures, what was drawn, and how many frames took the fixed walls");
	Check(Has(hudson, "WordForShadowsDrawn(), staticFramesWithWalls, staticFramesAsked,"), "with the frames that drew them first, then the frames that asked");
	Check(Has(Between(hudson, "void Hudson_EndOfFrame()", "void Hudson_StopIfTheGameWantsToAsk"), "InterlockedExchange(&staticFramesWithWalls, StaticScene_FramesWithWallsThisLevel());"), "read at the end of each frame, by the thread that draws");
	Check(Has(Between(hudson, "void Hudson_EndOfFrame()", "void Hudson_StopIfTheGameWantsToAsk"), "InterlockedExchange(&staticFramesAsked, StaticScene_FramesAskedThisLevel());"), "both of them");
	Check(!Has(Between(hudson, "static void WriteResult(const char *howItEnded, int code)", "static void EndAtOnce("), "StaticScene_"), "and never where the watchdog can be writing");
	Check(Has(Between(hudson, "void Hudson_EndOfFrame()", "void Hudson_StopIfTheGameWantsToAsk"), "InterlockedExchange(&lightingDrawn, LightingReallyDrawn());"), "which is read on the thread that draws");
	Check(!Has(Between(hudson, "static void WriteResult(const char *howItEnded, int code)", "static void EndAtOnce("), "LightingReallyDrawn"), "and never where the watchdog can be writing the result");
	Check(Has(Between(hudson, "void Hudson_EndOfFrame()", "void Hudson_StopIfTheGameWantsToAsk"), "if (done == 1 && !GotTheModeItAskedFor())"), "a run that can't have the mode it asked for ends on its first frame");
	Check(Has(Between(hudson, "static bool GotTheModeItAskedFor()", "void Hudson_TakePictureIfDue()"), "plan.lighting == kHudsonLightingAsItIs ||"), "and one that didn't ask doesn't mind");

	std::string const settingMode = Between(hudson, "void Hudson_SetTheMode()", "static bool GotTheModeItAskedFor()");

	std::string const gameLighting = Between(hudson, "static int GameLighting(int word, int asItIs)", "static void DrawThisCase()");

	Check(Has(gameLighting, "case kHudsonLightingGouraud:\n\t\t\treturn kLightingGouraud;"), "gouraud means Gouraud");
	Check(Has(gameLighting, "case kHudsonLightingPerPixel:\n\t\t\treturn kLightingPerPixel;"), "perpixel means per pixel");
	Check(Has(gameLighting, "case kHudsonLightingNormals:\n\t\t\treturn kLightingNormals;"), "normals means normals");
	Check(Has(gameLighting, "case kHudsonLightingRelief:\n\t\t\treturn kLightingRelief;"), "relief means relief");
	Check(Has(settingMode, "(plan.shadows == kHudsonShadowsAsTheyAre) ? ShadowVolumesOn :"), "and shadows only change if the run asked");
	Check(Has(settingMode, "GameLighting(plan.lighting, LightingNow)") && Has(gameLighting, "default:\n\t\t\treturn asItIs;"), "and the lighting too");
	Check(ComesBefore(settingMode, "plan.lighting == kHudsonLightingAsItIs && plan.shadows == kHudsonShadowsAsTheyAre", "ViewSwitches_SetAtStart(lighting, volumes);"), "a run that asks for neither changes nothing, and any other sets the view with them");

	std::string const atStart = Between(render, "void ViewSwitches_SetAtStart(int lighting, int volumes)", "static void SaySwitch");

	Check(Has(atStart, "switchesWereAllOff = lighting == kLightingGouraud && !volumes;"), "setting the switches at the start says whether they're all off");
	Check(Has(atStart, "ToyShadowsMode = switchesWereAllOff ? kToyViewOff : kToyViewFinal;"), "and moves the view with them, so they can't disagree");

	Check(Has(Between(hudson, "static bool GotTheModeItAskedFor()", "void Hudson_TakePictureIfDue()"), "(ToyShadowsMode == 0) == (lightingDrawn == kLightingGouraud && shadowsDrawn == 0)"), "and the check fails if the view is OFF when it shouldn't be, or the other way round");

	Check(Has(script, "lighting=$Lighting") && Has(script, "shadows=$Shadows") && Has(script, "record=$Record"), "the script passes on the mode and the recording");
	Check(Has(script, "hold=$Hold"), "and how long a hold is");
	Check(Has(hudson, "EndTheLevel(\"strategies aren't looked at or asked in a network game\", kHudsonExitNetworkGame);"), "strategies are refused in a network game");
	Check(Has(hudson, "if ((plan.strategy[0] || plan.listStrategies) && WatchTheStrategy(frame))"), "a run with a strategy word goes that way, and no other run does");
	Check(Has(hudson, "RequestState(target, 1, 0);"), "it's asked the way a switch asks");
	Check(Has(script, "'scene' { $staticSwitches = ' -staticscene' }"), "-Static scene turns the route on");
	Check(Has(script, "'compare' { $staticSwitches = ' -staticscene -staticcompare' }"), "and compare turns on both switches, because it needs the route");
	Check(Has(script, "'drop' { $staticSwitches = ' -staticscene -staticcompare -staticcomparedrop' }"), "and drop breaks the new route on purpose, to prove the comparison finds holes");
	Check(Has(script, "default { Stop-Here \"-Static has to be scene, compare or drop"), "and anything else stops the script, and isn't passed on");

	std::string const wholeRender = ReadSourceFile("3dc/d3d_render.cpp");

	Check(ComesBefore(wholeRender, "SurfaceStatic_SetFrame(frameRanges, numFrameRanges / 2);", "CompareSurfaceRoutes(device, worldAxes, toyFrame);"), "half the ranges go before the comparison");
	Check(ComesBefore(wholeRender, "CompareSceneRoutes(device, toyFrame);", "// every range again, for the frame's real pictures"), "and they all come back before the frame's real pictures");
	Check(Has(Between(wholeRender, "static void KeepDeveloperToolsOutOfNetworkGames()", "toldThem"), "StaticCompareDropsHalf = 0;"), "a network game turns it off");
	Check(Has(ReadSourceFile("3dc/avp/win95/winmain.cpp"), "if (devSwitch(\"-staticcomparedrop\"))"), "and it needs -dev");
	Check(ComesBefore(script, "-dev -statusoff$staticSwitches", "-hudson level=$Level"), "and they go before -hudson, because the game refuses anything after it that isn't name=value");
	Check(ComesBefore(script, "switch ($Static)", "-dev -statusoff$staticSwitches"), "once they've been worked out");
	Check(Has(hudson, "if (plan.compareRelief || (plan.holdSeconds > 0 ? GetTickCount() - holdStartedAt >= (DWORD)plan.holdSeconds * 1000 : framesInThisStage >= plan.holdFrames))"), "a hold lasts as long as was asked for");
	Check(ComesBefore(script, "Copy-Item -LiteralPath $picture.FullName", "'run.txt'"), "and keeps the pictures with the run, before the next run clears them");

	std::string const making = Between(hudson, "static void MakeTheMarine(LONG frame)", "static int GameLighting");

	Check(ComesBefore(making, "ChooseTheWall(&spot)", "MoveThePlayerTo(place);"), "a run with a wall finds it first");
	Check(Has(making, "wallStandZ = spot.z;\n\t\tMoveThePlayerTo(place);"), "and the player goes to that wall, and not only on a walk, which moves him as well");
	Check(Has(making, "HudsonNav_MiddleOfRoom(room->m_aimodule->m_index, &place, &away)"), "a walk starts in the middle of the room the level put the player in");
	Check(ComesBefore(making, "MoveThePlayerTo(place);", "CreateMarineBot(&place, MNPCW_PulseRifle + 1)"), "and moves the player there before making the marine, so the game draws that room and thinks about him");
	Check(Has(making, "EndTheLevel(\"the level hasn't the wall that was asked for\", kHudsonExitNoSuchWall);\n\t\t\treturn;"), "and if there's no such wall the run says so and goes no further");
	Check(Has(making, "if (plan.wall[0])"), "and without wall= none of that happens");

	std::string const finding = Between(hudson, "static void FindWallsInRoom(MODULE *room, int roomNumber, std::vector<WallChoice> *walls)", "static bool ChooseTheWall");

	Check(ComesBefore(finding, "Tex_GetRealSize(texture, &width, &height)", "Tex_GetTextureDetails(texture)"), "a polygon's texture number is checked before it's looked up");
	Check(Has(finding, "polygon->PolyItemType == I_ZB_GouraudPolygon"), "and a polygon with only a colour is left out, because that number isn't a texture");
	Check(Has(finding, "iflag_notvis | iflag_transparent | iflag_txanim"), "and one that isn't drawn, or is see-through and so doesn't go through the depth maps, or is animated and so changes by itself");
	Check(Has(finding, "!room->m_mapptr || room->m_sbptr"), "and a room with a behaviour of its own, like a door, which can move");
	Check(Has(finding, "place < 0 || place > (shape->numitems - 1) * 3"), "a polygon's place in the list of normals is checked");
	Check(Has(finding, "cornerNumbers[count] < 0 || cornerNumbers[count] >= shape->numpoints"), "and so is each of its corners");
	Check(Has(finding, "count < kMostCornersOfAWall"), "and how many it can have");
	Check(Has(finding, "ModuleFromPosition(&standHere, (MODULE *)0) == room"), "and the place to stand has to be in the same room");

	std::string const choosing = Between(hudson, "static bool ChooseTheWall(HudsonWallSpot *chosen)", "static void MoveThePlayerTo");

	Check(Has(choosing, "(int)walls.size() < plan.wallRank"), "a wall number past the end is no such wall");
	Check(Has(choosing, "(!plan.wallRoom[0] || (room->name && _stricmp(room->name, plan.wallRoom) == 0))"), "and a room asked for by name is the only one looked in, and one with no name is never it");
	Check(ComesBefore(choosing, "std::stable_sort(walls.begin(), walls.end(), ComesFirst);", "wall number %d is the biggest in room %s"), "and the log says each room's biggest wall by the number that would choose it, once they're in order");
	Check(ComesBefore(choosing, "std::stable_sort(walls.begin(), walls.end(), ComesFirst);", "walls[plan.wallRank - 1]"), "and the walls are in order before one is chosen, biggest first");

	std::string const moving = Between(hudson, "static void MoveThePlayerTo(const VECTORCH &place)", "static void StartThisView");

	Check(Has(moving, "block->containingModule = ModuleFromPosition(&Player->ObWorld, (MODULE *)0);") && Has(moving, "playerPherModule = block->containingModule;") && Has(moving, "AllNewModuleHandler();"), "moving the player is the same steps the game takes in a network game");

	std::string const measuring = Between(hudson, "static void MeasureThePicture(const SHAPEHEADER *shape, const POLYHEADER *polygon, int corners, WallChoice *wall)", "static bool ComesFirst");

	Check(ComesBefore(measuring, "listNumber >= (uint32_t)shape->sh_numuvlists", "shape->sh_textures[listNumber]"), "a polygon's list of picture coordinates is checked before it's looked up");
	Check(Has(measuring, "shape->sh_uvlistlengths[listNumber] < corners"), "and it has to be long enough for its corners");
	Check(Has(choosing, "one picture covers about %.0f across and %.0f down"), "the log says how much of the wall one picture covers");

	std::string const standing = Between(hudson, "static bool StandForThisView(STRATEGYBLOCK *marine)", "static void StartThisView");

	Check(ComesBefore(standing, "if (!room)", "marine->DynPtr->Position = place;"), "a step that leaves the level isn't taken");
	Check(Has(standing, "marine->DynPtr->PrevPosition = place;") && Has(standing, "marine->containingModule = room;"), "he's moved the way the player is, and into the room he's now in");
	Check(Has(standing, "marinePlaceLastFrame = place;"), "and the step doesn't count as him moving during a hold");
	Check(Has(standing, "MoveThePlayerTo(place);"), "and the player goes with him, so the game goes on drawing what he's looking at");
	Check(Has(standing, "marine->DynPtr->Position.vy"), "and he keeps the height he's standing at");
	Check(Has(standing, "wallStandX + stepX") && Has(standing, "wallStandZ + stepZ"), "each view's step is from where he first stood, so the steps don't add up");
	Check(Has(making, "wallStandX = spot.x;") && Has(making, "wallStandZ = spot.z;"), "which is the place the wall search gave");
	Check(Has(hudson, "MeasureThePicture(shape, polygon, count, &wall);"), "and each wall has its picture measured");

	std::string const startingView = Between(hudson, "static void StartThisView(STRATEGYBLOCK *marine)", "static void MakeTheMarine");

	Check(Has(startingView, "EndTheLevel(\"a view's step along the wall goes outside the level\", kHudsonExitNoSuchWall);\n\t\t\treturn;"), "and the run says so, and goes no further");
	Check(ComesBefore(startingView, "StandForThisView(marine)", "Marine_GuardAndFace"), "he's put in place before he's told where to look");

	std::string const drawingCase = Between(hudson, "static void DrawThisCase()", "static void WatchTheMarine");

	Check(Has(drawingCase, "ViewSwitches_AskLighting(GameLighting(drawing.lighting, LightingNow));"), "the lighting of a way of drawing is asked for the way the game's commands do it");
	Check(Has(drawingCase, "ReliefDepth_Clamped(drawing.noDepth ? 0.0f : kReliefDepthAtStart)"), "and its depth goes through the same limit as the command");
	Check(Has(drawingCase, "ReliefHeightFlipped = drawing.heightFlipped;") && Has(drawingCase, "ReliefTiltFlipped = drawing.tiltFlipped;"), "and each says its own flips, so none depends on the one before");

	std::string const watching = Between(hudson, "static void WatchTheMarine(LONG frame)", "void Hudson_SetTheMode()");

	Check(ComesBefore(watching, "lightingDrawn != GameLighting(drawing.lighting, lightingDrawn)", "pictureIsDue = true;"), "a way of drawing this machine can't do ends the run before a picture has the wrong name on it");
	Check(Has(watching, "EndTheLevel(\"this machine can't draw one of the ways it was asked to compare\", kHudsonExitModeNotThere);\n\t\t\t\t\treturn;"), "and says so");
	Check(Has(watching, "if (caseNow < HudsonPlan_CompareCaseCount())"), "the ways of drawing stop at the end of the list");
	Check(ComesBefore(watching, "if (!stoodStill || !facing)", "if (plan.compareRelief)\n\t\t{\n\t\t\tif (framesInThisStage == kFramesForACase / 2)"), "and he still has to stand still through all of them");
	Check(Has(hudson, "\"room%ld-view%d-%s.png\", roomsReached, viewNow + 1, HudsonPlan_CompareCase(caseNow).name"), "each picture is named for its view and its way of drawing");

	std::string const message = Between(ReadSourceFile("3dc/avp/hud.cpp"), "extern void NewOnScreenMessage(char *messagePtr)", "static void AimGunSight");

	Check(ComesBefore(message, "if (HudsonRunIsOn && !Hudson_NetIsHostile())", "GADGET_NewOnScreenMessage(messagePtr);"), "a run has no messages on the screen, so its pictures can match");

	Check(Has(script, "wall=$Wall") && Has(script, "wallrank=$WallRank") && Has(script, "wallroom=$WallRoom") && Has(script, "compare=$Compare") && Has(script, "combat=$Combat") && Has(script, "aliens=$Aliens"), "the script passes on the wall and the comparing");
	Check(!Has(settings, "ShaderPath"), "the known settings don't say where shaders come from, which the game no longer reads");
	Check(Has(settings, "[Video]\nAutoGenerateMaps = false\nDevViewsKey = 20\nRendering = GOURAUD\nRenderingKey = 22\nShadows = false\nShadowsKey = 18"), "and they start as a new player does, with no maps worked out, so the only map on a wall is one that's in a file");

	std::string const levelStart = Between(menus, "static int StartLevelForHudsonRun(void)", "int AvP_MainMenus(void)");

	Check(ComesBefore(levelStart, "MENUSSTATE_STARTGAME;", "Hudson_SetTheMode();"), "the mode is set just before the level loads");

	Check(ComesBefore(levelStart, "LoadAllAvPMenuGfx();", "ReleaseAllAvPMenuGfx();"), "a run loads and lets go of the menu pictures, as the menus do");
}

// A run going through a level room by room, and a run sharing the machine with someone. These read the code, because they're about what the game does while it runs.
static void TestTourAndSharedMachine()
{
	HudsonPlan plan;

	Check(HudsonPlan_Read("-hudson level=a frames=1", &plan) == 0 && !plan.machineShared, "a run has the machine to itself unless it says otherwise");
	Check(HudsonPlan_Read("-hudson level=a frames=1 machine=alone", &plan) == 0 && !plan.machineShared, "machine=alone says so");
	Check(HudsonPlan_Read("-hudson level=a frames=1 machine=shared", &plan) == 0 && plan.machineShared, "machine=shared shares it");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1000", &plan) == 0 && plan.rooms == 1000, "a tour can ask for as many rooms as any level has");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 holdseconds=3600", &plan) == 0 && plan.holdSeconds == 3600, "the longest hold in seconds");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 holdseconds=1", &plan) == 0 && plan.holdSeconds == 1, "and the shortest");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=2 tourfrom=VIEWD-05", &plan) == 0 && strcmp(plan.tourFrom, "VIEWD-05") == 0, "a tour can say which room it starts from");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=2", &plan) == 0 && plan.tourFrom[0] == 0, "or start where the player does");

	std::string const hudson = ReadSourceFile("3dc/avp/hudson.cpp");
	std::string const watchdog = Between(hudson, "static unsigned __stdcall WatchdogThread", "void Hudson_ReadCommandLine");

	Check(ComesBefore(watchdog, "InterlockedExchange(&machineWasTouched, 1);", "if (!plan.machineShared)"), "a touch is written down whoever's machine it is");
	Check(ComesBefore(watchdog, "if (!plan.machineShared)", "someone touched the machine"), "and it ends only a run that isn't sharing");
	Check(Has(watchdog, "it went past its time limit"), "the time limit stands for every run");
	Check(Has(hudson, "if (!plan.machineShared && (!touchKnown ||"), "only a run that isn't sharing waits for the machine to be idle");
	Check(Has(hudson, "sound=%s machine=%s touched=%s ended=%s"), "the result line says whether it was sharing and whether anyone touched it");
	Check(Has(hudson, "plan.machineShared ? \"shared\" : \"alone\", machineWasTouched ? \"yes\" : \"no\", howItEnded"), "and it gets them in that order");

	std::string const video = Between(hudson, "void Hudson_SetVideoSettings()", "int Hudson_MarineEpisode()");

	Check(Has(video, "bool const waitsForTheScreen = (plan.machineShared || plan.navModules > 0) && !plan.vsyncOff;"), "only a run someone may be watching waits for the screen");
	Check(Has(video, "Config_SetBool(\"[VideoMode]\", \"UseVSync\", waitsForTheScreen);"), "and that's what it asks for");

	std::string const reading = Between(hudson, "void Hudson_ReadCommandLine", "int Hudson_CheckBeforeStarting");

	Check(ComesBefore(reading, "refusalFromReading = HudsonPlan_Read(commandLine, &plan);", "!HudsonPlan_FolderIsAllowed(exePath, refusalFromReading, plan)"), "the folder's answer goes by what the words said, and whether they were refused");

	std::string const making = Between(hudson, "static void MakeTheMarine(LONG frame)", "static int GameLighting");

	Check(Has(making, "made->SBDamageBlock.SB_H_flags.Indestructable = 1;"), "nothing kills him");
	Check(ComesBefore(making, "PlanTheTour();", "CreateMarineBot"), "the tour is planned before he's made");

	std::string const watching = Between(hudson, "static void WatchTheMarine(LONG frame)", "void Hudson_SetTheMode()");

	Check(ComesBefore(watching, "SayWhyHeHasGone(frame);", "NextPlaceOnTheTour(&next)"), "the log says why he went before anything else happens");
	Check(Has(watching, "plan.rooms > 1 && NextPlaceOnTheTour(&next)"), "and only a tour carries on without him");
	Check(Has(watching, "marineStage = kMarineNotMade;"), "with a new marine");
	Check(ComesBefore(watching, "if (stoodStill && !facing)", "Marine_GuardAndFace(marine, &point);"), "he's told again which way to face from where he came to rest, because he may not be where he was put");
	Check(Has(watching, "VECTORCH const point = { place.vx + wantedX, place.vy, place.vz + wantedZ };"), "and it's from where he is now");
	Check(Has(watching, "HeCantStayHere(marine, \"he never settled facing the way he was told\");"), "a room he won't settle in goes through the same door");
	Check(ComesBefore(watching, "if (abs(place.vx - toldToFaceFrom.vx) > kStillIsWithin", "Marine_GuardAndFace(marine, &point);"), "he's only told again if he's somewhere new, because being told every frame stops him ever turning");
	Check(Has(hudson, "toldToFaceFrom = marine->DynPtr->Position;"), "and each view starts from where he stands");
	Check(Has(watching, "GetTickCount() - settlingSince > (plan.rooms > 1 ? kMostMillisecondsToSettleOnATour : kMostMillisecondsToSettle)"), "a run that won't settle ends in seconds, not when frames= runs out");
	Check(Has(watching, "SayWhySettlingIsTakingSoLong(marine, frame, stoodStill, facing);"), "and it says why as it goes");
	Check(Has(watching, "HeCantStayHere(marine, \"he moved while he was meant to hold still\");"), "and so does one he moves in");
	Check(Has(watching, "else if (!MoveHimToTheNextRoom(marine))"), "when the views run out he goes to the next room");

	std::string const cantStay = Between(hudson, "static void HeCantStayHere(STRATEGYBLOCK *marine, const char *howItEnds)", "// Which of the things TheMarine looks for");

	Check(ComesBefore(cantStay, "plan.rooms <= 1", "kHudsonExitMarineStuck"), "a run that isn't a tour still ends with its own number");
	Check(Has(cantStay, "kHudsonExitFinished"), "and a tour that's run out of rooms has finished");

	std::string const placing = Between(ReadSourceFile("3dc/avp/hudson_nav.cpp"), "bool HudsonNav_MiddleOfRoom(int room, VECTORCH *place, int *away)", "static void MakeTheLinks()");

	Check(ComesBefore(placing, "FarLoc_IsAPlaceToStand(biggest, &here)", "FALLP_AuxLocs[room]"), "he tries the middle of the room before the game's list of places");
	Check(Has(placing, "(!found || distance < nearest)"), "and takes the nearest on the list if he has to");
	Check(Has(placing, "here.vy -= kJustAboveTheFloor;"), "he goes a little above a floor he found");

	std::string const planning = Between(hudson, "static void PlanTheTour()", "static int awayFromTheMiddle");

	Check(Has(planning, "room >= 0 && room < AIModuleArraySize && !seen[room]"), "a room joined to another is checked before it's used, and visited once");
	Check(Has(planning, "start->m_aimodule->m_index < 0 || start->m_aimodule->m_index >= AIModuleArraySize"), "and so is the first");

	std::string const moving = Between(hudson, "static bool MoveHimToTheNextRoom(STRATEGYBLOCK *marine)", "static void HeCantStayHere");

	Check(ComesBefore(moving, "marinePlaceLastFrame = place;", "StartThisView(marine);"), "a move doesn't count as him moving during a view");
	Check(Has(moving, "MoveThePlayerTo(place);"), "and the player moves with him");
	Check(Has(moving, "InterlockedIncrement(&roomsReached);"), "and it counts as a room reached");

	std::string const marineCode = ReadSourceFile("3dc/avp/bh_marine.cpp");
	std::string const guard = Between(marineCode, "void Marine_GuardAndFace(STRATEGYBLOCK *sbPtr, const VECTORCH *point)", "void InitMarineBehaviour");

	Check(Has(guard, "marineStatus->my_spot = sbPtr->DynPtr->Position;"), "where he's told to guard from is where he stands");
	Check(Has(guard, "marineStatus->missionmodule = sbPtr->containingModule"), "and in the room he's in");

	// A marine on guard turns to what the squad suspects, ahead of the way he was told. On Invasion that held him for a whole run. A game can only show what it does to him.
	std::string const squadSuspicion = Between(marineCode, "/* Squad level suspicion? */", "/* Use squad suspicion. */");
	std::string const noNotice = Between(marineCode, "void Marine_TakesNoNoticeOfTheSquad(STRATEGYBLOCK *sbPtr)", "void Marine_WhatHeAndHisSquadSuspect");
	std::string const onlyLooks = Between(hudson, "static bool ThisRunOnlyLooksRound()", "}");
	std::string const suspects = Between(marineCode, "void Marine_WhatHeAndHisSquadSuspect", "// Gives a marine on guard");
	std::string const startingView = Between(hudson, "static void StartThisView(STRATEGYBLOCK *marine)\n{", "static void MakeTheMarine");

	Check(Has(squadSuspicion, "(sbPtr!=marineWhoTakesNoNotice)"), "the marine told to take no notice doesn't catch the squad's suspicion");
	Check(Has(noNotice, "marineWhoTakesNoNotice = sbPtr;"), "and telling him is what does it");
	Check(Has(noNotice, "marineStatus->suspicious = 0;"), "and what he was already worrying about goes, or it would hold him for as long as a tour waits");
	Check(Has(noNotice, "marineStatus->previous_suspicion = 0;"), "and what he'd go back to worrying about after losing a target");
	Check(Has(noNotice, "marineStatus->using_squad_suspicion = 0;"), "and he no longer counts it as the squad's");
	Check(Has(suspects, "*hisTime = marineStatus->suspicious;") && Has(suspects, "*hisPoint = marineStatus->suspect_point;"), "what he suspects can be read");
	Check(Has(suspects, "*squadTime = NpcSquad.Squad_Suspicion;") && Has(suspects, "*squadPoint = NpcSquad.squad_suspect_point;"), "and what the squad does");
	Check(Has(onlyLooks, "!plan.combatAlien"), "a fight wants him hearing what the squad hears");
	Check(Has(onlyLooks, "!plan.navModules") && Has(onlyLooks, "!plan.route[0]"), "and a walk");
	Check(Has(onlyLooks, "!plan.strategy[0]") && Has(onlyLooks, "!plan.listStrategies"), "and a strategy");
	Check(ComesBefore(startingView, "if (ThisRunOnlyLooksRound())", "Marine_TakesNoNoticeOfTheSquad(marine)"), "so only a run that just looks round tells him");
	Check(ComesBefore(startingView, "Marine_TakesNoNoticeOfTheSquad(marine)", "Marine_GuardAndFace(marine, &point)"), "and it does so before each view's telling, so nothing's left over from the last room");
	Check(Has(startingView, "SayIfTheSquadSuspectsAnything(marine)"), "and the log says what he's not looking at");
	Check(Has(Between(hudson, "static void SayWhySettlingIsTakingSoLong", "static void WatchTheMarine"), "Marine_WhatHeAndHisSquadSuspect(marine, &hisTime, &hisPoint, &squadTime, &squadPoint)"), "and a marine who won't turn has it in the log too");
	Check(ComesBefore(Between(marineCode, "Marine_ConsiderFallingDamage(STRATEGYBLOCK *sbPtr) {", "CauseDamageToObject"), "if (HudsonMarineIsOn)", "IsInContactWithFloor"), "a fall doesn't hurt a marine in a run");

	std::string const window = ReadSourceFile("3dc/avp/win95/win_proj.cpp");
	std::string const hands = Between(window, "static bool IsSomeonesHand(UINT message)", "LRESULT CALLBACK WindowProc");

	Check(ComesBefore(window, "if (HudsonRunIsOn && IsSomeonesHand(message))", "switch (message)\n\t{\n\t\tcase WM_MOUSEWHEEL:\n\t\t{"), "what someone does stops before the game sees it");
	Check(Has(hands, "case WM_KEYDOWN:") && Has(hands, "case WM_SYSKEYDOWN:") && Has(hands, "case WM_CHAR:"), "keys are someone's hand");
	Check(Has(hands, "case WM_MOUSEWHEEL:") && Has(hands, "case WM_LBUTTONDOWN:") && Has(hands, "case WM_RBUTTONDOWN:") && Has(hands, "case WM_MBUTTONDOWN:") && Has(hands, "case WM_XBUTTONDOWN:"), "and so are the mouse's buttons and wheel");
	Check(Has(hands, "case WM_SETCURSOR:"), "and the game doesn't hide the pointer over a run's window");
	Check(ComesBefore(window, "if (!HudsonRunIsOn)\n\t{\n\t\t::RegisterRawInputDevices", "BOOL ExitWindowsSystem"), "a run doesn't listen to the mouse at all");
	Check(Has(ReadSourceFile("win32/di_func.cpp"), "GotXPad = HudsonRunIsOn ? 0 : UpdateControllerState();"), "or read the game pads");

	std::string const systemCommands = Between(window, "case WM_SYSCOMMAND:", "case WM_CONTEXTMENU:");

	Check(ComesBefore(systemCommands, "if (HudsonRunIsOn)", "return 0;"), "only a run's window ignores being held");
	Check(Has(systemCommands, "case SC_MOVE:") && Has(systemCommands, "case SC_SIZE:"), "by its title bar or its edge, which would stop its frames for as long as they're held");
	Check(Has(systemCommands, "case SC_KEYMENU:") && Has(systemCommands, "case SC_MOUSEMENU:"), "and the menu on it won't open, which would stop them until it's closed");
	Check(ComesBefore(Between(window, "case WM_CONTEXTMENU:", "case WM_ERASEBKGND:"), "if (HudsonRunIsOn)", "return 0;"), "and nor will the one from the right button");

	Check(HudsonPlan_Read("-hudson level=a frames=1", &plan) == 0 && plan.width == 1280 && plan.height == 720 && !plan.sizeAsked, "a run is 1280x720, so that it compares with the last");
	Check(HudsonPlan_Read("-hudson level=a frames=1 machine=shared", &plan) == 0 && plan.width == 1920 && plan.height == 1080 && !plan.sizeAsked, "a run sharing the machine is bigger, because someone may be watching");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 machine=shared record=pictures", &plan) == 0 && plan.width == 1280 && plan.height == 720, "but not if it saves pictures, which have to compare");
	Check(HudsonPlan_Read("-hudson level=a frames=1 machine=shared size=1280x720", &plan) == 0 && plan.width == 1280 && plan.height == 720 && plan.sizeAsked, "and it can say otherwise");
	Check(HudsonPlan_Read("-hudson level=a frames=1 size=1600x900", &plan) == 0 && plan.width == 1600 && plan.height == 900 && plan.sizeAsked, "1600x900 can be asked for");
	Check(HudsonPlan_Read("-hudson level=a frames=1 size=1920x1080", &plan) == 0 && plan.width == 1920 && plan.height == 1080, "and 1920x1080");
	Check(HudsonPlan_Read("-hudson level=a frames=1 size=2560x1440", &plan) == 0 && plan.width == 2560 && plan.height == 1440, "and 2560x1440");
	Check(HudsonPlan_Read("-hudson level=a frames=1 size=640x480", &plan) == 0 && plan.width == 640 && plan.height == 480 && plan.sizeAsked, "and, for the HUD's pictures, 640x480");
	Check(HudsonPlan_Read("-hudson level=a frames=1 size=1024x768", &plan) == 0 && plan.width == 1024 && plan.height == 768, "and 1024x768");
	Check(HudsonPlan_Read("-hudson level=a frames=1 size=3440x1440", &plan) == 0 && plan.width == 3440 && plan.height == 1440, "and 3440x1440");
	Refused("-hudson level=a frames=1 size=3440x1080", kHudsonRefusedSizeUnknown);
	Refused("-hudson level=a frames=1 size=640x4800", kHudsonRefusedSizeUnknown);
	Check(HudsonPlan_SizeFits(1920, 1080, 2560, 1440), "1920x1080 fits on a 2560x1440 screen");
	Check(HudsonPlan_SizeFits(1920, 1080, 1920, 1160), "with exactly enough room for the window's frame");
	Check(!HudsonPlan_SizeFits(1920, 1080, 1920, 1159), "and not with one row less");
	Check(!HudsonPlan_SizeFits(1920, 1080, 1919, 1440), "or one column less");

	Check(HudsonPlan_Read("-hudson level=Co-op_Meat_Factory frames=1", &plan) == 0 && strcmp(plan.level, "Co-op_Meat_Factory") == 0, "a level's name can have capitals and hyphens, as the game's own do");

	std::string const levels = ReadSourceFile("3dc/avp/win95/frontend/env_info.cpp");
	std::string const finding = Between(levels, "int BuiltInEnvironmentNamed(const char *name)", "void SetLevelToLoadForMarine(int episode)");

	Check(Has(finding, "{ MarineEpisodes, PredatorEpisodes, AlienEpisodes, MultiplayerEpisodes, CooperativeEpisodes }"), "every list of the game's own levels is looked in");
	Check(Has(finding, "strcmp(RifNamesForEnvironments[lists[list][episode]], name) == 0"), "for exactly that name");
	Check(Has(finding, "return -1;"), "and anything else is none of them");

	std::string const before = Between(hudson, "int Hudson_CheckBeforeStarting()", "bool Hudson_RunFolder");

	Check(ComesBefore(before, "environment = BuiltInEnvironmentNamed(plan.level);", "isn't the exact name of one of the game's own levels\", kHudsonExitNoSuchLevel);"), "a name that's on none of the lists is refused, with its own reason");
	Check(Has(before, "if (environment < 0)"), "whenever it's not found");

	std::string const levelStart = Between(ReadSourceFile("3dc/avp/win95/frontend/menus.cpp"), "static int StartLevelForHudsonRun(void)", "int AvP_MainMenus(void)");

	Check(Has(levelStart, "SetLevelToLoad((enum AVP_ENVIRONMENT_ID)Hudson_Environment());"), "the name that's loaded is the list's, by its number, and not what the command line said");
	Check(Has(levelStart, "AvP.PlayerType = Hudson_PlayAsPredator() ? I_Predator : I_Marine;"), "and he's a marine wherever he is, unless a gun run asks for a Predator");

	std::string const makeHim = Between(hudson, "static void MakeTheMarine(LONG frame)", "static int GameLighting");

	Check(ComesBefore(makeHim, "if (!ModuleFromPosition(&place, (MODULE *)0))", "CreateMarineBot"), "a marine isn't made in no room at all");
	Check(Has(hudson, "bool const playerIsNowhere = !start;"), "a tour copes with a level that puts the player nowhere");

	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 lighting=relief", &plan) == 0 && !plan.vision, "a run only reads its pictures if it's asked");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 lighting=perpixel vision=check", &plan) == 0 && plan.vision, "vision=check asks, in perpixel");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 lighting=normals vision=check", &plan) == 0 && plan.vision, "in normals");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=3 views=4 holdseconds=3 lighting=relief vision=check machine=shared", &plan) == 0 && plan.vision, "and in relief, on a tour");

	std::string const looking = Between(hudson, "static void LookAtThisFrame()", "void Hudson_TakePictureIfDue()");
	std::string const hole = Between(hudson, "static RayFound WhatARayFound(const ShootTarget &target)", "static void LookAtThisFrame()");

	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 lighting=relief vision=check bright=yes", &plan) == 0 && plan.brightWorld, "bright=yes asks for the world fully lit");
	Check(Has(Between(hudson, "void Hudson_EndOfFrame()", "void Hudson_StopIfTheGameWantsToAsk"), "if (plan.brightWorld && AvP.Network == I_No_Network)\n\t{\n\t\tDrawFullBright = 1;"), "and only then does it light it, and never in a network game");
	Check(Has(looking, "doorsCompared++;\n\t\t\tdoorsAgree += agrees ? 1 : 0;"), "a door has its own count, so that one on the move isn't held against the rest");
	Check(Has(looking, "compared++;\n\t\t\tagree += agrees ? 1 : 0;"), "and everything else has the other");
	Check(Has(looking, "a ray and the distance picture disagree at %d %d: the picture says %.0f, and the ray went %d"), "every ray that disagrees says what it hit");
	Check(Has(looking, "if (!agrees)\n\t\t{\n\t\t\t_snprintf"), "and only those say it");
	Check(Has(looking, "a ray found nothing at %d %d") && Has(looking, "// The picture has a surface here and the ray found nothing at all, which is as far from agreeing as it gets\n\t\t\tcompared++;"), "a ray that finds nothing where there's a surface counts as not agreeing");
	Check(Has(Between(hudson, "static bool RayStoppedAtABack", "static void LookAtThisFrame()"), "target.hasPlane && !(target.flags & iflag_no_bfc) && HudsonVision_SeenFromBehind(direction, target.plane)"), "a ray stopped at a back only if the polygon has a side that isn't drawn");
	Check(Has(looking, "atBacks += atItsBack ? 1 : 0;") && Has(looking, "InterlockedExchangeAdd(&visionRaysAtBacks, atBacks);"), "rays that stopped at a back are counted");
	Check(Has(looking, "if (kind == kRayFoundAHole && RayStoppedAtABack(target, direction))\n\t\t{\n\t\t\tholesAtBacks++;") && Has(looking, "InterlockedExchangeAdd(&visionHolesAtBacks, holesAtBacks);"), "and so are holes that did");
	Check(Has(hudson, "vision_door_rays_agree=%ld vision_door_rays_compared=%ld vision_rays_at_backs=%ld vision_holes_at_backs=%ld vision_screen_differs=%ld vision_screen_too_dark=%ld"), "the result says all that, and how many pixels were too dark to tell");
	// Which runs have the shoulder lamp: none unless it asks, by the user's word on 2026-11-08, and never a run it would spoil
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 machine=shared", &plan) == 0 && !plan.lampOn && !plan.lampBlink && !plan.lampReal && !plan.lampWide, "a shared run has the lamp off unless it asks, by the user's word");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 machine=shared lamp=on", &plan) == 0 && plan.lampOn && !plan.lampBlink && !plan.lampReal && !plan.lampWide, "and lamp=on has it, painted");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 machine=shared lamp=off", &plan) == 0 && !plan.lampOn && !plan.lampBlink, "and lamp=off turns it off");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1", &plan) == 0 && !plan.lampOn, "a run that measures time doesn't have it, so its numbers can be compared with older runs");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 lamp=on", &plan) == 0 && plan.lampOn, "unless it asks");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 machine=shared lighting=relief vision=check", &plan) == 0 && !plan.lampOn, "a run comparing what's on the screen doesn't have it");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 machine=shared lighting=relief vision=check lamp=off", &plan) == 0 && !plan.lampOn, "and it can say so");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 machine=shared views=3 wall=hive01 wallroom=hole record=pictures lighting=relief", &plan) == 0 && !plan.lampOn, "a run judging how a wall is drawn doesn't have it");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 machine=shared views=3 wall=hive01 wallroom=hole record=pictures lighting=relief lamp=on", &plan) == 0 && plan.lampOn, "unless it asks");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 machine=shared lamp=blink", &plan) == 0 && plan.lampBlink && !plan.lampOn, "lamp=blink switches it on and off");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 machine=shared lampkind=real", &plan) == 0 && plan.lampReal && !plan.lampWide, "lampkind=real asks for the real light");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 machine=shared lampkind=painted", &plan) == 0 && !plan.lampReal, "and lampkind=painted for the other");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 machine=shared lampkind=real lampat=wide", &plan) == 0 && plan.lampWide, "lampat=wide sits it further out");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 machine=shared lampkind=real lampat=shoulder", &plan) == 0 && !plan.lampWide, "and lampat=shoulder at his shoulder");
	// A flare for each hold, for seeing what a flare's light does
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 machine=shared flare=yes", &plan) == 0 && plan.flareEachHold, "flare=yes throws one as each hold starts");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 machine=shared", &plan) == 0 && !plan.flareEachHold, "and a run that doesn't ask has none");
	Check(Has(hudson, "visionTakenThisHold = false;\n\n\t\t\tif (plan.flareEachHold)\n\t\t\t{\n\t\t\t\tThrowFlareForThisHold();"), "it's thrown as a hold starts");
	Check(Has(Between(hudson, "static void ThrowFlareForThisHold()", "// Is all he's doing standing in places"), "if (AvP.PlayerType == I_Marine)\n\t{\n\t\tThrowAFlare();"), "by a marine only, through the same call as the key");
	Check(Has(Between(hudson, "static void ThrowFlareForThisHold()", "// Is all he's doing standing in places"), "if (NumberOfFlaresActive > before)\n\t{\n\t\tflaresThrown++;"), "and only the ones that really went are counted");
	Check(Has(hudson, "fprintf(file, \"flares_thrown=%ld\\n\", flaresThrown);"), "the result always says how many");
	Check(Has(ReadSourceFile("tools/hudson/run-hudson.ps1"), "if ($Flare) { $arguments += \" flare=yes\" }"), "the script can ask for it");
	Check(Has(ReadSourceFile("tools/hudson/run-hudson.ps1"), "if ($Look) { $arguments += \" look=$Look\" }"), "the script can ask for look=");
	Check(Has(ReadSourceFile("tools/hudson/run-hudson.ps1"), "if ($Screen -eq 'main') { $arguments += \" screen=main\" }"), "the script can keep the window on the main screen");
	Check(Has(ReadSourceFile("tools/hudson/run-hudson.ps1"), "\" windowslot=$WindowSlot windowsof=$WindowsTogether\""), "and it says which part of the second screen is this game's");
	Check(Has(ReadSourceFile("tools/hudson/run-net-pair.ps1"), "'-WindowSlot', $slot, '-WindowsTogether'"), "and the pair gives each game its own part");
	Check(Has(ReadSourceFile("3dc/avp/win95/win_proj.cpp"), "Hudson_PlaceTheWindow(&startLeft, &startTop, &startWidth, &startHeight);"), "a run's window starts where it's going to stay");
	Check(Has(ReadSourceFile("3dc/avp/win95/win_proj.cpp"), "Hudson_PlaceTheWindow(&left, &top, &wide, &tall);"), "and the game's own sizing doesn't send it back to the main screen");
	Check(Has(ReadSourceFile("3dc/avp/win95/win_proj.cpp"), "::SetWindowPos(hWndMain, 0, left, top, wide, tall, howToSize)"), "which is what it uses to size it");
	Check(Has(ReadSourceFile("win32/d3_func.cpp"), "Hudson_SayWhereTheWindowWent();"), "the log says where it went");
	Check(Has(ReadSourceFile("tools/hudson/run-net-pair.ps1"), "if ($Look) { $words += @('-Look', $Look) }"), "and the pair passes it on");
	Check(Has(ReadSourceFile("3dc/avp/hudson.cpp"), "aimedThisFrame = true;"), "a fight's own aim stops the look on that frame");
	Check(Has(ReadSourceFile("3dc/avp/hudson.cpp"), "Hudson_LookAtWhatMatters(status, me, other);"), "the look runs on each frame of a pair");
	Check(ComesBefore(ReadSourceFile("3dc/avp/hudson.cpp"), "if (Hudson_SmokeTest(status, me, other, now))", "Hudson_Siege(status, me, other, now);"), "a siege waits for the smoke test");
	Check(Has(ReadSourceFile("3dc/avp/hudson.cpp"), "if ((!nearest || distance < nearestDistance) && Hudson_WayIsClear(me, block))"), "a siege only fires at what he can really see");
	Check(Has(ReadSourceFile("3dc/avp/hudson.cpp"), "(needsView && !Hudson_WayIsClearTo(me, place, NULL))"), "and enemies in the players' own room are only made where they can be seen coming");
	Check(Has(ReadSourceFile("3dc/avp/hudson.cpp"), "if (isHost && room && now >= nextEnemy && close < kMostAtOnce)"), "and the level's own, a long way off, don't stop them being made");
	Check(Has(ReadSourceFile("3dc/avp/hudson.cpp"), "(isHost ? 0 : kHostBehindTheJoiner)"), "and the joiner stands behind the host");
	Check(Has(ReadSourceFile("3dc/avp/hudson.cpp"), "Marine_GuardAndFace(made, &lookingAt);"), "the friendly marines stand on guard");
	Check(Has(ReadSourceFile("3dc/avp/hudson.cpp"), "predator->CloakStatus = PCLOAK_Activating;"), "a predator starts cloaked");
	Check(Has(ReadSourceFile("3dc/avp/bh_facehugger.cpp"), "sbPtr->maintainVisibility = 1;"), "and a new facehugger can get a body");
	Check(Has(ReadSourceFile("3dc/avp/win95/frontend/env_info.cpp"), "Hudson_IsASiege() ? Hudson_Environment() : Campaign_Environment"), "a siege loads the level its run named");
	Check(Has(ReadSourceFile("3dc/avp/win95/frontend/menus.cpp"), "if (Hudson_IsASiege() && netGameData.coopCampaign == kCampaignNone)"), "and plays it the way a campaign is played");
	Check(Has(ReadSourceFile("tools/hudson/run-hudson.ps1"), "if ($Siege) { $arguments += \" siege=$Siege\" }"), "the script can ask for a siege");
	Check(Has(ReadSourceFile("tools/hudson/run-hudson.ps1"), "if ($SiegeFrom) { $arguments += \" siegefrom=$SiegeFrom\" }"), "and say where its enemies come from");
	Check(Has(ReadSourceFile("tools/hudson/run-net-pair.ps1"), "if ($SiegeFrom) { $words += @('-SiegeFrom', $SiegeFrom) }"), "and the pair passes that on");
	Check(Has(ReadSourceFile("tools/hudson/run-net-pair.ps1"), "$SiegeFrom = 'canyon_01'"), "the Stranded test is one word");
	Check(Has(ReadSourceFile("tools/hudson/run-net-pair.ps1"), "$SiegeFar = 'canyon_02'"), "and its xenoborgs start round the canyon's corner");
	Check(Has(ReadSourceFile("3dc/avp/hudson.cpp"), "if (Hudson_OneOfOursIsInTheWay(me, nearest))"), "a siege's players don't fire through one of their own");
	Check(Has(ReadSourceFile("3dc/avp/hudson.cpp"), "if (off < kNearTheLine && off > -kNearTheLine)"), "or close past one of them, nearer than the target");
	Check(Has(ReadSourceFile("3dc/avp/hudson.cpp"), "Hudson_SiegeTheFinale(me)"), "and the run ends with a ring of facehuggers");
	Check(!Has(ReadSourceFile("3dc/avp/hudson.cpp"), "siege: the picture for clock"), "a siege takes no pictures during the fight, which stopped the game each time");
	Check(Has(ReadSourceFile("3dc/avp/weapons.cpp"), "return;\n\t\t}\n\n\t\t// A test run's siege: its players can't be hurt even while they fire, which ends the protection above. Outside one this is always false.\n\t\tif (Hudson_SiegeTurnsAwayHit(sbPtr, damage, multiple))\n\t\t{\n\t\t\treturn;"), "a blow on a whole player is turned away in a siege");
	Check(Has(ReadSourceFile("3dc/avp/weapons.cpp"), "if (Hudson_SiegeTurnsAwayHit(sbPtr, damage, multiple))\n\t\t{\n\t\t\treturn(NULL);"), "and so is one on a part of him");
	Check(Has(ReadSourceFile("3dc/avp/hudson.cpp"), "if (!HudsonRunIsOn || !HudsonPlan_PlayersCannotDie(plan) || !player || !damage)"), "and only in a run, whose plan says so");
	Check(Has(ReadSourceFile("3dc/avp/hudson.cpp"), "Hudson_SiegeSayWhatWasTurnedAway();"), "and every blow turned away is logged");
	Check(CountOf(ReadSourceFile("3dc/avp/hudson.cpp"), "Hudson_SiegePutOnTheFloor(&place, me)") == 6, "everything a siege makes is put on a floor the game can find");
	Check(Has(ReadSourceFile("3dc/avp/hudson.cpp"), "if (!LOS_ObjectHitPtr || LOS_Lambda > kLookFromAbove + kLookBelow)"), "and nothing is made where there is none");
	Check(Has(ReadSourceFile("3dc/avp/hudson.cpp"), "if (maxstblocks - NumActiveStBlocks < kBlocksKeptFree)"), "which leaves the level room for everything else");
	Check(Has(ReadSourceFile("3dc/avp/hudson.cpp"), "kHudsonBotFightXenoborg)\n\t\t{\n\t\t\tCreateXenoborg(&place, 0);\n\t\t\tHudson_SiegeWakeTheXenoborgAt(place);"), "and a xenoborg the siege makes starts waking");
	Check(Has(ReadSourceFile("tools/hudson/run-hudson.ps1"), "if ($SiegeFar) { $arguments += \" siegefar=$SiegeFar\" }"), "the script can say where xenoborgs come from");
	Check(Has(ReadSourceFile("tools/hudson/run-net-pair.ps1"), "if ($SiegeFar) { $words += @('-SiegeFar', $SiegeFar) }"), "and the pair passes it on");
	Check(Has(ReadSourceFile("3dc/avp/weapons.cpp"), "GLOBALASSERT(damage);\n\n\t// A test run notes what hit what. Outside one this does nothing.\n\tHudson_NoteDamage(sbPtr, damage->Id);"), "a blow on a whole thing is noted");
	Check(Has(ReadSourceFile("3dc/avp/weapons.cpp"), "frag=NULL;\n\n\t// A test run notes what hit what. Outside one this does nothing.\n\tHudson_NoteDamage(sbPtr, damage->Id);"), "and so is a blow on a part of one");
	Check(Has(ReadSourceFile("3dc/avp/hudson.cpp"), "if (!HudsonRunIsOn || !plan.siegeRoom[0] || !target ||"), "and the note does nothing outside a siege");
	Check(Has(ReadSourceFile("3dc/avp/hudson.cpp"), "ForceLoad_Hugger = TRUE;"), "a siege loads the facehugger's model");
	Check(Has(ReadSourceFile("3dc/avp/hudson.cpp"), "ForceLoad_SentryGun = TRUE;"), "and the sentry gun's");
	Check(Has(ReadSourceFile("3dc/avp/hudson.cpp"), "CreateSentrygun(&place, 0);"), "and the host makes the guns");
	Check(Has(ReadSourceFile("3dc/avp/hudson.cpp"), "if (count == marines)"), "and the players leave the friendly marines alone");
	Check(Has(ReadSourceFile("tools/hudson/run-net-pair.ps1"), "if ($Siege) { $words += @('-Siege', $Siege) }"), "and the pair passes it on");
	Check(Has(ReadSourceFile("3dc/avp/hudson.cpp"), "if (Hudson_SmokeTest(status, me, other, now))"), "the smoke test comes before anything else a pair run does");
	Check(ComesBefore(ReadSourceFile("3dc/avp/hudson.cpp"), "if (Hudson_SmokeTest(status, me, other, now))", "Hudson_NetBotFight(status, other);\n\t}\n\n\tif (plan.killKind"), "before a fight, too");
	Check(Has(ReadSourceFile("3dc/avp/hudson.cpp"), "if (plan.netMode != kHudsonNetNone && !smokeIsOver)"), "and before he dies, if a run wants him to");
	Check(Has(ReadSourceFile("3dc/avp/hudson.cpp"), "smoke=failed: the run ended before it was done"), "a run that never got that far doesn't pass");
	Check(Has(ReadSourceFile("tools/hudson/run-hudson.ps1"), "'smoke: failed: this run wrote nothing about it'"), "run.txt says so when a game wrote nothing");
	Check(Has(ReadSourceFile("tools/hudson/run-net-pair.ps1"), "$lines += \"smoke_$game=$verdict\""), "the pair's record says how each game's went");
	Check(Has(ReadSourceFile("tools/hudson/run-net-pair.ps1"), "if ($smokeFailed) { exit 111 }"), "and a failed one is a failed pair");
	{
		std::string const frames = Between(hudson, "void Hudson_EndOfFrame()", "void Hudson_StopIfTheGameWantsToAsk");
		std::string const lampScript = ReadSourceFile("tools/hudson/run-hudson.ps1");
		std::string const comparing = ReadSourceFile("tools/hudson/compare-pictures.ps1");

		Check(Has(frames, "bool const wanted = plan.lampOn || (plan.lampBlink && (GetTickCount() / 1500) % 2 == 0);\n\n\t\tif (wanted != ShoulderLamp_IsOn())\n\t\t{\n\t\t\tShoulderLamp_Toggle();"), "a run switches the lamp through the same call as the key");
		Check(Has(frames, "ShoulderLamp_SetRealLightWanted(plan.lampReal);"), "and asks for the real light");
		Check(Has(frames, "if (plan.lampWide)\n\t\t{\n\t\t\tShoulderLamp_SetPlaceFromEyes(kHudsonLampWideAcross, kHudsonLampWideUp, 0.0f);"), "and moves it");
		Check(Has(hudson, "\"lamp=%s lamp_kind=%s lamp_at=%s lamp_frames=%ld lamp_real_frames=%ld lamp_size=%d.%02d\\n\""), "the result always says whether a run had the lamp");
		{
			std::string const shapes = ReadSourceFile("3dc/kshape.cpp");
			std::string const noting = Between(shapes, "void ShadowLights_NoteFrame(int frame, int passes, float milliseconds)", "// The same numbers, for a Hudson run's result.");
			std::string const forResult = Between(shapes, "void ShadowLights_ForTheResult(char *line, int size)", "// Two lines for each report");
			std::string const resultLines = Between(hudson, "static void WriteResult(const char *howItEnded, int code)", "// A run that finished is no error,");

			Check(Has(resultLines, "ShadowLights_ForTheResult(shadowLights, (int)sizeof(shadowLights));\n\t\tfprintf(file, \"%s\\n\", shadowLights);"), "the result always gives the most shadow lights there have been");
			Check(Has(forResult, "shadow_light_frames=%d") && ComesBefore(forResult, "shadow_light_frames=%d", "most_in_reach=%d"), "with how many frames that's from, first, because none and no shadow pass would read the same");
			Check(Has(forResult, "most_steady=%d most_brief=%d") && Has(forResult, "allowed_steady=%d allowed_brief=%d"), "and steady and brief apart, with what's allowed");
			Check(Has(forResult, "most_shadow_passes=%d most_shadow_ms=%.1f"), "and what the shadow passes cost");
			Check(Has(forResult, "most_in_reach_room=%s\"") && Has(forResult, "line[size - 1] = 0;"), "and the room, last because it's a name, and the line always ends");
			Check(Has(noting, "framesNoted++;"), "the frames are counted");
			Check(Has(noting, "(lightsThisFrame.steadyChosen > mostSteady) ? lightsThisFrame.steadyChosen : mostSteady") && Has(noting, "(lightsThisFrame.briefChosen > mostBrief) ? lightsThisFrame.briefChosen : mostBrief"), "and the most steady and brief are kept apart");
		}
		Check(Has(lampScript, "if ($Lamp) { $arguments += \" lamp=$Lamp\" }") && Has(lampScript, "if ($LampKind) { $arguments += \" lampkind=$LampKind\" }") && Has(lampScript, "if ($LampAt) { $arguments += \" lampat=$LampAt\" }"), "the script can ask for all of it");
		Check(Has(comparing, "'(^| )lamp=(on|blink)( |$)'") && ComesBefore(comparing, "'(^| )lamp=(on|blink)( |$)'", "$folder = Join-Path $runFolderHere 'pictures'"), "and comparing pictures refuses a run that had the lamp on, because it flickers, so no two pictures of it match");
	}
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 machine=shared record=video", &plan) == 0 && plan.recordVideo && !plan.breakVideo && !plan.pictures, "record=video asks for a film, and nothing else");
	Check(HudsonPlan_Read("-hudson level=a frames=1", &plan) == 0 && plan.netMode == kHudsonNetNone, "a run isn't a network game unless it says");
	Check(HudsonPlan_Read("-hudson level=a frames=1 machine=shared net=host", &plan) == 0 && plan.netMode == kHudsonNetHost, "net=host is the host");
	Check(HudsonPlan_Read("-hudson level=a frames=1 machine=shared net=join", &plan) == 0 && plan.netMode == kHudsonNetJoin, "net=join is the other one");
	{
		std::string const networking = ReadSourceFile("3dc/networking.cpp");
		std::string const hudsonFile = ReadSourceFile("3dc/avp/hudson.cpp");
		std::string const pairScript = ReadSourceFile("tools/hudson/run-net-pair.ps1");
		std::string const oneRun = ReadSourceFile("tools/hudson/run-hudson.ps1");
		std::string const menusFile = ReadSourceFile("3dc/avp/win95/frontend/menus.cpp");
		std::string const winmainFile = ReadSourceFile("3dc/avp/win95/winmain.cpp");

		Check(ComesBefore(hudsonFile, "NetLoopbackOnly = true;", "void Hudson_EndOfFrame") && ComesBefore(winmainFile, "Hudson_CheckBeforeStarting();", "Net_Initialise();"), "a run with net= says it's loopback only before the game's networking starts");
		Check(Has(networking, "if (NetLoopbackOnly)\n\t\t\t{\n\t\t\t\tenet_address_set_host_ip(&ServerAddress, \"127.0.0.1\");"), "the host then has this machine's own address");
		Check(Has(networking, "enet_address_set_host_ip(&ourAddress, \"127.0.0.1\");\n\tourAddress.port = 0;\n\n\thost = enet_host_create(NetLoopbackOnly ? &ourAddress : NULL,"), "and the client too, on any free port, because the host has the game's port");
		Check(Has(networking, "if (NetLoopbackOnly && !NetLoopbackSearch)\n\t{\n\t\treturn false;\n\t}\n\n\tif (ENET_SOCKET_NULL != broadcastSocket)"), "and there's no socket for finding games");
		Check(Has(networking, "bool NetLoopbackOnly = false;"), "and an ordinary game doesn't get any of that");
		Check(hudsonFile.find("NetLoopbackOnly = true;") == hudsonFile.rfind("NetLoopbackOnly = true;") && !Has(networking, "NetLoopbackOnly = true;") && !Has(menusFile, "NetLoopbackOnly ="), "and only a run with net= can change that");
		Check(Has(hudsonFile, "if (plan.netMode != kHudsonNetNone)\n\t{\n\t\tNetLoopbackOnly = true;\n\t\tNetLoopbackSearch = plan.typed;\n\n\t\tif (MultiplayerEpisodeForEnvironment(environment) < 0 && !plan.coop)"), "and a level that isn't on the multiplayer list is refused");
		Check(Has(hudsonFile, "\"net=%s net_stage=%s\\n\""), "the result says how far it got");
		Check(Has(menusFile, "if (Hudson_NetIsOn())\n\t{\n\t\tStartNetworkGameForHudsonRun();"), "the menus' part goes this way");
		Check(Has(menusFile, "Hudson_NetStopBecause(\"the client connected, and the host never told it who it is\");"), "a join that stops says where");
		Check(Has(pairScript, "if ($socket -notmatch '^(udp|tcp) 127\\.0\\.0\\.1:')"), "the pair script looks at what each game really has open");
		Check(Has(pairScript, "exit 110") && Has(pairScript, "Stop-TheGames\n    $lines += @($offTheMachine"), "and ends both if anything is off this machine");
		Check(Has(pairScript, "Get-Process -Name 'claude'"), "and won't start with nobody here");
		Check(ComesBefore(pairScript, "$rulesBefore = @(Get-FirewallRulesForThese)", "$hostRun = Start-OneRun") && Has(pairScript, "$rulesAfter = @(Get-FirewallRulesForThese)"), "and compares the firewall's rules before and after");
		Check(Has(pairScript, "$hostFull -eq $joinFull") && Has(pairScript, "$hostFull -match 'steamapps' -or $joinFull -match 'steamapps'"), "and needs two folders, neither of them Steam's");
		Check(Has(oneRun, "-and -not ($partner -and $_.Path -like \"$partner\\*\")"), "a run only ever starts alongside that one other folder's game");
		Check(Has(oneRun, "if ($Net) { $arguments += \" net=$Net\" }"), "and the script can ask for net=");
		Check(Has(oneRun, "if ($Net -and -not $WhileInUse) { Stop-Here"), "and only with -WhileInUse");
	}
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 machine=shared record=video break=video", &plan) == 0 && plan.recordVideo && plan.breakVideo && !plan.breakPrisms, "break=video breaks the film on purpose");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 machine=shared break=video", &plan) == 0 && plan.recordVideo && plan.breakVideo, "and it doesn't have to ask for the film, because the run has one anyway");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 machine=shared", &plan) == 0 && plan.recordVideo && !plan.pictures, "a run that shares the machine films itself, without asking");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 machine=shared record=numbers", &plan) == 0 && !plan.recordVideo, "and record=numbers says not to");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 machine=shared film=no", &plan) == 0 && !plan.recordVideo, "and so does film=no");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 machine=shared record=numbers film=no", &plan) == 0 && !plan.recordVideo, "and saying it both ways is fine");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 machine=shared record=pictures", &plan) == 0 && plan.recordVideo && plan.pictures, "a run with pictures keeps a film as well");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 machine=shared record=pictures film=no", &plan) == 0 && !plan.recordVideo && plan.pictures, "unless it says not to, and it still has its pictures");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 machine=shared lighting=relief vision=check", &plan) == 0 && plan.recordVideo, "a run that checks what's drawn films too, because a film only reads it");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 machine=shared record=pictures compare=relief", &plan) == 0 && plan.recordVideo, "and so does one that compares pictures");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1", &plan) == 0 && !plan.recordVideo, "a run that measures time has no film");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 machine=alone", &plan) == 0 && !plan.recordVideo, "whichever way it says so");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 record=pictures", &plan) == 0 && !plan.recordVideo && plan.pictures, "not even one with pictures");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 machine=alone film=no", &plan) == 0 && !plan.recordVideo, "and it can say so, which changes nothing");
	{
		std::string const taking = Between(hudson, "void Hudson_TakePictureIfDue()", "if (visionIsDue)");
		std::string const ending = Between(hudson, "static void WriteResult(const char *howItEnded, int code)", "// A run that finished is no error,");
		std::string const scriptForFilm = ReadSourceFile("tools/hudson/run-hudson.ps1");
		std::string const writer = ReadSourceFile("3dc/avp/hudson_video_writer.cpp");

		Check(Has(taking, "if (plan.recordVideo)\n\t{\n\t\tTakeVideoFrame();"), "a film's pictures are only taken when the run has a film, and only in a run, because d3d_render.cpp only comes here in one");
		Check(Has(ReadSourceFile("3dc/d3d_render.cpp"), "if (HudsonRunIsOn)\n\t\t{\n\t\t\tHudson_TakePictureIfDue();"), "and it only comes here in a run");
		Check(Has(ending, "if (plan.recordVideo)\n\t{\n\t\tFinishTheVideo(&videoFacts);"), "every way a run ends finishes the film");
		Check(ComesBefore(ending, "FinishTheVideo(&videoFacts);", "video_file=%s video_size=%dx%d video_frames=%ld video_dropped=%ld video_no_frame=%ld video_bytes=%lld video_stopped=%s video_grab_ms_average=%.2f video_grab_ms_worst=%.2f"), "before the result says what became of it");
		Check(Has(hudson, "HudsonVideoWriter_Finish(kHudsonVideoFinishWaitMilliseconds);"), "and it waits a limited time, so a stuck writer can't hold the end of a run up");
		Check(Has(hudson, "video: this is not a clean recording: %ld pictures were dropped") && Has(hudson, "if (facts->dropped > 0)\n\t{\n\t\t_snprintf"), "a film with pictures dropped says it isn't clean");
		Check(Has(hudson, "LogErrorString(std::string(\"video: \") + facts.why);"), "a film that stops says why, as an error");
		Check(Has(hudson, "plan.breakVideo ? kHudsonVideoBreakAtPicture : 0, kHudsonVideoMostBytes);"), "break=video reaches the writer, and every film has the size limit");
		Check(ComesBefore(Between(hudson, "static void TakeVideoFrame()", "void Hudson_TakePictureIfDue()"), "if (HudsonVideoWriter_WantsPictureFor(seconds))", "GiveTheFilmThisFrame(filmSize, seconds);"), "a frame is only read back if the film wants it");
		Check(Has(ending, "fprintf(file, \"film=%s\\n\", videoFacts.written > 0 ? (std::string(folder ? folder : \"\") + videoName).c_str() : \"none\");") && ComesBefore(ending, "FinishTheVideo(&videoFacts);", "\"film=%s\\n\""), "the result always says where the film is, or that there's none, once it's known");
		Check(Has(scriptForFilm, "$filmLine = if ($filmKept) { \"film=$filmKept\" } else { 'film=none' }") && Has(scriptForFilm, "if ($_ -like 'film=*') { $filmLine } else { $_ }"), "and the script changes that to where it really is");
		Check(ComesBefore(scriptForFilm, "$filmKept = Join-Path $kept $film.Name", "$filmLine = if ($filmKept)") && ComesBefore(scriptForFilm, "Copy-Item -LiteralPath $file -Destination $kept", "$filmLine = if ($filmKept)"), "after it's moved the film and copied the result");
		Check(Has(scriptForFilm, "if ($NoFilm) { $arguments += \" film=no\" }"), "it can say film=no");
		Check(Has(hudson, "video_grab_ms_average") && Has(hudson, "Reading a frame back from the card took %.2f ms on average"), "what it costs a frame goes in the result and the log");
		Check(Has(hudson, "size.Width != filmSize.Width || size.Height != filmSize.Height || size.Format != filmSize.Format"), "a screen that changes size can't be read into a film of another");
		Check(Has(writer, "LoadLibraryExA(\"mfplat.dll\", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32)") && Has(writer, "LoadLibraryExA(\"mfreadwrite.dll\", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32)"), "the video writer only comes from Windows' own folder, so a file planted beside the game can't be picked up");
		Check(!Has(ReadSourceFile("win32/VS2026/AvP.vcxproj"), "mfplat") && !Has(ReadSourceFile("win32/VS2026/AvP.vcxproj"), "mfreadwrite"), "and the game doesn't link to it, so a normal game never loads it");
		Check(Has(scriptForFilm, "if ($Record -eq 'video' -and -not ($TimingsDontMatter -and $WhileInUse)) { Stop-Here"), "the script won't make a film of a run that measures time");
		Check(Has(scriptForFilm, "Move-Item -LiteralPath $film.FullName -Destination $kept"), "and it keeps the film with the run, because the next run clears what's left");
		Check(Has(scriptForFilm, "-Filter 'hudson-video-*.mp4'") && Has(hudson, "\"hudson-video-%04d-%02d-%02d-%02d%02d%02d.mp4\""), "and it looks for the name the game gives it");
		Check(Has(scriptForFilm, "$pictureLines += \"video=$($film.Name) sha256="), "with its fingerprint in run.txt");
	}
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 lighting=relief vision=check", &plan) == 0 && !plan.breakPrisms && !plan.brightWorld, "nothing is broken on purpose unless it's asked");
	Check(HudsonPlan_Read("-hudson level=a frames=1 rooms=1 lighting=relief vision=check break=prisms", &plan) == 0 && plan.breakPrisms, "break=prisms asks");
	Check(Has(hudson, "HudsonBreaksPrisms = plan.breakPrisms;"), "and the game is told");
	Check(ComesBefore(Between(ReadSourceFile("3dc/d3d_render.cpp"), "static HRESULT ToyDrawVolumes(D3DCOLOR colour)", "static int toyQueriesUnanswered"), "if (HudsonBreaksPrisms)", "visionShadows.corners += toyNumVerts;"), "a broken run sends no prisms, and counts none");
	Check(Has(ReadSourceFile("3dc/d3d_render.cpp"), "visionShadows.corners += numWorldCorners + keptCornersThisPass;"), "the corners counted include the ones kept on the card");
	Check(Has(ReadSourceFile("3dc/d3d_render.cpp"), "visionShadows.corners = 0;"), "and start from nothing each frame");
	Check(Has(hudson, "ShadowShoot_FindTarget(ray, target, marine ? marine->SBdptr : 0);"), "a ray goes through the marine, because the camera looks out from inside him");
	Check(Has(ReadSourceFile("3dc/kshape.cpp"), "FindPolygonInLineOfSight_TwoIgnores(&aim, &place, 0, Player, (DISPLAYBLOCK *)alsoIgnore);"), "as well as the player");
	Check(Has(looking, "HudsonVision_RayAgrees(counts.checkDistance[i], target.distance, direction, view.view)"), "rays are checked against the distance picture");
	Check(Has(looking, "if (HudsonVision_ShadowsAreMissing(view.shadows))"), "missing shadows are said");
	Check(ComesBefore(looking, "if (MirroringActive)", "SurfacePictures_LookAtThisFrame"), "a mirrored frame isn't judged");
	Check(Has(looking, "InterlockedIncrement(&visionReadsFailed);"), "pictures that can't be read are counted");
	Check(Has(looking, "SayWhatsMissing(\"colour and no normal\", counts.noNormal, view);"), "colour with no normal is said");
	Check(Has(looking, "SayWhatsMissing(\"colour and no distance\", counts.noDistance, view);"), "and colour with no distance");
	Check(Has(looking, "RayFound const kind = WhatARayFound(target);"), "what a ray through nothing found is sorted");
	Check(Has(hole, "target.isPartOfAModel || target.moduleNumber < 0"), "only a part of the level can be a hole, because models come and go");
	Check(Has(hole, "iflag_transparent | iflag_ignore0 | iflag_notvis"), "and not one that can have gaps, can be seen through, or isn't drawn");
	Check(Has(hole, "target.isMorphing ? kRayFoundADoor : kRayFoundAHole"), "or a door between two shapes, which the ray only sees one of");
	Check(ComesBefore(Between(hudson, "void Hudson_TakePictureIfDue()", "pictureIsDue = false;"), "if (visionIsDue)", "LookAtThisFrame();"), "it looks when it's due, in the same place as a picture, after the frame is drawn");
	Check(Has(hudson, "if (plan.vision && !visionTakenThisHold &&"), "once in each hold, and only if asked");
	Check(Has(hudson, "pictureTakenThisHold = false;\n\t\t\tvisionTakenThisHold = false;"), "and it's due again in the next hold");
	Check(Has(hudson, "vision_screen_too_dark=%ld vision_screen_compared=%ld"), "the result says how many empty pixels differ on the screen");
	Check(Has(looking, "differs += view.screen[i].differs ? 1 : 0;"), "and it counts them");
	Check(ComesBefore(Between(ReadSourceFile("3dc/d3d_render.cpp"), "bool SurfacePictures_LookAtThisFrame", "// Draws the pictures both ways and compares them."), "counts->samples > 0 &&", "finished.description.Format == D3DFMT_X8R8G8B8 || finished.description.Format == D3DFMT_A8R8G8B8"), "the screen is only read if there's something to compare, and only in a shape of pixel it understands");
	Check(Has(hudson, "vision_views=%ld vision_mirrored=%ld vision_reads_failed=%ld vision_no_normal=%ld vision_no_distance=%ld vision_holes=%ld vision_nothing_drawn=%ld vision_no_shadows=%ld vision_rays_agree=%ld vision_rays_compared=%ld vision_door_rays_agree=%ld vision_door_rays_compared=%ld vision_rays_at_backs=%ld vision_holes_at_backs=%ld vision_screen_differs=%ld vision_screen_too_dark=%ld vision_screen_compared=%ld vision_shader=%s"), "the result says what it found, and with which shader");
	Check(Has(looking, "drawn with shaders/%s") && Has(looking, "SurfacePictures_ShaderUsedLast()"), "and so does the log, for each view");
	Check(ComesBefore(Between(hudson, "void Hudson_SayWhichShaderWasCompiled(const char *path)", "bool Hudson_StaysOutOfTheWay()"), "if (!HudsonRunIsOn)", "fopen(path"), "only a run reads a shader back to say which it was");
	Check(Has(hudson, "hudson: compiled %s, %lu bytes, check %08X"), "the log says which shader was compiled, how big it is, and a check of its bytes");

	std::string const surfacePictures = ReadSourceFile("3dc/surface_pictures.cpp");

	Check(ComesBefore(surfacePictures, "effectID_t const effect = withRelief ? d3d.surfaceReliefEffect : d3d.surfaceEffect;", "shaderUsedLast = withRelief ? \"surfaceRelief.psh\" : \"surface.psh\";"), "what's said to be used is the same choice the drawing makes");

	std::string const compiling = Between(ReadSourceFile("win32/d3_func.cpp"), "CreatePixelShader failed for", "CreatePixelShader failed getting constant table");

	Check(Has(compiling, "Hudson_SayWhichShaderWasCompiled(actualPath.c_str());"), "and it's said when a pixel shader has really been made");
	Check(Has(ReadSourceFile("tools/hudson/run-hudson.ps1"), "if ($Vision) { $arguments += \" vision=$Vision\" }"), "and the script can ask");

	std::string const lookInMemory = Between(ReadSourceFile("3dc/d3d_render.cpp"), "bool SurfacePictures_LookAtThisFrame", "// Draws the pictures both ways and compares them.");

	Check(Has(lookInMemory, "!toyDepthSurface || !SurfacePictures_Normal() || !SurfacePictures_Colour() || !pictures.Read(d3d.lpD3DDevice, toyDepthSurface)"), "pictures that aren't there are a failed read, not a crash");
	Check(Has(lookInMemory, "pictures.normal.description.Format == D3DFMT_A2R10G10B10"), "and the normal's ten bits are allowed for");

	std::string const settingUp = Between(hudson, "void Hudson_SetVideoSettings()", "int Hudson_MarineEpisode()");

	Check(Has(settingUp, "(fits || plan.sizeAsked) ? plan.width : kHudsonDefaultWidth"), "a size nobody asked for gives way if it doesn't fit");
	Check(Has(settingUp, "(fits || plan.sizeAsked) ? plan.height : kHudsonDefaultHeight"), "both ways");
	Check(Has(settingUp, "Config_SetInt(\"[VideoMode]\", \"Width\", width);") && Has(settingUp, "Config_SetInt(\"[VideoMode]\", \"Height\", height);"), "and the game is told the size");
	Check(ComesBefore(Between(hudson, "int Hudson_CheckBeforeStarting()", "bool Hudson_RunFolder"), "if (plan.sizeAsked && !HudsonPlan_SizeFits(", "wouldn't fit on the main screen\", kHudsonExitBadArguments);"), "one somebody asked for is refused if it doesn't fit");
	Check(Has(ReadSourceFile("tools/hudson/run-hudson.ps1"), "if ($Size) { $arguments += \" size=$Size\" }"), "the script can ask for a size");

	Check(HudsonPlan_Read("-hudson level=a frames=1", &plan) == 0 && !plan.showWindow, "a run's window stays out of the way unless it's asked");
	Check(HudsonPlan_Read("-hudson level=a frames=1 machine=shared show=yes", &plan) == 0 && plan.showWindow, "show=yes asks");

	std::string const forward = Between(hudson, "void Hudson_BringTheWindowForward()", "static void SayWhereTheWindowIs");

	Check(Has(hudson, "return HudsonRunIsOn && !plan.showWindow;"), "a window stays out of the way only in a run that hasn't asked to show it");
	Check(ComesBefore(forward, "!plan.showWindow", "ShowWindow("), "only show=yes brings a window forward");
	Check(Has(forward, "IsIconic(hWndMain) ? SW_RESTORE : SW_SHOW"), "it's shown again, because the first time can be ignored, and restored if it's minimised");
	Check(Has(forward, "MonitorFromWindow(hWndMain, MONITOR_DEFAULTTONULL)"), "and put back if it's on no monitor");
	Check(ComesBefore(forward, "HWND_TOPMOST", "HWND_NOTOPMOST"), "and lifted over the others for a moment, without staying on top");
	Check(Has(forward, "SetForegroundWindow(hWndMain);"), "and asked to the front");
	Check(ComesBefore(window, "::UpdateWindow(hWndMain);", "Hudson_BringTheWindowForward();"), "when the window opens");

	std::string const endOfFrame = Between(hudson, "void Hudson_EndOfFrame()", "void Hudson_StopIfTheGameWantsToAsk");

	Check(ComesBefore(endOfFrame, "if (plan.showWindow)", "SayWhereTheWindowIs(done);"), "only show=yes says where the window is");
	Check(Has(endOfFrame, "if (done == 1)\n\t\t{\n\t\t\tHudson_BringTheWindowForward();"), "and it's brought forward again once the level is up, once only");
	Check(Has(hudson, "kWindowFileName).c_str(), frame == 1 ? \"w\" : \"a\")"), "it goes in its own file straight away, new for each run");
	Check(ComesBefore(Between(hudson, "int Hudson_CheckBeforeStarting()", "bool Hudson_RunFolder"), "ended=it hasn't ended", "HudsonMarineIsOn = true;"), "a run says it's running, so that the last run's result can't be taken for this one's");
	Check(Has(ReadSourceFile("tools/hudson/run-hudson.ps1"), "if ($Show) { $arguments += \" show=yes\" }"), "the script can ask");

	std::string const lastThing = Between(hudson, "int Hudson_LastThingBeforeExit()", "shutdown=clean");

	Check(ComesBefore(lastThing, "if (!resultIsWritten)", "exitCode = kHudsonExitStoppedEarly;"), "a run nothing ended has stopped early, and doesn't look like one that finished");
	Check(ComesBefore(lastThing, "exitCode = kHudsonExitStoppedEarly;", "WriteResult("), "and it says so in the result");
	Check(Has(lastThing, "WaitForTheResultToBeOnDisk();"), "the game doesn't end from under whoever is still writing the result");

	std::string const writingTheResult = Between(hudson, "static void WriteResult(const char *howItEnded, int code)", "// Ends the run where it stands,");

	Check(Has(writingTheResult, "if (!isTheFirst)\n\t{\n\t\tWaitForTheResultToBeOnDisk();"), "a second to end the run waits for the first's result, so that it can't be lost, and its own line goes after it");
	Check(ComesBefore(writingTheResult, "fclose(file);", "InterlockedExchange(&resultIsOnDisk, 1);"), "and that wait ends once the result really is written");
	Check(Has(hudson, "DWORD const most = kHudsonVideoFinishWaitMilliseconds + 5000;") && Has(hudson, "while (!resultIsOnDisk && GetTickCount() - started < most)"), "and it has a limit longer than finishing a film can take");

	std::string const afterTheFilm = Between(ReadSourceFile("3dc/avp/hudson_video_writer.cpp"), "HRESULT const finished = sink->Finalize();", "InterlockedExchange(&video.goingOn, 0);");

	Check(Has(afterTheFilm, "NoteTheSizeOfTheFile();") && !Has(afterTheFilm, "StopIfThereIsNoRoom();"), "a finished film has its size noted, and can't be said to have stopped");

	std::string const script = ReadSourceFile("tools/hudson/run-hudson.ps1");

	Check(Has(script, "if ($WhileInUse -and -not $TimingsDontMatter) { Stop-Here"), "the script won't share the machine unless the timings don't matter");
	Check(Has(script, "if ($WhileInUse) { $arguments += \" machine=shared\" }"), "and it passes the word on");
	Check(Has(script, "holdseconds=$HoldSeconds") && Has(script, "tourfrom=$TourFrom"), "and how long a hold is, and where a tour starts");
	Check(!Has(script, "steam=yes\""), "and the script never asks to go in a Steam folder, because it empties folders");
	Check(Has(script, "if ($folder -match 'steamapps') { Stop-Here"), "it refuses them itself");
}

static void CheckBotFight()
{
	puts("botfight=");

	HudsonPlan plan;
	Check(HudsonPlan_Read("-hudson level=invasion frames=5 machine=shared net=host coop=yes botfight=marine", &plan) == 0 && plan.botFight == kHudsonBotFightMarine, "marine is understood");
	Check(HudsonPlan_Read("-hudson level=invasion frames=5 machine=shared net=join coop=yes botfight=xenoborg", &plan) == 0 && plan.botFight == kHudsonBotFightXenoborg, "and xenoborg, on the joiner");
	Check(HudsonPlan_Read("-hudson level=hangar frames=5 machine=shared net=host coop=yes botfight=queen", &plan) == 0 && plan.botFight == kHudsonBotFightQueen, "queen is understood");
	Check(HudsonPlan_Read("-hudson level=hangar frames=5 machine=shared net=host coop=yes botfight=facehugger", &plan) == 0 && plan.botFight == kHudsonBotFightFacehugger, "and facehugger");
	Check(HudsonPlan_Read("-hudson level=hangar frames=5 machine=shared net=host coop=yes botfight=sentry", &plan) == 0 && plan.botFight == kHudsonBotFightSentry, "and sentry");
	Check(HudsonPlan_Read("-hudson level=hangar frames=5 machine=shared net=host coop=yes botfight=plasma", &plan) == 0 && plan.botFight == kHudsonBotFightPlasma, "and a Predator with a plasma caster");
	Check(HudsonPlan_Read("-hudson level=fall frames=5 machine=shared net=host coop=yes botfight=flame", &plan) == 0 && plan.botFight == kHudsonBotFightFlame, "and a marine with a flamethrower");
	Check(HudsonPlan_Read("-hudson level=hangar frames=5 machine=shared net=host coop=yes botfight=queen botstay=yes", &plan) == 0 && plan.botStay, "the host can stay beside the joiner");
	Check(HudsonPlan_Read("-hudson level=hangar frames=5 botfight=queen botsingle=yes", &plan) == 0 && plan.botSingle, "the same fight can be had in single player, to compare");
	Check(HudsonPlan_Read("-hudson level=hangar frames=5", &plan) == 0 && plan.framesPerSecond == 60, "a run is held to 60 frames a second unless it asks");
	Check(HudsonPlan_Read("-hudson level=hangar frames=5 fps=30", &plan) == 0 && plan.framesPerSecond == 30, "a run can ask for another rate");
	Check(HudsonPlan_Read("-hudson level=hangar frames=5 fps=uncapped", &plan) == 0 && plan.framesPerSecond == 0, "and for none, but only by name");
	Refused("-hudson level=hangar frames=5 fps=0", kHudsonRefusedFramesPerSecond);

	// The cap has to reach its number on the kind of screen that makes it hard. This plays frames on a pretend 240 Hz screen, in millionths of a second: each frame takes a millisecond of work, the wait for its turn, then the wait for the screen's next refresh.
	{
		long long const refresh = 4167;
		long long const askedFrame = 16667; // 60 a second
		long long now = 0, due = 0;
		int const frames = 100000;

		for (int i = 0; i < frames; i++)
		{
			now += 1000;
			due = HudsonNextFrameDue(due, now, askedFrame);

			while (now < due)
			{
				now += 1000; // Sleep(1)
			}

			now = (now / refresh + 1) * refresh;
		}

		double const reached = (double)frames * 1000000.0 / (double)now;

		Check(reached > 59.5 && reached < 60.5, "60 a second on a 240 Hz screen is really 60, because a late frame is made up by the next");
	}

	Check(HudsonNextFrameDue(1000, 1000 + 16667 + 100, 16667) == 1000 + 16667, "a frame a little late doesn't move the count");
	Check(HudsonNextFrameDue(1000, 5000000, 16667) == 5000000, "a very late one, like a level loading, starts it again, so there's no burst of frames after");

	{
		std::string const rateScript = ReadSourceFile("tools/hudson/run-hudson.ps1");
		std::string const ratePairScript = ReadSourceFile("tools/hudson/run-net-pair.ps1");
		std::string const rateLevelsScript = ReadSourceFile("tools/hudson/run-campaign-levels.ps1");
		std::string const rateHudson = ReadSourceFile("3dc/avp/hudson.cpp");

		Check(Has(rateScript, "[int]$Fps = 60,") && Has(ratePairScript, "[int]$Fps = 60,") && Has(rateLevelsScript, "[int]$Fps = 60,"), "the run scripts ask for 60 unless told otherwise");
		Check(Has(rateScript, "if ($Uncapped) { $arguments += \" fps=uncapped\" }") && Has(ratePairScript, "$words += '-Uncapped'") && Has(rateLevelsScript, "$arguments += '-Uncapped'"), "and each can ask for no cap by name");
		Check(Has(rateScript, "$_ -like 'frames_each_second_asked=*'"), "a run's own record says what rate it reached");
		Check(Has(rateHudson, "frames_each_second_asked=%d frames_each_second_reached=%.1f"), "and so does its result");
		Check(Has(rateHudson, "hudson: frames each second: asked for %d, reached %.1f"), "and its log");
		Check(Has(rateHudson, "HoldToTheFrameRate();\n\tNoteHowFastThisFrameWent();"), "every frame is timed, whatever the cap");
	}
	Refused("-hudson level=hangar frames=5 fps=9", kHudsonRefusedFramesPerSecond);

	// A pair run can take no pictures of its own, to show what a stall is without them
	Check(HudsonPlan_Read("-hudson level=hangar frames=5 machine=shared net=host pictures=no", &plan) == 0 && plan.noNetPictures, "a pair run can take no pictures");
	Refused("-hudson level=hangar frames=5 pictures=no", kHudsonRefusedPicturesWord);
	Refused("-hudson level=hangar frames=5 machine=shared net=host pictures=yes", kHudsonRefusedPicturesWord);
	Check(Has(ReadSourceFile("3dc/avp/hudson.cpp"), "if (!plan.noNetPictures)\n\t{\n\t\tnetPictureIsDue = true;"), "and that word stops them");
	Check(Has(ReadSourceFile("tools/hudson/run-hudson.ps1"), "if ($NoPictures) { $arguments += \" pictures=no\" }") && Has(ReadSourceFile("tools/hudson/run-net-pair.ps1"), "if ($NoPictures) { $words += '-NoPictures' }"), "and the scripts can say it");

	// Each player keeps looking at the body until after the picture that's meant to show it, on the host and on the joiner
	Check(Has(ReadSourceFile("3dc/avp/hudson.cpp"), "if (diedAt != 0 && now - diedAt < kLooksAtTheBodyFor)\n\t\t{\n\t\t\tHudson_FaceThePlace(status, whereItWas);"), "the host keeps looking at the body");
	Check(Has(ReadSourceFile("3dc/avp/hudson.cpp"), "if (ghostWentAt != 0 && now - ghostWentAt < kLooksAtTheBodyFor)\n\t{\n\t\tHudson_FaceThePlace(status, whereItWas);"), "and so does the joiner");
	Check(Has(ReadSourceFile("3dc/avp/hudson.cpp"), "kLooksAtTheBodyFor = 5500;") && Has(ReadSourceFile("3dc/avp/hudson.cpp"), "kPictureLongAfterTheDeath = 5000;"), "for longer than the last picture takes to come");
	Check(Has(ReadSourceFile("3dc/avp/hudson.cpp"), "if (diedAt != 0 && !saidLongAfter && now - diedAt >= kPictureLongAfterTheDeath)") && Has(ReadSourceFile("3dc/avp/hudson.cpp"), "if (ghostWentAt != 0 && !saidLongAfter && now - ghostWentAt >= kPictureLongAfterTheDeath)"), "each machine takes the later picture");
	Check(!Has(ReadSourceFile("3dc/avp/hudson.cpp"), "diedAt = now;\n\t\t\tHudson_AskForAPicture();") && !Has(ReadSourceFile("3dc/avp/hudson.cpp"), "ghostWentAt = now;\n\t\tHudson_AskForAPicture();"), "and neither stops the game for a picture at the death");

	// Killing a level's own named NPC
	{
		char const *const allOfIt = "-hudson level=hangar frames=5 machine=shared net=host coop=yes killkind=queen killat=-69532,893,-45326 killer=join";

		Check(HudsonPlan_Read(allOfIt, &plan) == 0 && plan.killKind == kHudsonBotFightQueen && plan.killX == -69532 && plan.killY == 893 && plan.killZ == -45326 && !plan.killerIsHost, "a run can be asked to have the joiner kill a level's own NPC, at a place that's below zero");
		Check(HudsonPlan_Read("-hudson level=hangar frames=5 machine=shared net=host coop=yes killkind=marine killat=1,2,3 killer=host", &plan) == 0 && plan.killKind == kHudsonBotFightMarine && plan.killerIsHost, "or the host");
		Refused("-hudson level=hangar frames=5 machine=shared net=host coop=yes killkind=queen killat=1,2,3", kHudsonRefusedKillWords);
		Refused("-hudson level=hangar frames=5 machine=shared net=host coop=yes killkind=queen killer=host", kHudsonRefusedKillWords);
		Refused("-hudson level=hangar frames=5 machine=shared net=host coop=yes killat=1,2,3 killer=host", kHudsonRefusedKillWords);
		Refused("-hudson level=hangar frames=5 killkind=queen killat=1,2,3 killer=host", kHudsonRefusedKillWords);
		Refused("-hudson level=hangar frames=5 machine=shared net=host killkind=queen killat=1,2,3 killer=host", kHudsonRefusedKillWords);
		Refused("-hudson level=hangar frames=5 machine=shared net=host coop=yes killkind=alien killat=1,2,3 killer=host", kHudsonRefusedKillWords);
		Refused("-hudson level=hangar frames=5 machine=shared net=host coop=yes killkind=queen killat=1,2,3 killer=both", kHudsonRefusedKillWords);
		Refused("-hudson level=hangar frames=5 machine=shared net=host coop=yes killkind=queen killat=1,2 killer=host", kHudsonRefusedKillWords);
		Refused("-hudson level=hangar frames=5 machine=shared net=host coop=yes killkind=queen killat=1,2,3,4 killer=host", kHudsonRefusedKillWords);
		Refused("-hudson level=hangar frames=5 machine=shared net=host coop=yes killkind=queen killat=a,b,c killer=host", kHudsonRefusedKillWords);
		Refused("-hudson level=hangar frames=5 machine=shared net=host coop=yes killkind=queen killat=1,,3 killer=host", kHudsonRefusedKillWords);

		std::string const killScript = ReadSourceFile("tools/hudson/run-hudson.ps1");
		std::string const killPairScript = ReadSourceFile("tools/hudson/run-net-pair.ps1");

		Check(Has(killScript, "killat=$($KillAt -replace 'm', '-') killer=$Killer"), "the script says all three words");
		Check(Has(killPairScript, "'-KillAt', $KillAt, '-Killer', $Killer"), "and the pair's script passes them on");
	}

	Refused("-hudson level=hangar frames=5 fps=1001", kHudsonRefusedFramesPerSecond);
	Refused("-hudson level=hangar frames=5 fps=fast", kHudsonRefusedFramesPerSecond);
	Check(HudsonPlan_Read("-hudson level=hangar frames=5 ratetest=fall", &plan) == 0 && plan.rateTest == kHudsonRateTestFall, "a run can measure a fall");
	Check(HudsonPlan_Read("-hudson level=hangar frames=5 ratetest=flame", &plan) == 0 && plan.rateTest == kHudsonRateTestFlame, "a run can measure a flamethrower");
	Refused("-hudson level=hangar frames=5 ratetest=walk", kHudsonRefusedRateTest);
	Check(HudsonPlan_Read("-hudson level=fall_m frames=5 machine=shared net=host coop=yes ghostlog=yes", &plan) == 0 && plan.ghostLog, "a run can log every NPC ghost");
	Refused("-hudson level=fall_m frames=5 ghostlog=no", kHudsonRefusedBotFightUnknown);
	Check(HudsonPlan_Read("-hudson level=fall frames=5 machine=shared net=host coop=yes botfight=marine botown=yes botstay=yes botweapon=plasma", &plan) == 0 && plan.botOwn && plan.botWeapon == kHudsonBotWeaponPlasma, "a fight can be with the level's own NPC, with a Predator's own weapon");
	Refused("-hudson level=fall frames=5 machine=shared net=host coop=yes botfight=marine botweapon=rifle", kHudsonRefusedBotWeapon);
	Check(HudsonPlan_Read("-hudson level=fall_m frames=5 machine=shared net=host coop=yes", &plan) == 0 && plan.deathWay == -1, "nobody has to die any special way");
	Check(HudsonPlan_Read("-hudson level=fall_m frames=5 machine=shared net=host coop=yes deathway=3", &plan) == 0 && plan.deathWay == 3, "a run can ask for one way of dying");
	Refused("-hudson level=fall_m frames=5 deathway=100", kHudsonRefusedDeathWay);
	Refused("-hudson level=fall_m frames=5 deathway=fast", kHudsonRefusedDeathWay);
	Check(HudsonPlan_Read("-hudson level=hangar frames=5 machine=shared vsync=off", &plan) == 0 && plan.vsyncOff, "a run sharing the machine can still leave the screen out of it");
	Refused("-hudson level=hangar frames=5 vsync=on", kHudsonRefusedVsync);
	Refused("-hudson level=hangar frames=5 botsingle=yes", kHudsonRefusedBotFightWithoutCoop);
	Refused("-hudson level=hangar frames=5 machine=shared net=host coop=yes botfight=queen botsingle=yes", kHudsonRefusedBotFightWithoutCoop);
	Refused("-hudson level=invasion frames=5 machine=shared net=host coop=yes botfight=wombat", kHudsonRefusedBotFightUnknown);
	Refused("-hudson level=invasion frames=5 machine=shared net=host botfight=marine", kHudsonRefusedBotFightWithoutCoop);
	Refused("-hudson level=invasion frames=5 botfight=marine", kHudsonRefusedBotFightWithoutCoop);
}

static void CheckFriendlyFire()
{
	puts("friendlyfire=");

	HudsonPlan plan;
	Check(HudsonPlan_Read("-hudson level=invasion frames=5 machine=shared net=host coop=yes friendlyfire=off", &plan) == 0 && plan.noFriendlyFire, "the host can turn it off");
	Check(HudsonPlan_Read("-hudson level=invasion frames=5 machine=shared net=join coop=yes friendlyfire=off", &plan) == 0 && plan.noFriendlyFire, "and a joiner can be told it's meant to be off, to read in its log");
	Check(HudsonPlan_Read("-hudson level=invasion frames=5 machine=shared net=host coop=yes", &plan) == 0 && !plan.noFriendlyFire, "without the word, it stays on");
	Refused("-hudson level=invasion frames=5 machine=shared net=host coop=yes friendlyfire=on", kHudsonRefusedFriendlyFireNotOff);
	Refused("-hudson level=invasion frames=5 machine=shared net=host coop=yes friendlyfire=", kHudsonRefusedFriendlyFireNotOff);
	Refused("-hudson level=invasion frames=5 machine=shared net=host coop=yes friendlyfire=off friendlyfire=off", kHudsonRefusedAskedTwice);
	Refused("-hudson level=invasion frames=5 machine=shared net=host friendlyfire=off", kHudsonRefusedFriendlyFireWithoutCoop);
	Refused("-hudson level=invasion frames=5 friendlyfire=off", kHudsonRefusedFriendlyFireWithoutCoop);
	Check(Has(ReadSourceFile("tools/hudson/run-hudson.ps1"), "if ($NoFriendlyFire) { $arguments += \" friendlyfire=off\" }"), "the run script can ask for it");
	Check(Has(ReadSourceFile("tools/hudson/run-net-pair.ps1"), "if ($NoFriendlyFire) { $words += '-NoFriendlyFire' }"), "and so can the pair script");
}

// What tools/hudson/join-films believes about its command line and about a film, which are both from outside
static void SecondsAre(const char *text, long expected)
{
	long got = -1;
	char words[200];

	_snprintf(words, sizeof(words) - 1, "[%s] is read as %ld ms", text, expected);
	words[sizeof(words) - 1] = 0;
	Check(HudsonPlan_ReadFilmSeconds(text, &got) == 0 && got == expected, words);
}

static void SecondsRefused(const char *text, const char *reason)
{
	long got = -1;
	char words[300];

	_snprintf(words, sizeof(words) - 1, "[%s] is refused: %s", text ? text : "nothing", reason);
	words[sizeof(words) - 1] = 0;
	const char *const why = HudsonPlan_ReadFilmSeconds(text, &got);
	Check(why != 0 && strcmp(why, reason) == 0 && got == -1, words);
}

static void TestFilmJoining()
{
	printf("What join-films believes:\n");

	SecondsAre("7.4", 7400);
	SecondsAre("7.053", 7053);
	SecondsAre("85.97", 85970);
	SecondsAre("0", 0);
	SecondsAre("12", 12000);
	SecondsAre("3600", 3600000);
	SecondsRefused("", kHudsonSecondsMissing);
	SecondsRefused(0, kHudsonSecondsMissing);
	SecondsRefused("7,4", kHudsonSecondsHaveAComma);
	SecondsRefused("00001", kHudsonSecondsTooManyFigures);
	SecondsRefused("7.4567", kHudsonSecondsTooManyFigures);
	SecondsRefused(".5", kHudsonSecondsNotANumber);
	SecondsRefused("-7", kHudsonSecondsNotANumber);
	SecondsRefused("7.", kHudsonSecondsNotANumber);
	SecondsRefused("7.4x", kHudsonSecondsNotANumber);
	SecondsRefused("7.4.1", kHudsonSecondsNotANumber);
	SecondsRefused(" 7", kHudsonSecondsNotANumber);
	SecondsRefused("3600.001", kHudsonSecondsTooMany);

	// A picture of ten rows of 40 bytes, in a buffer of 400 that starts at 1000
	Check(HudsonPlan_RowsFit(1000, 40, 10, 40, 1000, 400), "rows stored top down that fill their buffer exactly fit");
	Check(!HudsonPlan_RowsFit(1000, 40, 10, 40, 1000, 399), "but not with one byte fewer");
	Check(!HudsonPlan_RowsFit(999, 40, 10, 40, 1000, 400), "nor starting one byte before the buffer");
	Check(HudsonPlan_RowsFit(1360, -40, 10, 40, 1000, 400), "rows stored bottom up fit, counted down from the last row in the buffer");
	Check(!HudsonPlan_RowsFit(1359, -40, 10, 40, 1000, 400), "but not if the last one starts a byte before the buffer");
	Check(!HudsonPlan_RowsFit(1361, -40, 10, 40, 1000, 400), "nor if the first goes a byte past its end");
	Check(!HudsonPlan_RowsFit(1000, 39, 10, 40, 1000, 400), "rows longer than the step between them don't fit");
	Check(!HudsonPlan_RowsFit(1000, 40, 0, 40, 1000, 400), "and a picture of no rows isn't one");

	Check(HudsonPlan_ApertureFits(0, 0, 1920, 1080, 1920, 1088), "a film's shown part can be smaller than its picture");
	Check(HudsonPlan_ApertureFits(0, 8, 1920, 1080, 1920, 1088), "and lower down in it");
	Check(!HudsonPlan_ApertureFits(0, 9, 1920, 1080, 1920, 1088), "but not going past its bottom");
	Check(!HudsonPlan_ApertureFits(1, 0, 1920, 1080, 1920, 1088), "nor past its right side");
	Check(!HudsonPlan_ApertureFits(-1, 0, 1920, 1080, 1920, 1088), "nor starting left of it");
	Check(!HudsonPlan_ApertureFits(0, -1, 1920, 1080, 1920, 1088), "nor above it");
	Check(!HudsonPlan_ApertureFits(0, 0, 0, 1080, 1920, 1088), "and a shown part of no size isn't one");

	std::string const join = ReadSourceFile("tools/hudson/join-films.cpp");

	Check(Has(join, "HudsonPlan_ReadFilmSeconds(picture ? argv[3] : argv[2], &milliseconds)") && !Has(join, "atof("), "join-films reads its seconds with that, and with nothing else");
	Check(Has(join, "bool const fits = HudsonPlan_RowsFit(") && Has(join, "if (fits)") && ComesBefore(join, "bool const fits = HudsonPlan_RowsFit(", "memcpy("), "it reads no picture until its rows are known to fit");
	Check(Has(join, "!= INVALID_FILE_ATTRIBUTES") && CountOf(join, "if (AlreadyThere(outPath))") == 2, "and it writes over no file that's already there, a source film included");
}

int main()
{
	CheckBotFight();
	CheckFriendlyFire();
	printf("What a Hudson run has been asked to do:\n");

	TestWhoAsked();
	TestGoodPlans();
	TestBadPlans();
	TestFolders();
	TestMarinePlans();
	TestWallPlans();
	TestPicturesNames();
	TestWallSpots();
	TestWallViews();
	TestTheWaysOfDrawing();
	TestTheRealGame();
	TestTourAndSharedMachine();
	TestFilmJoining();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");

	return checksFailed ? 1 : 0;
}
