// Modified 2026-11-06 by Melanikus for the AvP port:
//   a gun can have states and hooks of its own, and there are 14 weapon slots. See weapon_hooks.h
//   a slot keeps the rounds left over from a reload
#ifndef _equipmnt_h_
#define _equipmnt_h_ 1

#include "language.h"
/* has to come after gameplat.h in the setup*/
#include "gamedef.h"

/* KJL 12:34:46 09/18/96 - new stuff under development */

/* KJL 12:39:41 09/18/96 - Okay, I think we should have two weapon structures:

    TEMPLATE_WEAPON_DATA:   First the basic template for each weapon, which remains constant
                            during the game & contains the base description of a weapon	eg.
                            ammo type, weight, graphics, etc.

    PLAYER_WEAPON_DATA:     Second is the structure which describes a player's weapon
                            eg. a pointer to a TEMPLATE_WEAPON_DATA, ammo remaining, jam
                            status, and so on.

                            eg. PLAYER_WEAPON_DATA PlayerWeapons[];

*/
/* KJL 16:09:51 09/20/96 - Weapon States */
enum WEAPON_STATE
{
	WEAPONSTATE_IDLE=0,

	WEAPONSTATE_FIRING_PRIMARY,
	WEAPONSTATE_RECOIL_PRIMARY,
	WEAPONSTATE_RELOAD_PRIMARY,

	WEAPONSTATE_FIRING_SECONDARY,
	WEAPONSTATE_RECOIL_SECONDARY,
	WEAPONSTATE_RELOAD_SECONDARY,

	WEAPONSTATE_SWAPPING_IN,
	WEAPONSTATE_SWAPPING_OUT,
	WEAPONSTATE_JAMMED,

	WEAPONSTATE_WAITING,

	WEAPONSTATE_READYING,
	WEAPONSTATE_UNREADYING,

	// States a gun with a file of its own can use for whatever it likes. The same number means different things on different guns: the gun's file names them, and its row in equipment.cpp gives each one its time and its function.
	// To the common weapon code each is only a timer and a function. When the time is up, the gun's StateEnded hook says what comes next, and if it has no opinion he goes back to idle. A gun that doesn't use one has a time of 0 there, so it ends at once.
	WEAPONSTATE_OWN_1,
	WEAPONSTATE_OWN_2,
	WEAPONSTATE_OWN_3,
	WEAPONSTATE_OWN_4,
	WEAPONSTATE_OWN_5,
	WEAPONSTATE_OWN_6,
	WEAPONSTATE_OWN_7,
	WEAPONSTATE_OWN_8,
	WEAPONSTATE_OWN_9,
	WEAPONSTATE_OWN_10,
	WEAPONSTATE_OWN_11,
	WEAPONSTATE_OWN_12,
	WEAPONSTATE_OWN_13,

	MAX_NO_OF_WEAPON_STATES
};

#define WEAPONSTATE_INITIALTIMEOUTCOUNT 65536
#define WEAPONSTATE_INSTANTTIMEOUT		0

/* KJL 10:42:38 09/19/96 - Weapon Enumerations */
enum WEAPON_ID
{
    /* USED TO DENOTE AN EMPTY SLOT */
	NULL_WEAPON=-1,

    /* MARINE WEAPONS */
	WEAPON_PULSERIFLE,
	WEAPON_AUTOSHOTGUN,
	WEAPON_SMARTGUN,
	WEAPON_FLAMETHROWER,
	WEAPON_PLASMAGUN,
	WEAPON_SADAR,
	WEAPON_GRENADELAUNCHER,
	WEAPON_MINIGUN,
	WEAPON_SONICCANNON,
	WEAPON_BEAMCANNON,
	WEAPON_MYSTERYGUN,

    /* PREDATOR WEAPONS */
	WEAPON_PRED_WRISTBLADE,
	WEAPON_PRED_PISTOL,
	WEAPON_PRED_RIFLE,
	WEAPON_PRED_SHOULDERCANNON,
	WEAPON_PRED_DISC,
	WEAPON_PRED_MEDICOMP,
	WEAPON_PRED_STAFF,

    /* ALIEN WEAPONS */
	WEAPON_ALIEN_CLAW,
	WEAPON_ALIEN_GRAB,
	WEAPON_ALIEN_SPIT,

	WEAPON_CUDGEL,
	WEAPON_MARINE_PISTOL,
	WEAPON_FRISBEE_LAUNCHER,
	WEAPON_TWO_PISTOLS,

	MAX_NO_OF_WEAPON_TEMPLATES
};

enum WEAPON_SLOT
{
	WEAPON_SLOT_1=0,
	WEAPON_SLOT_2,
	WEAPON_SLOT_3,
	WEAPON_SLOT_4,
	WEAPON_SLOT_5,
	WEAPON_SLOT_6,
	WEAPON_SLOT_7,
	WEAPON_SLOT_8,
	WEAPON_SLOT_9,
	WEAPON_SLOT_10,
	WEAPON_SLOT_11,
	WEAPON_SLOT_12,
	WEAPON_SLOT_13,
	WEAPON_SLOT_14,
	MAX_NO_OF_WEAPON_SLOTS,

	WEAPON_FINISHED_SWAPPING
};


enum AMMO_ID
{
	AMMO_NONE=-1,
	AMMO_10MM_CULW=0,
	AMMO_SHOTGUN,
	AMMO_SMARTGUN,
	AMMO_FLAMETHROWER,
	AMMO_PLASMA,
	AMMO_SADAR_TOW,
	AMMO_GRENADE,
	AMMO_MINIGUN,
	AMMO_PULSE_GRENADE,
	AMMO_FLARE_GRENADE,
	AMMO_FRAGMENTATION_GRENADE,
	AMMO_PROXIMITY_GRENADE,
	AMMO_PARTICLE_BEAM,
	AMMO_SONIC_PULSE,

	AMMO_PRED_WRISTBLADE,
	AMMO_PRED_PISTOL,
	AMMO_PRED_RIFLE,
	AMMO_PRED_ENERGY_BOLT,
	AMMO_PRED_DISC,

	AMMO_ALIEN_CLAW,
	AMMO_ALIEN_TAIL,
	AMMO_ALIEN_SPIT,

	AMMO_AUTOGUN,
	AMMO_XENOBORG,
	AMMO_FACEHUGGER,
	AMMO_NPC_OBSTACLE_CLEAR,
	AMMO_ALIEN_FRAG,
	AMMO_ALIEN_DEATH,

	AMMO_SHOTGUN_BLAST,
	AMMO_SADAR_BLAST,
	AMMO_ALIEN_BITE_KILLSECTION,
	AMMO_PRED_DISC_PM,

	AMMO_NPC_ALIEN_CLAW,
	AMMO_NPC_PAQ_CLAW,
	AMMO_PULSE_GRENADE_STRIKE,
	AMMO_NPC_ALIEN_TAIL,
	AMMO_NPC_ALIEN_BITE,
	AMMO_NPC_PREDALIEN_CLAW,
	AMMO_NPC_PREDALIEN_BITE,
	AMMO_NPC_PREDALIEN_TAIL,
	AMMO_NPC_PRAETORIAN_CLAW,
	AMMO_NPC_PRAETORIAN_BITE,
	AMMO_NPC_PRAETORIAN_TAIL,
	AMMO_PRED_STAFF,
	AMMO_NPC_PRED_STAFF,
	AMMO_PC_ALIEN_BITE,
	AMMO_HEAVY_PRED_WRISTBLADE,
	AMMO_MARINE_PISTOL,
	AMMO_PREDPISTOL_STRIKE,
	AMMO_PLASMACASTER_NPCKILL,
	AMMO_PLASMACASTER_PCKILL,

	AMMO_10MM_CULW_NPC,
	AMMO_SMARTGUN_NPC,
	AMMO_MINIGUN_NPC,
	AMMO_MOLOTOV,
	AMMO_ALIEN_OBSTACLE_CLEAR,
	AMMO_PRED_TROPHY_KILLSECTION,

	AMMO_CUDGEL,
	AMMO_ALIEN_BITE_KILLSECTION_SUPER,
	AMMO_MARINE_PISTOL_PC,
	AMMO_FRISBEE,
	AMMO_FRISBEE_BLAST,
	AMMO_FRISBEE_FIRE,

	// The new guns' own. The numbers are MelPyth M5's. The player's shotgun has its own row, so that the computer's shotgun marines stay as they are.
	AMMO_PLAYER_SHOTGUN,
	AMMO_SNIPER,
	AMMO_REVOLVER,
	AMMO_M79_STANDARD,
	AMMO_M79_FIRE,
	AMMO_M79_FRAG,
	AMMO_M79_DIRECT_HIT,
	AMMO_M79_FIRE_DIRECT_HIT,

	MAX_NO_OF_AMMO_TEMPLATES,
	AMMO_FLECHETTE_POSTMAX,
	AMMO_FALLING_POSTMAX,
	AMMO_FIREDAMAGE_POSTMAX
};

/* CDF - 15/9/97 New Damage System */
typedef struct
{
	short Impact;
	short Cutting;
	short Penetrative;
	short Fire;
	short Electrical;
	short Acid;
	/* New additions, 4/8/98 */
	unsigned int ExplosivePower	:3;
	/* XP: 0 is nothing, 1 is little, 2 is big (SADAR),
		3 is pred pistol, 4 is plasmacaster, 5 is molotov, 6 is big with no collisions,
		7+ unused. */
	unsigned int Slicing		:2;
	unsigned int ProduceBlood	:1;
	unsigned int ForceBoom		:2;
	unsigned int BlowUpSections	:1;
	unsigned int Special		:1;
	unsigned int MakeExitWounds	:1;

	enum AMMO_ID Id;
} DAMAGE_PROFILE;

/* CDF - 15/9/97 New Damage System */

typedef struct
{
	unsigned int MovementMultiple;
	unsigned int TurningMultiple;
	unsigned int JumpingMultiple;
	unsigned int CanCrouch:1;
	unsigned int CanRun:1;
} ENCUMBERANCE_STATE;

typedef struct
{
	int					AmmoPerMagazine;

	DAMAGE_PROFILE		MaxDamage[I_MaxDifficulties];
	int					MaxRange;

	enum TEXTSTRING_ID	ShortName;
		/* Added 20/11/97 by DHM: Abberviated name for ammo, to appear in HUD status panel */

    /* ammo flags */
	unsigned int		CreatesProjectile :1;
	unsigned int		ExplosionIsFlat:1;
} TEMPLATE_AMMO_DATA;

typedef struct
{
	enum WEAPON_ID		WeaponIDNumber;
	/* eg.==MARINE_WEAPON_PULSE. This can be used to access the TemplateWeapon[] data, graphics, etc. */

	enum WEAPON_STATE 	CurrentState;

	fixed_t				 StateTimeOutCounter;    /* in 16.16 */

	unsigned int		PrimaryRoundsRemaining;    /* in 'current' magazine */
	unsigned int		SecondaryRoundsRemaining;  /* in 'current' magazine */
	unsigned char		PrimaryMagazinesRemaining;
	unsigned char		SecondaryMagazinesRemaining;

	// The rounds left over from reloading part way through a magazine, so that none are lost. It's in whole rounds, and nobody sees it. See weapon_reload.h. It fits in the two bytes that were spare here, so the slot and a saved game stay the same size.
	unsigned short		PrimaryLeftoverRounds;

	VECTORCH			PositionOffset;
	EULER				DirectionOffset;

	SHAPEANIMATIONCONTROLLER ShpAnimCtrl;
	TXACTRLBLK			*TxAnimCtrl;

	/* general flags */
	signed int		Possessed :2; /* meaning you are carrying the weapon, not that an evil spirit etc etc. */
} PLAYER_WEAPON_DATA;

// What a gun with a file of its own can do that the common weapon code doesn't. A gun fills in only what it needs. Any of it can be left out, and no other weapon has any of it. playerStatus is the PLAYER_STATUS, as it is for a state's function.
typedef struct
{
	// A player has just started, or started again after dying. The gun forgets what it was doing, and puts back anything it changed for him, like the zoom.
	void	(*AtPlayerStart)(void);

	// Each frame while this gun is in his hands, just before the common code looks at the keys for an idle gun. Returning 1 means the gun has dealt with this frame itself, so the common code leaves the keys alone.
	int		(*EveryFrame)(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);

	// The gun has just gone into a new state, from wherever. It can go straight on to another one from here.
	void	(*StateBegan)(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);

	// A state's time is up. Returning 0 keeps it going, for a state that lasts as long as a key is held.
	int		(*MayLeaveState)(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);

	// A state's time is up, and it may leave. Returning 1 means the gun has chosen what comes next. Returning 0 means the common code does what it does for any gun.
	int		(*StateEnded)(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);

	// He's walked over ammunition for this gun: may it be added now? For a gun that's busy loading. Nothing calls this yet, and it goes in with the pickups.
	int		(*MayTakeAmmo)(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);

	// Is the gun drawn in his hands? Returning 0 leaves it out, for looking through a scope.
	int		(*DrawGun)(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);

	// Draws anything flat over the screen, like a scope's picture or a list of grenades. It comes after the world and the gun.
	void	(*DrawOverHud)(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);

	// The gun's own state, which goes into a save: a block of plain numbers with no pointers in it, and how big it is.
	void	*OwnState;
	int		OwnStateSize;

	// Is this copy of its own state, read from a save, safe to use? A save is a file that anybody could have changed, so each number is checked. A state that fails is left out, and the gun carries on as it started.
	int		(*OwnStateIsGood)(const void *state);

	// 1 if the slot's "magazines" count single rounds for this gun, not whole magazines, because it loads a round at a time. Then filling the gun takes as many as it holds, and not just one.
	int		SparesAreSingleRounds;

	// Draws the gun's own ammunition panel, in place of the usual rounds and magazines. For a gun whose count doesn't fit them.
	void	(*DrawAmmoPanel)(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);
} WEAPON_HOOKS;

typedef struct
{
	enum AMMO_ID		PrimaryAmmoID;
	enum AMMO_ID		SecondaryAmmoID;

	int					(*FirePrimaryFunction)(PLAYER_WEAPON_DATA *);
	int					(*FireSecondaryFunction)(PLAYER_WEAPON_DATA *);
	void				(*WeaponInitFunction)(PLAYER_WEAPON_DATA *);

	fixed_t				TimeOutRateForState[MAX_NO_OF_WEAPON_STATES];    /* in 16.16 */
	void				(*WeaponStateFunction[MAX_NO_OF_WEAPON_STATES])(void *, PLAYER_WEAPON_DATA *);    // void * is for PLAYER_STATUS
	int					ProbabilityOfJamming;
	int					FiringRate;

    /* display stuff */
	signed int			SmartTargetSpeed;    /* how fast crosshair moves */
	unsigned int		GunCrosshairSpeed;   /* how fast the gun moves to catch up */
	unsigned int		SmartTargetRadius;
	VECTORCH			RestPosition;

	int					RecoilMaxZ;
	int					RecoilMaxRandomZ;
	int					RecoilMaxXTilt;
	int					RecoilMaxYTilt;

	VECTORCH			StrikePosition;

	enum TEXTSTRING_ID	Name;

    /* shape reference */
	char *				WeaponShapeName;
	char *				MuzzleFlashShapeName;
	char *				RiffName;
	char *				HierarchyName;
	int					InitialSequenceType;
	int					InitialSubSequence;

    /* Encumberance */
	ENCUMBERANCE_STATE	Encum_Idle;
	ENCUMBERANCE_STATE	Encum_FirePrime;
	ENCUMBERANCE_STATE	Encum_FireSec;

    /* flags */
	unsigned int		UseStateMovement :1;
	unsigned int		IsSmartTarget :1;
	unsigned int		PrimaryIsRapidFire   :1;
	unsigned int		PrimaryIsAutomatic   :1;
	unsigned int		PrimaryIsMeleeWeapon :1;
	unsigned int		SecondaryIsRapidFire   :1;
	unsigned int		SecondaryIsAutomatic   :1;
	unsigned int		SecondaryIsMeleeWeapon :1;
	unsigned int		HasShapeAnimation: 1;
	unsigned int		HasTextureAnimation: 1;
	unsigned int		FireWhenCloaked: 1;
	unsigned int		FireInChangeVision: 1;
	unsigned int		FirePrimaryLate: 1;
	unsigned int		FireSecondaryLate: 1;
	unsigned int		PrimaryMuzzleFlash:	1;
	unsigned int		SecondaryMuzzleFlash:	1;
	unsigned int		LogAccuracy:	1;
	unsigned int		LogShots:	1;

	// What this gun does that the common code doesn't. Only the guns with files of their own have one.
	const WEAPON_HOOKS	*Hooks;

} TEMPLATE_WEAPON_DATA;

typedef struct
{
	enum AMMO_ID	SelectedAmmo;

	unsigned int	StandardRoundsRemaining;
	unsigned char	StandardMagazinesRemaining;

	unsigned int	FlareRoundsRemaining;
	unsigned char	FlareMagazinesRemaining;

	unsigned int	ProximityRoundsRemaining;
	unsigned char	ProximityMagazinesRemaining;

	unsigned int	FragmentationRoundsRemaining;
	unsigned char	FragmentationMagazinesRemaining;

} GRENADE_LAUNCHER_DATA;

typedef enum Pred_Disc_Modes
{
	I_Seek_Track,
	I_Search_Destroy,
	I_Proximity_Mine,
} PRED_DISC_MODES;

typedef enum Smartgun_Modes
{
	I_Track,
	I_Free,
} SMARTGUN_MODES;

extern PRED_DISC_MODES ThisDiscMode;
extern SMARTGUN_MODES SmartgunMode;

extern TEMPLATE_WEAPON_DATA TemplateWeapon[];
extern TEMPLATE_AMMO_DATA   TemplateAmmo[];
extern GRENADE_LAUNCHER_DATA GrenadeLauncherData;

extern enum WEAPON_ID MarineWeaponKey[MAX_NO_OF_WEAPON_SLOTS];
extern enum WEAPON_ID PredatorWeaponKey[MAX_NO_OF_WEAPON_SLOTS];
extern enum WEAPON_ID AlienWeaponKey[MAX_NO_OF_WEAPON_SLOTS];

extern DAMAGE_PROFILE certainDeath;
extern DAMAGE_PROFILE console_nuke;
extern DAMAGE_PROFILE firedamage;

extern DAMAGE_PROFILE SmallExplosionDamage;
extern DAMAGE_PROFILE BigExplosionDamage;

extern void InitialiseEquipment(void);

/*compare two damage profiles to see if they are the same*/
extern BOOL AreDamageProfilesEqual(DAMAGE_PROFILE* profile1,DAMAGE_PROFILE* profile2);

#endif
