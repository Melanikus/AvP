// Is the texture the graphics card offers big enough for what's going to be written into it? Written 2026-11-04 by Melanikus for the AvP port.
//
// A picture or a film says how big it is, so it can say anything. The game asks Direct3D's helper, D3DXCheckTextureRequirements, about a texture of that size, and the helper
// "corrects" the size to what the card can do. That can mean smaller: asked for 20000 x 1, on a card whose limit is 16384, it says yes, 16384 x 1. The code that fills the texture
// still writes 20000 pixels to a row, because that's what the file said, and runs past the end of the texture's memory. So smaller has to be refused.
//
// Bigger is fine. A card that wants sides that are a power of two gives more room than was asked for, and the code that fills the texture leaves the rest alone.

#ifndef TEXTURE_SIZE_CHECK_H
#define TEXTURE_SIZE_CHECK_H

#include <d3d9.h>
#include <stdint.h>

// The game keeps a texture's sizes in 16-bit numbers (see Texture in TextureManager.h), so a bigger one would come back as a different number when it's read later
static uint32_t const kTextureSize_LargestSide = 65535;

// How many bits a pixel takes in the formats the helper can reasonably hand back. A format that isn't listed counts as 0, which makes it too small, so it gets refused.
inline uint32_t TextureSize_BitsPerPixel(D3DFORMAT format)
{
	switch (format)
	{
		case D3DFMT_A8R8G8B8:
		case D3DFMT_X8R8G8B8:
		case D3DFMT_A8B8G8R8:
		case D3DFMT_X8B8G8R8:
			return 32;

		case D3DFMT_A1R5G5B5:
		case D3DFMT_X1R5G5B5:
		case D3DFMT_R5G6B5:
		case D3DFMT_A4R4G4B4:
		case D3DFMT_A8L8:
			return 16;

		case D3DFMT_L8:
			return 8;

		default:
			return 0;
	}
}

// "asked" is what the caller is going to write. "got" is what the helper said it can have.
inline bool TextureSize_IsEnough(uint32_t askedWidth, uint32_t askedHeight, uint32_t askedBitsPerPixel, uint32_t gotWidth, uint32_t gotHeight, uint32_t gotBitsPerPixel)
{
	if ((gotWidth > kTextureSize_LargestSide) || (gotHeight > kTextureSize_LargestSide))
	{
		return false;
	}

	return (gotWidth >= askedWidth) && (gotHeight >= askedHeight) && (gotBitsPerPixel >= askedBitsPerPixel);
}

#endif
