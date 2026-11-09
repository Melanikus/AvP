// Where an animated texture's corners are on its picture, between two of its frames. Written 2026-11-08 by Melanikus for the AvP port
// (issue #97, reported by Bobblen: the lava of Fury 161, the tunnel lights of Earthbound and the predalien sphere of Vaults).
//
// WHAT AN ANIMATED TEXTURE IS: some polygons in the levels move their picture over themselves, frame by frame: the molten lead's flow,
// a screen, a light. The level file gives each frame a picture and, for each corner of the polygon, a place on that picture, in texels
// (the picture's own dots). Some animations step from frame to frame. The ones that flow, 986 of the 11,317 in the game's 86 files, slide
// smoothly from one frame's places to the next: that's "interpolating", and a corner halfway between frames is halfway between the two.
//
// WHAT WENT WRONG: in 1999 every corner was worked out in sixteenths of sixteenths of a texel (16.16 fixed point: 65536 to a texel).
// The port changed the game's ordinary polygons to whole texels, and the animated ones' places with them. But a place between two
// frames is a fraction of a texel. In whole texels a corner can only jump a whole texel at a time, and the sum dropped each part's
// fraction, so a corner that doesn't move at all came out a texel short for most of each frame (24 and 24 gave 23). The flowing
// animations jerked and shook on the spot, on every level that has one.
//
// NOW: the places between frames are worked out in 16.16 again, exactly, and the two drawing functions that take an animated polygon
// (d3d_render.cpp) read its corners in 16.16, as the sky's already were. Every other polygon stays in whole texels.
//
// This file needs nothing from the rest of the game, so that tests/texture_animation_test.cpp can try every case.

#ifndef TEXTURE_ANIMATION_H
#define TEXTURE_ANIMATION_H

// The furthest a corner's place on its picture can be from the picture's corner, in texels, either way. In 16.16 that's just under
// half of what a whole number holds, so that the game's clipping, which takes the difference between two corners, never runs over.
// Every animated texture in the game's own files is between 0 and 127.
static const int kTextureAnimationFurthestTexel = 16383;

// The level file is input (AvP Tao 3): are these places, count of them, ones the game can play? The loader asks this of each
// animation, and one that doesn't fit stays still, as it does when one of its pictures isn't there. No list, or a negative count, isn't.
bool TextureAnimation_PlacesFit(const int *texels, long long count);

// The corners of an animated polygon at a moment between two of its frames, in 16.16 fixed point. firstFrame and secondFrame are the
// two frames' places, in texels, two numbers a corner (across, then down). alpha is how far it is from the first frame to the second,
// in 16.16: 0 is the first frame and 65536 the second, and anything outside that is brought inside. With interpolate false, it's the
// first frame's places, as an animation that steps shows them. corners is at
// most 4, as the loader makes them, and out has room for two numbers a corner.
void TextureAnimation_Corners(const int *firstFrame, const int *secondFrame, int corners, int alpha, bool interpolate, int *out);

#endif
