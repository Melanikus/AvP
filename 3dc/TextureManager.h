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

// Modified 2026-11-05 by Melanikus for the AvP port:
//   a square picture with all its smaller copies
// Modified 2026-11-23 by Melanikus (Frost) for the AvP port:
//   heldForReset: a texture released only so the device can be reset keeps its slot, apart from a free one (tests/texture_manager_test.cpp)

#ifndef _TextureManager_h_
#define _TextureManager_h_

#include "renderer.h"
#include "stdint.h"
#include <string>
#include <vector>

enum TextureUsage
{
	TextureUsage_Normal,
	TextureUsage_Dynamic,

	// A PICTURE DRAWN INTO ON THE CARD (added 2026-11-23 by Melanikus (Frost) for the level editor's live picker panes, R-INT-16): 32 bits a pixel only, it
	// lives on the card alone, so it is released for a reset of the device and made again after, as a Dynamic one is, and it is never locked (the card
	// draws into it; nothing is written into it from here)
	TextureUsage_RenderTarget
};

enum TextureLock
{
	TextureLock_Normal,
	TextureLock_Discard
};

struct Texture
{
	std::string		name;
	TextureUsage	usage;
	bool			isValid;

	// RELEASED ONLY FOR A RESET OF THE DEVICE: a texture that lives on the card alone (TextureUsage_Dynamic) is released before the device can be reset, and
	// made again after (Tex_ReleaseDynamicTextures, Tex_ReloadDynamicTextures). All that while its owner keeps its number, so its slot is still its owner's:
	// never given to another texture, and made again by the reset only while this is true. Its owner releasing it (Tex_Release) frees the slot then.
	bool			heldForReset;

	// width and height of textures before adjustment (eg 640x480)
	uint16_t		width;
	uint16_t		height;

	// what the graphics card really made, which can be larger than was asked for (eg 1024x512), or smaller, if the card has a limit
	uint16_t		realWidth;
	uint16_t		realHeight;

	uint8_t			bitsPerPixel;
	r_Texture		texture;

	// constructor
	Texture()
	{
		usage = TextureUsage_Normal;
		isValid = false;
		heldForReset = false;
		width  = 0;
		height = 0;
		realWidth  = 1;
		realHeight = 1;
		bitsPerPixel = 0;
		texture = 0;
	}
};

texID_t Tex_Create         (const std::string &textureName, uint32_t width, uint32_t height, uint32_t bitsPerPixel, enum TextureUsage usageType);
texID_t Tex_AddTexture     (const std::string &textureName, r_Texture texture, uint32_t width, uint32_t height, uint32_t bitsPerPixel, enum TextureUsage usage);
// edgeCell: for a sheet of small pictures, the size of its squares, so a see-through texel never takes a colour from the next one
// (rim_edges.h). 0, for every single picture. Added 2026-11-11 by Melanikus for the AvP port.
texID_t Tex_CreateFromRIM  (const std::string &fileName, uint32_t edgeCell = 0);
texID_t Tex_CreateWithCopies (const std::string &textureName, uint32_t side, uint32_t levels);
bool Tex_LockCopy   (texID_t textureID, uint32_t level, uint8_t **data, uint32_t *pitch);
bool Tex_UnlockCopy (texID_t textureID, uint32_t level);
texID_t Tex_CreateFromFile (const std::string &filePath);
// A .BMP picture read by bmp_picture.h (never handed to D3DX: it comes from anyone), opaque; MISSING_TEXTURE when there's no such file or it isn't one
// bmp_picture.h takes. For the level editor's Prefer .BMP over .RIM (projload.cpp). Added 2026-11-18 by Melanikus (Frost) for the AvP port.
texID_t Tex_CreateFromBMP  (const std::string &fileName);
texID_t Tex_CheckExists    (const std::string &textureName);

texID_t Tex_AddExistingTexture(Texture &texture);
const Texture& Tex_GetTextureDetails(texID_t textureID);

// texID_t Tex_CreateFromAvPTexture  (const std::string &textureName, AVPTEXTURE &AvPTexure, enum TextureUsage usageType);
texID_t Tex_CreateTallFontTexture (const std::string &textureName, AVPTEXTURE &AvPTexure, enum TextureUsage usageType);

std::string& Tex_GetName	(texID_t textureID);
const r_Texture& Tex_GetTexture	(texID_t textureID);
void Tex_GetNamesVector		(std::vector<std::string> &namesArray);
void Tex_GetDimensions		(texID_t textureID, uint32_t &width, uint32_t &height);
bool Tex_GetRealSize		(texID_t textureID, uint32_t *width, uint32_t *height);

bool Tex_Lock	(texID_t textureID, uint8_t **data, uint32_t *pitch, enum TextureLock lockType = TextureLock_Normal);
bool Tex_Unlock	(texID_t textureID);

void Tex_Release(texID_t textureID);
void Tex_DeInit();
void Tex_ReleaseDynamicTextures();
void Tex_ReloadDynamicTextures();
void Tex_CheckMemoryUsage();
void Tex_ListTextures();

#endif

