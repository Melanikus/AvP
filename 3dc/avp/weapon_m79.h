// What the game's tables need to know about the M79. Include equipment.h first. Written 2026-11-06 by Melanikus for the AvP port.

#pragma once

#include "m79.h"

// Its own state. See weapon_m79.cpp for what happens in it.
enum
{
	kM79State_ChangeKind = WEAPONSTATE_OWN_1
};

int FireM79(PLAYER_WEAPON_DATA *weaponPtr);

void M79_WhenBroughtUp(PLAYER_WEAPON_DATA *weaponPtr);

void M79_Idle(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);
void M79_Recoil(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);
void M79_Reload(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);
void M79_SwapIn(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);
void M79_SwapOut(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);
void M79_ChangingKind(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);

// He's walked over ammunition for the M79. It gives 1 if he took it.
int M79_TakeAmmo(enum AMMO_ID ammo, PLAYER_WEAPON_DATA *weaponPtr);

// For GIVEALLWEAPONS: plenty of every kind.
void M79_FillEveryKind(PLAYER_WEAPON_DATA *weaponPtr);

// Sets how many spares he has of each kind, as a class starts with.
void M79_SetEveryKind(PLAYER_WEAPON_DATA *weaponPtr, int spares);

extern const WEAPON_HOOKS M79_Hooks;
