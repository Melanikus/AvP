// What the game's tables need to know about the shotgun. Include equipment.h first. Written 2026-11-06 by Melanikus for the AvP port.

#pragma once

#include "shotgun.h"

// The sub-sequence of the HUD model that opens or closes the stock, if the file has one. M5's has none, and the number is the one M5's code asks for.
enum
{
	kShotgunStockMove = 10
};

// Its own states. See weapon_shotgun.cpp for what happens in each.
enum
{
	kShotgunState_Lift = WEAPONSTATE_OWN_1,
	kShotgunState_LoadShell = WEAPONSTATE_OWN_2,
	kShotgunState_Lower = WEAPONSTATE_OWN_3,
	kShotgunState_Pump = WEAPONSTATE_OWN_4,
	kShotgunState_StockOpening = WEAPONSTATE_OWN_5,
	kShotgunState_ZoomIn = WEAPONSTATE_OWN_6,
	kShotgunState_ZoomOut = WEAPONSTATE_OWN_7,
	kShotgunState_StockClosing = WEAPONSTATE_OWN_8,
	kShotgunState_ZoomOutToLoad = WEAPONSTATE_OWN_9,
	kShotgunState_ZoomInAfterLoad = WEAPONSTATE_OWN_10,
	kShotgunState_ZoomOutToPutAway = WEAPONSTATE_OWN_11,
	kShotgunState_ClosingToPutAway = WEAPONSTATE_OWN_12
};

int FireShotgun(PLAYER_WEAPON_DATA *weaponPtr);
void Shotgun_WhenBroughtUp(PLAYER_WEAPON_DATA *weaponPtr);

void Shotgun_Idle(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);
void Shotgun_Recoil(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);
void Shotgun_SwapIn(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);
void Shotgun_SwapOut(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);
void Shotgun_Lift(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);
void Shotgun_LoadShell(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);
void Shotgun_Lower(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);
void Shotgun_Pump(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);
void Shotgun_StockMoves(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);

extern const WEAPON_HOOKS Shotgun_Hooks;
