// What the game's tables need to know about the revolver. Include equipment.h first. Written 2026-11-06 by Melanikus for the AvP port.

#pragma once

#include "revolver.h"

// Its own states. See weapon_revolver.cpp for what happens in each.
enum
{
	kRevolverState_ThrowOut = WEAPONSTATE_OWN_1,
	kRevolverState_Open = WEAPONSTATE_OWN_2,
	kRevolverState_LoadRound = WEAPONSTATE_OWN_3,
	kRevolverState_Close = WEAPONSTATE_OWN_4,
	kRevolverState_ScopeStartsUp = WEAPONSTATE_OWN_5,
	kRevolverState_ScopeRises = WEAPONSTATE_OWN_6,
	kRevolverState_ScopeComesDown = WEAPONSTATE_OWN_7,
	kRevolverState_ScopeLastBitDown = WEAPONSTATE_OWN_8
};

int FireRevolver(PLAYER_WEAPON_DATA *weaponPtr);

void Revolver_ModelIsReady(PLAYER_WEAPON_DATA *weaponPtr);

void Revolver_Idle(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);
void Revolver_Recoil(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);
void Revolver_SwapIn(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);
void Revolver_SwapOut(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);
void Revolver_ThrowOut(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);
void Revolver_Open(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);
void Revolver_LoadRound(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);
void Revolver_Close(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);
void Revolver_ScopeStartsUp(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);
void Revolver_ScopeLastBitDown(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);

extern const WEAPON_HOOKS Revolver_Hooks;
