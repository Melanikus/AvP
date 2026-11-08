// Joins the rule in campaign_kit.h to the game: it reads what a player is carrying as a level ends, and gives it back as the next one
// starts. Written 2026-11-07 by Melanikus for the AvP port. Credit: Melanikus and Bobblen (the kept kit follows Bobblen's avp-forever,
// rewritten).

#include "3dc.h"
#include "inline.h"
#include "module.h"
#include "strategy_def.h"
#include "gamedef.h"
#include "gameplat.h"
#include "bh_types.h"
#include "equipment.h"
#include "inventory.h"
#include "pldnet.h"
#include "weapon_hooks.h"

#include <string.h>

#include "campaign_kit.h"

extern void LoadAllWeapons(PLAYER_STATUS *playerStatusPtr);

static_assert(MAX_NO_OF_WEAPON_SLOTS <= kCampaignKitMostGuns, "a kit has to have room for every slot a player has: raise kCampaignKitMostGuns");

static CampaignKitNote note;

// What this player is carrying now, as plain numbers
static void ReadTheKit(PLAYER_STATUS const *playerStatusPtr, CampaignKit &kit)
{
	memset(&kit, 0, sizeof(kit));
	kit.guns = MAX_NO_OF_WEAPON_SLOTS;

	for (int slot = 0; slot < MAX_NO_OF_WEAPON_SLOTS; slot++)
	{
		PLAYER_WEAPON_DATA const &weapon = playerStatusPtr->WeaponSlot[slot];
		CampaignKitGun &gun = kit.gun[slot];

		gun.weapon = (int)weapon.WeaponIDNumber;
		gun.possessed = weapon.Possessed;
		gun.primaryRounds = weapon.PrimaryRoundsRemaining;
		gun.secondaryRounds = weapon.SecondaryRoundsRemaining;
		gun.primaryMagazines = weapon.PrimaryMagazinesRemaining;
		gun.secondaryMagazines = weapon.SecondaryMagazinesRemaining;
		gun.leftoverRounds = weapon.PrimaryLeftoverRounds;
	}

	kit.launcherRounds[0] = GrenadeLauncherData.StandardRoundsRemaining;
	kit.launcherMagazines[0] = GrenadeLauncherData.StandardMagazinesRemaining;
	kit.launcherRounds[1] = GrenadeLauncherData.FlareRoundsRemaining;
	kit.launcherMagazines[1] = GrenadeLauncherData.FlareMagazinesRemaining;
	kit.launcherRounds[2] = GrenadeLauncherData.ProximityRoundsRemaining;
	kit.launcherMagazines[2] = GrenadeLauncherData.ProximityMagazinesRemaining;
	kit.launcherRounds[3] = GrenadeLauncherData.FragmentationRoundsRemaining;
	kit.launcherMagazines[3] = GrenadeLauncherData.FragmentationMagazinesRemaining;
}

// Puts numbers back. They all came from the player's own kit or the level's, so each still fits what holds it.
static void WriteTheKit(PLAYER_STATUS *playerStatusPtr, CampaignKit const &kit)
{
	for (int slot = 0; slot < MAX_NO_OF_WEAPON_SLOTS; slot++)
	{
		PLAYER_WEAPON_DATA &weapon = playerStatusPtr->WeaponSlot[slot];
		CampaignKitGun const &gun = kit.gun[slot];

		weapon.Possessed = gun.possessed;
		weapon.PrimaryRoundsRemaining = gun.primaryRounds;
		weapon.SecondaryRoundsRemaining = gun.secondaryRounds;
		weapon.PrimaryMagazinesRemaining = (unsigned char)gun.primaryMagazines;
		weapon.SecondaryMagazinesRemaining = (unsigned char)gun.secondaryMagazines;
		weapon.PrimaryLeftoverRounds = (unsigned short)gun.leftoverRounds;
	}

	GrenadeLauncherData.StandardRoundsRemaining = kit.launcherRounds[0];
	GrenadeLauncherData.StandardMagazinesRemaining = (unsigned char)kit.launcherMagazines[0];
	GrenadeLauncherData.FlareRoundsRemaining = kit.launcherRounds[1];
	GrenadeLauncherData.FlareMagazinesRemaining = (unsigned char)kit.launcherMagazines[1];
	GrenadeLauncherData.ProximityRoundsRemaining = kit.launcherRounds[2];
	GrenadeLauncherData.ProximityMagazinesRemaining = (unsigned char)kit.launcherMagazines[2];
	GrenadeLauncherData.FragmentationRoundsRemaining = kit.launcherRounds[3];
	GrenadeLauncherData.FragmentationMagazinesRemaining = (unsigned char)kit.launcherMagazines[3];
}

static bool Campaign_PlayerIsAliveAtTheEnd(void)
{
	return PlayerStatusPtr->IsAlive != 0;
}

void Campaign_NoteKit(int place)
{
	CampaignKit kit;

	ReadTheKit(PlayerStatusPtr, kit);
	CampaignKit_Note(note, Campaign_PlayerIsAliveAtTheEnd(), netGameData.coopCampaign, place, kit);
	Guns_KeepOwnStates();
}

void Campaign_ForgetKit(void)
{
	note.noted = false;
}

// Called last of all when a player starts, so both a new level's start and a level starting over after everyone died come here.
void Campaign_GiveKeptKit(PLAYER_STATUS *playerStatusPtr)
{
	if (!CampaignKit_IsFor(note, netGameData.coopKeepKit != 0, netGameData.coopCampaign, netGameData.coopCampaignLevel))
	{
		return;
	}

	CampaignKit kit;

	ReadTheKit(playerStatusPtr, kit);
	CampaignKit_Keep(kit, note.kit);
	WriteTheKit(playerStatusPtr, kit);
	Guns_GiveBackOwnStates();

	// A gun they ended with empty, with magazines left, starts loaded, as a level's own guns do
	LoadAllWeapons(playerStatusPtr);
}
