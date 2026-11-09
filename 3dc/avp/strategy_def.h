// Modified 2026-11-06 by Melanikus for the AvP port:
//   the M79's three grenades have numbers of their own, and so does a thrown staff
//   NameFromTheLevel and BehaviourIsAnNpcTheHostRuns
// Modified 2026-11-08 by Melanikus for the AvP port:
//   LevelNameIsAPlacedNpc (#130)
//   code that could never run is taken out (#135): the blocks kept for a lift's change of environment (MAX_PRESERVED_SB and the five
//   calls), and the xenoborg morph room, whose number stays and says why
#ifndef _stratdef_h_
#define _stratdef_h_ 1

/*
stratgey block. We gain game control via the strategy blocks
the overall sturcture is basically game independent, though if you
do not use module mapblocks

The description
of the world is in two structures. These are the Module Map block and the
strategy block. A object does not need to have a strategy block. In this case
in is simply something to be drawn and/or used for standard collsions. Objects
that have an existance beyond simple engine controls has a strategy block.



Control.

1 	creation and destruction of strategy blocks. After Showviews we generate/remove any
	displayblocks that we have responsibilty for. Most predictable elements of the
	world are handeled by Chris, though we do need to supply him with a function
	that attaches SBs to MOdMBs when needed.

2 	Strategies are run.

	Rather than passing the DBs to the stratgey functions we pass the SBs. we
	switch on the I_SBtype and use the SBdataptr via a cast to contain and modify the
	behaviour in a function specific to that I_SBType. The first element of the SBdata
	structure (whatever it is) MUST CONTAIN THE I_SBtype. This is the only check on the
	validity of the data as we are passing data via a cast.



changes

	Loading of the SBdataptr is entirley game-tool related and does not concern the
	over all design of how strategy blocks work

	TEMPLATES templates no longer exist

	Entities, entitities.c and entity.c We are keeping entities as names for objects
	(ModMBs) which have attached strategy blocks

	Relation to ObStrategy. The Obstrategy still can desiginate how something will behave.
	If we have a simplistic moving object it can have a OBstratgey. These still can run under
	display block control for Bwards stuff.



generation

	strategy blocks. Post loading attaching of stratgey blocks. (useful for testing)
	Certain objects may be created at run time eg missiles, dropped guns. We need functions
	that can initialise a stratgey block for these objects. Functions can be also wriiten for
	attaching testing strategies to objects.

	 B Laoding 	Map
				StrategyBlock
				StrategyExtensionBLock


	 if you still want ot use templates to fill out strategy blocks you can.

*/

#include "hmodel.h"

typedef enum bhvr_type
{
    I_BehaviourNull,

    I_BehaviourMarinePlayer,
    I_BehaviourPredatorPlayer,
   	I_BehaviourAlienPlayer,

	I_BehaviourAlien,
	I_BehaviourQueenAlien,
	I_BehaviourFaceHugger,
	I_BehaviourPredator,
	I_BehaviourXenoborg,
	I_BehaviourMarine,
	I_BehaviourSeal,
	I_BehaviourPredatorAlien,

	I_BehaviourProximityDoor,
	I_BehaviourLiftDoor,
	I_BehaviourSwitchDoor,
	I_BehaviourSimpleAnimation,
	I_BehaviourBinarySwitch,
	I_BehaviourLift,
	I_BehaviourPlatform,
	I_BehaviourAutoGun,
	I_BehaviourAutoGunMuzzleFlash,
	I_BehaviourGenerator,
	I_BehaviourDatabase,


	I_BehaviourHierarchicalFragment,

	I_BehaviourAlienFragment,
	I_BehaviourSmokeGenerator,

	/* KJL 12:51:18 03/20/97 -
		this fragment behaviour will be used by all objects */
	I_BehaviourFragment,

    I_BehaviourGrenade,
	I_BehaviourFlameProjectile,
	I_BehaviourRocket,
	I_BehaviourSonicPulse,
	I_BehaviourPPPlasmaBolt,
	I_BehaviourSpeargunBolt,

	I_BehaviourNPCPredatorDisc,
	I_BehaviourPredatorDisc_SeekTrack,
	I_BehaviourPredatorEnergyBolt,
	I_BehaviourXenoborgEnergyBolt,

	I_BehaviourOneShot,
	I_BehaviourOneShotAnim,
	I_BehaviourInanimateObject,

	I_BehaviourNetGhost, /* for preliminary network support */

    I_BehaviourPulseGrenade,
    I_BehaviourFlareGrenade,
    I_BehaviourFragmentationGrenade,
    I_BehaviourProximityGrenade,
	I_BehaviourMolotov,

// added by john for link switches
	I_BehaviourLinkSwitch,

    I_BehaviourClusterGrenade,

	I_BehaviourAlienSpit,

    I_BehaviourTest,

    // The xenoborg morph room's number. The room itself was taken out (#135): nothing in a level ever made one. The number stays, so
    // every kind after it keeps its number, as saved games carry these numbers (savegame.cpp reads them back).
    I_BehaviourXenoborgMorphRoom,

	I_BehaviourLightFX,
	I_BehaviourPlacedSound,
	I_BehaviourMissionComplete,
	I_BehaviourTrackObject,
	I_BehaviourFan,
	I_BehaviourMessage,
	I_BehaviourNetCorpse,

	I_BehaviourRubberDuck,
	I_BehaviourPlacedHierarchy,
	I_BehaviourPlacedLight,
	I_BehaviourPowerCable,
	I_BehaviourDormantPredator,
	I_BehaviourDeathVolume,
	I_BehaviourSelfDestruct,

	I_BehaviourGrapplingHook,
	I_BehaviourDummy,
	I_BehaviourParticleGenerator,
	I_BehaviourVideoScreen,

	I_BehaviourFrisbee,
	I_BehaviourFrisbeeEnergyBolt,

	// The M79's three grenades. They come last, so that no number already in a save file moves.
	I_BehaviourM79Grenade,
	I_BehaviourM79FireGrenade,
	I_BehaviourM79FragGrenade,

	// A staff the Predator has thrown. It comes last for the same reason.
	I_BehaviourThrownStaff,

}AVP_BEHAVIOUR_TYPE;


/* put the tags in here */

#define SB_NAME_LENGTH  8	/* DO NOT CHANGE THIS! */

typedef struct
{
	char name [SB_NAME_LENGTH];
} SBNAMEBLOCK;


typedef struct sb_flags_bitfield
{
	unsigned int please_destroy_me :1;
	unsigned int no_displayblock :1;
	unsigned int request_operate :1;
	unsigned int placed_in_the_level:1;	/* an NPC with a name in the level: see NameFromTheLevel */
	unsigned int preserve_until_end_of_level:1; /*strategy block targeted by something else , so must stay until end of level*/
	unsigned int destroyed_but_preserved:1;
	unsigned int not_on_motiontracker :1;

}SBFLAGS;

/* CDF 12/11/97 Damage structures moved to hmodel.h */

typedef struct strategyblock
{
	AVP_BEHAVIOUR_TYPE I_SBtype;		/* Strategy Extension Type*/
	void * SBdataptr;					/* Strategy Extension Data Pointer*/
	struct displayblock* SBdptr;		/* pointer to DB if drawn or collided*/
    struct dynamicsblock* DynPtr;   	/* KJL 17:17:15 11/05/96 - pointer to a DYNAMICSBLOCK */
    									/* (If this is NULL the object cannot move AND cannot be collided with.) */
	int integrity;
	/* CDF 15/9/97 New Damage System */
	DAMAGEBLOCK SBDamageBlock;
	/* CDF 15/9/97 New Damage System */

	SBFLAGS SBflags;						/* flags */
	char SBname[SB_NAME_LENGTH];
	#if SupportModules
	struct module *SBmoptr;				/* needed if DBdeS are deallocted*/
	struct modulemapblock*	SBmomptr;	/* module map block ref*/
	#endif
	#if SupportMorphing
	struct morphctrl *SBmorphctrl;
	#endif

	/* patrick 15/1/97 - these fields are for object visibility management system */
	char maintainVisibility;
	struct module *containingModule;
	int shapeIndex;
	#if debug
	short SBIsValid;
	#endif
	char* name;

} STRATEGYBLOCK;



// interface to Creating StrategyBlocks


extern void AssignNewSBName(STRATEGYBLOCK *sbPtr);

// A placed NPC's name. In single player it's the name the level gives it. In a network game it's a new one, which the network messages use, and FindSBWithName still
// finds it by the level's name, so a switch aimed at it works. See npc_level_names.h.
extern void NameFromTheLevel(STRATEGYBLOCK *sbPtr, const char *levelName);

// The name this NPC has in the level, which is what's the same on every machine. It's only different from its own name in a network game.
extern const char *NpcNameInTheLevel(const STRATEGYBLOCK *sbPtr);

// Is this kind one that only the host runs in a network game, with everyone else seeing what the host tells them?
extern bool BehaviourIsAnNpcTheHostRuns(AVP_BEHAVIOUR_TYPE type);

// Is this a name the level gives one of its placed NPCs, made here or not? In object_setup.cpp, which keeps the level's list.
extern bool LevelNameIsAPlacedNpc(const char *name);
extern STRATEGYBLOCK * AttachNewStratBlock(struct module* moptr,
											 struct modulemapblock* momptr,
											 struct displayblock* dptr);

extern void InitialiseSBValues(STRATEGYBLOCK* sbptr);

extern STRATEGYBLOCK* FindSBWithName(char* id_name);


// interface to removing strategy blocks


extern void DestroyAnyStrategyBlock(STRATEGYBLOCK* sbptr);
extern void RemoveDestroyedStrategyBlocks(void);
extern void DestroyAllStrategyBlocks(void);

/* This should be in PLAYER.  But Kevin was on holiday with his files checked out. */
extern void GivePlayerCloakAway(void);



extern int NumActiveStBlocks;
extern STRATEGYBLOCK *ActiveStBlockList[];

/****** MACROS FOR NAME COMAPRISONS AND COPYS*******/

#define COPY_NAME(name1, name2) \
			{	\
				GLOBALASSERT(SB_NAME_LENGTH == 8); \
				*(int*)name1 = *(int*)name2; \
				*((int*)name1 + 1) = *((int*)name2 + 1);\
			}

#define NAME_ISEQUAL(name1, name2) \
				((*(int*)name1 == *(int*)name2) && \
				(*(((int*)name1) + 1) == *(((int*)name2) + 1)))

#define NAME_ISNULL(name1) \
				((*(int*)name1 == '\0') && \
				(*(((int*)name1) + 1) == '\0'))

#endif
