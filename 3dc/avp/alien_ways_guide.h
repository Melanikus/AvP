// The guide: which aliens are following a way across the alien ways map (alien_ways_map.h), what each one's way is, and the one search that's
// running for them. Written 2026-11-08 by Melanikus for the AvP port ("For Aliens, Floor is lava", #109 and #118).
//
// It knows nothing of the game, so a test can drive it with made-up aliens. alien_ways.cpp is the game's side: it tells the guide where each
// alien is and where it's going, and does what the guide says.
//
// ONE FRAME:
//
//      AlienWays_Frame, before any alien moves
//        Frame: every alien that was steered last frame and not since is let go, and named, so the game gives it its usual gravity back.
//        |      Then the searches get up to kAlienWaysLooksPerFrame polygons between them, first asked, first served.
//      each alien that's coming for its target (bh_near.cpp)
//        Steer: says where to head for now, or that it has no way yet, and then the game's own code steers it as it always has.
//               It asks for a search when there's no way, or the way's 2 s old, or the goal has moved 3 m, or the alien is 3 m off the way.
//
// AN ALIEN STOPS BEING GUIDED (and gets its usual gravity back) when:
// - Steer says no. It's still coming for its target, so the game's own code sets its gravity that same frame, as it always has.
// - Nothing asks Steer for it in a frame: it's attacking, waiting, hunting, dying, gone far, or gone. The frame after that one, Frame names it.
//   So it moves for two frames with the gravity the way gave it, about 33 ms. Most of those states set their own gravity as they start
//   anyway; the two that don't, avoiding something and taunting, are the ones this is for.
// - The level ends: Clear, and the map and every alien go with it.
//
// A WAY THAT'S BEING FOLLOWED IS NEVER DROPPED BY A SEARCH THAT FAILS: only a new way replaces it. So an alien on a ceiling doesn't let go
// because its way's renewal found nothing.
//
// ALL ON THE GAME'S THREAD. Nothing here asks for memory after it's made: every way is kept in room set aside for it.
#ifndef ALIEN_WAYS_GUIDE_H
#define ALIEN_WAYS_GUIDE_H

#include <stddef.h>
#include <stdint.h>

#include "alien_ways_map.h"

// Who an alien is, in two numbers the game picks. The game uses where the alien is kept and its name, so a new alien kept where a dead one was
// is never taken for it.
struct AlienWaysWho
{
	uint64_t place;
	uint64_t name;
};

// Some of the polygons handed to the map's Build, from first up to end: one module's, say. See AlienWaysMap::NearestPolygon.
struct AlienWaysRange
{
	size_t first, end;
};

// The rules. Each is a first guess, to be tuned from the log's lines and from film (AvP-docs, engine/floor-is-lava-plan.md).
int const kAlienWaysFloorPrice = 10;              // the floor costs 10 times as much as a wall or a ceiling. Step 2c's knee.
size_t const kAlienWaysMostFollowers = 64;        // aliens with a way at once. One more is steered by the game's own code.
int const kAlienWaysLooksPerFrame = 500;          // polygons all the searches may look at in one frame, between them
int const kAlienWaysNearEnough = 2000;            // mm: how far an alien, or its goal, can be from a polygon and still start on it. An alien
                                                  // starts on the nearest polygon of the kind it's standing on, by its down, if there's one that near.
int const kAlienWaysReached = 600;                // mm across the surface the alien is on: a point that near has been reached...
int const kAlienWaysBodyHeight = 1500;            // ...as long as it's no more than this far above or below the alien's own middle
int const kAlienWaysOffTheWay = 3000;             // mm: an alien this far from its way has lost it, and asks again
int const kAlienWaysAskAgainAfter = 2000;         // ms: a way this old is searched for again, and followed till the new one comes
int const kAlienWaysGoalMoved = 3000;             // mm: a goal that's moved this far since the search is searched for again
int const kAlienWaysHeadway = 100;                // mm: getting this much nearer to the next point counts as headway...
int const kAlienWaysNoHeadwayFor = 1500;          // ...and with none for this many ms, the alien is stuck: its way is dropped...
int const kAlienWaysRestAfterStuck = 3000;        // ...and the game's own code steers it for this many ms before it asks again

// The kinds of door the game has (ModuleIsADoor, in pfarlocs.cpp), and whether one lets an alien through now. It's the rule the game's own
// routes for aliens follow (AIModuleAdmitsPheromones, in pheromone.cpp), which is why a hunting alien never plans through a locked door:
// - a proximity door opens by itself for an alien that comes near it, so it lets one through unless it's locked, open yet or not;
// - a lift's door, and a door worked by a switch, only while they're open: an alien can't open either. open is as the game says it
//   (GetState, in bh_types.cpp): for a lift's door, anything but shut; for a switch's door, only all the way open.
// A search keeps off a door that doesn't (DOORS, in alien_ways_map.h).
enum AlienWaysDoorKind
{
	kAlienWaysProximityDoor,
	kAlienWaysLiftDoor,
	kAlienWaysSwitchDoor
};

bool AlienWays_DoorLetsAnAlienThrough(AlienWaysDoorKind kind, bool locked, bool open);

// What an alien tells Steer, every frame it's coming for its target
struct AlienWaysAsk
{
	AlienWaysWho who;
	AlienWaysXYZ at;                // where it is
	AlienWaysXYZ down;              // which way is down for it, as the game keeps it: a length of 65536 (its OrientMat's second row)
	AlienWaysRange atPolygons;      // where to look for the polygon it's on: the module it's in
	AlienWaysXYZ goal;              // where it's going
	AlienWaysRange goalPolygons[2]; // where to look for the polygon the goal's on: the alien's module and the target's. The nearer wins.
	bool goalIsTheTarget;           // the goal is the target itself, not a point on the way to it: only so the log can say which
};

// How one search went, for the log
enum AlienWaysOutcome
{
	kAlienWaysOutcomeFound,
	kAlienWaysOutcomeNoWay,
	kAlienWaysOutcomeAllFloor,      // a way, but all of it on the floor: the game's own code does that as well, and climbs when it can
	kAlienWaysOutcomeNoPolygon,     // the alien, or its goal, wasn't near enough to any polygon to start on
	kAlienWaysOutcomeRefused,       // the map wouldn't start the search: why says why
	kAlienWaysOutcomeStuck          // not a search: an alien made no headway, so its way was dropped (from: where it was, to: where it was heading)
};

struct AlienWaysReport
{
	AlienWaysOutcome outcome;
	const char *why;                // for kAlienWaysOutcomeRefused
	size_t follower;                // which of the guide's places the alien has, so lines about one alien can be told apart
	AlienWaysWho who;               // the alien itself, so the game can say more about it
	AlienWaysXYZ from, to;
	int frames;                     // how many frames the search was worked on
	long long waitedMs;             // from the ask to the answer, in the game's time
	long long microseconds;         // the computer's time the search itself took, in all
	int looks;                      // polygons looked at
	int points;
	bool cutShort;                  // the way was longer than the map gives, and stops short of the goal
	bool toTheTarget;               // the goal was the target itself (AlienWaysAsk::goalIsTheTarget)
	int fromKind;                   // the kind of polygon the search started on (an AlienWaysKind), or -1 if none was near
	double floorMm, wallMm, ceilingMm;
};

class AlienWaysGuide
{
public:
	AlienWaysGuide();

	// Forgets every alien, every way and the search. At a level's start and end.
	void Clear();

	// Once a frame, before any alien moves, with how long the last frame took. Every alien that was guided last frame and wasn't steered since
	// is let go and written into lapsed, up to mostLapsed of them, so the game can give each its usual gravity back. It gives back how many.
	// Then the searches carry on, looking at no more than looksAllowed polygons. With no map ready, there are no searches.
	size_t Frame(AlienWaysMap *map, int stepMs, int looksAllowed, AlienWaysWho *lapsed, size_t mostLapsed);

	// An alien asks how to go. It gives back true, and the point to head for in towards, when the alien has a way to follow now. False means
	// the game's own code steers it this frame; it may have asked for a search, which takes a frame or a few.
	bool Steer(const AlienWaysMap *map, const AlienWaysAsk &ask, AlienWaysXYZ *towards);

	// Reports from Steer, of aliens found stuck since the last Frame, come out with the next Frame's reports.

	// Whether the last Steer for this alien gave back true
	bool Guided(const AlienWaysWho &who) const;

	// Whether Steer was asked for this alien since the last Frame, and gave back true: its way steers it in this frame. Unlike Guided, it's
	// false in a frame nothing asked Steer for it, even before the next Frame lets it go.
	bool GuidedThisFrame(const AlienWaysWho &who) const;

	// What every guided alien's way is now, for the developer view: up to most of them. The points are good until the next Frame or Clear.
	size_t Shown(const AlienWaysPoint **points, int *counts, int *headings, size_t most) const;

	// How the searches that ended in the last Frame went
	size_t Reports(const AlienWaysReport **reports) const { *reports = report; return reportCount; }

	// How many times an alien was turned away because all kAlienWaysMostFollowers places were taken, since the last Clear
	size_t TurnedAway() const { return turnedAway; }

private:
	struct Follower
	{
		bool used;
		AlienWaysWho who;
		bool steered;                       // Steer was asked for it since the last Frame
		bool guided;                        // the last Steer gave back true
		bool queued;                        // it wants a search
		long long askedAt;                  // when it last asked for one
		long long restUntil;                // no way, and no asking, till then
		AlienWaysAsk last;                  // what it last told Steer: a search starts from this

		int count, heading;                 // the way: count points, heading for point heading
		bool fresh;                         // the way has just come, and the alien hasn't taken it up yet
		long long wayAt;                    // when the way came
		AlienWaysXYZ wayGoal;               // the goal it was searched for
		double nearest;                     // the nearest the alien has been to the point it's heading for
		long long headwayAt;                // when it last got kAlienWaysHeadway nearer
		AlienWaysPoint points[kAlienWaysMostWayPoints];
	};

	int Find(const AlienWaysWho &who) const;
	int Take(const AlienWaysWho &who);
	void DropTheWay(Follower *follower);
	void StartNext(AlienWaysMap *map);
	void Finish(AlienWaysMap *map, AlienWaysSearch result);
	void NoNewWay(Follower *follower, AlienWaysXYZ goal);
	void AddReport(const AlienWaysReport &line);

	long long now;
	Follower followers[kAlienWaysMostFollowers];
	size_t turnedAway;

	// The one search that's running, and for which alien, or -1
	int searching;
	AlienWaysXYZ searchFrom, searchGoal;
	bool searchToTheTarget;
	int searchFromKind;
	long long searchAskedAt, searchMicroseconds;
	int searchFrames;

	AlienWaysReport report[kAlienWaysMostFollowers];
	size_t reportCount;
	bool reportsRead;                   // the reports have been read by a Frame, so the next report starts a new list
};

#endif
