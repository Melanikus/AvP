// Modified 2026-11-04 by Melanikus for the AvP port:
//   says what size a video screen's picture has to be
// Modified 2026-11-06 by Melanikus for the AvP port:
//   whatever is called for each frame of a film can stop it, as a key would
//   something can be done for each frame of a film
#ifndef _fmvCutscenes_h_
#define _fmvCutscenes_h_
#define HAVE_STRUCT_TIMESPEC

#include "3dc.h"
#include "module.h"
#include "strategy_def.h"
#include "TextureManager.h"


typedef struct FMVTEXTURE
{
	texID_t    textureID;
	uint32_t   width;
	uint32_t   height;
	int32_t    fmvHandle;
	int        SoundVolume;
	int        IsTriggeredPlotFMV;
	bool       StaticImageDrawn;
	int        MessageNumber;
	uint8_t    *RGBBuffer;

} FMVTEXTURE;

// variables
extern uint32_t NumberOfFMVTextures;
#define MAX_NO_FMVTEXTURES 10

// How wide and tall a video screen's picture has to be. Its RGBBuffer is made for this size in SetupFMVTexture, and every picture the stock game has is this size.
const uint32_t kFMVTextureSize = 128;

extern FMVTEXTURE FMVTexture[MAX_NO_FMVTEXTURES];

// functions
void PlayMenuMusic(void);
extern void PlayFMV(const char *filenamePtr);

// Called once for each frame of a film that PlayFMV is showing, unless it's 0. A network game needs its
// connection looked after while a film plays, or the others decide this machine has gone.
// If it says false, the film stops, as if someone had pressed a key.
extern bool (*WhileAFilmPlays)(void);
void StartMenuMusic();
extern void StartTriggerPlotFMV(int number);
void UpdateAllFMVTextures();
void StartMenuBackgroundFmv();
int NextFMVTextureFrame(FMVTEXTURE *ftPtr);
void ReleaseAllFMVTexturesForDeviceReset();
void RecreateAllFMVTexturesAfterDeviceReset();
void SetupFMVTexture(FMVTEXTURE *ftPtr);

#endif // #ifndef _FMV_
