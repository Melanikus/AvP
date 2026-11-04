// The surface pictures, drawn from the level's fixed buffer. Written 2026-11-04 by Melanikus for the AvP port.
//
// The surface pictures hold what's at each pixel: how far away, which way it faces, what colour. surface_pictures.cpp draws them by going over the frame's
// polygons a second time, and those polygons are worked out afresh by the processor every frame. A level's walls never change, so static_level.cpp keeps
// them on the card from the moment the level loads. This draws them from there. It sends no polygons at all: it only says which runs of the buffer to draw.
//
// WHAT'S STILL DRAWN THE OLD WAY: everything not in the buffer, such as objects, models and doors. SurfaceNormals_WritePolygon marks each polygon of a
// module as being in the buffer or not, so that the old drawing can leave out just the ones drawn here. That's decided while the pictures are drawn, so
// either route works on any frame, and if this one can't draw, even part way through, the old one still has everything.
//
// IT HAS TO MATCH THE GAME: the pictures are read by lights working on what the game drew, so a pixel here must be the same surface as the pixel on screen.
// surfaceStatic.vsh explains how the corners are made to land in the same places.
//
// THE BACKS OF WALLS: the game drops a polygon whose back is towards the camera (PolygonWithinFrustum). Here the card has to, because a wall seen from behind
// can be in front of what's really on screen, and it would end up in the pictures. A polygon's corners run anticlockwise on screen seen from the front, so
// the card drops the clockwise ones. The runs that can be seen from both sides are kept apart so that they can be drawn with nothing dropped.
//
// HOW IT'S READ: like every polygon in the level, with bilinear filtering and repeating at the edges of the texture. Those are the defaults of
// RenderList::AddItem, and nothing that draws a module asks for anything else.

#include <d3dx9.h>

#include "surface_static.h"
#include "texture_relief.h"
#include "d3_func.h"
#include "VertexDeclaration.h"
#include "logString.h"

void ToyShadows_ViewMatrix(float *matrix);

static const StaticRange *frameRanges = 0;
static int frameRangeCount = 0;

void SurfaceStatic_SetFrame(const StaticRange *ranges, int count)
{
	frameRanges = ranges;
	frameRangeCount = (ranges && count > 0) ? count : 0;
}

bool SurfaceStatic_Ready()
{
	return d3d.surfaceStaticDecl && d3d.surfaceStaticEffect != kNullEffectID && StaticLevel_Ready();
}

// The numbers surfaceStatic.vsh reads. They go straight into its registers: its first three hold the game's view, a line for each of x, y and z, and the
// next four hold the projection, read down its columns because that's how the shader multiplies.
static void SetShaderNumbers(LPDIRECT3DDEVICE9 device)
{
	float view[12];
	ToyShadows_ViewMatrix(view);
	device->SetVertexShaderConstantF(0, view, 3);

	D3DXMATRIX down;
	D3DXMatrixTranspose(&down, &d3d.matProjection);
	device->SetVertexShaderConstantF(3, (const float *)&down, 4);
}

static void SetHowTexturesAreRead(LPDIRECT3DDEVICE9 device, DWORD sampler)
{
	device->SetSamplerState(sampler, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
	device->SetSamplerState(sampler, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
	device->SetSamplerState(sampler, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	device->SetSamplerState(sampler, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
}

bool SurfaceStatic_Draw(LPDIRECT3DDEVICE9 device, bool withRelief)
{
	if (!SurfaceStatic_Ready())
	{
		return false;
	}

	if (!frameRangeCount)
	{
		return true;
	}

	bool const relief = withRelief && d3d.surfaceStaticReliefEffect != kNullEffectID;

	d3d.effectSystem->SetActive(relief ? d3d.surfaceStaticReliefEffect : d3d.surfaceStaticEffect);
	SetShaderNumbers(device);

	bool worked = d3d.surfaceStaticDecl->Set();
	worked = SUCCEEDED(device->SetStreamSource(0, StaticLevel_VertexBuffer(), 0, sizeof(StaticVertex))) && worked;
	worked = SUCCEEDED(device->SetIndices(StaticLevel_IndexBuffer())) && worked;

	SetHowTexturesAreRead(device, 0);

	if (relief)
	{
		SetHowTexturesAreRead(device, 1);
	}

	device->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);

	bool twoSided = false;
	bool textureIsSet = false;
	uint32_t texture = 0;

	for (int i = 0; worked && i < frameRangeCount; i++)
	{
		StaticRange const &range = frameRanges[i];

		if (!textureIsSet || range.texture != texture)
		{
			device->SetTexture(0, Tex_GetTexture(range.texture));

			if (relief)
			{
				device->SetTexture(1, Tex_GetTexture(textureRelief.PictureFor(range.texture)));
			}

			texture = range.texture;
			textureIsSet = true;
		}

		if (range.twoSided != twoSided)
		{
			device->SetRenderState(D3DRS_CULLMODE, range.twoSided ? D3DCULL_NONE : D3DCULL_CW);
			twoSided = range.twoSided;
		}

		worked = SUCCEEDED(device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, range.baseVertex, range.firstVertex, range.vertexCount, range.firstIndex, range.triangles));
	}

	// the old drawing that follows counts on nothing being dropped: the game has already dropped the backs of its polygons
	device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);

	if (relief)
	{
		device->SetTexture(1, NULL);
	}

	// once is enough: this frame and any like it just go the old way
	static bool told = false;

	if (!worked && !told)
	{
		LogString("surface pictures: the card refused the level's fixed buffer, so those frames draw it the old way");
		told = true;
	}

	return worked;
}
