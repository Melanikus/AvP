// Lists the polygons of the level that's loaded, for the alien ways map (alien_ways_map.h). Written 2026-11-08 by Melanikus for the AvP port
// (#109, #118).
// Modified 2026-11-08 by Melanikus for the AvP port:
//   lists the level's doors, marks their polygons with the door's number, and says each frame which doors won't let an alien through (#118)
//
// The map knows nothing of the game. This is the one place that does: it goes through every module of the level and every object that stands
// still, and makes a plain list of their polygons, in the world's numbers.
//
// WHAT COUNTS:
// - A room's polygons, and a door's: a door is in its open shape in the level, and its frame is a real wall, so it counts the same. A door's
//   polygons carry its number, so that a search can keep off it while it won't let an alien through (AlienWaysLevel_ReadDoors, below).
// - An object that stands still, like a crate, a lamp, a switch or a screen, where it stands, turned the way it's turned. Things picked up don't.
// - Left out: a lift's car, a platform and anything on a track, which won't be where the level has them; and anything the game says nothing
//   collides with.
//
// WHICH WAY A POLYGON FACES comes from the normal stored with its shape, which is what the game's own collisions test (FLOOR_THRESHOLD, in
// dynamics.cpp). Only a polygon with none stored has it worked out from its corners. A polygon drawn from both sides counts as facing up
// whichever way it's stored, because an alien can stand on its top.
//
// A POLYGON'S CORNERS: a list that ends at a negative number, inside the polygon's record, which has room for 5. Nothing past those 5 is ever
// read, and a polygon with no end within them is left out. Each corner has to be one of its shape's points, or the polygon can't be trusted and
// is left out. Each is turned and placed in 64 bits (AlienWays_PlaceCorner), so a level's numbers can't overflow, and a polygon with a corner
// more than 64 km out is left out. The counts say how many of each, for the log.
//
// It runs on the game's thread, at a level's start, because it reads the game's own shapes and objects. It's quick: a few thousand polygons a
// millisecond. The joining up is the slow part, and that's done on a thread of its own (alien_ways_build.h).
#ifndef ALIEN_WAYS_LEVEL_H
#define ALIEN_WAYS_LEVEL_H

#include <vector>

#include "alien_ways_map.h"
#include "alien_ways_guide.h"

// The most polygons listed from one level, before the map's own limits. A level with more is refused here, so the list can't grow without end.
// Twice the map's own most, because some of a level's polygons are left out by the map as scenery.
size_t const kAlienWaysMostListed = 400000;

struct AlienWaysLevelCounts
{
	int modules;              // modules listed
	int objects;              // still objects listed
	int polygons;             // polygons put on the list
	int tooFewCorners;        // left out: fewer than 3 corners
	int tooManyCorners;       // left out: more corners than the map takes
	int badCorner;            // left out: a corner that isn't one of its shape's points
	int farCorner;            // left out: a corner that lands more than kAlienWaysMostCoordinate from the middle, worked out in 64 bits
	int noEnd;                // left out: no negative number ends its corners within its record's 5 places
	int noStoredFacing;       // facing worked out from the corners, because none was stored
	int doors;                // modules listed that are doors
};

// One of the level's doors: which module it is, by its place in the scene's list of modules, and its kind. Its polygons carry its number,
// which is its place in the list of doors plus 1 (AlienWaysPolygon::door).
struct AlienWaysLevelDoor
{
	int module;
	AlienWaysDoorKind kind;
};

// Fills polygons and counts from the level that's loaded. Gives back nothing, or why there's no list, in words, and then the list is empty.
// modules gets one range for each of the level's modules, by the module's own number (its m_index): the polygons of the list that are that
// module's. A module that isn't listed, a lift's say, has an empty range, so no alien in it is ever guided. doors gets the level's doors.
const char *AlienWaysLevel_List(std::vector<AlienWaysPolygon> *polygons, std::vector<AlienWaysRange> *modules, std::vector<AlienWaysLevelDoor> *doors,
	AlienWaysLevelCounts *counts);

// Tells the map, for each door, whether it won't let an alien through now, so a search keeps off it (DOORS, in alien_ways_map.h). Each door
// is read from its own module's block, checked first, and judged by AlienWays_DoorLetsAnAlienThrough. A door that can't be read, because its
// module or block isn't there or isn't a door of its kind any more, is taken as shut: no way is planned through what can't be checked. On the
// game's thread, once a frame, before the searches carry on.
void AlienWaysLevel_ReadDoors(const std::vector<AlienWaysLevelDoor> &doors, AlienWaysMap *map);

#endif
