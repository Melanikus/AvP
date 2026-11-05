// One drawing of the surfaces each frame, for every light to read. Written 2026-11-04 by Melanikus for the AvP port.
//
// To light a pixel you need to know three things about the surface it shows: how far away it is, which way it faces, and what colour its
// texture is. The slow way is for every light to draw every polygon again to find out. Instead, the frame's solid polygons are drawn once,
// into three pictures the size of the screen, and every light reads those. So each thing is worked out once a frame, however many lights
// there are, and a light only does the work that's really its own.
//
// WHAT ISN'T IN THE PICTURES: they hold one surface for each pixel, so a surface you can see through can't be in them. Glass, particles,
// decals and the player's weapon are all left out, and a pixel with nothing on it, like the sky, stays empty. Reflections are left out too.
// shadows-plan.md, "What can't go in a surface picture", says what happens to each. The colour picture's alpha is 1 where a surface is.
//
// WHEN IT CAN'T RUN: this needs a card with shader model 3 that can draw three pictures at once, and no anti-aliasing, because the pictures
// have to match the depth buffer pixel for pixel. Without those, or if a picture can't be made, it says so once and stays off.

#include "surface_pictures.h"
#include "surface_normals.h"
#include "surface_static.h"
#include "static_scene.h"
#include "texture_relief.h"
#include "view_switches.h"
#include "d3_func.h"
#include "RenderList.h"
#include "logString.h"

extern RenderList *mainList;

static IDirect3DTexture9 *normalTexture = 0;
static IDirect3DTexture9 *colourTexture = 0;
static IDirect3DSurface9 *normalSurface = 0;
static IDirect3DSurface9 *colourSurface = 0;
static bool picturesFailed = false;

template <typename Thing>
static void GiveBack(Thing *&thing)
{
	if (thing)
	{
		thing->Release();
		thing = 0;
	}
}

void SurfacePictures_Release()
{
	GiveBack(normalSurface);
	GiveBack(colourSurface);
	GiveBack(normalTexture);
	GiveBack(colourTexture);

	// a reset can change what the card can do, such as the screen's size, so it's worth trying again
	picturesFailed = false;
}

// A normal needs more than 8 bits a number, or a smooth curve shows steps in its light. All three pictures must use the same number of
// bits a pixel, or the card can't draw them together, and this format has the same 32 as the other two.
static D3DFORMAT BestNormalFormat(LPDIRECT3DDEVICE9 device)
{
	D3DFORMAT chosen = D3DFMT_A8R8G8B8;
	IDirect3D9 *direct3D = 0;
	D3DDEVICE_CREATION_PARAMETERS created;
	D3DDISPLAYMODE mode;

	if (SUCCEEDED(device->GetDirect3D(&direct3D)))
	{
		if (SUCCEEDED(device->GetCreationParameters(&created)) && SUCCEEDED(device->GetDisplayMode(0, &mode))
			&& direct3D->CheckDeviceFormat(created.AdapterOrdinal, created.DeviceType, mode.Format, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, D3DFMT_A2R10G10B10) == D3D_OK)
		{
			chosen = D3DFMT_A2R10G10B10;
		}

		direct3D->Release();
	}

	return chosen;
}

bool SurfacePictures_CardCanDrawThem(LPDIRECT3DDEVICE9 device)
{
	D3DCAPS9 caps;

	return SUCCEEDED(device->GetDeviceCaps(&caps))
		&& caps.PixelShaderVersion >= D3DPS_VERSION(3, 0) && caps.VertexShaderVersion >= D3DVS_VERSION(3, 0) && caps.NumSimultaneousRTs >= 3;
}

// Can this device do it at all? If not, says why in the log.
static bool CardCanDoThis(LPDIRECT3DDEVICE9 device, const D3DSURFACE_DESC &screen)
{
	if (!SurfacePictures_CardCanDrawThem(device))
	{
		LogString("surface pictures: this card can't draw three pictures at once with shader model 3, so they stay off");
		return false;
	}

	if (screen.MultiSampleType != D3DMULTISAMPLE_NONE)
	{
		LogString("surface pictures: anti-aliasing is on, so they wouldn't match the depth buffer, and they stay off");
		return false;
	}

	if (d3d.surfaceEffect == kNullEffectID || !d3d.surfaceDecl || !SurfaceNormals_Ready())
	{
		LogString("surface pictures: the shaders or the list of normals aren't there, so they stay off");
		return false;
	}

	return true;
}

bool SurfacePictures_Ready(LPDIRECT3DDEVICE9 device)
{
	if (normalSurface && colourSurface)
	{
		return true;
	}

	if (picturesFailed)
	{
		return false;
	}

	IDirect3DSurface9 *screen = 0;
	D3DSURFACE_DESC size;

	if (FAILED(device->GetRenderTarget(0, &screen)))
	{
		picturesFailed = true;
		return false;
	}

	HRESULT const measured = screen->GetDesc(&size);
	screen->Release();

	if (FAILED(measured) || !CardCanDoThis(device, size))
	{
		picturesFailed = true;
		return false;
	}

	if (FAILED(device->CreateTexture(size.Width, size.Height, 1, D3DUSAGE_RENDERTARGET, BestNormalFormat(device), D3DPOOL_DEFAULT, &normalTexture, NULL))
		|| FAILED(device->CreateTexture(size.Width, size.Height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &colourTexture, NULL))
		|| FAILED(normalTexture->GetSurfaceLevel(0, &normalSurface))
		|| FAILED(colourTexture->GetSurfaceLevel(0, &colourSurface)))
	{
		SurfacePictures_Release();
		picturesFailed = true;
		LogString("surface pictures: the card wouldn't make them, so they stay off until the device resets");
		return false;
	}

	return true;
}

bool SurfacePictures_ReliefCouldRun()
{
	return textureRelief.IsOn() && d3d.surfaceReliefEffect != kNullEffectID && SurfaceNormals_TangentsReady();
}

bool SurfacePictures_ReliefCanRun()
{
	return textureRelief.IsReady() && d3d.surfaceReliefEffect != kNullEffectID && SurfaceNormals_TangentsReady();
}

void SurfacePictures_SetReliefConstants(LPDIRECT3DDEVICE9 device, const float *worldAxes, bool march)
{
	float const constants[4 * 4] =
	{
		worldAxes[0], worldAxes[1], worldAxes[2], 0.0f,
		worldAxes[3], worldAxes[4], worldAxes[5], 0.0f,
		worldAxes[6], worldAxes[7], worldAxes[8], 0.0f,
		march ? ReliefDepthNow : 0.0f, ReliefHeightFlipped ? -1.0f : 1.0f, ReliefTiltFlipped ? -1.0f : 1.0f, 0.0f,
	};

	device->SetPixelShaderConstantF(0, constants, 4);
}

// useStatic asks for the level's walls to be drawn from the fixed buffer, and only what isn't in it to be drawn the old way. It can be asked for on any frame,
// and if that route can't draw, the old way draws everything: see surface_static.cpp.
bool SurfacePictures_Draw(LPDIRECT3DDEVICE9 device, IDirect3DSurface9 *depthPicture, const float *worldAxes, bool reliefWanted, bool useStatic, bool marchWanted)
{
	if (!SurfacePictures_Ready(device))
	{
		return false;
	}

	// Clearing writes to every picture being drawn on, and the depth picture wants a different value, so these two are cleared alone.
	// All zeros means "no surface here".
	device->SetRenderTarget(0, normalSurface);
	device->Clear(0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0);
	device->SetRenderTarget(0, colourSurface);
	device->Clear(0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0);

	device->SetRenderTarget(0, depthPicture);
	device->SetRenderTarget(1, normalSurface);
	device->SetRenderTarget(2, colourSurface);

	// THE CHOICE: with relief pictures, or exactly as it was without them. Without them this draws with surface.vsh and surface.psh, from the same lists it
	// always did, so turning relief off leaves nothing of it behind.
	bool const withRelief = reliefWanted && SurfacePictures_ReliefCanRun();

	// A hand-made map has smaller copies, and this blends between them, which is what stops a detailed map shimmering in the distance. Every other relief picture has just the one.
	// The shadow pass puts the card's settings back when it ends.
	bool const march = withRelief && marchWanted;

	if (withRelief)
	{
		device->SetSamplerState(1, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
	}

	// The walk reads its depth map many times, so it's smoothed however the picture itself is read, or the walk would land on steps.
	if (march)
	{
		device->SetSamplerState(2, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
		device->SetSamplerState(2, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
		device->SetSamplerState(2, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
	}

	effectID_t const effect = withRelief ? d3d.surfaceReliefEffect : d3d.surfaceEffect;

	// The fixed buffer's own drawing doesn't set up the depth maps, so the walk goes the old way for now.
	bool fromBuffer = useStatic && !march && SurfaceStatic_Ready();

	// The world's three directions, for the shader to turn each normal to face the way the camera does, and what the walk needs. This is the only place that happens.
	// They go straight into the shader's registers. A vertex shader's numbers go through a list whose order the compiler picks, so a second
	// one there could land on top of WorldViewProj.
	SurfacePictures_SetReliefConstants(device, worldAxes, march);

	// If this fails part way, whatever it did draw is drawn again below, on the same pixels, so nothing goes missing.
	if (fromBuffer)
	{
		fromBuffer = SurfaceStatic_Draw(device, withRelief);
	}

	d3d.effectSystem->SetActive(effect);

	// These go straight into the shader's registers too, for the same reason. A matrix is read down its columns there.
	D3DXMATRIX projectionDown;
	D3DXMatrixTranspose(&projectionDown, &d3d.matProjection);
	device->SetVertexShaderConstantF(0, (const float *)&projectionDown, 4);

	float const route[4] = { fromBuffer ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f };
	device->SetVertexShaderConstantF(4, route, 1);

	SurfaceNormals_UseList();

	if (withRelief)
	{
		SurfaceNormals_UseTangents();
		mainList->DrawOpaqueAgain(d3d.surfaceReliefDecl, true, march);
		SurfaceNormals_StopUsingTangents();
	}
	else
	{
		mainList->DrawOpaqueAgain(d3d.surfaceDecl);
	}

	SurfaceNormals_StopUsingList();

	device->SetRenderTarget(1, NULL);
	device->SetRenderTarget(2, NULL);

	return true;
}

bool SurfacePictures_DrawWalls(LPDIRECT3DDEVICE9 device, IDirect3DSurface9 *depthPicture, bool fromBuffer)
{
	if (!SurfacePictures_Ready(device))
	{
		return false;
	}

	device->SetRenderTarget(0, normalSurface);
	device->Clear(0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0);
	device->SetRenderTarget(0, colourSurface);
	device->Clear(0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0);
	device->SetRenderTarget(0, depthPicture);
	device->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xFFFFFFFF, 1.0f, 0);

	device->SetRenderTarget(1, normalSurface);
	device->SetRenderTarget(2, colourSurface);

	bool worked = true;

	if (fromBuffer)
	{
		int count = 0;
		StaticRange const *ranges = SurfaceStatic_FrameRanges(&count);

		worked = StaticScene_DrawForComparing(device, ranges, count);
	}
	else
	{
		d3d.effectSystem->SetActive(d3d.surfaceSceneReplayEffect);

		D3DXMATRIX projectionDown;
		D3DXMatrixTranspose(&projectionDown, &d3d.matProjection);
		device->SetVertexShaderConstantF(0, (const float *)&projectionDown, 4);

		SurfaceNormals_UseList();
		mainList->DrawOpaqueAgain(d3d.surfaceDecl);
		SurfaceNormals_StopUsingList();
	}

	device->SetRenderTarget(1, NULL);
	device->SetRenderTarget(2, NULL);

	return worked;
}

void SurfacePictures_ShowNormals(LPDIRECT3DDEVICE9 device)
{
	D3DSURFACE_DESC size;

	if (!normalSurface || FAILED(normalSurface->GetDesc(&size)))
	{
		return;
	}

	struct ScreenCorner
	{
		float x, y, z, rhw;
		float u, v;
	};

	// Half a pixel back, so that each pixel of the screen reads the middle of the same pixel of the picture.
	float const left = -0.5f, top = -0.5f;
	float const right = (float)size.Width - 0.5f, bottom = (float)size.Height - 0.5f;

	ScreenCorner const corners[4] =
	{
		{ left,  top,    0.0f, 1.0f, 0.0f, 0.0f },
		{ right, top,    0.0f, 1.0f, 1.0f, 0.0f },
		{ left,  bottom, 0.0f, 1.0f, 0.0f, 1.0f },
		{ right, bottom, 0.0f, 1.0f, 1.0f, 1.0f },
	};

	device->SetVertexShader(NULL);
	device->SetPixelShader(NULL);
	device->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);

	device->SetTexture(0, normalTexture);
	device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
	device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
	device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);

	device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
	device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
	device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
	device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
	device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);

	device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, corners, sizeof(ScreenCorner));

	device->SetTexture(0, NULL);
}

IDirect3DTexture9 *SurfacePictures_Normal()
{
	return normalTexture;
}

IDirect3DTexture9 *SurfacePictures_Colour()
{
	return colourTexture;
}
