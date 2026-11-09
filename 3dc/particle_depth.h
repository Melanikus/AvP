// How far out a particle "drawn at the back" is moved, so that everything the level draws is in front of it (#176).
// Written 2026-11-09 by Melanikus for the AvP port. Reported by Bobblen147.
//
// THE 1999 GAME: particle.cpp's table marks some particles "drawn at the back" (IsDrawnAtBack). The only one is the star, PARTICLE_STAR:
// the starry night sky on levels like Stranded and Orbital. The 1999 Direct3D code gave such a particle's corners the depth at the very
// back of the view, whatever their real distance, so every wall and cliff the level drew was in front of it. The stars need that: they sit
// on a ball only about 65 metres round the camera (CreateStarArray in kshape.cpp), so at their own distance a cliff further away than that
// is behind them.
//
// WHAT WENT WRONG: when the game was moved to the graphics card's own transform (the hardware T&L port), a particle's corners became points
// in front of the camera, and the card works their depth out from how far away they are. The drawing code still worked out the "at the
// back" depth, but never used it. So the stars were drawn at their real 65 metres, in front of every cliff further away than that: #176.
//
// THE CURE: each corner is moved out along its own line from the eye. A point twice as far away and twice as far to the side lands on the
// same pixel, so the star covers exactly the pixels it did, in the same colour (the particles' drawing reads nothing from their depth).
// Only its depth changes: now everything the level draws nearer than where it's moved to is in front of it.
//
// HOW FAR: 99% of the farthest the camera draws (zFar in d3d_render.cpp, 1000 metres), so 990 metres. Not all the way: a corner exactly at
// the farthest can be cut off by rounding, and the star would flicker. Depths bunch up towards the back, so with the 24-bit depth picture
// the game asks for first (D3DFMT_D24S8), scenery up to about 980 metres away is clearly in front of a star. Only scenery in the last 2% the
// camera draws could be level with one.
//
// A corner nearer than the camera's nearest (zNear), behind the camera, or not a number at all is left where it is, so the card cuts it off
// as it always has.

#ifndef PARTICLE_DEPTH_H
#define PARTICLE_DEPTH_H

// How much of the farthest distance the camera draws a particle at the back is moved out to
const float kParticleAtTheBackShare = 0.99f;

// What to multiply a corner's three numbers by, to move it out to the back. distance is how far in front of the camera the corner is,
// nearest and farthest are the camera's nearest and farthest. Gives back 1, which moves nothing, for a corner the card won't draw anyway.
inline float ParticleDepth_AtTheBack(float distance, float nearest, float farthest)
{
	// written this way round so a distance that isn't a number (NaN) also lands here: every comparison with one is false
	if (!(distance >= nearest))
	{
		return 1.0f;
	}

	return (farthest * kParticleAtTheBackShare) / distance;
}

#endif
