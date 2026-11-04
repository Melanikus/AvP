// AvP port by Barry Duncan.
// Modified 2026-11-03 by Melanikus for the AvP port:
//   a small menu letter is skipped when the font picture is missing or empty, instead of being drawn with
//   broken picture coordinates

#include "console.h"
#include "OnScreenKeyboard.h"
#include "TextureManager.h"
#include "r2base.h"
#include "Fonts.h"
#include <vector>
#include <algorithm>
#include "logString.h"
#include "RenderList.h"
#include "VertexBuffer.h"
#include "FmvCutscenes.h"
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

// AvP uses numVerts to represent the actual number of polygon vertices we're passing to the backend.
// we then use the OUTPUT_TRIANGLE function to generate the indices required to represent those
// polygons. This function calculates the number of indices required based on the number of vertices.
// AvP code defined these, so hence the magic numbers.

uint32_t GetNumIndices(uint32_t numVerts)
{
	uint32_t numIndices = 0;

	switch (numVerts)
	{
		case 3:
			numIndices = 3;
			break;
		case 4:
			numIndices = 6;
			break;
		case 5:
			numIndices = 9;
			break;
		case 6:
			numIndices = 12;
			break;
		case 7:
			numIndices = 15;
			break;
		case 8:
			numIndices = 18;
			break;
		case 0:
			numIndices = 0;
			break;
		case 256:
			numIndices = 1350;
			break;
		default:
			// need to check this is ok!!
			numIndices = numVerts;
	}
	return numIndices;
}

bool staticDecalsTest = false;

// lock our dynamic vertex and index buffers, and reset counters and array indexes used to keep 
// track of the number of verts and indices in those buffers. Function could be renamed as 
// we no longer use an execute buffer
static bool LockExecuteBuffer()
{
	// lock particle vertex and index buffers
	d3d.particleVB->Lock((void**)&particleVertex);
	d3d.particleIB->Lock(&particleIndex);

	// lock main vertex and index buffers
	d3d.mainVB->Lock((void**)&mainVertex);
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
	// unlock particle vertex and index buffers
	d3d.particleVB->Unlock();
	d3d.particleIB->Unlock();

	// unlock main vertex and index buffers
	d3d.mainVB->Unlock();
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

float cot(float in)
{
	return 1.0f / tan(in);
}

static bool toyFinalFlush = false;
static void ToyShadows_Draw();
void ToyShadows_NoteFrameLights();

static bool ExecuteBuffer()
{
	// sort the list of render objects
	particleList->Sort();
	decalList->Sort();
//	mainList->Sort();

	// these two just add the vertex data to the below lists (they dont draw anything themselves
	// and they HAVE to be called before the below code)
	DrawParticles();
	DrawCoronas();

	if (mainList->GetSize())
	{
		// set main shaders to active
		d3d.effectSystem->SetActive(d3d.mainEffect);

		// we don't need world matrix here as avp has done all the world transforms itself
		R_MATRIX matWorldViewProj = /*d3d.matView **/ d3d.matProjection;

		d3d.effectSystem->SetVertexShaderConstant(d3d.mainEffect, 0, CONST_MATRIX, &matWorldViewProj);

		// draw our main list (level geometry, player weapon etc)
		mainList->Draw();
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
		ToyShadows_Draw();
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

	orthoList->AddItem(4, textureID, TRANSLUCENCY_OFF, FILTERING_BILINEAR_ON, TEXTURE_CLAMP);

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
		orthoList->AddItem(4, textureID, TRANSLUCENCY_GLOWING, FILTERING_BILINEAR_ON, TEXTURE_CLAMP);

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

	orthoList->AddItem(4, NO_TEXTURE, TRANSLUCENCY_GLOWING, FILTERING_BILINEAR_ON, TEXTURE_WRAP);

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

	orthoList->AddItem(4, textureID, translucencyType, FILTERING_BILINEAR_OFF, TEXTURE_CLAMP);

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

	orthoList->AddItem(4, textureID, translucencyType, FILTERING_BILINEAR_ON, TEXTURE_CLAMP);

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

	orthoList->AddItem(4, AAFontImageNumber, TRANSLUCENCY_GLOWING, FILTERING_BILINEAR_ON, TEXTURE_CLAMP);

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

		orthoList->AddItem(4, SpecialFXImageNumber, (enum TRANSLUCENCY_TYPE)particleDescPtr->TranslucencyType, FILTERING_BILINEAR_ON, TEXTURE_CLAMP);

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

void D3D_ZBufferedGouraudTexturedPolygon_Output(POLYHEADER *inputPolyPtr, RENDERVERTEX *renderVerticesPtr)
{
	// bjd - This is the function responsible for drawing level geometry and the players weapon
	if (!mainList->Check(RenderPolygon.NumberOfVertices)) {
		UnlockExecuteBufferAndPrepareForUse();
		ExecuteBuffer();
		LockExecuteBuffer();
	}

	texID_t textureID = (inputPolyPtr->PolyColour & ClrTxDefn);

	uint32_t texWidth, texHeight;
	Tex_GetDimensions(textureID, texWidth, texHeight);

	float RecipW = 1.0f / /*(float)*/ texWidth;
	float RecipH = 1.0f / /*(float)*/ texHeight;

	if (gunLayer) {
		weaponList->AddItem(RenderPolygon.NumberOfVertices, textureID, RenderPolygon.TranslucencyMode);
	} else {
		mainList->AddItem(RenderPolygon.NumberOfVertices, textureID, RenderPolygon.TranslucencyMode);
	}

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

void D3D_ZBufferedGouraudPolygon_Output(POLYHEADER *inputPolyPtr, RENDERVERTEX *renderVerticesPtr)
{
	// responsible for drawing predators lock on triangle, for one
	if (!mainList->Check(RenderPolygon.NumberOfVertices)) {
		UnlockExecuteBufferAndPrepareForUse();
		ExecuteBuffer();
		LockExecuteBuffer();
	}

	// Take header information
	int32_t flags = inputPolyPtr->PolyFlags;

	mainList->AddItem(RenderPolygon.NumberOfVertices, NO_TEXTURE, RenderPolygon.TranslucencyMode);

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
		UnlockExecuteBufferAndPrepareForUse();
		ExecuteBuffer();
		LockExecuteBuffer();
	}

	mainList->AddItem(RenderPolygon.NumberOfVertices, NO_TEXTURE, TRANSLUCENCY_OFF);

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
		UnlockExecuteBufferAndPrepareForUse();
		ExecuteBuffer();
		LockExecuteBuffer();
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
		weaponList->AddItem(RenderPolygon.NumberOfVertices, textureID, TRANSLUCENCY_NORMAL);
	} else {
		mainList->AddItem(RenderPolygon.NumberOfVertices, textureID, TRANSLUCENCY_NORMAL);
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

	orthoList->AddItem(4, NO_TEXTURE, TRANSLUCENCY_GLOWING, FILTERING_BILINEAR_ON, TEXTURE_CLAMP);

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

	orthoList->AddItem(4, textureID, TRANSLUCENCY_GLOWING, filteringType, TEXTURE_CLAMP);

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

		mainList->AddItem(3, NO_TEXTURE, TRANSLUCENCY_NORMAL);

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

	decalList->AddItem(RenderPolygon.NumberOfVertices, textureID, decalDescPtr->TranslucencyType, FILTERING_BILINEAR_ON, TEXTURE_CLAMP);

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

	particleList->AddItem(4, SpecialFXImageNumber, (enum TRANSLUCENCY_TYPE)particleDescPtr->TranslucencyType, FILTERING_BILINEAR_ON, TEXTURE_CLAMP);

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

	orthoList->AddItem(4, StaticImageNumber, TRANSLUCENCY_GLOWING, FILTERING_BILINEAR_ON, TEXTURE_WRAP);

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

	orthoList->AddItem(4, PredatorNumbersImageNumber, TRANSLUCENCY_NORMAL, FILTERING_BILINEAR_ON, TEXTURE_WRAP);

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
		UnlockExecuteBufferAndPrepareForUse();
		ExecuteBuffer();
		LockExecuteBuffer();
	}

	// We assume bit 15 (TxLocal) HAS been
	// properly cleared this time...
	texID_t textureID = (inputPolyPtr->PolyColour & ClrTxDefn);

	uint32_t texWidth, texHeight;
	Tex_GetDimensions(textureID, texWidth, texHeight);

	// divide by 65536.0f to convert from fixed point
	float RecipW = (1.0f / (float) texWidth)  / 65536.0f;
	float RecipH = (1.0f / (float) texHeight) / 65536.0f;

	mainList->AddItem(RenderPolygon.NumberOfVertices, textureID, RenderPolygon.TranslucencyMode, FILTERING_BILINEAR_ON, TEXTURE_WRAP);

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
		UnlockExecuteBufferAndPrepareForUse();
		ExecuteBuffer();
		LockExecuteBuffer();
	}

	mainList->AddItem(kNumMeshVerts, currentWaterTexture, TRANSLUCENCY_NORMAL);

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
	if (R_BeginScene()) // calls a function to perform d3d_device->Begin();
	{
		LockExecuteBuffer(); // lock vertex buffer
	}
}

size_t lastMem = 0;

void ThisFramesRenderingHasFinished()
{
	Osk_Draw();

	Con_Draw();

	ToyShadows_NoteFrameLights();

	UnlockExecuteBufferAndPrepareForUse();
	toyFinalFlush = true;
	ExecuteBuffer();
	toyFinalFlush = false;
	R_EndScene();

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
	UnlockExecuteBufferAndPrepareForUse();
	ExecuteBuffer();
	LockExecuteBuffer();

	for (uint32_t i = 0; i < 255;)
	{
		orthoList->AddItem(4, NO_TEXTURE, TRANSLUCENCY_OFF, FILTERING_BILINEAR_ON, TEXTURE_CLAMP);

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

	orthoList->AddItem(4, NO_TEXTURE, TRANSLUCENCY_NORMAL, FILTERING_BILINEAR_ON, TEXTURE_WRAP);

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

	orthoList->AddItem(4, BurningImageNumber, TRANSLUCENCY_GLOWING, FILTERING_BILINEAR_ON, TEXTURE_WRAP);

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

	theta[0] = (CloakingPhase/8)&4095;
	theta[1] = (800-CloakingPhase/8)&4095;

	orthoList->AddItem(4, SpecialFXImageNumber, TRANSLUCENCY_DARKENINGCOLOUR, FILTERING_BILINEAR_ON, TEXTURE_WRAP);

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
			orthoList->AddItem(4, SpecialFXImageNumber, TRANSLUCENCY_COLOUR, FILTERING_BILINEAR_ON, TEXTURE_WRAP);
		}
	}
}

extern void D3D_PredatorScreenInversionOverlay()
{
	uint32_t colour = 0xffffffff;

	orthoList->AddItem(4, NO_TEXTURE, TRANSLUCENCY_DARKENINGCOLOUR, FILTERING_BILINEAR_ON, TEXTURE_WRAP);

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

	orthoList->AddItem(4, SpecialFXImageNumber, TRANSLUCENCY_INVCOLOUR, FILTERING_BILINEAR_ON, TEXTURE_WRAP);

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
			orthoList->AddItem(4, SpecialFXImageNumber, TRANSLUCENCY_GLOWING, FILTERING_BILINEAR_ON, TEXTURE_WRAP);
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

int ToyShadowsEnabled = 1;
int ToyShadowsDebug = 0;      // -shadowdebug: shows the first light's volumes as red wires and the shadowed pixels in red
int ToyShadowsMaxLights = 1;  // -shadowlights=N, 1 to 8
int ToyShadowsTest = 0;       // -shadowtest: a small box round each light, so everything in its range must go dark

struct ToyVec
{
	float x, y, z;
};

static const int   kToyMostLights      = 8;
static const int   kToyMostPolyPoints  = 16;
static const int   kToyMaxCasters      = 60000;
static const int   kToyMaxCasterPoints = 240000;
static const int   kToyMaxVerts        = 1200000;
static const float kToyStartGap        = 8.0f;      // start just behind the polygon, or surfaces shadow themselves in speckles
static const float kToyWidestGap       = 200.0f;    // the most that gap can stretch to, at a very low angle
static const float kToyMostRange       = 100000.0f; // well inside the far plane, which z-fail needs
static const float kToyMostReach       = 400000.0f; // how far a corner can be thrown, also well inside the far plane
static const float kToyFullDarkness    = 0.44f;     // a pixel every chosen light fails to reach loses this much of its brightness

static ToyVec toyCasterPoints[kToyMaxCasterPoints];
static int toyCasterStart[kToyMaxCasters];
static int toyCasterCount[kToyMaxCasters];
static int toyNumCasters = 0;
static int toyNumCasterPoints = 0;
static int toyCastersLeftOut = 0;

static ToyVec toyVerts[kToyMaxVerts];
static int toyNumVerts = 0;
static int toyPrismsLeftOut = 0;
static double toyStretchTotal = 0.0;   // how far corners were thrown past their polygon, for the log
static int toyStretchCorners = 0;

int ToyShadows_PickLights(float *lights, int most);

// pts holds count points, x y z, in the same space mainVertex uses (view space with y turned over)
void ToyShadows_AddCaster(const float *pts, int count)
{
	if (count < 3 || count > kToyMostPolyPoints)
	{
		return;
	}

	if (toyNumCasters == kToyMaxCasters || toyNumCasterPoints + count > kToyMaxCasterPoints)
	{
		toyCastersLeftOut++;
		return;
	}

	toyCasterStart[toyNumCasters] = toyNumCasterPoints;
	toyCasterCount[toyNumCasters] = count;
	toyNumCasters++;

	for (int i = 0; i < count; i++)
	{
		toyCasterPoints[toyNumCasterPoints].x = pts[i * 3 + 0];
		toyCasterPoints[toyNumCasterPoints].y = pts[i * 3 + 1];
		toyCasterPoints[toyNumCasterPoints].z = pts[i * 3 + 2];
		toyNumCasterPoints++;
	}
}

static inline void ToyTri(const ToyVec &a, const ToyVec &b, const ToyVec &c)
{
	toyVerts[toyNumVerts++] = a;
	toyVerts[toyNumVerts++] = b;
	toyVerts[toyNumVerts++] = c;
}

// light holds x y z range
static void ToyAddPrism(const ToyVec *pts, int count, const float *light)
{
	int const needed = ((count - 2) * 2 + count * 2) * 3;
	if (toyNumVerts + needed > kToyMaxVerts)
	{
		toyPrismsLeftOut++;
		return;
	}

	float const ax = pts[1].x - pts[0].x, ay = pts[1].y - pts[0].y, az = pts[1].z - pts[0].z;
	float const bx = pts[2].x - pts[0].x, by = pts[2].y - pts[0].y, bz = pts[2].z - pts[0].z;
	float const nx = ay * bz - az * by;
	float const ny = az * bx - ax * bz;
	float const nz = ax * by - ay * bx;

	float const lx = light[0] - pts[0].x, ly = light[1] - pts[0].y, lz = light[2] - pts[0].z;
	float const facing = nx * lx + ny * ly + nz * lz;
	float const normalLength = sqrtf(nx * nx + ny * ny + nz * nz);
	float const lightDistance = sqrtf(lx * lx + ly * ly + lz * lz);

	if (normalLength < 0.001f || lightDistance < 1.0f || facing == 0.0f)
	{
		return;
	}

	// the polygon's flat surface is out of the light's reach
	float const planeDistance = fabsf(facing) / normalLength;
	if (planeDistance >= light[3])
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

	// the lower the light hits the surface, the further the start has to move to get behind it by the same amount
	float const angle = fabsf(facing) / (normalLength * lightDistance);
	float gap = kToyStartGap / (angle > 0.01f ? angle : 0.01f);
	if (gap > kToyWidestGap)
	{
		gap = kToyWidestGap;
	}

	ToyVec nearPts[kToyMostPolyPoints], farPts[kToyMostPolyPoints];

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

		dx /= length;
		dy /= length;
		dz /= length;

		float reach = length * scale;
		if (reach > kToyMostReach)
		{
			reach = kToyMostReach;
		}
		if (reach < length + gap + 1.0f)
		{
			reach = length + gap + 1.0f;   // the far end must stay behind the near end, or the prism turns inside out
		}

		toyStretchTotal += reach - length;
		toyStretchCorners++;

		nearPts[i].x = point.x + dx * gap;
		nearPts[i].y = point.y + dy * gap;
		nearPts[i].z = point.z + dz * gap;
		farPts[i].x  = light[0] + dx * reach;
		farPts[i].y  = light[1] + dy * reach;
		farPts[i].z  = light[2] + dz * reach;
	}

	for (int i = 1; i < count - 1; i++)
	{
		ToyTri(nearPts[0], nearPts[i], nearPts[i + 1]);
		ToyTri(farPts[0], farPts[i + 1], farPts[i]);
	}

	for (int i = 0; i < count; i++)
	{
		int const next = (i + 1) % count;
		ToyTri(nearPts[i], farPts[next], nearPts[next]);
		ToyTri(nearPts[i], farPts[i], farPts[next]);
	}
}

// through the game's own vertex shader and declaration, so these land exactly where the world's polygons do
static HRESULT ToyDrawVolumes(D3DCOLOR colour)
{
	const int kVertsPerDraw = 60000 * 3;
	static D3DLVERTEX chunk[kVertsPerDraw];
	HRESULT result = S_OK;

	for (int start = 0; start < toyNumVerts; start += kVertsPerDraw)
	{
		int const verts = (toyNumVerts - start < kVertsPerDraw) ? toyNumVerts - start : kVertsPerDraw;

		for (int i = 0; i < verts; i++)
		{
			chunk[i].x = toyVerts[start + i].x;
			chunk[i].y = toyVerts[start + i].y;
			chunk[i].z = toyVerts[start + i].z;
			chunk[i].color = colour;
			chunk[i].specular = 0;
			chunk[i].u = 0.0f;
			chunk[i].v = 0.0f;
		}

		HRESULT const drawn = d3d.lpD3DDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST, verts / 3, chunk, sizeof(D3DLVERTEX));
		if (FAILED(drawn))
		{
			result = drawn;
		}
	}

	return result;
}

// What one frame did. The hr values stay S_OK unless a call failed.
struct ToyFrameReport
{
	int frame;
	int casters;
	int lights;
	int verts;
	HRESULT block, clear, back, front, quad;
	unsigned int pixels;
	float milliseconds;
};

static void ToyLogFrame(const ToyFrameReport &report)
{
	char line[240];
	_snprintf(line, sizeof(line) - 1,
		"toy-shadows: frame %d: %d polygons (%d left out), %d lights, %d verts (%d prisms left out), state block %08x, clear %08x, back %08x, front %08x, quad %08x, %u pixels in shadow, %.1f ms",
		report.frame, report.casters, toyCastersLeftOut, report.lights, report.verts, toyPrismsLeftOut,
		(unsigned int)report.block, (unsigned int)report.clear, (unsigned int)report.back, (unsigned int)report.front,
		(unsigned int)report.quad, report.pixels, report.milliseconds);
	line[sizeof(line) - 1] = 0;
	LogString(line);
}

static void ToySetVolumeState(LPDIRECT3DDEVICE9 device)
{
	d3d.effectSystem->SetActive(d3d.mainEffect);
	d3d.effectSystem->SetVertexShaderConstant(d3d.mainEffect, 0, CONST_MATRIX, &d3d.matProjection);
	d3d.mainDecl->Set();

	device->SetPixelShader(NULL);
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

static int toyQueriesUnanswered = 0;

// The card answers when it gets there, so this waits, but not for ever. A count that never came is noted, not taken for a 0.
static unsigned int ToyAskCard(IDirect3DQuery9 *countQuery)
{
	DWORD counted = 0;
	HRESULT answer = S_FALSE;

	for (int tries = 0; tries < 1000000 && answer == S_FALSE; tries++)
	{
		answer = countQuery->GetData(&counted, sizeof(counted), D3DGETDATA_FLUSH);
	}

	if (answer != S_OK)
	{
		toyQueriesUnanswered++;
		return 0;
	}

	return counted;
}

static HRESULT ToyDarkenStencilledPixels(LPDIRECT3DDEVICE9 device, DWORD shade, IDirect3DQuery9 *countQuery, unsigned int *pixels)
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
		{ 0.0f, 0.0f, 0.0f, 1.0f, shade },
		{ w,    0.0f, 0.0f, 1.0f, shade },
		{ 0.0f, h,    0.0f, 1.0f, shade },
		{ w,    h,    0.0f, 1.0f, shade },
	};

	device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
	device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
	device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_NOTEQUAL);
	device->SetRenderState(D3DRS_STENCILREF, 0);
	device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	device->SetVertexShader(NULL);
	device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);

	if (countQuery)
	{
		countQuery->Issue(D3DISSUE_BEGIN);
	}

	HRESULT const result = device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(ToyScreenVertex));

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
static void ToyAddTestBox(const float *light)
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
		ToyAddPrism(points, 4, light);
	}
}

static void ToyForgetCasters()
{
	toyNumCasters = 0;
	toyNumCasterPoints = 0;
	toyCastersLeftOut = 0;
	toyPrismsLeftOut = 0;
	toyNumVerts = 0;
	toyStretchTotal = 0.0;
	toyStretchCorners = 0;
}

static void ToyShadows_Draw()
{
	static int toyFrame = 0;
	bool const logThisFrame = (++toyFrame % 300) == 0;

	ToyFrameReport report = { toyFrame, toyNumCasters, 0, 0, S_OK, S_OK, S_OK, S_OK, S_OK, 0, 0.0f };

	LARGE_INTEGER started, finished, ticksPerSecond;
	QueryPerformanceCounter(&started);

	float lights[kToyMostLights * 4];
	int const most = ToyShadowsMaxLights > kToyMostLights ? kToyMostLights : ToyShadowsMaxLights;
	report.lights = toyNumCasters ? ToyShadows_PickLights(lights, most) : 0;

	if (!report.lights)
	{
		if (logThisFrame && ToyShadowsEnabled)
		{
			ToyLogFrame(report);
		}
		ToyForgetCasters();
		return;
	}

	LPDIRECT3DDEVICE9 device = d3d.lpD3DDevice;

	// The game caches its render states, so everything is put back exactly as it was, whatever happens here.
	IDirect3DStateBlock9 *savedState = 0;
	report.block = device->CreateStateBlock(D3DSBT_ALL, &savedState);
	if (FAILED(report.block))
	{
		if (logThisFrame)
		{
			ToyLogFrame(report);
		}
		ToyForgetCasters();
		return;
	}

	// Each light takes a share, chosen so that a pixel none of the lights reach ends up at the full darkness.
	float const keepPerLight = powf(1.0f - kToyFullDarkness, 1.0f / (float)report.lights);
	DWORD const shade = ToyShadowsDebug ? 0xA0FF0000 : ((DWORD)((1.0f - keepPerLight) * 255.0f) << 24);

	// once in a while, ask the card how many pixels the stencil let through
	IDirect3DQuery9 *countQuery = 0;
	if (logThisFrame && FAILED(device->CreateQuery(D3DQUERYTYPE_OCCLUSION, &countQuery)))
	{
		countQuery = 0;
	}

	ToyMeasurements measured = { 0, 0, 0, 0, 0, 0 };
	unsigned int pixelsByLight[kToyMostLights] = { 0 };
	bool const counting = countQuery != 0;
	ToyVec sampleNear = { 0.0f, 0.0f, 0.0f };
	ToyVec sampleFar = { 0.0f, 0.0f, 0.0f };

	for (int light = 0; light < report.lights; light++)
	{
		toyNumVerts = 0;
		for (int caster = 0; caster < toyNumCasters; caster++)
		{
			ToyAddPrism(&toyCasterPoints[toyCasterStart[caster]], toyCasterCount[caster], &lights[light * 4]);
		}
		if (ToyShadowsTest)
		{
			ToyAddTestBox(&lights[light * 4]);
		}
		report.verts += toyNumVerts;

		if (countQuery && light == 0)
		{
			// the first thing each prism writes is a near corner, then the far corner that goes with it
			if (toyNumVerts >= 6)
			{
				sampleNear = toyVerts[0];
				sampleFar = toyVerts[3];
			}

			ToyMeasure(device, countQuery, &measured);
		}

		HRESULT const cleared = device->Clear(0, NULL, D3DCLEAR_STENCIL, 0, 1.0f, 0);
		if (FAILED(cleared))
		{
			report.clear = cleared;
		}

		ToySetVolumeState(device);

		device->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);
		device->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_INCR);
		HRESULT const back = ToyDrawVolumes(0);
		if (FAILED(back))
		{
			report.back = back;
		}

		device->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
		device->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_DECR);
		HRESULT const front = ToyDrawVolumes(0);
		if (FAILED(front))
		{
			report.front = front;
		}

		if (ToyShadowsDebug && light == 0)
		{
			device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
			device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
			device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
			device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
			device->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
			ToyDrawVolumes(0xFFFF0000);
			device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
			device->SetRenderState(D3DRS_STENCILENABLE, TRUE);
		}

		HRESULT const darkened = ToyDarkenStencilledPixels(device, shade, countQuery, &pixelsByLight[light]);
		if (FAILED(darkened))
		{
			report.quad = darkened;
		}
		report.pixels += pixelsByLight[light];
	}

	if (countQuery)
	{
		countQuery->Release();
	}

	savedState->Apply();
	savedState->Release();

	if (logThisFrame)
	{
		// the time the processor spent here. On this one frame it also waited for the card to answer the count, so it
		// shows more than an ordinary frame costs
		QueryPerformanceCounter(&finished);
		QueryPerformanceFrequency(&ticksPerSecond);
		report.milliseconds = (float)((double)(finished.QuadPart - started.QuadPart) * 1000.0 / (double)ticksPerSecond.QuadPart);
		ToyLogFrame(report);

		char line[240];
		if (counting)
		{
			float const distance = sqrtf(lights[0] * lights[0] + lights[1] * lights[1] + lights[2] * lights[2]);

			_snprintf(line, sizeof(line) - 1,
				"toy-shadows: first light, distance %.0f, range %.0f: behind the scene back %u front %u, in front of it back %u front %u, stencil control %u, z-pass %u, unanswered %d",
				distance, lights[3], measured.backBehind, measured.frontBehind, measured.backInFront, measured.frontInFront,
				measured.stencilControl, measured.zPass, toyQueriesUnanswered);
			line[sizeof(line) - 1] = 0;
			LogString(line);

			_snprintf(line, sizeof(line) - 1,
				"toy-shadows: first light at %.0f %.0f %.0f, a prism from %.0f %.0f %.0f to %.0f %.0f %.0f, corners thrown %.0f on average",
				lights[0], lights[1], lights[2], sampleNear.x, sampleNear.y, sampleNear.z, sampleFar.x, sampleFar.y, sampleFar.z,
				toyStretchCorners ? toyStretchTotal / toyStretchCorners : 0.0);
			line[sizeof(line) - 1] = 0;
			LogString(line);

			_snprintf(line, sizeof(line) - 1, "toy-shadows: pixels in shadow by light: %u %u %u %u %u %u %u %u",
				pixelsByLight[0], pixelsByLight[1], pixelsByLight[2], pixelsByLight[3],
				pixelsByLight[4], pixelsByLight[5], pixelsByLight[6], pixelsByLight[7]);
			line[sizeof(line) - 1] = 0;
			LogString(line);
		}
		else
		{
			LogString("toy-shadows: the card gave no occlusion query, so every pixel count on this frame means nothing");
		}
	}

	ToyForgetCasters();
}
