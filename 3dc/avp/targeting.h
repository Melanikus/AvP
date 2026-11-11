#ifndef _targeting_h_
#define _targeting_h_

/* KJL 16:20:20 19/05/98 - targeting externs */

extern void GetTargetingPointOfObject(DISPLAYBLOCK *objectPtr, VECTORCH *targetPtr);
extern void GetTargetingPointOfObject_Far(STRATEGYBLOCK *sbPtr, VECTORCH *targetPtr);
extern void CalculatePlayersTarget(TEMPLATE_WEAPON_DATA *twPtr, PLAYER_WEAPON_DATA *weaponPtr);
void SmartTarget_GetCofM(DISPLAYBLOCK *target,VECTORCH *viewSpaceOutput);

// Modified 2026-11-10 by Melanikus for the AvP port (#210): a rapid-fire gun's recoil judder, for a player's gun and for the marine who
// fights like a player (targeting.cpp says what it is)
void Weapon_RecoilJudder(const TEMPLATE_WEAPON_DATA *twPtr, int speed, MATRIXCH *out);

#endif
