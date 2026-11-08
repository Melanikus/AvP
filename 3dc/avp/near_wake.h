// A sleeping NPC is also woken when a player comes near it, through the walk-in box meant to wake it (#130). Written 2026-11-08 by
// Melanikus for the AvP port.
//
// WHY: a level puts some NPCs to sleep and wakes each one with a walk-in box. That's a space in the level which, when a player walks
// into it, sets off a switch, and the switch wakes the NPC. A player who comes another way, or a box that doesn't fire, leaves them asleep.
// Aliens wake anyway when touched or shot, but a facehugger only wakes when shot, and a dormant predator or a sentry gun never does: in the
// user's co-op game a facehugger in a locker stayed asleep for good. Third's survey of the 63 stock levels (AvP-docs, npc-asleep-report.md)
// found 218 NPCs placed asleep: 173 woken by a walk-in box, 25 by a chain of events that isn't one, and 20 by nothing, meant to wake by touch.
//
// WHAT IT DOES: a walk-in box is also set off when a player comes near enough to one of the sleeping NPCs it wakes. Near enough is nearer
// than the box itself is to that NPC. So a player who comes the way the level means still sets it off at the box, and one who comes another
// way sets it off no farther away than the box would have. Two limits: never farther than kNearWakeMostMillimetres (4 m), so an NPC whose
// box is far off isn't woken from across a room, and never nearer than kNearWakeLeastMillimetres (1.5 m), about touching, so one inside its
// box or at its edge still has a distance to wake at. It's the box itself that's set off, as if walked into, so whatever else it opens or
// moves (the facehugger's locker door, for one) goes with it, and it's said in the log:
//
//     near wake: a player came within 3.2 m of a sleeping facehugger at (-4.6, 1.9, 19.5), whose walk-in box is 7.5 m from it, so the box
//          was set off as if walked into (it's set off within 4.0 m)
//
// WHAT IT LEAVES ALONE: an NPC woken some other way (a button, a timer, another NPC's death), and one nothing wakes, because setting
// something off early would spoil what the level waits for. Only the NPCs a box wakes are looked at: its own targets, or the targets of a
// switch it sets off (kNearWakeMostSwitchesBetween, as in every stock level). An NPC that's awake already, or gone, isn't looked at, and
// nor is a xenoborg, which wakes by itself. Only a machine that runs the NPCs does this: single player, or a network game's host. Every
// player counts there, the host's own and the others, as they do for the box. A joiner's switches follow the host's, as they do for a box
// walked into (the strategy synch in pldnet.cpp).
//
// The numbers are the user's call (2026-11-08: "if it's sensible"). This file knows nothing about the game, so tests/near_wake_test.cpp
// can try all of it. near_wake.cpp finds the sleepers in the game, and the two switches' behaviours look for the players.

#ifndef NEAR_WAKE_H
#define NEAR_WAKE_H

#include <math.h>
#include <stdio.h>

#include <string>

// How near a player has to come, at most and at least, in mm
static const double kNearWakeMostMillimetres = 4000.0;
static const double kNearWakeLeastMillimetres = 1500.0;

enum
{
	kNearWakeMostSleepers = 32,         // looked at for one box. A box that wakes more looks at the first ones, and the log says so.
	kNearWakeMostSwitchesBetween = 1,   // switches between a box and an NPC it wakes
	kNearWakeMostLines = 100            // log lines a level gets about boxes set off this way
};

// The kinds that can be placed asleep and woken by a box
enum NearWakeKind
{
	kNearWakeAlien,        // aliens, praetorians and predaliens
	kNearWakeFacehugger,
	kNearWakePredator,     // a dormant one
	kNearWakeSentryGun,
	kNearWakeKinds
};

static const char *const kNearWakeKindNames[] = { "alien", "facehugger", "predator", "sentry gun" };

static inline const char *NearWake_KindName(int kind)
{
	return kind >= 0 && kind < kNearWakeKinds ? kNearWakeKindNames[kind] : "NPC";
}

// A place in the level, in mm
struct NearWakePoint
{
	double x = 0, y = 0, z = 0;
};

static inline NearWakePoint NearWake_Point(double x, double y, double z)
{
	NearWakePoint point;

	point.x = x;
	point.y = y;
	point.z = z;
	return point;
}

// A walk-in box: everything from least to most on each axis
struct NearWakeBox
{
	NearWakePoint least, most;
};

static inline double NearWake_Distance(NearWakePoint const &a, NearWakePoint const &b)
{
	double const dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;

	return sqrt(dx * dx + dy * dy + dz * dz);
}

// How far a point is from the box's nearest point, along one axis: nothing if it's between the two ends
static inline double NearWake_AxisOutside(double at, double least, double most)
{
	double const below = least - at, above = at - most;
	double const outside = below > above ? below : above;

	return outside > 0 ? outside : 0;
}

// How far a point is from the box's nearest point: nothing inside it
static inline double NearWake_DistanceToBox(NearWakePoint const &point, NearWakeBox const &box)
{
	double const dx = NearWake_AxisOutside(point.x, box.least.x, box.most.x);
	double const dy = NearWake_AxisOutside(point.y, box.least.y, box.most.y);
	double const dz = NearWake_AxisOutside(point.z, box.least.z, box.most.z);

	return sqrt(dx * dx + dy * dy + dz * dz);
}

// How near a player has to come to an NPC whose box is this far from it
static inline double NearWake_Reach(double boxDistance)
{
	if (boxDistance < kNearWakeLeastMillimetres)
	{
		return kNearWakeLeastMillimetres;
	}

	return boxDistance > kNearWakeMostMillimetres ? kNearWakeMostMillimetres : boxDistance;
}

// One sleeping NPC a box wakes
struct NearWakeSleeper
{
	NearWakePoint where;
	int kind = kNearWakeAlien;
	double boxDistance = 0;   // from it to its box's nearest point
	double reach = 0;         // how near a player has to come
};

// The sleeping NPCs one box wakes
struct NearWakeSleepers
{
	int count = 0;
	bool full = false;        // there were more than it holds
	NearWakeSleeper list[kNearWakeMostSleepers];
};

static inline void NearWakeSleepers_Clear(NearWakeSleepers *sleepers)
{
	sleepers->count = 0;
	sleepers->full = false;
}

// Adds one, with how near a player has to come to it, worked out from where its box is. Gives back false when there's no room for it.
static inline bool NearWakeSleepers_Add(NearWakeSleepers *sleepers, NearWakePoint const &where, int kind, NearWakeBox const &box)
{
	if (sleepers->count >= kNearWakeMostSleepers)
	{
		sleepers->full = true;
		return false;
	}

	NearWakeSleeper &sleeper = sleepers->list[sleepers->count++];

	sleeper.where = where;
	sleeper.kind = kind;
	sleeper.boxDistance = NearWake_DistanceToBox(where, box);
	sleeper.reach = NearWake_Reach(sleeper.boxDistance);
	return true;
}

// The first sleeper a player here is nearer to than its reach, or -1 for none. howFar gets how far the player is from that one.
static inline int NearWakeSleepers_NearOne(NearWakeSleepers const &sleepers, NearWakePoint const &player, double *howFar)
{
	for (int i = 0; i < sleepers.count; i++)
	{
		double const distance = NearWake_Distance(player, sleepers.list[i].where);

		if (distance < sleepers.list[i].reach)
		{
			*howFar = distance;
			return i;
		}
	}

	return -1;
}

// The log's line for a box set off this way, as at the top
static inline std::string NearWake_Line(NearWakeSleeper const &sleeper, double playerDistance)
{
	char text[320];

	snprintf(text, sizeof(text), "near wake: a player came within %.1f m of a sleeping %s at (%.1f, %.1f, %.1f), whose walk-in box is %.1f m from it, so the box "
		"was set off as if walked into (it's set off within %.1f m)", playerDistance / 1000.0, NearWake_KindName(sleeper.kind), sleeper.where.x / 1000.0,
		sleeper.where.y / 1000.0, sleeper.where.z / 1000.0, sleeper.boxDistance / 1000.0, sleeper.reach / 1000.0);
	return text;
}

// The line once a level has had kNearWakeMostLines of them
static inline std::string NearWake_LinesStop()
{
	char text[128];

	snprintf(text, sizeof(text), "near wake: %d walk-in boxes set off this way in this level, so no more lines about that until the next one", kNearWakeMostLines);
	return text;
}

// The line, once a level, for a box that wakes more sleeping NPCs than are looked at
static inline std::string NearWake_FullLine()
{
	char text[160];

	snprintf(text, sizeof(text), "near wake: a walk-in box wakes more than %d sleeping NPCs, so only the first %d are looked at", kNearWakeMostSleepers,
		kNearWakeMostSleepers);
	return text;
}

#endif
