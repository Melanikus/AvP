// A sleeping NPC is also woken when a player comes near it, through the walk-in box meant to wake it (#130). Written 2026-11-08 by
// Melanikus for the AvP port.
//
// WHY: a level puts some NPCs to sleep and wakes each one with a walk-in box. That's a space in the level which, when a player walks
// into it, sets off a switch, and the switch wakes the NPC. A player who comes another way, or a box that doesn't fire, leaves them asleep.
// Aliens wake anyway when touched or shot, but a facehugger only wakes when shot, and a dormant predator or a sentry gun never does: in the
// user's co-op game a facehugger in a locker stayed asleep for good. Third read the 63 stock level files on 2026-11-08, with no game running,
// and found 218 NPCs placed asleep (their record has AVPGENFLAG_GENERATORINACTIVE, which object_setup.cpp reads for aliens, praetorians,
// predaliens, predators, facehuggers and sentry guns, and never for a marine): 173
// woken by a walk-in box, 25 by a chain of events that isn't one, and 20 by nothing, meant to wake by touch.
//
// WHAT IT DOES: a walk-in box is also set off when a player comes near enough to one of the sleeping NPCs it wakes. Near enough is nearer
// than the box itself is to that NPC. So a player who comes the way the level means still sets it off at the box, and one who comes another
// way sets it off no farther away than the box would have. Two limits: never farther than kNearWakeMostMillimetres (4 m), so an NPC whose
// box is far off isn't woken from across a room, and never nearer than kNearWakeLeastMillimetres (1.5 m), about touching, so one inside its
// box or at its edge still has a distance to wake at.
//
// SAME ROOM OR IN VIEW (the user's ruling, 2026-11-08): the player also has to be in the sleeper's own module, or a module the game sees
// from it (IsModuleVisibleFromModule, module.cpp). A straight line goes through floors and walls, and a sleeper on the floor above isn't
// "near" someone underneath.
//
// ONLY SAFE JOBS (the user's ruling, 2026-11-08): the box itself is set off, as if walked into, only when everything else it does just
// opens the way to the sleeper: a door or a track asked on, near it, such as the facehugger's locker door. A box that does anything else
// (shuts or locks a door, calls a lift, starts a generator, sets off a self-destruct) would do it early, so then only the sleeper is woken,
// and the box waits for the player as before. NearWake_OnlyOpensTheWay has the rule, and near_wake.cpp sorts each target into one of its
// jobs. Either way it's said in the log:
//
//     near wake: a player came within 3.2 m of a sleeping facehugger at (-4.6, 1.9, 19.5), whose walk-in box is 7.5 m from it, so the box
//          was set off as if walked into (it's set off within 4.0 m)
//     near wake: a player came within 3.2 m of a sleeping alien at (-4.6, 1.9, 19.5), so it was woken by itself: its walk-in box, 7.5 m
//          from it, does other things too, which wait for the box (it's woken within 4.0 m)
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
	void *who = 0;            // the game's own block for it, so it can be woken by itself (near_wake.cpp). This file never reads it.
	void *room = 0;           // the module it's in, or none if the game hasn't said, for the same-room-or-in-view test. Never read here either.
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
// who and room are the game's, kept for near_wake.cpp.
static inline bool NearWakeSleepers_Add(NearWakeSleepers *sleepers, NearWakePoint const &where, int kind, NearWakeBox const &box, void *who = 0,
	void *room = 0)
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
	sleeper.who = who;
	sleeper.room = room;
	return true;
}

// The first sleeper a player here is nearer to than its reach, and that inRoomOrView(sleeper) says the player is in the same room as, or in
// view of; or -1 for none. howFar gets how far the player is from that one. The room test is asked only of a sleeper near enough.
template <typename InRoomOrView>
static inline int NearWakeSleepers_NearOne(NearWakeSleepers const &sleepers, NearWakePoint const &player, double *howFar, InRoomOrView inRoomOrView)
{
	for (int i = 0; i < sleepers.count; i++)
	{
		double const distance = NearWake_Distance(player, sleepers.list[i].where);

		if (distance < sleepers.list[i].reach && inRoomOrView(sleepers.list[i]))
		{
			*howFar = distance;
			return i;
		}
	}

	return -1;
}

// What one of a box's targets does, besides waking the sleeper a player came near. near_wake.cpp sorts each target into one of these.
// Third's count of the walk-in boxes in the same 63 files, read the same way, found 139 that wake sleepers: 112 do nothing else, 1 only opens the way
// (Sulaco's locker door, a track 1.4 m from hugger1, asked on), and 26 do something else, such as a self-destruct and an objective (Vaults_M)
// or shutting a door (Invasion_P's quantum alien).
enum NearWakeJob
{
	kNearWakeJobWakesASleeper,     // an NPC asked on: another sleeper, or one that's awake already and does nothing more
	kNearWakeJobOpensTheWay,       // a door or a track asked on (opened, unlocked, started) within kNearWakeMostJobDistance of the sleeper
	kNearWakeJobAnythingElse       // anything asked off (a door shut or locked, a sentry gun switched off), any other kind (a lift, a
	                               // generator, a message, a self-destruct), anything farther off, or a switch beyond kNearWakeMostSwitchesBetween
};

enum
{
	kNearWakeMostJobs = 32         // targets sorted for one box. A box with more counts as doing something else.
};

// How near the sleeper a door or a track has to be for opening it to count as opening the way to it, in mm: the distance Third's table used
static const double kNearWakeMostJobDistance = 4000.0;

// Whether a box whose other targets do these may be set off whole: only when every one of them wakes an NPC or opens the way to the sleeper
// (the user's "only safe jobs"). tooMany says the box had more targets than were sorted, and then it may not.
static inline bool NearWake_OnlyOpensTheWay(int const *jobs, int count, bool tooMany)
{
	if (tooMany)
	{
		return false;
	}

	for (int i = 0; i < count; i++)
	{
		if (jobs[i] != kNearWakeJobWakesASleeper && jobs[i] != kNearWakeJobOpensTheWay)
		{
			return false;
		}
	}

	return true;
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

// The log's line for a sleeper woken by itself, because its box does other things too, as at the top
static inline std::string NearWake_WokeAloneLine(NearWakeSleeper const &sleeper, double playerDistance)
{
	char text[320];

	snprintf(text, sizeof(text), "near wake: a player came within %.1f m of a sleeping %s at (%.1f, %.1f, %.1f), so it was woken by itself: its walk-in box, "
		"%.1f m from it, does other things too, which wait for the box (it's woken within %.1f m)", playerDistance / 1000.0, NearWake_KindName(sleeper.kind),
		sleeper.where.x / 1000.0, sleeper.where.y / 1000.0, sleeper.where.z / 1000.0, sleeper.boxDistance / 1000.0, sleeper.reach / 1000.0);
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
