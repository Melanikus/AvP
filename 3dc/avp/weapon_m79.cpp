// The M79. The game takes this from MelPyth M5 by Mupp. Only how it behaves was read from that mod, and this is written new.
// Written 2026-11-06 by Melanikus for the AvP port.
//
// The sums are in m79.h, which says what the gun does. This file joins them to the game: the gun in his hands, the list
// of kinds on the screen, and the grenades once they've left the gun.
//
// WHERE THE NUMBERS LIVE: the common weapon code looks after the rounds and spares in the gun's slot, and those are always
// the numbers for the kind that's chosen. The other two kinds wait in the gun's own state. M79_NumbersFromSlot and
// M79_NumbersToSlot pass them across.
//
// THE GRENADES: each kind goes off when it touches anything at all, or after two seconds. Something a standard or fire
// grenade hits directly takes extra damage first. A frag grenade's flying pieces do all its work.

#include "3dc.h"
#include "module.h"
#include "inline.h"
#include "strategy_def.h"
#include "gamedef.h"
#include "bh_types.h"
#include "dynblock.h"
#include "dynamics.h"
#include "hmodel.h"
#include "sequences.h"
#include "weapons.h"
#include "bh_weapon.h"
#include "particle.h"
#include "lighting.h"
#include "psnd.h"
#include "language.h"
#include "savegame.h"
#include "game_statistics.h"
#include "weapon_hooks.h"
#include "weapon_m79.h"
#include "load_shape.h"
#include "pldnet.h"

#define UseLocalAssert FALSE
#include "ourasert.h"

extern DISPLAYBLOCK PlayersWeapon;
extern HMODELCONTROLLER PlayersWeaponHModelController;
extern SOUND3DDATA Explosion_SoundData;
extern SCREENDESCRIPTORBLOCK ScreenDescriptorBlock;
extern fixed_t HUDScaleFactor;
extern int NormalFrameTime;

extern void PulseRifleFidget(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);
extern void NewOnScreenMessage(char *messagePtr);
extern void D3D_RenderHUDString_Centred(char *stringPtr, uint32_t centreX, uint32_t y, uint32_t colour);
extern void D3D_RenderHUDNumber_Centred(uint32_t number, uint32_t x, uint32_t y, uint32_t colour);
extern STRATEGYBLOCK *InitialiseGrenadeBehaviour(AVP_BEHAVIOUR_TYPE behaviourID);
extern STRATEGYBLOCK *CreateGrenadeKernel(AVP_BEHAVIOUR_TYPE behaviourID, VECTORCH *position, MATRIXCH *orient, int fromplayer);
extern void MakeGrenadeTrailParticles(VECTORCH *prevPositionPtr, VECTORCH *positionPtr);
extern void MakeFlechetteExplosionAt(VECTORCH *positionPtr, int seed);
extern void MakeMolotovExplosionAt(VECTORCH *positionPtr, int seed);

static M79OwnState Own;

// The part of the model that shows the grenade. It's the same part whichever kind is loaded.
static char kShellSection[] = "M79Granade01";

// What makes each kind different. The shapes are the same grenade in different colours, full or spent.
struct M79KindDetails
{
	enum AMMO_ID ammo;
	AVP_BEHAVIOUR_TYPE behaviour;
	const char *loadedShape;
	const char *spentShape;
	enum TEXTSTRING_ID name;
};

static const M79KindDetails kKinds[kM79Kinds] =
{
	{ AMMO_M79_STANDARD, I_BehaviourM79Grenade, "M79Granade01", "M79Granade01_Empty", TEXTSTRING_AMMO_SHORTNAME_M79_STANDARD },
	{ AMMO_M79_FIRE, I_BehaviourM79FireGrenade, "M79Granade04", "M79Granade04_Empty", TEXTSTRING_AMMO_SHORTNAME_M79_FIRE },
	{ AMMO_M79_FRAG, I_BehaviourM79FragGrenade, "M79Granade03", "M79Granade03_Empty", TEXTSTRING_AMMO_SHORTNAME_M79_FRAG },
};

// How fast a grenade leaves this gun. The game's other grenades go at 70000.
static const int kM79GrenadeSpeed = 110000;

// How long each move takes, in the game's time, where 65536 is a second
static const int kM79QuickMove = 65536 * 3 / 4;
static const int kM79LongMove = 65536 * 3 / 2;

static void M79_NumbersFromSlot(PLAYER_WEAPON_DATA const *weaponPtr)
{
	Own.RoundsInGun[Own.Kind] = weaponPtr->PrimaryRoundsRemaining > 0 ? 1 : 0;
	Own.Spares[Own.Kind] = weaponPtr->PrimaryMagazinesRemaining > kM79MostSpares ? kM79MostSpares : weaponPtr->PrimaryMagazinesRemaining;
}

static void M79_NumbersToSlot(PLAYER_WEAPON_DATA *weaponPtr)
{
	weaponPtr->PrimaryRoundsRemaining = Own.RoundsInGun[Own.Kind] > 0 ? ONE_FIXED : 0;
	weaponPtr->PrimaryMagazinesRemaining = Own.Spares[Own.Kind];
}

// Shows a full or a spent grenade in the gun. There may be no model yet, and the file could lack the shape.
static void M79_ShowShell(bool spent)
{
	if (!PlayersWeapon.HModelControlBlock)
	{
		return;
	}

	SECTION_DATA *const section = GetThisSectionData(PlayersWeaponHModelController.section_data, kShellSection);
	char const *const name = spent ? kKinds[Own.Kind].spentShape : kKinds[Own.Kind].loadedShape;
	int const shape = GetLoadedShapeMSL((char *)name);

	if (section && shape >= 0)
	{
		section->ShapeNum = shape;
		section->Shape = GetShapeData(shape);
		Setup_Texture_Animation_For_Section(section);
	}
}

// Plays one of the gun's moves on its model, from the beginning. The model may not have it, or there may be no model.
static void M79_Play(enum MarineHUDSubSequences move, int time)
{
	if (PlayersWeapon.HModelControlBlock && HModelSequence_Exists(&PlayersWeaponHModelController, HMSQT_MarineHUD, (int)move))
	{
		InitHModelSequence(&PlayersWeaponHModelController, HMSQT_MarineHUD, (int)move, time);
		PlayersWeaponHModelController.Looped = 0;
	}
}

void M79_Idle(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	if (PlayersWeapon.HModelControlBlock)
	{
		PulseRifleFidget(playerStatus, weaponPtr);
	}
}

void M79_Recoil(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	if (weaponPtr->StateTimeOutCounter == WEAPONSTATE_INITIALTIMEOUTCOUNT)
	{
		M79_Play(MHSS_Standard_Fire, kM79QuickMove);
	}
}

void M79_SwapIn(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	if (weaponPtr->StateTimeOutCounter == WEAPONSTATE_INITIALTIMEOUTCOUNT)
	{
		M79_Play(MHSS_Come, kM79QuickMove);
	}
}

void M79_SwapOut(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	if (weaponPtr->StateTimeOutCounter == WEAPONSTATE_INITIALTIMEOUTCOUNT)
	{
		M79_Play(MHSS_Go, kM79QuickMove);
	}
}

// The model marks the moment in a loading move when the new grenade goes in, with a flag on one of its frames.
static void M79_WatchForNewGrenade(int flag)
{
	if (PlayersWeapon.HModelControlBlock && (PlayersWeaponHModelController.keyframe_flags & flag))
	{
		M79_ShowShell(false);
	}
}

void M79_Reload(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	if (weaponPtr->StateTimeOutCounter == WEAPONSTATE_INITIALTIMEOUTCOUNT)
	{
		M79_Play(MHSS_Standard_Reload, kM79LongMove);
	}

	M79_WatchForNewGrenade(2);
}

// Changing kind: a full reload if the gun was empty, and otherwise a shorter move that only swaps the grenade.
void M79_ChangingKind(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	if (weaponPtr->StateTimeOutCounter == WEAPONSTATE_INITIALTIMEOUTCOUNT)
	{
		M79_Play(Own.GunWasEmptyAtChange ? MHSS_Standard_Reload : MHSS_Tertiary_Fire, kM79LongMove);
	}

	M79_WatchForNewGrenade(Own.GunWasEmptyAtChange ? 2 : 1);
}

int FireM79(PLAYER_WEAPON_DATA *weaponPtr)
{
	PLAYER_STATUS *const playerStatusPtr = (PLAYER_STATUS *)(Player->ObStrategyBlock->SBdataptr);
	STRATEGYBLOCK *const grenade = InitialiseGrenadeBehaviour(kKinds[Own.Kind].behaviour);

	// The game makes its grenades at their own speed. This one's faster, in the same direction.
	if (grenade && grenade->DynPtr)
	{
		DYNAMICSBLOCK *const dynPtr = grenade->DynPtr;

		dynPtr->LinImpulse.vx = MUL_FIXED(dynPtr->OrientMat.mat31, kM79GrenadeSpeed);
		dynPtr->LinImpulse.vy = MUL_FIXED(dynPtr->OrientMat.mat32, kM79GrenadeSpeed);
		dynPtr->LinImpulse.vz = MUL_FIXED(dynPtr->OrientMat.mat33, kM79GrenadeSpeed);
	}

	weaponPtr->PrimaryRoundsRemaining = 0;
	CurrentGameStats_WeaponFired(playerStatusPtr->SelectedWeaponSlot, 1);
	M79_ShowShell(true);

	return 1;
}

// Whether the key was already down last frame, so that one press gives one tap. It starts as "down", so a key still
// held from the last gun doesn't count. It's only about this moment, so it doesn't go into a save.
static int SecondaryWasDown = 1;

// The gun's just been brought up, or a save's just been loaded. The model's new, so it needs telling what to show.
void M79_WhenBroughtUp(PLAYER_WEAPON_DATA *weaponPtr)
{
	Own.ChangeIsWaiting = 0;
	Own.KindToChangeTo = Own.Kind;
	Own.ChangeCountdown = 0;
	SecondaryWasDown = 1;

	M79_ShowShell(weaponPtr->PrimaryRoundsRemaining == 0);
}

static void M79_AtPlayerStart(void)
{
	Own.Kind = kM79Standard;
	Own.ChangeIsWaiting = 0;
	Own.KindToChangeTo = kM79Standard;
	Own.ChangeCountdown = 0;
	Own.GunWasEmptyAtChange = 0;

	for (int kind = 0; kind < kM79Kinds; kind++)
	{
		Own.RoundsInGun[kind] = 0;
		Own.Spares[kind] = 0;
	}
}

static void M79_DoWaitingChange(PLAYER_WEAPON_DATA *weaponPtr)
{
	if (M79_ChangeKind(Own, Own.KindToChangeTo))
	{
		M79_NumbersToSlot(weaponPtr);
		NewOnScreenMessage(GetTextString(kKinds[Own.Kind].name));
		Weapon_EnterState(weaponPtr, kM79State_ChangeKind);
	}
}

static int M79_EveryFrame(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	PLAYER_STATUS const *const playerStatusPtr = (PLAYER_STATUS const *)playerStatus;
	int const state = (int)weaponPtr->CurrentState;
	int const secondaryDown = playerStatusPtr->Mvt_InputRequests.Flags.Rqst_FireSecondaryWeapon ? 1 : 0;

	M79_NumbersFromSlot(weaponPtr);

	if (Own.ChangeIsWaiting && Own.ChangeCountdown > 0)
	{
		Own.ChangeCountdown -= NormalFrameTime;

		if (Own.ChangeCountdown < 0)
		{
			Own.ChangeCountdown = 0;
		}
	}

	// A tap counts while he's loading or firing as well, and the change then waits until the gun's idle
	bool const canTap = state == WEAPONSTATE_IDLE || state == WEAPONSTATE_FIRING_PRIMARY || state == WEAPONSTATE_RECOIL_PRIMARY
		|| state == WEAPONSTATE_RELOAD_PRIMARY || state == kM79State_ChangeKind;

	if (secondaryDown && !SecondaryWasDown && canTap && M79_TapChangeKey(Own))
	{
		Sound_Play(SID_MENUS_CHANGE_ITEM, "h");
	}

	SecondaryWasDown = secondaryDown;

	if (state != WEAPONSTATE_IDLE)
	{
		return 0;
	}

	if (Own.ChangeIsWaiting && Own.ChangeCountdown <= 0)
	{
		M79_DoWaitingChange(weaponPtr);
		return 1;
	}

	// This kind has run out but another hasn't, so he changes to it, and doesn't put the gun away
	if (!Own.ChangeIsWaiting && !M79_KindHasAmmo(Own, Own.Kind) && M79_TapChangeKey(Own))
	{
		Own.ChangeCountdown = 0;
		return 1;
	}

	// While he's choosing, the gun can still fire, but it won't start loading a grenade he's about to take out
	if (Own.ChangeIsWaiting && !(playerStatusPtr->Mvt_InputRequests.Flags.Rqst_FirePrimaryWeapon && weaponPtr->PrimaryRoundsRemaining > 0))
	{
		return 1;
	}

	return 0;
}

// When a state's time is up. Everything but the change of kind goes on as for any gun.
static int M79_StateEnded(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	switch ((int)weaponPtr->CurrentState)
	{
		case WEAPONSTATE_RELOAD_PRIMARY:
			// The model's moment may have been missed in a long frame
			M79_ShowShell(false);
			return 0;

		case kM79State_ChangeKind:
			M79_ShowShell(weaponPtr->PrimaryRoundsRemaining == 0);
			weaponPtr->CurrentState = WEAPONSTATE_IDLE;
			weaponPtr->StateTimeOutCounter = 0;
			return 1;

		default:
			return 0;
	}
}

static int M79_SavedStateIsGood(const void *state)
{
	return M79_OwnStateIsGood(*(const M79OwnState *)state) ? 1 : 0;
}

// The list on the screen, down the right-hand side where the usual rounds and magazines go. Each kind has its name and how
// many he has. The chosen one is solid, with its count in red. The one the taps have reached so far is green, and a kind
// with none left is grey.
static void M79_DrawList(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	const uint32_t kSolid = 0xff000000u, kFaint = 0x80000000u;
	const uint32_t kWhite = 0x00ffffffu, kRed = 0x00ff0000u, kGrey = 0x00808080u, kGreen = 0x005fb327u;

	M79_NumbersFromSlot(weaponPtr);

	int const waiting = Own.ChangeIsWaiting ? Own.KindToChangeTo : Own.Kind;
	// In from the right and up from the bottom, by the same safe margin the usual panel keeps
	int const margin = ScreenDescriptorBlock.SDB_SafeZoneWidthOffset;
	int const x = MUL_FIXED(-40, HUDScaleFactor) + ScreenDescriptorBlock.SDB_Width - margin;
	int fromTheBottom = 40;

	for (int kind = kM79Kinds - 1; kind >= 0; kind--)
	{
		int const total = Own.RoundsInGun[kind] + Own.Spares[kind];
		bool const chosen = kind == Own.Kind;
		uint32_t alpha = chosen ? kSolid : kFaint;
		uint32_t nameColour = total > 0 || chosen ? kWhite : kGrey;
		uint32_t countColour = chosen ? kRed : (total > 0 ? kWhite : kGrey);

		if (kind == waiting)
		{
			alpha = kSolid;
			nameColour = kGreen;

			if (!chosen)
			{
				countColour = kGreen;
			}
		}

		D3D_RenderHUDString_Centred(GetTextString(kKinds[kind].name), x, ScreenDescriptorBlock.SDB_Height - MUL_FIXED(HUDScaleFactor, fromTheBottom) - margin, alpha | nameColour);
		D3D_RenderHUDNumber_Centred(total, x, ScreenDescriptorBlock.SDB_Height - MUL_FIXED(HUDScaleFactor, fromTheBottom - 16) - margin, alpha | countColour);

		if (kind > 0)
		{
			fromTheBottom += 46;
		}
	}

	D3D_RenderHUDString_Centred(GetTextString(TEXTSTRING_INGAME_M79), x, ScreenDescriptorBlock.SDB_Height - MUL_FIXED(HUDScaleFactor, fromTheBottom + 25) - margin, 0xffffffffu);
}

const WEAPON_HOOKS M79_Hooks =
{
	M79_AtPlayerStart,
	M79_EveryFrame,
	NULL, // StateBegan
	NULL, // MayLeaveState
	M79_StateEnded,
	NULL, // MayTakeAmmo
	NULL, // DrawGun
	NULL, // DrawOverHud
	&Own,
	sizeof(Own),
	M79_SavedStateIsGood,
	0, // SparesAreSingleRounds: each kind's count is grenades, but one holds a single one anyway
	M79_DrawList
};

int M79_TakeAmmo(enum AMMO_ID ammo, PLAYER_WEAPON_DATA *weaponPtr)
{
	for (int kind = 0; kind < kM79Kinds; kind++)
	{
		if (kKinds[kind].ammo == ammo)
		{
			M79_NumbersFromSlot(weaponPtr);

			if (!M79_AddPickup(Own, kind))
			{
				return 0;
			}

			M79_NumbersToSlot(weaponPtr);
			return 1;
		}
	}

	return 0;
}

void M79_SetEveryKind(PLAYER_WEAPON_DATA *weaponPtr, int spares)
{
	for (int kind = 0; kind < kM79Kinds; kind++)
	{
		Own.Spares[kind] = spares;
	}

	M79_NumbersToSlot(weaponPtr);
}

void M79_FillEveryKind(PLAYER_WEAPON_DATA *weaponPtr)
{
	M79_SetEveryKind(weaponPtr, kM79MostSpares);
}

/*---------------------------------------------------------------------------
  The grenades, once they've left the gun
  ---------------------------------------------------------------------------*/

// Which kind a grenade is. It's only ever asked about the three types below.
static int M79_KindOfGrenade(AVP_BEHAVIOUR_TYPE type)
{
	switch (type)
	{
		case I_BehaviourM79FireGrenade:
			return kM79Fire;

		case I_BehaviourM79FragGrenade:
			return kM79Frag;

		default:
			return kM79Standard;
	}
}

// Extra damage to something the grenade hit directly, from the way it was flying.
static void M79_HitDirectly(DYNAMICSBLOCK *dynPtr, STRATEGYBLOCK *target, enum AMMO_ID ammo)
{
	VECTORCH flying = dynPtr->LinImpulse;
	VECTORCH incoming = { 0, 0, 0 };

	if (flying.vx == 0 && flying.vy == 0 && flying.vz == 0)
	{
		flying.vx = ONE_FIXED;
	}

	GetDirectionOfAttack(target, &flying, &incoming);
	CauseDamageToObject(target, &TemplateAmmo[ammo].MaxDamage[AvP.Difficulty], ONE_FIXED, &incoming);
}

static void M79_GoOff(STRATEGYBLOCK *sbPtr, int kind)
{
	DYNAMICSBLOCK *const dynPtr = sbPtr->DynPtr;
	TEMPLATE_AMMO_DATA *const ammo = &TemplateAmmo[kKinds[kind].ammo];
	VECTORCH place = dynPtr->Position;

	// A frag grenade's own row does no damage, because its flying pieces do that
	HandleEffectsOfExplosion(sbPtr, &place, ammo->MaxRange, &ammo->MaxDamage[AvP.Difficulty], ammo->ExplosionIsFlat);

	switch (kind)
	{
		case kM79Fire:
			MakeMolotovExplosionAt(&place, 0);
			MakeLightElement(&place, LIGHTELEMENT_EXPLOSION);
			break;

		case kM79Frag:
			MakeFlechetteExplosionAt(&place, 0);
			break;

		default:
			break;
	}

	if (sbPtr->containingModule)
	{
		Explosion_SoundData.position = place;
		Sound_Play(kind == kM79Standard ? SID_ED_GRENADE_EXPLOSION : SID_NADEEXPLODE, "n", &Explosion_SoundData);
	}

	if (AvP.Network != I_No_Network)
	{
		AddNetMsg_LocalObjectDestroyed(sbPtr);
	}

	DestroyAnyStrategyBlock(sbPtr);
}

void M79GrenadeBehaviour(STRATEGYBLOCK *sbPtr)
{
	DYNAMICSBLOCK *const dynPtr = sbPtr->DynPtr;
	GRENADE_BEHAV_BLOCK *const behaviour = (GRENADE_BEHAV_BLOCK *)sbPtr->SBdataptr;
	int const kind = M79_KindOfGrenade(sbPtr->I_SBtype);
	COLLISIONREPORT *report = dynPtr->CollisionReportPtr;

	behaviour->counter -= NormalFrameTime;

	if (behaviour->counter > 0 && !report)
	{
		MakeGrenadeTrailParticles(&dynPtr->PrevPosition, &dynPtr->Position);
		DynamicallyRotateObject(dynPtr);
		return;
	}

	// What it touched takes a direct hit. A wall has no strategy block, and nothing to damage.
	for (; report; report = report->NextCollisionReportPtr)
	{
		STRATEGYBLOCK *const target = report->ObstacleSBPtr;

		if (target && kind == kM79Standard)
		{
			M79_HitDirectly(dynPtr, target, AMMO_M79_DIRECT_HIT);
			break;
		}

		if (target && kind == kM79Fire)
		{
			M79_HitDirectly(dynPtr, target, AMMO_M79_FIRE_DIRECT_HIT);
		}
	}

	M79_GoOff(sbPtr, kind);
}

// A grenade in the air goes into a save like any other. What's read back is checked: the save is hostile, and it could
// ask for a timer that never runs out.
typedef struct
{
	SAVE_BLOCK_STRATEGY_HEADER header;

	int counter;
	int bouncelastframe;

	DYNAMICSBLOCK dynamics;
} M79_GRENADE_SAVE_BLOCK;

void LoadStrategy_M79Grenade(SAVE_BLOCK_STRATEGY_HEADER *header)
{
	M79_GRENADE_SAVE_BLOCK const *const block = (M79_GRENADE_SAVE_BLOCK const *)header;

	if (header->size != sizeof(*block))
	{
		return;
	}

	VECTORCH place = block->dynamics.Position;
	MATRIXCH facing = block->dynamics.OrientMat;
	STRATEGYBLOCK *const sbPtr = CreateGrenadeKernel(header->bhvr_type, &place, &facing, 1);

	if (!sbPtr)
	{
		return;
	}

	GRENADE_BEHAV_BLOCK *const behaviour = (GRENADE_BEHAV_BLOCK *)sbPtr->SBdataptr;

	behaviour->counter = block->counter < 0 ? 0 : (block->counter > 2 * ONE_FIXED ? 2 * ONE_FIXED : block->counter);
	behaviour->bouncelastframe = block->bouncelastframe ? 1 : 0;

	*sbPtr->DynPtr = block->dynamics;
	sbPtr->DynPtr->CollisionReportPtr = 0;
}

void SaveStrategy_M79Grenade(STRATEGYBLOCK *sbPtr)
{
	GRENADE_BEHAV_BLOCK const *const behaviour = (GRENADE_BEHAV_BLOCK const *)sbPtr->SBdataptr;
	M79_GRENADE_SAVE_BLOCK *block;

	GET_STRATEGY_SAVE_BLOCK(M79_GRENADE_SAVE_BLOCK, block, sbPtr);

	block->counter = behaviour->counter;
	block->bouncelastframe = behaviour->bouncelastframe;
	block->dynamics = *sbPtr->DynPtr;
	block->dynamics.CollisionReportPtr = 0;
}
