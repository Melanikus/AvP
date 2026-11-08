// Modified 2026-11-08 by Melanikus for the AvP port:
//   HUD_FollowTheScreen and HUDScaleFactor are here, for hud.cpp (HUD-SCALE-1). Reported by Bobblen; follows his avp-forever (commit a53f482), rewritten
#include "langenum.h"
#include "TextureManager.h"
extern void D3D_HUDQuad_Output(texID_t textureID, struct VertexTag *quadVerticesPtr, uint32_t colour, enum FILTERING_MODE_ID filteringType);

extern int TransparencyLevelOfHUD;
/* takes values of 0 (not drawn) to 255 (opaque) */


struct VertexTag
{
	int X;
	int Y;
	int U;
	int V;
};

typedef struct
{
	texID_t ImageNumber;

	int TopLeftX;
	int TopLeftY;
	
	int TopLeftU;
	int TopLeftV;

	int Width;
	int Height;

	fixed_t Scale;

	BOOL filterTexture;

	unsigned char Red;
	unsigned char Green;
	unsigned char Blue;
	unsigned char Translucency; /* 0 == invisible, 255 == opaque */

} HUDImageDesc;


typedef struct
{
	char Character;
	int X;
	int Y;

	unsigned char Red;
	unsigned char Green;
	unsigned char Blue;
	unsigned char Alpha;

} HUDCharDesc;

extern texID_t HUDImageNumber;
extern texID_t SpecialFXImageNumber;
extern texID_t SmokyImageNumber;
extern texID_t ChromeImageNumber;
extern texID_t CloudyImageNumber;
extern texID_t BurningImageNumber;
extern texID_t HUDFontsImageNumber;
extern texID_t PredatorVisionChangeImageNumber;
extern texID_t PredatorNumbersImageNumber;
extern texID_t StaticImageNumber;
extern texID_t AlienTongueImageNumber;
extern texID_t AAFontImageNumber;
extern texID_t WaterShaftImageNumber;

extern signed int HUDTranslucencyLevel;

extern void D3D_DrawHUDFontCharacter(HUDCharDesc *charDescPtr);
extern void D3D_DrawHUDDigit(HUDCharDesc *charDescPtr);
void D3D_BLTMotionTrackerBlipToHUD(int x, int y, int brightness);
void D3D_BLTGunSightToHUD(int screenX, int screenY, enum GUNSIGHT_SHAPE gunsightShape);
void D3D_DrawHUDPredatorDigit(HUDCharDesc *charDescPtr, int scale);
void D3D_BLTMotionTrackerToHUD(int scanLineSize);
void Draw_HUDImage(HUDImageDesc *imageDescPtr);
extern void D3D_BlitWhiteChar(int x, int y, unsigned char c);

// How big the HUD is drawn, in the game's fixed-point numbers, where 65536 means 1: HUD_FollowTheScreen sets it, for every species
extern fixed_t HUDScaleFactor;

// Works out how big the HUD is drawn, for the screen as it is now and the HUD size the player chose (see d3d_hud.cpp). A level's start asks
// with evenIfUnchanged true, and MaintainHUD asks with it false on every frame, which changes nothing unless the screen or the size has.
void HUD_FollowTheScreen(bool evenIfUnchanged);

/* KJL 17:55:48 18/04/98 - called by HUD.c */
extern void Render_HealthAndArmour(unsigned int health, unsigned int armour);
extern void Render_MarineAmmo(enum TEXTSTRING_ID ammoText, enum TEXTSTRING_ID magazinesText, unsigned int magazines, enum TEXTSTRING_ID roundsText, unsigned int rounds, int primaryAmmo);
