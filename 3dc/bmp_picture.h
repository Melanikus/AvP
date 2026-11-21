// Reads a .BMP picture, for the level editor's Prefer .BMP over .RIM. Written 2026-11-18 by Melanikus (Frost) for the AvP port.
//
// WHY: inteng's Construction > Options > Prefer .BMP over .RIM (0x1f5, DAT_00636f88) has its picture loader FUN_0046df30 try the picture's name with the
// ending after its last '.' made ".bmp" before it takes the .RIM, so a mapper can try a picture without making a .RIM of it each time (the Bible:
// "any textures loaded as .BMPs won't be able to use transparency"). The game never reads a .BMP, and the one loader it has for a file of any kind hands it
// to D3DX whole. A picture beside a level comes from anyone, so this reads it itself, and refuses anything it does not fully understand.
//
// WHAT IT TAKES: "BM", a BITMAPINFOHEADER (40 bytes) or a later one (its own size, up to 124), one plane, no compression (BI_RGB), 8 bits a texel with a
// palette of at most 256 colours, or 24 or 32 bits; 1 to 4096 texels a side, rows bottom-up or (a negative height) top-down; rows padded to 4 bytes; the
// texels and the palette inside the file. Nothing else: no RLE, no bit fields, no 1 / 4 / 16 bits.
// WHAT IT GIVES: 4 bytes a texel, blue, green, red and alpha 255 (opaque, as the Bible says), top row first, rows tightly packed (width x 4 bytes).
// The answer is false, with why saying what was refused, and the texels untouched, for any file it doesn't take.

#ifndef BMP_PICTURE_H
#define BMP_PICTURE_H

#include <stddef.h>
#include <stdint.h>
#include <vector>

uint32_t const kBmpPictureMostSide = 4096;
uint32_t const kBmpPictureMostBytes = 64u * 1024u * 1024u;   // a 4096 x 4096 picture of 32 bits is 64 MiB

inline uint32_t BmpPicture_U32(const uint8_t *at)
{
	return (uint32_t)at[0] | ((uint32_t)at[1] << 8) | ((uint32_t)at[2] << 16) | ((uint32_t)at[3] << 24);
}

inline uint16_t BmpPicture_U16(const uint8_t *at)
{
	return (uint16_t)(at[0] | (at[1] << 8));
}

inline bool BmpPicture_Decode(const uint8_t *bytes, size_t size, uint32_t *width, uint32_t *height, std::vector<uint8_t> *texels, const char **why)
{
	char const *whyNot = 0;
	char const **say = why ? why : &whyNot;

	if (!bytes || !width || !height || !texels)
	{
		*say = "nothing to read";
		return false;
	}

	if (size < 14 + 40 || size > kBmpPictureMostBytes + 2048)
	{
		*say = "its size can't be a picture's";
		return false;
	}

	if (bytes[0] != 'B' || bytes[1] != 'M')
	{
		*say = "it doesn't start with BM";
		return false;
	}

	uint32_t const dataAt = BmpPicture_U32(bytes + 10);
	uint32_t const headerSize = BmpPicture_U32(bytes + 14);

	if (headerSize < 40 || headerSize > 124 || 14 + (size_t)headerSize > size)
	{
		*say = "its header isn't one this reads";
		return false;
	}

	int32_t const w = (int32_t)BmpPicture_U32(bytes + 18);
	int32_t const h = (int32_t)BmpPicture_U32(bytes + 22);
	uint16_t const planes = BmpPicture_U16(bytes + 26);
	uint16_t const bits = BmpPicture_U16(bytes + 28);
	uint32_t const compression = BmpPicture_U32(bytes + 30);
	uint32_t const coloursUsed = BmpPicture_U32(bytes + 46);

	if (planes != 1 || compression != 0 || (bits != 8 && bits != 24 && bits != 32))
	{
		*say = "it isn't an uncompressed picture of 8, 24 or 32 bits";
		return false;
	}

	// a negative height is a picture stored top row first; INT32_MIN has no positive, so it's refused with the rest
	if (w < 1 || w > (int32_t)kBmpPictureMostSide || h == 0 || h < -(int32_t)kBmpPictureMostSide || h > (int32_t)kBmpPictureMostSide)
	{
		*say = "its size is not 1 to 4096 texels a side";
		return false;
	}

	uint32_t const across = (uint32_t)w;
	uint32_t const down = (uint32_t)(h < 0 ? -h : h);
	bool const topFirst = h < 0;

	// each row is padded to 4 bytes; at most 4096 x 4 = 16384 bytes, so none of this can wrap
	uint32_t const rowBytes = ((across * bits + 31) / 32) * 4;
	uint64_t const texelBytes = (uint64_t)rowBytes * down;

	uint32_t paletteCount = 0;

	if (bits == 8)
	{
		paletteCount = coloursUsed == 0 ? 256 : coloursUsed;

		if (paletteCount > 256)
		{
			*say = "its palette has more than 256 colours";
			return false;
		}

		if (14 + (uint64_t)headerSize + (uint64_t)paletteCount * 4 > size)
		{
			*say = "its palette runs past the end of the file";
			return false;
		}
	}

	if (dataAt < 14 + headerSize + paletteCount * 4 || (uint64_t)dataAt + texelBytes > size)
	{
		*say = "its texels are not inside the file";
		return false;
	}

	const uint8_t *const palette = bytes + 14 + headerSize;
	std::vector<uint8_t> out((size_t)across * down * 4);

	for (uint32_t y = 0; y < down; y++)
	{
		uint32_t const fileRow = topFirst ? y : down - 1 - y;
		const uint8_t *const row = bytes + dataAt + (size_t)fileRow * rowBytes;
		uint8_t *const to = &out[(size_t)y * across * 4];

		for (uint32_t x = 0; x < across; x++)
		{
			uint8_t blue, green, red;

			if (bits == 8)
			{
				uint8_t const index = row[x];

				if (index >= paletteCount)
				{
					*say = "a texel names a colour past its palette";
					return false;
				}

				blue = palette[index * 4];
				green = palette[index * 4 + 1];
				red = palette[index * 4 + 2];
			}
			else
			{
				uint32_t const step = bits / 8;
				blue = row[x * step];
				green = row[x * step + 1];
				red = row[x * step + 2];
			}

			to[x * 4] = blue;
			to[x * 4 + 1] = green;
			to[x * 4 + 2] = red;
			to[x * 4 + 3] = 255;   // a .BMP has no see-through texels (the Bible)
		}
	}

	*width = across;
	*height = down;
	texels->swap(out);
	return true;
}

#endif
