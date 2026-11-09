// Where a polygon's corner falls on its texture, in the numbers Direct3D reads: 0 at one edge of the texture and 1 at the other.
// Written 2026-11-08 by Melanikus for the AvP port (#148). Reported by the user; Bobblen (avp-forever) pointed at the old fixed-point maths.
//
// THE GAME'S NUMBERS: a corner names a texel. 0 is the first texel of a row, and 127 the last of a texture 128 wide. A polygon using
// texels 192 to 255 of a 256-wide texture says 192 and 255, both ends included. That was right for the 1999 game's own drawing, which
// painted the texel each number named.
//
// DIRECT3D'S NUMBERS: Direct3D reads between texels. Its 0 is the left edge of the first texel, not its middle, and the middle of
// texel n is (n + 0.5) / width. The 1999 Direct3D code knew that, and put every corner in the middle of its texel:
// ((U >> 16) + 0.5f) * RecipW, or the same with a fixed 1.0f / 256.0f, which is half a texel of a texture 128 wide.
//
// WHAT WENT WRONG: when the game was moved to the graphics card's own transform (the hardware T&L port), that half texel was left
// out. So every polygon's edge landed exactly on the line between two texels. With smooth (bilinear) filtering and a texture that
// repeats (wrap), the last half texel inside a polygon's top edge takes half its colour from the texture's bottom row, and the same at
// the other three edges. On a cliff whose top is see-through and whose bottom is rock, that's a thin line of half-rock along the top,
// where there should be nothing: #148.
//
// THE CURE is the 1999 one: the middle of the texel the corner names. This doesn't change which texels a polygon shows. A texture
// that repeats moves by half a texel, which nobody can see.
//
// cornerNumbersPerTexel: how many of the corner's numbers make one texel. 1 for most polygons; 65536 where the corners are in 16.16
// fixed point, as the sky's are, and animated textures' are once #97's fix is in. recipOfTextureInCornerNumbers: 1 divided by (the
// texture's width or height in texels, times cornerNumbersPerTexel). The drawing code works that out once a polygon.

#ifndef TEXEL_CENTRE_H
#define TEXEL_CENTRE_H

inline float TexelCentre(int corner, float cornerNumbersPerTexel, float recipOfTextureInCornerNumbers)
{
	return ((float)corner + 0.5f * cornerNumbersPerTexel) * recipOfTextureInCornerNumbers;
}

#endif
