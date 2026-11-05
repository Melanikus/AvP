// AvP port by Barry Duncan.
// Modified 2026-11-03 by Melanikus for the AvP port:
//   a small menu letter is skipped when the font picture is missing or empty, instead of being drawn with
//   broken picture coordinates
// Modified 2026-11-04 by Melanikus for the AvP port:
//   with the surface pictures, a shadow's start depends on the polygon and the light, not on where the camera is
//   a still light and a still shape keep their shadow prisms on the graphics card, and the prisms are only worked out again if something moves
//   the same for a polygon's flat surface worked out each frame
//   walls drawn from the fixed buffer cast shadows from their shapes, with nothing on the caster list
//   with -keptsnap, a kept prism is finished each frame in the shader, from the same cut corners as the walls
//   particles and coronas are written into their lists before those lists are closed, not after. Every writer now
//   stops when its list says it has no room, and rain makes room the way the level's other shapes do. A frame that
//   couldn't start draws nothing, and an overlay in two parts goes whole or not at all
//   -shadowshoot: a key sends a ray through the crosshair, and the log says every shadow prism it goes through. This comes out before merge
// Modified 2026-11-05 by Melanikus for the AvP port:
//   the LIGHTING and SHADOWS console commands, with L and H as their keys under -dev. See view_switches.h. LIGHTING replaces G's FINAL WITHOUT RELIEF view

#include "console.h"
#include "OnScreenKeyboard.h"
#include "TextureManager.h"
#include "r2base.h"
#include "Fonts.h"
#include <vector>
#include <algorithm>
#include "logString.h"
#include "input.h"
#include "iofocus.h"
#include "RenderList.h"
#include "VertexBuffer.h"
#include "FmvCutscenes.h"
#include "surface_normals.h"
#include "surface_pictures.h"
#include "shadow_gap.h"
#include "static_scene.h"
#include "shadow_shoot.h"
#include "shadow_shoot_sums.h"
#define UseLocalAssert FALSE
#include "ourasert.h"
#include <assert.h>
#include "menus.h"
#include "user_profile.h"
#include "menu_gfx.hpp"
#include "io.h"
#include "pheromone.h"
#include "tables.h"
#include "particle.h"
#include "kshape.h"
#include "3dc.h"
#include "inline.h"
#include "gamedef.h"
#include "dxlog.h"
#include "d3d_hud.h"
#include "hud_layout.h"
#define HAVE_VISION_H 1
#include "lighting.h"
#include "showcmds.h"
#include "frustum.h"
#include "d3d_render.h"
#include "pixel_lights.h"
#include "shadow_reach.h"
#include "frame_timing.h"
#include "shadow_debug.h"
#include "texture_relief.h"
#include "view_switches.h"

// The commit this was built from, for the tag on the screen. The build script writes this file, and it isn't in the repository.
#if __has_include("build_commit.h")
#include "build_commit.h"
#else
#define AVP_BUILD_COMMIT "not known"
#endif
#include "static_frame.h"
#include "surface_static.h"
#include "shadow_volumes.h"
#include "shadow_volumes_card.h"
#include "bh_types.h"

bool frustumCull = false;
bool gunLayer = false;

#define FMV_ON 0
#define FMV_SIZE 128

AVPTEXTURE FMVTextureHandle[4];
AVPTEXTURE NoiseTextureHandle;

int WaterXOrigin;
int WaterZOrigin;
float WaterUScale;
float WaterVScale;

int MeshZScale;
int MeshXScale;
int WaterFallBase;

VECTORCH MeshWorldVertex[256];
uint32_t MeshVertexColour[256];

int WireFrameMode;

uint32_t FMVParticleColour;

const int kNumMeshVerts = 256;
VECTORCH MeshVertex[kNumMeshVerts];

extern void DrawParticles();
extern void DrawCoronas();
extern bool IsDemoVersion();

int NumberOfLandscapePolygons;

// set to 'null' texture initially
texID_t currentWaterTexture = NO_TEXTURE;

const float zNear = 64.0f;
const float zFar  = 1000000.0f;

// extern variables
extern D3DINFO    d3d;
extern SCREENDESCRIPTORBLOCK ScreenDescriptorBlock;
extern VIEWDESCRIPTORBLOCK* Global_VDB_Ptr;
extern float CameraZoomScale;
extern float p, o;
extern DISPLAYBLOCK *Player;
extern DISPLAYBLOCK *ActiveBlockList[];
extern DISPLAYBLOCK *OnScreenBlockList[];
extern int HUDScaleFactor;
extern int NumOnScreenBlocks;
extern char LevelName[];
extern int NumActiveBlocks;
extern unsigned char GammaValues[256];
extern int GlobalAmbience;
extern char CloakedPredatorIsMoving;

// extern functions
extern void BuildFrustum();
extern BOOL LevelHasStars;
extern void HandleRainShaft(MODULE *modulePtr, int bottomY, int topY, int numberOfRaindrops);
extern void RenderMirrorSurface(void);
extern void RenderMirrorSurface2(void);
extern void RenderParticlesInMirror(void);
extern void CheckForObjectsInWater(int minX, int maxX, int minZ, int maxZ, int averageY);
extern void HandleRain(int numberOfRaindrops);
extern void RenderSky(void);
extern void RenderStarfield(void);
extern void ScanImagesForFMVs();
extern bool CheckPointIsInFrustum(D3DXVECTOR3 *point);
extern bool CreateVolatileResources();
extern bool ReleaseVolatileResources();
void ColourFillBackBuffer(int FillColour);

int LightIntensityAtPoint(VECTORCH *pointPtr);
static float currentCameraZoomScale = 1.0f;

D3DLVERTEX *mainVertex = NULL;
uint16_t *mainIndex = NULL;

ORTHOVERTEX *orthoVertex = NULL;
uint16_t *orthoIndex = NULL;

PARTICLEVERTEX *particleVertex = NULL;
uint16_t *particleIndex = NULL;

DECAL_VERTEX *decalVertex = NULL;
uint16_t *decalIndex = NULL;

static uint32_t pVb = 0;
static uint32_t vb = 0;
static uint32_t decalVb = 0;
static uint32_t orthoVBOffset = 0;
static uint32_t NumberOfRenderedTriangles = 0;

RenderList *particleList = 0;
RenderList *mainList  = 0;
RenderList *orthoList = 0;
RenderList *decalList  = 0;
RenderList *weaponList = 0;

static HRESULT LastError; // remove me eventually (when no more D3D calls exist in this file)

// set our std::vectors capacity allowing us to use them as regular arrays
void RenderListInit()
{
	mainList     = new RenderList("mainList",     400, &d3d.mainVB,  &d3d.mainIB,  &d3d.mainDecl);
	orthoList    = new RenderList("orthoList",    50,  &d3d.orthoVB, &d3d.orthoIB, &d3d.orthoDecl);
	particleList = new RenderList("particleList", 200, &d3d.particleVB, &d3d.particleIB, &d3d.particleDecl);
	decalList    = new RenderList("decalList",    200, &d3d.decalVB, &d3d.decalIB, &d3d.decalDecl);
	weaponList   = new RenderList("weaponList",   80,  &d3d.mainVB,  &d3d.mainIB,  &d3d.mainDecl);
}

void RenderListDeInit()
{
	delete particleList;
	particleList = 0;

	delete mainList;
	mainList = 0;

	delete orthoList;
	orthoList = 0;

	delete decalList;
	decalList = 0;

	delete weaponList;
	weaponList = 0;
}

struct renderParticle
{
	PARTICLE		particle;
	PARTICLEVERTEX	vertices[4];
};

// particle buckets
std::vector<std::vector<renderParticle> > particleBucket;
bool pbInit = false;

struct renderCorona
{
	PARTICLE		particle;
	VECTORCHF		coronaPoint;
	uint32_t		numVerts;
	int				translucency;

	inline bool operator<(const renderCorona& rhs) const {return translucency < rhs.translucency;}
};

std::vector<renderParticle> particleArray;
std::vector<renderCorona>   coronaArray;

// temp
inline float FMV_WPos2DC(int32_t pos)
{
	return (float(pos / (float)640) * 2) - 1;
}
inline float FMV_HPos2DC(int32_t pos)
{
	return (float(pos / (float)480) * 2) - 1;
}

// convert a width pixel range value (eg 0-640) to device coordinate (eg -1.0 to 1.0)
inline float WPos2DC(int32_t pos)
{
//	if (mainMenu) {
	if ((GetAvPMenuState() != MENUSSTATE_STARTGAME)) {
		return (float(pos / (float)640) * 2) - 1;
	}
	else {
		return (float(pos / (float)ScreenDescriptorBlock.SDB_Width) * 2) - 1;
	}
}

// convert a width pixel range value (eg 0-480) to device coordinate (eg -1.0 to 1.0)
inline float HPos2DC(int32_t pos)
{
//	if (mainMenu) {
	if ((GetAvPMenuState() != MENUSSTATE_STARTGAME)) {
		return (float(pos / (float)480) * 2) - 1;
	}
	else {
		return (float(pos / (float)ScreenDescriptorBlock.SDB_Height)* 2) - 1;
	}
}

bool staticDecalsTest = false;

// lock our dynamic vertex and index buffers, and reset counters and array indexes used to keep 
// track of the number of verts and indices in those buffers. Function could be renamed as 
// we no longer use an execute buffer
static bool LockExecuteBuffer()
{
	ProcessorStageClock clock(kProcessorLocks);
	// While the game is getting the graphics card back, there are no buffers. The lists know what to do about that.
	if (!d3d.particleVB || !d3d.particleIB || !d3d.mainVB || !d3d.mainIB || !d3d.orthoVB || !d3d.orthoIB || !d3d.decalVB || !d3d.decalIB)
	{
		return false;
	}

	// lock particle vertex and index buffers
	d3d.particleVB->Lock((void**)&particleVertex);
	d3d.particleIB->Lock(&particleIndex);

	// lock main vertex and index buffers
	d3d.mainVB->Lock((void**)&mainVertex);
	SurfaceNormals_Lock();
	d3d.mainIB->Lock(&mainIndex);

	// lock ortho vertex and index buffers
	d3d.orthoVB->Lock((void**)&orthoVertex);
	d3d.orthoIB->Lock(&orthoIndex);

	d3d.decalVB->Lock((void**)&decalVertex);
	d3d.decalIB->Lock(&decalIndex);

#if 0
	if (!staticDecalsTest)
	{
		d3d.decalIB->Lock(&decalIndex);

		uint32_t vertexCount = 0;

		// generate indices
		for (int i = 0; i < (12288 * 2); i+=6)
		{
			decalIndex[i+0]   = 0 + vertexCount;//(vertexCount - (4) + (0));
			decalIndex[i+1] = 1 + vertexCount;//(vertexCount - (4) + (1));
			decalIndex[i+2] = 2 + vertexCount;//(vertexCount - (4) + (2));

//			vertexCount+=3;

			decalIndex[i+3] = 0 + vertexCount;//(vertexCount - (4) + (0));
			decalIndex[i+4] = 2 + vertexCount;//(vertexCount - (4) + (2));
			decalIndex[i+5] = 3 + vertexCount;//(vertexCount - (4) + (3));

			vertexCount+=4;
		}

//		staticDecalsTest = true;
	}
#endif

	// reset lists to empty state
	particleList->Reset();
	mainList->Reset();
	orthoList->Reset();
	decalList->Reset();
	weaponList->Reset();

	// reset counters and indexes
	vb = 0;
	decalVb = 0;
	orthoVBOffset = 0;
	pVb = 0;

    return true;
}

// unlock all dynamic vertex and index buffers. function could be renamed as no longer using execute buffers
static bool UnlockExecuteBufferAndPrepareForUse()
{
	// Particles and coronas are collected during the frame, and only now are their corners written into the lists.
	// They have to be written here, before anything below closes those lists. Writing to a closed list means writing
	// to memory the graphics card's driver has taken back, and the driver may be using that memory for something else.
	// The two sorts stay ahead of the writing, in the order they've always had, so everything draws in the same order.
	// This time still counts as drawing on the frame report, where it always has.
	{
		ProcessorStageClock writing(kProcessorDrawing);

		particleList->Sort();
		decalList->Sort();
//		mainList->Sort();

		DrawParticles();
		DrawCoronas();
	}

	ProcessorStageClock clock(kProcessorLocks);

	if (!d3d.particleVB || !d3d.particleIB || !d3d.mainVB || !d3d.mainIB || !d3d.orthoVB || !d3d.orthoIB || !d3d.decalVB || !d3d.decalIB)
	{
		return false;
	}

	// unlock particle vertex and index buffers
	d3d.particleVB->Unlock();
	d3d.particleIB->Unlock();

	// unlock main vertex and index buffers
	d3d.mainVB->Unlock();
	SurfaceNormals_Unlock();
	d3d.mainIB->Unlock();

	// unlock ortho vertex and index buffers
	d3d.orthoVB->Unlock();
	d3d.orthoIB->Unlock();

//	if (!staticDecalsTest)
	{
		d3d.decalIB->Unlock();
//		staticDecalsTest = true;
	}

	d3d.decalVB->Unlock();

	return true;
}

// True between the start of a frame and its end, if the frame could start. The graphics card can refuse to start one, and the game carries on running anyway.
static bool frameIsOpen = false;

static bool ExecuteBuffer();

// Draws everything waiting in the lists and starts them again, for when a list is full in the middle of a frame. If the frame couldn't start, there's nothing to draw.
static void FlushLists()
{
	if (!frameIsOpen)
	{
		return;
	}

	UnlockExecuteBufferAndPrepareForUse();
	ExecuteBuffer();
	LockExecuteBuffer();
}

float cot(float in)
{
	return 1.0f / tan(in);
}

static bool toyFinalFlush = false;
static int toyEarlyFlushes = 0;     // how often the lists were drawn before the end of a frame, since the last report
static bool earlyFlushThisFrame = false;
static void ToyShadows_Draw();
static void ViewSwitches_ApplyAsked();
static void ShadowStatus_Draw();
extern int ToyShadowsMode;
void ToyShadows_NoteFrameLights(int frame);
static const int kFramesBetweenReports = 300;   // how often the shadow pass writes its report to the log
static int toyFrame = 0;            // counts the shadow pass's frames. Both reports use it, so what they say belongs together

static bool ExecuteBuffer()
{
	ProcessorStageClock clock(kProcessorDrawing);

	FrameTiming_CountMainVertices(vb);

	if (!toyFinalFlush)
	{
		toyEarlyFlushes++;
		earlyFlushThisFrame = true;
	}

	// With shadows on, a frame's last drawing goes in the order solid polygons, decals, shadows, see-through polygons. Glass has to come
	// after the shadows, or they'd darken it as if it were part of the wall behind.
	bool const seeThroughLast = toyFinalFlush && ToyShadowsMode != 0;

	// The walls from the fixed buffer go once, after the solid polygons, whichever drawing of the frame gets there first. See static_scene.cpp.
	bool const wallsAreDue = StaticScene_WallsAreDue();

	if (mainList->GetSize())
	{
		// set main shaders to active
		d3d.effectSystem->SetActive(d3d.mainEffect);

		// we don't need world matrix here as avp has done all the world transforms itself
		R_MATRIX matWorldViewProj = /*d3d.matView **/ d3d.matProjection;

		d3d.effectSystem->SetVertexShaderConstant(d3d.mainEffect, 0, CONST_MATRIX, &matWorldViewProj);

		// draw our main list (level geometry, player weapon etc)
		mainList->Draw((seeThroughLast || wallsAreDue) ? RenderList::kSolid : RenderList::kEverything);
	}

	if (wallsAreDue)
	{
		StaticScene_DrawWalls(d3d.lpD3DDevice);

		if (!seeThroughLast && mainList->GetSize())
		{
			d3d.effectSystem->SetActive(d3d.mainEffect);
			d3d.effectSystem->SetVertexShaderConstant(d3d.mainEffect, 0, CONST_MATRIX, &d3d.matProjection);

			mainList->Draw(RenderList::kSeeThrough);
		}
	}

	// render any decals
	if (decalList->GetSize())
	{
		// set decal shaders as active
		d3d.effectSystem->SetActive(d3d.decalEffect);

		R_MATRIX matWorldViewProj = /*d3d.matView **/ d3d.matProjection;

		// pass the projection matrix to the vertex shader
		d3d.effectSystem->SetVertexShaderConstant(d3d.decalEffect, 0, CONST_MATRIX, &matWorldViewProj);

		ChangeZWriteEnable(ZWRITE_DISABLED);

		// daw the ortho list
		decalList->Draw();

		ChangeZWriteEnable(ZWRITE_ENABLED);
	}

	if (toyFinalFlush)
	{
		FrameTiming_Mark(kStageScene);

		ProcessorStageClock shadowClock(kProcessorShadowPass);
		ToyShadows_Draw();
		ViewSwitches_ApplyAsked();
	}

	if (seeThroughLast && mainList->GetSize())
	{
		d3d.effectSystem->SetActive(d3d.mainEffect);
		d3d.effectSystem->SetVertexShaderConstant(d3d.mainEffect, 0, CONST_MATRIX, &d3d.matProjection);

		mainList->Draw(RenderList::kSeeThrough);
	}

	// render any particles
	if (particleList->GetSize())
	{
		// set main shaders to active
		d3d.effectSystem->SetActive(d3d.particleEffect);

		d3d.effectSystem->SetVertexShaderConstant(d3d.particleEffect, 0, CONST_MATRIX, &d3d.matProjection);

		// Draw the particles in the list
		particleList->Draw();
	}

	// draw player weapon
	if (weaponList->GetSize())
	{
		// set main shaders to active
		d3d.effectSystem->SetActive(d3d.mainEffect);

		R_MATRIX matWorldViewProj;

		// bjd - 15/01/2013 - AvP would adjust the projection to a 'wide' projection before rendering the Aliens
		// claws and tail. This is a hacky way to do that with the new rendering system
		if (AvP.PlayerType == I_Alien) {
			matWorldViewProj = /*d3d.matView **/ d3d.matAlienWeaponProjection;
		} else {
			matWorldViewProj = /*d3d.matView **/ d3d.matProjection;
		}

		d3d.effectSystem->SetVertexShaderConstant(d3d.mainEffect, 0, CONST_MATRIX, &matWorldViewProj);

		R_ClearZBuffer();

		// draw our main list (level geometry, player weapon etc)
		weaponList->Draw();
	}

	// render any orthographic quads
	if (orthoList->GetSize())
	{
		// set orthographic projection shaders as active
		d3d.effectSystem->SetActive(d3d.orthoEffect);

		// pass the orthographic projection matrix to the vertex shader
		d3d.effectSystem->SetVertexShaderConstant(d3d.orthoEffect, 0, CONST_MATRIX, &d3d.matOrtho);

		// daw the ortho list
		orthoList->Draw();
	}

	return true;
}

void DrawFmvFrame(uint32_t frameWidth, uint32_t frameHeight, const std::vector<texID_t> &textureIDs)
{
#ifdef _XBOX
	return;
#endif

	// offset the video vertically in the centre of the screen
	uint32_t topX, topY;

//	uint32_t wScale = ScreenDescriptorBlock.SDB_Width / 640;
//	uint32_t yScale = ScreenDescriptorBlock.SDB_Height / 480;
	/*
	if (mainMenu) {
		topX = (640 - frameWidth) / 2;
		topY = (480 - frameHeight) / 2;
	} else {
		topX = (ScreenDescriptorBlock.SDB_Width  - frameWidth) / 2;
		topY = (ScreenDescriptorBlock.SDB_Height - frameHeight) / 2;
	}

	float x1 = WPos2DC(topX);
	float y1 = HPos2DC(topY);

	float x2 = WPos2DC(topX + frameWidth);
	float y2 = HPos2DC(topY + frameHeight);
	*/

//	frameWidth *= wScale;
//	frameHeight *= yScale;

	topX = (/*ScreenDescriptorBlock.SDB_Width*/640  - frameWidth) / 2;
	topY = (/*ScreenDescriptorBlock.SDB_Height*/480 - frameHeight) / 2;

	float x1 = FMV_WPos2DC(topX);
	float y1 = FMV_HPos2DC(topY);

	float x2 = FMV_WPos2DC(topX + frameWidth);
	float y2 = FMV_HPos2DC(topY + frameHeight);

	// for non power of 2 texture UV adjustment - get top two textures (tex 2 and 3 should be the same resolution)
	Texture tempTexture  = Tex_GetTextureDetails(textureIDs[0]);
	Texture tempTexture2 = Tex_GetTextureDetails(textureIDs[1]);

	FMVVERTEX fmvVerts[4];

	// bottom left
	fmvVerts[0].x = x1;
	fmvVerts[0].y = y2;
	fmvVerts[0].z = 1.0f;
	fmvVerts[0].u1 = 0.0f;
	fmvVerts[0].v1 = (1.0f / tempTexture.realHeight) * tempTexture.height;

	fmvVerts[0].u2 = 0.0f;
	fmvVerts[0].v2 = (1.0f / tempTexture2.realHeight) * tempTexture2.height;

	fmvVerts[0].u3 = 0.0f;
	fmvVerts[0].v3 = (1.0f / tempTexture2.realHeight) * tempTexture2.height;

	// top left
	fmvVerts[1].x = x1;
	fmvVerts[1].y = y1;
	fmvVerts[1].z = 1.0f;
	fmvVerts[1].u1 = 0.0f;
	fmvVerts[1].v1 = 0.0f;

	fmvVerts[1].u2 = 0.0f;
	fmvVerts[1].v2 = 0.0f;

	fmvVerts[1].u3 = 0.0f;
	fmvVerts[1].v3 = 0.0f;

	// bottom right
	fmvVerts[2].x = x2;
	fmvVerts[2].y = y2;
	fmvVerts[2].z = 1.0f;
	fmvVerts[2].u1 = (1.0f / tempTexture.realWidth)  * tempTexture.width;
	fmvVerts[2].v1 = (1.0f / tempTexture.realHeight) * tempTexture.height;

	fmvVerts[2].u2 = (1.0f / tempTexture2.realWidth)  * tempTexture2.width;
	fmvVerts[2].v2 = (1.0f / tempTexture2.realHeight) * tempTexture2.height;

	fmvVerts[2].u3 = (1.0f / tempTexture2.realWidth)  * tempTexture2.width;
	fmvVerts[2].v3 = (1.0f / tempTexture2.realHeight) * tempTexture2.height;

	// top right
	fmvVerts[3].x = x2;
	fmvVerts[3].y = y1;
	fmvVerts[3].z = 1.0f;
	fmvVerts[3].u1 = (1.0f / tempTexture.realWidth) * tempTexture.width;
	fmvVerts[3].v1 = 0.0f;

	fmvVerts[3].u2 = (1.0f / tempTexture2.realWidth) * tempTexture2.width;
	fmvVerts[3].v2 = 0.0f;

	fmvVerts[3].u3 = (1.0f / tempTexture2.realWidth) * tempTexture2.width;
	fmvVerts[3].v3 = 0.0f;

	d3d.fmvDecl->Set();

	// set the YUV->RGB FMV shader
	d3d.effectSystem->SetActive(d3d.fmvEffect);

	// set orthographic projection
	d3d.effectSystem->SetVertexShaderConstant(d3d.fmvEffect, 0, CONST_MATRIX, &d3d.matOrtho);

	// set the texture
	for (size_t i = 0; i < textureIDs.size(); i++)
	{
		R_SetTexture(i, textureIDs[i]);
		ChangeTextureAddressMode(i, TEXTURE_CLAMP);
		ChangeFilteringMode(i, FILTERING_BILINEAR_OFF);
	}

	ChangeTranslucencyMode(TRANSLUCENCY_OFF);

	LastError = d3d.lpD3DDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, &fmvVerts[0], sizeof(FMVVERTEX));
	if (FAILED(LastError))
	{
		LogDxError(LastError, __LINE__, __FILE__);
		OutputDebugString("DrawPrimitiveUP failed\n");
	}
}

void DrawProgressBar(const RECT &srcRect, const RECT &destRect, texID_t textureID)
{
	float x1 = WPos2DC(destRect.left);
	float y1 = HPos2DC(destRect.top);

	float x2 = WPos2DC(destRect.right);
	float y2 = HPos2DC(destRect.bottom);

	Texture thisTexture = Tex_GetTextureDetails(textureID);

	float RecipW = 1.0f / thisTexture.realWidth;
	float RecipH = 1.0f / thisTexture.realHeight;

	if (!orthoList->AddItem(4, textureID, TRANSLUCENCY_OFF, FILTERING_BILINEAR_ON, TEXTURE_CLAMP)) {
		return;
	}

	RCOLOR colour = RCOLOR_XRGB(255, 255, 255);

	// bottom left
	orthoVertex[orthoVBOffset].x = x1;
	orthoVertex[orthoVBOffset].y = y2;
	orthoVertex[orthoVBOffset].z = 1.0f;
	orthoVertex[orthoVBOffset].colour = colour;
	orthoVertex[orthoVBOffset].u = (float)((thisTexture.width - srcRect.left) * RecipW);
	orthoVertex[orthoVBOffset].v = (float)((thisTexture.height - srcRect.bottom) * RecipH);
	orthoVBOffset++;

	// top left
	orthoVertex[orthoVBOffset].x = x1;
	orthoVertex[orthoVBOffset].y = y1;
	orthoVertex[orthoVBOffset].z = 1.0f;
	orthoVertex[orthoVBOffset].colour = colour;
	orthoVertex[orthoVBOffset].u = (float)((thisTexture.width - srcRect.left) * RecipW);
	orthoVertex[orthoVBOffset].v = (float)((thisTexture.height - srcRect.top) * RecipH);
	orthoVBOffset++;

	// bottom right
	orthoVertex[orthoVBOffset].x = x2;
	orthoVertex[orthoVBOffset].y = y2;
	orthoVertex[orthoVBOffset].z = 1.0f;
	orthoVertex[orthoVBOffset].colour = colour;
	orthoVertex[orthoVBOffset].u = (float)((thisTexture.width - srcRect.right) * RecipW);
	orthoVertex[orthoVBOffset].v = (float)((thisTexture.height - srcRect.bottom) * RecipH);
	orthoVBOffset++;

	// top right
	orthoVertex[orthoVBOffset].x = x2;
	orthoVertex[orthoVBOffset].y = y1;
	orthoVertex[orthoVBOffset].z = 1.0f;
	orthoVertex[orthoVBOffset].colour = colour;
	orthoVertex[orthoVBOffset].u = (float)((thisTexture.width - srcRect.right) * RecipW);
	orthoVertex[orthoVBOffset].v = (float)((thisTexture.height - srcRect.top) * RecipH);
	orthoVBOffset++;

	orthoList->CreateOrthoIndices(orthoIndex);
}

void DrawTallFontCharacter(uint32_t topX, uint32_t topY, texID_t textureID, uint32_t texU, uint32_t texV, uint32_t charWidth, uint32_t charHeight, uint32_t alpha)
{
	alpha = (alpha / 256);
	if (alpha > 255) {
		alpha = 255;
	}

	RCOLOR colour = RCOLOR_ARGB(alpha, 255, 255, 255);

	Texture tempTexture = Tex_GetTextureDetails(textureID);

	float RecipW = 1.0f / tempTexture.realWidth;
	float RecipH = 1.0f / tempTexture.realHeight;

	float x1 = WPos2DC(topX);
	float y1 = HPos2DC(topY);

	float x2 = WPos2DC(topX + charWidth);
	float y2 = HPos2DC(topY + charHeight);

	if ((/*d3d.supportsShaders*/1 ) && (!IsDemoVersion()))
	{
		R_SetTexture(0, textureID);
		R_SetTexture(1, AVPMENUGFX_CLOUDY);

		// set vertex declaration
		d3d.tallTextDecl->Set();

		// set orthographic projection shaders as active
		d3d.effectSystem->SetActive(d3d.tallTextEffect);

		// we need to pass these values as floats
		float CloakingPhaseF = static_cast<float>(CloakingPhase);
		float topXF = static_cast<float>(topX);

		d3d.effectSystem->SetVertexShaderConstant(d3d.tallTextEffect, 0, CONST_FLOAT,  &CloakingPhaseF);
		d3d.effectSystem->SetVertexShaderConstant(d3d.tallTextEffect, 1, CONST_MATRIX, &d3d.matOrtho);
		d3d.effectSystem->SetVertexShaderConstant(d3d.tallTextEffect, 2, CONST_FLOAT,  &topXF);

		ChangeTextureAddressMode(0, TEXTURE_CLAMP);
		ChangeTextureAddressMode(1, TEXTURE_WRAP);
		ChangeTranslucencyMode(TRANSLUCENCY_GLOWING);
		ChangeFilteringMode(0, FILTERING_BILINEAR_ON);
		ChangeFilteringMode(1, FILTERING_BILINEAR_ON);

		TALLFONT textQuad[4];

		// bottom left
		textQuad[0].x = x1;
		textQuad[0].y = y2;
		textQuad[0].z = 1.0f;
		textQuad[0].colour = colour;
		textQuad[0].u1 = (float)((texU) * RecipW);
		textQuad[0].v1 = (float)((texV + charHeight) * RecipH);

		textQuad[0].u2 = 0.0f;
		textQuad[0].v2 = 1.0f;

		// top left
		textQuad[1].x = x1;
		textQuad[1].y = y1;
		textQuad[1].z = 1.0f;
		textQuad[1].colour = colour;
		textQuad[1].u1 = (float)((texU) * RecipW);
		textQuad[1].v1 = (float)((texV) * RecipH);

		textQuad[1].u2 = 0.0f;
		textQuad[1].v2 = 0.0f;

		// bottom right
		textQuad[2].x = x2;
		textQuad[2].y = y2;
		textQuad[2].z = 1.0f;
		textQuad[2].colour = colour;
		textQuad[2].u1 = (float)((texU + charWidth) * RecipW);
		textQuad[2].v1 = (float)((texV + charHeight) * RecipH);

		textQuad[2].u2 = 1.0f;
		textQuad[2].v2 = 1.0f;

		// top right
		textQuad[3].x = x2;
		textQuad[3].y = y1;
		textQuad[3].z = 1.0f;
		textQuad[3].colour = colour;
		textQuad[3].u1 = (float)((texU + charWidth) * RecipW);
		textQuad[3].v1 = (float)((texV) * RecipH);

		textQuad[3].u2 = 1.0f;
		textQuad[3].v2 = 0.0f;

		LastError = d3d.lpD3DDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, textQuad, sizeof(TALLFONT));
		if (FAILED(LastError)) 
		{
			OutputDebugString("DrawTallFontCharacter quad draw failed\n");
		}
	}
	else
	{
		if (!orthoList->AddItem(4, textureID, TRANSLUCENCY_GLOWING, FILTERING_BILINEAR_ON, TEXTURE_CLAMP)) {
			return;
		}

		// bottom left
		orthoVertex[orthoVBOffset].x = x1;
		orthoVertex[orthoVBOffset].y = y2;
		orthoVertex[orthoVBOffset].z = 1.0f;
		orthoVertex[orthoVBOffset].colour = colour;
		orthoVertex[orthoVBOffset].u = (float)((texU) * RecipW);
		orthoVertex[orthoVBOffset].v = (float)((texV + charHeight) * RecipH);
		orthoVBOffset++;

		// top left
		orthoVertex[orthoVBOffset].x = x1;
		orthoVertex[orthoVBOffset].y = y1;
		orthoVertex[orthoVBOffset].z = 1.0f;
		orthoVertex[orthoVBOffset].colour = colour;
		orthoVertex[orthoVBOffset].u = (float)((texU) * RecipW);
		orthoVertex[orthoVBOffset].v = (float)((texV) * RecipH);
		orthoVBOffset++;

		// bottom right
		orthoVertex[orthoVBOffset].x = x2;
		orthoVertex[orthoVBOffset].y = y2;
		orthoVertex[orthoVBOffset].z = 1.0f;
		orthoVertex[orthoVBOffset].colour = colour;
		orthoVertex[orthoVBOffset].u = (float)((texU + charWidth) * RecipW);
		orthoVertex[orthoVBOffset].v = (float)((texV + charHeight) * RecipH);
		orthoVBOffset++;

		// top right
		orthoVertex[orthoVBOffset].x = x2;
		orthoVertex[orthoVBOffset].y = y1;
		orthoVertex[orthoVBOffset].z = 1.0f;
		orthoVertex[orthoVBOffset].colour = colour;
		orthoVertex[orthoVBOffset].u = (float)((texU + charWidth) * RecipW);
		orthoVertex[orthoVBOffset].v = (float)((texV) * RecipH);
		orthoVBOffset++;

		orthoList->CreateOrthoIndices(orthoIndex);
	}
}

void SetupFMVTexture(FMVTEXTURE *ftPtr)
{
	ftPtr->RGBBuffer = new uint8_t[128 * 128 * sizeof(uint32_t)];
	ftPtr->SoundVolume = 0;
}

void UpdateFMVTexture(FMVTEXTURE *ftPtr)
{
	assert(ftPtr);

	if (!ftPtr) {
		return;
	}

	if (!NextFMVTextureFrame(ftPtr)) {
	 	return;
	}

	uint32_t width = 0;
	uint32_t height = 0;
	Tex_GetDimensions(ftPtr->textureID, width, height);

	uint8_t *srcPtr = &ftPtr->RGBBuffer[0];
	uint8_t *destPtr = 0;
	uint8_t *originalPtr = 0;
	uint32_t pitch = 0;

	if (!Tex_Lock(ftPtr->textureID, &originalPtr, &pitch, TextureLock_Discard))
	{
		// don't touch the pointer if the lock failed
		return;
	}

	for (uint32_t y = 0; y < height; y++)
	{
		destPtr = originalPtr + y * pitch;

		for (uint32_t x = 0; x < width; x++)
		{
			memcpy(destPtr, srcPtr, sizeof(uint32_t));

			destPtr += sizeof(uint32_t);
			srcPtr  += sizeof(uint32_t);
		}
	}

	Tex_Unlock(ftPtr->textureID);
}

void DrawFadeQuad(uint32_t topX, uint32_t topY, uint32_t alpha)
{
	alpha = alpha / 256;
	if (alpha > 255) {
		alpha = 255;
	}

	RCOLOR colour = RCOLOR_ARGB(alpha,0,0,0);

	if (!orthoList->AddItem(4, NO_TEXTURE, TRANSLUCENCY_GLOWING, FILTERING_BILINEAR_ON, TEXTURE_WRAP)) {
		return;
	}

	// bottom left
	orthoVertex[orthoVBOffset].x = -1.0f;
	orthoVertex[orthoVBOffset].y = 1.0f;
	orthoVertex[orthoVBOffset].z = 1.0f;
	orthoVertex[orthoVBOffset].colour = colour;
	orthoVertex[orthoVBOffset].u = 0.0f;
	orthoVertex[orthoVBOffset].v = 0.0f;
	orthoVBOffset++;

	// top left
	orthoVertex[orthoVBOffset].x = -1.0f;
	orthoVertex[orthoVBOffset].y = -1.0f;
	orthoVertex[orthoVBOffset].z = 1.0f;
	orthoVertex[orthoVBOffset].colour = colour;
	orthoVertex[orthoVBOffset].u = 0.0f;
	orthoVertex[orthoVBOffset].v = 0.0f;
	orthoVBOffset++;

	// bottom right
	orthoVertex[orthoVBOffset].x = 1.0f;
	orthoVertex[orthoVBOffset].y = 1.0f;
	orthoVertex[orthoVBOffset].z = 1.0f;
	orthoVertex[orthoVBOffset].colour = colour;
	orthoVertex[orthoVBOffset].u = 0.0f;
	orthoVertex[orthoVBOffset].v = 0.0f;
	orthoVBOffset++;

	// top right
	orthoVertex[orthoVBOffset].x = 1.0f;;
	orthoVertex[orthoVBOffset].y = -1.0f;
	orthoVertex[orthoVBOffset].z = 1.0f;
	orthoVertex[orthoVBOffset].colour = colour;
	orthoVertex[orthoVBOffset].u = 0.0f;
	orthoVertex[orthoVBOffset].v = 0.0f;
	orthoVBOffset++;

	orthoList->CreateOrthoIndices(orthoIndex);
}

void DrawFontQuad(uint32_t x, uint32_t y, uint32_t charWidth, uint32_t charHeight, texID_t textureID, float *uvArray, uint32_t colour, enum TRANSLUCENCY_TYPE translucencyType)
{
	float x1 = WPos2DC(x);
	float y1 = HPos2DC(y);

	float x2 = WPos2DC(x + charWidth);
	float y2 = HPos2DC(y + charHeight);

	if (!orthoList->AddItem(4, textureID, translucencyType, FILTERING_BILINEAR_OFF, TEXTURE_CLAMP)) {
		return;
	}

	// bottom left
	orthoVertex[orthoVBOffset].x = x1;
	orthoVertex[orthoVBOffset].y = y2;
	orthoVertex[orthoVBOffset].z = 1.0f;
	orthoVertex[orthoVBOffset].colour = colour;
	orthoVertex[orthoVBOffset].u = uvArray[0];
	orthoVertex[orthoVBOffset].v = uvArray[1];
	orthoVBOffset++;

	// top left
	orthoVertex[orthoVBOffset].x = x1;
	orthoVertex[orthoVBOffset].y = y1;
	orthoVertex[orthoVBOffset].z = 1.0f;
	orthoVertex[orthoVBOffset].colour = colour;
	orthoVertex[orthoVBOffset].u = uvArray[2];
	orthoVertex[orthoVBOffset].v = uvArray[3];
	orthoVBOffset++;

	// bottom right
	orthoVertex[orthoVBOffset].x = x2;
	orthoVertex[orthoVBOffset].y = y2;
	orthoVertex[orthoVBOffset].z = 1.0f;
	orthoVertex[orthoVBOffset].colour = colour;
	orthoVertex[orthoVBOffset].u = uvArray[4];
	orthoVertex[orthoVBOffset].v = uvArray[5];
	orthoVBOffset++;

	// top right
	orthoVertex[orthoVBOffset].x = x2;
	orthoVertex[orthoVBOffset].y = y1;
	orthoVertex[orthoVBOffset].z = 1.0f;
	orthoVertex[orthoVBOffset].colour = colour;
	orthoVertex[orthoVBOffset].u = uvArray[6];
	orthoVertex[orthoVBOffset].v = uvArray[7];
	orthoVBOffset++;

	orthoList->CreateOrthoIndices(orthoIndex);
}

void DrawQuad(uint32_t x, uint32_t y, uint32_t width, uint32_t height, texID_t textureID, uint32_t colour, enum TRANSLUCENCY_TYPE translucencyType)
{
	float x1 = WPos2DC(x);
	float y1 = HPos2DC(y);

	float x2 = WPos2DC(x + width);
	float y2 = HPos2DC(y + height);

	Texture tempTexture = Tex_GetTextureDetails(textureID);

	if (!orthoList->AddItem(4, textureID, translucencyType, FILTERING_BILINEAR_ON, TEXTURE_CLAMP)) {
		return;
	}

	// bottom left
	orthoVertex[orthoVBOffset].x = x1;
	orthoVertex[orthoVBOffset].y = y2;
	orthoVertex[orthoVBOffset].z = 1.0f;
	orthoVertex[orthoVBOffset].colour = colour;
	orthoVertex[orthoVBOffset].u = 0.0f;
	orthoVertex[orthoVBOffset].v = (1.0f / tempTexture.realHeight) * tempTexture.height;
	orthoVBOffset++;

	// top left
	orthoVertex[orthoVBOffset].x = x1;
	orthoVertex[orthoVBOffset].y = y1;
	orthoVertex[orthoVBOffset].z = 1.0f;
	orthoVertex[orthoVBOffset].colour = colour;
	orthoVertex[orthoVBOffset].u = 0.0f;
	orthoVertex[orthoVBOffset].v = 0.0f;
	orthoVBOffset++;

	// bottom right
	orthoVertex[orthoVBOffset].x = x2;
	orthoVertex[orthoVBOffset].y = y2;
	orthoVertex[orthoVBOffset].z = 1.0f;
	orthoVertex[orthoVBOffset].colour = colour;
	orthoVertex[orthoVBOffset].u = (1.0f / tempTexture.realWidth) * tempTexture.width;
	orthoVertex[orthoVBOffset].v = (1.0f / tempTexture.realHeight) * tempTexture.height;
	orthoVBOffset++;

	// top right
	orthoVertex[orthoVBOffset].x = x2;
	orthoVertex[orthoVBOffset].y = y1;
	orthoVertex[orthoVBOffset].z = 1.0f;
	orthoVertex[orthoVBOffset].colour = colour;
	orthoVertex[orthoVBOffset].u = (1.0f / tempTexture.realWidth) * tempTexture.width;
	orthoVertex[orthoVBOffset].v = 0.0f;
	orthoVBOffset++;

	orthoList->CreateOrthoIndices(orthoIndex);
}

void DrawAlphaMenuQuad(uint32_t topX, uint32_t topY, texID_t textureID, uint32_t alpha)
{
	// textures actual height/width (whether it's non power of two or not)
	uint32_t textureWidth, textureHeight;
	Tex_GetDimensions(textureID, textureWidth, textureHeight);

	alpha = (alpha / 256);
	if (alpha > 255) {
		alpha = 255;
	}

	DrawQuad(topX, topY, textureWidth, textureHeight, textureID, RCOLOR_ARGB(alpha, 255, 255, 255), TRANSLUCENCY_GLOWING);
}

void DrawMenuTextGlow(uint32_t topLeftX, uint32_t topLeftY, uint32_t size, uint32_t alpha)
{
	uint32_t textureWidth  = 0;
	uint32_t textureHeight = 0;

	if (alpha > ONE_FIXED) { // ONE_FIXED = 65536
		alpha = ONE_FIXED;
	}

	alpha = (alpha / 256);
	if (alpha > 255) {
		alpha = 255;
	}

	// textures original resolution (if it's a non power of 2, these will be the non power of 2 values)
	Tex_GetDimensions(AVPMENUGFX_GLOWY_LEFT, textureWidth, textureHeight);

	// do the text alignment justification
	topLeftX -= textureWidth;

	DrawQuad(topLeftX, topLeftY, textureWidth, textureHeight, AVPMENUGFX_GLOWY_LEFT, RCOLOR_ARGB(alpha, 255, 255, 255), TRANSLUCENCY_GLOWING);

	// now do the middle section
	topLeftX += textureWidth;

	Tex_GetDimensions(AVPMENUGFX_GLOWY_MIDDLE, textureWidth, textureHeight);

	DrawQuad(topLeftX, topLeftY, textureWidth * size, textureHeight, AVPMENUGFX_GLOWY_MIDDLE, RCOLOR_ARGB(alpha, 255, 255, 255), TRANSLUCENCY_GLOWING);

	// now do the right section
	topLeftX += textureWidth * size;

	Tex_GetDimensions(AVPMENUGFX_GLOWY_RIGHT, textureWidth, textureHeight);

	DrawQuad(topLeftX, topLeftY, textureWidth, textureHeight, AVPMENUGFX_GLOWY_RIGHT, RCOLOR_ARGB(alpha, 255, 255, 255), TRANSLUCENCY_GLOWING);
}

void DrawSmallMenuCharacter(uint32_t topX, uint32_t topY, uint32_t texU, uint32_t texV, uint32_t red, uint32_t green, uint32_t blue, uint32_t alpha)
{
	alpha = (alpha / 256);
	red   = (red   / 256);
	green = (green / 256);
	blue  = (blue  / 256);

	// clamp if needed
	if (alpha > 255) alpha = 255;
	if (red   > 255) red   = 255;
	if (green > 255) green = 255;
	if (blue  > 255) blue  = 255;

	RCOLOR colour = RCOLOR_ARGB(alpha, 255, 255, 255);

	uint32_t font_height = HUD_FONT_HEIGHT;
	uint32_t font_width  = HUD_FONT_WIDTH;

	// aa_font.bmp is 256 x 256
	uint32_t textureWidth  = 0;
	uint32_t textureHeight = 0;

	Tex_GetDimensions(AAFontImageNumber, textureWidth, textureHeight);

	// the font texture is missing or empty: draw nothing rather than inf/NaN texture coordinates
	if (textureWidth == 0 || textureHeight == 0) {
		return;
	}

	float RecipW = 1.0f / textureWidth; // 0.00390625
	float RecipH = 1.0f / textureHeight;

	float x1 = WPos2DC(topX);
	float y1 = HPos2DC(topY);

	float x2 = WPos2DC(topX + font_width);
	float y2 = HPos2DC(topY + font_height);

	if (!orthoList->AddItem(4, AAFontImageNumber, TRANSLUCENCY_GLOWING, FILTERING_BILINEAR_ON, TEXTURE_CLAMP)) {
		return;
	}

	// bottom left
	orthoVertex[orthoVBOffset].x = x1;
	orthoVertex[orthoVBOffset].y = y2;
	orthoVertex[orthoVBOffset].z = 1.0f;
	orthoVertex[orthoVBOffset].colour = colour;
	orthoVertex[orthoVBOffset].u = (float)((texU) * RecipW);
	orthoVertex[orthoVBOffset].v = (float)((texV + font_height) * RecipH);
	orthoVBOffset++;

	// top left
	orthoVertex[orthoVBOffset].x = x1;
	orthoVertex[orthoVBOffset].y = y1;
	orthoVertex[orthoVBOffset].z = 1.0f;
	orthoVertex[orthoVBOffset].colour = colour;
	orthoVertex[orthoVBOffset].u = (float)((texU) * RecipW);
	orthoVertex[orthoVBOffset].v = (float)((texV) * RecipH);
	orthoVBOffset++;

	// bottom right
	orthoVertex[orthoVBOffset].x = x2;
	orthoVertex[orthoVBOffset].y = y2;
	orthoVertex[orthoVBOffset].z = 1.0f;
	orthoVertex[orthoVBOffset].colour = colour;
	orthoVertex[orthoVBOffset].u = (float)((texU + font_height) * RecipW);
	orthoVertex[orthoVBOffset].v = (float)((texV + font_height) * RecipH);
	orthoVBOffset++;

	// top right
	orthoVertex[orthoVBOffset].x = x2;
	orthoVertex[orthoVBOffset].y = y1;
	orthoVertex[orthoVBOffset].z = 1.0f;
	orthoVertex[orthoVBOffset].colour = colour;
	orthoVertex[orthoVBOffset].u = (float)((texU + font_width) * RecipW);
	orthoVertex[orthoVBOffset].v = (float)((texV) * RecipH);
	orthoVBOffset++;

	orthoList->CreateOrthoIndices(orthoIndex);
}

void TransformToViewspace(VECTORCHF *vector)
{
	D3DXVECTOR3 output;
	D3DXVECTOR3 input(vector->vx, vector->vy, vector->vz);

	D3DXVec3TransformCoord(&output, &input, &d3d.matView);

	vector->vx = output.x;
	vector->vy = output.y;
	vector->vz = output.z;
}

void UpdateProjectionMatrix()
{
	// manually generate the perspective matrix as per http://msdn.microsoft.com/en-us/library/bb205350(VS.85).aspx
	float fovY = D3DXToRadian(d3d.fieldOfView * CameraZoomScale);
	float aspect = (float)ScreenDescriptorBlock.SDB_Width / (float)ScreenDescriptorBlock.SDB_Height;
	float yScale = cot(fovY/2.0f);

	float originalYScale = yScale;
	yScale *= p;

	float xScale = originalYScale / aspect;
	aspect /= p;

	xScale *= o;

	// column 1
	d3d.matProjection._11 = xScale;
	d3d.matProjection._21 = 0.0f;
	d3d.matProjection._31 = 0.0f;
	d3d.matProjection._41 = 0.0f;

	// column 2
	d3d.matProjection._12 = 0.0f;
	d3d.matProjection._22 = yScale;
	d3d.matProjection._32 = 0.0f;
	d3d.matProjection._42 = 0.0f;

	// column 3
	d3d.matProjection._13 = 0.0f;
	d3d.matProjection._23 = 0.0f;
	d3d.matProjection._33 = zFar/(zFar-zNear);
	d3d.matProjection._43 = -zNear*zFar/(zFar-zNear);

	// column 4
	d3d.matProjection._14 = 0.0f;
	d3d.matProjection._24 = 0.0f;
	d3d.matProjection._34 = 1.0f;
	d3d.matProjection._44 = 0.0f;
}

void UpdateViewMatrix(float *viewMat)
{	
	/*
	 * Melanikus - 09/11/11
	 * We inverted vecFront's Y component because vecUp is upside down (VecUp points to -Y).
	 * We forgot to invert vecRight's Y component, an error that could only be noticed with 
	 * the alien climbing a wall and looking to the Up axis relative to that wall. The error 
	 * was due the different axis between world player orientation and local camera matrix.
	 */ 
	D3DXVECTOR3 vecRight	(viewMat[0], -viewMat[1], viewMat[2]);
	D3DXVECTOR3 vecUp		(viewMat[4],  viewMat[5], viewMat[6]);
	D3DXVECTOR3 vecFront	(viewMat[8], -viewMat[9], viewMat[10]);
	D3DXVECTOR3 vecPosition (viewMat[3], -viewMat[7], viewMat[11]);

	D3DXVec3Normalize(&vecFront, &vecFront);

	D3DXVec3Cross(&vecUp, &vecFront, &vecRight);
	D3DXVec3Normalize(&vecUp, &vecUp);

	D3DXVec3Cross(&vecRight, &vecUp, &vecFront);
	D3DXVec3Normalize(&vecRight, &vecRight);

	// right
	d3d.matView._11 = vecRight.x;
	d3d.matView._21 = vecRight.y;
	d3d.matView._31 = vecRight.z;

	// up
	d3d.matView._12 = vecUp.x;
	d3d.matView._22 = vecUp.y;
	d3d.matView._32 = vecUp.z;

	// front
	d3d.matView._13 = vecFront.x;
	d3d.matView._23 = vecFront.y;
	d3d.matView._33 = vecFront.z;

	// 4th
	d3d.matView._14 = 0.0f;
	d3d.matView._24 = 0.0f;
	d3d.matView._34 = 0.0f;
	d3d.matView._44 = 1.0f;

	d3d.matView._41 = -D3DXVec3Dot(&vecPosition, &vecRight);
	d3d.matView._42 = -D3DXVec3Dot(&vecPosition, &vecUp);
	d3d.matView._43 = -D3DXVec3Dot(&vecPosition, &vecFront);
}

void DrawCoronas()
{
	if (!coronaArray.size()) {
		return;
	}

	uint32_t numVertsBackup = RenderPolygon.NumberOfVertices;

	RCOLOR colour;

	uint32_t texWidth, texHeight;
	Tex_GetDimensions(SpecialFXImageNumber, texWidth, texHeight);

	float RecipW = 1.0f / (float) texWidth;
	float RecipH = 1.0f / (float) texHeight;

	// set a command to disable z-writes
	orthoList->AddCommand(kCommandZWriteDisable);

	for (size_t i = 0; i < coronaArray.size(); i++)
	{
		PARTICLE_DESC *particleDescPtr = &ParticleDescription[coronaArray[i].particle.ParticleID];

		renderCorona &thisCorona = coronaArray[i];

		assert (thisCorona.numVerts == 4);

		if (particleDescPtr->IsLit && !(thisCorona.particle.ParticleID == PARTICLE_ALIEN_BLOOD && CurrentVisionMode == VISION_MODE_PRED_SEEALIENS))
		{
			int intensity = LightIntensityAtPoint(&thisCorona.particle.Position);
			if (thisCorona.particle.ParticleID == PARTICLE_SMOKECLOUD || thisCorona.particle.ParticleID == PARTICLE_ANDROID_BLOOD)
			{
				colour = RGBA_MAKE
						(
							MUL_FIXED(intensity, thisCorona.particle.ColourComponents.Red),
							MUL_FIXED(intensity, thisCorona.particle.ColourComponents.Green),
							MUL_FIXED(intensity, thisCorona.particle.ColourComponents.Blue),
							thisCorona.particle.ColourComponents.Alpha
						);
			}
			else
			{
				colour = RGBA_MAKE
						(
							MUL_FIXED(intensity, particleDescPtr->RedScale[CurrentVisionMode]),
							MUL_FIXED(intensity, particleDescPtr->GreenScale[CurrentVisionMode]),
							MUL_FIXED(intensity, particleDescPtr->BlueScale[CurrentVisionMode]),
							particleDescPtr->Alpha
						);
			}
		}
		else
		{
			colour = thisCorona.particle.Colour;
		}

		if (RAINBOWBLOOD_CHEATMODE)
		{
			colour = RGBA_MAKE
						(
							FastRandom()&255,
							FastRandom()&255,
							FastRandom()&255,
							particleDescPtr->Alpha
						);
		}

		// transform our coronas worldview position into projection then viewport space
		D3DXVECTOR3 transformedPoint;

		transformedPoint.x = thisCorona.coronaPoint.vx;
		transformedPoint.y = thisCorona.coronaPoint.vy;
		transformedPoint.z = thisCorona.coronaPoint.vz;

		float centreX = transformedPoint.x / transformedPoint.z;
		float centreY = transformedPoint.y / transformedPoint.z;

		D3DXMATRIX finalMatrix = d3d.matProjection * d3d.matViewPort;

		// already view transformed, do projection transform and viewport transform
		D3DXVec3TransformCoord(&transformedPoint, &transformedPoint, &finalMatrix);

		// generate the quad around this point
		uint32_t size = 100;

		uint32_t sizeX = (ScreenDescriptorBlock.SDB_Width / 100) * 10;
		uint32_t sizeY = sizeX;//(ScreenDescriptorBlock.SDB_Height / 100) * 11;

		if (!orthoList->AddItem(4, SpecialFXImageNumber, (enum TRANSLUCENCY_TYPE)particleDescPtr->TranslucencyType, FILTERING_BILINEAR_ON, TEXTURE_CLAMP)) {
			continue; // no room for this one, and the end of this function still has to run
		}

		// bottom left
		orthoVertex[orthoVBOffset].x = WPos2DC(transformedPoint.x - sizeX);
		orthoVertex[orthoVBOffset].y = HPos2DC(transformedPoint.y + sizeY);
		orthoVertex[orthoVBOffset].z = 1.0f;
		orthoVertex[orthoVBOffset].colour = colour;
		orthoVertex[orthoVBOffset].u = 192.0f * RecipW;
		orthoVertex[orthoVBOffset].v = 63.0f * RecipH;
		orthoVBOffset++;

		// top left
		orthoVertex[orthoVBOffset].x = WPos2DC(transformedPoint.x - sizeX);
		orthoVertex[orthoVBOffset].y = HPos2DC(transformedPoint.y - sizeY);
		orthoVertex[orthoVBOffset].z = 1.0f;
		orthoVertex[orthoVBOffset].colour = colour;
		orthoVertex[orthoVBOffset].u = 192.0f * RecipW;
		orthoVertex[orthoVBOffset].v = 0.0f * RecipH;
		orthoVBOffset++;

		// bottom right
		orthoVertex[orthoVBOffset].x = WPos2DC(transformedPoint.x + sizeX);
		orthoVertex[orthoVBOffset].y = HPos2DC(transformedPoint.y + sizeY);
		orthoVertex[orthoVBOffset].z = 1.0f;
		orthoVertex[orthoVBOffset].colour = colour;
		orthoVertex[orthoVBOffset].u = 255.0f * RecipW;
		orthoVertex[orthoVBOffset].v = 63.0f * RecipH;
		orthoVBOffset++;

		// top right
		orthoVertex[orthoVBOffset].x = WPos2DC(transformedPoint.x + sizeX);
		orthoVertex[orthoVBOffset].y = HPos2DC(transformedPoint.y - sizeY);
		orthoVertex[orthoVBOffset].z = 1.0f;
		orthoVertex[orthoVBOffset].colour = colour;
		orthoVertex[orthoVBOffset].u = 255.0f * RecipW;
		orthoVertex[orthoVBOffset].v = 0.0f * RecipH;
		orthoVBOffset++;

		orthoList->CreateOrthoIndices(orthoIndex);
	}

	coronaArray.clear();

	// set a command to enable z-writes
	orthoList->AddCommand(kCommandZWriteEnable);

	// restore RenderPolygon.NumberOfVertices value...
	RenderPolygon.NumberOfVertices = numVertsBackup;
}

void DrawParticles()
{
	if (!particleBucket.size()) {
		return;
	}

	// set command to disable z-writes
	particleList->AddCommand(kCommandZWriteDisable);

	// loop particles and add them to vertex buffer
	for (size_t i = 0; i < NUM_TRANSLUCENCY_TYPES; i++)
	{
		for (size_t j = 0; j < particleBucket[i].size(); j++)
		{
			D3D_Particle_Output(&particleBucket[i][j].particle, &particleBucket[i][j].vertices[0]);
		}
	}

	// set command to disable z-writes
	particleList->AddCommand(kCommandZWriteEnable);

	for (size_t i = 0; i < NUM_TRANSLUCENCY_TYPES; i++)
	{
		particleBucket[i].clear();
	}
}

// Writes down a polygon's normals, if it goes in the surface pictures. Only a real polygon of the shape being drawn has normals to look up,
// and shapePolygon is its place in that shape. The other callers make their polygons up on the spot, like an explosion's, and those have no
// corner numbers, so they leave it at -1. A see-through polygon can be drawn long after its shape, when the shape's normals are no longer
// the current ones. It never reaches the pictures, and nor does the weapon, so neither needs a normal.
static void WriteSurfaceNormals(const POLYHEADER *polygon, int shapePolygon)
{
	if (shapePolygon >= 0 && !gunLayer && RenderPolygon.TranslucencyMode == TRANSLUCENCY_OFF)
	{
		SurfaceNormals_WritePolygon(polygon, shapePolygon, vb, RenderPolygon.NumberOfVertices);
	}
}

void D3D_ZBufferedGouraudTexturedPolygon_Output(POLYHEADER *inputPolyPtr, RENDERVERTEX *renderVerticesPtr, int shapePolygon)
{
	// bjd - This is the function responsible for drawing level geometry and the players weapon
	if (!mainList->Check(RenderPolygon.NumberOfVertices)) {
		FlushLists();
	}

	texID_t textureID = (inputPolyPtr->PolyColour & ClrTxDefn);

	uint32_t texWidth, texHeight;
	Tex_GetDimensions(textureID, texWidth, texHeight);

	float RecipW = 1.0f / /*(float)*/ texWidth;
	float RecipH = 1.0f / /*(float)*/ texHeight;

	if (gunLayer) {
		if (!weaponList->AddItem(RenderPolygon.NumberOfVertices, textureID, RenderPolygon.TranslucencyMode)) {
			return;
		}
	} else {
		if (!mainList->AddItem(RenderPolygon.NumberOfVertices, textureID, RenderPolygon.TranslucencyMode)) {
			return;
		}
	}

	WriteSurfaceNormals(inputPolyPtr, shapePolygon);

	for (uint32_t i = 0; i < RenderPolygon.NumberOfVertices; i++)
	{
		RENDERVERTEX *vertices = &renderVerticesPtr[i];

		mainVertex[vb].x = (float)vertices->X;
		mainVertex[vb].y = (float)-vertices->Y;
		mainVertex[vb].z = (float)vertices->Z;

		// gamma testing
//		mainVertex[vb].color = RGBA_MAKE(vertices->R, vertices->G, vertices->B, vertices->A);
//		mainVertex[vb].specular = RGBA_MAKE(vertices->SpecularR, vertices->SpecularG, vertices->SpecularB, /*255*/0);

		mainVertex[vb].color = RGBA_MAKE(GammaValues[vertices->R], GammaValues[vertices->G], GammaValues[vertices->B], vertices->A);
		mainVertex[vb].specular = RGBA_MAKE(GammaValues[vertices->SpecularR], GammaValues[vertices->SpecularG], GammaValues[vertices->SpecularB], /*255*/0);

		mainVertex[vb].u = (float)(vertices->U) * RecipW;
		mainVertex[vb].v = (float)(vertices->V) * RecipH;
		vb++;
	}

	if (gunLayer) {
		weaponList->CreateIndices(mainIndex, RenderPolygon.NumberOfVertices);
	} else {
		mainList->CreateIndices(mainIndex, RenderPolygon.NumberOfVertices);
	}
}

void D3D_ZBufferedGouraudPolygon_Output(POLYHEADER *inputPolyPtr, RENDERVERTEX *renderVerticesPtr, int shapePolygon)
{
	// responsible for drawing predators lock on triangle, for one
	if (!mainList->Check(RenderPolygon.NumberOfVertices)) {
		FlushLists();
	}

	// Take header information
	int32_t flags = inputPolyPtr->PolyFlags;

	if (!mainList->AddItem(RenderPolygon.NumberOfVertices, NO_TEXTURE, RenderPolygon.TranslucencyMode)) {
		return;
	}

	// With no texture, this reads the white one, so the surface pictures hold it as white. It's still a real surface, and the lights left out
	// of the game's lighting for its shape are added from those pictures.
	WriteSurfaceNormals(inputPolyPtr, shapePolygon);

	for (uint32_t i = 0; i < RenderPolygon.NumberOfVertices; i++)
	{
		RENDERVERTEX *vertices = &renderVerticesPtr[i];

		mainVertex[vb].x = (float)vertices->X;
		mainVertex[vb].y = (float)-vertices->Y;
		mainVertex[vb].z = (float)vertices->Z;

		if (flags & iflag_transparent)
		{
	  		mainVertex[vb].color = RGBA_MAKE(vertices->R, vertices->G, vertices->B, vertices->A);
		}
		else
		{
			mainVertex[vb].color = RGBA_MAKE(vertices->R, vertices->G, vertices->B, 255);
		}

		mainVertex[vb].specular = RGBA_MAKE(0, 0, 0, 0);
		mainVertex[vb].u = 0.0f;
		mainVertex[vb].v = 0.0f;
		vb++;
	}

	mainList->CreateIndices(mainIndex, RenderPolygon.NumberOfVertices);
}

void D3D_PredatorThermalVisionPolygon_Output(POLYHEADER *inputPolyPtr, RENDERVERTEX *renderVerticesPtr)
{
	if (!mainList->Check(RenderPolygon.NumberOfVertices)) {
		FlushLists();
	}

	if (!mainList->AddItem(RenderPolygon.NumberOfVertices, NO_TEXTURE, TRANSLUCENCY_OFF)) {
		return;
	}

	for (uint32_t i = 0; i < RenderPolygon.NumberOfVertices; i++)
	{
		RENDERVERTEX *vertices = &renderVerticesPtr[i];

		mainVertex[vb].x = (float)vertices->X;
		mainVertex[vb].y = (float)-vertices->Y;
		mainVertex[vb].z = (float)vertices->Z;

		mainVertex[vb].color = RGBA_MAKE(vertices->R, vertices->G, vertices->B, vertices->A);
		mainVertex[vb].specular = RCOLOR_ARGB(0,0,0,0);
		mainVertex[vb].u = 0.0f;
		mainVertex[vb].v = 0.0f;
		vb++;
	}

	mainList->CreateIndices(mainIndex, RenderPolygon.NumberOfVertices);
}

void D3D_ZBufferedCloakedPolygon_Output(POLYHEADER *inputPolyPtr, RENDERVERTEX *renderVerticesPtr)
{
	if (!mainList->Check(RenderPolygon.NumberOfVertices)) {
		FlushLists();
	}

	// We assume bit 15 (TxLocal) HAS been
	// properly cleared this time...
	texID_t textureID = (inputPolyPtr->PolyColour & ClrTxDefn);

	uint32_t texWidth, texHeight;
	Tex_GetDimensions(textureID, texWidth, texHeight);

	float RecipW = 1.0f / (float) texWidth;
	float RecipH = 1.0f / (float) texHeight;

//	mainList->AddItem(RenderPolygon.NumberOfVertices, textureID, TRANSLUCENCY_NORMAL);
	if (gunLayer) {
		if (!weaponList->AddItem(RenderPolygon.NumberOfVertices, textureID, TRANSLUCENCY_NORMAL)) {
			return;
		}
	} else {
		if (!mainList->AddItem(RenderPolygon.NumberOfVertices, textureID, TRANSLUCENCY_NORMAL)) {
			return;
		}
	}

	for (uint32_t i = 0; i < RenderPolygon.NumberOfVertices; i++)
	{
		RENDERVERTEX *vertices = &renderVerticesPtr[i];

		mainVertex[vb].x = (float)vertices->X;
		mainVertex[vb].y = (float)-vertices->Y;
		mainVertex[vb].z = (float)vertices->Z;

	   	mainVertex[vb].color = RGBA_MAKE(vertices->R, vertices->G, vertices->B, vertices->A);
		mainVertex[vb].specular = RGBA_MAKE(0,0,0,/*255*/0);

		mainVertex[vb].u = (float)(vertices->U) * RecipW;
		mainVertex[vb].v = (float)(vertices->V) * RecipH;
		vb++;
	}

//	mainList->CreateIndices(mainIndex, RenderPolygon.NumberOfVertices);

	if (gunLayer) {
		weaponList->CreateIndices(mainIndex, RenderPolygon.NumberOfVertices);
	} else {
		mainList->CreateIndices(mainIndex, RenderPolygon.NumberOfVertices);
	}
}

void D3D_Rectangle(int x0, int y0, int x1, int y1, int r, int g, int b, int a)
{
	float new_x1 = WPos2DC(x0);
	float new_y1 = HPos2DC(y0);

	float new_x2 = WPos2DC(x1);
	float new_y2 = HPos2DC(y1);

	if (!orthoList->AddItem(4, NO_TEXTURE, TRANSLUCENCY_GLOWING, FILTERING_BILINEAR_ON, TEXTURE_CLAMP)) {
		return;
	}

	// bottom left
	orthoVertex[orthoVBOffset].x = new_x1;
	orthoVertex[orthoVBOffset].y = new_y2;
	orthoVertex[orthoVBOffset].z = 1.0f;
	orthoVertex[orthoVBOffset].colour = RGBA_MAKE(r,g,b,a);
	orthoVertex[orthoVBOffset].u = 0.0f;
	orthoVertex[orthoVBOffset].v = 0.0f;
	orthoVBOffset++;

	// top left
	orthoVertex[orthoVBOffset].x = new_x1;
	orthoVertex[orthoVBOffset].y = new_y1;
	orthoVertex[orthoVBOffset].z = 1.0f;
	orthoVertex[orthoVBOffset].colour = RGBA_MAKE(r,g,b,a);
	orthoVertex[orthoVBOffset].u = 0.0f;
	orthoVertex[orthoVBOffset].v = 0.0f;
	orthoVBOffset++;

	// bottom right
	orthoVertex[orthoVBOffset].x = new_x2;
	orthoVertex[orthoVBOffset].y = new_y2;
	orthoVertex[orthoVBOffset].z = 1.0f;
	orthoVertex[orthoVBOffset].colour = RGBA_MAKE(r,g,b,a);
	orthoVertex[orthoVBOffset].u = 0.0f;
	orthoVertex[orthoVBOffset].v = 0.0f;
	orthoVBOffset++;

	// top right
	orthoVertex[orthoVBOffset].x = new_x2;
	orthoVertex[orthoVBOffset].y = new_y1;
	orthoVertex[orthoVBOffset].z = 1.0f;
	orthoVertex[orthoVBOffset].colour = RGBA_MAKE(r,g,b,a);
	orthoVertex[orthoVBOffset].u = 0.0f;
	orthoVertex[orthoVBOffset].v = 0.0f;
	orthoVBOffset++;

	orthoList->CreateOrthoIndices(orthoIndex);
}

void D3D_HUDQuad_Output(texID_t textureID, struct VertexTag *quadVerticesPtr, uint32_t colour, enum FILTERING_MODE_ID filteringType)
{
	uint32_t texWidth, texHeight;
	Tex_GetDimensions(textureID, texWidth, texHeight);

	float RecipW = 1.0f / texWidth;
	float RecipH = 1.0f / texHeight;

	if (!orthoList->AddItem(4, textureID, TRANSLUCENCY_GLOWING, filteringType, TEXTURE_CLAMP)) {
		return;
	}

	// bottom left
	orthoVertex[orthoVBOffset].x = WPos2DC(quadVerticesPtr[3].X);
	orthoVertex[orthoVBOffset].y = HPos2DC(quadVerticesPtr[3].Y);
	orthoVertex[orthoVBOffset].z = 1.0f;
	orthoVertex[orthoVBOffset].colour = colour;
	orthoVertex[orthoVBOffset].u = quadVerticesPtr[3].U * RecipW;
	orthoVertex[orthoVBOffset].v = quadVerticesPtr[3].V * RecipH;
	orthoVBOffset++;

	// top left
	orthoVertex[orthoVBOffset].x = WPos2DC(quadVerticesPtr[0].X);
	orthoVertex[orthoVBOffset].y = HPos2DC(quadVerticesPtr[0].Y);
	orthoVertex[orthoVBOffset].z = 1.0f;
	orthoVertex[orthoVBOffset].colour = colour;
	orthoVertex[orthoVBOffset].u = quadVerticesPtr[0].U * RecipW;
	orthoVertex[orthoVBOffset].v = quadVerticesPtr[0].V * RecipH;
	orthoVBOffset++;

	// bottom right
	orthoVertex[orthoVBOffset].x = WPos2DC(quadVerticesPtr[2].X);
	orthoVertex[orthoVBOffset].y = HPos2DC(quadVerticesPtr[2].Y);
	orthoVertex[orthoVBOffset].z = 1.0f;
	orthoVertex[orthoVBOffset].colour = colour;
	orthoVertex[orthoVBOffset].u = quadVerticesPtr[2].U * RecipW;
	orthoVertex[orthoVBOffset].v = quadVerticesPtr[2].V * RecipH;
	orthoVBOffset++;

	// top right
	orthoVertex[orthoVBOffset].x = WPos2DC(quadVerticesPtr[1].X);
	orthoVertex[orthoVBOffset].y = HPos2DC(quadVerticesPtr[1].Y);
	orthoVertex[orthoVBOffset].z = 1.0f;
	orthoVertex[orthoVBOffset].colour = colour;
	orthoVertex[orthoVBOffset].u = quadVerticesPtr[1].U * RecipW;
	orthoVertex[orthoVBOffset].v = quadVerticesPtr[1].V * RecipH;
	orthoVBOffset++;

	orthoList->CreateOrthoIndices(orthoIndex);
}

void D3D_DrawParticle_Rain(PARTICLE *particlePtr, VECTORCH *prevPositionPtr)
{
	VECTORCH vertices[3];
	vertices[0] = *prevPositionPtr;

	// translate second vertex into view space
	TranslatePointIntoViewspace(&vertices[0]);

#ifndef USE_D3DVIEWTRANSFORM
	// is particle within normal view frustum?
	if ((-vertices[0].vx <= vertices[0].vz)
	&& (vertices[0].vx <= vertices[0].vz)
	&& (-vertices[0].vy <= vertices[0].vz)
	&& (vertices[0].vy <= vertices[0].vz))
#endif
	{
		vertices[1] = particlePtr->Position;
		vertices[2] = particlePtr->Position;
		vertices[1].vx += particlePtr->Offset.vx;
		vertices[2].vx -= particlePtr->Offset.vx;
		vertices[1].vz += particlePtr->Offset.vz;
		vertices[2].vz -= particlePtr->Offset.vz;

		// translate particle into view space
		TranslatePointIntoViewspace(&vertices[1]);
		TranslatePointIntoViewspace(&vertices[2]);

		// A raindrop is three corners and a level can have 999 drops, so rain can reach the end of the main list like
		// anything else in the level. When it does, what's waiting is drawn and the list starts again.
		if (!mainList->Check(3)) {
			FlushLists();
		}

		if (!mainList->AddItem(3, NO_TEXTURE, TRANSLUCENCY_NORMAL)) {
			return;
		}

		VECTORCH *verticesPtr = vertices;

		for (uint32_t i = 0; i < 3; i++)
		{
			mainVertex[vb].x = (float)verticesPtr->vx;
			mainVertex[vb].y = (float)-verticesPtr->vy;
			mainVertex[vb].z = (float)verticesPtr->vz;

			if (i==3)
			{
				mainVertex[vb].color = RGBA_MAKE(0, 255, 255, 32);
			}
			else
			{
				mainVertex[vb].color = RGBA_MAKE(255, 255, 255, 32);
			}

			mainVertex[vb].specular = RGBA_MAKE(0,0,0,0);
			mainVertex[vb].u = 0.0f;
			mainVertex[vb].v = 0.0f;

			vb++;
			verticesPtr++;
		}

		mainList->CreateIndices(mainIndex, 3);
	}
}

void D3D_DecalSystem_Setup()
{
/*
	UnlockExecuteBufferAndPrepareForUse();
	ExecuteBuffer();
	LockExecuteBuffer();
*/
}

void D3D_DecalSystem_End()
{
//	DrawParticles();
//	DrawCoronas();
/*
	UnlockExecuteBufferAndPrepareForUse();
	ExecuteBuffer();
	LockExecuteBuffer();
*/
}

void D3D_Decal_Output(DECAL *decalPtr, RENDERVERTEX *renderVerticesPtr)
{
	// function responsible for bullet marks on walls, etc
	DECAL_DESC *decalDescPtr = &DecalDescription[decalPtr->DecalID];

	texID_t textureID = SpecialFXImageNumber;

	float RecipW = 1.0f;
	float RecipH = 1.0f;
	RCOLOR colour;

	assert(RenderPolygon.NumberOfVertices == 4);

	if (decalPtr->DecalID == DECAL_SHAFTOFLIGHT || decalPtr->DecalID == DECAL_SHAFTOFLIGHT_OUTER)
	{
		textureID = NO_TEXTURE;
	}
	else
	{
		uint32_t texWidth, texHeight;
		Tex_GetDimensions(textureID, texWidth, texHeight);

		RecipW = 1.0f / (float) texWidth;
		RecipH = 1.0f / (float) texHeight;
	}

	if (decalDescPtr->IsLit)
	{
		int intensity = LightIntensityAtPoint(decalPtr->Vertices);
		colour = RGBA_MAKE
	  		  	(
	  		   		MUL_FIXED(intensity, decalDescPtr->RedScale[CurrentVisionMode]),
	  		   		MUL_FIXED(intensity, decalDescPtr->GreenScale[CurrentVisionMode]),
	  		   		MUL_FIXED(intensity, decalDescPtr->BlueScale[CurrentVisionMode]),
	  		   		decalDescPtr->Alpha
	  		   	);
	}
	else
	{
		colour = RGBA_MAKE
			  	(
			   		decalDescPtr->RedScale[CurrentVisionMode],
			   		decalDescPtr->GreenScale[CurrentVisionMode],
			   		decalDescPtr->BlueScale[CurrentVisionMode],
			   		decalDescPtr->Alpha
			   	);
	}

	if (RAINBOWBLOOD_CHEATMODE)
	{
		colour = RGBA_MAKE
				(
					FastRandom()&255,
					FastRandom()&255,
					FastRandom()&255,
					decalDescPtr->Alpha
				);
	}

	if (!decalList->AddItem(RenderPolygon.NumberOfVertices, textureID, decalDescPtr->TranslucencyType, FILTERING_BILINEAR_ON, TEXTURE_CLAMP)) {
		return;
	}

	for (uint32_t i = 0; i < RenderPolygon.NumberOfVertices; i++)
	{
		RENDERVERTEX *vertices = &renderVerticesPtr[i];

		decalVertex[decalVb].x = (float)vertices->X;
		decalVertex[decalVb].y = (float)-vertices->Y;
		decalVertex[decalVb].z = (float)vertices->Z;

		decalVertex[decalVb].colour = colour;

		decalVertex[decalVb].u = (float)(vertices->U) * RecipW;
		decalVertex[decalVb].v = (float)(vertices->V) * RecipH;
		decalVb++;
	}

	decalList->CreateIndices(decalIndex, RenderPolygon.NumberOfVertices);
}

void AddCorona(PARTICLE *particlePtr, VECTORCHF *coronaPoint)
{
	renderCorona newCorona;

	newCorona.numVerts = RenderPolygon.NumberOfVertices;
	newCorona.particle = *particlePtr;

	memcpy(&newCorona.coronaPoint, coronaPoint, sizeof(VECTORCHF));
	newCorona.translucency = ParticleDescription[particlePtr->ParticleID].TranslucencyType;

	coronaArray.push_back(newCorona);
}

void AddParticle(PARTICLE *particlePtr, RENDERVERTEX *renderVerticesPtr)
{
	if (!pbInit)
	{
		particleBucket.resize(NUM_TRANSLUCENCY_TYPES);
		pbInit = true;
	}

	renderParticle newParticle;
	newParticle.particle = *particlePtr;

	// copy vertex data
	for (int i = 0; i < 4; i++)
	{
		newParticle.vertices[i].x = (float)renderVerticesPtr->X;
		newParticle.vertices[i].y = (float)renderVerticesPtr->Y;
		newParticle.vertices[i].z = (float)renderVerticesPtr->Z;
		newParticle.vertices[i].u = (float)renderVerticesPtr->U;
		newParticle.vertices[i].v = (float)renderVerticesPtr->V;
		renderVerticesPtr++;
	}

	uint32_t transID = ParticleDescription[particlePtr->ParticleID].TranslucencyType;

	// add to correct bucket
	particleBucket[transID].push_back(newParticle);
}

static void D3D_Particle_Output(PARTICLE *particlePtr, PARTICLEVERTEX *renderVerticesPtr)
{
	// steam jets, pulse rifle muzzle flash, wall lights, fire (inc aliens on fire) etc
	PARTICLE_DESC *particleDescPtr = &ParticleDescription[particlePtr->ParticleID];

	uint32_t texWidth, texHeight;
	Tex_GetDimensions(SpecialFXImageNumber, texWidth, texHeight);

	float RecipW = 1.0f / (float) texWidth;
	float RecipH = 1.0f / (float) texHeight;

	if (!particleList->AddItem(4, SpecialFXImageNumber, (enum TRANSLUCENCY_TYPE)particleDescPtr->TranslucencyType, FILTERING_BILINEAR_ON, TEXTURE_CLAMP)) {
		return;
	}

	RCOLOR colour;

	if (particleDescPtr->IsLit && !(particlePtr->ParticleID == PARTICLE_ALIEN_BLOOD && CurrentVisionMode == VISION_MODE_PRED_SEEALIENS))
	{
		int intensity = LightIntensityAtPoint(&particlePtr->Position);
		if (particlePtr->ParticleID == PARTICLE_SMOKECLOUD || particlePtr->ParticleID == PARTICLE_ANDROID_BLOOD)
		{
			colour = RGBA_MAKE
				  	(
				   		MUL_FIXED(intensity, particlePtr->ColourComponents.Red),
				   		MUL_FIXED(intensity, particlePtr->ColourComponents.Green),
				   		MUL_FIXED(intensity, particlePtr->ColourComponents.Blue),
				   		particlePtr->ColourComponents.Alpha
				   	);
		}
		else
		{
			colour = RGBA_MAKE
				  	(
				   		MUL_FIXED(intensity, particleDescPtr->RedScale[CurrentVisionMode]),
				   		MUL_FIXED(intensity, particleDescPtr->GreenScale[CurrentVisionMode]),
				   		MUL_FIXED(intensity, particleDescPtr->BlueScale[CurrentVisionMode]),
				   		particleDescPtr->Alpha
				   	);
		}
	}
	else
	{
		colour = particlePtr->Colour;
	}

	if (RAINBOWBLOOD_CHEATMODE)
	{
		colour = RGBA_MAKE
					(
						FastRandom()&255,
						FastRandom()&255,
						FastRandom()&255,
						particleDescPtr->Alpha
					);
	}

	for (uint32_t i = 0; i < 4; i++)
	{
		PARTICLEVERTEX *vertices = &renderVerticesPtr[i];

		float zvalue;

		if (particleDescPtr->IsDrawnInFront)
		{
			zvalue = 0.0f;
		}
		else if (particleDescPtr->IsDrawnAtBack)
		{
			zvalue = 1.0f;
		}
		else
		{
			zvalue = (float)vertices->z;
		}

		particleVertex[pVb].x = vertices->x;
		particleVertex[pVb].y = -vertices->y;
		particleVertex[pVb].z = vertices->z;

		particleVertex[pVb].colour = colour;

		particleVertex[pVb].u = vertices->u * RecipW;
		particleVertex[pVb].v = vertices->v * RecipH;
		pVb++;
	}

	// 3: Create Indices
	particleList->CreateIndices(particleIndex, 4);
}

void PostLandscapeRendering()
{
	int numOfObjects = NumOnScreenBlocks;

	if (!strcmp(LevelName,"fall") || !strcmp(LevelName,"fall_m"))
	{
		bool drawWaterFall = false;
		bool drawStream    = false;

		while (numOfObjects)
		{
			DISPLAYBLOCK *objectPtr = OnScreenBlockList[--numOfObjects];
			MODULE *modulePtr = objectPtr->ObMyModule;

			// if it's a module, which isn't inside another module
			if (modulePtr && modulePtr->name)
			{
				if ((!strcmp(modulePtr->name, "fall01"))
				  ||(!strcmp(modulePtr->name, "well01"))
				  ||(!strcmp(modulePtr->name, "well02"))
				  ||(!strcmp(modulePtr->name, "well03"))
				  ||(!strcmp(modulePtr->name, "well04"))
				  ||(!strcmp(modulePtr->name, "well05"))
				  ||(!strcmp(modulePtr->name, "well06"))
				  ||(!strcmp(modulePtr->name, "well07"))
				  ||(!strcmp(modulePtr->name, "well08"))
				  ||(!strcmp(modulePtr->name, "well")))
				{
					drawWaterFall = true;
				}
				else if ((!strcmp(modulePtr->name, "stream02"))
				       ||(!strcmp(modulePtr->name, "stream03"))
				       ||(!strcmp(modulePtr->name, "watergate")))
				{
		   			drawStream = true;
				}
			}
		}

		if (drawWaterFall)
		{
	   		//UpdateWaterFall();
			WaterFallBase = 109952;

			MeshZScale = (66572 - 51026)/15;
			MeshXScale = (109952 + 3039)/45;

	   		D3D_DrawWaterFall(175545, -3039, 51026);
		}
		if (drawStream)
		{
			int x = 68581;
			int y = 12925;
			int z = 93696;
			MeshXScale = (87869-68581);
			MeshZScale = (105385-93696);

			CheckForObjectsInWater(x, x+MeshXScale, z, z+MeshZScale, y);

			WaterXOrigin = x;
			WaterZOrigin = z;
			WaterUScale = 4.0f / (float)MeshXScale;
			WaterVScale = 4.0f / (float)MeshZScale;
		 	MeshXScale /= 4;
		 	MeshZScale /= 2;

			currentWaterTexture = ChromeImageNumber;

		 	D3D_DrawWaterPatch(x, y, z);
		 	D3D_DrawWaterPatch(x+MeshXScale, y, z);
		 	D3D_DrawWaterPatch(x+MeshXScale*2, y, z);
		 	D3D_DrawWaterPatch(x+MeshXScale*3, y, z);
		 	D3D_DrawWaterPatch(x, y, z+MeshZScale);
		 	D3D_DrawWaterPatch(x+MeshXScale, y, z+MeshZScale);
		 	D3D_DrawWaterPatch(x+MeshXScale*2, y, z+MeshZScale);
		 	D3D_DrawWaterPatch(x+MeshXScale*3, y, z+MeshZScale);
		}
	}
	else if (!_stricmp(LevelName,"invasion_a"))
	{
		bool drawWater    = false;
		bool drawEndWater = false;

		while (numOfObjects)
		{
			DISPLAYBLOCK *objectPtr = OnScreenBlockList[--numOfObjects];
			MODULE *modulePtr = objectPtr->ObMyModule;

			// if it's a module, which isn't inside another module
			if (modulePtr && modulePtr->name)
			{
				if ((!strcmp(modulePtr->name, "hivepool"))
				  ||(!strcmp(modulePtr->name, "hivepool04")))
				{
					drawWater = true;
					break;
				}
				else
				{
					if (!strcmp(modulePtr->name, "shaftbot"))
					{
						drawEndWater = true;
					}
					if ((!_stricmp(modulePtr->name, "shaft01"))
					 ||(!_stricmp(modulePtr->name, "shaft02"))
					 ||(!_stricmp(modulePtr->name, "shaft03"))
					 ||(!_stricmp(modulePtr->name, "shaft04"))
					 ||(!_stricmp(modulePtr->name, "shaft05"))
					 ||(!_stricmp(modulePtr->name, "shaft06")))
					{
						HandleRainShaft(modulePtr, -11726,-107080,10);
						drawEndWater = true;
						break;
					}
				}
			}
		}

		if (drawWater)
		{
			int x = 20767;
			int y = -36000+200;
			int z = 30238;
			MeshXScale = (36353-20767);
			MeshZScale = (41927-30238);
				
			CheckForObjectsInWater(x, x+MeshXScale, z, z+MeshZScale, y);

			WaterXOrigin = x;
			WaterZOrigin = z;
			WaterUScale = 4.0f / (float)MeshXScale;
			WaterVScale = 4.0f / (float)MeshZScale;
		 	MeshXScale /= 4;
		 	MeshZScale /= 2;

			currentWaterTexture = ChromeImageNumber;

		 	D3D_DrawWaterPatch(x, y, z);
		 	D3D_DrawWaterPatch(x+MeshXScale, y, z);
		 	D3D_DrawWaterPatch(x+MeshXScale*2, y, z);
		 	D3D_DrawWaterPatch(x+MeshXScale*3, y, z);
		 	D3D_DrawWaterPatch(x, y, z+MeshZScale);
		 	D3D_DrawWaterPatch(x+MeshXScale, y, z+MeshZScale);
		 	D3D_DrawWaterPatch(x+MeshXScale*2, y, z+MeshZScale);
		 	D3D_DrawWaterPatch(x+MeshXScale*3, y, z+MeshZScale);
		}
		else if (drawEndWater)
		{
			int x = -15471;
			int y = -11720-500;
			int z = -55875;
			MeshXScale = (15471-1800);
			MeshZScale = (55875-36392);

			CheckForObjectsInWater(x, x+MeshXScale, z, z+MeshZScale, y);

			WaterXOrigin = x;
			WaterZOrigin = z;
			WaterUScale = 4.0f / (float)(MeshXScale+1800-3782);
			WaterVScale = 4.0f / (float)MeshZScale;
		 	MeshXScale /= 4;
		 	MeshZScale /= 2;

			currentWaterTexture = WaterShaftImageNumber;

		 	D3D_DrawWaterPatch(x, y, z);
		 	D3D_DrawWaterPatch(x+MeshXScale, y, z);
		 	D3D_DrawWaterPatch(x+MeshXScale*2, y, z);
		 	D3D_DrawWaterPatch(x+MeshXScale*3, y, z);
		 	D3D_DrawWaterPatch(x, y, z+MeshZScale);
		 	D3D_DrawWaterPatch(x+MeshXScale, y, z+MeshZScale);
		 	D3D_DrawWaterPatch(x+MeshXScale*2, y, z+MeshZScale);
		 	D3D_DrawWaterPatch(x+MeshXScale*3, y, z+MeshZScale);
		}
	}
	else if (!_stricmp(LevelName, "derelict"))
	{
		bool drawMirrorSurfaces = false;
		bool drawWater = false;

		while (numOfObjects)
		{
			DISPLAYBLOCK *objectPtr = OnScreenBlockList[--numOfObjects];
			MODULE *modulePtr = objectPtr->ObMyModule;

			// if it's a module, which isn't inside another module
			if (modulePtr && modulePtr->name)
			{
			  	if ((!_stricmp(modulePtr->name, "start-en01"))
			  	  ||(!_stricmp(modulePtr->name, "start")))
				{
					drawMirrorSurfaces = true;
				}
				else if (!_stricmp(modulePtr->name, "water-01"))
				{
					drawWater = true;
					HandleRainShaft(modulePtr, 32000, 0, 16);
				}
			}
		}

		if (drawMirrorSurfaces)
		{
			// dirt on mirror in marines room
			RenderParticlesInMirror();
			RenderMirrorSurface();
			RenderMirrorSurface2();
		}
		if (drawWater)
		{
			int x = -102799;
			int y = 32000;
			int z = -200964;
			MeshXScale = (102799-87216);
			MeshZScale = (200964-180986);

			CheckForObjectsInWater(x, x+MeshXScale, z, z+MeshZScale, y);

			WaterXOrigin = x;
			WaterZOrigin = z;
			WaterUScale = 4.0f / (float)MeshXScale;
			WaterVScale = 4.0f / (float)MeshZScale;
		 	MeshXScale /= 2;
		 	MeshZScale /= 2;

			currentWaterTexture = ChromeImageNumber;

		 	D3D_DrawWaterPatch(x, y, z);
		 	D3D_DrawWaterPatch(x+MeshXScale, y, z);
		 	D3D_DrawWaterPatch(x, y, z+MeshZScale);
		 	D3D_DrawWaterPatch(x+MeshXScale, y, z+MeshZScale);
		}
	}
	else if (!_stricmp(LevelName, "genshd1"))
	{
		bool drawWater = false;

		while (numOfObjects)
		{
			DISPLAYBLOCK *objectPtr = OnScreenBlockList[--numOfObjects];
			MODULE *modulePtr = objectPtr->ObMyModule;

			// if it's a module, which isn't inside another module
			if (modulePtr && modulePtr->name)
			{
				if ((!_stricmp(modulePtr->name, "largespace"))
				  ||(!_stricmp(modulePtr->name, "proc13"))
				  ||(!_stricmp(modulePtr->name, "trench01"))
				  ||(!_stricmp(modulePtr->name, "trench02"))
				  ||(!_stricmp(modulePtr->name, "trench03"))
				  ||(!_stricmp(modulePtr->name, "trench04"))
				  ||(!_stricmp(modulePtr->name, "trench05"))
				  ||(!_stricmp(modulePtr->name, "trench06"))
				  ||(!_stricmp(modulePtr->name, "trench07"))
				  ||(!_stricmp(modulePtr->name, "trench08"))
				  ||(!_stricmp(modulePtr->name, "trench09")))
				{
					HandleRain(999);
					break;
				}
			}
		}
	}
}

void D3D_DrawWaterTest(MODULE *testModulePtr)
{
	if (!strcmp(LevelName, "genshd1"))
	{
		MODULE *modulePtr = testModulePtr;

		if (modulePtr && modulePtr->name)
		{
			if (!strcmp(modulePtr->name,"05"))
			{
				int y = modulePtr->m_maxy+modulePtr->m_world.vy-500;
		   		int x = modulePtr->m_minx+modulePtr->m_world.vx;
		   		int z = modulePtr->m_minz+modulePtr->m_world.vz;
				MeshXScale = (7791 - -7794);
				MeshZScale = (23378 - 7793);
				{
					CheckForObjectsInWater(x, x+MeshXScale, z, z+MeshZScale, y);
				}

				currentWaterTexture = WaterShaftImageNumber;

				WaterXOrigin=x;
				WaterZOrigin=z;
				WaterUScale = 4.0f/(float)(MeshXScale);
				WaterVScale = 4.0f/(float)MeshZScale;

				MeshXScale/=2;
				MeshZScale/=2;
				D3D_DrawWaterPatch(x, y, z);
				D3D_DrawWaterPatch(x+MeshXScale, y, z);
				D3D_DrawWaterPatch(x, y, z+MeshZScale);
				D3D_DrawWaterPatch(x+MeshXScale, y, z+MeshZScale);

				HandleRainShaft(modulePtr, y,-21000,1);
			}
		}
	}
}

void D3D_DrawWaterPatch(int xOrigin, int yOrigin, int zOrigin)
{
	int i = 0;

	for (uint32_t x = 0; x < 16; x++)
	{
		for (uint32_t z = 0; z < 16; z++)
		{
			VECTORCH *point = &MeshVertex[i];

			point->vx = xOrigin + (x*MeshXScale)/15;
			point->vz = zOrigin + (z*MeshZScale)/15;

			int offset = 0;

 			offset += EffectOfRipples(point);

			point->vy = yOrigin + offset;

			int alpha = 128 - offset / 4;

			switch (CurrentVisionMode)
			{
				default:
				case VISION_MODE_NORMAL:
				{
					MeshVertexColour[i] = RGBA_MAKE(255, 255, 255, alpha);
					break;
				}
				case VISION_MODE_IMAGEINTENSIFIER:
				{
					MeshVertexColour[i] = RGBA_MAKE(0, 51 ,0, alpha);
					break;
				}
				case VISION_MODE_PRED_THERMAL:
				case VISION_MODE_PRED_SEEALIENS:
				case VISION_MODE_PRED_SEEPREDTECH:
				{
					MeshVertexColour[i] = RGBA_MAKE(0, 0, 28, alpha);
				  	break;
				}
			}

			MeshWorldVertex[i].vx = ((point->vx - WaterXOrigin)/4 + MUL_FIXED(GetSin((point->vy*16) & 4095), 128));
			MeshWorldVertex[i].vy = ((point->vz - WaterZOrigin)/4 + MUL_FIXED(GetSin((point->vy*16+200) & 4095), 128));

			TranslatePointIntoViewspace(point);

			i++;
		}
	}

	D3D_DrawMoltenMetalMesh_Unclipped();
}

void D3D_DrawWaterFall(int xOrigin, int yOrigin, int zOrigin)
{
	uint32_t nRequired = MUL_FIXED(250, NormalFrameTime);

	for (uint32_t i = 0; i < nRequired; i++)
	{
		VECTORCH velocity;
		VECTORCH position;
		position.vx = xOrigin;
		position.vy = yOrigin-(FastRandom()&511);//+45*MeshXScale;
		position.vz = zOrigin+(FastRandom()%(15*MeshZScale));

		velocity.vy = (FastRandom()&511)+512;//-((FastRandom()&1023)+2048)*8;
		velocity.vx = ((FastRandom()&511)+256)*2;
		velocity.vz = 0;//-((FastRandom()&511))*8;
		MakeParticle(&position, &velocity, PARTICLE_WATERFALLSPRAY);
	}
}

void D3D_DrawBackdrop(void)
{
	if (TRIPTASTIC_CHEATMODE || MOTIONBLUR_CHEATMODE)
		return;

	if (WireFrameMode)
	{
   		ColourFillBackBuffer(0);
		return;
	}
	else if (ShowDebuggingText.Tears)
	{
		ColourFillBackBuffer((63<<5));
		return;
	}

	bool needToDrawBackdrop = false;

	int numOfObjects = NumActiveBlocks;
	while (numOfObjects--)
	{
		DISPLAYBLOCK *objectPtr = ActiveBlockList[numOfObjects];
		MODULE *modulePtr = objectPtr->ObMyModule;

		if (modulePtr && (ModuleCurrVisArray[modulePtr->m_index] == 2) && modulePtr->m_flags & MODULEFLAG_SKY)
		{
			needToDrawBackdrop = true;
			break;
		}
	}
	if (needToDrawBackdrop)
	{
		ColourFillBackBuffer(0);

		if (LevelHasStars)
		{
			RenderStarfield();
		}
		else
		{
	  		RenderSky();
		}
		return;
	}

	// if the player is outside the environment, clear the screen!
	if (!playerPherModule)
	{
		ColourFillBackBuffer(0);
		return;
	}

	PLAYER_STATUS *playerStatusPtr = (PLAYER_STATUS *)(Player->ObStrategyBlock->SBdataptr);

	if (!playerStatusPtr->IsAlive || FREEFALL_CHEATMODE)
	{
		// minimise effects of camera glitches
		ColourFillBackBuffer(0);
		return;
	}
}

void DrawNoiseOverlay(int t)
{
	// t == 64 for image intensifier
	float u = (float)(FastRandom()&255);
	float v = (float)(FastRandom()&255);
	uint32_t c = 255;
	uint32_t size = 256;//*CameraZoomScale;

	if (!orthoList->AddItem(4, StaticImageNumber, TRANSLUCENCY_GLOWING, FILTERING_BILINEAR_ON, TEXTURE_WRAP)) {
		return;
	}

	// bottom left
	orthoVertex[orthoVBOffset].x = -1.0f;
	orthoVertex[orthoVBOffset].y = 1.0f;
	orthoVertex[orthoVBOffset].z = 1.0f;
	orthoVertex[orthoVBOffset].colour = RGBA_MAKE(c,c,c,t);
	orthoVertex[orthoVBOffset].u = u/256.0f;
	orthoVertex[orthoVBOffset].v = (v+size)/256.0f;
	orthoVBOffset++;

	// top left
	orthoVertex[orthoVBOffset].x = -1.0f;
	orthoVertex[orthoVBOffset].y = -1.0f;
	orthoVertex[orthoVBOffset].z = 1.0f;
	orthoVertex[orthoVBOffset].colour = RGBA_MAKE(c,c,c,t);
	orthoVertex[orthoVBOffset].u = u/256.0f;
	orthoVertex[orthoVBOffset].v = v/256.0f;
	orthoVBOffset++;

	// bottom right
	orthoVertex[orthoVBOffset].x = 1.0f;
	orthoVertex[orthoVBOffset].y = 1.0f;
	orthoVertex[orthoVBOffset].z = 1.0f;
	orthoVertex[orthoVBOffset].colour = RGBA_MAKE(c,c,c,t);
	orthoVertex[orthoVBOffset].u = (u+size)/256.0f;
	orthoVertex[orthoVBOffset].v = (v+size)/256.0f;
	orthoVBOffset++;

	// top right
	orthoVertex[orthoVBOffset].x = 1.0f;
	orthoVertex[orthoVBOffset].y = -1.0f;
	orthoVertex[orthoVBOffset].z = 1.0f;
	orthoVertex[orthoVBOffset].colour = RGBA_MAKE(c,c,c,t);
	orthoVertex[orthoVBOffset].u = (u+size)/256.0f;
	orthoVertex[orthoVBOffset].v = v/256.0f;
	orthoVBOffset++;

	orthoList->CreateOrthoIndices(orthoIndex);
}

void DrawScanlinesOverlay(float level)
{
	float u = 0.0f;//FastRandom()&255;
	float v = 128.0f;//FastRandom()&255;
	uint32_t c = 255;

	int t = static_cast<int>(4.0f+level*64.0f);

	float size = 128.0f*(1.0f-level*0.8f);//*CameraZoomScale;

	if (!orthoList->AddItem(4, PredatorNumbersImageNumber, TRANSLUCENCY_NORMAL, FILTERING_BILINEAR_ON, TEXTURE_WRAP)) {
		return;
	}

	// bottom left
	orthoVertex[orthoVBOffset].x = -1.0f;
	orthoVertex[orthoVBOffset].y = 1.0f;
	orthoVertex[orthoVBOffset].z = 1.0f;
	orthoVertex[orthoVBOffset].colour = RGBA_MAKE(c,c,c,t);
	orthoVertex[orthoVBOffset].u = (v+size)/256.0f;
	orthoVertex[orthoVBOffset].v = 1.0f;
	orthoVBOffset++;

	// top left
	orthoVertex[orthoVBOffset].x = -1.0f;
	orthoVertex[orthoVBOffset].y = -1.0f;
	orthoVertex[orthoVBOffset].z = 1.0f;
	orthoVertex[orthoVBOffset].colour = RGBA_MAKE(c,c,c,t);
	orthoVertex[orthoVBOffset].u = (v-size)/256.0f;
	orthoVertex[orthoVBOffset].v = 1.0f;
	orthoVBOffset++;

	// bottom right
	orthoVertex[orthoVBOffset].x = 1.0f;
	orthoVertex[orthoVBOffset].y = 1.0f;
	orthoVertex[orthoVBOffset].z = 1.0f;
	orthoVertex[orthoVBOffset].colour = RGBA_MAKE(c,c,c,t);
	orthoVertex[orthoVBOffset].u = (v+size)/256.0f;
	orthoVertex[orthoVBOffset].v = 1.0f;
	orthoVBOffset++;

	// top right
	orthoVertex[orthoVBOffset].x = 1.0f;;
	orthoVertex[orthoVBOffset].y = -1.0f;
	orthoVertex[orthoVBOffset].z = 1.0f;
	orthoVertex[orthoVBOffset].colour = RGBA_MAKE(c,c,c,t);
	orthoVertex[orthoVBOffset].u = (v-size)/256.0f;
	orthoVertex[orthoVBOffset].v = 1.0f;
	orthoVBOffset++;

	orthoList->CreateOrthoIndices(orthoIndex);

	if (level == 1.0f)
	{
		DrawNoiseOverlay(128);
	}
}

void D3D_SkyPolygon_Output(POLYHEADER *inputPolyPtr, RENDERVERTEX *renderVerticesPtr)
{
	if (!mainList->Check(RenderPolygon.NumberOfVertices)) {
		FlushLists();
	}

	// We assume bit 15 (TxLocal) HAS been
	// properly cleared this time...
	texID_t textureID = (inputPolyPtr->PolyColour & ClrTxDefn);

	uint32_t texWidth, texHeight;
	Tex_GetDimensions(textureID, texWidth, texHeight);

	// divide by 65536.0f to convert from fixed point
	float RecipW = (1.0f / (float) texWidth)  / 65536.0f;
	float RecipH = (1.0f / (float) texHeight) / 65536.0f;

	if (!mainList->AddItem(RenderPolygon.NumberOfVertices, textureID, RenderPolygon.TranslucencyMode, FILTERING_BILINEAR_ON, TEXTURE_WRAP)) {
		return;
	}

	for (uint32_t i = 0; i < RenderPolygon.NumberOfVertices; i++)
	{
		RENDERVERTEX *vertices = &renderVerticesPtr[i];

		mainVertex[vb].x = (float)vertices->X;
		mainVertex[vb].y = (float)-vertices->Y;
		mainVertex[vb].z = (float)vertices->Z;

		mainVertex[vb].color = RGBA_MAKE(vertices->R,vertices->G,vertices->B,vertices->A);
		mainVertex[vb].specular = RGBA_MAKE(0,0,0,/*255*/0);

		mainVertex[vb].u = ((float)vertices->U) * RecipW;
		mainVertex[vb].v = ((float)vertices->V) * RecipH;
		vb++;
	}

	mainList->CreateIndices(mainIndex, RenderPolygon.NumberOfVertices);
}

// draws water etc
void D3D_DrawMoltenMetalMesh_Unclipped()
{
	VECTORCH *point = MeshVertex;
	VECTORCH *pointWS = MeshWorldVertex;

	if (!mainList->Check(kNumMeshVerts)) {
		FlushLists();
	}

	if (!mainList->AddItem(kNumMeshVerts, currentWaterTexture, TRANSLUCENCY_NORMAL)) {
		return;
	}

	for (uint32_t i = 0; i < kNumMeshVerts; i++)
	{
		mainVertex[vb].x = (float)point->vx;
		mainVertex[vb].y = (float)-point->vy;
		mainVertex[vb].z = (float)point->vz;

		mainVertex[vb].color = MeshVertexColour[i];
		mainVertex[vb].specular = 0;

		mainVertex[vb].u = pointWS->vx*WaterUScale+(1.0f/256.0f);
		mainVertex[vb].v = pointWS->vy*WaterVScale+(1.0f/256.0f);

		vb++;
		point++;
		pointWS++;
	}

	for (uint32_t x = 0; x < 15; x++)
	{
		for (uint32_t y = 0; y < 15; y++)
		{
			mainList->AddTriangle(mainIndex, 0+x+(16*y),1+x+(16*y),16+x+(16*y), kNumMeshVerts);
			mainList->AddTriangle(mainIndex, 1+x+(16*y),17+x+(16*y),16+x+(16*y), kNumMeshVerts);
		}
	}
}

void ThisFramesRenderingHasBegun()
{
	// calls a function to perform d3d_device->Begin();
	if (R_BeginScene())
	{
		FrameTiming_StartFrame(d3d.lpD3DDevice);
		frameIsOpen = true;
		LockExecuteBuffer(); // lock vertex buffer
	}
}

size_t lastMem = 0;

void ThisFramesRenderingHasFinished()
{
	Osk_Draw();

	Con_Draw();

	if (!frameIsOpen)
	{
		// Nothing can be drawn, but these two lists of what was collected this frame still have to be emptied, or they'd grow for as long as this lasts.
		DrawParticles();
		DrawCoronas();
	}
	else
	{
		frameIsOpen = false;

		ShadowStatus_Draw();

		ToyShadows_NoteFrameLights(toyFrame + 1);  // the pass below is that frame

		UnlockExecuteBufferAndPrepareForUse();
		toyFinalFlush = true;
		ExecuteBuffer();
		toyFinalFlush = false;
		FrameTiming_Mark(kStageAfter);

		{
			ProcessorStageClock clock(kProcessorEndScene);
			R_EndScene();
		}

		FrameTiming_EndFrame();
		ShadowCapture_EndFrame(d3d.lpD3DDevice, toyFrame);
	}

	// The game goes on whether or not this frame could be drawn, so what's below happens either way
	Tex_CheckMemoryUsage();

#ifdef _XBOX
#if 0 // output how much memory is free
	#define MB	(1024*1024)
	MEMORYSTATUS stat;
	char buf[100];

	// Get the memory status.
	GlobalMemoryStatus( &stat );

	if ((stat.dwAvailPhys / MB) < 13)
	{
//		OutputDebugString("break here plz\n");
	}

	if (stat.dwAvailPhys != lastMem)
	{
		sprintf(buf, "%4d  free bytes of physical memory.\n", stat.dwAvailPhys);
		OutputDebugString( buf );

		sprintf(buf, "%4d  free MB of physical memory.\n", stat.dwAvailPhys / MB );
		OutputDebugString( buf );

		lastMem = stat.dwAvailPhys;
	}
#endif
#endif

 	/* KJL 11:46:56 01/16/97 - kill off any lights which are fated to be removed */
	LightBlockDeallocation();
}

extern void D3D_DrawSliderBar(int x, int y, int alpha)
{
	VertexTag quadVertices[4];
	uint32_t sliderHeight = 11;
	uint32_t colour = alpha >> 8;

	if (colour > 255) {
		colour = 255;
	}

	colour = (colour << 24) + 0xffffff;

	quadVertices[0].Y = y;
	quadVertices[1].Y = y;
	quadVertices[2].Y = y + sliderHeight;
	quadVertices[3].Y = y + sliderHeight;

	{
		int topLeftU = 1;
		int topLeftV = 68;

		quadVertices[0].U = topLeftU;
		quadVertices[0].V = topLeftV;
		quadVertices[1].U = topLeftU + 2;
		quadVertices[1].V = topLeftV;
		quadVertices[2].U = topLeftU + 2;
		quadVertices[2].V = topLeftV + sliderHeight;
		quadVertices[3].U = topLeftU;
		quadVertices[3].V = topLeftV + sliderHeight;

		quadVertices[0].X = x;
		quadVertices[3].X = x;
		quadVertices[1].X = x + 2;
		quadVertices[2].X = x + 2;

		D3D_HUDQuad_Output
		(
			HUDFontsImageNumber,
			quadVertices,
			colour,
			FILTERING_BILINEAR_OFF
		);
	}
	{
		int topLeftU = 7;
		int topLeftV = 68;

		quadVertices[0].U = topLeftU;
		quadVertices[0].V = topLeftV;
		quadVertices[1].U = topLeftU + 2;
		quadVertices[1].V = topLeftV;
		quadVertices[2].U = topLeftU + 2;
		quadVertices[2].V = topLeftV + sliderHeight;
		quadVertices[3].U = topLeftU;
		quadVertices[3].V = topLeftV + sliderHeight;

		quadVertices[0].X = x+213+2;
		quadVertices[3].X = x+213+2;
		quadVertices[1].X = x+2 +213+2;
		quadVertices[2].X = x+2 +213+2;

		D3D_HUDQuad_Output
		(
			HUDFontsImageNumber,
			quadVertices,
			colour,
			FILTERING_BILINEAR_OFF
		);
	}
	quadVertices[2].Y = y + 2;
	quadVertices[3].Y = y + 2;

	{
		int topLeftU = 5;
		int topLeftV = 77;

		quadVertices[0].U = topLeftU;
		quadVertices[0].V = topLeftV;
		quadVertices[1].U = topLeftU;
		quadVertices[1].V = topLeftV;
		quadVertices[2].U = topLeftU;
		quadVertices[2].V = topLeftV + 2;
		quadVertices[3].U = topLeftU;
		quadVertices[3].V = topLeftV + 2;

		quadVertices[0].X = x + 2;
		quadVertices[3].X = x + 2;
		quadVertices[1].X = x + 215;
		quadVertices[2].X = x + 215;

		D3D_HUDQuad_Output
		(
			HUDFontsImageNumber,
			quadVertices,
			colour,
			FILTERING_BILINEAR_OFF
		);
	}
	quadVertices[0].Y = y + 9;
	quadVertices[1].Y = y + 9;
	quadVertices[2].Y = y + 11;
	quadVertices[3].Y = y + 11;

	{
		int topLeftU = 5;
		int topLeftV = 77;

		quadVertices[0].U = topLeftU;
		quadVertices[0].V = topLeftV;
		quadVertices[1].U = topLeftU;
		quadVertices[1].V = topLeftV;
		quadVertices[2].U = topLeftU;
		quadVertices[2].V = topLeftV + 2;
		quadVertices[3].U = topLeftU;
		quadVertices[3].V = topLeftV + 2;

		quadVertices[0].X = x + 2;
		quadVertices[3].X = x + 2;
		quadVertices[1].X = x + 215;
		quadVertices[2].X = x + 215;

		D3D_HUDQuad_Output
		(
			HUDFontsImageNumber,
			quadVertices,
			colour,
			FILTERING_BILINEAR_OFF
		);
	}
}

extern void D3D_DrawSlider(int x, int y, int alpha)
{
	// the little thingy that slides through the rectangle
	VertexTag quadVertices[4];
	uint32_t sliderHeight = 5;
	uint32_t colour = alpha >> 8;

	if (colour > 255) {
		colour = 255;
	}

	colour = (colour << 24) + 0xffffff;

	quadVertices[0].Y = y;
	quadVertices[1].Y = y;
	quadVertices[2].Y = y + sliderHeight;
	quadVertices[3].Y = y + sliderHeight;

	{
		int topLeftU = 11;
		int topLeftV = 74;

		quadVertices[0].U = topLeftU;
		quadVertices[0].V = topLeftV;
		quadVertices[1].U = topLeftU + 9;
		quadVertices[1].V = topLeftV;
		quadVertices[2].U = topLeftU + 9;
		quadVertices[2].V = topLeftV + sliderHeight;
		quadVertices[3].U = topLeftU;
		quadVertices[3].V = topLeftV + sliderHeight;

		quadVertices[0].X = x;
		quadVertices[3].X = x;
		quadVertices[1].X = x + 9;
		quadVertices[2].X = x + 9;

		D3D_HUDQuad_Output
		(
			HUDFontsImageNumber,
			quadVertices,
			colour,
			FILTERING_BILINEAR_ON
		);
	}
}

extern void D3D_DrawColourBar(int yTop, int yBottom, int rScale, int gScale, int bScale)
{
	float yTop1    = HPos2DC(yTop);
	float yBottom1 = HPos2DC(yBottom);

	// this function draws a lot of triangles so we'll force a buffer flush so we can keen
	// the size of the ortho buffer small
	FlushLists();

	for (uint32_t i = 0; i < 255;)
	{
		if (!orthoList->AddItem(4, NO_TEXTURE, TRANSLUCENCY_OFF, FILTERING_BILINEAR_ON, TEXTURE_CLAMP)) {
			return;
		}

		unsigned int colour = 0;
		unsigned int c = 0;

		c = GammaValues[i];

		// set alpha to 255 otherwise d3d alpha test stops pixels being rendered
		colour = RGBA_MAKE(MUL_FIXED(c,rScale), MUL_FIXED(c,gScale), MUL_FIXED(c,bScale),255);

		float x = WPos2DC((Global_VDB_Ptr->VDB_ClipRight*i)/255);

		uint32_t offs = orthoVBOffset;

		// top left
		orthoVertex[offs+1].x = x;
		orthoVertex[offs+1].y = yTop1;
		orthoVertex[offs+1].z = 1.0f;
		orthoVertex[offs+1].colour = colour;
		orthoVertex[offs+1].u = 0.0f;
		orthoVertex[offs+1].v = 0.0f;

		// bottom left
		orthoVertex[offs].x = x;
		orthoVertex[offs].y = yBottom1;
		orthoVertex[offs].z = 1.0f;
		orthoVertex[offs].colour = colour;
		orthoVertex[offs].u = 0.0f;
		orthoVertex[offs].v = 0.0f;

		i++;
		c = GammaValues[i];
		colour = RGBA_MAKE(MUL_FIXED(c,rScale),MUL_FIXED(c,gScale),MUL_FIXED(c,bScale),255);

		x = WPos2DC((Global_VDB_Ptr->VDB_ClipRight*i)/255);

		// bottom right
		orthoVertex[offs+2].x = x;
		orthoVertex[offs+2].y = yBottom1;
		orthoVertex[offs+2].z = 1.0f;
		orthoVertex[offs+2].colour = colour;
		orthoVertex[offs+2].u = 0.0f;
		orthoVertex[offs+2].v = 0.0f;

		// top right
		orthoVertex[offs+3].x = x;
		orthoVertex[offs+3].y = yTop1;
		orthoVertex[offs+3].z = 1.0f;
		orthoVertex[offs+3].colour = colour;
		orthoVertex[offs+3].u = 0.0f;
		orthoVertex[offs+3].v = 0.0f;

		orthoVBOffset += 4;

		orthoList->CreateOrthoIndices(orthoIndex);
	}
}


extern void D3D_FadeDownScreen(int brightness, int colour)
{
	int t = 255 - (brightness>>8);
	if (t < 0)
	{
		t = 0;
	}

	if (!orthoList->AddItem(4, NO_TEXTURE, TRANSLUCENCY_NORMAL, FILTERING_BILINEAR_ON, TEXTURE_WRAP)) {
		return;
	}

	// bottom left
	orthoVertex[orthoVBOffset].x = -1.0f;
	orthoVertex[orthoVBOffset].y = 1.0f;
	orthoVertex[orthoVBOffset].z = 1.0f;
	orthoVertex[orthoVBOffset].colour = (t<<24)+colour;
	orthoVertex[orthoVBOffset].u = 0.0f;
	orthoVertex[orthoVBOffset].v = 0.0f;
	orthoVBOffset++;

	// top left
	orthoVertex[orthoVBOffset].x = -1.0f;
	orthoVertex[orthoVBOffset].y = -1.0f;
	orthoVertex[orthoVBOffset].z = 1.0f;
	orthoVertex[orthoVBOffset].colour = (t<<24)+colour;
	orthoVertex[orthoVBOffset].u = 0.0f;
	orthoVertex[orthoVBOffset].v = 0.0f;
	orthoVBOffset++;

	// bottom right
	orthoVertex[orthoVBOffset].x = 1.0f;
	orthoVertex[orthoVBOffset].y = 1.0f;
	orthoVertex[orthoVBOffset].z = 1.0f;
	orthoVertex[orthoVBOffset].colour = (t<<24)+colour;
	orthoVertex[orthoVBOffset].u = 0.0f;
	orthoVertex[orthoVBOffset].v = 0.0f;
	orthoVBOffset++;

	// top right
	orthoVertex[orthoVBOffset].x = 1.0f;;
	orthoVertex[orthoVBOffset].y = -1.0f;
	orthoVertex[orthoVBOffset].z = 1.0f;
	orthoVertex[orthoVBOffset].colour = (t<<24)+colour;
	orthoVertex[orthoVBOffset].u = 0.0f;
	orthoVertex[orthoVBOffset].v = 0.0f;
	orthoVBOffset++;

	orthoList->CreateOrthoIndices(orthoIndex);
}

extern void D3D_PlayerOnFireOverlay(void)
{
	int t = 128;
	int colour = (FMVParticleColour&0xffffff)+(t<<24);

	float u = (FastRandom()&255)/256.0f;
	float v = (FastRandom()&255)/256.0f;

	if (!orthoList->AddItem(4, BurningImageNumber, TRANSLUCENCY_GLOWING, FILTERING_BILINEAR_ON, TEXTURE_WRAP)) {
		return;
	}

	// bottom left
	orthoVertex[orthoVBOffset].x = -1.0f;
	orthoVertex[orthoVBOffset].y = 1.0f;
	orthoVertex[orthoVBOffset].z = 1.0f;
	orthoVertex[orthoVBOffset].colour = colour;
	orthoVertex[orthoVBOffset].u = u;
	orthoVertex[orthoVBOffset].v = v+1.0f;
	orthoVBOffset++;

	// top left
	orthoVertex[orthoVBOffset].x = -1.0f;
	orthoVertex[orthoVBOffset].y = -1.0f;
	orthoVertex[orthoVBOffset].z = 1.0f;
	orthoVertex[orthoVBOffset].colour = colour;
	orthoVertex[orthoVBOffset].u = u;
	orthoVertex[orthoVBOffset].v = v;
	orthoVBOffset++;

	// bottom right
	orthoVertex[orthoVBOffset].x = 1.0f;
	orthoVertex[orthoVBOffset].y = 1.0f;
	orthoVertex[orthoVBOffset].z = 1.0f;
	orthoVertex[orthoVBOffset].colour = colour;
	orthoVertex[orthoVBOffset].u = u+1.0f;
	orthoVertex[orthoVBOffset].v = v+1.0f;
	orthoVBOffset++;

	// top right
	orthoVertex[orthoVBOffset].x = 1.0f;;
	orthoVertex[orthoVBOffset].y = -1.0f;
	orthoVertex[orthoVBOffset].z = 1.0f;
	orthoVertex[orthoVBOffset].colour = colour;
	orthoVertex[orthoVBOffset].u = u+1.0f;
	orthoVertex[orthoVBOffset].v = v;
	orthoVBOffset++;

	orthoList->CreateOrthoIndices(orthoIndex);
}

extern void D3D_ScreenInversionOverlay()
{
	// alien overlay
	int theta[2];
	int colour = 0xffffffff;

	// It's two rectangles that only look right together, so if both won't fit, neither is drawn
	if (!orthoList->Check(8, 12))
	{
		orthoList->NoteLeftOutBecauseFull();
		return;
	}

	theta[0] = (CloakingPhase/8)&4095;
	theta[1] = (800-CloakingPhase/8)&4095;

	if (!orthoList->AddItem(4, SpecialFXImageNumber, TRANSLUCENCY_DARKENINGCOLOUR, FILTERING_BILINEAR_ON, TEXTURE_WRAP)) {
		return;
	}

	for (uint32_t i = 0; i < 2; i++)
	{
		float sin = (GetSin(theta[i]))/65536.0f/16.0f;
		float cos = (GetCos(theta[i]))/65536.0f/16.0f;
		{
			// bottom left
			orthoVertex[orthoVBOffset].x = -1.0f;
			orthoVertex[orthoVBOffset].y = 1.0f;
			orthoVertex[orthoVBOffset].z = 1.0f;
			orthoVertex[orthoVBOffset].colour = colour;
			orthoVertex[orthoVBOffset].u = 0.375f + (cos*(-1) - sin*(+1));
			orthoVertex[orthoVBOffset].v = 0.375f + (sin*(-1) + cos*(+1));
			orthoVBOffset++;

			// top left
			orthoVertex[orthoVBOffset].x = -1.0f;
			orthoVertex[orthoVBOffset].y = -1.0f;
			orthoVertex[orthoVBOffset].z = 1.0f;
			orthoVertex[orthoVBOffset].colour = colour;
			orthoVertex[orthoVBOffset].u = 0.375f + (cos*(-1) - sin*(-1));
			orthoVertex[orthoVBOffset].v = 0.375f + (sin*(-1) + cos*(-1));
			orthoVBOffset++;

			// bottom right
			orthoVertex[orthoVBOffset].x = 1.0f;
			orthoVertex[orthoVBOffset].y = 1.0f;
			orthoVertex[orthoVBOffset].z = 1.0f;
			orthoVertex[orthoVBOffset].colour = colour;
			orthoVertex[orthoVBOffset].u = 0.375f + (cos*(+1) - sin*(+1));
			orthoVertex[orthoVBOffset].v = 0.375f + (sin*(+1) + cos*(+1));
			orthoVBOffset++;

			// top right
			orthoVertex[orthoVBOffset].x = 1.0f;;
			orthoVertex[orthoVBOffset].y = -1.0f;
			orthoVertex[orthoVBOffset].z = 1.0f;
			orthoVertex[orthoVBOffset].colour = colour;
			orthoVertex[orthoVBOffset].u = 0.375f + (cos*(+1) - sin*(-1));
			orthoVertex[orthoVBOffset].v = 0.375f + (sin*(+1) + cos*(-1));
			orthoVBOffset++;

			orthoList->CreateOrthoIndices(orthoIndex);
		}

		// only do this when finishing first loop, otherwise we reserve space for 4 verts we never add
		if (i == 0)
		{
			if (!orthoList->AddItem(4, SpecialFXImageNumber, TRANSLUCENCY_COLOUR, FILTERING_BILINEAR_ON, TEXTURE_WRAP)) {
				return;
			}
		}
	}
}

extern void D3D_PredatorScreenInversionOverlay()
{
	uint32_t colour = 0xffffffff;

	if (!orthoList->AddItem(4, NO_TEXTURE, TRANSLUCENCY_DARKENINGCOLOUR, FILTERING_BILINEAR_ON, TEXTURE_WRAP)) {
		return;
	}

	// bottom left
	orthoVertex[orthoVBOffset].x = -1.0f;
	orthoVertex[orthoVBOffset].y = 1.0f;
	orthoVertex[orthoVBOffset].z = 1.0f;
	orthoVertex[orthoVBOffset].colour = colour;
	orthoVertex[orthoVBOffset].u = 0.0f;
	orthoVertex[orthoVBOffset].v = 0.0f;
	orthoVBOffset++;

	// top left
	orthoVertex[orthoVBOffset].x = -1.0f;
	orthoVertex[orthoVBOffset].y = -1.0f;
	orthoVertex[orthoVBOffset].z = 1.0f;
	orthoVertex[orthoVBOffset].colour = colour;
	orthoVertex[orthoVBOffset].u = 0.0f;
	orthoVertex[orthoVBOffset].v = 0.0f;
	orthoVBOffset++;

	// bottom right
	orthoVertex[orthoVBOffset].x = 1.0f;
	orthoVertex[orthoVBOffset].y = 1.0f;
	orthoVertex[orthoVBOffset].z = 1.0f;
	orthoVertex[orthoVBOffset].colour = colour;
	orthoVertex[orthoVBOffset].u = 0.0f;
	orthoVertex[orthoVBOffset].v = 0.0f;
	orthoVBOffset++;

	// top right
	orthoVertex[orthoVBOffset].x = 1.0f;
	orthoVertex[orthoVBOffset].y = -1.0f;
	orthoVertex[orthoVBOffset].z = 1.0f;
	orthoVertex[orthoVBOffset].colour = colour;
	orthoVertex[orthoVBOffset].u = 0.0f;
	orthoVertex[orthoVBOffset].v = 0.0f;
	orthoVBOffset++;

	orthoList->CreateOrthoIndices(orthoIndex);
}

extern void D3D_PlayerDamagedOverlay(int intensity)
{
	int theta[2];
	int colour, baseColour;

	// It's two rectangles that only look right together, so if both won't fit, neither is drawn
	if (!orthoList->Check(8, 12))
	{
		orthoList->NoteLeftOutBecauseFull();
		return;
	}

	theta[0] = (CloakingPhase/8)&4095;
	theta[1] = (800-CloakingPhase/8)&4095;

	switch (AvP.PlayerType)
	{
		default:
			LOCALASSERT(0);
			/* if no debug then fall through to marine */
		case I_Marine:
			baseColour = 0xff0000;
			break;

		case I_Alien:
			baseColour = 0xffff00;
			break;

		case I_Predator:
			baseColour = 0x00ff00;
			break;
	}

	colour = 0xffffff - baseColour + (intensity<<24);

	if (!orthoList->AddItem(4, SpecialFXImageNumber, TRANSLUCENCY_INVCOLOUR, FILTERING_BILINEAR_ON, TEXTURE_WRAP)) {
		return;
	}

	for (uint32_t i = 0; i < 2; i++)
	{
		float sin = (GetSin(theta[i]))/65536.0f/16.0f;
		float cos = (GetCos(theta[i]))/65536.0f/16.0f;
		{
			// bottom left
			orthoVertex[orthoVBOffset].x = -1.0f;
			orthoVertex[orthoVBOffset].y = 1.0f;
			orthoVertex[orthoVBOffset].z = 1.0f;
			orthoVertex[orthoVBOffset].colour = colour;
			orthoVertex[orthoVBOffset].u = (float)(0.875f + (cos*(-1) - sin*(+1)));
			orthoVertex[orthoVBOffset].v = (float)(0.375f + (sin*(-1) + cos*(+1)));
			orthoVBOffset++;

			// top left
			orthoVertex[orthoVBOffset].x = -1.0f;
			orthoVertex[orthoVBOffset].y = -1.0f;
			orthoVertex[orthoVBOffset].z = 1.0f;
			orthoVertex[orthoVBOffset].colour = colour;
			orthoVertex[orthoVBOffset].u = (float)(0.875f + (cos*(-1) - sin*(-1)));
			orthoVertex[orthoVBOffset].v = (float)(0.375f + (sin*(-1) + cos*(-1)));
			orthoVBOffset++;

			// bottom right
			orthoVertex[orthoVBOffset].x = 1.0f;
			orthoVertex[orthoVBOffset].y = 1.0f;
			orthoVertex[orthoVBOffset].z = 1.0f;
			orthoVertex[orthoVBOffset].colour = colour;
			orthoVertex[orthoVBOffset].u = (float)(0.875f + (cos*(+1) - sin*(+1)));
			orthoVertex[orthoVBOffset].v = (float)(0.375f + (sin*(+1) + cos*(+1)));
			orthoVBOffset++;

			// top right
			orthoVertex[orthoVBOffset].x = 1.0f;
			orthoVertex[orthoVBOffset].y = -1.0f;
			orthoVertex[orthoVBOffset].z = 1.0f;
			orthoVertex[orthoVBOffset].colour = colour;
			orthoVertex[orthoVBOffset].u = (float)(0.875f + (cos*(+1) - sin*(-1)));
			orthoVertex[orthoVBOffset].v = (float)(0.375f + (sin*(+1) + cos*(-1)));
			orthoVBOffset++;

			orthoList->CreateOrthoIndices(orthoIndex);
		}

		colour = baseColour +(intensity<<24);

		// only do this when finishing first loop, otherwise we reserve space for 4 verts we never add
		if (i == 0)
		{
			if (!orthoList->AddItem(4, SpecialFXImageNumber, TRANSLUCENCY_GLOWING, FILTERING_BILINEAR_ON, TEXTURE_WRAP)) {
				return;
			}
		}
	}
}

// D3D_DrawCable - draws predator grappling hook
void D3D_DrawCable(VECTORCH *centrePtr, MATRIXCH *orientationPtr)
{
	// TODO - not disabling zwrites. probably need to do this but double check (be handy if we didn't have to)

	currentWaterTexture = NO_TEXTURE;

	MeshXScale = 4096 / 16;
	MeshZScale = 4096 / 16;

	for (int field = 0; field < 3; field++)
	{
		int i = 0;
		for (int x = (0 + field*15); x < (16 + field*15); x++)
		{
			for (int z = 0; z < 16; z++)
			{
				VECTORCH *point = &MeshVertex[i];

				int innerRadius = 20;
				VECTORCH radius;
				int theta = ((4096*z) / 15) & 4095;
				int rOffset = GetSin((x*64 + theta/32 - CloakingPhase) & 4095);
				rOffset = MUL_FIXED(rOffset, rOffset) / 512;

				radius.vx = MUL_FIXED(innerRadius + rOffset/8, GetSin(theta));
				radius.vy = MUL_FIXED(innerRadius + rOffset/8, GetCos(theta));
				radius.vz = 0;

				RotateVector(&radius, orientationPtr);

				point->vx = centrePtr[x].vx + radius.vx;
				point->vy = centrePtr[x].vy + radius.vy;
				point->vz = centrePtr[x].vz + radius.vz;

				MeshVertexColour[i] = RGBA_MAKE(0, rOffset, 255, 128);

				TranslatePointIntoViewspace(point);

				i++;
			}
		}

		D3D_DrawMoltenMetalMesh_Unclipped();
	}
}

void r2rect :: AlphaFill
(
	unsigned char R,
	unsigned char G,
	unsigned char B,
	unsigned char translucency
) const
{
	GLOBALASSERT
	(
		bValidPhys()
	);

	if (y1 <= y0)
	{
		return;
	}

	D3D_Rectangle(x0, y0, x1, y1, R, G, B, translucency);
}

extern void D3D_RenderHUDNumber_Centred(uint32_t number, uint32_t x, uint32_t y, uint32_t colour)
{
	// green and red ammo numbers
	VertexTag quadVertices[4];
	int noOfDigits = 3;
	int h = MUL_FIXED(HUDScaleFactor, HUD_DIGITAL_NUMBERS_HEIGHT);
	int w = MUL_FIXED(HUDScaleFactor, HUD_DIGITAL_NUMBERS_WIDTH);

	quadVertices[0].Y = y;
	quadVertices[1].Y = y;
	quadVertices[2].Y = y + h;
	quadVertices[3].Y = y + h;

	x += (3*w)/2;

	do
	{
		int digit = number%10;
		number/=10;
		{
			int topLeftU;
			int topLeftV;
			if (digit < 8)
			{
				topLeftU = 1+(digit)*16;
				topLeftV = 1;
			}
			else
			{
				topLeftU = 1+(digit-8)*16;
				topLeftV = 1+24;
			}
			if (AvP.PlayerType == I_Marine) topLeftV+=80;

			quadVertices[0].U = topLeftU;
			quadVertices[0].V = topLeftV;
			quadVertices[1].U = topLeftU + HUD_DIGITAL_NUMBERS_WIDTH;
			quadVertices[1].V = topLeftV;
			quadVertices[2].U = topLeftU + HUD_DIGITAL_NUMBERS_WIDTH;
			quadVertices[2].V = topLeftV + HUD_DIGITAL_NUMBERS_HEIGHT;
			quadVertices[3].U = topLeftU;
			quadVertices[3].V = topLeftV + HUD_DIGITAL_NUMBERS_HEIGHT;

			x -= 1+w;
			quadVertices[0].X = x;
			quadVertices[3].X = x;
			quadVertices[1].X = x + w;
			quadVertices[2].X = x + w;

			D3D_HUDQuad_Output(HUDFontsImageNumber, quadVertices, colour, FILTERING_BILINEAR_OFF);
		}
	}
	while (--noOfDigits);
}

extern void D3D_RenderHUDString(char *stringPtr, int x, int y, int colour)
{
	// mission briefing text?
	if (stringPtr == NULL) {
		return;
	}

	VertexTag quadVertices[4];

	quadVertices[0].Y = y - 1;
	quadVertices[1].Y = y - 1;
	quadVertices[2].Y = y + HUD_FONT_HEIGHT + 1;
	quadVertices[3].Y = y + HUD_FONT_HEIGHT + 1;

	while (*stringPtr)
	{
		char c = *stringPtr++;
		{
			int topLeftU = 1+((c-32)&15)*16;
			int topLeftV = 1+((c-32)>>4)*16;

			quadVertices[0].U = topLeftU - 1;
			quadVertices[0].V = topLeftV - 1;
			quadVertices[1].U = topLeftU + HUD_FONT_WIDTH + 1;
			quadVertices[1].V = topLeftV - 1;
			quadVertices[2].U = topLeftU + HUD_FONT_WIDTH  + 1;
			quadVertices[2].V = topLeftV + HUD_FONT_HEIGHT + 1;
			quadVertices[3].U = topLeftU - 1;
			quadVertices[3].V = topLeftV + HUD_FONT_HEIGHT + 1;

			quadVertices[0].X = x - 1;
			quadVertices[3].X = x - 1;
			quadVertices[1].X = x + HUD_FONT_WIDTH + 1;
			quadVertices[2].X = x + HUD_FONT_WIDTH + 1;

			D3D_HUDQuad_Output(AAFontImageNumber, quadVertices, colour, FILTERING_BILINEAR_OFF);
		}
		x += AAFontWidths[(unsigned char)c];
	}
}

extern void D3D_RenderHUDString_Clipped(char *stringPtr, int x, int y, int colour)
{
	if (stringPtr == NULL) {
		return;
	}

	VertexTag quadVertices[4];

	LOCALASSERT(y<=0);

	quadVertices[2].Y = y + HUD_FONT_HEIGHT + 1;
	quadVertices[3].Y = y + HUD_FONT_HEIGHT + 1;

	quadVertices[0].Y = 0;
	quadVertices[1].Y = 0;

	while (*stringPtr)
	{
		char c = *stringPtr++;
		{
			int topLeftU = 1+((c-32)&15)*16;
			int topLeftV = 1+((c-32)>>4)*16;

			// top left
			quadVertices[0].U = topLeftU - 1;
			quadVertices[0].V = topLeftV - y;

			// top right
			quadVertices[1].U = topLeftU + HUD_FONT_WIDTH + 1;
			quadVertices[1].V = topLeftV - y;

			// bottom right
			quadVertices[2].U = topLeftU + HUD_FONT_WIDTH + 1;
			quadVertices[2].V = topLeftV + HUD_FONT_HEIGHT + 1;

			// bottom left
			quadVertices[3].U = topLeftU - 1;
			quadVertices[3].V = topLeftV + HUD_FONT_HEIGHT + 1;

			quadVertices[0].X = x - 1;
			quadVertices[3].X = x - 1;
			quadVertices[1].X = x + HUD_FONT_WIDTH + 1;
			quadVertices[2].X = x + HUD_FONT_WIDTH + 1;

			D3D_HUDQuad_Output(AAFontImageNumber, quadVertices, colour, FILTERING_BILINEAR_OFF);
		}
		x += AAFontWidths[(unsigned char)c];
	}
}

void D3D_RenderHUDString_Centred(char *stringPtr, uint32_t centreX, uint32_t y, uint32_t colour)
{
	// white text only of marine HUD ammo, health numbers etc
	if (stringPtr == NULL) {
		return;
	}

	int length = 0;
	char *ptr = stringPtr;

	while (*ptr)
	{
		length += AAFontWidths[(uint8_t)*ptr++];
	}

	length = MUL_FIXED(HUDScaleFactor, length);

	int x = centreX-length/2;

	VertexTag quadVertices[4];

	quadVertices[0].Y = y - MUL_FIXED(HUDScaleFactor, 1);
	quadVertices[1].Y = y - MUL_FIXED(HUDScaleFactor, 1);
	quadVertices[2].Y = y + MUL_FIXED(HUDScaleFactor, HUD_FONT_HEIGHT + 1);
	quadVertices[3].Y = y + MUL_FIXED(HUDScaleFactor, HUD_FONT_HEIGHT + 1);

	while (*stringPtr)
	{
		char c = *stringPtr++;
		{
			int topLeftU = 1+((c-32)&15)*16;
			int topLeftV = 1+((c-32)>>4)*16;

			// top left
			quadVertices[0].U = topLeftU - 1;
			quadVertices[0].V = topLeftV;// - 1;

			// top right
			quadVertices[1].U = topLeftU + HUD_FONT_WIDTH + 1;
			quadVertices[1].V = topLeftV;// - 1;

			// bottom right
			quadVertices[2].U = topLeftU + HUD_FONT_WIDTH + 1;
			quadVertices[2].V = topLeftV + HUD_FONT_HEIGHT + 1;

			// bottom left
			quadVertices[3].U = topLeftU - 1;
			quadVertices[3].V = topLeftV + HUD_FONT_HEIGHT + 1;

			quadVertices[0].X = x - MUL_FIXED(HUDScaleFactor, 1);
			quadVertices[3].X = x - MUL_FIXED(HUDScaleFactor, 1);
			quadVertices[1].X = x + MUL_FIXED(HUDScaleFactor, HUD_FONT_WIDTH + 1);
			quadVertices[2].X = x + MUL_FIXED(HUDScaleFactor, HUD_FONT_WIDTH + 1);

			D3D_HUDQuad_Output(AAFontImageNumber, quadVertices, colour, FILTERING_BILINEAR_ON);
		}
		x += MUL_FIXED(HUDScaleFactor, AAFontWidths[(unsigned char)c]);
	}
}

extern void RenderString(char *stringPtr, int x, int y, int colour)
{
	D3D_RenderHUDString(stringPtr, x, y, colour);
}

extern void RenderStringCentred(char *stringPtr, int centreX, int y, int colour)
{
	int length = 0;
	char *ptr = stringPtr;

	while (*ptr)
	{
		length += AAFontWidths[(uint8_t)*ptr++];
	}

	D3D_RenderHUDString(stringPtr, centreX-length/2, y, colour);
}

extern void RenderStringVertically(char *stringPtr, int centreX, int bottomY, int colour)
{
	VertexTag quadVertices[4];
	int y = bottomY;

	quadVertices[0].X = centreX - (HUD_FONT_HEIGHT/2) - 1;
	quadVertices[1].X = quadVertices[0].X;
	quadVertices[2].X = quadVertices[0].X+2+HUD_FONT_HEIGHT*1;
	quadVertices[3].X = quadVertices[2].X;

	int width = (centreX - (HUD_FONT_HEIGHT/2) - 1) + 2+HUD_FONT_HEIGHT*1;

	while (*stringPtr)
	{
		char c = *stringPtr++;
		{
			int topLeftU = 1+((c-32)&15)*16;
			int topLeftV = 1+((c-32)>>4)*16;

			// top left
			quadVertices[0].U = topLeftU - 1;
			quadVertices[0].V = topLeftV - 1;

			// top right
			quadVertices[1].U = topLeftU + HUD_FONT_WIDTH;
			quadVertices[1].V = topLeftV - 1;

			// bottom right
			quadVertices[2].U = topLeftU + HUD_FONT_WIDTH;
			quadVertices[2].V = topLeftV + HUD_FONT_HEIGHT + 1;

			// bottom left
			quadVertices[3].U = topLeftU - 1;
			quadVertices[3].V = topLeftV + HUD_FONT_HEIGHT + 1;

			quadVertices[0].Y = y;
			quadVertices[1].Y = y - HUD_FONT_WIDTH*1 -1;
			quadVertices[2].Y = y - HUD_FONT_WIDTH*1 -1;
			quadVertices[3].Y = y;

			int height = y - (y - HUD_FONT_WIDTH*1 -1);

			D3D_HUDQuad_Output(AAFontImageNumber, quadVertices, colour, FILTERING_BILINEAR_OFF);

		}
		y -= AAFontWidths[(unsigned char)c];
	}
}


// ---- toy-shadows ------------------------------------------------------------------------------------------------
// Stencil shadow volumes between lights and the level. The polygons of every module drawn this frame go in one list. Then, for
// each of the frame's chosen lights, each polygon in that light's reach gets a closed prism running away from the light to
// the end of its range. The stencil counts which pixels are inside a prism, and those pixels are darkened.
// Each prism is closed, so z-fail counts come out right with no edge data, and neighbours' shared sides cancel out.

// What the shadows show. G steps through these while playing, so they can be compared live. view_switches.h says how this goes with LIGHTING and SHADOWS.
enum ToyShadowsView
{
	kToyViewOff,
	kToyViewFinal,      // the real thing
	kToyViewEdges,      // each light's volumes as lines, in that light's colour
	kToyViewPolygons,   // each light's shadowed pixels filled in that light's colour, blended so overlaps show
	kViewRedVolumes,    // every light's shadowed pixels filled in red, and every volume drawn over them as red lines
	kToyViewMask,       // just what final multiplies the picture by: white is untouched, black is the full depth of shadow
	kViewNormals,       // which way each pixel faces, from the surface pictures
	kToyViewCount
};

static char const * const kToyViewNames[kToyViewCount] = { "OFF", "FINAL", "EDGES", "POLYGONS", "RED VOLUMES", "MASK", "NORMALS" };
static DWORD const kToyLightColours[kToyMostLights] = { 0xFF0000, 0x00FF00, 0x0060FF, 0xFFFF00, 0xFF00FF, 0x00FFFF, 0xFF8000, 0xFFFFFF, 0xFF0080, 0x80FF00, 0x00FF80, 0x8080FF };

extern void NewOnScreenMessage(char *messagePtr);

int ToyShadowsMode = kToyViewFinal;
int LightingNow = kLightingRelief;
int ShadowVolumesOn = 1;
int ToyShadowsMaxLights = 8;  // -shadowlights=N, 1 to 8: how many steady lights
int ToyShadowsMaxBrief = 4;   // -shadowbrief=N, 0 to 4: how many brief lights on top of those, such as muzzle flashes and explosions
int ToyShadowsDarkness = 85;  // -shadowdark=N, 0 to 99: how much of a pixel goes when no light reaches it. What's left is the glow
int ToyShadowsFlareReach = 100; // -shadowflarereach=N, 50 to 400: how far a flare's light goes, in percent of the game's 10 metres
int ToyShadowsFlareCap = 150;  // -shadowflare=N, 10 to 800: the brightest a flare's light gets, in percent. See ToyShadows_ChooseLights
int ToyShadowsLightsByPixel = 0;   // set each frame, for the next one: chosen live lights leave the game's lighting, and are added per pixel here
int ToyShadowsWorld = 70;     // -shadowworld=N, 10 to 100: how bright the whole world is, in percent, so that lit places stand out
int ToyShadowsSoft = 12;      // -shadowsoft=N, 0 to 32: how wide the soft edge of a shadow is, in pixels, up close. 0 is hard
int ToyShadowsModelGap = 60;  // -shadowmodelgap=N, 8 to 200: the furthest behind a model's polygon its shadow starts
int ShadowsDepthGap = 0;      // -shadowdepthgap: a shadow also starts further behind its polygon as depth gets coarser. Only on prisms made each frame in the view's numbers: not kept prisms, and not the walls -staticscene walks. The tag says PART if some can be those
float ShadowSurfaceBias = -0.000001f;   // -shadowbiassteps=N: how much nearer the surface pictures draw, in the depth buffer's own numbers
int ShadowBiasSteps = 0;      // what -shadowbiassteps asked for, for the tag, or 0
int ShadowsEvenGap = 0;       // -shadowevengap: a shadow starts the same distance behind every part of its polygon. See shadow_gap.h
// Whether every prism is worked out every frame, as if nothing could be kept. This is the default for now, because the user saw shadows flicker with
// kept prisms and not without them. -keptprismson turns them back on for testing. It doesn't change the aim, which is that a still light keeps its
// volumes: this goes back once a fix is proven and the user has seen it.
int KeptPrismsOff = 1;
int ToyShadowsTest = 0;       // -shadowtest: a small box round each light, so everything in its range must go dark
int ToyShadowsPositions = 4;   // -shadowpositions=N, 1, 4 or 8: how many places a light is treated as shining from. 1 is a point of light, with no penumbra
int ToyShadowsNearLights = 2;  // -shadownearlights=N, 0 to 12: how many lights besides flares get those positions, nearest first
int ToyShadowsLampSize = 300;  // -shadowlampsize=N, 0 to 2000: how wide a lamp or other light is, in the game's units. The levels don't say
int ToyShadowsFlareSize = 200; // -shadowflaresize=N, 0 to 2000: how wide a flare's light is
int ToyShadowsCasters = 7;    // -shadowcasters=N, 0 to 7: what may cast and take shadows. Add up 1 for the level, 2 for objects and 4 for models, so that one run can compare what each costs
int ToyShadowsInMirror = 0;   // set by kzsort.cpp while a body part is drawn a second time, as its reflection in a mirror

struct ToyVec
{
	float x, y, z;
};

// A polygon's flat surface, which is all the prism builder asks of it: which way it faces, and which side of it a place is on. Working that
// out takes a square root, and there can be thousands of polygons, several lights and several positions for each light. So no light ever
// works it out. A level's polygon keeps its surface in its shape, made once at load, and that's in the world's numbers. Anything else works it
// out from its corners once a frame, when it's collected, and that's in the numbers of the light list. Every light reads the answer.
// It's never the normal the level file stores for a polygon. A prism's inside and outside are decided by the order of the corners, and on
// 1,688 stock polygons that normal points the other way from that order, mostly on models.
struct ToyFlatSurface
{
	float nx, ny, nz;   // an arrow straight through the surface, 1 long. All zero means the polygon has no area, and casts nothing
	float distance;     // how far the surface is from the middle of its numbers, along that arrow. See ToyDistanceFrom
	float halfSize;     // half the longer of its first two sides, which says how big the polygon is. Only a model's is kept
	bool inWorld;       // true if these are the world's numbers, and false if they're the light list's
	bool keepsStartGap; // true if its shadow starts at the ordinary distance even with its front turned from the light. See ToyAddPrism
};

// How far a place is from a flat surface, and the sign says which side it's on. The place has to be in the surface's own numbers.
static inline float ToyDistanceFrom(const ToyFlatSurface &surface, const float *place)
{
	return surface.nx * place[0] + surface.ny * place[1] + surface.nz * place[2] - surface.distance;
}

// Is a place on the back of a polygon? distance is what ToyDistanceFrom gave for it. A polygon's arrow points out of its back, and
// surface_normals.cpp says why. The mirror cheat turns the picture over once more, so the corners on the list stop being a mirror image of the
// shape, and an arrow worked out from them points out of the front instead. Stored arrows aren't used under that cheat (ToyStoredPlanes).
static inline bool IsBehindPolygon(float distance)
{
	return MIRROR_CHEATMODE ? distance < 0.0f : distance > 0.0f;
}

static const int   kToyMostPolyPoints  = 16;
static const int   kToyMaxCasters      = 60000;
static const int   kToyMaxCasterPoints = 240000;
static const int   kToyMaxVerts        = 1200000;
static const float kToyStartGap        = 8.0f;      // start just behind the polygon, or surfaces shadow themselves in speckles
static const float kToyWidestGap       = 200.0f;    // the most that gap can stretch to, at a very low angle
// How far past a polygon its shadow starts when the polygon's front is turned from the light. It can be this small because nothing on that
// polygon is lit by that light, so there's nothing there to shadow. It can't be less: the game cuts a corner down to whole numbers, which can
// move it by up to 1.73, and the start still has to be past the surface after that.
static const float kUnlitStartGap      = 2.0f;

// How many of the depth buffer's steps a shadow's start stands behind its polygon, with -shadowdepthgap. The depth buffer holds one over the distance, so
// it gets coarser with the square of how far away something is: one step is 0.09 units at 10 metres, 3.4 at 60 and 9.3 at 100. Past about 40 metres, 8
// units is only a few steps, and a surface and the start of its own shadow can come out at the same depth, and swap as the camera moves. 4 steps stays
// under about half a pixel on screen at 100 metres, so it shouldn't change how anything looks.
static const float kDepthStepsBehind   = 4.0f;

// The size of one step of a 24-bit depth buffer, at this far from the camera.
static float DepthStepAt(float z)
{
	return z * z / (-d3d.matProjection._43 * 16777216.0f);
}
static const float kToyMostReach       = 400000.0f; // how far a corner can be thrown, also well inside the far plane
static const float kToyFlareTouching   = 40.0f;     // a flare this close to a polygon's flat surface is lying on it
static const float kToyCloakedShadow   = 0.25f;     // how strong a cloaked Predator's shadow is, against an ordinary one
static const float kToySoftStaysWideTo = 6000.0f;   // how far away a shadow's soft edge stays at its full width, before it narrows
static const float kToyFullDarkness    = 0.44f;     // a pixel every chosen light fails to reach loses this much of its brightness

static ToyVec toyCasterPoints[kToyMaxCasterPoints];
static int toyCasterStart[kToyMaxCasters];
static int toyCasterCount[kToyMaxCasters];
static ToyFlatSurface toyCasterSurface[kToyMaxCasters];
static float toyCameraInWorld[3];   // where the camera is, in the world's numbers
static const float kCameraOnTheList[3] = { 0.0f, 0.0f, 0.0f };   // and in the light list's numbers it's always here
// For a capture: which light's prisms are being built now, or -1, and what each light has cost this frame, added up over its passes.
static int captureLightBeingBuilt = -1;
static int captureCornersSent[kToyMostLights];
static int captureCornersKept[kToyMostLights];
static int keptGroupsMadeThisFrame = 0;

int ShadowFlickerOn = 0;    // -shadowflicker: see shadow_debug.cpp
int DevMode = 0;            // -dev: the developer tools. Without it the tag doesn't show, and G, L, H, F7, F8 and the shoot tool's keys do nothing
int ShadowStatusOn = 1;     // -statusoff, or F7: the tag on the top right of the screen

static int keptCornersThisPass = 0;          // how many corners of kept prisms the pass being drawn uses
static bool prismsForPictures = false;   // true if this frame draws the surface pictures, and false if it takes the flat shadows
static bool casterDrawnOneSided[kToyMaxCasters];  // only on a report frame: true for a polygon the game draws and hides the back of. See CheckNoPolygonSeenFromBehind

// All the polygons of one still shape, whose prisms can be kept between frames. They sit together on the caster list.
struct KeptGroup
{
	const void *shape;      // only handed back to kshape.cpp, which knows what it is
	unsigned int listNumber;
	int place[3];           // where the shape is, in the world
	int firstCaster;
	int casters;
	bool whole;             // false if some of its polygons didn't fit on the caster list this frame, so its prisms can't be made
	bool walked;            // true if none of its polygons are on the caster list, because the game drew it from the fixed buffer. Its prisms come from the shape
	int numPolygons;        // only a walked group: how many its shape has
	float ball[4];          // only a walked group: a ball round it, in the world's numbers
};

static const int kMostKeptGroups = 4096;
static KeptGroup keptGroups[kMostKeptGroups];
static int numKeptGroups = 0;
static int walkedGroups[kMostKeptGroups];   // which of keptGroups are walked
static int numWalkedGroups = 0;
static KeptCorner worldPrisms[kToyMaxVerts];   // this frame's prisms for a walked group that isn't kept, in the world's numbers, for one light
static bool prismsAsKeptCorners = false;   // true while prisms are being made for the card with their corners to be cut: see ToyAddPrism
static int numWorldCorners = 0;
static int worldCornersSinceReport = 0;    // how many of those there have been, added up, for the log
static int casterGroup[kToyMaxCasters];     // which of keptGroups a polygon is in, or -1
static int casterPolygon[kToyMaxCasters];   // its place in its shape
static const void *casterShape[kToyMaxCasters];   // its shape, only for -shadowshoot, and only ever compared
static CasterSource casterSource[kToyMaxCasters];   // only filled in on a report frame, for a polygon that's drawn
static int toyCasterKind[kToyMaxCasters];  // one of ToyCasterKind, which says what this polygon may do
static void const *toyCasterOwner[kToyMaxCasters];  // what the polygon belongs to, or nothing for the level. Only compared, never read through
static int toyCasterModule[kToyMaxCasters];  // for a cast-only polygon, which of toyUnseenModules it belongs to
static int toyNumCasters = 0;
static int toyNumCasterPoints = 0;
static int toyCastersLeftOut = 0;

// What each kind of polygon cost this frame, for the log
static int toyPolygonsOfKind[kToyCasterKinds];
static int toyVertsOfKind[kToyCasterKinds];
static void const *toyLastModel = 0;  // only ever compared, never read through, so it can't go stale in a harmful way
static int toyThisModelPolygons = 0;
static int toyNumModels = 0;
static int toyLargestModel = 0;

static const int kToyVertsPerDraw = 60000 * 3;
static D3DLVERTEX toyChunk[kToyVertsPerDraw];

static ToyVec toyVerts[kToyMaxVerts];
static bool prismCornersInWorld = false;   // true while prisms are being made from corners in the world's numbers, which have no distance from the camera
static float toyVertEnds[kToyMaxVerts];   // each of those corners' KeptCorner end. Only a prism made for the card reads it
static int toyNumVerts = 0;
static int toyPrismsLeftOut = 0;
static double toyStretchTotal = 0.0;   // how far corners were thrown past their polygon, for the log
static int toyStretchCorners = 0;

int ToyShadows_ChosenLights(float *lights, const void **owners);
const char *ToyShadows_RoomName();
void ToyShadows_WorldAxes(float *axes);
void ToyShadows_ViewMatrix(float *matrix);
int ToyShadows_CornersInWorld(const void *shapePointer, int polygonNumber, const int *place, float *corners, int mostCorners);
int ToyShadows_PolygonInWorld(const void *shapePointer, int polygonNumber, const int *place, float *corners, int mostCorners, float *plane, bool *twoSided);
void ToyShadows_CameraPlace(float *place);
int ToyShadows_CollectOutOfView(const float *lights, int numLights, const float *axes, int *polygonsLeftOut);
void ToyShadows_NextCollectingFrame();

// Walls out of view arrive a module at a time, each with a ball round it. There can be thousands of their polygons and up to 26
// stencil passes a frame, so a pass asks the ball once, and if that module's shadow can't show, it skips every polygon in it.
static const int kToyMostUnseenModules = 4096;
static ShadowReachBall toyUnseenModules[kToyMostUnseenModules];
static int toyNumUnseenModules = 0;

// Starts a new module of walls out of view. The cast-only polygons added after this belong to it. ball is in the same numbers as the
// polygons. Gives back false if there's no room for another module, and then its polygons should be left out.
bool ToyShadows_StartUnseenModule(const ShadowReachBall &ball)
{
	if (toyNumUnseenModules == kToyMostUnseenModules)
	{
		return false;
	}

	toyUnseenModules[toyNumUnseenModules++] = ball;
	return true;
}

// Where a shadow can show this frame: see shadow_reach.cpp.
static ShadowReachView shadowReachView;

// A box round each thing drawn this frame, in the same numbers as the polygons. The first holds the camera, which is at 0 0 0, so that whatever's
// right in front of it counts as drawn. Past the limit, the last box simply grows, which can only let in too many. A model is many parts with one
// owner, and they share a box, so that this stays a list of rooms and objects.
static const int kMostDrawnBoxes = 256;
static ShadowReachBox drawnBoxes[kMostDrawnBoxes];
static int numDrawnBoxes = 0;
static void const *drawnBoxOwner = 0;

// How far a box stands out from what's in it. A few things are drawn without being on the list: a sprite, or a spear stuck in a wall. They sit
// among things that are on it, so this is enough to take them in.
static const float kDrawnBoxMargin = 500.0f;

static void StartDrawnBoxes()
{
	numDrawnBoxes = 1;
	drawnBoxOwner = 0;

	for (int axis = 0; axis < 3; axis++)
	{
		drawnBoxes[0].smallest[axis] = 0.0f;
		drawnBoxes[0].largest[axis] = 0.0f;
	}
}

// The polygons added after this belong to a new thing on the list of what's drawn. owner is what it belongs to, or nothing for a piece of the level.
void ShadowReach_StartDrawnThing(const void *owner)
{
	if (numDrawnBoxes == 0)
	{
		StartDrawnBoxes();
	}

	if ((owner && owner == drawnBoxOwner) || numDrawnBoxes == kMostDrawnBoxes)
	{
		return;
	}

	ShadowReachBox &box = drawnBoxes[numDrawnBoxes++];

	drawnBoxOwner = owner;

	// inside out, so that the first corner sets it
	for (int axis = 0; axis < 3; axis++)
	{
		box.smallest[axis] = 1.0e30f;
		box.largest[axis] = -1.0e30f;
	}
}

static void MakeShadowReachView()
{
	if (numDrawnBoxes == 0)
	{
		StartDrawnBoxes();
	}

	for (int i = 0; i < numDrawnBoxes; i++)
	{
		for (int axis = 0; axis < 3; axis++)
		{
			drawnBoxes[i].smallest[axis] -= kDrawnBoxMargin;
			drawnBoxes[i].largest[axis] += kDrawnBoxMargin;
		}
	}

	ShadowReach_StartView(&shadowReachView);
	ShadowReach_AddCameraView(&shadowReachView, d3d.matProjection._11, d3d.matProjection._22);
}

// What the shadow reach test did this frame, for the log.
static int polygonsReachKept = 0;
static int polygonsReachLeftOut = 0;

// What leaving out polygons beyond a light's range saved this frame, added up over every pass. Prisms taken from the store aren't counted, because
// they were never built: with kept prisms off, as they are by default, these cover the whole frame.
static int prismsOutOfRange = 0;
static int vertsOutOfRange = 0;

// -shadowreport: the full report, with a count of pixels from the card and a reading of its pictures on every report frame. Each of those makes the
// game wait for the card, so a report frame then takes many times as long, and it shows as a hitch every few seconds. It's off unless asked for.
int ShadowsFullReport = 0;

// pts holds count points, at least three. Gives back twice the area of the polygon. Its way round comes from all its triangles, because its first three
// corners can be almost in a line, and then their triangle can point anywhere: see polygon_facing.h, which is the same sum for shapes.
static float ToyWorkOutSurface(const ToyVec *pts, int count, ToyFlatSurface &surface)
{
	float const ax = pts[1].x - pts[0].x, ay = pts[1].y - pts[0].y, az = pts[1].z - pts[0].z;
	float const bx = pts[2].x - pts[0].x, by = pts[2].y - pts[0].y, bz = pts[2].z - pts[0].z;
	float nx = 0.0f, ny = 0.0f, nz = 0.0f;

	for (int i = 1; i < count - 1; i++)
	{
		float const ux = pts[i].x - pts[0].x, uy = pts[i].y - pts[0].y, uz = pts[i].z - pts[0].z;
		float const vx = pts[i + 1].x - pts[0].x, vy = pts[i + 1].y - pts[0].y, vz = pts[i + 1].z - pts[0].z;

		nx += uy * vz - uz * vy;
		ny += uz * vx - ux * vz;
		nz += ux * vy - uy * vx;
	}

	float const length = sqrtf(nx * nx + ny * ny + nz * nz);

	float const sideA = ax * ax + ay * ay + az * az;
	float const sideB = bx * bx + by * by + bz * bz;
	surface.halfSize = sqrtf(sideA > sideB ? sideA : sideB) * 0.5f;
	surface.inWorld = false;
	surface.keepsStartGap = false;

	if (length < 0.001f)
	{
		surface.nx = 0.0f;
		surface.ny = 0.0f;
		surface.nz = 0.0f;
		surface.distance = 0.0f;
		return length;
	}

	surface.nx = nx / length;
	surface.ny = ny / length;
	surface.nz = nz / length;
	surface.distance = surface.nx * pts[0].x + surface.ny * pts[0].y + surface.nz * pts[0].z;

	return length;
}

// pts holds count points, x y z, in the same space mainVertex uses (view space with y turned over)
// kind is one of ToyCasterKind. owner is what the polygon belongs to, or nothing for the level.
void ToyShadows_AddCaster(const float *pts, int count, int kind, const void *owner, const CasterFacts &facts)
{
	if (count < 3 || count > kToyMostPolyPoints || kind < 0 || kind >= kToyCasterKinds)
	{
		return;
	}

	// this comes ahead of the limits: something left off the list is still on screen, and a shadow can still land on it
	if (kind != kToyCasterCastOnly && numDrawnBoxes > 0)
	{
		ShadowReachBox &box = drawnBoxes[numDrawnBoxes - 1];

		for (int i = 0; i < count * 3; i++)
		{
			int const axis = i % 3;

			if (pts[i] < box.smallest[axis])
			{
				box.smallest[axis] = pts[i];
			}

			if (pts[i] > box.largest[axis])
			{
				box.largest[axis] = pts[i];
			}
		}
	}

	if (toyNumCasters == kToyMaxCasters || toyNumCasterPoints + count > kToyMaxCasterPoints)
	{
		toyCastersLeftOut++;

		if (facts.keptGroup >= 0)
		{
			keptGroups[facts.keptGroup].whole = false;
		}

		return;
	}

	int const firstPoint = toyNumCasterPoints;

	casterGroup[toyNumCasters] = facts.keptGroup;
	casterPolygon[toyNumCasters] = facts.polygonNumber;
	casterShape[toyNumCasters] = facts.shape;

	if (facts.drawn && facts.source)
	{
		casterSource[toyNumCasters] = *facts.source;
	}

	if (facts.keptGroup >= 0)
	{
		keptGroups[facts.keptGroup].casters++;
	}

	toyCasterStart[toyNumCasters] = toyNumCasterPoints;
	toyCasterCount[toyNumCasters] = count;
	toyCasterKind[toyNumCasters] = kind;
	toyCasterOwner[toyNumCasters] = owner;
	toyCasterModule[toyNumCasters] = toyNumUnseenModules - 1;
	toyPolygonsOfKind[kind]++;

	for (int i = 0; i < count; i++)
	{
		toyCasterPoints[toyNumCasterPoints].x = pts[i * 3 + 0];
		toyCasterPoints[toyNumCasterPoints].y = pts[i * 3 + 1];
		toyCasterPoints[toyNumCasterPoints].z = pts[i * 3 + 2];
		toyNumCasterPoints++;
	}

	ToyFlatSurface &surface = toyCasterSurface[toyNumCasters];

	// a model needs its polygons' sizes, which aren't stored
	if (facts.storedPlane && kind != kToyCasterModel)
	{
		surface.nx = facts.storedPlane[0];
		surface.ny = facts.storedPlane[1];
		surface.nz = facts.storedPlane[2];
		surface.distance = facts.storedPlane[3];
		surface.halfSize = 0.0f;
		surface.inWorld = true;
	}
	else
	{
		ToyWorkOutSurface(&toyCasterPoints[firstPoint], count, surface);
	}

	// A polygon with two sides can be seen on either, so its shadow can't count on the side away from the light being unlit. A cloaked
	// Predator isn't in the surface pictures at all, so it has no surface for the small gap to be about.
	surface.keepsStartGap = facts.twoSided || kind == kToyCasterCloaked;

	casterDrawnOneSided[toyNumCasters] = facts.drawn && !facts.twoSided && (kind == kToyCasterLevel || kind == kToyCasterObject);

	toyNumCasters++;
}

// Starts a group of polygons whose prisms can be kept, and gives back its number, or -1 if there's no room for another. place is x y z.
int ShadowGroup_Start(const void *shape, unsigned int listNumber, const int *place)
{
	if (numKeptGroups == kMostKeptGroups)
	{
		return -1;
	}

	KeptGroup &group = keptGroups[numKeptGroups];

	group.shape = shape;
	group.listNumber = listNumber;
	memcpy(group.place, place, sizeof(group.place));
	group.firstCaster = toyNumCasters;
	group.casters = 0;
	group.whole = true;
	group.walked = false;

	return numKeptGroups++;
}

// A module the game drew from the fixed buffer this frame. It went through no pipeline, so it has no corners on the list, and this is all that's known about
// it. ballInWorld is four numbers. The ball also stands in for its corners in the box round what's drawn: a box that's too big only costs a little time.
void ShadowGroup_StartWalked(const void *shape, unsigned int listNumber, const int *place, int numPolygons, const float *ballInWorld, const ShadowReachBall &ballOnTheList)
{
	ShadowReach_StartDrawnThing(0);

	if (numDrawnBoxes > 0)
	{
		ShadowReachBox &box = drawnBoxes[numDrawnBoxes - 1];
		float const middle[3] = { ballOnTheList.x, ballOnTheList.y, ballOnTheList.z };

		for (int axis = 0; axis < 3; axis++)
		{
			float const lowest = middle[axis] - ballOnTheList.radius, highest = middle[axis] + ballOnTheList.radius;

			box.smallest[axis] = (lowest < box.smallest[axis]) ? lowest : box.smallest[axis];
			box.largest[axis] = (highest > box.largest[axis]) ? highest : box.largest[axis];
		}
	}

	int const number = ShadowGroup_Start(shape, listNumber, place);

	if (number >= 0)
	{
		keptGroups[number].walked = true;
		keptGroups[number].numPolygons = numPolygons;
		memcpy(keptGroups[number].ball, ballInWorld, sizeof(keptGroups[number].ball));
		walkedGroups[numWalkedGroups++] = number;
	}
}

// Is the frame being collected now one that ToyShadows_Draw will report on? It counts a frame when it draws, and that comes after collecting.
bool ShadowReportIsDue()
{
	return ((toyFrame + 1) % kFramesBetweenReports) == 0;
}

// The proof that a stored plane and the polygon's corners agree, on a report frame. The corners on the list are a mirror image of the shape,
// and the stored arrow was made to allow for that, so turning it to match the camera has to give the same arrow the corners give. Only the
// sign matters, so that's what's counted. A polygon can disagree without anything being wrong: the game rounds its corners to whole numbers,
// and a thin enough sliver can come out flipped or with no area at all, which is why the largest area is on the line.
static void ToyCheckStoredPlanes(const float *axes)
{
	int compared = 0, opposite = 0, oneHasNoArea = 0;
	float largest = 0.0f;

	for (int caster = 0; caster < toyNumCasters; caster++)
	{
		ToyFlatSurface const &stored = toyCasterSurface[caster];

		if (!stored.inWorld)
		{
			continue;
		}

		ToyFlatSurface corners;
		float const area = ToyWorkOutSurface(&toyCasterPoints[toyCasterStart[caster]], toyCasterCount[caster], corners) * 0.5f;

		float const x = stored.nx * axes[0] + stored.ny * axes[3] + stored.nz * axes[6];
		float const y = stored.nx * axes[1] + stored.ny * axes[4] + stored.nz * axes[7];
		float const z = stored.nx * axes[2] + stored.ny * axes[5] + stored.nz * axes[8];

		bool const storedHasArea = stored.nx != 0.0f || stored.ny != 0.0f || stored.nz != 0.0f;
		bool const cornersHaveArea = corners.nx != 0.0f || corners.ny != 0.0f || corners.nz != 0.0f;

		compared++;

		if (storedHasArea != cornersHaveArea)
		{
			oneHasNoArea++;
		}
		else if (storedHasArea && x * corners.nx + y * corners.ny + z * corners.nz <= 0.0f)
		{
			opposite++;
		}
		else
		{
			continue;
		}

		if (area > largest)
		{
			largest = area;
		}
	}

	char line[200] = { 0 };
	_snprintf(line, sizeof(line) - 1, "toy-shadows: frame %d: %d stored planes checked against their corners, %d point the other way, %d have no area on one side only, largest of those %.0f",
		toyFrame, compared, opposite, oneHasNoArea, largest);
	LogString(line);
}

// The proof behind the rule in ToyAddPrism that lets a shadow leave the camera out. That rule says nobody can see the back of a one-sided
// polygon, so its back never needs lighting, because the game hides it. So on a report frame this counts the polygons the game is drawing
// with the camera behind them. It should find next to none, and of two kinds. A polygon almost edge-on can land either side, because the
// game works in whole numbers, and "furthest" stays under a few units if that's all they are. And on 34 of the 715,116 one-sided polygons in
// the stock levels the normal stored in the file disagrees with the corner order. Those can be big, and the rule still can't take one's own
// surface into its shadow, because either way its shadow starts past the surface.
static void CheckNoPolygonSeenFromBehind()
{
	int checked[2] = { 0, 0 }, behind[2] = { 0, 0 };   // the first of each pair is for planes worked out, the second for stored planes
	float furthest = 0.0f, largest = 0.0f;
	int furthestCaster = -1;

	for (int caster = 0; caster < toyNumCasters; caster++)
	{
		if (!casterDrawnOneSided[caster])
		{
			continue;
		}

		ToyFlatSurface const &surface = toyCasterSurface[caster];
		int const which = surface.inWorld ? 1 : 0;
		float const cameraSide = ToyDistanceFrom(surface, surface.inWorld ? toyCameraInWorld : kCameraOnTheList);

		checked[which]++;

		if (!IsBehindPolygon(cameraSide))
		{
			continue;
		}

		behind[which]++;

		ToyFlatSurface corners;
		float const area = ToyWorkOutSurface(&toyCasterPoints[toyCasterStart[caster]], toyCasterCount[caster], corners) * 0.5f;

		if (area > largest)
		{
			largest = area;
		}

		if (fabsf(cameraSide) > furthest)
		{
			furthest = fabsf(cameraSide);
			furthestCaster = caster;
		}
	}

	char line[200] = { 0 };
	_snprintf(line, sizeof(line) - 1, "shadows: frame %d: camera behind a drawn polygon: %d of %d stored planes, %d of %d worked out, furthest %.1f, largest area %.0f",
		toyFrame, behind[1], checked[1], behind[0], checked[0], furthest, largest);
	LogString(line);

	if (furthestCaster >= 0)
	{
		CasterSource const &source = casterSource[furthestCaster];

		_snprintf(line, sizeof(line) - 1, "shadows: frame %d: furthest was polygon %d of shape %d at %d %d %d, behaviour %d, %s",
			toyFrame, casterPolygon[furthestCaster], source.shapeNumber, source.place[0], source.place[1], source.place[2], source.behaviour, source.why ? source.why : "");
		LogString(line);
	}
}

// Adds one body part to the log's count of how big each model is. All the parts of one model arrive together, so a new owner
// means a new model. model is only used to tell one owner from the next.
void ToyShadows_NoteModelPart(const void *model, int polygons)
{
	if (model != toyLastModel)
	{
		toyLastModel = model;
		toyThisModelPolygons = 0;
		toyNumModels++;
	}

	toyThisModelPolygons += polygons;

	if (toyThisModelPolygons > toyLargestModel)
	{
		toyLargestModel = toyThisModelPolygons;
	}
}

// A real light isn't a point, and that's what makes the edge of a shadow soft: a spot near the edge can see part of the light.
// So a light is drawn from several positions around its true place, each taking its part of the light. A spot blocked from all
// of them is full shadow. One blocked from some is the soft edge, the penumbra. Its width comes out right without being worked out:
// narrow where a shadow touches what casts it, wider further away, and wider for a bigger light.
//
// These are the corners of a cube, and the first four are every other corner, so 4 and 8 positions both balance around the true
// place, and neither lies flat: a flat ring would shrink to a line when seen from the side. They're directions in the world, so
// the shadows stay still while the camera turns.
static const float kToyPositions[8][3] =
{
	{  0.577f,  0.577f,  0.577f }, {  0.577f, -0.577f, -0.577f }, { -0.577f,  0.577f, -0.577f }, { -0.577f, -0.577f,  0.577f },
	{ -0.577f, -0.577f, -0.577f }, { -0.577f,  0.577f,  0.577f }, {  0.577f, -0.577f,  0.577f }, {  0.577f,  0.577f, -0.577f },
};

// What one light did on a report frame, so that the log shows what reached the screen and not only what the stencil marked. Marked
// isn't the same as darkened: a prism runs on past the light's range, and a pixel out there is marked and loses nothing.
struct ToyLightReport
{
	bool drawn;            // it got as far as its passes. A light whose whole range is off screen doesn't
	int polygonsInReach;   // added up over all its passes, like the prisms
	int prismsBuilt;       // only those worked out this frame. Kept ones don't come through here, so these two stay low once a light's prisms are kept
	int keptCorners;       // the corners drawn from the kept prisms instead
	float nearestCaster;   // how close any corner of a polygon that cast came to the light
	float maskRise;        // for a lamp, how much the mask's average went up. For a flare, the average of its own mask
	unsigned int stayedMarked;   // for a lamp, marked pixels no polygon painted, added up over its passes
};

// the light being counted, on a report frame. The rest of the time it's empty, and nothing is counted
static ToyLightReport *toyNoteLight = 0;

static inline void ToyTri(const ToyVec &a, const ToyVec &b, const ToyVec &c)
{
	toyVerts[toyNumVerts++] = a;
	toyVerts[toyNumVerts++] = b;
	toyVerts[toyNumVerts++] = c;
}

// Says what ends the last three corners from ToyTri have: see KeptCorner.
static inline void NoteCornerEnds(float a, float b, float c)
{
	toyVertEnds[toyNumVerts - 3] = a;
	toyVertEnds[toyNumVerts - 2] = b;
	toyVertEnds[toyNumVerts - 1] = c;
}

// Is every point of this polygon out of the light's range? light holds x y z range.
static bool PolygonIsOutOfRange(const ToyVec *pts, int count, const float *light)
{
	float smallest[3] = { pts[0].x, pts[0].y, pts[0].z };
	float largest[3] = { pts[0].x, pts[0].y, pts[0].z };

	for (int i = 1; i < count; i++)
	{
		float const point[3] = { pts[i].x, pts[i].y, pts[i].z };

		for (int axis = 0; axis < 3; axis++)
		{
			smallest[axis] = (point[axis] < smallest[axis]) ? point[axis] : smallest[axis];
			largest[axis] = (point[axis] > largest[axis]) ? point[axis] : largest[axis];
		}
	}

	float distanceSquared = 0.0f;

	for (int axis = 0; axis < 3; axis++)
	{
		float const outside = (light[axis] < smallest[axis]) ? smallest[axis] - light[axis] : (light[axis] > largest[axis]) ? light[axis] - largest[axis] : 0.0f;

		distanceSquared += outside * outside;
	}

	return distanceSquared >= light[3] * light[3];
}

// surface is the polygon's flat surface. light holds x y z range, and pts and centre's first three numbers have to be in the same numbers as it.
// centre is where the light really is: the two differ while the prisms
// are built from one of the light's positions. model says this polygon is part of a model, such as an alien
static void ToyAddPrism(const ToyVec *pts, int count, const ToyFlatSurface &surface, const float *light, const float *centre, bool model)
{
	int const needed = ((count - 2) * 2 + count * 2) * 3;
	if (toyNumVerts + needed > kToyMaxVerts)
	{
		toyPrismsLeftOut++;
		return;
	}

	float const nx = surface.nx, ny = surface.ny, nz = surface.nz;

	// These places are held against the surface, and they have to be in its own numbers. That's the only difference between the two kinds.
	float const *lightPlace = surface.inWorld ? light + kToyLightWorld : light;
	float const *centrePlace = surface.inWorld ? centre + kToyLightWorld : centre;

	float const lx = light[0] - pts[0].x, ly = light[1] - pts[0].y, lz = light[2] - pts[0].z;
	float const facing = ToyDistanceFrom(surface, lightPlace);
	float const lightDistance = sqrtf(lx * lx + ly * ly + lz * lz);

	if ((nx == 0.0f && ny == 0.0f && nz == 0.0f) || lightDistance < 1.0f || facing == 0.0f)
	{
		return;
	}

	// A position can land behind a surface the light is really in front of: a flare lying on the floor, or a lamp just under a
	// ceiling. That surface would then block the position from the whole room, and part of the light would go, in the shape of
	// the floor. So a polygon whose flat surface lies between the light and the position doesn't cast for that position.
	float const centreFacing = ToyDistanceFrom(surface, centrePlace);

	if ((centreFacing < 0.0f) != (facing < 0.0f))
	{
		return;
	}

	// the polygon's flat surface is out of the light's reach
	float const planeDistance = fabsf(facing);
	if (planeDistance >= light[3])
	{
		return;
	}

	// A long flat surface can pass that test with the polygon itself nowhere near. A shadow only runs away from a light, so a polygon wholly beyond
	// the light's range can't block anything the light reaches, and it would only be more work for the card. A box round the polygon can't be
	// further away than the polygon is, so if the box is out of range, the polygon is too. This is measured from the light's true place, even
	// for a position: everything between a position and a lit spot is inside the range of the true place. It's only with the pictures that a spot
	// past the range loses nothing. The flat shadows on older cards have no fading, so they stay as they were.
	if (prismsForPictures && PolygonIsOutOfRange(pts, count, centre))
	{
		prismsOutOfRange++;
		vertsOutOfRange += needed;
		return;
	}

	if (toyNoteLight)
	{
		toyNoteLight->polygonsInReach++;
	}

	// A flare comes to rest touching a surface, so it can end up a hair behind it, and then the floor it's lying on would block
	// it from the whole room. So a polygon that close doesn't cast for that flare, or for any other light added per pixel, measured
	// from where the light really is. A light's eighth number is its kind, and 2 means added per pixel.
	if (light[7] == 2.0f && fabsf(centreFacing) < kToyFlareTouching)
	{
		return;
	}

	// The far end is the polygon scaled up from the light, so it stays parallel to the polygon, one light range away from the
	// light. Moving each corner out to the range instead fails on a big wall: its corners can all lie beyond the range while
	// its middle is right by the light, and the prism comes out paper thin.
	float const scale = light[3] / planeDistance;

	// Rooms are shells seen from inside, so light from the next room hits the back of a wall. Polygons cast from both sides.
	// A polygon facing the other way is read backwards, so that every prism is built the same way round.
	bool const backwards = facing < 0.0f;

	// A model is many small polygons, about 110 units along an edge, so starting just behind each one lets every rib and buckle
	// cast a hard little shadow on the next, and the model speckles. So its shadow starts further back, and only something well
	// clear of the surface, like an arm across a chest, shadows it. Half the polygon's own size is the limit: a facehugger's
	// polygons are about 48, and with a fixed gap all of its shadow would start below the floor it sits on.
	float startGap = kToyStartGap;
	if (model)
	{
		startGap = ((float)ToyShadowsModelGap < surface.halfSize) ? (float)ToyShadowsModelGap : surface.halfSize;
		if (startGap < kToyStartGap)
		{
			startGap = kToyStartGap;
		}
	}
	else if (prismsForPictures && !surface.keepsStartGap && IsBehindPolygon(facing))
	{
		// The light is behind this polygon, and the game hides a polygon's back, so the only side anyone sees is the one the light can't
		// reach. With the pictures, lightShapes.fxh already gives that side nothing from this light, so it doesn't have to be inside its own
		// shadow to stay dark. All that's left is what's beyond it, like the floor at the foot of a wall, so the shadow starts as close as it can.
		startGap = kUnlitStartGap;
	}

	// the lower the light hits the surface, the further the start has to move to get behind it by the same amount
	float const angle = fabsf(facing) / lightDistance;
	float gap = startGap / (angle > 0.01f ? angle : 0.01f);
	if (gap > kToyWidestGap)
	{
		gap = kToyWidestGap;
	}

	// The flat shadows have no pictures to say which way a pixel faces, and the game adds a live light to every point in range, walls or no
	// walls, even a point turned away from it. So on those frames a surface we see from the side the light can't reach has to be in that
	// light's shadow, even with nothing else in the way, as when the light is behind it. Its prism starts just in front of it, towards the
	// light, and takes the surface in. These are the only prisms that depend on where the camera is, so a frame with the pictures can keep
	// its prisms however the camera moves.
	bool takeSurfaceIn = false;

	if (!prismsForPictures && !model)
	{
		// A model doesn't need this. lightShapes.fxh gives a surface turned away from a lamp or a flare nothing to lose, and
		// on a model all it would do is leave a thin dark strip along every edge of the lit part.
		float const cameraSide = ToyDistanceFrom(surface, surface.inWorld ? toyCameraInWorld : kCameraOnTheList);
		takeSurfaceIn = (cameraSide < 0.0f) != (facing < 0.0f);
	}

	if (takeSurfaceIn)
	{
		gap = -kToyStartGap;
	}

	ToyVec nearPts[kToyMostPolyPoints], farPts[kToyMostPolyPoints];
	float nearEnds[kToyMostPolyPoints], farEnds[kToyMostPolyPoints];

	// A kept corner's length changes a little from frame to frame with the cut, so its far end stands further clear of its near end.
	float const farEndClear = prismsAsKeptCorners ? 4.0f : 1.0f;

	for (int i = 0; i < count; i++)
	{
		ToyVec const &point = pts[backwards ? count - 1 - i : i];

		float dx = point.x - light[0];
		float dy = point.y - light[1];
		float dz = point.z - light[2];
		float const length = sqrtf(dx * dx + dy * dy + dz * dz);

		if (length < 1.0f)
		{
			return;
		}

		if (toyNoteLight && (toyNoteLight->nearestCaster == 0.0f || length < toyNoteLight->nearestCaster))
		{
			toyNoteLight->nearestCaster = length;
		}

		dx /= length;
		dy /= length;
		dz /= length;

		float start = gap;

		if (takeSurfaceIn)
		{
			// a light almost touching the polygon: the start stays on the light's far side of the corner
			if (length < kToyStartGap * 2.0f)
			{
				start = length * -0.5f;
			}
		}
		else if (ShadowsEvenGap)
		{
			start = ShadowGap_ForCorner(startGap, length, planeDistance, kShadowGapWidest);
		}

		// Far away, depth is too coarse to tell a surface from the start of its own shadow. This only ever moves a start further back, and only where
		// the steps are coarser than what's already there, so a shadow close to the camera stays exactly as it is.
		if (ShadowsDepthGap && !takeSurfaceIn && !prismCornersInWorld)
		{
			float const clearOfTheSteps = ShadowGap_ForCorner(kDepthStepsBehind * DepthStepAt(point.z), length, planeDistance, kShadowGapWidest);

			start = (clearOfTheSteps > start) ? clearOfTheSteps : start;
		}

		float reach = length * scale;
		if (reach > kToyMostReach)
		{
			reach = kToyMostReach;
		}
		if (reach < length + start + farEndClear)
		{
			reach = length + start + farEndClear;   // the far end must stay behind the near end, or the prism turns inside out
		}

		toyStretchTotal += reach - length;
		toyStretchCorners++;

		nearPts[i].x = point.x + dx * start;
		nearPts[i].y = point.y + dy * start;
		nearPts[i].z = point.z + dz * start;
		farPts[i].x  = light[0] + dx * reach;
		farPts[i].y  = light[1] + dy * reach;
		farPts[i].z  = light[2] + dz * reach;

		nearEnds[i] = start;
		farEnds[i] = -(kKeptCornerFarMark + reach);

		// a corner the shader will finish stays as the wall's own corner: see KeptCorner
		if (prismsAsKeptCorners)
		{
			nearPts[i] = point;
			farPts[i] = point;
		}
	}

	for (int i = 1; i < count - 1; i++)
	{
		ToyTri(nearPts[0], nearPts[i], nearPts[i + 1]);
		NoteCornerEnds(nearEnds[0], nearEnds[i], nearEnds[i + 1]);
		ToyTri(farPts[0], farPts[i + 1], farPts[i]);
		NoteCornerEnds(farEnds[0], farEnds[i + 1], farEnds[i]);
	}

	for (int i = 0; i < count; i++)
	{
		int const next = (i + 1) % count;
		ToyTri(nearPts[i], farPts[next], nearPts[next]);
		NoteCornerEnds(nearEnds[i], farEnds[next], nearEnds[next]);
		ToyTri(nearPts[i], farPts[i], farPts[next]);
		NoteCornerEnds(nearEnds[i], farEnds[i], farEnds[next]);
	}

	if (toyNoteLight)
	{
		toyNoteLight->prismsBuilt++;
	}
}

// the game's own vertex shader and declaration, so a prism lands exactly where the world's polygons do
static void ToySetVolumeProgram(LPDIRECT3DDEVICE9 device)
{
	d3d.effectSystem->SetActive(d3d.mainEffect);
	d3d.effectSystem->SetVertexShaderConstant(d3d.mainEffect, 0, CONST_MATRIX, &d3d.matProjection);
	d3d.mainDecl->Set();
	device->SetPixelShader(NULL);
}

// the prisms in toyVerts, then whatever this light has kept on the card, under the same stencil and depth settings
static HRESULT ToyDrawVolumes(D3DCOLOR colour)
{
	HRESULT result = S_OK;

	FrameTiming_Mark(kStageShading);
	FrameTiming_CountPrismCorners(toyNumVerts, 0);

	for (int start = 0; start < toyNumVerts; start += kToyVertsPerDraw)
	{
		int const verts = (toyNumVerts - start < kToyVertsPerDraw) ? toyNumVerts - start : kToyVertsPerDraw;

		for (int i = 0; i < verts; i++)
		{
			toyChunk[i].x = toyVerts[start + i].x;
			toyChunk[i].y = toyVerts[start + i].y;
			toyChunk[i].z = toyVerts[start + i].z;
			toyChunk[i].color = colour;
			toyChunk[i].specular = 0;
			toyChunk[i].u = 0.0f;
			toyChunk[i].v = 0.0f;
		}

		HRESULT const drawn = d3d.lpD3DDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST, verts / 3, toyChunk, sizeof(D3DLVERTEX));
		if (FAILED(drawn))
		{
			result = drawn;
		}
	}

	FrameTiming_Mark(kStagePrismsSent);

	if (ShadowVolumes_NumRuns() > 0 || numWorldCorners > 0)
	{
		HRESULT const kept = ShadowVolumesCard_Draw(d3d.lpD3DDevice, colour, worldPrisms, numWorldCorners);
		if (FAILED(kept))
		{
			result = kept;
		}

		ToySetVolumeProgram(d3d.lpD3DDevice);
		FrameTiming_CountPrismCorners(numWorldCorners, keptCornersThisPass);
		FrameTiming_Mark(kStagePrismsKept);
	}

	return result;
}

static int toyQueriesUnanswered = 0;   // a count that never came reads as 0, so the log has to say how many didn't

// What one frame did. The hr values stay S_OK unless a call failed.
struct ToyFrameReport
{
	int frame;
	int casters;
	int lights;
	int verts;
	HRESULT block, clear, back, front, quad;
	unsigned int pixels;
	float milliseconds;        // the time spent here on this one frame, which also waits for the card to answer its counts
	float frameMilliseconds;   // how long a frame took on average since the last report, which is the number to compare
};

static void ToyLogFrame(const ToyFrameReport &report)
{
	char line[400];
	_snprintf(line, sizeof(line) - 1,
		"toy-shadows: frame %d, view %s: %d polygons (%d left out), %d lights, %d verts (%d prisms left out), state block %08x, clear %08x, back %08x, front %08x, quad %08x, %u pixels in shadow, %.1f ms here, %.1f ms a frame, %d early flushes, soft %d, %d counts unanswered",
		report.frame, kToyViewNames[ToyShadowsMode], report.casters, toyCastersLeftOut, report.lights, report.verts, toyPrismsLeftOut,
		(unsigned int)report.block, (unsigned int)report.clear, (unsigned int)report.back, (unsigned int)report.front,
		(unsigned int)report.quad, report.pixels, report.milliseconds, report.frameMilliseconds, toyEarlyFlushes, ToyShadowsSoft, toyQueriesUnanswered);
	toyEarlyFlushes = 0;
	line[sizeof(line) - 1] = 0;
	LogString(line);

	// What each kind cost. Verts are the real price, and they're added up over every light: a polygon outside all the lights
	// costs none, however many polygons it belongs with.
	_snprintf(line, sizeof(line) - 1,
		"toy-shadows: frame %d, polygons: %d level, %d objects, %d models, %d cloaked, %d out of view; verts: %d level, %d objects, %d models, %d cloaked, %d out of view; %d models drawn, the largest %d polygons",
		report.frame, toyPolygonsOfKind[kToyCasterLevel], toyPolygonsOfKind[kToyCasterObject], toyPolygonsOfKind[kToyCasterModel],
		toyPolygonsOfKind[kToyCasterCloaked], toyPolygonsOfKind[kToyCasterCastOnly],
		toyVertsOfKind[kToyCasterLevel], toyVertsOfKind[kToyCasterObject], toyVertsOfKind[kToyCasterModel],
		toyVertsOfKind[kToyCasterCloaked], toyVertsOfKind[kToyCasterCastOnly], toyNumModels, toyLargestModel);
	line[sizeof(line) - 1] = 0;
	LogString(line);

	// Modules in a light's range, and what the reach test did with them. The polygons it left out are only counted, never built.
	_snprintf(line, sizeof(line) - 1,
		"shadow volumes: frame %d, out of view: %d polygons kept and %d left out by the reach test, %d verts; %d things drawn; beyond the range: %d prisms and %d verts left out of %d verts built",
		report.frame, polygonsReachKept, polygonsReachLeftOut, toyVertsOfKind[kToyCasterCastOnly], numDrawnBoxes, prismsOutOfRange, vertsOutOfRange, report.verts);
	line[sizeof(line) - 1] = 0;
	LogString(line);
}

static void ToySetVolumeState(LPDIRECT3DDEVICE9 device)
{
	ToySetVolumeProgram(device);

	device->SetTexture(0, NULL);
	device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
	device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
	device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
	device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);

	device->SetRenderState(D3DRS_LIGHTING, FALSE);
	device->SetRenderState(D3DRS_FOGENABLE, FALSE);
	device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
	device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
	device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
	device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
	device->SetRenderState(D3DRS_COLORWRITEENABLE, 0);

	device->SetRenderState(D3DRS_TWOSIDEDSTENCILMODE, FALSE);
	device->SetRenderState(D3DRS_STENCILENABLE, TRUE);
	device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
	device->SetRenderState(D3DRS_STENCILREF, 0);
	device->SetRenderState(D3DRS_STENCILMASK, 0xFFFFFFFF);
	device->SetRenderState(D3DRS_STENCILWRITEMASK, 0xFFFFFFFF);
	device->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
	device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
}

// The card answers when it gets there, which for the first question of a frame means finishing everything drawn so far. So
// this waits by the clock, half a second at most, and says whether the answer came.
static bool ToyWaitForCard(IDirect3DQuery9 *query, void *answer, DWORD bytes)
{
	HRESULT result = S_FALSE;

	LARGE_INTEGER started, now, ticksPerSecond;
	QueryPerformanceCounter(&started);
	QueryPerformanceFrequency(&ticksPerSecond);

	do
	{
		result = query->GetData(answer, bytes, D3DGETDATA_FLUSH);
		QueryPerformanceCounter(&now);
	} while (result == S_FALSE && now.QuadPart - started.QuadPart < ticksPerSecond.QuadPart / 2);

	return result == S_OK;
}

// A count that never came is noted, not taken for a 0.
static unsigned int ToyAskCard(IDirect3DQuery9 *countQuery)
{
	DWORD counted = 0;

	if (!ToyWaitForCard(countQuery, &counted, sizeof(counted)))
	{
		toyQueriesUnanswered++;
		return 0;
	}

	return counted;
}

// One flat colour over the whole screen. The blend and stencil settings decide what that does.
static HRESULT ToyDrawScreenQuad(LPDIRECT3DDEVICE9 device, DWORD colour)
{
	struct ToyScreenVertex
	{
		float x, y, z, rhw;
		DWORD colour;
	};

	float const w = (float)ScreenDescriptorBlock.SDB_Width;
	float const h = (float)ScreenDescriptorBlock.SDB_Height;

	ToyScreenVertex const quad[4] =
	{
		{ 0.0f, 0.0f, 0.0f, 1.0f, colour },
		{ w,    0.0f, 0.0f, 1.0f, colour },
		{ 0.0f, h,    0.0f, 1.0f, colour },
		{ w,    h,    0.0f, 1.0f, colour },
	};

	device->SetVertexShader(NULL);
	device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);

	return device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(ToyScreenVertex));
}

// Everything drawn so far, turned down together. Particles, the weapon and the HUD come after this, so they keep their brightness.
static void ToyDimWorld(LPDIRECT3DDEVICE9 device)
{
	int const level = ToyShadowsWorld * 255 / 100;

	ToySetVolumeState(device);
	device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
	device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
	device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
	device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ZERO);
	device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_SRCCOLOR);
	ToyDrawScreenQuad(device, D3DCOLOR_ARGB(255, level, level, level));
}

static HRESULT ToyDarkenStencilledPixels(LPDIRECT3DDEVICE9 device, DWORD shade, IDirect3DQuery9 *countQuery, unsigned int *pixels)
{
	device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
	device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
	device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_NOTEQUAL);
	device->SetRenderState(D3DRS_STENCILREF, 0);
	device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

	if (countQuery)
	{
		countQuery->Issue(D3DISSUE_BEGIN);
	}

	HRESULT const result = ToyDrawScreenQuad(device, shade);

	if (countQuery)
	{
		countQuery->Issue(D3DISSUE_END);
		*pixels += ToyAskCard(countQuery);
	}

	return result;
}

// Draws the volumes without touching the stencil or the picture, and counts the fragments that pass this depth test.
static unsigned int ToyCountFragments(LPDIRECT3DDEVICE9 device, IDirect3DQuery9 *countQuery, DWORD cull, DWORD depthTest)
{
	device->SetRenderState(D3DRS_CULLMODE, cull);
	device->SetRenderState(D3DRS_ZFUNC, depthTest);

	countQuery->Issue(D3DISSUE_BEGIN);
	ToyDrawVolumes(0);
	countQuery->Issue(D3DISSUE_END);

	return ToyAskCard(countQuery);
}

struct ToyMeasurements
{
	unsigned int backBehind, frontBehind;     // fragments behind what the scene drew: these are what z-fail counts
	unsigned int backInFront, frontInFront;   // fragments in front of it
	unsigned int stencilControl;              // pixels marked with no depth test at all: 0 here means the stencil path is broken
	unsigned int zPass;                       // the same shadow, counted from the camera's side instead
};

// Leaves the picture alone: the depth buffer isn't written, colour writes are off, and the two stencil counts are read with a
// fully see-through quad. The real pass clears the stencil again afterwards.
static void ToyMeasure(LPDIRECT3DDEVICE9 device, IDirect3DQuery9 *countQuery, ToyMeasurements *measured)
{
	DWORD const seeThrough = 0;

	ToySetVolumeState(device);
	device->SetRenderState(D3DRS_STENCILENABLE, FALSE);

	measured->backBehind   = ToyCountFragments(device, countQuery, D3DCULL_CW,  D3DCMP_GREATER);
	measured->frontBehind  = ToyCountFragments(device, countQuery, D3DCULL_CCW, D3DCMP_GREATER);
	measured->backInFront  = ToyCountFragments(device, countQuery, D3DCULL_CW,  D3DCMP_LESSEQUAL);
	measured->frontInFront = ToyCountFragments(device, countQuery, D3DCULL_CCW, D3DCMP_LESSEQUAL);

	// every fragment of the volumes marks the stencil, whatever the depth buffer holds
	device->Clear(0, NULL, D3DCLEAR_STENCIL, 0, 1.0f, 0);
	ToySetVolumeState(device);
	device->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
	device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE);
	device->SetRenderState(D3DRS_STENCILREF, 1);
	ToyDrawVolumes(0);
	ToyDarkenStencilledPixels(device, seeThrough, countQuery, &measured->stencilControl);

	// z-pass: front faces in front of the scene add one, back faces in front of it take one away
	device->Clear(0, NULL, D3DCLEAR_STENCIL, 0, 1.0f, 0);
	ToySetVolumeState(device);
	device->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
	device->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
	device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_INCR);
	ToyDrawVolumes(0);
	device->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);
	device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_DECR);
	ToyDrawVolumes(0);
	ToyDarkenStencilledPixels(device, seeThrough, countQuery, &measured->zPass);
}

// A closed box round the light. Every direction out of the light hits it, so everything in range is in its shadow, whatever
// the room looks like.
static void ToyAddTestBox(const float *light, const float *centre)
{
	float const size = 100.0f;
	ToyVec corners[8];

	for (int i = 0; i < 8; i++)
	{
		corners[i].x = light[0] + ((i & 1) ? size : -size);
		corners[i].y = light[1] + ((i & 2) ? size : -size);
		corners[i].z = light[2] + ((i & 4) ? size : -size);
	}

	static const int kFaces[6][4] = { { 0, 1, 3, 2 }, { 4, 5, 7, 6 }, { 0, 1, 5, 4 }, { 2, 3, 7, 6 }, { 0, 2, 6, 4 }, { 1, 3, 7, 5 } };

	for (int face = 0; face < 6; face++)
	{
		ToyVec const points[4] = { corners[kFaces[face][0]], corners[kFaces[face][1]], corners[kFaces[face][2]], corners[kFaces[face][3]] };
		ToyFlatSurface surface;
		ToyWorkOutSurface(points, 4, surface);
		ToyAddPrism(points, 4, surface, light, centre, false);
	}
}

// Lines in the light's colour, seen through everything.
static void ToyDrawWires(LPDIRECT3DDEVICE9 device, D3DCOLOR colour)
{
	ToySetVolumeState(device);
	device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
	device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
	device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
	device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	device->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
	ToyDrawVolumes(colour);
	device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
}

// A small solid box where each light is, in that light's colour, seen through walls.
static void ToyDrawLightMarkers(LPDIRECT3DDEVICE9 device, const float *lights, int numLights)
{
	// the last light's prisms aren't part of these boxes
	ShadowVolumes_StartRuns();
	numWorldCorners = 0;

	float const size = 80.0f;
	static const int kFaces[6][4] = { { 0, 1, 3, 2 }, { 4, 5, 7, 6 }, { 0, 1, 5, 4 }, { 2, 3, 7, 6 }, { 0, 2, 6, 4 }, { 1, 3, 7, 5 } };

	ToySetVolumeState(device);
	device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
	device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
	device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
	device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);

	for (int light = 0; light < numLights; light++)
	{
		float const *position = &lights[light * kToyLightFloats];
		ToyVec corners[8];

		if (LightOnListIsDark(position))
		{
			continue;
		}

		for (int i = 0; i < 8; i++)
		{
			corners[i].x = position[0] + ((i & 1) ? size : -size);
			corners[i].y = position[1] + ((i & 2) ? size : -size);
			corners[i].z = position[2] + ((i & 4) ? size : -size);
		}

		toyNumVerts = 0;
		for (int face = 0; face < 6; face++)
		{
			ToyTri(corners[kFaces[face][0]], corners[kFaces[face][1]], corners[kFaces[face][2]]);
			ToyTri(corners[kFaces[face][0]], corners[kFaces[face][2]], corners[kFaces[face][3]]);
		}

		ToyDrawVolumes(0xFF000000 | kToyLightColours[light]);
	}
}

static void ToySetFloatState(LPDIRECT3DDEVICE9 device, D3DRENDERSTATETYPE state, float value)
{
	DWORD asBits;
	memcpy(&asBits, &value, sizeof(asBits));
	device->SetRenderState(state, asBits);
}

static inline void ToyPutScreenCorner(D3DLVERTEX &vertex, float x, float y, float u, float v)
{
	vertex.x = x;
	vertex.y = y;
	vertex.z = 0.0f;
	vertex.color = 0;
	vertex.specular = 0;
	vertex.u = u;
	vertex.v = v;
}

// Soft edges need two pictures the size of the screen: the mask, which says how much of each pixel to keep, and how far away
// each pixel is. They live on the graphics card, so they're lost whenever the device resets: Alt-Tab, or a new screen size.
static IDirect3DTexture9 *toyMaskTexture = 0;
static IDirect3DTexture9 *toyDepthTexture = 0;
static IDirect3DSurface9 *toyMaskSurface = 0;
static IDirect3DSurface9 *toyDepthSurface = 0;
static bool toyTargetsFailed = false;

template <class T> static void ToyRelease(T *&thing)
{
	if (thing)
	{
		thing->Release();
		thing = 0;
	}
}

// Called just before the device resets, which Direct3D won't do while these exist. They're made again when next needed.
// A copy of all the card's settings, to put back after the shadow pass. It's only made once, because making one every frame means asking for memory and
// copying everything into it. It holds on to whatever was set when it last took its copy, so it has to go before the device can be reset.
static IDirect3DStateBlock9 *savedCardSettings = 0;

static void ReleaseShadowPictures()
{
	ToyRelease(toyMaskSurface);
	ToyRelease(toyDepthSurface);
	ToyRelease(toyMaskTexture);
	ToyRelease(toyDepthTexture);
	SurfacePictures_Release();
	PixelLights_Release();
	toyTargetsFailed = false;
}

void ToyShadows_ReleaseTargets()
{
	ToyRelease(savedCardSettings);
	ReleaseShadowPictures();
	ShadowShoot_ReleaseCardThings();
}

// The mask is added to many times, in small amounts, so eight bits would round them away. A float picture is better, but only if
// the card can blend on one. Failing that, it's the old picture: it still works, with slightly rougher soft edges.
static D3DFORMAT ToyBestMaskFormat(LPDIRECT3DDEVICE9 device)
{
	static D3DFORMAT const wanted[2] = { D3DFMT_R16F, D3DFMT_A16B16G16R16F };

	D3DFORMAT chosen = D3DFMT_A8R8G8B8;
	IDirect3D9 *direct3D = 0;
	D3DDEVICE_CREATION_PARAMETERS created;
	D3DDISPLAYMODE mode;

	if (SUCCEEDED(device->GetDirect3D(&direct3D)))
	{
		if (SUCCEEDED(device->GetCreationParameters(&created)) && SUCCEEDED(device->GetDisplayMode(0, &mode)))
		{
			for (int i = 1; i >= 0; i--)
			{
				if (direct3D->CheckDeviceFormat(created.AdapterOrdinal, created.DeviceType, mode.Format,
					D3DUSAGE_RENDERTARGET | D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING, D3DRTYPE_TEXTURE, wanted[i]) == D3D_OK)
				{
					chosen = wanted[i];
				}
			}
		}

		direct3D->Release();
	}

	LogString(chosen == D3DFMT_R16F ? "toy-shadows: the mask is one 16-bit float"
		: chosen == D3DFMT_A16B16G16R16F ? "toy-shadows: the mask is four 16-bit floats"
		: "toy-shadows: the card can't blend on a float picture, so the mask stays 8 bits");

	return chosen;
}

static bool ToyMakeTargets(LPDIRECT3DDEVICE9 device)
{
	if (toyMaskSurface && toyDepthSurface)
	{
		return true;
	}

	if (toyTargetsFailed)
	{
		return false;
	}

	IDirect3DSurface9 *screen = 0;
	D3DSURFACE_DESC size;

	if (FAILED(device->GetRenderTarget(0, &screen)))
	{
		toyTargetsFailed = true;
		return false;
	}

	HRESULT const measured = screen->GetDesc(&size);
	screen->Release();

	if (FAILED(measured)
		|| FAILED(device->CreateTexture(size.Width, size.Height, 1, D3DUSAGE_RENDERTARGET, ToyBestMaskFormat(device), D3DPOOL_DEFAULT, &toyMaskTexture, NULL))
		|| FAILED(device->CreateTexture(size.Width, size.Height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_R32F, D3DPOOL_DEFAULT, &toyDepthTexture, NULL))
		|| FAILED(toyMaskTexture->GetSurfaceLevel(0, &toyMaskSurface))
		|| FAILED(toyDepthTexture->GetSurfaceLevel(0, &toyDepthSurface)))
	{
		// Only the pictures: this can happen partway through the shadow pass, which still has to put the card's settings back when it ends.
		ReleaseShadowPictures();
		toyTargetsFailed = true;
		LogString("toy-shadows: couldn't make the pictures for soft edges, so shadow edges stay hard");
		return false;
	}

	return true;
}

// Draws the surface pictures, which also write down how far away each pixel is. They're the same surfaces the game has just drawn, so they go
// a hair nearer, or rounding would leave specks of them behind the game's own. The hair is a fixed number of the depth buffer's steps, about 17, and
// doesn't grow with a surface's slant. In the world it does grow: it's 1.6 units at 10 metres, 56 at 60 and 156 at 100. Depth isn't written here, so of
// two polygons that both pass, the last drawn wins, and a wider margin lets the wrong one win along the line where two surfaces meet. So far away,
// the pictures can hold the wrong polygon's normal near a crease. -shadowbiassteps tries a smaller hair. Says false if nothing was drawn.
static bool ToyDrawPictures(LPDIRECT3DDEVICE9 device, const float *worldAxes, bool useStatic)
{
	ToySetVolumeState(device);

	device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
	device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
	device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	ToySetFloatState(device, D3DRS_DEPTHBIAS, ShadowSurfaceBias);

	bool const drawn = SurfacePictures_Draw(device, toyDepthSurface, worldAxes, LightingNow == kLightingRelief, useStatic);

	ToySetFloatState(device, D3DRS_DEPTHBIAS, 0.0f);

	return drawn;
}

// -staticsurface: the walls in the surface pictures come from the fixed buffer. -staticcompare: the same, and a report frame also draws the pictures
// the old way and compares every pixel, which makes that frame wait for the card, so it's only for checking. Off is the old route. It stays
// off by default until the comparison has been seen to come out clean in the game.
int StaticSurfaceOn = 0;
int StaticSurfaceCompare = 0;
extern int StaticSceneOn;

// Can the walls come from the fixed buffer on this frame, as far as the shadows go? Only in the real picture, with lights added per pixel, because the other
// views draw things the buffer doesn't hold. A frame that's being compared needs the old route, to have something to compare with.
bool StaticScene_ViewAllows()
{
	return ToyShadowsLightsByPixel && ToyShadowsMode == kToyViewFinal && !(StaticSurfaceCompare && ShadowReportIsDue());
}

static const int kMostFrameRanges = 16384;
static StaticRange frameRanges[kMostFrameRanges];
static int numFrameRanges = 0;

// Works out which parts of the fixed buffer this frame draws, and whether it can use them. Every module the frame drew has to be in the list, or its
// polygons would vanish from the pictures. So if there's any doubt, the answer is no, and the old route draws everything.
static bool ChooseStaticRoute()
{
	numFrameRanges = 0;

	// these three move corners or flip the view in ways the buffer doesn't know about
	if ((!StaticSurfaceOn && !StaticSurfaceCompare && !StaticScene_TookAny()) || !StaticLevel_Ready() || !SurfaceStatic_Ready()
		|| MIRROR_CHEATMODE || TRIPTASTIC_CHEATMODE || UNDERWATER_CHEATMODE)
	{
		return false;
	}

	int numModules = 0;
	int const *modules = SurfaceNormals_ModulesDrawn(&numModules);

	if (numModules > kMostModulesInAFrame)
	{
		return false;
	}

	for (int i = 0; i < numModules; i++)
	{
		int count = 0;
		StaticRange const *ranges = StaticLevel_RangesOf(modules[i], &count);

		if (count > kMostFrameRanges - numFrameRanges)
		{
			return false;
		}

		for (int range = 0; range < count; range++)
		{
			frameRanges[numFrameRanges++] = ranges[range];
		}
	}

	// a card can have a limit on how many triangles one draw can hold
	static uint32_t mostTriangles = 0;

	if (!mostTriangles)
	{
		D3DCAPS9 caps;
		mostTriangles = SUCCEEDED(d3d.lpD3DDevice->GetDeviceCaps(&caps)) ? caps.MaxPrimitiveCount : 65535;
	}

	numFrameRanges = StaticFrame_SortAndJoin(frameRanges, numFrameRanges, mostTriangles);
	SurfaceStatic_SetFrame(frameRanges, numFrameRanges);
	return true;
}

// A picture read back into memory. It's given back when this goes.
class PictureInMemory
{
public:
	PictureInMemory() : copy(0), bits(0), pitch(0)
	{
		memset(&description, 0, sizeof(description));
	}

	~PictureInMemory()
	{
		if (bits)
		{
			copy->UnlockRect();
		}

		if (copy)
		{
			copy->Release();
		}
	}

	bool Read(LPDIRECT3DDEVICE9 device, IDirect3DSurface9 *picture)
	{
		D3DLOCKED_RECT locked;

		if (FAILED(picture->GetDesc(&description))
			|| FAILED(device->CreateOffscreenPlainSurface(description.Width, description.Height, description.Format, D3DPOOL_SYSTEMMEM, &copy, NULL))
			|| FAILED(device->GetRenderTargetData(picture, copy))
			|| FAILED(copy->LockRect(&locked, NULL, D3DLOCK_READONLY)))
		{
			return false;
		}

		bits = (uint8_t const *)locked.pBits;
		pitch = (uint32_t)locked.Pitch;
		return true;
	}

	IDirect3DSurface9 *copy;
	uint8_t const *bits;
	uint32_t pitch;
	D3DSURFACE_DESC description;
};

// One route's pictures, read back. The two picture textures come from surface_pictures.cpp.
struct RoutePictures
{
	PictureInMemory distance, normal, colour;

	bool Read(LPDIRECT3DDEVICE9 device, IDirect3DSurface9 *distancePicture)
	{
		IDirect3DSurface9 *normalPicture = 0;
		IDirect3DSurface9 *colourPicture = 0;
		bool worked = false;

		if (SUCCEEDED(SurfacePictures_Normal()->GetSurfaceLevel(0, &normalPicture)) && SUCCEEDED(SurfacePictures_Colour()->GetSurfaceLevel(0, &colourPicture)))
		{
			worked = distance.Read(device, distancePicture) && normal.Read(device, normalPicture) && colour.Read(device, colourPicture);
		}

		if (normalPicture)
		{
			normalPicture->Release();
		}

		if (colourPicture)
		{
			colourPicture->Release();
		}

		return worked;
	}

	SurfacePicturesInMemory InMemory() const
	{
		SurfacePicturesInMemory pictures = { distance.bits, normal.bits, colour.bits, distance.pitch, normal.pitch, colour.pitch };
		return pictures;
	}
};

// Draws the pictures both ways and compares them. Whatever's left in the pictures after this, the caller draws them again.
static void CompareSurfaceRoutes(LPDIRECT3DDEVICE9 device, const float *worldAxes, int frame)
{
	RoutePictures oldRoute, newRoute;

	device->Clear(0, NULL, D3DCLEAR_TARGET, 0xFFFFFFFF, 1.0f, 0);
	bool const oldDrawn = ToyDrawPictures(device, worldAxes, false) && oldRoute.Read(device, toyDepthSurface);

	device->SetRenderTarget(0, toyDepthSurface);
	device->Clear(0, NULL, D3DCLEAR_TARGET, 0xFFFFFFFF, 1.0f, 0);
	bool const newDrawn = ToyDrawPictures(device, worldAxes, true) && newRoute.Read(device, toyDepthSurface);

	device->SetRenderTarget(0, toyDepthSurface);
	device->Clear(0, NULL, D3DCLEAR_TARGET, 0xFFFFFFFF, 1.0f, 0);

	char line[300] = { 0 };

	if (!oldDrawn || !newDrawn)
	{
		_snprintf(line, sizeof(line) - 1, "static compare: frame %d: couldn't read the pictures back, so nothing was compared", frame);
		LogString(line);
		return;
	}

	PictureDifferences differences;
	StaticFrame_Compare(oldRoute.InMemory(), newRoute.InMemory(), oldRoute.colour.description.Width, oldRoute.colour.description.Height,
		oldRoute.normal.description.Format == D3DFMT_A2R10G10B10, &differences);

	_snprintf(line, sizeof(line) - 1,
		"static compare: frame %d, %d pixels, %d ranges: %d holes in the new route, %d only in the new route, %d colours changed, %d normals changed, %d distances changed, largest change %.2f",
		frame, differences.pixelsCompared, numFrameRanges, differences.holesInNew, differences.extraInNew, differences.colourChanged,
		differences.normalChanged, differences.distanceChanged, differences.largestDistanceChange);
	LogString(line);
}

// Draws just the walls that are in the fixed buffer, both ways, each with a depth buffer of its own, and compares them. It has to run on a frame that went the
// old way, so that the game's own polygons are there to compare with, so it says that its frame is no good for timing.
static void CompareSceneRoutes(LPDIRECT3DDEVICE9 device, int frame)
{
	D3DSURFACE_DESC size;
	IDirect3DSurface9 *gameDepth = 0;
	IDirect3DSurface9 *ownDepth = 0;
	char line[300] = { 0 };

	if (FAILED(toyDepthSurface->GetDesc(&size)) || FAILED(device->GetDepthStencilSurface(&gameDepth))
		|| FAILED(device->CreateDepthStencilSurface(size.Width, size.Height, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, TRUE, &ownDepth, NULL)))
	{
		if (gameDepth)
		{
			gameDepth->Release();
		}

		LogString("static scene compare: couldn't make a depth buffer, so nothing was compared");
		return;
	}

	RoutePictures oldRoute, newRoute;

	device->SetDepthStencilSurface(ownDepth);
	ToySetVolumeState(device);
	device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
	device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
	device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);

	bool const oldDrawn = SurfacePictures_DrawWalls(device, toyDepthSurface, false) && oldRoute.Read(device, toyDepthSurface);
	bool const newDrawn = SurfacePictures_DrawWalls(device, toyDepthSurface, true) && newRoute.Read(device, toyDepthSurface);

	device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
	device->SetDepthStencilSurface(gameDepth);
	gameDepth->Release();
	ownDepth->Release();

	device->SetRenderTarget(0, toyDepthSurface);
	device->Clear(0, NULL, D3DCLEAR_TARGET, 0xFFFFFFFF, 1.0f, 0);

	if (!oldDrawn || !newDrawn)
	{
		_snprintf(line, sizeof(line) - 1, "static scene compare: frame %d: couldn't read the pictures back, so nothing was compared", frame);
		LogString(line);
		return;
	}

	PictureDifferences differences;
	StaticFrame_Compare(oldRoute.InMemory(), newRoute.InMemory(), oldRoute.colour.description.Width, oldRoute.colour.description.Height,
		oldRoute.normal.description.Format == D3DFMT_A2R10G10B10, &differences);

	_snprintf(line, sizeof(line) - 1,
		"static scene compare: frame %d (drawn the old way, so its timings don't count), %d pixels: %d holes in the new route, %d only in the new route, %d colours changed, %d distances changed, the largest by %.2f",
		frame, differences.pixelsCompared, differences.holesInNew, differences.extraInNew, differences.colourChanged, differences.distanceChanged, differences.largestDistanceChange);
	LogString(line);
}

// There are two ways to show a shadow. If this card can do everything below, the surface pictures are drawn, every light reads them, and
// chosen live lights leave the game's lighting to be added per pixel. If it can't, the game keeps all its lights, and each light darkens
// the pixels it can't reach by a flat share. Nothing is left in between, so this one test decides both.
static bool ToyPicturesCanRun(LPDIRECT3DDEVICE9 device)
{
	return d3d.shadowBlurEffect != kNullEffectID && PixelLights_Ready(device) && ToyMakeTargets(device);
}

// The stencil now marks the pixels this light can't reach. On each of those, this adds the light's share of all the light there to the mask. So
// the mask ends up holding how much of each pixel's light is blocked. A spot in one lamp's shadow keeps the light of the others, and only a
// spot blocked from all its lamps goes right down to the glow.
static HRESULT ToyShadeLevel(LPDIRECT3DDEVICE9 device, const float *light, float strength, IDirect3DQuery9 *countQuery, unsigned int *pixels)
{
	if (countQuery)
	{
		countQuery->Issue(D3DISSUE_BEGIN);
	}

	HRESULT const result = PixelLights_AddShareToMask(device, light, strength, toyDepthTexture);

	if (countQuery)
	{
		countQuery->Issue(D3DISSUE_END);
		*pixels += ToyAskCard(countQuery);
	}

	if (ShadowShootFrame)
	{
		ShadowShoot_AfterShading(device, toyMaskSurface);
	}

	return result;
}

// Puts the mask on the screen through the softening shader, multiplying what's there. Or, for the mask view, instead of what's there.
static void ToyApplySoftenedMask(LPDIRECT3DDEVICE9 device, bool showMaskAlone)
{
	D3DSURFACE_DESC size;
	toyMaskSurface->GetDesc(&size);

	float const pixelAcross = 1.0f / (float)size.Width;
	float const pixelDown   = 1.0f / (float)size.Height;

	ToySetVolumeState(device);

	d3d.effectSystem->SetActive(d3d.shadowBlurEffect);
	d3d.mainDecl->Set();

	float const constants[8] =
	{
		pixelAcross, pixelDown, (float)ToyShadowsSoft, kToySoftStaysWideTo,
		(float)ToyShadowsDarkness / 100.0f, 0.0f, 0.0f, 0.0f,
	};
	device->SetPixelShaderConstantF(0, constants, 2);

	device->SetTexture(0, toyMaskTexture);
	device->SetTexture(1, toyDepthTexture);

	for (int sampler = 0; sampler < 2; sampler++)
	{
		device->SetSamplerState(sampler, D3DSAMP_MINFILTER, D3DTEXF_POINT);
		device->SetSamplerState(sampler, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
		device->SetSamplerState(sampler, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
		device->SetSamplerState(sampler, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
		device->SetSamplerState(sampler, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
	}

	device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
	device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
	device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
	device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	device->SetRenderState(D3DRS_ALPHABLENDENABLE, !showMaskAlone);
	device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ZERO);
	device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_SRCCOLOR);

	// half a pixel on each way, or every pixel would read the corner of its place in the mask rather than the middle
	float const left = pixelAcross * 0.5f, top = pixelDown * 0.5f;

	ToyPutScreenCorner(toyChunk[0], -1.0f,  1.0f, left,        top);
	ToyPutScreenCorner(toyChunk[1],  1.0f,  1.0f, 1.0f + left, top);
	ToyPutScreenCorner(toyChunk[2], -1.0f, -1.0f, left,        1.0f + top);
	ToyPutScreenCorner(toyChunk[3],  1.0f, -1.0f, 1.0f + left, 1.0f + top);

	device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, toyChunk, sizeof(D3DLVERTEX));

	device->SetTexture(0, NULL);
	device->SetTexture(1, NULL);
}

// While this isn't kShootRouteNone, WorkOutPrismsInWorld tells the shoot tool about each polygon's prism, and how that prism gets to the card.
static ShootRoute shootCrossingRoute = kShootRouteNone;

// Works out a group's prisms for a light, in the world's numbers. They're left just past the end of toyVerts, at the place toyNumVerts still says, and
// cornersMade says how many. false means they aren't all there, so they can't be used.
static bool WorkOutPrismsInWorld(const KeptGroup &group, const float *light, const float *centre, int *cornersMade)
{
	struct InWorldForNow
	{
		InWorldForNow() { prismCornersInWorld = true; }
		~InWorldForNow() { prismCornersInWorld = false; }
	} inWorldForNow;

	// The light's first three numbers are its place in the light list's numbers, which change whenever the camera does. So this gives the sums in
	// ToyAddPrism a copy with the world's numbers there instead. The surfaces are all in the world's numbers already, because a group only
	// has shapes that keep their planes.
	float lightInWorld[kToyLightFloats];
	memcpy(lightInWorld, light, sizeof(lightInWorld));
	memcpy(lightInWorld, light + kToyLightWorld, 3 * sizeof(float));

	float centreInWorld[kToyLightFloats];
	memcpy(centreInWorld, centre, sizeof(centreInWorld));
	memcpy(centreInWorld, centre + kToyLightWorld, 3 * sizeof(float));

	int const vertsBefore = toyNumVerts;
	int const leftOutBefore = toyPrismsLeftOut;
	bool cornersAgree = true;

	prismsAsKeptCorners = ShadowVolumesCard_CornersAreCut();

	if (group.walked)
	{
		for (int polygon = 0; polygon < group.numPolygons; polygon++)
		{
			ToyVec corners[kToyMostPolyPoints];
			float plane[4];
			bool twoSided = false;
			int const count = ToyShadows_PolygonInWorld(group.shape, polygon, group.place, &corners[0].x, kToyMostPolyPoints, plane, &twoSided);

			if (count >= 3 && !(ShadowShootOn && ShadowShoot_IsStopped(group.shape, polygon)))
			{
				ToyFlatSurface surface;

				surface.nx = plane[0];
				surface.ny = plane[1];
				surface.nz = plane[2];
				surface.distance = plane[3];
				surface.halfSize = 0.0f;
				surface.inWorld = true;
				surface.keepsStartGap = twoSided;

				int const vertsBeforePolygon = toyNumVerts;

				ToyAddPrism(corners, count, surface, lightInWorld, centreInWorld, false);

				if (shootCrossingRoute != kShootRouteNone && toyNumVerts > vertsBeforePolygon)
				{
					ShadowShoot_CrossInWorld(&toyVerts[vertsBeforePolygon].x, &toyVertEnds[vertsBeforePolygon], toyNumVerts - vertsBeforePolygon, count, group.shape, polygon, shootCrossingRoute, prismsAsKeptCorners);
				}
			}
		}
	}
	else
	{
		for (int caster = group.firstCaster; caster < group.firstCaster + group.casters; caster++)
		{
			ToyVec corners[kToyMostPolyPoints];
			int const count = ToyShadows_CornersInWorld(group.shape, casterPolygon[caster], group.place, &corners[0].x, kToyMostPolyPoints);

			if (count != toyCasterCount[caster])
			{
				cornersAgree = false;
				break;
			}

			int const vertsBeforePolygon = toyNumVerts;

			ToyAddPrism(corners, count, toyCasterSurface[caster], lightInWorld, centreInWorld, false);

			if (shootCrossingRoute != kShootRouteNone && toyNumVerts > vertsBeforePolygon)
			{
				ShadowShoot_CrossInWorld(&toyVerts[vertsBeforePolygon].x, &toyVertEnds[vertsBeforePolygon], toyNumVerts - vertsBeforePolygon, count, group.shape, casterPolygon[caster], shootCrossingRoute, prismsAsKeptCorners);
			}
		}
	}

	prismsAsKeptCorners = false;

	*cornersMade = toyNumVerts - vertsBefore;
	toyNumVerts = vertsBefore;

	return cornersAgree && toyPrismsLeftOut == leftOutBefore;
}

// Works out a group's prisms for a light, and puts them on the card. false means they aren't kept, and nothing has changed.
static bool MakeKeptPrisms(const KeptGroup &group, const ShadowVolumeKey &key, const float *light, const float *centre, ShadowVolumeRun *run)
{
	int const vertsBefore = toyNumVerts;
	int corners = 0;

	if (!WorkOutPrismsInWorld(group, light, centre, &corners))
	{
		return false;
	}

	int const place = ShadowVolumes_PlaceFor(corners);

	if (place < 0 || (corners > 0 && !ShadowVolumesCard_Write(place, &toyVerts[vertsBefore].x, &toyVertEnds[vertsBefore], corners)))
	{
		return false;
	}

	ShadowVolumes_Keep(key, corners);
	FrameTiming_CountKeptGroupsMade();
	keptGroupsMadeThisFrame++;
	run->first = place;
	run->corners = corners;

	return true;
}

// A kept group's prisms are on the card, and can't be read back. So on a shot's frame they're worked out again, the same way, for the shoot tool to look at. Nothing here is drawn, and nothing is kept.
static void ShootCrossKeptPrisms(const KeptGroup &group, const float *light, const float *centre, int cornersOnCard)
{
	// this isn't work the frame did, so it stays out of the frame's numbers
	ToyLightReport *const noteLight = toyNoteLight;
	int const leftOut = toyPrismsLeftOut, outOfRange = prismsOutOfRange, vertsOut = vertsOutOfRange, stretchCorners = toyStretchCorners;
	double const stretchTotal = toyStretchTotal;
	int corners = 0;

	toyNoteLight = 0;
	shootCrossingRoute = kShootRouteKept;

	bool const whole = WorkOutPrismsInWorld(group, light, centre, &corners);

	shootCrossingRoute = kShootRouteNone;
	toyNoteLight = noteLight;
	toyPrismsLeftOut = leftOut;
	prismsOutOfRange = outOfRange;
	vertsOutOfRange = vertsOut;
	toyStretchCorners = stretchCorners;
	toyStretchTotal = stretchTotal;

	ShadowShoot_NoteKeptGroup(cornersOnCard, corners, whole);
}

// Puts a group's kept prisms for this light on the list to draw, making them first if they're new and the light is staying still.
// false means this group goes the ordinary way this time.
static bool UseKeptPrisms(const KeptGroup &group, const float *light, const float *centre, int *corners)
{
	ShadowVolumeKey key;
	memcpy(key.castFrom, light + kToyLightWorld, sizeof(key.castFrom));
	memcpy(key.lightPlace, centre + kToyLightWorld, sizeof(key.lightPlace));
	key.range = light[3];
	key.lightKind = light[7];
	key.listNumber = group.listNumber;
	memcpy(key.place, group.place, sizeof(key.place));

	ShadowVolumeRun run;

	if (!ShadowVolumes_Find(key, &run))
	{
		float const numbers[kShadowVolumeNumbersEach] = { centre[kToyLightWorld + 0], centre[kToyLightWorld + 1], centre[kToyLightWorld + 2], centre[3], centre[7] };

		if (!group.whole || !ShadowVolumes_LightIsStill(numbers) || !MakeKeptPrisms(group, key, light, centre, &run))
		{
			return false;
		}
	}

	*corners = run.corners;

	if (toyNoteLight)
	{
		toyNoteLight->keptCorners += run.corners;
	}

	bool const added = ShadowVolumes_AddRun(run);

	if (added && ShadowShootFrame)
	{
		ShootCrossKeptPrisms(group, light, centre, run.corners);
	}

	return added;
}

// A walked group's prisms for this light, for this frame only: a light that's moving, or a store that's full or switched off. The ordinary route can't take
// them, because that list is in the view's numbers and these have no corners there, so they go in a list of their own, in the world's numbers.
static void AddWorldPrisms(const KeptGroup &group, const float *light, const float *centre)
{
	int const vertsBefore = toyNumVerts;
	int corners = 0;
	size_t const crossingsBefore = ShadowShootFrame ? ShadowShoot_CrossingMark() : 0;

	shootCrossingRoute = ShadowShootFrame ? kShootRouteWalked : kShootRouteNone;

	bool const worked = WorkOutPrismsInWorld(group, light, centre, &corners);

	shootCrossingRoute = kShootRouteNone;

	if (!worked || corners > kToyMaxVerts - numWorldCorners)
	{
		// these aren't going to be drawn, so the shoot tool mustn't count them
		if (ShadowShootFrame)
		{
			ShadowShoot_ForgetCrossings(crossingsBefore);
		}

		toyPrismsLeftOut++;
		return;
	}

	for (int i = 0; i < corners; i++)
	{
		KeptCorner &out = worldPrisms[numWorldCorners + i];

		out.x = toyVerts[vertsBefore + i].x;
		out.y = toyVerts[vertsBefore + i].y;
		out.z = toyVerts[vertsBefore + i].z;
		out.end = toyVertEnds[vertsBefore + i];
	}

	numWorldCorners += corners;
	worldCornersSinceReport += corners;
	toyVertsOfKind[kToyCasterLevel] += corners;
}

// Could anything in this group block this light? The same test as PolygonIsOutOfRange, with the group's ball in place of a polygon, from the light's true place.
static bool GroupIsInRange(const KeptGroup &group, const float *centre)
{
	float const dx = group.ball[0] - centre[kToyLightWorld + 0];
	float const dy = group.ball[1] - centre[kToyLightWorld + 1];
	float const dz = group.ball[2] - centre[kToyLightWorld + 2];
	float const reach = group.ball[3] + centre[3];

	return dx * dx + dy * dy + dz * dz < reach * reach;
}

// Fills toyVerts with this light's prisms. A cloaked model casts only a faint shadow, and a stencil only knows yes or no, so its
// prisms go through a pass of their own: cloakedOnly picks those, and leaving it false picks all the others.
// owner is what carries this light, or nothing. Anything carrying a light sits around it, so it would block that light in every
// direction, and the light would go dark everywhere. So it casts no shadow from its own light, and still casts from all the others.
// centre is where the light really is, and light is where these prisms are cast from, which can be one of its positions.
static void ToyBuildPrisms(const float *light, const float *centre, const void *owner, ToyFrameReport *report, bool cloakedOnly)
{
	toyNumVerts = 0;
	int moduleAsked = -1;
	bool moduleCanShow = false;

	// Kept prisms were made without the camera, so they're only right on a frame that doesn't use it. A cloaked model is never kept.
	bool const keeping = prismsForPictures && !cloakedOnly && !KeptPrismsOff && ShadowVolumesCard_Ready();
	int groupGoingTheOrdinaryWay = -1;
	int keptCorners = 0;

	ShadowVolumes_StartRuns();
	ShadowVolumesCard_SetLight(light);
	numWorldCorners = 0;

	if (ShadowShootFrame)
	{
		ShadowShoot_StartPass(captureLightBeingBuilt, light, centre, cloakedOnly);
	}

	for (int caster = 0; caster < toyNumCasters; caster++)
	{
		int const kind = toyCasterKind[caster];

		if ((kind == kToyCasterCloaked) != cloakedOnly || (owner && toyCasterOwner[caster] == owner))
		{
			continue;
		}

		if (ShadowShootOn && ShadowShoot_IsStopped(casterShape[caster], casterPolygon[caster]))
		{
			continue;
		}

		// a module's polygons sit together in the list, so its ball only has to be asked when the module changes
		if (kind == kToyCasterCastOnly)
		{
			if (toyCasterModule[caster] != moduleAsked)
			{
				moduleAsked = toyCasterModule[caster];
				moduleCanShow = ShadowReach_CanReach(shadowReachView, drawnBoxes, numDrawnBoxes, toyUnseenModules[moduleAsked], light);
			}

			if (!moduleCanShow)
			{
				continue;
			}
		}

		int const groupNumber = casterGroup[caster];

		// A group holding a stopped polygon can't use its kept prisms, because they still have that polygon in them. They stay on the card for when it's undone.
		if (keeping && groupNumber >= 0 && groupNumber != groupGoingTheOrdinaryWay && !(ShadowShootOn && ShadowShoot_ShapeHasStops(keptGroups[groupNumber].shape)))
		{
			KeptGroup const &group = keptGroups[groupNumber];
			int corners = 0;

			if (UseKeptPrisms(group, light, centre, &corners))
			{
				keptCorners += corners;
				toyVertsOfKind[kind] += corners;
				caster = group.firstCaster + group.casters - 1;   // its last polygon, and the loop then steps past it
				continue;
			}

			groupGoingTheOrdinaryWay = groupNumber;
		}

		int const vertsBefore = toyNumVerts;
		ToyAddPrism(&toyCasterPoints[toyCasterStart[caster]], toyCasterCount[caster], toyCasterSurface[caster], light, centre, kind == kToyCasterModel);
		toyVertsOfKind[kind] += toyNumVerts - vertsBefore;

		if (ShadowShootFrame && toyNumVerts > vertsBefore)
		{
			ShadowShoot_CrossOnList(&toyVerts[vertsBefore].x, toyNumVerts - vertsBefore, toyCasterCount[caster], casterShape[caster], casterPolygon[caster]);
		}
	}

	// walls drawn from the fixed buffer, which have nothing on the list above. A wall isn't a cloaked Predator, and it has no owner to step over.
	for (int walked = 0; walked < numWalkedGroups && !cloakedOnly; walked++)
	{
		KeptGroup const &group = keptGroups[walkedGroups[walked]];
		int corners = 0;

		if (!GroupIsInRange(group, centre))
		{
			continue;
		}

		if (keeping && !(ShadowShootOn && ShadowShoot_ShapeHasStops(group.shape)) && UseKeptPrisms(group, light, centre, &corners))
		{
			keptCorners += corners;
			toyVertsOfKind[kToyCasterLevel] += corners;
			continue;
		}

		AddWorldPrisms(group, light, centre);
	}

	if (ToyShadowsTest && !cloakedOnly)
	{
		ToyAddTestBox(light, centre);
	}
	report->verts += toyNumVerts + keptCorners + numWorldCorners;

	if (captureLightBeingBuilt >= 0)
	{
		captureCornersSent[captureLightBeingBuilt] += toyNumVerts + numWorldCorners;
		captureCornersKept[captureLightBeingBuilt] += keptCorners;
	}
	keptCornersThisPass = keptCorners;
}

// Tells the store which lights there are this frame, and tells the card where the camera is.
static void StartKeptPrisms(const float *lights, int numLights)
{
	float numbers[kToyMostLights * kShadowVolumeNumbersEach];
	int const count = (numLights < kToyMostLights) ? numLights : kToyMostLights;

	for (int i = 0; i < count; i++)
	{
		float const *light = &lights[i * kToyLightFloats];
		float *out = &numbers[i * kShadowVolumeNumbersEach];

		out[0] = light[kToyLightWorld + 0];
		out[1] = light[kToyLightWorld + 1];
		out[2] = light[kToyLightWorld + 2];
		out[3] = light[3];
		out[4] = light[7];
	}

	ShadowVolumes_StartFrame(numbers, count);

	float view[12];
	ToyShadows_ViewMatrix(view);
	ShadowVolumesCard_SetView(view);
}

static void ReportKeptPrisms()
{
	// These walls are on no caster list, so no other line would show they were there.
	if (numWalkedGroups > 0)
	{
		char walls[200] = { 0 };

		_snprintf(walls, sizeof(walls) - 1, "shadows: frame %d: walls from the fixed buffer: %d this frame, and %d corners worked out for one frame only since the last report",
			toyFrame, numWalkedGroups, worldCornersSinceReport);
		LogString(walls);
	}

	worldCornersSinceReport = 0;

	// without this, a run with the switch on would log all zeros, which reads like a store that's broken
	if (KeptPrismsOff)
	{
		LogString("shadows: kept prisms are off, so nothing is kept (-keptprismson turns them on)");
		return;
	}

	ShadowVolumeCounts counts;
	ShadowVolumes_TakeCounts(&counts);

	char line[200] = { 0 };
	_snprintf(line, sizeof(line) - 1, "shadows: frame %d: kept prisms: %d found, %d made, %d refused, emptied %d times, %d corners in %d entries, %s",
		toyFrame, counts.found, counts.kept, counts.refused, counts.emptied, counts.cornersInUse, counts.entriesInUse, ShadowVolumesCard_CornerWords());
	LogString(line);
}

// Clears the stencil and marks the pixels the light behind the prisms in toyVerts can't reach.
static void ToyMarkShadow(LPDIRECT3DDEVICE9 device, ToyFrameReport *report)
{
	FrameTiming_Mark(kStageShading);

	HRESULT const cleared = device->Clear(0, NULL, D3DCLEAR_STENCIL, 0, 1.0f, 0);
	if (FAILED(cleared))
	{
		report->clear = cleared;
	}

	FrameTiming_Mark(kStageClearing);

	ToySetVolumeState(device);

	// A prism's inside faces add one where they're hidden, and its outside faces take one away. If the card can do both from
	// a single draw, that saves drawing every prism twice.
	static DWORD toyStencilCaps = 0;
	if (!toyStencilCaps)
	{
		D3DCAPS9 caps;
		device->GetDeviceCaps(&caps);
		toyStencilCaps = caps.StencilCaps | 0x80000000;
	}

	if (toyStencilCaps & D3DSTENCILCAPS_TWOSIDED)
	{
		device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
		device->SetRenderState(D3DRS_TWOSIDEDSTENCILMODE, TRUE);
		device->SetRenderState(D3DRS_CCW_STENCILFUNC, D3DCMP_ALWAYS);
		device->SetRenderState(D3DRS_CCW_STENCILFAIL, D3DSTENCILOP_KEEP);
		device->SetRenderState(D3DRS_CCW_STENCILPASS, D3DSTENCILOP_KEEP);
		device->SetRenderState(D3DRS_CCW_STENCILZFAIL, D3DSTENCILOP_INCR);
		device->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_DECR);

		HRESULT const both = ToyDrawVolumes(0);
		if (FAILED(both))
		{
			report->back = both;
		}

		device->SetRenderState(D3DRS_TWOSIDEDSTENCILMODE, FALSE);

		if (ShadowShootFrame)
		{
			ShadowShoot_AfterMarking(device);
		}

		return;
	}

	device->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);
	device->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_INCR);
	HRESULT const back = ToyDrawVolumes(0);
	if (FAILED(back))
	{
		report->back = back;
	}

	device->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
	device->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_DECR);
	HRESULT const front = ToyDrawVolumes(0);
	if (FAILED(front))
	{
		report->front = front;
	}

	if (ShadowShootFrame)
	{
		ShadowShoot_AfterMarking(device);
	}
}

// Works out the part of the screen a light can reach: the box round its range, seen from the camera. It's a little bigger than
// it needs to be, never smaller, because the corners of a box stick out past a sphere. Says false if the light can't reach the
// screen at all. A light gives nothing past its range, so limiting its work to this part changes nothing in the picture.
static bool ToyLightRectangle(LPDIRECT3DDEVICE9 device, const float *light, RECT *rectangle)
{
	float const x = light[0], y = light[1], z = light[2], range = light[3];

	if (z + range < zNear)
	{
		return false;  // all of it is behind the camera
	}

	float left = -1.0f, right = 1.0f, bottom = -1.0f, top = 1.0f;

	// with the camera inside the box, or the box round the camera's side, the whole screen is the safe answer
	if (z - range > zNear)
	{
		float const nearZ = z - range, farZ = z + range;
		float const across = d3d.matProjection._11, up = d3d.matProjection._22;

		left   = across * min((x - range) / nearZ, (x - range) / farZ);
		right  = across * max((x + range) / nearZ, (x + range) / farZ);
		bottom = up * min((y - range) / nearZ, (y - range) / farZ);
		top    = up * max((y + range) / nearZ, (y + range) / farZ);

		if (left >= 1.0f || right <= -1.0f || bottom >= 1.0f || top <= -1.0f)
		{
			return false;
		}

		left   = (left < -1.0f) ? -1.0f : left;
		right  = (right > 1.0f) ? 1.0f : right;
		bottom = (bottom < -1.0f) ? -1.0f : bottom;
		top    = (top > 1.0f) ? 1.0f : top;
	}

	D3DVIEWPORT9 viewport;
	device->GetViewport(&viewport);

	rectangle->left   = viewport.X + (LONG)floor((left + 1.0f) * 0.5f * viewport.Width);
	rectangle->right  = viewport.X + (LONG)ceil((right + 1.0f) * 0.5f * viewport.Width);
	rectangle->top    = viewport.Y + (LONG)floor((1.0f - top) * 0.5f * viewport.Height);
	rectangle->bottom = viewport.Y + (LONG)ceil((1.0f - bottom) * 0.5f * viewport.Height);

	return true;
}

// Turns the stencil's marks for this flare into a picture: white where it's blocked. The flare's shader softens that as it
// reads it, which a stencil can't do: a stencil is only ever yes or no. amount is how much each mark counts for, and addToMask
// puts it on top of what this flare has already painted, as a cloaked shadow or a further position of the flare does.
static void ToyPaintFlareMask(LPDIRECT3DDEVICE9 device, IDirect3DSurface9 *screen, const D3DVIEWPORT9 *viewport, const RECT *reach, bool addToMask, float amount, IDirect3DQuery9 *countQuery, unsigned int *pixels)
{
	device->SetRenderTarget(0, toyMaskSurface);
	device->SetViewport(viewport);

	// the whole mask is cleared, not just this flare's part, or the last flare's marks would still be there
	if (!addToMask)
	{
		device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
		device->Clear(0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0);
		device->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
	}

	// changing the picture being drawn on can reset this
	device->SetScissorRect(reach);

	// rounded up, so that all the positions together reach all the way: a spot blocked from every one gets none of the flare
	DWORD const grey = (DWORD)ceilf(amount * 255.0f);

	device->SetPixelShader(NULL);
	device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
	device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
	device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	device->SetRenderState(D3DRS_ALPHABLENDENABLE, addToMask ? TRUE : FALSE);
	device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
	device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
	device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_NOTEQUAL);
	device->SetRenderState(D3DRS_STENCILREF, 0);
	device->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
	device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);

	if (countQuery)
	{
		countQuery->Issue(D3DISSUE_BEGIN);
	}

	ToyDrawScreenQuad(device, D3DCOLOR_ARGB(255, grey, grey, grey));

	if (countQuery)
	{
		countQuery->Issue(D3DISSUE_END);
		*pixels += ToyAskCard(countQuery);
	}

	device->SetRenderTarget(0, screen);
	device->SetViewport(viewport);
	device->SetScissorRect(reach);

	if (ShadowShootFrame)
	{
		ShadowShoot_AfterShading(device, toyMaskSurface);
	}
}

// Adds one light to the picture: a flare, or any other live light chosen this frame. The mask takes out the part that's blocked, softly, so it
// lights nothing it can't see. Each light does this for itself, so where one is blocked another can still light the same place.
static void ToyAddLight(LPDIRECT3DDEVICE9 device, const float *light)
{
	if (ShadowShootFrame)
	{
		ShadowShoot_BeforeAddingLight(device, captureLightBeingBuilt);
	}

	PixelLights_AddLight(device, light, toyMaskTexture, toyDepthTexture, (float)ToyShadowsSoft, kToySoftStaysWideTo);

	if (ShadowShootFrame)
	{
		ShadowShoot_AfterAddingLight(device, captureLightBeingBuilt);
	}
}

// Is this a flare being added per pixel? Only those have a ninth number of 0. The kind can't say: 2 covers every light added per
// pixel, such as a muzzle flash too.
static bool ToyLightIsFlare(const float *light)
{
	return light[8] == 0.0f;
}

// How many positions this light is drawn from: every flare, and the lamps PickLampsForPositions picked. A light with no size is a point, so
// one position is all it can have.
static int ToyPositionsForLight(const float *light)
{
	bool const flare = ToyLightIsFlare(light);
	int const size = flare ? ToyShadowsFlareSize : ToyShadowsLampSize;

	if (size <= 0 || ToyShadowsPositions < 4 || (!flare && light[kLightManyPositions] == 0.0f))
	{
		return 1;
	}

	return (ToyShadowsPositions < 8) ? 4 : 8;
}

// Fills moved with a copy of the light at one of its positions. axes holds the world's three directions, in the light list's space.
static void ToyLightAtPosition(const float *light, int position, const float *axes, float *moved)
{
	float const radius = (float)(ToyLightIsFlare(light) ? ToyShadowsFlareSize : ToyShadowsLampSize) * 0.5f;
	float const *place = kToyPositions[position];

	memcpy(moved, light, kToyLightFloats * sizeof(float));

	for (int i = 0; i < 3; i++)
	{
		moved[i] = light[i] + radius * (place[0] * axes[0 + i] + place[1] * axes[3 + i] + place[2] * axes[6 + i]);
		moved[kToyLightWorld + i] = light[kToyLightWorld + i] + radius * place[i];
	}
}

// Could this module's shadow show, from any light on the list? A position moves a shadow, so each one a light is drawn from gets asked about,
// as well as the light's true place, which a cloaked Predator's shadow and any single-position pass are cast from.
bool ModuleShadowCanShow(const ShadowReachBall &ball, const float *lights, int numLights, const float *axes)
{
	for (int i = 0; i < numLights; i++)
	{
		float const *light = &lights[i * kToyLightFloats];

		if (LightOnListIsDark(light))
		{
			continue;
		}

		if (ShadowReach_CanReach(shadowReachView, drawnBoxes, numDrawnBoxes, ball, light))
		{
			return true;
		}

		int const positions = ToyPositionsForLight(light);

		for (int position = 0; positions > 1 && position < positions; position++)
		{
			float moved[kToyLightFloats];
			ToyLightAtPosition(light, position, axes, moved);

			if (ShadowReach_CanReach(shadowReachView, drawnBoxes, numDrawnBoxes, ball, moved))
			{
				return true;
			}
		}
	}

	return false;
}

// One lamp, from all its positions. Each adds its part of the lamp's share to the mask, through the same stencil and shader as a
// single position. Where the lamp's light falls on a pixel doesn't change: only how much of it gets blocked.
static HRESULT ToyShadeFromEveryPosition(LPDIRECT3DDEVICE9 device, const float *light, const void *owner, int positions, const float *axes, bool haveCloaked, ToyFrameReport *report, IDirect3DQuery9 *countQuery, unsigned int *pixels)
{
	float const part = 1.0f / (float)positions;
	HRESULT result = S_OK;

	for (int position = 0; position < positions; position++)
	{
		float moved[kToyLightFloats];
		ToyLightAtPosition(light, position, axes, moved);

		ToyBuildPrisms(moved, light, owner, report, false);
		ToyMarkShadow(device, report);

		HRESULT const darkened = ToyShadeLevel(device, light, part, countQuery, pixels);
		if (FAILED(darkened))
		{
			result = darkened;
		}
	}

	// a cloaked Predator's shadow is faint, from something that can barely be seen, so one position is plenty
	if (haveCloaked)
	{
		unsigned int notCounted = 0;

		ToyBuildPrisms(light, light, owner, report, true);
		ToyMarkShadow(device, report);
		ToyShadeLevel(device, light, kToyCloakedShadow, 0, &notCounted);
	}

	return result;
}

// One flare's mask, from all its positions. The first clears the picture, and the rest add to it. The flare's colour, strength and
// cap don't come into this: they stay with the flare's true place, so a flare out in the open looks the same as a single point.
static void ToyPaintFlareFromEveryPosition(LPDIRECT3DDEVICE9 device, IDirect3DSurface9 *screen, const D3DVIEWPORT9 *viewport, const RECT *reach, const float *light, const void *owner, int positions, const float *axes, bool haveCloaked, ToyFrameReport *report, IDirect3DQuery9 *countQuery, unsigned int *pixels)
{
	float const part = 1.0f / (float)positions;

	for (int position = 0; position < positions; position++)
	{
		float moved[kToyLightFloats];
		ToyLightAtPosition(light, position, axes, moved);

		ToyBuildPrisms(moved, light, owner, report, false);
		ToyMarkShadow(device, report);
		ToyPaintFlareMask(device, screen, viewport, reach, position != 0, part, countQuery, pixels);
	}

	// a cloaked Predator's shadow is faint, from something that can barely be seen, so one position is plenty
	if (haveCloaked)
	{
		unsigned int notCounted = 0;

		ToyBuildPrisms(light, light, owner, report, true);
		ToyMarkShadow(device, report);
		ToyPaintFlareMask(device, screen, viewport, reach, true, kToyCloakedShadow, 0, &notCounted);
	}
}

// What's in a picture on the card, read back on report frames only, because it makes the processor wait for the card. Every fourth
// pixel each way is plenty for an average.
struct ToyPictureReading
{
	bool read;
	float average;
	float largest;
	float partOver;   // how much of the picture is over the amount that was asked about, 0 to 1
};

// A 16-bit float as the card stores it: a sign, 5 bits of size and 10 of detail.
static float ToyHalfFloat(unsigned short half)
{
	int const size = (half >> 10) & 31;
	int const detail = half & 1023;
	float const value = (size == 0) ? ldexpf((float)detail, -24)
		: (size == 31) ? 65536.0f
		: ldexpf(1.0f + (float)detail / 1024.0f, size - 15);

	return (half & 0x8000) ? -value : value;
}

// Failing to read is no harm: read stays false, and the log says so. A picture with several samples to a pixel can't be read this way.
// Says whether the readers below know how big this kind of picture's pixels are. They walk along a row by that size, so a kind they don't know could take them past
// the end of the copy. The screen can be one: the game falls back to a display format of three bytes a pixel if the four-byte ones fail (DisplayFormats, in d3_func.cpp).
static bool CanReadPicture(D3DFORMAT format)
{
	return format == D3DFMT_R32F || format == D3DFMT_R16F || format == D3DFMT_A16B16G16R16F || format == D3DFMT_A8R8G8B8 || format == D3DFMT_X8R8G8B8;
}

static void ToyReadPicture(LPDIRECT3DDEVICE9 device, IDirect3DSurface9 *picture, float over, ToyPictureReading *reading)
{
	D3DSURFACE_DESC size;
	IDirect3DSurface9 *copy = 0;
	D3DLOCKED_RECT locked;

	reading->read = false;
	reading->average = 0.0f;
	reading->largest = 0.0f;
	reading->partOver = 0.0f;

	if (FAILED(picture->GetDesc(&size))
		|| !CanReadPicture(size.Format)
		|| FAILED(device->CreateOffscreenPlainSurface(size.Width, size.Height, size.Format, D3DPOOL_SYSTEMMEM, &copy, NULL)))
	{
		return;
	}

	if (SUCCEEDED(device->GetRenderTargetData(picture, copy)) && SUCCEEDED(copy->LockRect(&locked, NULL, D3DLOCK_READONLY)))
	{
		double total = 0.0;
		int samples = 0, overCount = 0;

		for (UINT y = 0; y < size.Height; y += 4)
		{
			unsigned char const *row = (unsigned char const *)locked.pBits + y * locked.Pitch;

			for (UINT x = 0; x < size.Width; x += 4)
			{
				float value;

				if (size.Format == D3DFMT_R16F)
				{
					value = ToyHalfFloat(*(unsigned short const *)(row + x * 2));
				}
				else if (size.Format == D3DFMT_A16B16G16R16F)
				{
					value = ToyHalfFloat(*(unsigned short const *)(row + x * 8));
				}
				else
				{
					// blue, green, red, and one spare. For the screen this is a simple average of the colours, 0 to 255
					unsigned char const *pixel = row + x * 4;
					value = ((float)pixel[0] + (float)pixel[1] + (float)pixel[2]) / 3.0f;
					if (size.Format == D3DFMT_A8R8G8B8 && picture == toyMaskSurface)
					{
						value = (float)pixel[2] / 255.0f;
					}
				}

				total += value;
				samples++;
				overCount += (value > over) ? 1 : 0;
				reading->largest = (value > reading->largest) ? value : reading->largest;
			}
		}

		copy->UnlockRect();

		if (samples)
		{
			reading->read = true;
			reading->average = (float)(total / samples);
			reading->partOver = (float)overCount / (float)samples;
		}
	}

	copy->Release();
}

// Reads a picture back, and gives every second pixel each way as a number. It makes the game wait for the card, so it's only with -shadowflicker.
static bool ReadSamples(LPDIRECT3DDEVICE9 device, IDirect3DSurface9 *picture, std::vector<float> &samples)
{
	D3DSURFACE_DESC size;
	IDirect3DSurface9 *copy = 0;
	D3DLOCKED_RECT locked;
	bool read = false;

	if (FAILED(picture->GetDesc(&size))
		|| !CanReadPicture(size.Format)
		|| FAILED(device->CreateOffscreenPlainSurface(size.Width, size.Height, size.Format, D3DPOOL_SYSTEMMEM, &copy, NULL)))
	{
		return false;
	}

	if (SUCCEEDED(device->GetRenderTargetData(picture, copy)) && SUCCEEDED(copy->LockRect(&locked, NULL, D3DLOCK_READONLY)))
	{
		samples.clear();

		for (UINT y = 0; y < size.Height; y += 2)
		{
			unsigned char const *row = (unsigned char const *)locked.pBits + y * locked.Pitch;

			for (UINT x = 0; x < size.Width; x += 2)
			{
				if (size.Format == D3DFMT_R32F)
				{
					samples.push_back(*(float const *)(row + x * 4));
				}
				else if (size.Format == D3DFMT_R16F)
				{
					samples.push_back(ToyHalfFloat(*(unsigned short const *)(row + x * 2)));
				}
				else if (size.Format == D3DFMT_A16B16G16R16F)
				{
					samples.push_back(ToyHalfFloat(*(unsigned short const *)(row + x * 8)));
				}
				else
				{
					// blue, green, red and a spare. A mask in this format keeps its number in red, from 0 to 1, and the screen is an average, from 0 to 255
					unsigned char const *pixel = row + x * 4;
					samples.push_back((picture == toyMaskSurface) ? (float)pixel[2] / 255.0f : ((float)pixel[0] + (float)pixel[1] + (float)pixel[2]) / 3.0f);
				}
			}
		}

		copy->UnlockRect();
		read = true;
	}

	copy->Release();
	return read;
}

// One picture for the flicker number.
static void ReadPictureSamples(LPDIRECT3DDEVICE9 device, IDirect3DSurface9 *picture, FlickerPicture which)
{
	static std::vector<float> samples;

	if (ReadSamples(device, picture, samples))
	{
		Flicker_AddPicture(which, samples);
	}
}

// A small number for what carries a light, for a capture's numbers. The real thing is a place in memory, and that's no use to anyone reading them. 0 means nothing does.
static int OwnerNumber(const void *owner)
{
	static void const *seen[64];

	if (!owner)
	{
		return 0;
	}

	for (int i = 0; i < 64; i++)
	{
		if (seen[i] == owner)
		{
			return i + 1;
		}

		if (!seen[i])
		{
			seen[i] = owner;
			return i + 1;
		}
	}

	return 65;
}

// One frame's lines for a capture's numbers.txt.
static void NoteCaptureNumbers(int frame, const float *lights, const void *const *owners, int numLights, const unsigned int *pixels)
{
	float view[12];
	ToyShadows_ViewMatrix(view);

	char line[500];
	std::string numbers;

	_snprintf(line, sizeof(line) - 1,
		"frame %d, view %s: camera at %.1f %.1f %.1f; right %.4f %.4f %.4f, up %.4f %.4f %.4f, forward %.4f %.4f %.4f; %d polygons on the list, %d walls from the fixed buffer, %d kept groups made, %d prisms beyond range left out\n",
		frame, kToyViewNames[ToyShadowsMode], toyCameraInWorld[0], toyCameraInWorld[1], toyCameraInWorld[2],
		view[0], view[1], view[2], view[4], view[5], view[6], view[8], view[9], view[10],
		toyNumCasters, numWalkedGroups, keptGroupsMadeThisFrame, prismsOutOfRange);
	line[sizeof(line) - 1] = 0;
	numbers += line;

	for (int i = 0; i < numLights && i < kToyMostLights; i++)
	{
		float const *light = &lights[i * kToyLightFloats];

		if (LightOnListIsDark(light))
		{
			continue;
		}

		_snprintf(line, sizeof(line) - 1,
			"  light %d: kind %.0f, at %.1f %.1f %.1f, range %.0f, strength %.3f, owner %d, %d positions, corners sent %d, corners kept %d, pixels marked %u\n",
			i, light[7], light[kToyLightWorld + 0], light[kToyLightWorld + 1], light[kToyLightWorld + 2], light[3], (light[4] + light[5] + light[6]) / 3.0f,
			OwnerNumber(owners[i]), ToyPositionsForLight(light), captureCornersSent[i], captureCornersKept[i], pixels[i]);
		line[sizeof(line) - 1] = 0;
		numbers += line;
	}

	ShadowCapture_AddNumbers(numbers);
}

static void ToyForgetCasters()
{
	FrameTiming_SetPolygonsCollected(toyNumCasters);
	captureLightBeingBuilt = -1;
	keptGroupsMadeThisFrame = 0;
	memset(captureCornersSent, 0, sizeof(captureCornersSent));
	memset(captureCornersKept, 0, sizeof(captureCornersKept));

	ToyShadows_NextCollectingFrame();
	toyNumUnseenModules = 0;
	toyNumCasters = 0;
	toyNumCasterPoints = 0;
	numKeptGroups = 0;
	numWalkedGroups = 0;
	toyCastersLeftOut = 0;
	toyLastModel = 0;
	toyThisModelPolygons = 0;
	toyNumModels = 0;
	toyLargestModel = 0;
	for (int kind = 0; kind < kToyCasterKinds; kind++)
	{
		toyPolygonsOfKind[kind] = 0;
		toyVertsOfKind[kind] = 0;
	}
	toyPrismsLeftOut = 0;
	toyNumVerts = 0;
	toyStretchTotal = 0.0;
	toyStretchCorners = 0;
	earlyFlushThisFrame = false;
	SurfaceNormals_ForgetModulesDrawn();
	polygonsReachKept = 0;
	polygonsReachLeftOut = 0;
	prismsOutOfRange = 0;
	vertsOutOfRange = 0;
	numDrawnBoxes = 0;
}

// Whether the depth buffer the game was given has stencil bits, which is where a light's shadow marks go. The game asks for only two
// kinds that have them (DepthFormats, d3_func.cpp). On a card that offers neither, the pass would work from marks that were never
// made, and what it drew would be up to the driver, most likely a dark world.
static bool ToyDepthBufferHasStencil()
{
	D3DFORMAT const format = d3d.d3dpp.AutoDepthStencilFormat;

	return format == D3DFMT_D24S8 || format == D3DFMT_D24FS8;
}

// Whether lights can be added per pixel. They go through the shadow pass, so that needs stencil bits as well as the pictures. Asking makes the pictures if they
// aren't there yet, so anything that has no use for them shouldn't ask.
static bool PerPixelLightsCanRun()
{
	return ToyDepthBufferHasStencil() && ToyPicturesCanRun(d3d.lpD3DDevice);
}

// What's really being drawn, whatever was asked for
static int LightingReallyDrawn()
{
	if (LightingNow == kLightingGouraud || !PerPixelLightsCanRun())
	{
		return kLightingGouraud;
	}

	return Lighting_Drawn(LightingNow, SurfacePictures_ReliefCanRun(), true);
}

// One line of the tag, with its right edge at rightX. The same sums as D3D_RenderHUDString_Centred, which is the game's own text, at another size.
static void DrawStatusLine(const char *text, int rightX, int y, int scale)
{
	int x = rightX;

	for (char const *p = text; *p; p++)
	{
		x -= MUL_FIXED(scale, AAFontWidths[(unsigned char)*p]);
	}

	VertexTag quadVertices[4];

	quadVertices[0].Y = y - MUL_FIXED(scale, 1);
	quadVertices[1].Y = y - MUL_FIXED(scale, 1);
	quadVertices[2].Y = y + MUL_FIXED(scale, HUD_FONT_HEIGHT + 1);
	quadVertices[3].Y = y + MUL_FIXED(scale, HUD_FONT_HEIGHT + 1);

	for (char const *p = text; *p; p++)
	{
		unsigned char const c = (unsigned char)*p;
		int const topLeftU = 1 + ((c - 32) & 15) * 16;
		int const topLeftV = 1 + ((c - 32) >> 4) * 16;

		quadVertices[0].U = topLeftU - 1;
		quadVertices[0].V = topLeftV;
		quadVertices[1].U = topLeftU + HUD_FONT_WIDTH + 1;
		quadVertices[1].V = topLeftV;
		quadVertices[2].U = topLeftU + HUD_FONT_WIDTH + 1;
		quadVertices[2].V = topLeftV + HUD_FONT_HEIGHT + 1;
		quadVertices[3].U = topLeftU - 1;
		quadVertices[3].V = topLeftV + HUD_FONT_HEIGHT + 1;

		quadVertices[0].X = x - MUL_FIXED(scale, 1);
		quadVertices[3].X = x - MUL_FIXED(scale, 1);
		quadVertices[1].X = x + MUL_FIXED(scale, HUD_FONT_WIDTH + 1);
		quadVertices[2].X = x + MUL_FIXED(scale, HUD_FONT_WIDTH + 1);

		D3D_HUDQuad_Output(AAFontImageNumber, quadVertices, 0xFFFFFFFF, FILTERING_BILINEAR_ON);

		x += MUL_FIXED(scale, AAFontWidths[c]);
	}
}

// The tag on the top right of the screen, which says how this build is running, so that a moment in a video can be matched to the log. It's only
// a few more polygons on the list the display already uses, so it can't hold anything up. It's only there while a level is being shown.
static void ShadowStatus_Draw()
{
	const char *ToyShadows_RoomName();
	bool ToyShadows_TakeViewRan();

	bool const viewRan = ToyShadows_TakeViewRan();

	if (!DevMode || !ShadowStatusOn || !viewRan)
	{
		return;
	}

	enum { kMostLines = 13, kLineLength = 48 };
	char lines[kMostLines][kLineLength];
	int numLines = 0;

	_snprintf(lines[numLines++], kLineLength - 1, "KEPT PRISMS %s%s", KeptPrismsOff ? "OFF" : ShadowVolumesCard_Ready() ? "ON" : "ON, NO STORE", ShadowVolumesCard_TagWords());
	_snprintf(lines[numLines++], kLineLength - 1, "LIGHTING %s, SHADOWS %s", Lighting_Word(LightingReallyDrawn()), ShadowVolumesOn ? "ON" : "OFF");
	_snprintf(lines[numLines++], kLineLength - 1, "STATIC %s", StaticSceneOn ? (StaticScene_TookAny() ? "SCENE" : "SCENE WAITING") : (StaticSurfaceOn || StaticSurfaceCompare) ? "SURFACE" : "OFF");
	if (ShadowsDepthGap || ShadowsEvenGap || ShadowBiasSteps)
	{
		_snprintf(lines[numLines++], kLineLength - 1, "%s%sBIAS %d", ShadowsDepthGap ? ((StaticSceneOn || !KeptPrismsOff) ? "DEPTH GAP PART " : "DEPTH GAP ") : "", ShadowsEvenGap ? "EVEN GAP " : "", ShadowBiasSteps ? ShadowBiasSteps : 17);
	}

	_snprintf(lines[numLines++], kLineLength - 1, "%s", kToyViewNames[ToyShadowsMode]);
	_snprintf(lines[numLines++], kLineLength - 1, "COMMIT %s", AVP_BUILD_COMMIT);
	_snprintf(lines[numLines++], kLineLength - 1, "FRAME %d", toyFrame + 1);
	_snprintf(lines[numLines++], kLineLength - 1, "ROOM %s", ToyShadows_RoomName());
	_snprintf(lines[numLines++], kLineLength - 1, "KEYS: G VIEW, F7 TAG, F8 CAPTURE");   // so that nobody has to remember them
	_snprintf(lines[numLines++], kLineLength - 1, "KEYS: L LIGHTING, H SHADOWS");

	if (ShadowShootOn)
	{
		_snprintf(lines[numLines++], kLineLength - 1, ShadowShoot_NumStopped() ? "F9 SHOOT, F10 STOP, F5 UNDO: %d STOPPED" : "F9 SHOOT, F10 STOP, F5 UNDO", ShadowShoot_NumStopped());
	}


	if (ShadowFlickerOn)
	{
		_snprintf(lines[numLines++], kLineLength - 1, "COUNTING FLICKER");
	}

	if (ShadowCapture_IsOn())
	{
		_snprintf(lines[numLines++], kLineLength - 1, "CAPTURING");
	}

	int characters = 0;

	for (int i = 0; i < numLines; i++)
	{
		lines[i][kLineLength - 1] = 0;

		// a room's name comes from the level file, and only these have a place in the font
		for (char *p = lines[i]; *p; p++, characters++)
		{
			if ((unsigned char)*p < 32 || (unsigned char)*p > 126)
			{
				*p = '?';
			}
		}
	}

	// half the HUD's own size is still easy to read in a video, and hides less
	int const scale = (HUDScaleFactor / 2 > ONE_FIXED) ? HUDScaleFactor / 2 : ONE_FIXED;
	int const lineHeight = MUL_FIXED(scale, HUD_FONT_HEIGHT + 3);
	int const margin = MUL_FIXED(scale, 6);
	int const rightX = ScreenDescriptorBlock.SDB_Width - margin;

	int widest = 0;

	for (int i = 0; i < numLines; i++)
	{
		int width = 0;

		for (char const *p = lines[i]; *p; p++)
		{
			width += MUL_FIXED(scale, AAFontWidths[(unsigned char)*p]);
		}

		widest = (width > widest) ? width : widest;
	}

	// Room for the backing and every letter, or none of it. It leaves half the list free as well, because the tag is only for looking at, and coronas
	// are added to this list after it.
	if (!orthoList->Check(2048 + 4 + characters * 4) || widest + margin * 2 > (int)ScreenDescriptorBlock.SDB_Width)
	{
		return;
	}

	// a dark backing, so that the white letters can be read over anything
	DrawQuad(rightX - widest - margin, margin / 2, widest + margin * 2 - margin / 2, lineHeight * numLines + margin, NO_TEXTURE, RCOLOR_ARGB(150, 0, 0, 0), TRANSLUCENCY_NORMAL);

	for (int i = 0; i < numLines; i++)
	{
		DrawStatusLine(lines[i], rightX, margin + i * lineHeight, scale);
	}
}

// Puts the shoot tool's words on the screen, if it has any.
static void ShootSayWords(const char *words)
{
	static char shootWords[64];

	if (words)
	{
		_snprintf(shootWords, sizeof(shootWords) - 1, "%s", words);
		shootWords[sizeof(shootWords) - 1] = 0;
		NewOnScreenMessage(shootWords);
	}
}

// Shows the polygon last shot in green and each stopped polygon in red, so that anyone can see it's the one they meant. The colour stays on what's nearest, and the outline shows through
// anything, so a polygon with something in front of it can still be found. Its corners are where it was when it was shot, so on something moving, it stays behind.
static void ShootDrawTints(LPDIRECT3DDEVICE9 device)
{
	float view[12];
	float corners[kShootMostCorners * 3];
	int count = 0;
	bool stopped = false;

	ToyShadows_ViewMatrix(view);

	for (int number = 0; ShadowShoot_TintNumber(number, corners, &count, &stopped); number++)
	{
		if (count < 3 || count > kShootMostCorners)
		{
			continue;
		}

		D3DCOLOR const fill = stopped ? 0x70FF0000 : 0x7000FF00;
		D3DCOLOR const outline = stopped ? 0xFFFF0000 : 0xFF00FF00;
		ToyVec onList[kShootMostCorners];

		for (int i = 0; i < count; i++)
		{
			Shoot_WorldToList(view, &corners[i * 3], &onList[i].x);
		}

		int verts = 0;

		for (int i = 1; i + 1 < count; i++)
		{
			int const order[3] = { 0, i, i + 1 };

			for (int corner = 0; corner < 3; corner++)
			{
				D3DLVERTEX &out = toyChunk[verts++];

				out.x = onList[order[corner]].x;
				out.y = onList[order[corner]].y;
				out.z = onList[order[corner]].z;
				out.color = fill;
				out.specular = 0;
				out.u = 0.0f;
				out.v = 0.0f;
			}
		}

		ToySetVolumeState(device);
		device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
		device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
		device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
		device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
		device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
		device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

		// a little nearer than the wall it's on, or the two would fight over the same depth
		ToySetFloatState(device, D3DRS_DEPTHBIAS, -0.00001f);
		device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, verts / 3, toyChunk, sizeof(D3DLVERTEX));
		ToySetFloatState(device, D3DRS_DEPTHBIAS, 0.0f);

		for (int i = 0; i <= count; i++)
		{
			D3DLVERTEX &out = toyChunk[i];
			ToyVec const &corner = onList[i % count];

			out.x = corner.x;
			out.y = corner.y;
			out.z = corner.z;
			out.color = outline;
			out.specular = 0;
			out.u = 0.0f;
			out.v = 0.0f;
		}

		device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
		device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		device->DrawPrimitiveUP(D3DPT_LINESTRIP, count, toyChunk, sizeof(D3DLVERTEX));
	}
}

// What the commands and keys have asked for, until the end of this frame's shadows. See view_switches.h.
static int shadowsAsked = kAskedNothing;
static int lightingAsked = kAskedNothing;

// Whether LIGHTING and SHADOWS were both off when last looked at, for G and for seeing when that changes
static bool switchesWereAllOff = false;
static int viewToComeBackTo = kToyViewFinal;

static void SaySwitch(const char *words)
{
	static char message[100];
	_snprintf(message, sizeof(message) - 1, "%s", words);
	message[sizeof(message) - 1] = 0;
	NewOnScreenMessage(message);
	LogString(message);
}

// A reason is worth hearing once. After that, a press only needs to say what it did.
static void SayReasonOnce(bool *said, const char *words)
{
	if (!*said)
	{
		*said = true;
		SaySwitch(words);
	}
}

void ShadowsCommand(char *words)
{
	if (!SwitchWords_ShadowsAsk(words, &shadowsAsked))
	{
		SaySwitch("SHADOWS TAKES 0 OR 1, OR NOTHING TO SWITCH IT");
	}
}

void LightingCommand(char *words)
{
	if (!SwitchWords_LightingAsk(words, &lightingAsked))
	{
		SaySwitch("LIGHTING TAKES 1 RELIEF, 2 PERPIXEL OR 3 GOURAUD, OR NOTHING FOR THE NEXT");
	}
}

// Where LIGHTING and SHADOWS change. It runs after this frame's shadows, so everything the next frame reads, from its lights onwards, agrees.
static void ViewSwitches_ApplyAsked()
{
	// Not while typing, or an L in the console or a chat message would change the lighting. Only with -dev, so that a player's own controls can't be taken by accident.
	if (DevMode && IOFOCUS_AcceptControls() && !(IOFOCUS_Get() & (IOFOCUS_NEWCONSOLE | IOFOCUS_OSK)))
	{
		if (DebouncedKeyboardInput[KEY_L])
		{
			lightingAsked = kAskedNext;
		}

		if (DebouncedKeyboardInput[KEY_H])
		{
			shadowsAsked = kAskedNext;
		}
	}

	bool const hasStencil = ToyDepthBufferHasStencil();

	// ToyShadows_Draw has already put the view at OFF for good on a card like this. Holding these off as well means nothing here can ever bring it back.
	if (!hasStencil)
	{
		ShadowVolumesOn = 0;
		LightingNow = kLightingGouraud;
	}

	static char const kNoStencil[] = "THE DEPTH BUFFER HAS NO STENCIL BITS, SO THERE ARE NO SHADOWS AND ONLY GOURAUD LIGHTING";

	if (shadowsAsked != kAskedNothing)
	{
		static bool saidNoStencil = false;

		if (hasStencil)
		{
			ShadowVolumesOn = Shadows_AfterAsking(ShadowVolumesOn, shadowsAsked);
		}

		SaySwitch(ShadowVolumesOn ? "SHADOWS: ON" : "SHADOWS: OFF");

		if (!hasStencil)
		{
			SayReasonOnce(&saidNoStencil, kNoStencil);
		}

		shadowsAsked = kAskedNothing;
	}

	if (lightingAsked != kAskedNothing)
	{
		static bool saidNoStencil = false, saidNoPerPixel = false, saidNoRelief = false;

		bool const perPixelThere = PerPixelLightsCanRun();
		bool const reliefThere = SurfacePictures_ReliefCanRun();

		LightingNow = Lighting_AfterAsking(LightingNow, lightingAsked, reliefThere, perPixelThere);

		static char message[40];
		_snprintf(message, sizeof(message) - 1, "LIGHTING: %s", Lighting_Word(Lighting_Drawn(LightingNow, reliefThere, perPixelThere)));
		message[sizeof(message) - 1] = 0;
		SaySwitch(message);

		if (!hasStencil)
		{
			SayReasonOnce(&saidNoStencil, kNoStencil);
		}
		else if (!perPixelThere)
		{
			SayReasonOnce(&saidNoPerPixel, "THIS CARD CANNOT DRAW LIGHTS PER PIXEL, SO ONLY GOURAUD IS THERE");
		}
		else if (!reliefThere)
		{
			SayReasonOnce(&saidNoRelief, textureRelief.IsOn() ? "THIS CARD CANNOT DRAW RELIEF, SO IT IS SKIPPED" : "RELIEF WAS NOT BUILT, BECAUSE OF -RELIEFOFF, SO IT IS SKIPPED");
		}

		lightingAsked = kAskedNothing;
	}

	bool const allOff = !ShadowVolumesOn && LightingReallyDrawn() == kLightingGouraud;

	ToyShadowsMode = View_AfterSwitches(ToyShadowsMode, kToyViewOff, switchesWereAllOff, allOff, &viewToComeBackTo);
	switchesWereAllOff = allOff;

	// The next frame's lights are chosen before its polygons are known, from this. Going to Gouraud, they have to stay in the game's own lighting from that very frame.
	if (LightingNow == kLightingGouraud)
	{
		ToyShadowsLightsByPixel = 0;
	}
}

static void ToyShadows_Draw()
{
	// With no stencil bits, shadows turn off and the game runs as it always has. This comes before the G key, so nothing can turn
	// them back on, and with ToyShadowsMode at 0 nothing collects polygons or chooses lights.
	if (!ToyDepthBufferHasStencil())
	{
		if (ToyShadowsMode)
		{
			LogString("toy-shadows: the depth buffer has no stencil bits, so shadows stay off");
			ToyShadowsMode = 0;
		}

		ToyShadowsLightsByPixel = 0;
		ToyForgetCasters();
		return;
	}

	// G, which no default control set uses. Not while typing, or a G in the console or a chat message would change the view.
	if (DevMode && DebouncedKeyboardInput[KEY_G] && IOFOCUS_AcceptControls() && !(IOFOCUS_Get() & (IOFOCUS_NEWCONSOLE | IOFOCUS_OSK)))
	{
		ToyShadowsMode = View_AfterG(ToyShadowsMode, kToyViewCount, kToyViewFinal, switchesWereAllOff);

		static char message[40];
		_snprintf(message, sizeof(message) - 1, "VIEW: %s", kToyViewNames[ToyShadowsMode]);
		message[sizeof(message) - 1] = 0;
		NewOnScreenMessage(message);
		LogString(message);
	}

	// F8 saves the next frames as images, and F7 hides the tag on the screen. Neither is used by the game. Not while typing, like G.
	if (DevMode && IOFOCUS_AcceptControls() && !(IOFOCUS_Get() & (IOFOCUS_NEWCONSOLE | IOFOCUS_OSK)))
	{
		if (DebouncedKeyboardInput[KEY_F8])
		{
			ShadowCapture_Start(toyFrame + 1);
		}

		if (DebouncedKeyboardInput[KEY_F7])
		{
			ShadowStatusOn = !ShadowStatusOn;
		}

		// F9 shoots, F10 stops the polygon last shot from casting, and F5 undoes the newest stop. See shadow_shoot.cpp. The game leaves these three keys alone.
		if (ShadowShootOn)
		{
			const char *words = 0;

			if (DebouncedKeyboardInput[KEY_F9])
			{
				ShadowShoot_AskForShot();
			}

			if (DebouncedKeyboardInput[KEY_F10])
			{
				words = ShadowShoot_StopCasting(!KeptPrismsOff);
			}

			if (DebouncedKeyboardInput[KEY_F5])
			{
				words = ShadowShoot_UndoStop();
			}

			ShootSayWords(words);
		}
	}

	// Chosen live lights are ours while the real view is on. With no polygons this frame the pass isn't running, so they go back to
	// the game. This takes effect from the next frame, because this frame's lights were chosen before its polygons were known.
	bool const lightsOurs = (ToyShadowsMode == kToyViewFinal || ToyShadowsMode == kToyViewMask || ToyShadowsMode == kViewNormals)
		&& (toyNumCasters > 0 || numWalkedGroups > 0) && ToyPicturesCanRun(d3d.lpD3DDevice);

	bool const logThisFrame = (++toyFrame % kFramesBetweenReports) == 0;

	FrameTiming_SetFrameNumber(toyFrame);

	ToyFrameReport report = { toyFrame, toyNumCasters, 0, 0, S_OK, S_OK, S_OK, S_OK, S_OK, 0, 0.0f, 0.0f };

	LARGE_INTEGER started, finished, ticksPerSecond;
	QueryPerformanceCounter(&started);
	QueryPerformanceFrequency(&ticksPerSecond);

	if (logThisFrame)
	{
		static LARGE_INTEGER lastReport = { 0 };

		if (lastReport.QuadPart)
		{
			report.frameMilliseconds = (float)((double)(started.QuadPart - lastReport.QuadPart) * 1000.0 / (double)ticksPerSecond.QuadPart / 300.0);
		}

		lastReport = started;
	}

	// chosen before this frame's lighting ran: see ToyShadows_ChooseLights
	float lights[kToyMostLights * kToyLightFloats];
	void const *lightOwners[kToyMostLights];
	report.lights = (toyNumCasters || numWalkedGroups) ? ToyShadows_ChosenLights(lights, lightOwners) : 0;

	if (!report.lights)
	{
		if (ShadowShootOn)
		{
			ShootSayWords(ShadowShoot_DropAskedShot(toyFrame, (toyNumCasters || numWalkedGroups) ? "this frame has no lights chosen" : "nothing on this frame casts a shadow"));
		}

		if (logThisFrame)
		{
			ToyLogFrame(report);
		}
		ToyShadowsLightsByPixel = lightsOurs;
		ToyForgetCasters();
		return;
	}

	float worldAxes[9];
	ToyShadows_WorldAxes(worldAxes);
	StartKeptPrisms(lights, report.lights);

	// These go after everything that was drawn, so if the lists fill up, it's these that miss out. By now everything drawn is known, which is
	// what the view needs.
	MakeShadowReachView();
	polygonsReachKept = ToyShadows_CollectOutOfView(lights, report.lights, worldAxes, &polygonsReachLeftOut);

	LPDIRECT3DDEVICE9 device = d3d.lpD3DDevice;

	// The game caches its render states, so everything is put back exactly as it was, whatever happens here.
	{
		ProcessorStageClock stateClock(kProcessorStateBlock);
		report.block = savedCardSettings ? savedCardSettings->Capture() : device->CreateStateBlock(D3DSBT_ALL, &savedCardSettings);
	}
	if (FAILED(report.block))
	{
		if (ShadowShootOn)
		{
			ShootSayWords(ShadowShoot_DropAskedShot(toyFrame, "the card's settings couldn't be saved on this frame"));
		}

		if (logThisFrame)
		{
			ToyLogFrame(report);
		}

		// The pass can't run this frame, so next frame's lights have to stay in the game's lighting, or nothing would draw them.
		ToyShadowsLightsByPixel = 0;
		ToyForgetCasters();
		return;
	}

	int litLights = 0;

	for (int light = 0; light < report.lights; light++)
	{
		if (!LightOnListIsDark(&lights[light * kToyLightFloats]))
		{
			litLights++;
		}
	}

	// Without the pictures, each light takes a flat share, so that a pixel none of them reach ends up at the full darkness.
	float const keepPerLight = powf(1.0f - kToyFullDarkness, 1.0f / (float)(litLights ? litLights : 1));
	ToyShadows_CameraPlace(toyCameraInWorld);

	if (ShadowShootOn)
	{
		extern int KeptSnap;
		float view[12];
		int crosshairX = -1, crosshairY = -1;

		ToyShadows_ViewMatrix(view);
		ShadowShoot_CrosshairPixel(&crosshairX, &crosshairY);

		ShootFrameFacts const facts =
		{
			toyFrame, AVP_BUILD_COMMIT, ToyShadows_RoomName(), view, toyCameraInWorld,
			d3d.matProjection._11, d3d.matProjection._22, d3d.matProjection._33, d3d.matProjection._43,
			crosshairX, crosshairY, KeptSnap == 2, ShadowVolumesCard_CornersAreCut(), ToyShadowsTest != 0
		};

		ShadowShoot_StartFrame(device, facts);
	}

	DWORD const flatShare = (DWORD)((1.0f - keepPerLight) * 255.0f) << 24;

	// with the full report, ask the card how many pixels the stencil let through
	bool const fullReport = logThisFrame && ShadowsFullReport != 0;
	IDirect3DQuery9 *countQuery = 0;
	bool const capturingFrame = ShadowCapture_IsOn();

	if ((fullReport || capturingFrame) && FAILED(device->CreateQuery(D3DQUERYTYPE_OCCLUSION, &countQuery)))
	{
		countQuery = 0;
	}

	ToyMeasurements measured = { 0, 0, 0, 0, 0, 0 };
	unsigned int pixelsByLight[kToyMostLights] = { 0 };
	bool const counting = countQuery != 0;
	bool measuredThisFrame = false;
	float const *measuredLight = lights;
	ToyVec sampleNear = { 0.0f, 0.0f, 0.0f };
	ToyVec sampleFar = { 0.0f, 0.0f, 0.0f };

	// The darkening below goes into the mask, not onto the screen. Without the pictures, it falls back to a flat share. From here on, having
	// screen means the pictures are drawn.
	IDirect3DSurface9 *screen = 0;
	D3DVIEWPORT9 viewport;

	bool const darkening = ToyShadowsMode == kToyViewFinal || ToyShadowsMode == kToyViewMask || ToyShadowsMode == kViewNormals;
	bool const haveCloaked = toyPolygonsOfKind[kToyCasterCloaked] > 0;

	if (darkening && ToyPicturesCanRun(device)
		&& SUCCEEDED(device->GetRenderTarget(0, &screen)) && SUCCEEDED(device->GetViewport(&viewport)))
	{
		device->SetRenderTarget(0, toyDepthSurface);
		device->SetViewport(&viewport);
		device->Clear(0, NULL, D3DCLEAR_TARGET, 0xFFFFFFFF, 1.0f, 0);

		bool const useStatic = ChooseStaticRoute();

		if (logThisFrame && StaticSurfaceCompare)
		{
			if (!useStatic)
			{
				LogString("static compare: the fixed buffer can't draw this frame, so nothing was compared");
			}
			else if (earlyFlushThisFrame)
			{
				LogString("static compare: this frame's lists flushed early, so the old route only holds the last of them, and nothing was compared");
			}
			else
			{
				CompareSurfaceRoutes(device, worldAxes, toyFrame);

				if (StaticSceneOn)
				{
					CompareSceneRoutes(device, toyFrame);
				}
			}
		}

		if (ToyDrawPictures(device, worldAxes, useStatic))
		{
			FrameTiming_Mark(kStageSurfacePictures);
			PixelLights_DrawTotal(device, lights, report.lights, toyDepthTexture);
			FrameTiming_Mark(kStageLightTotal);

			// the mask starts at nothing blocked
			device->SetRenderTarget(0, toyMaskSurface);
			device->SetViewport(&viewport);
			device->Clear(0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0);
		}
		else
		{
			// the pictures hold last frame's surfaces, so this frame takes the flat shadows
			device->SetRenderTarget(0, screen);
			device->SetViewport(&viewport);
			screen->Release();
			screen = 0;
		}
	}
	else if (screen)
	{
		screen->Release();
		screen = 0;
	}

	// On a report frame, what each light did, and what reached the screen. A mask under this size of number has no effect worth
	// counting as a shadow. All of this only reads: nothing here changes what's drawn.
	float const kToyMaskCounts = 0.02f;
	bool const noting = fullReport && screen != 0;

	// Every prism this frame is built after this line. The edges view draws no pictures, but it's there to show what a frame with them would use.
	prismsForPictures = screen != 0 || ((ToyShadowsMode == kToyViewEdges || ToyShadowsMode == kViewRedVolumes) && ToyPicturesCanRun(device));

	// these only read what's already on the processor's side, so they don't wait for anything
	if (logThisFrame && screen != 0)
	{
		ToyCheckStoredPlanes(worldAxes);
		CheckNoPolygonSeenFromBehind();
		ReportKeptPrisms();
	}

	ToyLightReport lightReports[kToyMostLights];
	ToyPictureReading maskReading = { false, 0.0f, 0.0f, 0.0f };
	ToyPictureReading screenBefore = maskReading, screenDarkened = maskReading, screenAtEnd = maskReading;
	int lastLampNoted = -1;
	float maskSoFar = 0.0f;

	memset(lightReports, 0, sizeof(lightReports));

	for (int light = 0; light < report.lights; light++)
	{
		float const *thisLight = &lights[light * kToyLightFloats];

		captureLightBeingBuilt = light;

		// lights added per pixel come after the world has been darkened
		if (thisLight[7] == 2.0f || LightOnListIsDark(thisLight))
		{
			continue;
		}

		// a light that can't reach the screen has nothing to do. The edges view still shows it.
		if (ToyShadowsMode != kToyViewEdges && ToyShadowsMode != kViewRedVolumes)
		{
			RECT reach;
			if (!ToyLightRectangle(device, thisLight, &reach))
			{
				continue;
			}

			device->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
			device->SetScissorRect(&reach);
		}

		// the mask before this light is the mask after the one before it
		if (noting)
		{
			ToyReadPicture(device, toyMaskSurface, kToyMaskCounts, &maskReading);

			if (lastLampNoted >= 0)
			{
				lightReports[lastLampNoted].maskRise = maskReading.average - maskSoFar;
			}

			maskSoFar = maskReading.average;
			lastLampNoted = light;
			lightReports[light].drawn = true;
			toyNoteLight = &lightReports[light];
		}

		// a light drawn from several positions, which only the mask can add up
		int const positions = screen ? ToyPositionsForLight(thisLight) : 1;
		if (positions > 1)
		{
			HRESULT const darkened = ToyShadeFromEveryPosition(device, thisLight, lightOwners[light], positions, worldAxes, haveCloaked, &report, countQuery, &pixelsByLight[light]);
			if (FAILED(darkened))
			{
				report.quad = darkened;
			}
			report.pixels += pixelsByLight[light];
			continue;
		}

		ToyBuildPrisms(thisLight, thisLight, lightOwners[light], &report, false);

		if (ToyShadowsMode == kToyViewEdges)
		{
			ToyDrawWires(device, 0xFF000000 | kToyLightColours[light]);
			continue;
		}

		if (fullReport && countQuery && !measuredThisFrame && ToyShadowsMode == kToyViewPolygons)
		{
			measuredLight = thisLight;

			// the first thing each prism writes is a near corner, then the far corner that goes with it
			if (toyNumVerts >= 6)
			{
				sampleNear = toyVerts[0];
				sampleFar = toyVerts[3];
			}

			ToyMeasure(device, countQuery, &measured);
			measuredThisFrame = true;
		}

		ToyMarkShadow(device, &report);

		HRESULT darkened;
		if (ToyShadowsMode == kToyViewPolygons)
		{
			darkened = ToyDarkenStencilledPixels(device, 0x60000000 | kToyLightColours[light], countQuery, &pixelsByLight[light]);
		}
		else if (ToyShadowsMode == kViewRedVolumes)
		{
			darkened = ToyDarkenStencilledPixels(device, 0xA0FF0000, countQuery, &pixelsByLight[light]);
			ToyDrawWires(device, 0xFFFF0000);
		}
		else if (screen)
		{
			darkened = ToyShadeLevel(device, thisLight, 1.0f, countQuery, &pixelsByLight[light]);

			if (haveCloaked)
			{
				unsigned int notCounted = 0;

				ToyBuildPrisms(thisLight, thisLight, lightOwners[light], &report, true);
				ToyMarkShadow(device, &report);
				ToyShadeLevel(device, thisLight, kToyCloakedShadow, 0, &notCounted);
			}
		}
		else
		{
			darkened = ToyDarkenStencilledPixels(device, flatShare, countQuery, &pixelsByLight[light]);
		}

		if (FAILED(darkened))
		{
			report.quad = darkened;
		}
		report.pixels += pixelsByLight[light];
	}

	device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

	toyNoteLight = 0;

	if (noting)
	{
		ToyReadPicture(device, toyMaskSurface, kToyMaskCounts, &maskReading);

		if (lastLampNoted >= 0)
		{
			lightReports[lastLampNoted].maskRise = maskReading.average - maskSoFar;
		}
	}

	if (ToyShadowsMode == kToyViewEdges || ToyShadowsMode == kToyViewPolygons)
	{
		ToyDrawLightMarkers(device, lights, report.lights);
	}
	else if (ToyShadowsMode != kViewRedVolumes)
	{
		if (screen)
		{
			device->SetRenderTarget(0, screen);
			device->SetViewport(&viewport);

			if (noting)
			{
				ToyReadPicture(device, screen, 255.0f, &screenBefore);
			}

			FrameTiming_Mark(kStageShading);

			if (capturingFrame)
			{
				ShadowCapture_TakeMask(device, toyMaskSurface);
			}

			if (ShadowFlickerOn)
			{
				static std::vector<float> distances;

				if (!ReadSamples(device, toyDepthSurface, distances))
				{
					// Last frame's distances could be wrong for this frame, so there are none. The pictures still count, but not by distance. The bands would
					// read 0 beside totals that aren't, so the log says why, once.
					static bool toldAboutDistances = false;

					if (!toldAboutDistances)
					{
						LogString("flicker: the distance picture couldn't be read, so the frames it failed on aren't counted by distance");
						toldAboutDistances = true;
					}

					distances.clear();
				}

				Flicker_SetDistances(distances);

				ReadPictureSamples(device, toyMaskSurface, kFlickerMask);
			}

			ToyApplySoftenedMask(device, ToyShadowsMode == kToyViewMask);
			FrameTiming_Mark(kStageSoftening);

			if (noting)
			{
				ToyReadPicture(device, screen, 255.0f, &screenDarkened);
			}
		}

		if (ToyShadowsMode == kToyViewFinal)
		{
			ToyDimWorld(device);
			FrameTiming_Mark(kStageDimming);
		}

		for (int light = 0; light < report.lights; light++)
		{
			float const *thisLight = &lights[light * kToyLightFloats];

			captureLightBeingBuilt = light;

			// Without the pictures there's no adding a light. That's only ever one frame: the first after they're lost, before the lights go back to the game.
			RECT reach;
			if (screen && thisLight[7] == 2.0f && !LightOnListIsDark(thisLight) && ToyLightRectangle(device, thisLight, &reach))
			{
				device->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
				device->SetScissorRect(&reach);

				if (noting)
				{
					lightReports[light].drawn = true;
					toyNoteLight = &lightReports[light];
				}

				int const positions = ToyPositionsForLight(thisLight);
				if (positions > 1)
				{
					ToyPaintFlareFromEveryPosition(device, screen, &viewport, &reach, thisLight, lightOwners[light], positions, worldAxes, haveCloaked, &report, countQuery, &pixelsByLight[light]);

					if (noting)
					{
						ToyPictureReading flareMask;
						ToyReadPicture(device, toyMaskSurface, kToyMaskCounts, &flareMask);
						lightReports[light].maskRise = flareMask.average;
					}

					ToyAddLight(device, thisLight);
					report.pixels += pixelsByLight[light];
					device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
					continue;
				}

				ToyBuildPrisms(thisLight, thisLight, lightOwners[light], &report, false);
				ToyMarkShadow(device, &report);
				ToyPaintFlareMask(device, screen, &viewport, &reach, false, 1.0f, countQuery, &pixelsByLight[light]);

				if (haveCloaked)
				{
					unsigned int notCounted = 0;

					ToyBuildPrisms(thisLight, thisLight, lightOwners[light], &report, true);
					ToyMarkShadow(device, &report);
					ToyPaintFlareMask(device, screen, &viewport, &reach, true, kToyCloakedShadow, 0, &notCounted);
				}

				if (noting)
				{
					ToyPictureReading flareMask;
					ToyReadPicture(device, toyMaskSurface, kToyMaskCounts, &flareMask);
					lightReports[light].maskRise = flareMask.average;
				}

				ToyAddLight(device, thisLight);
				report.pixels += pixelsByLight[light];

				device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
			}
		}

		toyNoteLight = 0;
		FrameTiming_Mark(kStageAddingLights);

		if (screen)
		{
			if (ToyShadowsMode == kViewNormals)
			{
				SurfacePictures_ShowNormals(device);
			}

			if (noting)
			{
				ToyReadPicture(device, screen, 255.0f, &screenAtEnd);
			}

			if (ShadowFlickerOn)
			{
				ReadPictureSamples(device, screen, kFlickerScreen);
			}

			screen->Release();
		}
	}

	if (countQuery)
	{
		countQuery->Release();
	}

	if (capturingFrame)
	{
		NoteCaptureNumbers(toyFrame, lights, lightOwners, report.lights, pixelsByLight);

		if (ShadowShootOn)
		{
			ShadowCapture_AddNumbers("-shadowshoot is on, so these pictures can have its green and red tints on them\n");
		}
	}

	if (ShadowFlickerOn)
	{
		Flicker_EndFrame(toyFrame, toyCameraInWorld, worldAxes);
	}

	if (ShadowShootOn)
	{
		ShadowShoot_EndFrame(device, lights, report.lights, screen != 0, toyMaskSurface, toyDepthSurface, toyDepthTexture);
		ShootDrawTints(device);
		ShootSayWords(ShadowShoot_EndOfEveryFrame());
	}

	{
		ProcessorStageClock stateClock(kProcessorStateBlock);
		savedCardSettings->Apply();
	}

	if (logThisFrame)
	{
		QueryPerformanceCounter(&finished);
		report.milliseconds = (float)((double)(finished.QuadPart - started.QuadPart) * 1000.0 / (double)ticksPerSecond.QuadPart);
		ToyLogFrame(report);

		char line[480];

		// the number that says what the positions are costing: each pass is its own prisms, stencil and mask
		int passes = 0;
		for (int i = 0; i < report.lights; i++)
		{
			if (!LightOnListIsDark(&lights[i * kToyLightFloats]))
			{
				passes += darkening ? ToyPositionsForLight(&lights[i * kToyLightFloats]) : 1;
			}
		}
		_snprintf(line, sizeof(line) - 1, "toy-shadows: %d passes for %d lit lights; %d positions, on flares and the nearest %d others; lamp size %d, flare size %d",
			passes, litLights, ToyShadowsPositions, ToyShadowsNearLights, ToyShadowsLampSize, ToyShadowsFlareSize);
		line[sizeof(line) - 1] = 0;
		LogString(line);

		if (noting)
		{
			// Each light's own numbers, all from this one frame. Pixels are added up over its passes, and they count every surface
			// the light can't reach, including surfaces turned away from it, and everything past its range. The share and the keep
			// are what the marked pixels really lost, on average: the mask's rise spread back over the pixels that caused it.
			float const screenPixels = (float)viewport.Width * (float)viewport.Height;
			float const darkest = (float)ToyShadowsDarkness / 100.0f;

			for (int i = 0; i < report.lights; i++)
			{
				float const *thisLight = &lights[i * kToyLightFloats];

				if (LightOnListIsDark(thisLight))
				{
					continue;
				}

				ToyLightReport const *did = &lightReports[i];
				int const passesHere = ToyPositionsForLight(thisLight);
				float const kind = thisLight[7];
				float const distance = sqrtf(thisLight[0] * thisLight[0] + thisLight[1] * thisLight[1] + thisLight[2] * thisLight[2]);
				float const weight = (thisLight[4] + thisLight[5] + thisLight[6]) / 3.0f;
				float const perPass = (float)pixelsByLight[i] / (float)passesHere;
				float const share = (kind != 2.0f && pixelsByLight[i]) ? did->maskRise * screenPixels * (float)passesHere / (float)pixelsByLight[i] : 0.0f;

				if (kind == 2.0f)
				{
					_snprintf(line, sizeof(line) - 1,
						"toy-shadows: frame %d, light %d, added per pixel: %.0f away at %.0f %.0f %.0f, range %.0f, strength %.2f, %s, %d passes marked %u pixels (%.1f%% of the screen a pass), %d polygons in reach, %d prisms, %d kept corners, nearest caster %.0f, its mask averages %.3f",
						report.frame, i, distance, thisLight[0], thisLight[1], thisLight[2], thisLight[3], weight, did->drawn ? "drawn" : "off screen",
						passesHere, pixelsByLight[i], 100.0f * perPass / screenPixels, did->polygonsInReach, did->prismsBuilt, did->keptCorners, did->nearestCaster, did->maskRise);
				}
				else
				{
					_snprintf(line, sizeof(line) - 1,
						"toy-shadows: frame %d, light %d, kind %.0f: %.0f away at %.0f %.0f %.0f, range %.0f, weight %.2f, %s, %d passes painted %u pixels (%.1f%% of the screen a pass), %u stayed marked, %d polygons in reach, %d prisms, %d kept corners, nearest caster %.0f, mask rose %.4f, so a painted pixel lost a share of %.3f and keeps %.3f",
						report.frame, i, kind, distance, thisLight[0], thisLight[1], thisLight[2], thisLight[3], weight, did->drawn ? "drawn" : "off screen",
						passesHere, pixelsByLight[i], 100.0f * perPass / screenPixels, did->stayedMarked, did->polygonsInReach, did->prismsBuilt, did->keptCorners, did->nearestCaster, did->maskRise, share, 1.0f - darkest * share);
				}

				line[sizeof(line) - 1] = 0;
				LogString(line);
			}

			// The mask is before softening. The screen is an average of its colours, 0 to 255: before darkening, after it, and at the
			// end of the pass with the world dimmed and the flares added. -1 means it couldn't be read.
			_snprintf(line, sizeof(line) - 1,
				"toy-shadows: frame %d, end result: mask %s, average %.3f, largest %.3f, over %.2f on %.1f%% of the screen, so the screen keeps %.3f on average; screen %.1f before, %.1f darkened, %.1f at the end",
				report.frame, maskReading.read ? "read" : "NOT READ", maskReading.average, maskReading.largest, kToyMaskCounts, 100.0f * maskReading.partOver,
				1.0f - darkest * (maskReading.average < 1.0f ? maskReading.average : 1.0f),
				screenBefore.read ? screenBefore.average : -1.0f, screenDarkened.read ? screenDarkened.average : -1.0f, screenAtEnd.read ? screenAtEnd.average : -1.0f);
			line[sizeof(line) - 1] = 0;
			LogString(line);
		}

		if (measuredThisFrame)
		{
			float const distance = sqrtf(measuredLight[0] * measuredLight[0] + measuredLight[1] * measuredLight[1] + measuredLight[2] * measuredLight[2]);

			_snprintf(line, sizeof(line) - 1,
				"toy-shadows: first light, distance %.0f, range %.0f: behind the scene back %u front %u, in front of it back %u front %u, stencil control %u, z-pass %u, unanswered %d",
				distance, measuredLight[3], measured.backBehind, measured.frontBehind, measured.backInFront, measured.frontInFront,
				measured.stencilControl, measured.zPass, toyQueriesUnanswered);
			line[sizeof(line) - 1] = 0;
			LogString(line);

			_snprintf(line, sizeof(line) - 1,
				"toy-shadows: first light at %.0f %.0f %.0f, a prism from %.0f %.0f %.0f to %.0f %.0f %.0f, corners thrown %.0f on average",
				measuredLight[0], measuredLight[1], measuredLight[2], sampleNear.x, sampleNear.y, sampleNear.z, sampleFar.x, sampleFar.y, sampleFar.z,
				toyStretchCorners ? toyStretchTotal / toyStretchCorners : 0.0);
			line[sizeof(line) - 1] = 0;
			LogString(line);
		}

		if (counting)
		{
			// (only the full report counts pixels)
			_snprintf(line, sizeof(line) - 1, "toy-shadows: pixels in shadow by light: %u %u %u %u %u %u %u %u %u %u %u %u",
				pixelsByLight[0], pixelsByLight[1], pixelsByLight[2], pixelsByLight[3],
				pixelsByLight[4], pixelsByLight[5], pixelsByLight[6], pixelsByLight[7],
				pixelsByLight[8], pixelsByLight[9], pixelsByLight[10], pixelsByLight[11]);
			line[sizeof(line) - 1] = 0;
			LogString(line);
		}
		else if (fullReport)
		{
			LogString("toy-shadows: the card gave no occlusion query, so every pixel count on this frame means nothing");
		}
	}

	ToyShadowsLightsByPixel = lightsOurs;
	ToyForgetCasters();
}
