// AvP port by Barry Duncan.
// Modified 2026-11-03 by Melanikus for the AvP port:
//   the maximum number of lights (500) is defined here, so view.cpp can size an object's light list
#ifndef _Lighting_h_
#define _Lighting_h_

enum LIGHTING_EFFECTS_ID
{
	LFX_EXPLOSION,
	LFX_MUZZLEFLASH,
	LFX_PARTICLECANNON,
	LFX_ROCKETJET,
	LFX_FLARE,
	LFX_BIGEXPLOSION,
	LFX_XENO_FIRING,
	LFX_PLASMA_BOLT,
	LFX_OBJECTONFIRE,
	LFX_SPEARGUNBOLT
};

/* explosions light an area which has a max 15m radius */
#define EXPLOSION_LIGHT_RANGE 15000
//#define EXPLOSION_LIGHT_RANGE 30000

extern void AddLightingEffectToObject(DISPLAYBLOCK *objectPtr, enum LIGHTING_EFFECTS_ID lfxID);
extern void LightBlockDeallocation(void);



enum LIGHTELEMENT_BEHAVIOUR_ID
{
	LIGHTELEMENT_MOLTENMETAL,
	LIGHTELEMENT_PLASMACASTERHIT,
	LIGHTELEMENT_FROMFMV,
	LIGHTELEMENT_ROTATING,
	LIGHTELEMENT_EXPLOSION,
	LIGHTELEMENT_ELECTRICAL_EXPLOSION,
	LIGHTELEMENT_ELECTRICAL_SPARKS,
	LIGHTELEMENT_ALIEN_TEETH,
	LIGHTELEMENT_ALIEN_TEETH2,
	LIGHTELEMENT_PARGEN_FLAME,
};

typedef struct
{
	enum LIGHTELEMENT_BEHAVIOUR_ID BehaviourID;

	LIGHTBLOCK LightBlock;
	int LifeTime;

} LIGHTELEMENT;

// size of LightElementStorage (lighting.cpp); view.cpp sizes an object's light list with it
#define MAX_NO_OF_LIGHTELEMENTS 500


void InitialiseLightElementSystem(void);
void MakeLightElement(VECTORCH *positionPtr, enum LIGHTELEMENT_BEHAVIOUR_ID behaviourID);
void HandleLightElementSystem(void);

#endif
