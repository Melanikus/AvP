// What a player keeps from one level of a co-op campaign to the next. Written 2026-11-07 by Melanikus for the AvP port.
// Credit: Melanikus and Bobblen (the kept kit follows Bobblen's avp-forever, rewritten).
//
// A level hands out its own kit when it starts. With "Keep" chosen on the host's page, each player also gets what they carried out
// of the last level: for every gun and every kind of ammunition, whichever is larger, what they had or what the level gives. So
// nobody loses by going on, and a gun the new level hands out still arrives.
//
// A player who was dead when the level ended has nothing to carry, so they get the level's own kit.
//
// Health and armour aren't here. A level starts everyone at the most they can have, and nothing that adds to them goes past that
// (AbleToPickupHealth and AbleToPickupArmour in inventory.cpp, InitPlayer in player.cpp). So "the larger of the two, capped" is
// always what the level gives, and there's nothing to work out.
//
// The kit lives in memory on each player's own machine. It's never saved to a file and never sent to anyone, so nothing from
// outside the game can change it.
//
// Nothing here touches the game, so tests/campaign_kit_test.cpp can try every case. campaign_kit.cpp joins it to the game.

#ifndef _campaign_kit_h_
#define _campaign_kit_h_ 1

enum
{
	kCampaignKitMostGuns = 32,       // more than any species has slots: campaign_kit.cpp stops the build if that changes
	kCampaignKitLauncherKinds = 4    // the grenade launcher's standard, flare, proximity and fragmentation rounds
};

// One slot of a player's weapons, as numbers. The rounds are in the game's own units, whatever those are for this gun.
struct CampaignKitGun
{
	int weapon;                    // which gun the slot holds
	int possessed;                 // 1 carried, 0 not carried, and -1 means this level doesn't allow it
	unsigned int primaryRounds;    // in the current magazine
	unsigned int secondaryRounds;
	unsigned int primaryMagazines;
	unsigned int secondaryMagazines;
	unsigned int leftoverRounds;   // rounds kept back from reloading part way through a magazine
};

struct CampaignKit
{
	int guns;
	CampaignKitGun gun[kCampaignKitMostGuns];
	unsigned int launcherRounds[kCampaignKitLauncherKinds];
	unsigned int launcherMagazines[kCampaignKitLauncherKinds];
};

// What was written down when a level ended, and which level it's for
struct CampaignKitNote
{
	bool noted;      // false if there's nothing to give back, because nothing was noted or the player was dead
	int campaign;
	int place;       // the level this is for: the one that comes next
	CampaignKit kit;
};

// Writes down what a player is carrying as a level ends. place is the level that comes next.
inline void CampaignKit_Note(CampaignKitNote &note, bool playerIsAlive, int campaign, int place, CampaignKit const &kit)
{
	note.noted = playerIsAlive;
	note.campaign = campaign;
	note.place = place;
	note.kit = kit;
}

// Is there a kit to give this player for this level? Only with Keep chosen, and only the one noted for exactly this level of this
// campaign. So a kit from a game that ended can't turn up in a new one.
inline bool CampaignKit_IsFor(CampaignKitNote const &note, bool keepIsChosen, int campaign, int place)
{
	return keepIsChosen && note.noted && note.campaign == campaign && note.place == place;
}

inline unsigned int CampaignKit_Larger(unsigned int a, unsigned int b)
{
	return a > b ? a : b;
}

// Adds what was kept to what the level has given: the larger of each.
inline void CampaignKit_Keep(CampaignKit &level, CampaignKit const &kept)
{
	int const guns = level.guns < kept.guns ? level.guns : kept.guns;

	for (int i = 0; i < guns; i++)
	{
		CampaignKitGun &now = level.gun[i];
		CampaignKitGun const &had = kept.gun[i];

		// The two lists are the same list unless the game's own table has changed. If a slot holds another gun now, its numbers mean nothing here.
		if (now.weapon != had.weapon)
		{
			continue;
		}

		// A gun this level doesn't allow stays away
		if (now.possessed < 0)
		{
			continue;
		}

		if (had.possessed > 0)
		{
			now.possessed = 1;
		}

		now.primaryRounds = CampaignKit_Larger(now.primaryRounds, had.primaryRounds);
		now.secondaryRounds = CampaignKit_Larger(now.secondaryRounds, had.secondaryRounds);
		now.primaryMagazines = CampaignKit_Larger(now.primaryMagazines, had.primaryMagazines);
		now.secondaryMagazines = CampaignKit_Larger(now.secondaryMagazines, had.secondaryMagazines);
		now.leftoverRounds = CampaignKit_Larger(now.leftoverRounds, had.leftoverRounds);
	}

	for (int kind = 0; kind < kCampaignKitLauncherKinds; kind++)
	{
		level.launcherRounds[kind] = CampaignKit_Larger(level.launcherRounds[kind], kept.launcherRounds[kind]);
		level.launcherMagazines[kind] = CampaignKit_Larger(level.launcherMagazines[kind], kept.launcherMagazines[kind]);
	}
}

#endif
