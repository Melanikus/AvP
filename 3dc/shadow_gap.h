// How far behind a polygon its shadow starts. Written 2026-11-04 by Melanikus for the AvP port.
//
// A polygon's shadow has to start a little way behind it, or the polygon would be inside its own shadow, and shadow itself in speckles. The start is found by going
// along the line from the light through each corner, until that line is far enough behind the polygon's flat surface.
//
// WHY EACH CORNER NEEDS ITS OWN: a line that hits a surface at a low angle has to go a long way to get a short way behind it. A lamp beside a long wall sees the near end
// of the wall almost square on, and the far end almost edge on. If every corner goes the same distance along its line, the far end of the shadow's start can be under
// one unit behind the wall. The game cuts each corner it draws down to a whole number, which can move a surface by up to 1.73 units, and which way changes as the
// camera moves. So a start that close to the surface is sometimes in front of it, and the wall flickers in and out of its own shadow.
//
// With a distance worked out for each corner, the start is one flat surface, the same distance behind the polygon everywhere. Only a very low angle needs the distance to
// be large, and even then nothing goes wrong, because the start stays that same small distance behind the polygon: all that's ever left out of the shadow is that thin
// slab. On the stock levels, the limit below leaves 48 of 966,760 polygons lit by a lamp closer to their shadow's start than the cut can reach.

#ifndef SHADOW_GAP_H
#define SHADOW_GAP_H

#include <math.h>

static const float kShadowGapWidest = 20000.0f;   // only so that a polygon almost exactly edge on to a light doesn't throw a corner out past the end of the world

// How far along its line from the light a corner's shadow starts. startGap is how far behind the flat surface it has to be, length is how far the corner is from the
// light, and planeDistance how far the light is from the flat surface. It goes up to the next quarter of a unit, so that two polygons lying flat together give exactly
// the same answer, even if their flat surfaces differ in the last digit.
inline float ShadowGap_ForCorner(float startGap, float length, float planeDistance, float widestGap)
{
	float const gap = ceilf(startGap * length / planeDistance * 4.0f) * 0.25f;

	return (gap < widestGap) ? gap : widestGap;
}

#endif
