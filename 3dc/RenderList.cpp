// Copyright (C) 2010 Barry Duncan. All Rights Reserved.
// The original author of this code can be contacted at: bduncan22@hotmail.com

// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
// INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
// FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// DEVELOPERS AND CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Modified 2026-11-04 by Melanikus for the AvP port:
//   a list no longer takes a shape its buffers have no room for, or a shape while its buffers are closed. It leaves
//   the shape out and says so once in the log. Adding a command can no longer run off the end of the list. The log
//   also says when a list passes each quarter of its room. GetNumIndices moved here from d3d_render.cpp, because
//   this file is the one that writes the index numbers it counts. A shape with any number of corners gets its index
//   numbers, a list's buffers can be missing without harm, and index numbers can't go past the end of their buffer

#include "RenderList.h"
#include "TextureManager.h"
#include "texture_relief.h"
#include "renderer.h"
#include <algorithm>
#include <string>
#include <assert.h>
#include "console.h"
#include <assert.h>
#include "logString.h"

// AvP uses numVerts to represent the actual number of polygon vertices we're passing to the backend.
// we then use the OUTPUT_TRIANGLE function to generate the indices required to represent those
// polygons. This function calculates the number of indices required based on the number of vertices.
// AvP code defined these, so hence the magic numbers.

uint32_t GetNumIndices(uint32_t numVerts)
{
	// the water and molten metal meshes: 15 x 15 squares of two triangles each. They write their own triangles, with AddTriangle
	if (numVerts == 256)
	{
		return 1350;
	}

	// anything under three corners is no shape at all
	if (numVerts < 3)
	{
		return 0;
	}

	// a shape with this many corners is cut into two fewer triangles, of three numbers each
	return (numVerts - 2) * 3;
}

// bit getters
#define get_unused(src)			((src & 4278190080) >> 24)
#define get_commandType(src)	((src & 16711680) >> 16)
#define get_sequenceNumber(src)	((src & 65472) >> 6)
#define get_command(src)		((src & 32) >> 5)
#define get_transType1(src)		((src & 24) >> 3)
#define get_layerID(src)		(src & 7)

#define get_texID(src)			((src & 4294443008) >> 19)
#define get_transType(src)		((src & 522240) >> 11)
#define get_filterType(src)		((src & 1792) >> 8)
#define get_texAddressType(src)	((src & 192) >> 6)

// bit setters
#define set_unused(out, in)			out |= (in << 24)
#define set_commandType(out, in)	out |= (in << 16)
#define set_sequenceNumber(out, in)	out |= (in << 6)

#define set_command(out, in)		out |= (in << 5)
#define set_transType1(out, in)		out |= (in << 3)
#define set_layerID(out, in)		out |= in

#define set_texID(out, in)			out |= (in << 19)
#define set_transType(out, in)		out |= (in << 11)
#define set_filterType(out, in)		out |= (in << 8)
#define set_texAddressType(out, in)	out |= (in << 6)


RenderList::RenderList(const std::string &listName, size_t size, VertexBuffer **vb, IndexBuffer **ib, VertexDeclaration **decl)
{
	this->listName = listName;
	listIndex   = 0;

	layer = kLayerWorld;

	useIndicesOffset = false;

	leftOutBecauseFull = 0;
	leftOutBecauseClosed = 0;
	quartersReported = 0;

	// treat the vector as an array so resize it to desired size
	Items.reserve(size);
	Items.resize(size);

	this->vb = vb;
	this->ib = ib;
	this->decl = decl;
}

void RenderList::EnableIndicesOffset()
{
	useIndicesOffset = true;
}

void RenderList::IncrementIndexCount(uint32_t nI)
{
	if (!BuffersExist())
	{
		return;
	}

	uint32_t iCount = (**ib).GetSize();
	(**ib).SetSize(iCount + nI);
}

RenderList::~RenderList()
{
	// nothing to do here
}

void RenderList::AddTriangle(uint16_t *indexArray, uint32_t a, uint32_t b, uint32_t c, uint32_t n)
{
	AddIndices(indexArray, a,b,c, n);
}

// we use this function to create the indices needed to render our screen space quads. 
// We generate the indices differently here than the engine itself does for quads 
// (which it uses for general level geometry.
void RenderList::CreateOrthoIndices(uint16_t *indexArray)
{
	AddIndices(indexArray, 0,1,2, 4);
	AddIndices(indexArray, 2,1,3, 4);
}

// generates triangle indices for the verts we've just added to a vertex buffer
// this function was originally in d3d_render.cpp
void RenderList::CreateIndices(uint16_t *indexArray, uint32_t numVerts)
{
	switch (numVerts)
	{
		case 0:
		case 1:
		case 2:
			Con_PrintError("Asked to render too few verts in CreateIndices");
			break;
		case 3:
		{
			AddIndices(indexArray, 0,2,1, 3);
			break;
		}
		case 4:
		{
			// winding order for main avp rendering system
			AddIndices(indexArray, 0,1,2, 4);
			AddIndices(indexArray, 0,2,3, 4);
			break;
		}
		case 5:
		{
			AddIndices(indexArray, 0,1,4, 5);
			AddIndices(indexArray, 1,3,4, 5);
			AddIndices(indexArray, 1,2,3, 5);
			break;
		}
		default:
		{
			// Six corners or more: a fan of triangles, all from the first corner. Stored polygons have three or four corners. More can only come from
			// the game's clipping (3dc/frustum.cpp), which cuts a polygon against five flat surfaces, and each cut can add corners. USE_FRUSTUM_CULL isn't
			// defined in this build, so the only thing clipped is the trail behind a thrown disc (RenderTrailSegment), which calls it directly. Nine is not a
			// limit on what clipping can make. It's what a flat polygon with no dent comes to when each cut adds one. With the switch off each cut assumes its
			// first corner is on the visible side, and a dent or a twist adds more, so that trail can go past the nine-corner arrays the clipper uses as things
			// stand. The arithmetic came to about fourteen for the shapes tried, which isn't a proven ceiling. Only the clipper stopping at nine itself (CLIP-1)
			// keeps that safe. This code copes with any number of corners, so it isn't what does.
			for (uint32_t i = numVerts - 2; i >= 1; i--)
			{
				AddIndices(indexArray, 0, i, i + 1, numVerts);
			}

			break;
		}
	}
}

// adds a single triangle's worth of indices to an index buffer in the correct order
// where a, b and c are the index order, and n is the number of verts we've just added to the VB
void RenderList::AddIndices(uint16_t *indexArray, uint32_t a, uint32_t b, uint32_t c, uint32_t n)
{
	if (!BuffersExist())
	{
		return;
	}

	uint32_t vCount = (**vb).GetSize();
	uint32_t iCount = (**ib).GetSize();

	// AddItem has saved room for a shape's numbers, but this is what writes them, and a mesh's triangles only have its one AddItem between them. It's the last place to stop.
	if (iCount > (**ib).GetCapacity() || 3 > (**ib).GetCapacity() - iCount)
	{
		NoteLeftOutBecauseFull();
		return;
	}

	indexArray[iCount]   = (vCount - (n) + (a));
	indexArray[iCount+1] = (vCount - (n) + (b));
	indexArray[iCount+2] = (vCount - (n) + (c));
	(**ib).SetSize(iCount + 3);
}

// The list of shapes grows when it needs to. This is what keeps the next entry inside it.
void RenderList::MakeRoomForOneMoreItem()
{
	if (GetSize() + 1 >= Items.size())
	{
		// list is full. let's just resize it
		LogString("Resizing list " + listName);
		Items.resize(Items.size() * 2);
	}
}

void RenderList::AddCommand(int commandType)
{
	// Commands use none of the buffers, so they only need room in the list itself. Two in a row at the very end of the
	// list used to write one entry past it.
	MakeRoomForOneMoreItem();

	Items[listIndex].sortKey = 0;
	set_command     (Items[listIndex].sortKey, 1);
	set_commandType (Items[listIndex].sortKey, commandType);
	listIndex++;
}

void RenderList::SetLayer(int layer)
{
	this->layer = layer;
//	Items[listIndex].layerID = layer;
}

// Is there room in the buffers for one more shape with this many corners?
bool RenderList::Check(uint32_t numVerts)
{
	return Check(numVerts, GetNumIndices(numVerts));
}

// The same, for this many corners and this many index numbers. Taking what's used away from the room can't wrap round to a small number the way adding a huge
// count to it could. What's used can be set from several places, so a size past the room is checked for first, rather than trusted never to happen.
bool RenderList::Check(uint32_t numVerts, uint32_t numIndices)
{
	if (!BuffersExist())
	{
		return false;
	}

	uint32_t const vertexSize = (**vb).GetSize(), vertexRoom = (**vb).GetCapacity();
	uint32_t const indexSize = (**ib).GetSize(), indexRoom = (**ib).GetCapacity();

	if (vertexSize > vertexRoom || numVerts > vertexRoom - vertexSize)
	{
		return false;
	}

	if (indexSize > indexRoom || numIndices > indexRoom - indexSize)
	{
		return false;
	}

	return true;
}

// Says once that shapes are being left out because the list is full, and counts them.
void RenderList::NoteLeftOutBecauseFull()
{
	if (leftOutBecauseFull == 0) {
		LogString("Render list " + listName + " is full, so a shape is left out. Any more like it won't be logged.");
	}
	leftOutBecauseFull++;
}

bool RenderList::AddItem(uint32_t numVerts, texID_t textureID, enum TRANSLUCENCY_TYPE translucencyMode, enum FILTERING_MODE_ID filteringMode, enum TEXTURE_ADDRESS_MODE textureAddress)
{
	assert(numVerts != 0);

	// The caller is about to write this shape's corners into the buffers, so both have to be open: a closed buffer's
	// memory belongs to the graphics card's driver again. A buffer also stays closed if opening it failed, and it isn't there at all
	// while the game is getting the graphics card back after losing it.
	if (!BuffersExist() || !(**vb)._isLocked || !(**ib)._isLocked)
	{
		if (leftOutBecauseClosed == 0) {
			LogString("Render list " + listName + " was asked for a shape while it was closed, so the shape is left out. Any more like it won't be logged.");
		}
		leftOutBecauseClosed++;
		return false;
	}

	// Both also need room for the whole shape. Leaving it out costs one shape for one frame. Writing past the end of a
	// buffer could damage anything at all.
	if (!Check(numVerts))
	{
		NoteLeftOutBecauseFull();
		return false;
	}

	// check that we have room in the render list array
	MakeRoomForOneMoreItem();

	uint32_t numIndices = GetNumIndices(numVerts);

	Items[listIndex].sortKey = 0; // zero it out

	set_layerID        (Items[listIndex].sortKey, layer);
	set_texID          (Items[listIndex].sortKey, textureID);
	set_transType      (Items[listIndex].sortKey, translucencyMode);
	set_filterType     (Items[listIndex].sortKey, filteringMode);
	set_texAddressType (Items[listIndex].sortKey, textureAddress);

	// lets see if we can merge this item with the previous item
	if ((listIndex != 0) &&		// only do this check if we're not adding the first item
		(Items[listIndex-1].sortKey == Items[listIndex].sortKey))
	{
		// our new item uses the same states as previous item.
		listIndex--;
	}
	else
	{
		// we need to add a new item
		Items[listIndex].vertStart  = (**vb).GetSize();
		Items[listIndex].indexStart = (**ib).GetSize();
	}

	Items[listIndex].vertEnd = (**vb).GetSize() + numVerts;
	Items[listIndex].indexEnd = (**ib).GetSize() + numIndices;

	listIndex++;

	// update the vertex buffer object with how many items it's holding
	uint32_t currentSize = (**vb).GetSize();
	(**vb).SetSize(currentSize + numVerts);

	return true;
}

void RenderList::Sort()
{
	// sort the list on the sortKey int value. We use the vector
	// as an array so we can't do Items.end() as we don't use push_back
	// or pop_back. Our size is controlled by the value of listIndex
	std::sort(Items.begin(), Items.begin() + listIndex);
}

// Writes a line to the log each time this list gets through another quarter of its room, at most four lines per list.
// It's there to answer one question from a real run: how close does a busy frame come to filling each list?
void RenderList::ReportHowFullItGot()
{
	if (!BuffersExist())
	{
		return;
	}

	uint32_t const vertexRoom = (**vb).GetCapacity();
	uint32_t const indexRoom  = (**ib).GetCapacity();

	if (!vertexRoom || !indexRoom) {
		return;
	}

	uint32_t const verts   = (**vb).GetSize();
	uint32_t const indices = (**ib).GetSize();

	// Multiplied as 64-bit numbers, so a very large count can't wrap round to a small one. Whichever buffer is fuller
	// is the one that counts.
	uint32_t const vertexQuarters = static_cast<uint32_t>((static_cast<uint64_t>(verts)   * 4) / vertexRoom);
	uint32_t const indexQuarters  = static_cast<uint32_t>((static_cast<uint64_t>(indices) * 4) / indexRoom);
	uint32_t const quarters = (vertexQuarters > indexQuarters) ? vertexQuarters : indexQuarters;

	if (quarters > quartersReported)
	{
		quartersReported = quarters;
		LogString("Render list " + listName + " reached " + std::to_string(verts) + " of " + std::to_string(vertexRoom) + " corners and " + std::to_string(indices) + " of " + std::to_string(indexRoom) + " index numbers");
	}
}

void RenderList::Reset()
{
	// the frame's numbers are about to go, so this is the moment to look at them
	ReportHowFullItGot();

	// reset our index back to the first item
	listIndex = 0;

	// lets specify that the VB and IB are now empty
	if (BuffersExist())
	{
		(**vb).SetSize(0);
		(**ib).SetSize(0);
	}
}

// Draws this list's solid polygons a second time, for a pass that adds light to them. The caller has set the shaders and
// the blending. This goes straight to the device, because the caller puts the device back afterwards, and the game's own
// record of its settings has to stay true.
void RenderList::DrawOpaqueAgain(VertexDeclaration *declaration, bool withRelief, bool withHeight)
{
	declaration->Set();
	(**vb).Set();
	(**ib).Set();

	for (size_t i = 0; i < listIndex; i++)
	{
		RenderItem *it = &Items[i];
		uint32_t numPrimitives = (it->indexEnd - it->indexStart) / 3;

		if (get_command(it->sortKey) || !numPrimitives || get_transType(it->sortKey) != TRANSLUCENCY_OFF)
		{
			continue;
		}

		DWORD const addressing = (get_texAddressType(it->sortKey) == TEXTURE_WRAP) ? D3DTADDRESS_WRAP : D3DTADDRESS_CLAMP;

		d3d.lpD3DDevice->SetTexture(0, Tex_GetTexture(get_texID(it->sortKey)));
		d3d.lpD3DDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, addressing);
		d3d.lpD3DDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, addressing);

		DWORD const filter = (get_filterType(it->sortKey) == FILTERING_BILINEAR_ON) ? D3DTEXF_LINEAR : D3DTEXF_POINT;

		d3d.lpD3DDevice->SetSamplerState(0, D3DSAMP_MINFILTER, filter);
		d3d.lpD3DDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, filter);

		// the relief picture lies exactly over its texture, so it's read the same way
		if (withRelief)
		{
			extern void NoteTextureUse(texID_t textureID);
			NoteTextureUse(textureRelief.PictureFor(get_texID(it->sortKey)));
			d3d.lpD3DDevice->SetTexture(1, Tex_GetTexture(textureRelief.PictureFor(get_texID(it->sortKey))));
			d3d.lpD3DDevice->SetSamplerState(1, D3DSAMP_ADDRESSU, addressing);
			d3d.lpD3DDevice->SetSamplerState(1, D3DSAMP_ADDRESSV, addressing);
			d3d.lpD3DDevice->SetSamplerState(1, D3DSAMP_MINFILTER, filter);
			d3d.lpD3DDevice->SetSamplerState(1, D3DSAMP_MAGFILTER, filter);
		}

		// The depth map lies over it the same way. It's on the card only for the Relief map lighting.
		if (withHeight)
		{
			extern void NoteTextureUse(texID_t textureID);
			NoteTextureUse(textureRelief.HeightFor(get_texID(it->sortKey)));
			d3d.lpD3DDevice->SetTexture(2, Tex_GetTexture(textureRelief.HeightFor(get_texID(it->sortKey))));
			d3d.lpD3DDevice->SetSamplerState(2, D3DSAMP_ADDRESSU, addressing);
			d3d.lpD3DDevice->SetSamplerState(2, D3DSAMP_ADDRESSV, addressing);
		}

		R_DrawIndexedPrimitive(0, 0, (**vb).GetSize(), it->indexStart, numPrimitives);
	}

	if (withRelief)
	{
		d3d.lpD3DDevice->SetTexture(1, NULL);
	}

	if (withHeight)
	{
		d3d.lpD3DDevice->SetTexture(2, NULL);
	}
}

void RenderList::Draw(Part part, VertexDeclaration *declarationWithHeights)
{
	uint32_t baseIndexValue = 0;

	if (!BuffersExist() || !*decl)
	{
		return;
	}

	// The sky is drawn before anything else, as glowing polygons with depth writes off, so that the level simply covers it. That makes it
	// see-through by its kind, but it has to stay in the solid half, or it would be drawn over the level.
	bool depthWritesOff = false;

	// set vertex declaration
	if (declarationWithHeights)
	{
		declarationWithHeights->Set();

		// The shadow pass and the surface pass both use stage 2 without telling R_SetTexture, so what it remembers about that stage may not be so.
		R_ForgetTexture(2);
	}
	else
	{
		(**decl).Set();
	}

	// set vb and ibs to active
	(**vb).Set();
	(**ib).Set();

	for (size_t i = 0; i < listIndex; i++)
	{
		RenderItem *it = &Items[i];

		if (get_command(it->sortKey)) {
			switch (get_commandType(it->sortKey)) {
				case kCommandZClear:
					R_ClearZBuffer();
					break;
				case kCommandZWriteEnable:
					ChangeZWriteEnable(ZWRITE_ENABLED);
					depthWritesOff = false;
					break;
				case kCommandZWriteDisable:
					ChangeZWriteEnable(ZWRITE_DISABLED);
					depthWritesOff = true;
					break;
			}
		}
		else
		{
			uint32_t numPrimitives = (it->indexEnd - it->indexStart) / 3;
			bool const solid = get_transType(it->sortKey) == TRANSLUCENCY_OFF || depthWritesOff;

			if (numPrimitives && (part == kEverything || (part == kSolid) == solid))
			{
				// set texture
				R_SetTexture(0, get_texID(it->sortKey));
				ChangeTranslucencyMode((enum TRANSLUCENCY_TYPE)			get_transType(it->sortKey));
				ChangeFilteringMode(0, (enum FILTERING_MODE_ID)			get_filterType(it->sortKey));
				ChangeTextureAddressMode(0, (enum TEXTURE_ADDRESS_MODE) get_texAddressType(it->sortKey));

				// the height picture lies exactly over its texture, so it wraps where the texture wraps
				if (declarationWithHeights)
				{
					R_SetTexture(2, textureRelief.HeightFor(get_texID(it->sortKey)));
					ChangeTextureAddressMode(2, (enum TEXTURE_ADDRESS_MODE) get_texAddressType(it->sortKey));
				}

				R_DrawIndexedPrimitive(baseIndexValue, 0, (**vb).GetSize(), it->indexStart, numPrimitives);
			}
		}
	}

	if (declarationWithHeights)
	{
		d3d.lpD3DDevice->SetTexture(2, NULL);
		R_ForgetTexture(2);
	}
}
