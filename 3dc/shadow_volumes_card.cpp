// The graphics card's side of the kept shadow prisms. Written 2026-11-04 by Melanikus for the AvP port.
//
// shadow_volumes.cpp says what's kept and where. This holds the corners themselves, in one long list on the card. Corners are only ever added at
// the end of what's there, and the card may still be drawing from the part before it, so adding promises not to touch that part. When the store
// starts again from the beginning, the card is told the whole list can go, so it never has to wait for a draw to finish.
//
// The corners are in the world's numbers, so one matrix takes them to the screen, however many there are: see volume.vsh.

#include "shadow_volumes_card.h"
#include "shadow_volumes.h"
#include "d3_func.h"
#include "ShaderManager.h"

#include <string.h>

static const int kBytesEachCorner = 3 * sizeof(float);

// The ordinary route never sends more than this in one draw, because some cards have a limit
static const int kMostCornersInOneDraw = 60000 * 3;

static IDirect3DVertexBuffer9 *cornerList = 0;
static D3DXMATRIX worldToClip;

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

bool ShadowVolumesCard_Write(int first, const float *corners, int count)
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

	memcpy(memory, corners, count * kBytesEachCorner);
	cornerList->Unlock();

	return true;
}

HRESULT ShadowVolumesCard_Draw(LPDIRECT3DDEVICE9 device, D3DCOLOR colour)
{
	HRESULT result = S_OK;

	d3d.effectSystem->SetActive(d3d.volumeEffect);
	d3d.effectSystem->SetVertexShaderConstant(d3d.volumeEffect, 0, CONST_MATRIX, &worldToClip);
	d3d.volumeDecl->Set();
	device->SetPixelShader(NULL);
	device->SetStreamSource(0, cornerList, 0, kBytesEachCorner);

	// these corners have no colour of their own, so the debug views' colour comes from here
	device->SetRenderState(D3DRS_TEXTUREFACTOR, colour);
	device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TFACTOR);
	device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);

	for (int number = 0; number < ShadowVolumes_NumRuns(); number++)
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

	device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
	device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);

	// a device that's about to be reset can't still be holding this list
	device->SetStreamSource(0, NULL, 0, 0);

	return result;
}
