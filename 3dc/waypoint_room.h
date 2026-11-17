// The room an alien has to aim for inside a waypoint, on one axis. Written 2026-11-14 by Melanikus for the AvP port.
//
// A waypoint is a box in a module that the aliens find their way through (WAYPOINT chunk, chunk_waypoint.cpp). When an alien is sent to
// a big one, it aims for a random place inside it (GetTargetPositionInWaypoint, bh_waypoint.cpp). On each axis that's the room's lowest
// place, plus a random number cut down to fit the room: what's left over after dividing it by the room. Dividing needs a room of 1 or more.
//
// The room comes from the box's three corners in the level file, its lowest, its highest and its centre, in two steps:
// - the level setup (copy_rif_data, projload.cpp) scales each one by the level's scale and keeps the whole number (ChunkVectorInt's
//   operator*, chunk_type.cpp), then keeps the lowest and the highest as how far they are from the centre, and takes 1 off the highest;
// - GetTargetPositionInWaypoint takes three quarters of each, as (x * 3) >> 2, and the room is the highest's three quarters take away the
//   lowest's. It adds the centre to both first, which makes no difference to what one takes away from the other.
// The game's sums are on 32-bit whole numbers, which wrap round past the largest one, and these sums do the same, so they come out the same.
//
// Three of the game's own level files have a waypoint with a room of -1 on an axis: a box with no size on that axis, exactly at its centre.
// Their aliens aim for the room's lowest place there, as what's left over after dividing by -1 is always 0. None has a room of 0.
//
// So:
// - WaypointRoom works the room out from the file's numbers and the level's scale, so the level reader refuses a box with a room of 0, or
//   one it can't work out (Module_Waypoint_Chunk::post_input_processing, chunk_waypoint.cpp);
// - WaypointPlaceInRoom is the place GetTargetPositionInWaypoint takes, and it never divides by a room under 1: it gives 0, the lowest
//   place, which is what a room of -1 already gives.
//
// This is plain sums, with nothing of the game's in it, so waypoint_room_test checks it on its own, against a copy of the game's own lines.

#ifndef WAYPOINT_ROOM_H
#define WAYPOINT_ROOM_H

#include <stdint.h>
#include <string.h>

// Is this a number the sums can use: not infinity, and not "not a number"? Asked of the number's bits, its 11 exponent bits all being 1,
// because the game is built with fast floating point (/fp:fast), and that lets the compiler take a comparison with such a number any way
// it likes.
inline bool WaypointNumberIsFinite(double number)
{
	uint64_t bits;
	memcpy(&bits, &number, sizeof(bits));

	return ((bits >> 52) & 0x7FF) != 0x7FF;
}

// One corner's number at the level's scale, as the level setup keeps it: the number times the scale, with what's after the point cut off.
// False, with nothing kept, if the answer isn't a whole number the game can hold, 32 bits with a sign. No level the game allows comes near.
inline bool WaypointScaledCorner(int corner, double scale, int32_t *scaled)
{
	double const exact = corner * scale;

	if (!WaypointNumberIsFinite(exact) || !(exact > -2147483649.0 && exact < 2147483648.0))
	{
		return false;
	}

	*scaled = (int32_t)exact;
	return true;
}

// The room a waypoint gives on one axis, from that axis's three numbers in the file and the level's scale, worked out as the game works it
// out (see the top). False if the scale or a corner isn't a number the sums can use.
inline bool WaypointRoom(int lowest, int highest, int centre, double scale, int32_t *room)
{
	int32_t low;
	int32_t high;
	int32_t middle;

	if (!WaypointNumberIsFinite(scale) || !WaypointScaledCorner(lowest, scale, &low) || !WaypointScaledCorner(highest, scale, &high)
		|| !WaypointScaledCorner(centre, scale, &middle))
	{
		return false;
	}

	// How far each is from the centre, and 1 off the highest. Unsigned numbers wrap round past the largest, as the game's sums do.
	uint32_t const lowFromCentre = (uint32_t)low - (uint32_t)middle;
	uint32_t const highFromCentre = (uint32_t)high - (uint32_t)middle - 1u;

	// Three quarters of each: times 3, wrapping round, then back to a number with a sign, and shifted right by 2, which keeps the sign as
	// the game's shift does
	int32_t const lowQuarters = (int32_t)(lowFromCentre * 3u) >> 2;
	int32_t const highQuarters = (int32_t)(highFromCentre * 3u) >> 2;

	*room = (int32_t)((uint32_t)highQuarters - (uint32_t)lowQuarters);
	return true;
}

// Where in a room an alien aims, counted from its lowest place, from a random number of 0 or more (FastRandom): what's left over after
// dividing the random number by the room, from 0 up to one less than the room. A room under 1 has no other place to give, so it gives 0, the
// lowest place, and never divides.
inline int WaypointPlaceInRoom(int random, int room)
{
	return room >= 1 ? random % room : 0;
}

#endif
