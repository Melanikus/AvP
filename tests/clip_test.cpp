// The test of cutting a polygon against the edges of the view, as the game builds today. Written 2026-11-04 by Melanikus
// for the AvP port.
//
// The checks themselves, and what they test, are in clip_checks.h, which clip_on_test.cpp runs too with the switch
// USE_FRUSTUM_CULL turned on. This file runs them without it.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this file on its own, with the settings in tests/clip_test.args. The program gives back a
// non-zero result if anything failed.

#include "clip_checks.h"

int main()
{
	return RunTheClipChecks();
}
