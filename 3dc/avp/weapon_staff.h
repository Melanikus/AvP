// What the game's tables need to know about the Predator's staff. Include equipment.h first. Written 2026-11-06 by Melanikus for the AvP port.

#pragma once

// A press of secondary fire asks the gun whether it can start: the staff always can. The throw itself comes later, at
// the moment the model marks in its move.
int PredStaff_StartThrow(PLAYER_WEAPON_DATA *weaponPtr);

void PredStaff_Idle(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);
void PredStaff_Swing(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);
void PredStaff_Throw(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);
void PredStaff_SwapIn(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);
void PredStaff_SwapOut(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);

// The staff he has thrown, each frame of its life
void ThrownStaffBehaviour(STRATEGYBLOCK *sbPtr);
