// quad_part.h: where on a texture a quad showing only PART of it reads from (d3d_render.cpp DrawQuadPart). Written 2026-11-23 by Melanikus (Frost) for the AvP
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

// RED STUB (tests/quad_part_test.cpp): refuses every part
inline bool QuadPart_UVs(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float *)
{
	return false;
}

#endif
