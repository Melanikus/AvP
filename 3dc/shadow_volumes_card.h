// The graphics card's side of the kept shadow prisms. Written 2026-11-04 by Melanikus for the AvP port.
// See shadow_volumes_card.cpp. The bookkeeping is in shadow_volumes.cpp.

#ifndef SHADOW_VOLUMES_CARD_H
#define SHADOW_VOLUMES_CARD_H

#include <d3d9.h>

// One corner of a kept prism on the card. x y z is a place in the world. What that place is depends on ShadowVolumesCard_CornersAreCut.
//
// If corners aren't cut, it's where the corner of the prism is, and that's all the card needs. If they are, it's the corner of the wall, and the shader finishes
// it each frame. Then end says what to do: for a near end, how far past the wall's corner it starts along its ray from the light. For a far end, it's
// kKeptCornerFarMark plus how far along the ray from the light it reaches, with the sign turned over. A near end is never that far below zero: only the
// flat shadows make its number negative at all, and then by a few units.
struct KeptCorner
{
	float x, y, z;
	float end;
};

static const float kKeptCornerFarMark = 1000.0f;   // the same as kFarMark in volumeCut.vsh

// Makes the card's list, and takes it back. The card holds it, so it goes whenever the device resets, and everything kept goes with it.
void ShadowVolumesCard_Create();
void ShadowVolumesCard_Release();

// Whether kept prisms can be used. If not, every prism goes the ordinary way.
bool ShadowVolumesCard_Ready();

// Says where the camera is this frame. view is the twelve numbers that turn a place in the world into the game's view.
void ShadowVolumesCard_SetView(const float *view);

// Says where the prisms about to be drawn are cast from. light holds x y z in the numbers polygons are drawn in.
void ShadowVolumesCard_SetLight(const float *light);

// Does the shader finish each corner, from the same cut numbers as the walls? -keptsnap asks for it.
bool ShadowVolumesCard_CornersAreCut();

// How corners are being treated, in a few words, for the log.
const char *ShadowVolumesCard_CornerWords();

// Writes corners to the list, starting at the place the store gave. corners holds x y z for each, and ends holds its KeptCorner end. false means it wasn't written.
bool ShadowVolumesCard_Write(int first, const float *corners, const float *ends, int count);

// Draws the store's list of runs, and then any corners that aren't kept, with whatever stencil and depth settings are on.
HRESULT ShadowVolumesCard_Draw(LPDIRECT3DDEVICE9 device, D3DCOLOR colour, const KeptCorner *worldCorners, int numWorldCorners);

#endif
