// Where to try putting players around one spot, on a level that has no multiplayer starts.
// Written 2026-11-06 by Melanikus for the AvP port.
//
// The single player levels each have one place to start. When two people play one, the first gets that
// place and the second has to go somewhere near it, not inside the first. This is the list of somewhere
// nears, best first: beside him, then behind, then in front, then further out, then further along
// straight lines, because some levels start in a corridor too thin for anything else.
//
// Each spot is given in steps of kPlayerSpotGap: how many to his right (left is minus) and how many in
// front of him (behind is minus). The game then checks each one against the real level, so a spot in this
// list is only a place worth asking about.
//
// Every machine walks this same list in the same order, and takes the place that goes with that player's
// number in the player list. So nobody has to tell anybody where they went.
//
// Nothing here touches the game, so a test can check it.

#ifndef _player_spots_h_
#define _player_spots_h_ 1

enum
{
	// Far enough apart for two bodies: each is 450 mm from middle to edge (CollisionExtents, extents.cpp).
	kPlayerSpotGap = 1500,
	kPlayerSpotCount = 32
};

struct PlayerSpot
{
	int stepsRight;
	int stepsForward;
};

static const PlayerSpot kPlayerSpots[kPlayerSpotCount] =
{
	// right next to him
	{ 1, 0 }, { -1, 0 }, { 1, -1 }, { -1, -1 }, { 0, -1 }, { 1, 1 }, { -1, 1 }, { 0, 1 },

	// the next square out
	{ 2, 0 }, { -2, 0 }, { 0, -2 }, { 0, 2 },
	{ 2, -1 }, { -2, -1 }, { 2, 1 }, { -2, 1 },
	{ 1, -2 }, { -1, -2 }, { 1, 2 }, { -1, 2 },
	{ 2, -2 }, { -2, -2 }, { 2, 2 }, { -2, 2 },

	// further along straight lines, for a corridor
	{ 0, -3 }, { 0, 3 }, { 3, 0 }, { -3, 0 },
	{ 0, -4 }, { 0, 4 }, { 4, 0 }, { -4, 0 },
};

#endif
