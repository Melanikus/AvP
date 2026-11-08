// What each place means on the level row of the multiplayer and skirmish pages.
// Written 2026-11-06 by Melanikus for the AvP port.
//
// That row only counts: 0, 1, 2 and so on. Each number is a place on a list, and each list has three
// stretches, one after the other:
//
//   1. the levels made for multiplayer, at the numbers they've always had
//   2. all 32 single player levels: the Marine's 11, then the Predator's 11, then the Alien's 10
//   3. any custom levels found on this computer
//
// The middle stretch goes where it does so that no old number changes. Only the custom levels move
// along, and a custom level is never told apart by its number: a saved game setup and a game on the
// network both find it by its name.
//
// There are two lists. The co-op one is what skirmish always uses. Choosing one of the 32 on the co-op
// list is what starts the mode that plays the way single player does, with that level's species. On
// the other list, the deathmatch one, the same level is only somewhere to fight.
//
// Nothing here touches the game, so a test can check every number.

#ifndef _level_lists_h_
#define _level_lists_h_ 1

#include "env_info.h"

enum
{
	kLevelListMarineLevels = MAX_NO_OF_MARINE_EPISODES,
	kLevelListPredatorLevels = MAX_NO_OF_PREDATOR_EPISODES,
	kLevelListAlienLevels = MAX_NO_OF_ALIEN_EPISODES,
	kLevelListCampaignLevels = kLevelListMarineLevels + kLevelListPredatorLevels + kLevelListAlienLevels
};

// The first custom level's place, on a list that starts with this many levels made for multiplayer.
inline int LevelList_FirstCustom(int stockLevels)
{
	return stockLevels + kLevelListCampaignLevels;
}

inline bool LevelList_IsCustom(int place, int stockLevels)
{
	return place >= LevelList_FirstCustom(stockLevels);
}

inline bool LevelList_IsSinglePlayerLevel(int place, int stockLevels)
{
	return place >= stockLevels && !LevelList_IsCustom(place, stockLevels);
}

// Which campaign a place belongs to, and how far along that campaign it is. False, with nothing
// changed, for a place that isn't one of the 32.
inline bool LevelList_CampaignAt(int place, int stockLevels, int *campaign, int *campaignLevel)
{
	if (!LevelList_IsSinglePlayerLevel(place, stockLevels))
	{
		return false;
	}

	int along = place - stockLevels;

	if (along < kLevelListMarineLevels)
	{
		*campaign = kCampaignMarine;
		*campaignLevel = along;
		return true;
	}

	along -= kLevelListMarineLevels;

	if (along < kLevelListPredatorLevels)
	{
		*campaign = kCampaignPredator;
		*campaignLevel = along;
		return true;
	}

	*campaign = kCampaignAlien;
	*campaignLevel = along - kLevelListPredatorLevels;
	return true;
}

// The other way round: the place on a list of a campaign's level. -1 if the campaign has no such level.
inline int LevelList_PlaceOf(int stockLevels, int campaign, int campaignLevel)
{
	int before = 0;
	int levels = 0;

	switch (campaign)
	{
		case kCampaignMarine:
			levels = kLevelListMarineLevels;
			break;
		case kCampaignPredator:
			before = kLevelListMarineLevels;
			levels = kLevelListPredatorLevels;
			break;
		case kCampaignAlien:
			before = kLevelListMarineLevels + kLevelListPredatorLevels;
			levels = kLevelListAlienLevels;
			break;
		default:
			return -1;
	}

	if (campaignLevel < 0 || campaignLevel >= levels)
	{
		return -1;
	}

	return stockLevels + before + campaignLevel;
}

#endif
