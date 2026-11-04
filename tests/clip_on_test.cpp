// The test of clipping again, with the switch USE_FRUSTUM_CULL turned on. Written 2026-11-04 by Melanikus for the AvP port.
//
// The game doesn't build with this switch on today. If it ever did, every polygon crossing the edge of the view would be clipped, including polygons from a level file, which anyone can
// make. So this checks the limit holds that way too. Everything is explained in clip_test.cpp.

#define USE_FRUSTUM_CULL
#include "clip_test.cpp"
