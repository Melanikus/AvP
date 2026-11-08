// Tests the mode that plays as single player does on a network game's level lists: what each place on a list
// means, where a second player is tried, and that the game's code is wired to them. Written 2026-11-06 by
// Melanikus for the AvP port.
//
// Nothing here needs the game or its data. It reads the source for the wiring. That a level really starts that
// way takes a run: listplace= starts a skirmish from the level row, and its "meets:" lines are compared with
// the same level in single player.

#include "avp/win95/frontend/level_lists.h"
#include "avp/player_spots.h"
#include "avp/win95/shared_roll.h"
#include "avp/level_events.h"

#include <stdio.h>
#include <stdlib.h>

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

static int Times(const std::string &text, const char *what)
{
	int times = 0;
	std::string::size_type at = 0;
	std::string const wanted(what);

	while ((at = text.find(wanted, at)) != std::string::npos)
	{
		times++;
		at += wanted.size();
	}

	return times;
}

static std::string Between(const std::string &text, const char *from, const char *to)
{
	std::string::size_type const start = text.find(from);

	if (start == std::string::npos)
	{
		return "";
	}

	std::string::size_type const end = text.find(to, start + 1);

	return text.substr(start, end == std::string::npos ? std::string::npos : end - start);
}

static void TestTheNumbers()
{
	Check(kLevelListCampaignLevels == 32, "32 single player levels");
	Check(kLevelListMarineLevels == 11 && kLevelListPredatorLevels == 11 && kLevelListAlienLevels == 10, "11, 11 and 10");
	Check(LevelList_FirstCustom(14) == 46, "custom levels start after them on the deathmatch list");
	Check(LevelList_FirstCustom(13) == 45, "and on the co-op list");
	Check(LevelList_FirstCustom(14) < 100, "and every number is below the 100 a network game uses to mean a custom level");
}

static void TestWhatEachPlaceIs()
{
	int const stock = 13;
	int campaign = 77;
	int level = 77;

	// the old numbers stay what they were
	for (int place = 0; place < stock; place++)
	{
		Check(!LevelList_IsSinglePlayerLevel(place, stock) && !LevelList_IsCustom(place, stock), "a level made for multiplayer is neither");
		Check(!LevelList_CampaignAt(place, stock, &campaign, &level), "and it's no campaign's");
	}

	Check(campaign == 77 && level == 77, "and that leaves the answers alone");

	Check(LevelList_CampaignAt(13, stock, &campaign, &level) && campaign == kCampaignMarine && level == 0, "the first one is the Marine's first");
	Check(LevelList_CampaignAt(23, stock, &campaign, &level) && campaign == kCampaignMarine && level == 10, "then his last");
	Check(LevelList_CampaignAt(24, stock, &campaign, &level) && campaign == kCampaignPredator && level == 0, "then the Predator's first");
	Check(LevelList_CampaignAt(34, stock, &campaign, &level) && campaign == kCampaignPredator && level == 10, "then his last");
	Check(LevelList_CampaignAt(35, stock, &campaign, &level) && campaign == kCampaignAlien && level == 0, "then the Alien's first");
	Check(LevelList_CampaignAt(44, stock, &campaign, &level) && campaign == kCampaignAlien && level == 9, "then its last");

	campaign = 77;
	Check(!LevelList_CampaignAt(45, stock, &campaign, &level) && campaign == 77, "and the next isn't one");
	Check(LevelList_IsCustom(45, stock) && !LevelList_IsSinglePlayerLevel(45, stock), "it's the first custom level");
	Check(!LevelList_IsSinglePlayerLevel(-1, stock), "a number below the list is none of them");
	Check(!LevelList_IsSinglePlayerLevel(12, stock), "the last old one is still old");
	Check(LevelList_IsSinglePlayerLevel(13, stock), "and the first new one is new");

	// Every one of the 32 there and back
	int seen = 0;

	for (int place = stock; place < LevelList_FirstCustom(stock); place++)
	{
		if (LevelList_CampaignAt(place, stock, &campaign, &level) && LevelList_PlaceOf(stock, campaign, level) == place)
		{
			seen++;
		}
	}

	Check(seen == 32, "all 32 can be found again from their campaign and place");

	Check(LevelList_PlaceOf(stock, kCampaignNone, 0) == -1, "no campaign has no place");
	Check(LevelList_PlaceOf(stock, kCampaignMarine, 11) == -1, "the Marine has no level after his last");
	Check(LevelList_PlaceOf(stock, kCampaignPredator, 11) == -1, "nor the Predator");
	Check(LevelList_PlaceOf(stock, kCampaignAlien, 10) == -1, "nor the Alien");
	Check(LevelList_PlaceOf(stock, kCampaignAlien, -1) == -1, "and none before the first");
	Check(LevelList_PlaceOf(14, kCampaignPredator, 1) == 14 + 11 + 1, "on the other list they're one further on");
}

static void TestTheSpots()
{
	bool noneOnHim = true;
	bool allDifferent = true;
	bool closeEnough = true;

	for (int i = 0; i < kPlayerSpotCount; i++)
	{
		noneOnHim = noneOnHim && !(kPlayerSpots[i].stepsRight == 0 && kPlayerSpots[i].stepsForward == 0);
		closeEnough = closeEnough && abs(kPlayerSpots[i].stepsRight) <= 4 && abs(kPlayerSpots[i].stepsForward) <= 4;

		for (int j = i + 1; j < kPlayerSpotCount; j++)
		{
			allDifferent = allDifferent && !(kPlayerSpots[i].stepsRight == kPlayerSpots[j].stepsRight && kPlayerSpots[i].stepsForward == kPlayerSpots[j].stepsForward);
		}
	}

	Check(noneOnHim, "no spot is on top of the one he's standing on");
	Check(allDifferent, "no two spots are the same, so two players never share one");
	Check(closeEnough, "and they're all near him");
	Check(kPlayerSpots[0].stepsRight == 1 && kPlayerSpots[0].stepsForward == 0, "the first is right beside him");
	Check(kPlayerSpots[1].stepsRight == -1 && kPlayerSpots[1].stepsForward == 0, "then the other side");
	Check(kPlayerSpots[kPlayerSpotCount - 1].stepsRight == -4, "the last is the furthest along a line");
	Check(kPlayerSpotGap >= 2 * 450 + 100, "and the gap has room for two bodies, 450 mm from middle to edge");
	Check(kPlayerSpotCount >= 7, "and there are enough for 8 players");
}

static void TestTheSharedRoll()
{
	Check(SharedRoll(1, 2, 3, 4) == SharedRoll(1, 2, 3, 4), "the same numbers give the same roll on every machine");
	Check(SharedRoll(1, 2, 3, 4) != SharedRoll(2, 2, 3, 4), "another game's number changes it");
	Check(SharedRoll(1, 2, 3, 4) != SharedRoll(1, 3, 3, 4), "and so does the object's first");
	Check(SharedRoll(1, 2, 3, 4) != SharedRoll(1, 2, 4, 4), "and second");
	Check(SharedRoll(1, 2, 3, 4) != SharedRoll(1, 2, 3, 5), "and third");

	bool inRange = true;
	int counts[4] = { 0, 0, 0, 0 };

	for (int game = -500; game < 500; game++)
	{
		int const roll = SharedRoll(game * 7919, 1000, -2000, 3000);

		inRange = inRange && roll >= 0 && roll <= 0xffff;
		counts[roll % 4]++;
	}

	Check(inRange, "it's always from 0 to 65535, so it can't be negative when it's used to choose a place");
	Check(counts[0] > 150 && counts[1] > 150 && counts[2] > 150 && counts[3] > 150, "and one of four places isn't favoured much");
}

static void TestTheWiring()
{
	std::string const mpConfig = ReadSource("3dc/avp/win95/frontend/mp_config.cpp");
	std::string const envInfo = ReadSource("3dc/avp/win95/frontend/env_info.cpp");
	std::string const menus = ReadSource("3dc/avp/win95/frontend/menus.cpp");
	std::string const pldnet = ReadSource("3dc/avp/win95/pldnet.cpp");
	std::string const inventory = ReadSource("3dc/avp/inventory.cpp");
	std::string const pvisible = ReadSource("3dc/avp/pvisible.cpp");

	// the lists
	Check(Has(mpConfig, "NumMultiplayerLevels = LevelList_FirstCustom(MAX_NO_OF_MULTIPLAYER_EPISODES) + NumCustomLevels;"), "the deathmatch list has room for them");
	Check(Has(mpConfig, "NumCoopLevels = LevelList_FirstCustom(MAX_NO_OF_COOPERATIVE_EPISODES) + NumCustomLevels;"), "and the co-op list");
	Check(Has(mpConfig, "CoopLevelNames[MAX_NO_OF_COOPERATIVE_EPISODES + i] = NewSinglePlayerLevelName(i, TRUE);"), "they're named on the co-op list");
	Check(Has(mpConfig, "MultiplayerLevelNames[MAX_NO_OF_MULTIPLAYER_EPISODES + i] = NewSinglePlayerLevelName(i, FALSE);"), "and on the other");
	Check(Has(mpConfig, "CoopLevelNames[i+LevelList_FirstCustom(MAX_NO_OF_COOPERATIVE_EPISODES)] = CustomLevelNameList[i];"), "custom levels go after them");
	Check(Has(mpConfig, "MultiplayerLevelNames[i+LevelList_FirstCustom(MAX_NO_OF_MULTIPLAYER_EPISODES)] = CustomLevelNameList[i];"), "on both lists");
	Check(Has(mpConfig, "for (int i = LevelList_FirstCustom(MAX_NO_OF_COOPERATIVE_EPISODES); i < NumCoopLevels; i++)"), "a custom level is looked for among the custom levels");
	Check(Has(mpConfig, "for (int i = LevelList_FirstCustom(MAX_NO_OF_MULTIPLAYER_EPISODES); i < NumMultiplayerLevels; i++)"), "on both lists");
	Check(Has(mpConfig, "return LevelList_IsCustom(index, gameType == NGT_Coop ? MAX_NO_OF_COOPERATIVE_EPISODES : MAX_NO_OF_MULTIPLAYER_EPISODES);"), "one of the 32 doesn't count as custom");
	Check(Has(mpConfig, "if (index < LevelList_FirstCustom(MAX_NO_OF_COOPERATIVE_EPISODES)) {"), "a joiner accepts one of the 32 on the co-op list");
	Check(Has(mpConfig, "if (index < LevelList_FirstCustom(MAX_NO_OF_MULTIPLAYER_EPISODES)) {"), "and on the other");
	Check(Has(mpConfig, "if (LevelList_IsSinglePlayerLevel(index, gameType == NGT_Coop ? MAX_NO_OF_COOPERATIVE_EPISODES : MAX_NO_OF_MULTIPLAYER_EPISODES))\n\t{\n\t\treturn return_string;"), "one of the 32 isn't looked for by name, so a saved setup keeps it");

	// loading it
	Check(Has(envInfo, "int const singlePlayerLevel = SinglePlayerLevelAtPlace(episode, MAX_NO_OF_MULTIPLAYER_EPISODES);"), "a level on the deathmatch list loads");
	Check(Has(envInfo, "int const singlePlayerLevel = SinglePlayerLevelAtPlace(episode, MAX_NO_OF_COOPERATIVE_EPISODES);"), "and on the co-op list");
	Check(Has(envInfo, "sizeof(MarineEpisodes) / sizeof(MarineEpisodes[0]) == kLevelListMarineLevels + 1"), "the Marine's real list is as long as the numbers here say");
	Check(Has(envInfo, "sizeof(PredatorEpisodes) / sizeof(PredatorEpisodes[0]) == kLevelListPredatorLevels + 1"), "and the Predator's");
	Check(Has(envInfo, "sizeof(AlienEpisodes) / sizeof(AlienEpisodes[0]) == kLevelListAlienLevels + 1"), "and the Alien's");
	Check(Has(envInfo, "if (netGameData.gameType == NGT_Coop)\n\t{\n\t\tLevelList_CampaignAt(netGameData.levelNumber, MAX_NO_OF_COOPERATIVE_EPISODES, &campaign, &campaignLevel);"), "only the co-op list starts the mode");
	Check(Has(envInfo, "netGameData.levelNumber = LevelList_PlaceOf(MAX_NO_OF_COOPERATIVE_EPISODES, netGameData.coopCampaign, netGameData.coopCampaignLevel);"), "the level row follows the campaign");

	// choosing it
	Check(Has(menus, "if (AvPMenus.CurrentMenu != AVPMENU_MULTIPLAYER_CONFIG_JOIN)\n\t{\n\t\tCampaign_SetFromLevelRow();"), "the level row chooses it on a host's page, not a joiner's");
	Check(Has(Between(menus, "case AVPMENU_ELEMENT_STARTMPGAME:", "case AVPMENU_ELEMENT_JOINMPGAME:"), "Campaign_SetFromLevelRow();"), "and once more as the game starts");
	Check(Has(Between(menus, "case AVPMENU_ELEMENT_STARTMPGAME:", "case AVPMENU_ELEMENT_JOINMPGAME:"), "SetLevelToLoadForThisNetworkGame();"), "the host loads it");
	Check(Has(Between(menus, "case AVPMENU_ELEMENT_JOINMPGAME:", "case AVPMENU_ELEMENT_LISTCHOICE:"), "SetLevelToLoadForThisNetworkGame();"), "and so does a joiner");
	Check(Has(Between(envInfo, "void SetLevelToLoadForThisNetworkGame(void)", "else if (netGameData.gameType == NGT_Coop)"), "Coop_TakeCampaignSpecies();\n\t\tSetLevelToLoadForCampaign();"), "and everyone plays the campaign's species, set just before it loads");
	Check(Has(menus, "SetBriefingTextForEpisode(netGameData.coopCampaignLevel, species);"), "the briefing is single player's");
	Check(Has(pldnet, "\tCharacterSubTypesAvailable[NGSCT_M79] = netGameData.maxMarineM79;\n\tCampaignFixesWhoMayBeWhat();"), "everyone is the campaign's species");
	Check(Has(pldnet, "CharacterTypesAvailable[NGCT_Alien] = NET_MAXPLAYERS;"), "and that can be an Alien");
	Check(Has(pldnet, "while (count <= kClassSliderLast);"), "looking for the species reaches every place on the slider");

	// films
	Check(Has(menus, "LoadGameRequest == SAVELOAD_REQUEST_NONE && SameAsSinglePlayer())"), "a deathmatch gets no film");
	Check(Times(menus, "PlayFilmWithTheNetworkGoing(\"FMVs/") == 6, "and all six films keep the network going");
	Check(Has(menus, "WhileAFilmPlays = KeepTheNetworkGoing;"), "by this");
	Check(Has(menus, "static bool KeepTheNetworkGoing(void)\n{\n\tNet_ServiceNetwork();\n\treturn true;\n}"), "which only looks after the connection, because reading messages could act on a level that isn't loaded");
	Check(Has(ReadSource("3dc/FmvCutscenes.cpp"), "if (WhileAFilmPlays && !WhileAFilmPlays()) {\n\t\t\ttoldToStop = true;"), "which is called while it plays, and can stop it");
	Check(Has(ReadSource("3dc/FmvCutscenes.cpp"), "if ((GotAnyKey && DebouncedGotAnyKey) || toldToStop)"), "as a key would");

	// what a player meets
	Check(Has(inventory, "A game that plays as single player does has single player's kit instead.\n\t\t\tif(!SameAsSinglePlayer())"), "single player's kit");
	Check(Times(inventory, "if(!SameAsSinglePlayer())\n\t\t\t{\n\t\t\t\t/*if in a multiplayer game , check to see if player is a specialist marine*/") == 2, "and single player's ammunition and guns to pick up");
	Check(Has(inventory, "return !SameAsSinglePlayer() && weaponID == WEAPON_SONICCANNON;"), "the sniper rifle can be picked up");
	Check(Has(inventory, "if (!SameAsSinglePlayer() && ammoID == AMMO_SNIPER)"), "and its ammunition");
	Check(Has(inventory, "if (!SameAsSinglePlayer() && netGameData.allowStaff)"), "the host's staff setting doesn't count");
	Check(Has(ReadSource("3dc/avp/inventory.h"), "(netGameData.pistolInfiniteAmmo && !SameAsSinglePlayer())"), "nor unlimited pistol ammunition");
	Check(Has(pvisible, "if(!SameAsSinglePlayer())\n\t\t{\n\t\t\t//if in a network game , see if this is an allowable weapon"), "the level's guns are all there");
	Check(Has(pvisible, "if(netGameData.timeForRespawn>0 && !SameAsSinglePlayer())"), "and don't come back");
	Check(Has(ReadSource("3dc/avp/win95/projload.cpp"), "if(!SameAsSinglePlayer())\n\t{\n\t\tStartingEquipment.predator_pistol=netGameData.allowPistol;"), "the level's own starting equipment");
	Check(Times(ReadSource("3dc/avp/dynamics.cpp"), "(!netGameData.fallingDamage && !SameAsSinglePlayer())") == 2, "falling hurts");
	Check(Times(ReadSource("3dc/avp/bh_light.cpp"), "netGameData.preDestroyLights && !SameAsSinglePlayer()") == 2, "and the lights are on");

	// limits and the screen
	Check(Has(pldnet, "if (netGameData.timeLimit > 0 && !SameAsSinglePlayer())\n\t\t\t{\n\t\t\t\tif (((netGameData.GameTimeElapsed"), "no time limit");
	Check(Has(pldnet, "if (netGameData.scoreLimit > 0 && !SameAsSinglePlayer())"), "no score limit");
	Check(Has(pldnet, "if (!SameAsSinglePlayer() && (score || IsItPossibleToScore()))"), "no score on the screen");
	Check(Has(pldnet, "// show time left\n\tif (netGameData.timeLimit > 0 && !SameAsSinglePlayer())"), "no time on the screen");

	// where he goes
	std::string const teleport = Between(pldnet, "\nvoid TeleportNetPlayerToAStartingPosition(STRATEGYBLOCK *playerSbPtr, int startOfGame)\n{\n\tint numStartPositions = 0;\n\tMULTIPLAYER_START *startPositions = 0;\n\tint sbIndex = 0;\n\tint start_index;\n\tint numChecked = 0;\n\tint bestDistance", "\n#endif");

	Check(Has(teleport, "if (netGameData.coopCampaign != kCampaignNone)\n\t{\n\t\tpsPtr->invulnerabilityTimer = 0;"), "a campaign's level starts with no time of being safe");
	Check(Has(teleport, "PlacePlayerWithNoMultiplayerStart(playerSbPtr);\n\t\treturn;\n\t}\n\n\tif (MultiplayerRestartSeed)"), "and a campaign doesn't look for multiplayer starts, even on starting again");
	Check(Has(teleport, "if (!numStartPositions) {\n\t\tPlacePlayerWithNoMultiplayerStart(playerSbPtr);"), "no starts means this, not staying at 0,0,0");
	Check(Has(pldnet, "if (netGameData.playerData[i].playerId == AvPNetID) {\n\t\t\t\tPlacePlayerWithNoMultiplayerStart(playerSbPtr);"), "and after a restart too");
	Check(Has(pldnet, "if (HowFarALineRuns(&from, &way, distance + kBodyWidth) < distance + kBodyWidth)"), "he has to be able to walk there");
	Check(Has(pldnet, "if (toFloor >= mostDown || toFloor < kChestHeight - kMostStepUp)"), "and there's a floor");
	Check(Has(pldnet, "if (HowFarALineRuns(&chest, &up, BodyHeight() - kChestHeight) < BodyHeight() - kChestHeight)"), "and room for his head");
	Check(Has(pldnet, "AvP.PlayerType == I_Predator ? CE_PREDATOR : AvP.PlayerType == I_Alien ? CE_ALIEN : CE_MARINE"), "however tall what he's playing is");
	Check(Has(pldnet, "if (HowFarALineRuns(&chest, &sideways, kBodyWidth) < kBodyWidth)"), "and for his sides");
	Check(Has(pldnet, "if (ModuleFromPosition(&chest, (MODULE *)0) == 0)"), "and it's inside the level");
	Check(Has(pldnet, "if (!SomeoneIsOnThisSpot(candidate))"), "and nobody's on it");
	Check(Has(pldnet, "if (Magnitude(&apart) < kFarFromTheStart)"), "someone joining late goes to a teammate only if they've gone on ahead");

	// dying
	std::string const pmove = ReadSource("3dc/avp/pmove.cpp");
	std::string const whileOut = Between(pldnet, "void Coop_WhileOut(bool asksToChangeWhoHeWatches)", "bool SameAsSinglePlayer(void)");
	std::string const everyoneDead = Between(pldnet, "bool Coop_EveryoneIsDead(void)", "static const int kSecondsToWaitWithOthersWatching");

	Check(Has(pmove, "if (SameAsSinglePlayer())\n\t{\n\t\tCoop_WhileOut(psPtr->Mvt_InputRequests.Flags.Rqst_Operate != 0);\n\t\treturn;\n\t}\n\n\t/* check for re-spawn */"), "a dead player in the mode doesn't come back");
	Check(Has(whileOut, "if (someoneElseIsHere && (asksToChangeWhoHeWatches || !MultiplayerObservedPlayer))\n\t{\n\t\tGetNextMultiplayerObservedPlayer();"), "he watches someone, and use moves on, but never himself");
	Check(Has(Between(pldnet, "static void Coop_GoToPlace(int place)", "netGameData.coopCampaignLevel = place;"), "TurnOffMultiplayerObserveMode();"), "the watcher lets go at the next level");
	Check(Has(pldnet, "because watching works by moving the player to where they are.\n\tTurnOffMultiplayerObserveMode();"), "by this");
	Check(Has(whileOut, "if (AvP.Network != I_Host || !Coop_EveryoneIsDead())"), "only the host starts the level again, when everyone is out");
	Check(Has(whileOut, "AddNetMsg_RestartNetworkGame(seed);\n\t\tNetSendMessagesNow();\n\t\tRestartNetworkGame(seed);"), "and it tells everyone, at once");
	Check(Has(whileOut, "bool const timeIsUp = someoneElseIsHere && everyoneOutFor >= kSecondsToWaitWithOthersWatching * ONE_FIXED;"), "alone he waits as long as he likes, as in single player, and others aren't kept waiting");
	Check(Has(everyoneDead, "if (PlayerStatusPtr->IsAlive)\n\t{\n\t\treturn false;"), "this machine's player has to be out");
	Check(Has(everyoneDead, "if (netGameData.playerData[i].playerAlive && FindGhost(netGameData.playerData[i].playerId, GHOST_PLAYEROBJECTID))"), "and so does everyone who's really in the level");
	Check(Has(everyoneDead, "netGameData.coopCampaign == kCampaignNone"), "and it's only for a campaign");
	Check(Has(pldnet, "if (netGameData.maxLives > 0 && !SameAsSinglePlayer())"), "no lives to count");
	Check(Has(pldnet, "(!playerStatusPtr->IsAlive && !MultiplayerObservedPlayer && !SameAsSinglePlayer()))"), "and no score table for a dead player");
	Check(Has(pldnet, "if (netGameData.pointsForRespawn == 0 || SameAsSinglePlayer()) {"), "nothing comes back on points either");
	Check(Has(pldnet, "if (netGameData.coopCampaign != kCampaignNone)\n\t{\n\t\tLogString(\"campaign: the level is starting again\");\n\t\tLevelEvents_LevelStartsAgain();"), "starting again forgets what was reported of the level");
	Check(Has(ReadSource("3dc/avp/hud.cpp"), "if (AvP.Network == I_No_Network || Coop_EveryoneIsDead())"), "everyone out gets single player's screen");
	Check(Has(ReadSource("3dc/avp/game_statistics.cpp"), "AvP.Network == I_Peer ? TEXTSTRING_MULTIPLAYER_WAITFORRESTARTGAME : TEXTSTRING_MULTIPLAYER_PRESSKEYTORESTARTGAME"), "with a word to wait, for those who aren't the host");

	// places that have to agree
	std::string const objectSetup = ReadSource("3dc/avp/win95/object_setup.cpp");

	Check(Has(objectSetup, "SharedRoll(netGameData.coopRollNumber, group, 0, 0) : FastRandom() & 0xffff;"), "a group of objects agrees in a campaign");
	Check(Has(objectSetup, "dieroll=SharedRoll(netGameData.coopRollNumber, bbd->alt_vector[0].vx, bbd->alt_vector[0].vy, bbd->alt_vector[0].vz);"), "and so does one by itself");
	Check(Has(pldnet, "messagePtr->coopRollNumber = netGameData.coopRollNumber;"), "the number goes in the game description");
	Check(Has(pldnet, "netGameData.coopRollNumber = messagePtr->coopRollNumber;"), "and arrives");
	Check(Times(menus, "Campaign_ChooseRollNumber();") == 1, "the host chooses it as a game starts");
	Check(Has(pldnet, "case (NetMT_RespawnPickups):\n\t\t\t{\n\t\t\t\tif (!SameAsSinglePlayer() && (netGameData.myGameState == NGS_Playing ||"), "and a message can't bring back every pick-up");

	// difficulty
	std::string const menuData = ReadSource("3dc/avp/win95/frontend/menu_data.cpp");

	Check(Times(menuData, "{AVPMENU_ELEMENT_TEXTSLIDER,\t{TEXTSTRING_DIFFICULTY_LEVEL},\t{2},\t{&netGameData.coopDifficulty},") == 3, "the host's page, skirmish's and the co-op campaign page have the row");
	Check(Times(menuData, "{AVPMENU_ELEMENT_DUMMYTEXTSLIDER,\t{TEXTSTRING_DIFFICULTY_LEVEL},\t{2},\t{&netGameData.coopDifficulty},") == 2, "and a joiner's two pages show it");
	Check(Has(menus, "row->ElementID = (isCampaign && AvPMenus.CurrentMenu != AVPMENU_MULTIPLAYER_CONFIG_JOIN) ? AVPMENU_ELEMENT_TEXTSLIDER : AVPMENU_ELEMENT_DUMMYTEXTSLIDER;"), "only a campaign's host can change it");
	Check(Has(menus, "row->SliderValuePtr = isCampaign ? &netGameData.coopDifficulty : &mediumToShow;"), "any other game shows Medium");
	Check(Has(Between(menus, "case AVPMENU_ELEMENT_STARTMPGAME:", "case AVPMENU_ELEMENT_JOINMPGAME:"), "Campaign_SetDifficulty();"), "the host plays it");
	Check(Has(Between(menus, "case AVPMENU_ELEMENT_JOINMPGAME:", "case AVPMENU_ELEMENT_LISTCHOICE:"), "Campaign_SetDifficulty();"), "and so does a joiner, whatever his last single player game was");
	Check(Has(envInfo, "AvP.Difficulty = netGameData.coopCampaign != kCampaignNone ? (I_HARDANUFF)netGameData.coopDifficulty : I_Medium;"), "the host's choice in a campaign, and Medium otherwise");
	Check(Has(pldnet, "messagePtr->coopDifficulty = (unsigned char)netGameData.coopDifficulty;"), "it goes in the game description");
	Check(Has(pldnet, "netGameData.coopDifficulty = NetCampaignDifficultyIsGood(messagePtr->coopDifficulty) ? messagePtr->coopDifficulty : I_Medium;"), "and it's checked as it arrives");
	Check(Has(pldnet, "netGameData.coopDifficulty = I_Medium;"), "Medium to start with");
	Check(Has(mpConfig, "ReadLaterSetting(file, &netGameData.coopDifficulty, 0, 2);"), "it's checked as it's read from a saved setup");
	Check(Has(mpConfig, "file.PutInt32LE(netGameData.coopDifficulty);"), "and it's saved");
	Check(Has(pldnet, "!netGameData.playerData[i].playerAlive)"), "and a living one");

	// the version
	std::string const networking = ReadSource("3dc/networking.h");
	// A dead player leaves no gun: anything within 1.6 m of a player is picked up at once, and the next player is 1.5 m away
	std::string const ghost = ReadSource("3dc/avp/win95/pldghost.cpp");
	std::string const droppedGun = Between(pldnet, "static void ProcessNetMsg_CreateWeapon(", "CreateMultiplayerWeaponPickup(&messagePtr->location");

	Check(Has(ghost, "if (!SameAsSinglePlayer())\n\t\t\t   \t{\n\n\t\t\t   \t\tVECTORCH location=Player->ObStrategyBlock->DynPtr->Position;"), "a dead player leaves no gun in a game that plays as single player");
	Check(Has(droppedGun, "if (SameAsSinglePlayer())\n\t{\n\t\tNet_NoteRefused(\"a dropped gun was sent in a game that plays as single player\");\n\t\treturn;"), "and nobody can send one");
	Check(!Has(whileOut, "MakePlayersWeaponPickupVisible"), "and someone out has nothing of his to show");

	// A level starting again keeps its list: an NPC's entry is only written while the file loads, and a restart doesn't load it
	{
		static LevelEventList events;

		LevelEvents_Empty(&events);
		Check(LevelEvents_Add(&events, "aaaaaaaa", "bbbbbbbb", 1, false), "an event goes on the list");
		LevelEvents_Find(&events, "aaaaaaaa")->happened = true;
		LevelEvents_ForgetWhatHappened(&events);
		Check(LevelEvents_Find(&events, "aaaaaaaa") != NULL, "starting again keeps what can happen");
		Check(!LevelEvents_Find(&events, "aaaaaaaa")->happened, "and forgets that it has");
	}

	Check(Has(pldnet, "AvP.Network = I_Host;\n\n\t// A new game, so nothing that happened in the last one counts, even on the same level\n\tLevelEvents_NewLevel();"), "a new game starts a new list on a host");
	Check(Has(pldnet, "AvP.Network  = I_Peer;\n\n\t// A new game, so nothing that happened in the last one counts, even on the same level\n\tLevelEvents_NewLevel();"), "and on a joiner");
	Check(Has(pldnet, "AddNetMsg_GameDescription();\n\t\t\t\t\tLevelEvents_SayAgainWhatHappened();"), "a host says again what has happened, for someone who joined late");
	Check(Has(pldnet, "if (!SameAsSinglePlayer())\n\t\t\t\t\t{\n\t\t\t\t\t\tAddNetMsg_StrategySynch();"), "other games say objects once a second, as they always have");
	Check(Has(pldnet, "if (SameAsSinglePlayer() && timeGetTime() - lastObjectSynch >= kMillisecondsBetweenObjectSynchs)"), "a game that plays as single player says them oftener");
	Check(Has(pldnet, "kMillisecondsBetweenObjectSynchs = 100;"), "10 times a second");
	Check(Has(pldnet, "if (AvP.Network == I_Host && timeGetTime() - lastDescription >= 1000) {"), "a host's loading screen says the description once a second, not every frame");
	Check(Has(ReadSource("3dc/avp/targeting.cpp"), "if ((AvP.Network!=I_No_Network)\n\t   &&(AvP.PlayerType == I_Predator)"), "only a Predator sends sights");
	Check(Has(menus, "netGameData.myStartFlag = 1;\n\n\t\t\t\t// The host has to hear this, or it never says this player has joined. The only other send is on the species page, before this was set.\n\t\t\t\tAddNetMsg_PlayerDescription();"), "a joiner tells the host it has started");

	// What the first runs with two machines showed
	std::string const winmain = ReadSource("3dc/avp/win95/winmain.cpp");
	std::string const levelEnd = Between(pldnet, "void Coop_LevelEndReached(void)", "static void ProcessNetMsg_CoopNextLevel");

	Check(Has(pldnet, "static void NetSendMessagesNow(void)\n{\n\tnetGameData.sendTimer = 0;\n\tNetSendMessages();"), "something urgent doesn't wait for the turn of the clock");
	Check(Has(levelEnd, "AddNetMsg_CoopNextLevel(place);") && Has(levelEnd, "NetSendMessagesNow();\n\tNet_ServiceNetwork();"), "the message moving everyone on goes out before the level stops");
	Check(Has(pldnet, "AddNetMsg_PlayerLeaving();\n\t\tNetSendMessagesNow();"), "so does someone saying he's leaving");
	Check(Has(pldnet, "kMostLeavingsEachSecond = NET_MESSAGEITERATIONS;") && Has(pldnet, "for (int i = 0; i < NET_MESSAGEITERATIONS; i++)\n\t{\n\t\tAddNetMsg_PlayerLeaving();"), "and however many times a machine says it, that's never over the limit on goodbyes");
	Check(Has(pldnet, "AddNetMsg_StartGame();\n\t\tNetSendMessagesNow();"), "and the start of a game");
	Check(Has(pldnet, "// AddNetMsg_EndGame();\n\t\tNetSendMessagesNow();"), "and the end of one");
	Check(Has(pldnet, "if (leftBehind)\n\t\t\t\t{") && Has(pldnet, "Coop_GoToPlace(messagePtr->coopCampaignLevel);"), "someone left behind goes on when he hears of it");
	Check(Has(pldnet, "bool const leftBehind = netGameData.myGameState == NGS_Playing && AvP.MainLoopRunning"), "only if he's actually playing a level");
	// The host's send at a level's end can put its once-a-second description, still naming the old place, behind the word to go on
	Check(Has(pldnet, "\t\t\tif (Coop_LevelChangeIsDue())\n\t\t\t{\n\t\t\t\t// The host's word to go on has come") && Has(pldnet, "\t\t\t}\n\t\t\t// Where the campaign has got to picks a level from a list, so a number that isn't a real place means no campaign at all\n\t\t\telse if (NetCampaignPlaceIsGood("),
		"SOURCE: a description that comes after the word to go on changes neither the campaign nor the place, so a joiner loads the host's next level");
	Check(Has(winmain, "AvP.MainLoopRunning = Coop_LevelChangeIsDue() ? 0 : 1;"), "a level called off while it loads doesn't start");
	Check(Has(pldnet, "VECTORCH spot = ground;"), "the places round a start are on the floor under it");
	Check(Has(pldnet, "if (ABodyFitsHere(&ground, &spot))"), "and they're judged from there");
	Check(Has(pldnet, "if (toTheFloor < kMostDropFromAStart)\n\t{\n\t\tground.vy = lookFrom.vy + toTheFloor;"), "the floor is what's under the start");
	Check(Has(pldnet, "if (playerNumber >= found)\n\t{\n\t\tLogErrorString("), "too many players for the places is an error in the log");

	// A joiner has to use the host's settings, or the machines take different pick-ups off a level as it loads, and the sync then points at the wrong objects. So every gun's "allowed" and every marine class's limit in
	// the game's settings has to be in the game description, and go each way. This reads the settings for itself, so that the next gun to be added can't be left out without this saying so.
	{
		std::string const header = ReadSource("3dc/avp/win95/pldnet.h");
		std::string const settings = Between(header, "typedef struct netgame_gamedata", "} NETGAME_GAMEDATA;");
		std::string const description = Between(header, "typedef struct netmessage_gamedescription", "} NETMESSAGE_GAMEDESCRIPTION;");
		static const char *const kLines[2][2] = { { "\tBOOL ", "allow" }, { "\tint ", "maxMarine" } };
		int found = 0;

		for (int kind = 0; kind < 2; kind++)
		{
			std::string const lineStart = std::string(kLines[kind][0]) + kLines[kind][1];
			size_t at = 0;

			while ((at = settings.find(lineStart, at)) != std::string::npos)
			{
				size_t const nameAt = at + strlen(kLines[kind][0]);
				std::string const name = settings.substr(nameAt, settings.find(';', nameAt) - nameAt);

				found++;
				at = nameAt;
				Check(Has(description, ("unsigned int " + name + ":").c_str()), (name + " is in the game description").c_str());
				Check(Has(pldnet, ("messagePtr->" + name + " = netGameData." + name + ";").c_str()), (name + " is sent").c_str());
				Check(Has(pldnet, ("netGameData." + name + " = messagePtr->" + name + ";").c_str()), (name + " is read").c_str());
			}
		}

		Check(found >= 29, "the settings were really read: 16 guns and 13 limits, or more");
	}

	std::string const versionCheck = ReadSource("3dc/avp/win95/net_version_check.h");

	Check(Has(pldnet, "#include \"net_version_check.h\""), "the build checks the version against the sizes");
	Check(Has(versionCheck, "static_assert(kMultiplayerVersion == kVersionTheseSizesBelongTo,"), "the version has to be the one the sizes were written down for");
	Check(Has(versionCheck, "kSizesNow == kSizesAtThatVersion"), "and the sizes have to be those");
	Check(Has(versionCheck, "2 * sizeof(NETMESSAGE_GAMEDESCRIPTION) +"), "the game description is one of them");
	Check(Has(versionCheck, "61 * kLevelListCampaignLevels;"), "and so is the level list");
	Check(Has(versionCheck, "58 * (sizeof(kNetPartRules) / sizeof(kNetPartRules[0])) +"), "and how many kinds of message there are");
	Check(Has(networking, "const int kMultiplayerVersion = 111;"), "this build's version");
}

int main()
{
	TestTheNumbers();
	TestWhatEachPlaceIs();
	TestTheSpots();
	TestTheSharedRoll();
	TestTheWiring();

	printf("%d checks, %d failed\n", checks, failures);
	return failures == 0 ? 0 : 1;
}
