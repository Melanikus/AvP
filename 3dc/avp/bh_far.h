// Modified 2026-11-06 by Melanikus for the AvP port:
//   how many NPCs may be near at once comes from NearNpcLimit, which gives each player a share in a game that plays the same as single player

/*------------------------Patrick 26/11/96-----------------------------
  Header file for FAR AI alien behaviour
  --------------------------------------------------------------------*/

#ifndef _bhfar_h_
#define _bhfar_h_ 1

/* enum for far alien target module	status */
typedef	enum fnpc_targetmodulestatus
{
	NPCTM_NoEntryPoint,
	NPCTM_NormalRoom,
	NPCTM_AirDuct,
	NPCTM_LiftTeleport,
	NPCTM_ProxDoorOpen,
	NPCTM_ProxDoorNotOpen,
	NPCTM_LiftDoorOpen,
	NPCTM_LiftDoorNotOpen,
	NPCTM_SecurityDoorOpen,
	NPCTM_SecurityDoorNotOpen,


} NPC_TARGETMODULESTATUS;

/* prototypes */
extern void FarAlienBehaviour(STRATEGYBLOCK *sbPtr);
extern void BuildFarModuleLocs(void);
extern void KillFarModuleLocs(void);

extern void LocateFarNPCInModule(STRATEGYBLOCK *sbPtr, MODULE *targetModule);
extern void LocateFarNPCInAIModule(STRATEGYBLOCK *sbPtr, AIMODULE *targetModule);
extern NPC_TARGETMODULESTATUS GetTargetAIModuleStatus(STRATEGYBLOCK *sbPtr, AIMODULE *targetModule, int alien);

extern AIMODULE *FarNPC_GetTargetAIModuleForHunt(STRATEGYBLOCK *sbPtr, int alien);
extern AIMODULE *FarNPC_GetTargetAIModuleForGlobalHunt(STRATEGYBLOCK *sbPtr);
extern AIMODULE *FarNPC_GetTargetAIModuleForWander(STRATEGYBLOCK *sbPtr, AIMODULE *exception, int alien);
extern AIMODULE *FarNPC_GetTargetAIModuleForRetreat(STRATEGYBLOCK *sbPtr);
extern AIMODULE *FarNPC_GetTargetAIModuleForMarineRespond(STRATEGYBLOCK *sbPtr);
extern void FarNpc_FlipAround(STRATEGYBLOCK *sbPtr);

/* this define to help stop aliens coagulating in the environment */
#define MAX_GENERATORNPCSPERMODULE	5
#define MAX_VISIBLEGENERATORNPCS	8 //12

// How many NPCs may be near at once. That limit is for what one player can see, so a game that plays the same as single player with several players gets it once for each.
int NearNpcLimit(void);

#endif
