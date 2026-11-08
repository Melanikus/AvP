// What the game's tables need to know about the sniper rifle. Include equipment.h first. Written 2026-11-06 by Melanikus
// for the AvP port.

#pragma once

// Its own states. See weapon_sniper.cpp for what happens in each, and weapon_sniper_sums.h for how they follow each other.
enum
{
	kSniperState_Bolt = WEAPONSTATE_OWN_1,
	kSniperState_ScopeRising = WEAPONSTATE_OWN_2,
	kSniperState_ScopeLowering = WEAPONSTATE_OWN_3,
	kSniperState_WaitingForTrigger = WEAPONSTATE_OWN_4,
	kSniperState_ZoomingIn = WEAPONSTATE_OWN_5,
	kSniperState_ZoomingBack = WEAPONSTATE_OWN_6,
	kSniperState_ZoomingOut = WEAPONSTATE_OWN_7
};

int FireSniperRifle(PLAYER_WEAPON_DATA *weaponPtr);

void Sniper_Recoil(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);
void Sniper_Reload(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);
void Sniper_SwapIn(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);
void Sniper_SwapOut(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);
void Sniper_Bolt(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);
void Sniper_ScopeRising(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);
void Sniper_ScopeLowering(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);

extern const WEAPON_HOOKS Sniper_Hooks;
