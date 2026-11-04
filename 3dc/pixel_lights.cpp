// Lighting each pixel from the surface pictures. Written 2026-11-04 by Melanikus for the AvP port.
//
// surface_pictures.cpp draws the frame's solid polygons once, so three pictures say how far away each pixel is, which way it faces and what
// colour its texture is. Everything here reads those. No light draws a polygon: each one is a rectangle on the screen, and the pictures say
// what's under it. So the work that belongs to the surface is done once a frame, and the work that belongs to a light is done once a light.
//
// THE LIGHT TOTAL: to keep every shadow visible, each light only takes its share of all the light on a pixel, which means knowing all the
// light on that pixel. That sum was the same for every light, so it's now worked out once and kept as a picture of its own. It holds two
// numbers a pixel. Lights baked into the walls are in the first, and are taken out by the mask. Lights added per pixel are in the second.
// A light is in one or the other, never both.
//
// WHAT THE PICTURES CAN'T DO: they hold one surface a pixel, so a polygon you can see through isn't in them, and per-pixel lights can't reach
// it. So a light left out of the game's lighting for an object is remembered, and each see-through corner of that object gets it back at
// the end of this file. That's the game's own sum. Glass keeps the light it always had, and takes no shadow, as in the original game.
//
// shaders/lightShapes.fxh holds the sums the shaders share. The numbers come from shaders written earlier for the same job, and the look of
// flares and lamps depends on them, so change them only on purpose.

#include "3dc.h"
#include "inline.h"
#include "module.h"
#include "strategy_def.h"
#include "gamedef.h"
#include "kshape.h"
#include "d3_func.h"
#include "d3d_render.h"
#include "logString.h"
#include "pixel_lights.h"
#include "surface_pictures.h"

extern VECTORCH *Global_ShapePoints;
extern VECTORCH *Global_ShapeVNormals;
extern DISPLAYBLOCK *Global_ODB_Ptr;

// Where each number is in a light on the list. See kToyLightFloats.
enum
{
	kLightRange = 3,
	kLightRed = 4,
	kLightGreen = 5,
	kLightBlue = 6,
	kLightKind = 7,
	kLightLook = 8
};

// What a light's kind number means for one added per pixel, and the shapes in lightShapes.fxh.
static const float kKindAddedPerPixel = 2.0f;
static const float kShapeGame = 1.0f;
static const float kShapeFlare = 2.0f;

// Which sampler each picture goes on, as lightShapes.fxh, lightShare.psh and lightOne.psh expect.
enum
{
	kSamplerDepth = 0,
	kSamplerNormals = 1,
	kSamplerColours = 2,
	kSamplerTotals = 3,
	kSamplerMask = 4,
	kNumSamplers = 5
};

static IDirect3DTexture9 *totalTexture = 0;
static IDirect3DSurface9 *totalSurface = 0;
static bool totalFailed = false;
static UINT totalWidth = 0;
static UINT totalHeight = 0;

void PixelLights_Release()
{
	if (totalSurface)
	{
		totalSurface->Release();
		totalSurface = 0;
	}

	if (totalTexture)
	{
		totalTexture->Release();
		totalTexture = 0;
	}

	// a reset can change what the card can do, such as the screen's size, so it's worth trying again
	totalFailed = false;
}

// The total only needs two numbers a pixel, and it can go well over 1, so it needs a float picture. A picture of four numbers does as well,
// at twice the memory. Gives back D3DFMT_UNKNOWN if the card has neither.
static D3DFORMAT BestTotalFormat(LPDIRECT3DDEVICE9 device, D3DFORMAT screenFormat)
{
	static D3DFORMAT const wanted[2] = { D3DFMT_G16R16F, D3DFMT_A16B16G16R16F };

	D3DFORMAT chosen = D3DFMT_UNKNOWN;
	IDirect3D9 *direct3D = 0;
	D3DDEVICE_CREATION_PARAMETERS created;

	if (SUCCEEDED(device->GetDirect3D(&direct3D)))
	{
		if (SUCCEEDED(device->GetCreationParameters(&created)))
		{
			for (int i = 1; i >= 0; i--)
			{
				if (direct3D->CheckDeviceFormat(created.AdapterOrdinal, created.DeviceType, screenFormat, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, wanted[i]) == D3D_OK)
				{
					chosen = wanted[i];
				}
			}
		}

		direct3D->Release();
	}

	return chosen;
}

bool PixelLights_Ready(LPDIRECT3DDEVICE9 device)
{
	if (totalSurface)
	{
		return true;
	}

	if (totalFailed || !SurfacePictures_Ready(device))
	{
		return false;
	}

	if (d3d.lightTotalEffect == kNullEffectID || d3d.lightShareEffect == kNullEffectID || d3d.lightOneEffect == kNullEffectID)
	{
		LogString("pixel lights: a shader is missing, so lights stay in the game's own lighting");
		totalFailed = true;
		return false;
	}

	IDirect3DSurface9 *screen = 0;
	D3DSURFACE_DESC size;

	if (FAILED(device->GetRenderTarget(0, &screen)))
	{
		totalFailed = true;
		return false;
	}

	HRESULT const measured = screen->GetDesc(&size);
	screen->Release();

	D3DFORMAT const format = SUCCEEDED(measured) ? BestTotalFormat(device, size.Format) : D3DFMT_UNKNOWN;

	if (format == D3DFMT_UNKNOWN
		|| FAILED(device->CreateTexture(size.Width, size.Height, 1, D3DUSAGE_RENDERTARGET, format, D3DPOOL_DEFAULT, &totalTexture, NULL))
		|| FAILED(totalTexture->GetSurfaceLevel(0, &totalSurface)))
	{
		PixelLights_Release();
		totalFailed = true;
		LogString("pixel lights: the card wouldn't make the light total, so lights stay in the game's own lighting");
		return false;
	}

	totalWidth = size.Width;
	totalHeight = size.Height;

	return true;
}

// Tells the shaders what they need to turn a pixel into a place: see PositionHere in lightShapes.fxh. The projection is read every time,
// because zooming changes it.
static void SetScreenConstant(LPDIRECT3DDEVICE9 device)
{
	float const screen[4] =
	{
		1.0f / (float)totalWidth, 1.0f / (float)totalHeight,
		1.0f / d3d.matProjection._11, 1.0f / d3d.matProjection._22,
	};

	device->SetPixelShaderConstantF(0, screen, 1);
}

// Puts a picture where a shader can read it. Every pixel is read exactly, with no blending between neighbours.
static void ReadPicture(LPDIRECT3DDEVICE9 device, int sampler, IDirect3DTexture9 *picture)
{
	device->SetTexture(sampler, picture);
	device->SetSamplerState(sampler, D3DSAMP_MINFILTER, D3DTEXF_POINT);
	device->SetSamplerState(sampler, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
	device->SetSamplerState(sampler, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
	device->SetSamplerState(sampler, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	device->SetSamplerState(sampler, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
}

// A picture can't be read and drawn on at once, and the mask is drawn on again after this, so nothing stays on a sampler.
static void StopReadingPictures(LPDIRECT3DDEVICE9 device)
{
	for (int sampler = 0; sampler < kNumSamplers; sampler++)
	{
		device->SetTexture(sampler, NULL);
	}
}

// Everything a rectangle needs. Nothing tests depth, because the pictures already say what's on each pixel. onMarksOnly limits it to the pixels
// the stencil has marked, and adding means what it writes goes on top of what's there.
static void SetRectangleState(LPDIRECT3DDEVICE9 device, bool onMarksOnly, bool adding)
{
	device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
	device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
	device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
	device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
	device->SetRenderState(D3DRS_TWOSIDEDSTENCILMODE, FALSE);
	device->SetRenderState(D3DRS_STENCILENABLE, onMarksOnly ? TRUE : FALSE);
	device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_NOTEQUAL);
	device->SetRenderState(D3DRS_STENCILREF, 0);
	device->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
	device->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
	device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
	device->SetRenderState(D3DRS_ALPHABLENDENABLE, adding ? TRUE : FALSE);
	device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
	device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
}

// The whole screen, or as much of it as the scissor lets through. shadowBlur.vsh leaves these corners where they are.
static HRESULT DrawRectangle(LPDIRECT3DDEVICE9 device)
{
	D3DLVERTEX const corners[4] =
	{
		{ -1.0f,  1.0f, 0.0f, 0, 0, 0.0f, 0.0f },
		{  1.0f,  1.0f, 0.0f, 0, 0, 1.0f, 0.0f },
		{ -1.0f, -1.0f, 0.0f, 0, 0, 0.0f, 1.0f },
		{  1.0f, -1.0f, 0.0f, 0, 0, 1.0f, 1.0f },
	};

	d3d.mainDecl->Set();

	return device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, corners, sizeof(D3DLVERTEX));
}

static float Largest(float a, float b, float c)
{
	float const larger = (a > b) ? a : b;
	return (larger > c) ? larger : c;
}

void PixelLights_DrawTotal(LPDIRECT3DDEVICE9 device, const float *lights, int numLights, IDirect3DTexture9 *depth)
{
	// see lightTotal.psh for what each of these is. Places with no light stay at 0, so they add nothing.
	enum { kFirstLight = 0, kFirstKind = kFirstLight + kToyMostLights, kConstants = kFirstKind + kToyMostLights };
	float constants[kConstants * 4] = { 0.0f };

	for (int i = 0; i < numLights && i < kToyMostLights; i++)
	{
		float const *light = &lights[i * kToyLightFloats];
		bool const perPixel = light[kLightKind] == kKindAddedPerPixel;

		constants[(kFirstLight + i) * 4 + 0] = light[0];
		constants[(kFirstLight + i) * 4 + 1] = light[1];
		constants[(kFirstLight + i) * 4 + 2] = light[2];
		constants[(kFirstLight + i) * 4 + 3] = 1.0f / light[kLightRange];

		// A light's three colours hold its colour times its brightness, so the brightest of them is its brightness. For a baked light, the
		// average of the three is what a wall's picture really holds of it.
		if (perPixel)
		{
			constants[(kFirstKind + i) * 4 + 0] = Largest(light[kLightRed], light[kLightGreen], light[kLightBlue]);
			constants[(kFirstKind + i) * 4 + 1] = (light[kLightLook] == 0.0f) ? kShapeFlare : kShapeGame;
			constants[(kFirstKind + i) * 4 + 2] = 1.0f;
		}
		else
		{
			constants[(kFirstKind + i) * 4 + 0] = (light[kLightRed] + light[kLightGreen] + light[kLightBlue]) / 3.0f;
			constants[(kFirstKind + i) * 4 + 1] = light[kLightKind];
			constants[(kFirstKind + i) * 4 + 2] = 0.0f;
		}
	}

	device->SetRenderTarget(0, totalSurface);
	device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

	d3d.effectSystem->SetActive(d3d.lightTotalEffect);
	SetScreenConstant(device);
	device->SetPixelShaderConstantF(1, constants, kConstants);

	ReadPicture(device, kSamplerDepth, depth);
	ReadPicture(device, kSamplerNormals, SurfacePictures_Normal());
	ReadPicture(device, kSamplerColours, SurfacePictures_Colour());

	SetRectangleState(device, false, false);
	DrawRectangle(device);

	StopReadingPictures(device);
}

HRESULT PixelLights_AddShareToMask(LPDIRECT3DDEVICE9 device, const float *light, float strength, IDirect3DTexture9 *depth)
{
	// see lightShare.psh for what each of these is
	float const constants[2 * 4] =
	{
		light[0], light[1], light[2], 1.0f / light[kLightRange],
		(light[kLightRed] + light[kLightGreen] + light[kLightBlue]) / 3.0f, light[kLightKind], strength, 0.0f,
	};

	d3d.effectSystem->SetActive(d3d.lightShareEffect);
	SetScreenConstant(device);
	device->SetPixelShaderConstantF(1, constants, 2);

	ReadPicture(device, kSamplerDepth, depth);
	ReadPicture(device, kSamplerNormals, SurfacePictures_Normal());
	ReadPicture(device, kSamplerColours, SurfacePictures_Colour());
	ReadPicture(device, kSamplerTotals, totalTexture);

	SetRectangleState(device, true, true);
	HRESULT const result = DrawRectangle(device);

	StopReadingPictures(device);

	return result;
}

void PixelLights_AddLight(LPDIRECT3DDEVICE9 device, const float *light, IDirect3DTexture9 *mask, IDirect3DTexture9 *depth, float softWidth, float softStaysWideTo)
{
	// A light's three colours hold its colour times its brightness, so dividing by the brightest gives its colour alone.
	float const bright = Largest(light[kLightRed], light[kLightGreen], light[kLightBlue]);
	float const perBright = (bright > 0.0f) ? 1.0f / bright : 0.0f;

	// see lightOne.psh for what each of these is
	float const constants[4 * 4] =
	{
		light[0], light[1], light[2], 1.0f / light[kLightRange],
		light[kLightRed] * perBright, light[kLightGreen] * perBright, light[kLightBlue] * perBright, 0.0f,
		softWidth, softStaysWideTo, 0.0f, 0.0f,
		bright, light[kLightLook], 0.0f, 0.0f,
	};

	d3d.effectSystem->SetActive(d3d.lightOneEffect);
	SetScreenConstant(device);
	device->SetPixelShaderConstantF(1, constants, 4);

	ReadPicture(device, kSamplerDepth, depth);
	ReadPicture(device, kSamplerNormals, SurfacePictures_Normal());
	ReadPicture(device, kSamplerColours, SurfacePictures_Colour());
	ReadPicture(device, kSamplerTotals, totalTexture);
	ReadPicture(device, kSamplerMask, mask);

	SetRectangleState(device, false, true);
	DrawRectangle(device);

	StopReadingPictures(device);
}

// The lights left out of the object being lit. Each one's LocalLP has been set for that object, the same as a light that stayed in.
static LIGHTBLOCK *lightsLeftOut[kToyMostLights];
static int numLightsLeftOut = 0;

void PixelLights_ForgetLeftOut()
{
	numLightsLeftOut = 0;
}

void PixelLights_NoteLeftOut(LIGHTBLOCK *light)
{
	if (numLightsLeftOut < kToyMostLights)
	{
		lightsLeftOut[numLightsLeftOut++] = light;
	}
}

static unsigned char AddUpTo255(unsigned char value, int more)
{
	int const total = value + more;
	return (unsigned char)((total > 255) ? 255 : total);
}

// The game's own sum for one corner (kshape.cpp, VertexIntensity_Standard_Opt), for the lights left out. This adds to the corner, and not to the
// shape's list of finished corners, because a solid polygon can share this point and mustn't get these lights. A flare comes here at its
// dim cap, because ToyHoldFlaresToTheirCap has already set the number read here.
void PixelLights_LightSeeThroughCorner(RENDERVERTEX *corner, int vertexNumber)
{
	if (!numLightsLeftOut)
	{
		return;
	}

	VECTORCH const *point = Global_ShapePoints + vertexNumber;
	VECTORCH const *normal = Global_ShapeVNormals ? Global_ShapeVNormals + vertexNumber : 0;
	bool const usesFacing = normal && !(Global_ODB_Ptr->ObFlags3 & ObFlag3_NoLightDot);
	int red = 0, green = 0, blue = 0;
	int glowRed = 0, glowGreen = 0, glowBlue = 0;

	for (int i = 0; i < numLightsLeftOut; i++)
	{
		LIGHTBLOCK const *light = lightsLeftOut[i];

		VECTORCH toLight;
		toLight.vx = light->LocalLP.vx - point->vx;
		toLight.vy = light->LocalLP.vy - point->vy;
		toLight.vz = light->LocalLP.vz - point->vz;

		int const distance = Approximate3dMagnitude(&toLight);

		if (distance >= light->LightRange)
		{
			continue;
		}

		int amount = MUL_FIXED(light->LightRange - distance, light->BrightnessOverRange);

		if (distance > 0 && usesFacing)
		{
			int const facing = MUL_FIXED(normal->vx, toLight.vx) + MUL_FIXED(normal->vy, toLight.vy) + MUL_FIXED(normal->vz, toLight.vz);

			amount = (facing > 0) ? (WideMulNarrowDiv(amount, facing, distance) + amount / 4) / 2 : amount / 8;
		}

		int const r = MUL_FIXED(amount, light->RedScale);
		int const g = MUL_FIXED(amount, light->GreenScale);
		int const b = MUL_FIXED(amount, light->BlueScale);

		red += r;
		green += g;
		blue += b;

		if (!(light->LightFlags & LFlag_PreLitSource) && !(light->LightFlags & LFlag_NoSpecular))
		{
			glowRed += r;
			glowGreen += g;
			glowBlue += b;
		}
	}

	corner->R = AddUpTo255(corner->R, red >> 8);
	corner->G = AddUpTo255(corner->G, green >> 8);
	corner->B = AddUpTo255(corner->B, blue >> 8);
	corner->SpecularR = AddUpTo255(corner->SpecularR, glowRed >> 10);
	corner->SpecularG = AddUpTo255(corner->SpecularG, glowGreen >> 10);
	corner->SpecularB = AddUpTo255(corner->SpecularB, glowBlue >> 10);
}
