// How Hudson feels his way as he walks. Written 2026-11-05 by Melanikus for the AvP port.
//
// WHY THIS IS HERE. The map says where he should go next. It can't say whether he can really take the next few steps that way: it thinks of him as a point, a stride apart, and he's a box. So he can press against a
// corner for as long as anyone lets him, with the map quite sure there's a way. This looks at the next metre, in the shape he really is, a few times a second, the way a person does without thinking about it.
//
// THE SHAPE OF HIM. His box is a square that never turns (DYN_TYPE_NRBB_COLLISIONS in dynamics.cpp). So going along x or z he's 450 either side of his middle, and going on a slant he's about 636. That's what the rays use.
//
// WHAT IT FEELS FOR, going one way, from five places across the front of his box:
//   - Something at his head, as far as he feels. Stairs don't get that high in a metre, so this sees walls at their true distance.
//   - Something at his knee, a foot at a time, from the height he's got to, so that stairs aren't taken for a wall. Something narrower than the gap between those five places can be missed, and the stuck rule is for that.
//   - The floor, straight down, under the front of his box, because that's what the game lifts him onto. A rise of more than a step stops him, and so does anything too steep. So does a drop of more than a step: going down a drop
//     is a decision for the map, and nothing he should wander into.
//
// WHICH WAY HE GOES. The way he's been asked, if that's clear. If not, the nearest way that is, a little further round each time. If none of them is clear, the one that goes furthest, as long as that's far enough to be
// worth it. And if there's a wall close on one side and room on the other, he turns a little towards the room, because that's how anyone walks down a corridor.
//
// NOTHING HERE KNOWS ABOUT THE GAME. It only asks the rays, as the map does, so a test can give it a made-up world.

#ifndef HUDSON_NAV_FEEL_H
#define HUDSON_NAV_FEEL_H

#include "hudson_nav_graph.h"

int const kHudsonNavFeelAhead = 1000;       // how far ahead he feels. About two strides, or a second's walk
int const kHudsonNavFeelEnough = 300;       // a way that's clear for less than this isn't one to set off along
int const kHudsonNavFeelSides = 2000;       // how far to each side he looks for walls
int const kHudsonNavNearAWall = 1200;       // a wall nearer than this, from his middle, is worth turning away from
int const kHudsonNavMostTurnFromAWall = 25; // the most he turns away from one, in degrees
int const kHudsonNavMostFeelRays = 300;     // the most one choice can use: nine ways, at 29 rays each, two to look to the sides, and one more way for turning from a wall

// How much of the next metre he could walk this way, in millimetres. wayX and wayZ are the way he's going, one long.
int HudsonNav_FreeAhead(HudsonNavRays &rays, int x, int y, int z, double wayX, double wayZ);

struct HudsonNavFeel
{
	bool canGo;       // false if there's no way worth setting off along. The way in x and z is then the one he asked for
	double x, z;      // which way to go, one long
	int freeAhead;    // how far that way is clear
	int turned;       // how many degrees that is from the way he asked for, not counting turning away from a wall
};

// Chooses which way to go, given where he'd like to go. wantX and wantZ are one long.
HudsonNavFeel HudsonNav_FeelTheWay(HudsonNavRays &rays, int x, int y, int z, double wantX, double wantZ);

// The way with the most room, of the eight ways round, for getting out of somewhere he's stuck. Where there's the same room several ways, it's the one most to the side of wantX and wantZ, which is the way that didn't work.
HudsonNavFeel HudsonNav_MostRoom(HudsonNavRays &rays, int x, int y, int z, double wantX, double wantZ);

#endif
