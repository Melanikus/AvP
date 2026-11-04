// The graphics card's side of the kept shadow prisms. Written 2026-11-04 by Melanikus for the AvP port.
// See shadow_volumes_card.cpp. The bookkeeping is in shadow_volumes.cpp.

#ifndef SHADOW_VOLUMES_CARD_H
#define SHADOW_VOLUMES_CARD_H

#include <d3d9.h>

// Makes the card's list, and takes it back. The card holds it, so it goes whenever the device resets, and everything kept goes with it.
void ShadowVolumesCard_Create();
void ShadowVolumesCard_Release();

// Whether kept prisms can be used. If not, every prism goes the ordinary way.
bool ShadowVolumesCard_Ready();

// Says where the camera is this frame. view is the twelve numbers that turn a place in the world into the game's view.
void ShadowVolumesCard_SetView(const float *view);

// Writes corners to the list, starting at the place the store gave. Each corner is x y z. false means it wasn't written.
bool ShadowVolumesCard_Write(int first, const float *corners, int count);

// Draws the store's list of runs, with whatever stencil and depth settings are on.
HRESULT ShadowVolumesCard_Draw(LPDIRECT3DDEVICE9 device, D3DCOLOR colour);

#endif
