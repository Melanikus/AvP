#include "3dc.h"
#include "module.h"
#include "inline.h"
#include "strategy_def.h"
#include "gamedef.h"
#include "bh_types.h"
#include "huddefs.h"
#include "io.h"
/* patrick's sound include */
#include "psnd.h"
#include "psndplat.h"
#include "vdb.h"
#define UseLocalAssert TRUE
#include "ourasert.h"

/*KJL****************************************************************************************
*                                    P R O T O T Y P E S                                    *
****************************************************************************************KJL*/
#include "vision.h"
#include "frustum.h"
#include "view.h"
#include "game_statistics.h"

/*KJL****************************************************************************************
*                                    G L O B A L S                                          *
****************************************************************************************KJL*/
enum VISION_MODE_ID CurrentVisionMode;

static int visionModeDebounced=0;

extern ACTIVESOUNDSAMPLE ActiveSounds[];
int predOVision_SoundHandle;

extern uint32_t FMVParticleColour;
extern int LogosAlphaLevel;
int PredatorVisionChangeCounter;

static struct PredOVisionDescriptor PredOVision;
static struct MarineOVisionDescriptor MarineOVision;
extern int GlobalAmbience;
/* JH 29/5/97 - to control how D3D does the lighting */
struct D3DLightColourControl d3d_light_ctrl;
/*KJL****************************************************************************************
*                                     F U N C T I O N S	                                    *
****************************************************************************************KJL*/
void SetupVision(void);

extern void DrawNoiseOverlay(int t);
extern void D3D_PredatorScreenInversionOverlay();
extern void D3D_FadeDownScreen(int brightness, int colour);
extern void D3D_ScreenInversionOverlay();

const int MARINE_FOV =	75;
const int ALIEN_FOV	 =	105;

extern void R_SetFov(uint32_t fov);

void SetupVision(void)
{
	/* KJL 16:33:47 01/10/97 - change view for alien;
	this must be called after ProcessSystemObjects() */
	if (AvP.PlayerType == I_Alien)
	{
		/* setup wide-angle lens */
		VIEWDESCRIPTORBLOCK *VDBPtr = ActiveVDBList[0];

		LOCALASSERT(VDBPtr);

		/* change clipping planes for new field of view */
		/* KJL 12:00:54 07/04/97 - new projection angles */
		VDBPtr->VDB_ProjX = ScreenDescriptorBlock.SDB_Width    / 4;
		VDBPtr->VDB_ProjY = (ScreenDescriptorBlock.SDB_Height) / 4;

		/* KJL 17:37:51 7/17/97 - frustum setup */
		SetFrustumType(FRUSTUM_TYPE_WIDE);
		R_SetFov(ALIEN_FOV);
	}
	else if (AvP.PlayerType == I_Predator)
	{
		/* setup normal-angle lens */
		VIEWDESCRIPTORBLOCK *VDBPtr = ActiveVDBList[0];

		LOCALASSERT(VDBPtr);

		/* change clipping planes for new field of view */
		VDBPtr->VDB_ProjX = ScreenDescriptorBlock.SDB_Width    / 2;
		VDBPtr->VDB_ProjY = (ScreenDescriptorBlock.SDB_Height) / 2;

		SetupPredOVision();
		/* KJL 17:37:51 7/17/97 - frustum setup */
		SetFrustumType(FRUSTUM_TYPE_NORMAL);
		R_SetFov(MARINE_FOV);
	}
	else
	{
		/* setup normal-angle lens */
		VIEWDESCRIPTORBLOCK *VDBPtr = ActiveVDBList[0];

		LOCALASSERT(VDBPtr);

		/* change clipping planes for new field of view */
		VDBPtr->VDB_ProjX = ScreenDescriptorBlock.SDB_Width    / 2;
		VDBPtr->VDB_ProjY = (ScreenDescriptorBlock.SDB_Height) / 2;

		SetupMarineOVision();
		/* KJL 17:37:51 7/17/97 - frustum setup */
		SetFrustumType(FRUSTUM_TYPE_NORMAL);
		R_SetFov(MARINE_FOV);
	}

	InitCameraValues();

	/* KJL 12:01:09 16/02/98 - init visionmode */
	CurrentVisionMode = VISION_MODE_NORMAL;

	predOVision_SoundHandle=SOUND_NOACTIVEINDEX;
	PredatorVisionChangeCounter=0;

	FMVParticleColour = RGBA_MAKE(255,255,255,128);
	LogosAlphaLevel = 3*ONE_FIXED;
}

/*
IMPORTANT NOTE OF KNOWN BUG NOT YET FIXED
Note that there is a potential problem in that
if the game exits with *-o-vision on or in transition,
the next instance of playing the game with the
alien character will still have the same vision settings.
- JH
*/

/* Pred-O-Vision */
void SetupPredOVision(void)
{
	/* setup in-game data */
	PredOVision.VisionMode = PREDOVISION_NORMAL;
	PredOVision.VisionIsChanging = 0;
	/* JH - 29/5/97 for d3d */
	d3d_light_ctrl.ctrl = LCCM_NORMAL;
}


void HandlePredOVision(void)
{
	PLAYER_STATUS *playerStatusPtr = (PLAYER_STATUS *) (Player->ObStrategyBlock->SBdataptr);

	if (playerStatusPtr->IsAlive)
	{
		CurrentGameStats_VisionMode(CurrentVisionMode);
	}

	if (playerStatusPtr->cloakOn)
	{
		DrawNoiseOverlay(16);
	}

	if (CurrentVisionMode==VISION_MODE_PRED_SEEPREDTECH)
	{
		D3D_PredatorScreenInversionOverlay();
	}

	if (CurrentVisionMode==VISION_MODE_NORMAL)
	{
		if (predOVision_SoundHandle!=SOUND_NOACTIVEINDEX)
		{
			Sound_Stop(predOVision_SoundHandle);
		}
	}
	else
	{
		if (predOVision_SoundHandle!=SOUND_NOACTIVEINDEX)
		{
			if (ActiveSounds[predOVision_SoundHandle].soundIndex!=SID_PREDATOR_CLOAKING_ACTIVE)
			{
				Sound_Stop(predOVision_SoundHandle);
			}
		}
		if (predOVision_SoundHandle==SOUND_NOACTIVEINDEX)
		{
			Sound_Play(SID_PREDATOR_CLOAKING_ACTIVE,"elh",&predOVision_SoundHandle);
		}
	}

	if (PredatorVisionChangeCounter)
	{
		D3D_FadeDownScreen(ONE_FIXED-PredatorVisionChangeCounter,0xffffffff);
		PredatorVisionChangeCounter -= NormalFrameTime*4;

		if (PredatorVisionChangeCounter<0)
		{
			PredatorVisionChangeCounter=0;
		}
	}
}

extern void ChangePredatorVisionMode(void)
{
	switch (CurrentVisionMode)
	{
		case VISION_MODE_NORMAL:
		{
			CurrentVisionMode=VISION_MODE_PRED_THERMAL;
			break;
		}
		case VISION_MODE_PRED_THERMAL:
		{
			CurrentVisionMode=VISION_MODE_PRED_SEEALIENS;
			break;
		}
		case VISION_MODE_PRED_SEEALIENS:
		{
			CurrentVisionMode=VISION_MODE_PRED_SEEPREDTECH;
			break;
		}
		case VISION_MODE_PRED_SEEPREDTECH:
		{
			CurrentVisionMode=VISION_MODE_NORMAL;
			break;
		}
		default:
			break;
	}
	Sound_Play(SID_VISION_ON,"h");
	PredatorVisionChangeCounter=ONE_FIXED;
}


/* Marine-O-Vision */

void SetupMarineOVision(void)
{
	/* setup in-game data */
	MarineOVision.VisionMode = MARINEOVISION_NORMAL;
	MarineOVision.VisionIsChanging = 0;
	/* JH - 29/5/97 for d3d */
	d3d_light_ctrl.ctrl = LCCM_NORMAL;
}

void HandleMarineOVision(void)
{
	PLAYER_STATUS *playerStatusPtr = (PLAYER_STATUS *) (Player->ObStrategyBlock->SBdataptr);

	if (playerStatusPtr->IsAlive)
	{
		CurrentGameStats_VisionMode(CurrentVisionMode);
	}

	if (CurrentVisionMode == VISION_MODE_IMAGEINTENSIFIER)
	{
		DrawNoiseOverlay(64);
	}

	/* We might have just morphed. */
	if (predOVision_SoundHandle!=SOUND_NOACTIVEINDEX) {
		Sound_Stop(predOVision_SoundHandle);
	}

	if (playerStatusPtr->Mvt_InputRequests.Flags.Rqst_ChangeVision)
	{
		if (visionModeDebounced)
		{
			visionModeDebounced = 0;
			if (CurrentVisionMode == VISION_MODE_IMAGEINTENSIFIER)
			{
				/* then we'll be changing to normal vision */
				NewOnScreenMessage(GetTextString(TEXTSTRING_INGAME_INTENSIFIEROFF));
				CurrentVisionMode = VISION_MODE_NORMAL;
				FMVParticleColour = RGBA_MAKE(255,255,255,128);
				Sound_Play(SID_IMAGE_OFF,"h");
			}
			else
			{
				/* then we'll be changing to intensified vision */
				NewOnScreenMessage(GetTextString(TEXTSTRING_INGAME_INTENSIFIERON));
				CurrentVisionMode = VISION_MODE_IMAGEINTENSIFIER;
				FMVParticleColour = RGBA_MAKE(0,255,0,128);
				Sound_Play(SID_IMAGE,"h");
			}
		}
	}
	else
	{
		visionModeDebounced = 1;
	}
}


void HandleAlienOVision(void)
{
	PLAYER_STATUS *playerStatusPtr= (PLAYER_STATUS *) (Player->ObStrategyBlock->SBdataptr);

	if (playerStatusPtr->IsAlive)
	{
		CurrentGameStats_VisionMode(CurrentVisionMode);
	}

	if (CurrentVisionMode == VISION_MODE_ALIEN_SENSE)
	{
		D3D_ScreenInversionOverlay();
	}

	/* We might have just morphed. */
	if (predOVision_SoundHandle!=SOUND_NOACTIVEINDEX)
	{
		Sound_Stop(predOVision_SoundHandle);
	}

	if (playerStatusPtr->Mvt_InputRequests.Flags.Rqst_ChangeVision)
	{
		if (visionModeDebounced)
		{
			visionModeDebounced = 0;
			if (CurrentVisionMode == VISION_MODE_ALIEN_SENSE)
			{
				/* then we'll be changing to normal vision */
				CurrentVisionMode = VISION_MODE_NORMAL;
				FMVParticleColour = RGBA_MAKE(255,255,255,128);
			}
			else
			{
				/* then we'll be changing to alien sense */
				CurrentVisionMode = VISION_MODE_ALIEN_SENSE;
				FMVParticleColour = RGBA_MAKE(255,255,255,128);
			}
		}
	}
	else
	{
		visionModeDebounced = 1;
	}

}
/* used when the palette has been changed, eg. by database screens */


int IsVisionChanging(void)
{
	switch (AvP.PlayerType)
	{
		case I_Marine:
			return (MarineOVision.VisionIsChanging);
			break;
		case I_Predator:
			return (PredOVision.VisionIsChanging);
			break;
		case I_Alien:
			return(0);
			break;
		default:
			return(0);
			break;
	}
}
