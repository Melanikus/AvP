// The test of clipping again, with the switch USE_FRUSTUM_CULL turned on. Written 2026-11-04 by Melanikus for the AvP port.
//
// The game doesn't build with this switch on today. If it ever did, every polygon crossing the edge of the view would be clipped, including polygons from a level file, which anyone can
// make. So this checks the limit holds that way too. The checks, and what they test, are in clip_checks.h; clip_test.cpp runs them without the switch.
//
// The switch has to be set before the game's frustum.cpp is read, which clip_checks.h does, so it comes first here.

#define USE_FRUSTUM_CULL
#include "clip_checks.h"

int main()
{
	return RunTheClipChecks();
}
