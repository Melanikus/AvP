// The graphics card's side of the kept shadow prisms. Written 2026-11-04 by Melanikus for the AvP port.
//
// shadow_volumes.cpp says what's kept and where. This holds the corners themselves, in one long list on the card. Corners are only ever added at
// the end of what's there, and the card may still be drawing from the part before it, so adding promises not to touch that part. When the store
// starts again from the beginning, the card is told the whole list can go, so it never has to wait for a draw to finish.
//
// The corners are in the world's numbers, so one matrix takes them to the screen, however many there are: see volume.vsh.
//
// WHY A CORNER CAN BE CUT
//
// The game cuts every corner of the walls it draws to a whole number, in the view's numbers, and how it falls changes as the camera moves. A prism made each
// frame starts from those same cut corners, so it stays in step with its wall. A prism kept in the world's numbers doesn't: its wall moves about it by up to
// a unit along each direction. The thin gap between a wall and its own prism then changes width from frame to frame, and close to a wall a unit is several
// pixels, so the shadow flickers there. With -keptsnap, the world's numbers stay on the card, and volumeCut.vsh makes the game's cut and then ToyAddPrism's
// sum each frame, so a kept prism lands exactly where a prism made this frame would. Only the view is new each frame: nothing else is worked out again.
//
// The sides of a prism made that way all pass through the light, so two neighbours' sides lie in one flat surface and nothing can show between them. The price
// is that a far end moves with the cut too, so a shadow's far edge swims a little, as it does on prisms made each frame. -keptsnapfar leaves far ends where the
// world says, which stops that, but then a side no longer passes through the light, and neighbours with different reaches can leave a hair between them.

#include "shadow_volumes_card.h"
#include "shadow_volumes.h"
#include "d3_func.h"
#include "ShaderManager.h"
#include "surface_static.h"

#include <string.h>

static const int kBytesEachCorner = sizeof(KeptCorner);

int KeptSnap = 0;   // -keptsnap makes this 1, and -keptsnapfar makes it 2. 0 leaves a kept prism's corners exact

// The ordinary route never sends more than this in one draw, because some cards have a limit
static const int kMostCornersInOneDraw = 60000 * 3;

static IDirect3DVertexBuffer9 *cornerList = 0;
static D3DXMATRIX worldToClip;
static float lightForCutCorners[4];   // what volumeCut.vsh reads: see LightOnList

void ShadowVolumesCard_Create()
{
	ShadowVolumesCard_Release();

	HRESULT const made = d3d.lpD3DDevice->CreateVertexBuffer(kShadowVolumeMostCorners * kBytesEachCorner, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &cornerList, NULL);

	if (FAILED(made))
	{
		cornerList = 0;
	}
}

void ShadowVolumesCard_Release()
{
	if (cornerList)
	{
		cornerList->Release();
		cornerList = 0;
	}

	ShadowVolumes_Empty();
}

bool ShadowVolumesCard_Ready()
{
	return cornerList != 0 && d3d.volumeEffect != kNullEffectID && d3d.volumeDecl != 0;
}

void ShadowVolumesCard_SetView(const float *view)
{
	// The game's view, with y turned over, which gives the numbers its polygons are drawn in. Then the same projection they go through.
	D3DXMATRIX const worldToList(
		view[0], -view[4], view[8],  0.0f,
		view[1], -view[5], view[9],  0.0f,
		view[2], -view[6], view[10], 0.0f,
		view[3], -view[7], view[11], 1.0f);

	D3DXMatrixMultiply(&worldToClip, &worldToList, &d3d.matProjection);
}

void ShadowVolumesCard_SetLight(const float *light)
{
	lightForCutCorners[0] = light[0];
	lightForCutCorners[1] = light[1];
	lightForCutCorners[2] = light[2];
	lightForCutCorners[3] = (KeptSnap == 2) ? 1.0f : 0.0f;
}

bool ShadowVolumesCard_CornersAreCut()
{
	return KeptSnap != 0 && d3d.volumeCutEffect != kNullEffectID && d3d.volumeCutDecl != 0;
}

const char *ShadowVolumesCard_CornerWords()
{
	if (!ShadowVolumesCard_CornersAreCut())
	{
		return (KeptSnap != 0) ? "corners exact, because the shader for -keptsnap isn't there" : "corners exact";
	}

	return (KeptSnap == 2) ? "corners cut, far ends exact" : "corners cut";
}

const char *ShadowVolumesCard_TagWords()
{
	if (KeptSnap == 0)
	{
		return "";
	}

	if (!ShadowVolumesCard_CornersAreCut())
	{
		return " SNAP MISSING";
	}

	return (KeptSnap == 2) ? " SNAP FAR" : " SNAP";
}

bool ShadowVolumesCard_Write(int first, const float *corners, const float *ends, int count)
{
	if (!cornerList || first < 0 || count <= 0 || count > kShadowVolumeMostCorners - first)
	{
		return false;
	}

	void *memory = 0;
	DWORD const promise = (first == 0) ? D3DLOCK_DISCARD : D3DLOCK_NOOVERWRITE;

	if (FAILED(cornerList->Lock(first * kBytesEachCorner, count * kBytesEachCorner, &memory, promise)) || !memory)
	{
		return false;
	}

	KeptCorner *out = (KeptCorner *)memory;

	for (int i = 0; i < count; i++)
	{
		out[i].x = corners[i * 3 + 0];
		out[i].y = corners[i * 3 + 1];
		out[i].z = corners[i * 3 + 2];
		out[i].end = ends[i];
	}

	cornerList->Unlock();

	return true;
}

HRESULT ShadowVolumesCard_Draw(LPDIRECT3DDEVICE9 device, D3DCOLOR colour, const KeptCorner *worldCorners, int numWorldCorners)
{
	HRESULT result = S_OK;

	if (d3d.volumeEffect == kNullEffectID || !d3d.volumeDecl)
	{
		return E_FAIL;
	}

	if (ShadowVolumesCard_CornersAreCut())
	{
		d3d.effectSystem->SetActive(d3d.volumeCutEffect);
		SurfaceStatic_SetShaderNumbers(device);
		device->SetVertexShaderConstantF(7, lightForCutCorners, 1);
		d3d.volumeCutDecl->Set();
	}
	else
	{
		// this shader only asks for x y z, so the fourth number of a corner goes unread
		d3d.effectSystem->SetActive(d3d.volumeEffect);
		d3d.effectSystem->SetVertexShaderConstant(d3d.volumeEffect, 0, CONST_MATRIX, &worldToClip);
		d3d.volumeDecl->Set();
	}

	device->SetPixelShader(NULL);
	device->SetStreamSource(0, cornerList, 0, kBytesEachCorner);

	// these corners have no colour of their own, so the debug views' colour comes from here
	device->SetRenderState(D3DRS_TEXTUREFACTOR, colour);
	device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TFACTOR);
	device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);

	// With kept prisms off there's no list on the card, and all there is to draw is the corners handed in.
	int const numRuns = cornerList ? ShadowVolumes_NumRuns() : 0;

	for (int number = 0; number < numRuns; number++)
	{
		ShadowVolumeRun const &run = ShadowVolumes_Run(number);

		for (int done = 0; done < run.corners; done += kMostCornersInOneDraw)
		{
			int const corners = (run.corners - done < kMostCornersInOneDraw) ? run.corners - done : kMostCornersInOneDraw;
			HRESULT const drawn = device->DrawPrimitive(D3DPT_TRIANGLELIST, run.first + done, corners / 3);

			if (FAILED(drawn))
			{
				result = drawn;
			}
		}
	}

	// corners from a moving light, or from a store that's full or off. These are this frame's only, so they aren't worth a list on the card.
	for (int done = 0; done < numWorldCorners; done += kMostCornersInOneDraw)
	{
		int const corners = (numWorldCorners - done < kMostCornersInOneDraw) ? numWorldCorners - done : kMostCornersInOneDraw;
		HRESULT const drawn = device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, corners / 3, worldCorners + done, kBytesEachCorner);

		if (FAILED(drawn))
		{
			result = drawn;
		}
	}

	device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
	device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);

	// a device that's about to be reset can't still be holding this list
	device->SetStreamSource(0, NULL, 0, 0);

	return result;
}
