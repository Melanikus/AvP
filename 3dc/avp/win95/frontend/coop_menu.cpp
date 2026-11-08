// The pages of the co-op campaign: see coop_menu.h. Written 2026-11-07 by Melanikus for the AvP port. Credit: Melanikus and Bobblen
// (the co-op campaign system follows Bobblen's avp-forever, rewritten).

#include "3dc.h"
#include "inline.h"
#include "module.h"
#include "strategy_def.h"
#include "gamedef.h"
#include "bh_types.h"
#include "pldnet.h"
#include "menus.h"
#include "menu_data.h"
#include "env_info.h"
#include "level_lists.h"
#include "mp_config.h"
#include "custom_maps.h"
#include "coop_menu.h"
#include "networking.h"
#include "ourasert.h"
#include "language.h"

#include <stdio.h>
#include <string.h>

#include <vector>

extern AVPMENU_ELEMENT AvPMenu_CoopHost[];
extern AVPMENU_ELEMENT AvPMenu_CoopJoin[];
extern AVPMENU_ELEMENT AvPMenu_CoopOut[];
extern void MinimalNetCollectMessages(void);
extern void SetDefaultMultiplayerConfig();
extern texID_t MISSING_TEXTURE;
extern texID_t AVPMENUGFX_MARINE_EPISODE1;
extern texID_t AVPMENUGFX_MARINE_EPISODE7;
extern texID_t AVPMENUGFX_PREDATOR_EPISODE1;
extern texID_t AVPMENUGFX_ALIEN_EPISODE1;

int CoopRowCampaign = 0;
int CoopRowPlace = 0;
int CoopRowSpecies = 0;
char CoopRowPlayers[kCoopRowTextBytes] = "";
char CoopRowNoMap[kCoopRowTextBytes] = "";
char CoopRowMapLine[kCoopRowTextBytes] = "";

// The game's list of levels gives a custom level this number, so a game's description is the same for every custom level
static const uint16_t kLevelNumberOfACustomLevel = 100;

// The names on the Start at row. It holds pointers, and everything it points at stays where it is while the page is open.
static std::vector<char *> placeNames;
static int campaignOnTheRow = kCampaignNone;

// The text of each joiner's row on the host's page
static char removeRowText[NET_MAXPLAYERS - 1][kCoopRowTextBytes];
static int removeRowsShown = 0;
static bool playingACustomMapWithOthers = false;

static AVPMENU_ELEMENT *RowOf(AVPMENU_ELEMENT *page, int textOfTheRow)
{
	for (AVPMENU_ELEMENT *row = page; row->ElementID != AVPMENU_ELEMENT_ENDOFMENU; row++)
	{
		if (row->TextDescription == (uint32_t)textOfTheRow)
		{
			return row;
		}
	}

	// The rows are in menu_data.cpp, and coop_lobby_test checks they're all there
	GLOBALASSERT(0);
	return page;
}

// Fills the Start at row for this campaign. A joiner has only the one map the host named.
static void FillPlaceNames(AVPMENU_ELEMENT *page, int campaign, bool isTheHostsPage)
{
	placeNames.clear();

	if (campaign == kCampaignCustom)
	{
		if (!isTheHostsPage)
		{
			placeNames.push_back(netGameData.coopCustomMap);
		}

		for (int map = 0; isTheHostsPage && map < CustomMaps_Count(); map++)
		{
			placeNames.push_back(const_cast<char *>(CustomMaps_Name(map)));
		}
	}
	else
	{
		int const firstName = campaign == kCampaignPredator ? TEXTSTRING_PREDATORLEVELS_1 : campaign == kCampaignAlien ? TEXTSTRING_ALIENLEVELS_1 : TEXTSTRING_MARINELEVELS_1;

		for (int place = 0; place < Campaign_LevelCount(campaign); place++)
		{
			placeNames.push_back(GetTextString((enum TEXTSTRING_ID)(firstName + place)));
		}
	}

	// A joiner who hasn't heard the host's choice yet has nothing to show
	if (placeNames.empty())
	{
		placeNames.push_back(GetTextString(TEXTSTRING_BLANK));
	}

	AVPMENU_ELEMENT *const row = RowOf(page, TEXTSTRING_COOP_START_AT);

	row->TextSliderStringPointer = placeNames.data();
	row->MaxSliderValue = (int)placeNames.size() - 1;
	campaignOnTheRow = campaign;
}

// ---- The host's page

// The rows are the truth: what they say goes into the game's own numbers, and from there into the host's choice.
static void HostRowsToTheGame(void)
{
	// A custom map is only offered if there is one to play
	int const mostCampaignRow = CustomMaps_Count() > 0 ? 3 : 2;

	RowOf(AvPMenu_CoopHost, TEXTSTRING_CAMPAIGN_LEVEL)->MaxSliderValue = mostCampaignRow;

	if (CoopRowCampaign < 0 || CoopRowCampaign > mostCampaignRow)
	{
		CoopRowCampaign = 0;
	}

	int const campaign = kCampaignMarine + CoopRowCampaign;
	bool const isCustom = campaign == kCampaignCustom;

	if (campaign != campaignOnTheRow)
	{
		FillPlaceNames(AvPMenu_CoopHost, campaign, true);
		CoopRowPlace = 0;
	}

	if (CoopRowPlace < 0 || CoopRowPlace >= (int)placeNames.size())
	{
		CoopRowPlace = 0;
	}

	netGameData.gameType = NGT_Coop;
	netGameData.coopCampaign = campaign;
	netGameData.coopCampaignLevel = isCustom ? 0 : CoopRowPlace;

	memset(netGameData.coopCustomMap, 0, sizeof(netGameData.coopCustomMap));
	memset(netGameData.coopCustomMapSum, 0, sizeof(netGameData.coopCustomMapSum));
	netGameData.coopCustomMapSize = 0;

	AVPMENU_ELEMENT *const speciesRow = RowOf(AvPMenu_CoopHost, TEXTSTRING_CUSTOMMAPS_SPECIES);

	if (isCustom)
	{
		// Names on the list have been checked, and they fit: kCoopMapNameBytes is the longest a name can be, with its end marker
		strncpy(netGameData.coopCustomMap, CustomMaps_Name(CoopRowPlace), sizeof(netGameData.coopCustomMap) - 1);
		CustomMaps_SizeAndSum(CoopRowPlace, &netGameData.coopCustomMapSize, netGameData.coopCustomMapSum);

		// Only a custom map leaves this to choose: each campaign has its own species
		speciesRow->ElementID = AVPMENU_ELEMENT_TEXTSLIDER;

		if (CoopRowSpecies < 0 || CoopRowSpecies > kCoopChoiceMostSpecies)
		{
			CoopRowSpecies = 0;
		}
	}
	else
	{
		speciesRow->ElementID = AVPMENU_ELEMENT_DUMMYTEXTSLIDER;
		CoopRowSpecies = campaign - kCampaignMarine;
	}

	netGameData.coopCustomSpecies = CoopRowSpecies;
}

// Tells anyone searching what's chosen now, so a game on someone's list never names an old choice
static void HostSaysWhatTheGameIs(int lobbyState)
{
	if (netGameData.coopCampaign == kCampaignCustom)
	{
		Net_SayWhatTheGameIs(kLevelNumberOfACustomLevel, netGameData.coopCustomMap, MP_SessionName, (uint8_t)lobbyState);
		return;
	}

	netGameData.levelNumber = LevelList_PlaceOf(MAX_NO_OF_COOPERATIVE_EPISODES, netGameData.coopCampaign, netGameData.coopCampaignLevel);
	Net_SayWhatTheGameIs((uint16_t)netGameData.levelNumber, GetCustomMultiplayerLevelName(netGameData.levelNumber, NGT_Coop), MP_SessionName, (uint8_t)lobbyState);
}

// How many are here, and who hasn't said they have the map, so the host knows before pressing Start who'll be left on the page
static void HostPlayerLines(void)
{
	int joined = 0;
	const char *names[NET_MAXPLAYERS];
	int count = 0;

	for (int place = 0; place < NET_MAXPLAYERS; place++)
	{
		NetID const id = netGameData.playerData[place].playerId;

		if (id == 0)
		{
			continue;
		}

		joined++;

		if (id != AvPNetID && !Net_CoopPlayerHasTheMap(place))
		{
			names[count++] = netGameData.playerData[place].name;
		}
	}

	// As many as fit on the row, and how many more there are
	CoopNames_Line(CoopRowNoMap, sizeof(CoopRowNoMap), names, count, NET_PLAYERNAMELENGTH, GetTextString(TEXTSTRING_COOP_AND_MORE), kCoopRowMostLetters);

	_snprintf(CoopRowPlayers, sizeof(CoopRowPlayers) - 1, "%d", joined);
	CoopRowPlayers[sizeof(CoopRowPlayers) - 1] = 0;

	if (CoopRowNoMap[0] == 0)
	{
		strncpy(CoopRowNoMap, GetTextString(TEXTSTRING_COOP_NOBODY), sizeof(CoopRowNoMap) - 1);
	}
}

// One row for each joiner, after Start, so that Start stays where it is as they come and go. Says true if the rows have changed.
static bool HostRemoveRows(void)
{
	AVPMENU_ELEMENT *const firstRow = RowOf(AvPMenu_CoopHost, TEXTSTRING_LEVELSELECT_START) + 1;
	int rows = 0;

	for (int place = 0; place < NET_MAXPLAYERS && rows < NET_MAXPLAYERS - 1; place++)
	{
		NetID const id = netGameData.playerData[place].playerId;

		if (id == 0 || id == AvPNetID)
		{
			continue;
		}

		_snprintf(removeRowText[rows], sizeof(removeRowText[rows]) - 1, "%s %s", GetTextString(TEXTSTRING_COOP_REMOVE), netGameData.playerData[place].name);
		removeRowText[rows][sizeof(removeRowText[rows]) - 1] = 0;

		firstRow[rows].ElementID = AVPMENU_ELEMENT_COOP_REMOVE;
		firstRow[rows].TextPtr = removeRowText[rows];
		firstRow[rows].MaxValue = place;
		rows++;
	}

	bool const changed = rows != removeRowsShown;

	removeRowsShown = rows;

	for (int row = rows; row < NET_MAXPLAYERS - 1; row++)
	{
		firstRow[row].ElementID = AVPMENU_ELEMENT_ENDOFMENU;
	}

	return changed;
}

void CoopMenu_HostRemoves(int place)
{
	if (place >= 0 && place < NET_MAXPLAYERS)
	{
		Net_RemoveThisPlayer(netGameData.playerData[place].playerId);
	}
}

bool CoopMenu_HostPageOpens(void)
{
	campaignOnTheRow = kCampaignNone;
	removeRowsShown = 0;

	// Found afresh each time the page opens, so a map added while the game runs shows up
	CustomMaps_Find();

	// The campaign's own settings. The page the host came from puts back whatever else was set up, when the host goes back to it.
	SetDefaultMultiplayerConfig();
	netGameData.gameType = NGT_Coop;
	netGameData.coopCampaign = kCampaignMarine;
	netGameData.coopCampaignLevel = 0;

	if (Net_HostGame(MP_PlayerName, MP_SessionName, 0, NGT_Coop, (uint16_t)LevelList_PlaceOf(MAX_NO_OF_COOPERATIVE_EPISODES, kCampaignMarine, 0)) == NET_FAIL)
	{
		return false;
	}

	netGameData.disableFriendlyFire = TRUE;
	netGameData.coopKeepKit = 1;
	netGameData.coopDifficulty = I_Medium;

	Net_CoopLobbyOpens();
	HostRowsToTheGame();

	return true;
}

static CoopPageAnswer HostEveryFrame(void)
{
	MinimalNetCollectMessages();

	HostRowsToTheGame();
	HostPlayerLines();
	HostSaysWhatTheGameIs(kCoopLobbyOpen);

	Net_CoopHostSays();
	NetSendMessages();

	return HostRemoveRows() ? kCoopPageRowsChanged : kCoopPageStays;
}

bool CoopMenu_HostStarts(void)
{
	HostRowsToTheGame();

	unsigned int size = 0;
	unsigned char sum[kCoopMapSumBytes];

	if (netGameData.coopCampaign == kCampaignCustom && !CustomMaps_SizeAndSum(CoopRowPlace, &size, sum))
	{
		return false;
	}

	Campaign_ChooseRollNumber();
	Net_CoopTakeTheCampaign();
	HostSaysWhatTheGameIs(kCoopLobbyUnderWay);
	Net_CoopHostStarts();

	playingACustomMapWithOthers = netGameData.coopCampaign == kCampaignCustom;
	CustomMaps_SetPlaying(playingACustomMapWithOthers);
	CustomMaps_ForgetPictures();

	return true;
}

// ---- The joiner's page

// A joiner's rows only ever show what the host's choice said, which pldnet.cpp has already checked
static void JoinerRowsFromTheChoice(void)
{
	int const campaign = netGameData.coopCampaign;

	if (campaign != campaignOnTheRow)
	{
		FillPlaceNames(AvPMenu_CoopJoin, campaign, false);
	}

	bool const isCustom = campaign == kCampaignCustom;

	CoopRowCampaign = campaign >= kCampaignMarine ? campaign - kCampaignMarine : 0;
	CoopRowPlace = isCustom ? 0 : netGameData.coopCampaignLevel;
	CoopRowSpecies = netGameData.coopCustomSpecies;

	// Checked as it arrived. This row of pointers is the one thing here that would read past its end if that ever changed.
	if (CoopRowPlace < 0 || CoopRowPlace >= (int)placeNames.size())
	{
		CoopRowPlace = 0;
	}

	if (CoopRowSpecies < 0 || CoopRowSpecies > kCoopChoiceMostSpecies)
	{
		CoopRowSpecies = 0;
	}

	enum TEXTSTRING_ID line = TEXTSTRING_COOP_WAITING;

	if (Net_CoopChoiceIsHeard())
	{
		int const answer = Net_CoopMyMapAnswer();

		line = answer == kCoopMapIsHere ? TEXTSTRING_COOP_MAP_HERE : answer == kCoopMapIsDifferent ? TEXTSTRING_COOP_MAP_DIFFERENT : TEXTSTRING_COOP_MAP_MISSING;
	}

	strncpy(CoopRowMapLine, GetTextString(line), sizeof(CoopRowMapLine) - 1);
	CoopRowMapLine[sizeof(CoopRowMapLine) - 1] = 0;

	int joined = 0;

	for (int place = 0; place < NET_MAXPLAYERS; place++)
	{
		joined += netGameData.playerData[place].playerId != 0 ? 1 : 0;
	}

	_snprintf(CoopRowPlayers, sizeof(CoopRowPlayers) - 1, "%d", joined);
	CoopRowPlayers[sizeof(CoopRowPlayers) - 1] = 0;
}

void CoopMenu_JoinPageOpens(void)
{
	campaignOnTheRow = -1;

	// The list a host's words are looked up in. A name is never used for anything but finding one of these.
	CustomMaps_Find();
	JoinerRowsFromTheChoice();
}

// A player left behind gets the map's name too, and the help line says where a map goes. A removed player only needs the sentence, and may
// have come straight here, turned away at the door, so they may never have heard a map's name.
void CoopMenu_OutPageOpens(void)
{
	bool const removed = Net_WasRemovedByTheHost();

	AvPMenu_CoopOut[0].TextDescription = removed ? TEXTSTRING_COOP_REMOVED : TEXTSTRING_COOP_LEFT_BEHIND;
	AvPMenu_CoopOut[0].HelpString = removed ? TEXTSTRING_BLANK : TEXTSTRING_COOP_MAP_LINE_HELP;
	AvPMenu_CoopOut[1].ElementID = removed ? AVPMENU_ELEMENT_ENDOFMENU : AVPMENU_ELEMENT_DUMMYTEXTFIELD;
}

// This joiner has the map, and the host has started. It's what the joiner's own Start does in every other kind of game.
static void JoinerGoesIntoTheLoad(void)
{
	Net_CoopLobbyEnds();
	Net_CoopTakeTheCampaign();

	netGameData.myStartFlag = 1;

	// The host has to hear this, or it never says this player has joined
	AddNetMsg_PlayerDescription();
	NetSendMessages();

	playingACustomMapWithOthers = netGameData.coopCampaign == kCampaignCustom;
	CustomMaps_SetPlaying(playingACustomMapWithOthers);
	CustomMaps_ForgetPictures();
}

// The game has gone on without this player, or the host has put them out, so they leave it. Staying would hold a seat, and hold the others
// up on the loading screen. Leaving wipes everything the rows of this page show, so the player moves to a page that shows none of it.
static CoopPageAnswer JoinerIsOut(void)
{
	// The one thing to keep from the wipe: the name of the map a player left behind would need
	strncpy(CoopRowMapLine, netGameData.coopCustomMap, sizeof(CoopRowMapLine) - 1);
	CoopRowMapLine[sizeof(CoopRowMapLine) - 1] = 0;

	CoopMenu_PageLeft();
	return kCoopPageJoinerIsOut;
}

static CoopPageAnswer JoinerEveryFrame(void)
{
	MinimalNetCollectMessages();

	// Told why, so a page can say so, and this player can see it before going back
	if (Net_WasRemovedByTheHost())
	{
		return JoinerIsOut();
	}

	if (netGameData.myGameState == NGS_Error_HostLost)
	{
		return kCoopPageHostHasGone;
	}

	JoinerRowsFromTheChoice();

	if (Net_CoopHostHasStarted())
	{
		if (Net_CoopMyMapAnswer() == kCoopMapIsHere)
		{
			JoinerGoesIntoTheLoad();
			return kCoopPageGoesToTheLoad;
		}

		return JoinerIsOut();
	}

	Net_CoopJoinerSays();
	NetSendMessages();

	return kCoopPageStays;
}

// ---- Both

CoopPageAnswer CoopMenu_EveryFrame(bool isTheHostsPage)
{
	return isTheHostsPage ? HostEveryFrame() : JoinerEveryFrame();
}

void CoopMenu_PageLeft(void)
{
	if (AvP.Network != I_No_Network)
	{
		EndAVPNetGame();
	}

	CustomMaps_ForgetPictures();
}

texID_t CoopMenu_Picture(void)
{
	int const campaign = netGameData.coopCampaign;

	if (campaign == kCampaignCustom)
	{
		// On either page, the map is found among this machine's own, by name
		return CustomMaps_Picture(CustomMaps_Named(netGameData.coopCustomMap, sizeof(netGameData.coopCustomMap)));
	}

	texID_t first = AVPMENUGFX_MARINE_EPISODE1;
	int basicLevels = MAX_NO_OF_BASIC_MARINE_EPISODES;

	if (campaign == kCampaignPredator)
	{
		first = AVPMENUGFX_PREDATOR_EPISODE1;
		basicLevels = MAX_NO_OF_BASIC_PREDATOR_EPISODES;
	}
	else if (campaign == kCampaignAlien)
	{
		first = AVPMENUGFX_ALIEN_EPISODE1;
		basicLevels = MAX_NO_OF_BASIC_ALIEN_EPISODES;
	}
	else if (campaign != kCampaignMarine)
	{
		return MISSING_TEXTURE;
	}

	int const place = netGameData.coopCampaignLevel;

	if (place < 0 || place >= Campaign_LevelCount(campaign))
	{
		return MISSING_TEXTURE;
	}

	// The bonus levels share one picture, as they do on the single player pages
	return place >= basicLevels ? AVPMENUGFX_MARINE_EPISODE7 : first + place;
}

int CoopMenu_PageAfterACustomMap(void)
{
	int const page = playingACustomMapWithOthers ? AVPMENU_MULTIPLAYER : AVPMENU_CUSTOMMAPS;

	playingACustomMapWithOthers = false;
	return page;
}
