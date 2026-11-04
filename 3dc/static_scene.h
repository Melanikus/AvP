// The level's walls, drawn from the fixed buffer in place of working them out each frame. Written 2026-11-04 by Melanikus for the AvP port.
// See static_scene.cpp for how it works.

#ifndef STATIC_SCENE_H
#define STATIC_SCENE_H

#include <d3d9.h>

#include "static_level.h"

// A new level has its lists on the card: forget anything that went wrong with the last one.
void StaticScene_StartLevel();

// Decides a frame, once the game knows what's visible and before anything is drawn. Call StartChoosing, then OfferModule for each module about to be drawn,
// then FinishChoosing. A module the frame takes skips the whole of the game's shape drawing. Anything in doubt stays on the old route.
void StaticScene_StartChoosing();
void StaticScene_OfferModule(const struct displayblock *block);
void StaticScene_FinishChoosing();

// Whether this frame took this module.
bool StaticScene_TookModule(int moduleNumber);

// Whether this frame took any at all. If it did, the surface pictures have to draw from the fixed buffer: there's nothing else to draw those walls from.
bool StaticScene_TookAny();

// Whether the walls still have to be drawn this frame. They go once, after the solid part of the game's list and before anything that needs them behind it.
bool StaticScene_WallsAreDue();
void StaticScene_DrawWalls(LPDIRECT3DDEVICE9 device);

// Draws these runs for the comparison, with its own effect. It runs inside the shadow pass, which sets the card up by hand. false means the card refused.
bool StaticScene_DrawForComparing(LPDIRECT3DDEVICE9 device, const StaticRange *ranges, int count);

#endif
