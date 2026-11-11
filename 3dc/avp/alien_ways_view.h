// The ALIEN WAYS developer view: the way each alien is following, drawn in the level as lines, coloured by what each stretch of it is on.
// Written 2026-11-08 by Melanikus for the AvP port (issue #109, "floor is lava": aliens that keep off the floor). Fifth's alien_ways.h
// gives the ways (AlienWays_WaysToShow); this turns them into lines; d3d_render.cpp draws them.
//
// WHAT IT SHOWS: for each alien following a way, a line from each point of its way to the next, in the colour of what that stretch is
// on: gold for the floor, blue for a wall, purple for a ceiling. They're the colours of the offline study's pictures (their darker
// set, because the game is dark), so a film and those pictures read the same. A small cross marks each point. The stretch the alien is
// on now, and the point it's heading for, are white, and that point's cross is twice the size.
//
// WHO SEES IT: only a developer, with -dev and -alienways (DrawAlienWays, d3d_render.cpp), and never in a network game, where seeing an
// alien's way through a wall would be cheating (KeepDeveloperToolsOutOfNetworkGames, d3d_render.cpp).
//
// This file needs nothing from the rest of the game, so tests/alien_ways_view_test.cpp can try every case.

#ifndef ALIEN_WAYS_VIEW_H
#define ALIEN_WAYS_VIEW_H

#include "alien_ways_map.h"

#include <stddef.h>
#include <stdint.h>
#include <vector>

// One end of a line: where it is, in the card's numbers, and its colour (0xAARRGGBB). The card's numbers are the camera's view, with up
// turned over, the same three sums the game's own polygons go through, so the depth buffer can compare the lines with the level, and a wall
// hides what's behind it.
struct AlienWaysViewEnd
{
	float x, y, z;
	uint32_t colour;
};

// The colours
static const uint32_t kAlienWaysViewFloor = 0xffd9b44au;
static const uint32_t kAlienWaysViewWall = 0xff78a2d0u;
static const uint32_t kAlienWaysViewCeiling = 0xffb48ad0u;
static const uint32_t kAlienWaysViewHere = 0xffffffffu;      // the stretch it's on, and the point it's heading for
static const uint32_t kAlienWaysViewUnknown = 0xff808080u;   // a kind that isn't one of the three, which a real way never has

// Half of one arm of the cross at each point, in millimetres: a 30 cm cross, and 60 cm at the point it's heading for
static const int kAlienWaysViewCrossArm = 150;

// The most line ends one frame draws: 64 aliens, each with a way of up to kAlienWaysMostWayPoints points, each point with one stretch and
// a cross of three lines, and two ends to a line. Past it, nothing more is added that frame, so a frame's list can't grow without end.
static const size_t kAlienWaysViewMostEnds = 64 * kAlienWaysMostWayPoints * 4 * 2;

// The colour for a stretch of this kind
uint32_t AlienWaysView_ColourOf(AlienWaysKind kind);

// Adds one alien's way to ends: a line for each stretch, in its kind's colour, and a cross at each point. heading is the point it's heading
// for: the stretch into that point and the point itself are white. A heading outside the way marks nothing white. Only the first
// kAlienWaysMostWayPoints points are drawn. matrix is the game's ViewMatrix, three rows of four numbers, which take a place in the level
// into the camera's view. Gives back how many ends it added, always an even number: nothing for no points, or once ends holds
// kAlienWaysViewMostEnds.
size_t AlienWaysView_AddWay(const float *matrix, const AlienWaysPoint *points, int count, int heading, std::vector<AlienWaysViewEnd> *ends);

// A colour with its red, green and blue at two fifths, its alpha as it was: a kept way's
uint32_t AlienWaysView_Dimmed(uint32_t colour);

// Adds the last way of an alien that isn't following it now: the same lines and crosses as AlienWaysView_AddWay with no point it's heading
// for, every colour dimmed (AlienWaysView_Dimmed), so the line stays on screen but plainly isn't being followed. Gives back how many ends
// it added, as AlienWaysView_AddWay does. Added 2026-11-11 by Melanikus (Third) for the AvP port (#109; the owner's ask).
size_t AlienWaysView_AddKeptWay(const float *matrix, const AlienWaysPoint *points, int count, std::vector<AlienWaysViewEnd> *ends);

#endif
