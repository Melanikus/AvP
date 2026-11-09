// Hands the real level's polygons to the grid. Written 2026-11-05 by Melanikus for the AvP port.
#ifndef HUDSON_GRID_LEVEL_H
#define HUDSON_GRID_LEVEL_H

#include "hudson_grid.h"

#include <vector>

// It only ever happens in a Hudson run that asks for map=grid, so a normal game never pays for it. There are two halves, because only the first can touch the game.

// Lists the level's polygons, on the game's own thread, because the list of objects is the game's and it can change. It's quick. It gives back why not, in words, or nothing.
const char *HudsonGridLevel_ListPolygons(std::vector<HudsonGridPolygon> *polygons);

// Builds the grid from that list, and writes what it found to the log. It touches nothing of the game's, so it can run on a thread of its own while the game goes on. This is the slow half.
const char *HudsonGridLevel_Build(HudsonGrid *grid, const std::vector<HudsonGridPolygon> &polygons);

#endif
