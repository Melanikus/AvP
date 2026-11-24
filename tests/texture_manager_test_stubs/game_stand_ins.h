// What TextureManager.cpp needs from the game's Direct3D side, made small enough for a test. Written 2026-11-23 by Melanikus (Frost) for the AvP port.
//
// The compiler puts this ahead of everything else (/FI in the .args file). d3_func.h is the game's Direct3D side, which needs the DirectX SDK; its guard is
// defined here, so when renderer.h asks for it the compiler skips it, and the one thing TextureManager takes from it, r_Texture (a texture on the card), is a
// pointer to a stand-in instead. Everything else the test needs (the R_ functions the manager calls) is in texture_manager_test.cpp.

#ifndef _included_d3_func_h_
#define _included_d3_func_h_

#include <stddef.h>
#include <windows.h>

struct StandInCardTexture
{
	int number;   // which one it is, made by the test's own R_CreateTexture
};

typedef StandInCardTexture *r_Texture;

// the game's two special textures' numbers, which d3_func.h declares (renderer.h's texID_t, the same typedef)
typedef size_t texID_t;
extern texID_t NO_TEXTURE;
extern texID_t MISSING_TEXTURE;

#endif
