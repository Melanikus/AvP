// quad_part.h: where on a texture a quad showing only PART of it reads from (the places d3d_render.cpp DrawFontQuad takes). Written 2026-11-23 by Melanikus (Frost) for the AvP
// port, for the level editor's live picker panes: they draw every pane into one texture (their atlas, TextureUsage_RenderTarget), and each pane's quad shows its
// own region of it.
//
// The part is given in texels: its top left (u, v) and its size (partWidth by partHeight), inside what the texture holds (width by height: Texture::width and
// height, what was asked for). The card may have made the texture larger (realWidth by realHeight: Texture::realWidth and realHeight), so the places are
// fractions of what it made. False, and uv left as it was, for a part of no size, a part not wholly inside what the texture holds, or sizes that make no sense
// (a texture of no real size, or holding more than the card made): nothing is drawn then.
//
// uv: left, top, right, bottom, each 0 to 1 across what the card made.

#ifndef _quad_part_h_
#define _quad_part_h_

#include <stdint.h>

inline bool QuadPart_UVs(uint32_t width, uint32_t height, uint32_t realWidth, uint32_t realHeight, uint32_t u, uint32_t v, uint32_t partWidth, uint32_t partHeight,
	float uv[4])
{
	// each test by subtraction from what the texture holds, never by adding to the part's start, so no large number can wrap round and pass
	if (partWidth == 0 || partHeight == 0 || realWidth == 0 || realHeight == 0 || width > realWidth || height > realHeight || u > width || v > height ||
		partWidth > width - u || partHeight > height - v)
	{
		return false;
	}

	uv[0] = (float)u / (float)realWidth;
	uv[1] = (float)v / (float)realHeight;
	uv[2] = (float)(u + partWidth) / (float)realWidth;    // (u + partWidth is at most width here: no wrap)
	uv[3] = (float)(v + partHeight) / (float)realHeight;
	return true;
}

#endif
