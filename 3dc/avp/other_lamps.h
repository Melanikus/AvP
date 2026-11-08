// Other marines' shoulder lamps: whose are on, which few are shown, where each one is and which way it points. Written 2026-11-07 by Melanikus for the AvP port.
//
// A marine's lamp, his own or another player's, can be on: see the lampOn in MARINE_STATUS_BLOCK and NETGHOSTDATABLOCK. Once a frame, the lamps
// that are on are gathered from the game's own list of what's there, and only the nearest few are shown (lamp_pick.h). Each fades in and out, so
// one switched every message can't flash. Nothing here lights or draws anything yet: the drawing comes later, and reads these.
//
// Only in single player and in the network mode that plays as single player, the user's ruling: in any other network game, none are gathered.

#pragma once

#include "3dc.h"

struct OtherLamp
{
	unsigned int owner;   // whose it is, the same from frame to frame
	VECTORCH place;       // where the lamp is, in the world
	VECTORCH aim;         // the way it points, 65536 long
	float strength;       // from 0, off, to 1, fully on, as it fades
	const DISPLAYBLOCK *body;   // what carries it. Only for this frame: the next one gathers them all again
};

// Gathers this frame's lamps. seconds is how long the last frame took, for the fading.
void OtherLamps_Gather(float seconds);

// How many lamps have any strength this frame, and each of them
int OtherLamps_Count();
OtherLamp const *OtherLamps_At(int which);
