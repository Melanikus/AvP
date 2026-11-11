// See alien_ways.h. Written 2026-11-08 by Melanikus for the AvP port.
// Modified 2026-11-08 by Melanikus for the AvP port:
//   with -alienwaysgrid, the build makes the grid (npc_grid.h) after the map, the guide searches it, and the log says how both went (#109, #145)
// Modified 2026-11-08 by Melanikus for the AvP port:
//   an alien found stuck has its state, its last frames' states, its speed, gravity and body's down, its obstruction count, its module and
//   the game's own goal for it in the log, and an alien let go has its state (alien_ways_stuck.h). One function finds a live alien by who it is.
// Modified 2026-11-08 by Melanikus for the AvP port:
//   the map is told each frame which of the level's doors won't let an alien through, so a way never goes through a locked door (#118)
// Modified 2026-11-08 by Melanikus for the AvP port:
//   an alien the guide keeps through a frame that didn't ask for it is let go at once if it's in a state that never asks, with its gravity
//   back, and otherwise counted by state for a once-a-second line (#109)
// Modified 2026-11-08 by Melanikus for the AvP port:
//   an alien its state didn't ask about this frame is kept going along its way (AlienWays_KeepDriving), and one alien can be watched every
//   frame (-alienwayswatch) (#109)
#include "3dc.h"
#include "module.h"
#include "strategy_def.h"
#include "gamedef.h"
#include "bh_types.h"
#include "dynblock.h"
#include "logString.h"
#include "pvisible.h"
#include "bh_ais.h"
#include "bh_alien.h"
#include "pheromone.h"
#include "pfarlocs.h"

#include "alien_ways.h"
#include "alien_ways_build.h"
#include "alien_ways_guide.h"
#include "alien_ways_level.h"
#include "alien_ways_stuck.h"
#include "npc_grid.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

extern int NormalFrameTime;

// The game's lists of AI modules and of modules (pfarlocs.cpp), for the mover's scent (StrongestScent)
extern AIMODULE *AIModuleArray;
extern int AIModuleArraySize;
extern int ModuleArraySize;

int AlienWaysSwitchedOn = 0;
int AlienWaysGridSwitchedOn = 0;
int AlienWaysMoverSwitchedOn = 0;
int AlienWaysViewSwitchedOn = 0;
int AlienWaysAimAheadMm = kAlienWaysAimAheadMm;
int AlienWaysWatchPlace = -1;

// The watch's places are the guide's
static_assert(kAlienWaysMostWatchPlace == static_cast<int>(kAlienWaysMostFollowers) - 1, "-alienwayswatch's places are the guide's");

namespace
{
	// The one build, for the level that's loaded. A thing of its own and not made with new, so it's stopped and waited for as the program ends
	// too, even if a level is still loading then.
	AlienWaysBuild theBuild;

	// Which polygons of the map are each module's, by the module's own number (alien_ways_level.h)
	std::vector<AlienWaysRange> theModules;

	// The level's doors, whose state the map is told each frame (AlienWaysLevel_ReadDoors)
	std::vector<AlienWaysLevelDoor> theDoors;

	// Every alien's way. Kept here, not made with new, for the same reason as the build.
	AlienWaysGuide theGuide;

	// Whether the log has been told how the build went, and that aliens were turned away for want of room
	bool told = true;
	bool toldTurnedAway = false;

	// The aliens that have asked for a way, with their last frames' states and the goal the game's own code last picked for them, for the
	// line when one is found stuck (alien_ways_stuck.h). As many as the guide can guide at once. When it's full, the one that asked longest
	// ago makes room. Kept here, not in the alien's own status block, so a saved game's format doesn't change.
	struct Watched
	{
		bool used = false;
		AlienWaysWho who = {};
		AlienWaysStateHistory history;
		bool hasGoal = false;
		VECTORCH goal = {};
		unsigned int lastAsked = 0;   // the frame it last asked, by frameNumber
		long long lastWouldHaveMs = -1000000;   // when the log last said a test would have sent it off (AlienWays_WouldHaveAvoided)

		// The mover (#149): the last time it saw its target, where, and which target that was
		bool seen = false;
		AlienWaysXYZ seenAt = {};
		long long seenMs = 0;   // in the level's time, clockMs
		AlienWaysWho seenWho = {};
	};

	Watched watched[kAlienWaysMostFollowers];
	unsigned int frameNumber = 0;

	// How many lines this level has had about aliens let go (kAlienWaysMostLetGoLines at most)
	int letGoLines = 0;

	// The game's time this level, in ms, from the frames' own lengths, and how many lines it has had about a test that would have sent an
	// alien off (AlienWays_WouldHaveAvoided)
	long long clockMs = 0;
	int const kAlienWaysMostWouldHaveLines = 200;
	int wouldHaveLines = 0;

	// The last second of this level the log was told where the guided aliens are (lanes, #138)
	long long placesSecond = -1;

	// Frames since the last once-a-second line in which an alien kept on its way wasn't asked about, by the state it was in. A hunting alien
	// that skips asking now and then is steered by the game's own code in those frames, so this says which states make the two take turns.
	int missedFrames = 0;
	int missedByState[kAlienWaysStateKinds] = {};

	// The watch on one alien (-alienwayswatch): how many lines it has written this level, and this frame's notes for it
	int watchLines = 0;
	std::string watchNotes;

	// The mover's goals (#149, MoverGoal), counted for the once-a-second line: what each ask was given to head for, and how many asks had
	// no way and were sent straight for the goal
	enum AlienWaysMoverGoal
	{
		kMoverTarget,      // its target, which it can see
		kMoverLastSeen,    // where it last saw its target
		kMoverScent,       // where the marines' scent is strongest in the level
		kMoverStatesOwn,   // the point the game's own state picked
		kMoverGoalKinds
	};

	int moverGoals[kMoverGoalKinds] = {};
	int moverStraight = 0;
}

// Whether this machine should have a map at all: the switch is on, and the aliens are run here
static bool AlienWays_On()
{
	if (!AlienWaysSwitchedOn)
	{
		return false;
	}

	return AvP.Network == I_No_Network || AvP.NetworkAIServer;
}

// The map, if it's ready to search, or nothing
static AlienWaysMap *ReadyMap()
{
	return theBuild.State() == kAlienWaysReady ? &theBuild.Map() : 0;
}

// Who an alien is, for the guide: where the game keeps it, and its name. The name tells apart a new alien kept where a dead one was.
static AlienWaysWho WhoIs(const STRATEGYBLOCK *alien)
{
	static_assert(SB_NAME_LENGTH == sizeof(uint64_t), "an alien's name fits in one of the guide's numbers");
	AlienWaysWho who;
	who.place = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(alien));
	memcpy(&who.name, alien->SBname, sizeof(who.name));
	return who;
}

// The map's polygons for one module, or none
static AlienWaysRange PolygonsOf(const MODULE *module)
{
	AlienWaysRange const none = { 0, 0 };

	if (!module || module->m_index < 0 || static_cast<size_t>(module->m_index) >= theModules.size())
	{
		return none;
	}

	return theModules[module->m_index];
}

// Whether the target is in the alien's own module, or one that can be seen from it. The game's own visibility lists say so
// (IsModuleVisibleFromModule, module.cpp), and a module with no list sees only itself.
static bool InView(MODULE *from, MODULE *to)
{
	if (!from || !to)
	{
		return false;
	}

	if (from == to)
	{
		return true;
	}

	return from->m_vmptr && IsModuleVisibleFromModule(from, to);
}

// The alien the guide knows by who, if it's still one of the game's live objects, the same one, and still an alien with dynamics. Anything
// else gives nothing, so something dead or gone is never read.
static STRATEGYBLOCK *LiveAlien(const AlienWaysWho &who)
{
	for (int i = 0; i < NumActiveStBlocks; i++)
	{
		STRATEGYBLOCK *const object = ActiveStBlockList[i];

		if (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(object)) != who.place)
		{
			continue;
		}

		AlienWaysWho const now = WhoIs(object);

		if (now.name == who.name && object->I_SBtype == I_BehaviourAlien && object->DynPtr)
		{
			return object;
		}

		return 0;
	}

	return 0;
}

// An alien that's no longer guided gets the game's usual gravity back
static void GiveGravityBack(const AlienWaysWho &who)
{
	STRATEGYBLOCK *const alien = LiveAlien(who);

	if (alien)
	{
		alien->DynPtr->UseStandardGravity = 1;
	}
}

// A live alien's state (an ALIEN_BHSTATE), or -1 if its status block is missing
static int StateOf(const STRATEGYBLOCK *alien)
{
	ALIEN_STATUS_BLOCK const *const status = static_cast<ALIEN_STATUS_BLOCK const *>(alien->SBdataptr);

	return status ? static_cast<int>(status->BehaviourState) : -1;
}

static bool SameWho(const AlienWaysWho &a, const AlienWaysWho &b)
{
	return a.place == b.place && a.name == b.name;
}

// This alien's place in the watched list. With make, an alien not on it is put on it, in a free place or the one that asked longest ago.
static Watched *WatchedFor(const AlienWaysWho &who, bool make)
{
	// Already on it
	for (Watched &w : watched)
	{
		if (w.used && SameWho(w.who, who))
		{
			return &w;
		}
	}

	if (!make)
	{
		return 0;
	}

	// A free place, or else the one that asked longest ago
	Watched *room = &watched[0];

	for (Watched &w : watched)
	{
		if (!w.used)
		{
			room = &w;
			break;
		}

		if (w.lastAsked < room->lastAsked)
		{
			room = &w;
		}
	}

	*room = Watched();
	room->used = true;
	room->who = who;
	return room;
}

// What the game says about an alien the guide found stuck, for the rest of its line (alien_ways_stuck.h)
static std::string StuckDetails(const AlienWaysWho &who)
{
	AlienWaysStuckFacts facts;
	STRATEGYBLOCK *const alien = LiveAlien(who);

	if (!alien)
	{
		return AlienWaysStuck_Details(facts);
	}

	DYNAMICSBLOCK const *const dynamics = alien->DynPtr;
	ALIEN_STATUS_BLOCK const *const status = static_cast<ALIEN_STATUS_BLOCK const *>(alien->SBdataptr);
	Watched const *const w = WatchedFor(who, false);

	facts.found = true;
	facts.stateNow = StateOf(alien);

	if (w)
	{
		facts.history = w->history;
		facts.hasGoal = w->hasGoal;
		facts.goal[0] = w->goal.vx;
		facts.goal[1] = w->goal.vy;
		facts.goal[2] = w->goal.vz;
	}

	facts.velocity[0] = dynamics->LinVelocity.vx;
	facts.velocity[1] = dynamics->LinVelocity.vy;
	facts.velocity[2] = dynamics->LinVelocity.vz;
	facts.standardGravity = dynamics->UseStandardGravity != 0;
	facts.onFloor = dynamics->IsInContactWithFloor != 0;
	facts.gravity[0] = dynamics->GravityDirection.vx;
	facts.gravity[1] = dynamics->GravityDirection.vy;
	facts.gravity[2] = dynamics->GravityDirection.vz;
	facts.bodyDown[0] = dynamics->OrientMat.mat21;
	facts.bodyDown[1] = dynamics->OrientMat.mat22;
	facts.bodyDown[2] = dynamics->OrientMat.mat23;
	facts.obstructedFrames = status ? status->moveData.numObstructiveCollisions : 0;
	facts.module = alien->containingModule ? alien->containingModule->name : 0;

	return AlienWaysStuck_Details(facts);
}

// A point in metres, for the log
static std::string Metres(AlienWaysXYZ point)
{
	char text[96];
	snprintf(text, sizeof(text), "(%.1f, %.1f, %.1f)", point.x / 1000.0, point.y / 1000.0, point.z / 1000.0);
	return text;
}

// One line in the log for each search that ended
static void TellHowASearchWent(const AlienWaysReport &line)
{
	char text[512];
	const char *const startedOn = line.fromKind == kAlienWaysFloor ? " on the floor" : (line.fromKind == kAlienWaysWall ? " on a wall" : (line.fromKind == kAlienWaysCeiling ? " on the ceiling" : ""));
	std::string const where = "from " + Metres(line.from) + startedOn + " to " + Metres(line.to) + (line.toTheTarget ? " (its target)" : " (on the way to its target)");

	// A search on the grid looks at cells, and one on the map at polygons. The line is otherwise word for word the same, so Third's reader
	// reads both.
	const char *const looked = line.onGrid ? "cells" : "polygons";

	// What the game says about an alien found stuck, after the line's first part (alien_ways_stuck.h)
	std::string details;

	switch (line.outcome)
	{
		case kAlienWaysOutcomeFound:
		case kAlienWaysOutcomeAllFloor:
			snprintf(text, sizeof(text), "alien ways: alien %u %s: %s%d points, %.1f m of floor, %.1f m of wall, %.1f m of ceiling%s. "
				"Searched in %d frame%s, %.2f ms, %d %s looked at, %lld ms after it asked",
				static_cast<unsigned>(line.follower), where.c_str(),
				line.outcome == kAlienWaysOutcomeAllFloor ? "a way all on the floor, so the game steers it as always: " : "a way of ",
				line.points, line.floorMm / 1000.0, line.wallMm / 1000.0, line.ceilingMm / 1000.0, line.cutShort ? ", cut short there" : "",
				line.frames, line.frames == 1 ? "" : "s", line.microseconds / 1000.0, line.looks, looked, line.waitedMs);

			// What it cost, whether it's a flanker's, and which way it comes into the goal from, so a film can tell a flanker from the rest
			// The grid gives no cost yet (-1), so a way found on it has none in its line
			details += line.flanker ? ". A flanker's way" : ". A direct way";

			if (line.cost >= 0)
			{
				details += ", costing " + std::to_string(static_cast<long long>(line.cost));
			}

			if (line.arrivalDegrees >= 0)
			{
				details += ", coming in from " + std::to_string(line.arrivalDegrees) + " degrees";
			}

			break;

		case kAlienWaysOutcomeNoWay:
			// A grid search that looked at as many cells as it may and stopped says so, so a film can tell it from a way that isn't there
			snprintf(text, sizeof(text), "alien ways: alien %u %s: no way joins up%s. Searched in %d frame%s, %.2f ms, %d %s looked at",
				static_cast<unsigned>(line.follower), where.c_str(), line.tooFar ? ": the search stopped without reaching the goal" : "",
				line.frames, line.frames == 1 ? "" : "s", line.microseconds / 1000.0, line.looks, looked);
			break;

		case kAlienWaysOutcomeNoPolygon:
		{
			// Which end had no polygon near, so a film shows it by name
			const char *const which = line.fromKind < 0 && line.toKind < 0 ? "both it and where it's going are" : (line.fromKind < 0 ? "it is" : "where it's going is");
			snprintf(text, sizeof(text), "alien ways: alien %u %s: not searched, as %s more than %.1f m from any polygon of the map",
				static_cast<unsigned>(line.follower), where.c_str(), which, kAlienWaysNearEnough / 1000.0);
			break;
		}

		case kAlienWaysOutcomeStuck:
			snprintf(text, sizeof(text), "alien ways: alien %u stuck at %s, heading for %s (%.1f m away), with no headway for %.1f s: the game's own code has it for %.1f s",
				static_cast<unsigned>(line.follower), Metres(line.from).c_str(), Metres(line.to).c_str(),
				sqrt(pow(static_cast<double>(line.to.x) - line.from.x, 2) + pow(static_cast<double>(line.to.y) - line.from.y, 2) + pow(static_cast<double>(line.to.z) - line.from.z, 2)) / 1000.0,
				kAlienWaysNoHeadwayFor / 1000.0, kAlienWaysRestAfterStuck / 1000.0);
			details = StuckDetails(line.who);
			break;

		case kAlienWaysOutcomeRefused:
		default:
			snprintf(text, sizeof(text), "alien ways: alien %u %s: the search was refused: %s", static_cast<unsigned>(line.follower), where.c_str(),
				line.why ? line.why : "no reason given");
			break;
	}

	// The polygons its way is on, for measuring how several aliens' ways stack (lanes, #138)
	std::string polygons;

	if ((line.outcome == kAlienWaysOutcomeFound || line.outcome == kAlienWaysOutcomeAllFloor) && line.polygonCount > 0)
	{
		polygons = line.onGrid ? ". Cells:" : ". Polygons:";

		for (int p = 0; p < line.polygonCount && p < static_cast<int>(kAlienWaysMostWayPoints); p++)
		{
			polygons += " " + std::to_string(line.polygons[p]);
		}
	}

	details += polygons;
	LogString(std::string(text) + details);
}

void AlienWays_LevelStart()
{
	// Whatever was left of the last level goes first
	AlienWays_LevelEnd();

	// How far ahead along their ways the aliens aim (-alienwaysaim)
	theGuide.SetAimAhead(AlienWaysAimAheadMm);

	if (!AlienWays_On())
	{
		return;
	}

	std::vector<AlienWaysPolygon> polygons;
	AlienWaysLevelCounts counts;
	const char *const why = AlienWaysLevel_List(&polygons, &theModules, &theDoors, &counts);

	LogString("alien ways: listed " + std::to_string(counts.polygons) + " polygons from " + std::to_string(counts.modules) + " modules ("
		+ std::to_string(counts.doors) + " of them doors) and "
		+ std::to_string(counts.objects) + " still objects. Left out: " + std::to_string(counts.badCorner) + " with a corner that isn't one of its shape's, "
		+ std::to_string(counts.farCorner) + " with a corner more than 64 km out, " + std::to_string(counts.noEnd) + " whose corners have no end, "
		+ std::to_string(counts.tooFewCorners) + " with under 3 corners, " + std::to_string(counts.tooManyCorners) + " with over 8. "
		+ std::to_string(counts.noStoredFacing) + " had their facing worked out from their corners");

	if (why)
	{
		LogErrorString(std::string("alien ways: no map: ") + why);
		return;
	}

	// With -alienwaysgrid the build makes the grid right after the map, on the same thread. The map is the fallback if the grid is refused.
	theBuild.Start(std::move(polygons), AlienWaysGridSwitchedOn != 0);
	told = false;

	LogString(AlienWaysGridSwitchedOn ? "alien ways: this level's aliens search the grid, with the polygon map as the fallback"
		: "alien ways: this level's aliens search the polygon map");
}

// Once the build has finished, the log hears how it went, once
static void TellHowTheBuildWent()
{
	if (told)
	{
		return;
	}

	const AlienWaysBuildState state = theBuild.State();

	if (state == kAlienWaysBuilding)
	{
		return;
	}

	told = true;
	std::string const where = theBuild.BuiltOnItsOwnThread() ? "on its own thread" : "on the game's thread, as no thread could be had";

	if (state == kAlienWaysReady)
	{
		AlienWaysMap &map = theBuild.Map();
		LogString("alien ways: the map is ready after " + std::to_string(theBuild.Milliseconds()) + " ms, " + where + ": "
			+ std::to_string(map.Polygons()) + " polygons (" + std::to_string(map.PolygonsLeftOut()) + " left out as scenery), "
			+ std::to_string(map.Joins()) + " joins, " + std::to_string(map.BytesKept() / 1024) + " KB kept");
	}
	else if (state == kAlienWaysRefused)
	{
		const char *const why = theBuild.Why();
		LogErrorString("alien ways: no map, after " + std::to_string(theBuild.Milliseconds()) + " ms " + where + ": " + (why ? why : "no reason given"));
	}

	// The grid, when -alienwaysgrid asked for it: ready, with its size, or why not, and what the aliens search instead
	if (theBuild.GridWanted())
	{
		NpcGrid *const grid = theBuild.Grid();

		if (grid)
		{
			char size[64];
			snprintf(size, sizeof(size), "%.1f MB kept", static_cast<double>(grid->BytesKept()) / (1024.0 * 1024.0));
			LogString("alien ways: the grid is ready after " + std::to_string(theBuild.GridMilliseconds()) + " ms, after the map: "
				+ std::to_string(static_cast<long long>(grid->Sheets())) + " sheets, " + std::to_string(static_cast<long long>(grid->SurfaceCells())) + " cells on the surface, "
				+ std::to_string(static_cast<long long>(grid->Links())) + " links, " + size);
		}
		else
		{
			const char *const why = theBuild.GridWhy();
			LogErrorString("alien ways: no grid, after " + std::to_string(theBuild.GridMilliseconds()) + " ms: " + (why ? why : "no reason given")
				+ (state == kAlienWaysReady ? ". Aliens search the polygon map." : ". There's no map either, so aliens move as they always have."));
		}
	}
}

void AlienWays_Frame()
{
	TellHowTheBuildWent();

	if (!AlienWays_On())
	{
		return;
	}

	// The frame's time is in 65536ths of a second
	int const stepMs = static_cast<int>((static_cast<long long>(NormalFrameTime) * 1000) >> 16);
	clockMs += std::max(0, stepMs);

	// Each watched alien's state this frame, whatever state it's in, so a stuck line can say where its last frames went. One that's gone is
	// taken off the list.
	frameNumber++;

	for (Watched &w : watched)
	{
		if (!w.used)
		{
			continue;
		}

		STRATEGYBLOCK *const alien = LiveAlien(w.who);

		if (!alien)
		{
			w = Watched();
			continue;
		}

		AlienWaysStateHistory_Note(&w.history, StateOf(alien));
	}

	// The doors as they are now, before this frame's searches carry on: a search keeps off a door that won't let an alien through (#118)
	AlienWaysLevel_ReadDoors(theDoors, ReadyMap());

	// Every alien that stopped being guided since the last frame gets its usual gravity back, before any alien moves this frame. The log says
	// what state it left for, up to kAlienWaysMostLetGoLines times a level.
	AlienWaysWho lapsed[kAlienWaysMostFollowers];
	// The grid, once it's ready and was asked for; nothing means the map (-alienwaysgrid)
	theGuide.UseGrid(theBuild.Grid());
	size_t const lapsedCount = theGuide.Frame(ReadyMap(), stepMs, kAlienWaysLooksPerFrame, lapsed, kAlienWaysMostFollowers);

	size_t letGoCount = lapsedCount;

	// The aliens the guide kept though nothing asked about them last frame (it keeps them half a second, way and all). One that's now in a
	// state that never asks for a way, attacking or dying say, is let go at once, and joins the list below to get its usual gravity back,
	// so it doesn't hang on a wall. One still in a state that asks is counted, by that state, for the once-a-second line. Each is found by
	// LiveAlien, so a dead alien, or a new one kept where it was, is never read.
	const AlienWaysWho *missed = 0;
	size_t const missedCount = theGuide.MissedLastFrame(&missed);

	for (size_t i = 0; i < missedCount; i++)
	{
		STRATEGYBLOCK *const alien = LiveAlien(missed[i]);
		int const state = alien ? StateOf(alien) : -1;

		if (state != ABS_Approach && state != ABS_Hunt && state != ABS_Wander)
		{
			// Each alien is in the guide's lists once, so lapsed always has room for it
			if (theGuide.Forget(missed[i]) && letGoCount < kAlienWaysMostFollowers)
			{
				lapsed[letGoCount] = missed[i];
				letGoCount++;
			}

			continue;
		}

		missedFrames++;
		missedByState[state]++;
	}

	for (size_t i = 0; i < letGoCount; i++)
	{
		STRATEGYBLOCK *const letGo = LiveAlien(lapsed[i]);

		if (letGo && letGoLines < kAlienWaysMostLetGoLines)
		{
			letGoLines++;
			LogString(AlienWaysStuck_LetGoLine(StateOf(letGo), letGo->DynPtr->Position.vx, letGo->DynPtr->Position.vy, letGo->DynPtr->Position.vz));

			if (letGoLines == kAlienWaysMostLetGoLines)
			{
				LogString(AlienWaysStuck_LetGoLinesStop());
			}
		}

		GiveGravityBack(lapsed[i]);
	}

	const AlienWaysReport *reports = 0;
	size_t const reportCount = theGuide.Reports(&reports);

	for (size_t i = 0; i < reportCount; i++)
	{
		TellHowASearchWent(reports[i]);
	}

	// Once a second, where every guided alien is, so a film with several aliens can be measured for how close they keep (lanes, #138)
	if (clockMs / 1000 != placesSecond)
	{
		placesSecond = clockMs / 1000;
		size_t slots[kAlienWaysMostFollowers];
		AlienWaysXYZ at[kAlienWaysMostFollowers];
		int kinds[kAlienWaysMostFollowers];
		size_t const guided = theGuide.GuidedPlaces(slots, at, kinds, kAlienWaysMostFollowers);

		if (guided > 0)
		{
			std::string places = "alien ways: guided now, second " + std::to_string(placesSecond) + ":";

			for (size_t i = 0; i < guided; i++)
			{
				const char *const on = kinds[i] == kAlienWaysFloor ? "the floor" : (kinds[i] == kAlienWaysWall ? "a wall" : (kinds[i] == kAlienWaysCeiling ? "the ceiling" : "nothing known"));
				places += std::string(i == 0 ? " " : "; ") + "alien " + std::to_string(slots[i]) + " at " + Metres(at[i]) + " on " + on;
			}

			LogString(places);
		}

		if (missedFrames > 0)
		{
			std::string skipped = "alien ways: second " + std::to_string(placesSecond) + ": in the second before, aliens kept on their ways went "
				+ std::to_string(missedFrames) + " frames without asking for them:";

			for (int s = 0; s < kAlienWaysStateKinds; s++)
			{
				if (missedByState[s] > 0)
				{
					skipped += std::string(" ") + AlienWaysStuck_StateName(s) + " " + std::to_string(missedByState[s]);
				}
			}

			LogString(skipped);
			missedFrames = 0;
			std::fill(missedByState, missedByState + kAlienWaysStateKinds, 0);
		}

		// What the mover sent the aliens for in the second before (#149), so a film can tell which goal it was
		if (moverGoals[kMoverTarget] + moverGoals[kMoverLastSeen] + moverGoals[kMoverScent] + moverGoals[kMoverStatesOwn] > 0)
		{
			LogString("alien ways: second " + std::to_string(placesSecond) + ": the mover's asks were for the target " + std::to_string(moverGoals[kMoverTarget])
				+ ", where it was last seen " + std::to_string(moverGoals[kMoverLastSeen]) + ", the strongest scent " + std::to_string(moverGoals[kMoverScent])
				+ ", the game's own point " + std::to_string(moverGoals[kMoverStatesOwn]) + "; with no way to follow, straight for the goal "
				+ std::to_string(moverStraight));
			std::fill(moverGoals, moverGoals + kMoverGoalKinds, 0);
			moverStraight = 0;
		}
	}

	if (!toldTurnedAway && theGuide.TurnedAway() > 0)
	{
		toldTurnedAway = true;
		LogString("alien ways: more than " + std::to_string(kAlienWaysMostFollowers) + " aliens wanted a way at once. The rest are steered as they always were.");
	}
}

// The direction from one point to another, worked out in doubles so no distance in the level can overflow it, and made 65536 long, as the
// game's directions are. False, with direction untouched, when the two are the same point.
static bool DirectionTo(AlienWaysXYZ from, AlienWaysXYZ to, VECTORCH *direction)
{
	double const dx = static_cast<double>(to.x) - from.x, dy = static_cast<double>(to.y) - from.y, dz = static_cast<double>(to.z) - from.z;
	double const length = sqrt(dx * dx + dy * dy + dz * dz);

	if (!(length > 0.0))
	{
		return false;
	}

	direction->vx = static_cast<int>(dx / length * 65536.0);
	direction->vy = static_cast<int>(dy / length * 65536.0);
	direction->vz = static_cast<int>(dz / length * 65536.0);
	return true;
}

// Modified 2026-11-08 by Melanikus for the AvP port: THE MOVER (#149), an MVP, only with -alienwaysmover (which needs -alienwaysgrid).
//
// The user's rulings, through PO: the 1999 near AI's own movement is to be replaced by "pure pathfinding", and a hunting alien plans "a
// way straight to the freshest, strongest scent it can reach", or to where the target was last seen, "whichever is fresher", rather than
// following the trail one room at a time. So where an alien heads is picked here, the same whatever its state is, and the game's own Hunt
// and Wander can't pull it two ways (at Third's 5-alien spot they swapped every frame). Freshest first:
//   1. its target, while the target's module can be seen from the alien's (InView): the way goes to the target itself, as it already did;
//   2. where it last saw that target, for kAlienWaysMoverMemoryMs, until it gets within kAlienWaysMoverThere of that place;
//   3. the AI module where the marines' scent is strongest in the whole level (PherAls_ReadBuf: every marine, and the player, adds to it
//      where they are, each frame, and it spreads out from there a room at a time; the game's own Hunt follows it one room at a time), when
//      it's stronger there than where the alien is: the way goes to a place on that module's floor (StrongestScent);
//   4. otherwise the point the game's own state picked, as before (Wander's, say).
// With no way to follow yet, or none at all, it heads straight for that goal, holding on to whatever it's on, instead of being steered by
// the game's own code; except in the kAlienWaysRestAfterStuck after it's found stuck, when the game's own code has it, as before.
//
// NOT DONE YET (debt, for after the films): being heard; the scent says where a marine is, not which one, so an alien may smell another
// marine than its target; the scent's point is its module's first place on the floor, not the nearest one; the states' own gates (Hunt's
// 2 m, Wander's attack distance) still decide whether it asks at all; nothing keeps aliens from touching in the last metres; the 1999
// tests that would send an alien off to avoid something are still held back only while it's guided or sent straight, and say so in the log.
int const kAlienWaysMoverMemoryMs = 10000;   // how long it heads for where it last saw its target
int const kAlienWaysMoverThere = 2000;       // mm: this near the place it last saw it, it has got there

// The distance between two points, in mm, worked out in doubles so no two points of the level can overflow it
static double DistanceBetween(AlienWaysXYZ a, AlienWaysXYZ b)
{
	double const dx = static_cast<double>(a.x) - b.x, dy = static_cast<double>(a.y) - b.y, dz = static_cast<double>(a.z) - b.z;
	return sqrt(dx * dx + dy * dy + dz * dz);
}

// The goal is a point the mover picked: not the target itself, and in the module the game's own search finds it in (ModuleFromPosition,
// pvisible.cpp), whose polygons the map looks through first
static void MoverGoalAt(STRATEGYBLOCK *alien, AlienWaysXYZ point, AlienWaysAsk *ask)
{
	ask->goal = point;
	ask->goalIsTheTarget = false;
	VECTORCH where = { point.x, point.y, point.z };
	ask->goalPolygons[1] = PolygonsOf(ModuleFromPosition(&where, alien->containingModule));
}

// A place on the floor of the AI module where the marines' scent is strongest in the level, if it's stronger there than in the alien's own
// module: the first place the game's far AI keeps for that module's first part (FALLP_AuxLocs, pfarlocs.cpp), 10 mm above its floor. False
// if there's none stronger, or that module has no such place. Each number is checked against the size of its list before it's used, as the
// lists are made from the level file (AvP Tao #3).
static bool StrongestScent(const STRATEGYBLOCK *alien, VECTORCH *out)
{
	if (!PherAls_ReadBuf || !AIModuleArray || AIModuleArraySize <= 0 || !alien->containingModule || !alien->containingModule->m_aimodule)
	{
		return false;
	}

	int const here = alien->containingModule->m_aimodule->m_index;

	if (here < 0 || here >= AIModuleArraySize)
	{
		return false;
	}

	unsigned int strongest = PherAls_ReadBuf[here];
	int strongestModule = -1;

	for (int m = 0; m < AIModuleArraySize; m++)
	{
		if (PherAls_ReadBuf[m] > strongest)
		{
			strongest = PherAls_ReadBuf[m];
			strongestModule = m;
		}
	}

	if (strongestModule < 0)
	{
		return false;
	}

	MODULE *const *const parts = AIModuleArray[strongestModule].m_module_ptrs;
	MODULE const *const first = parts ? parts[0] : 0;

	if (!first || !FALLP_AuxLocs || first->m_index < 0 || first->m_index >= ModuleArraySize)
	{
		return false;
	}

	FARLOCATIONSHEADER const &places = FALLP_AuxLocs[first->m_index];

	if (places.numLocations <= 0 || !places.locationsList)
	{
		return false;
	}

	*out = places.locationsList[0];
	return true;
}

// The mover's goal for this ask (see above), put into ask, and which of the four it was
static int MoverGoal(STRATEGYBLOCK *alien, Watched *w, AlienWaysAsk *ask)
{
	// 1. It can see its target, so the way already goes to the target itself: it remembers where, when, and which target
	if (ask->goalIsTheTarget)
	{
		w->seen = true;
		w->seenAt = ask->goal;
		w->seenMs = clockMs;
		w->seenWho = ask->target;
		return kMoverTarget;
	}

	// 2. Where it last saw the same target, lately, until it's there. There, with the target gone, that memory is used up.
	if (w->seen && ask->hasTarget && w->seenWho.place == ask->target.place && w->seenWho.name == ask->target.name
		&& clockMs - w->seenMs <= kAlienWaysMoverMemoryMs)
	{
		if (DistanceBetween(ask->at, w->seenAt) > kAlienWaysMoverThere)
		{
			MoverGoalAt(alien, w->seenAt, ask);
			return kMoverLastSeen;
		}

		w->seen = false;
	}

	// 3. The strongest scent of the marines in the level
	VECTORCH scent;

	if (StrongestScent(alien, &scent))
	{
		MoverGoalAt(alien, AlienWaysXYZ{ scent.vx, scent.vy, scent.vz }, ask);
		return kMoverScent;
	}

	// 4. The point the game's own state picked
	return kMoverStatesOwn;
}

bool AlienWays_Steer(STRATEGYBLOCK *alien, STRATEGYBLOCK *target, const VECTORCH *goal, VECTORCH *direction)
{
	AlienWaysMap const *const map = ReadyMap();

	if (!AlienWays_On() || !map || !alien || !alien->DynPtr || !alien->containingModule || !goal || !direction)
	{
		return false;
	}

	DYNAMICSBLOCK const *const dynamics = alien->DynPtr;
	AlienWaysAsk ask;
	ask.who = WhoIs(alien);
	ask.at = AlienWaysXYZ{ dynamics->Position.vx, dynamics->Position.vy, dynamics->Position.vz };
	ask.down = AlienWaysXYZ{ dynamics->OrientMat.mat21, dynamics->OrientMat.mat22, dynamics->OrientMat.mat23 };
	ask.atPolygons = PolygonsOf(alien->containingModule);
	ask.goal = AlienWaysXYZ{ goal->vx, goal->vy, goal->vz };
	ask.goalPolygons[0] = ask.atPolygons;
	ask.goalPolygons[1] = PolygonsOf(target ? target->containingModule : 0);
	ask.goalIsTheTarget = false;

	// Which target it's coming for, so the aliens coming for one can be counted, and the ones past the first few made flankers
	ask.hasTarget = target != 0;
	ask.target = target ? WhoIs(target) : AlienWaysWho{ 0, 0 };

	// A point on the way to the target is the way into a neighbouring module, as a rule: its polygons are looked for in the module it's in,
	// found from where it is by the game's own search (ModuleFromPosition, pvisible.cpp), starting from the alien's
	if (!target || !InView(alien->containingModule, target->containingModule))
	{
		VECTORCH where = *goal;
		ask.goalPolygons[1] = PolygonsOf(ModuleFromPosition(&where, alien->containingModule));
	}

	// When the target can be seen from where the alien is, the way goes all the way to it. The game's own code heads for the way into the
	// next module instead whenever the player is in another one, and a way that short is mostly floor: the user's first play showed that.
	if (target && target->DynPtr && InView(alien->containingModule, target->containingModule))
	{
		ask.goal = AlienWaysXYZ{ target->DynPtr->Position.vx, target->DynPtr->Position.vy, target->DynPtr->Position.vz };
		ask.goalIsTheTarget = true;
	}

	// The alien goes on the watched list, if it isn't on it, with the goal the game's own code picked for it this frame (alien_ways_stuck.h)
	Watched *const w = WatchedFor(ask.who, true);
	w->hasGoal = true;
	w->goal = *goal;
	w->lastAsked = frameNumber;

	// The mover, with -alienwaysmover: where it heads is picked here, the same whatever its state (MoverGoal, above)
	if (AlienWaysMoverSwitchedOn)
	{
		moverGoals[MoverGoal(alien, w, &ask)]++;
	}

	AlienWaysXYZ towards;

	if (theGuide.Steer(map, ask, &towards))
	{
		return DirectionTo(ask.at, towards, direction);
	}

	// No way to follow yet, or none at all. Without the mover, the game's own code steers it, as before. With it, it heads straight for its
	// goal, holding on to whatever it's on, while the guide asks for a way as it always does; but in the rest after it was found stuck, the
	// game's own code has it, as before, to get it free.
	if (!AlienWaysMoverSwitchedOn || theGuide.RestingAfterStuck(ask.who))
	{
		return false;
	}

	moverStraight++;
	return DirectionTo(ask.at, ask.goal, direction);
}

void AlienWays_WouldHaveAvoided(STRATEGYBLOCK *alien, bool turnedBack)
{
	if (!alien || !alien->DynPtr || !AlienWays_On() || wouldHaveLines >= kAlienWaysMostWouldHaveLines)
	{
		return;
	}

	// At most once a second for each alien. One not on the watched list (it always is, once it's asked for a way) isn't told of at all.
	Watched *const w = WatchedFor(WhoIs(alien), false);

	if (!w || clockMs - w->lastWouldHaveMs < 1000)
	{
		return;
	}

	w->lastWouldHaveMs = clockMs;
	wouldHaveLines++;
	AlienWaysXYZ const at = { alien->DynPtr->Position.vx, alien->DynPtr->Position.vy, alien->DynPtr->Position.vz };
	const char *const module = alien->containingModule && alien->containingModule->name ? alien->containingModule->name : "no module";
	char text[384];
	snprintf(text, sizeof(text), "alien ways: an alien on its way at %s, in %s, would have been sent off to avoid something: %s. Its body's down is (%d, %d, %d)",
		Metres(at).c_str(), module,
		turnedBack ? "it turned back twice towards the same point (NPC_CannotReachTarget)" : "it ran into something for more than 10 frames in a row (NPC_IsObstructed)",
		alien->DynPtr->OrientMat.mat21, alien->DynPtr->OrientMat.mat22, alien->DynPtr->OrientMat.mat23);
	LogString(text);

	if (wouldHaveLines == kAlienWaysMostWouldHaveLines)
	{
		LogString("alien ways: " + std::to_string(kAlienWaysMostWouldHaveLines) + " lines about tests that would have sent an alien off, so no more until the next level");
	}
}

bool AlienWays_Following(STRATEGYBLOCK *alien)
{
	return alien && AlienWays_On() && ReadyMap() && theGuide.Guided(WhoIs(alien));
}

bool AlienWays_GuidedThisFrame(STRATEGYBLOCK *alien)
{
	return alien && AlienWays_On() && ReadyMap() && theGuide.GuidedThisFrame(WhoIs(alien));
}

// Modified 2026-11-08 by Melanikus for the AvP port: keeps an alien going on a frame its state didn't ask, and the watch on one alien (#109)
bool AlienWays_KeepDriving(STRATEGYBLOCK *alien, VECTORCH *direction)
{
	AlienWaysMap const *const map = ReadyMap();

	if (!map || !AlienWays_On() || !alien || !alien->DynPtr || !direction)
	{
		return false;
	}

	// Only the states that ask for a way. One that never asks, attacking or dying say, is let go by the next AlienWays_Frame.
	int const state = StateOf(alien);

	if (state != ABS_Approach && state != ABS_Hunt && state != ABS_Wander)
	{
		return false;
	}

	DYNAMICSBLOCK const *const dynamics = alien->DynPtr;
	AlienWaysXYZ const at = { dynamics->Position.vx, dynamics->Position.vy, dynamics->Position.vz };
	AlienWaysXYZ const down = { dynamics->OrientMat.mat21, dynamics->OrientMat.mat22, dynamics->OrientMat.mat23 };
	AlienWaysXYZ towards;

	if (!theGuide.Drive(map, WhoIs(alien), at, down, &towards))
	{
		return false;
	}

	return DirectionTo(at, towards, direction);
}

// Whether this is the alien the watch is on, and the watch still has lines left this level
static bool Watching(STRATEGYBLOCK *alien)
{
	return AlienWaysWatchPlace >= 0 && alien && watchLines < kAlienWaysMostWatchLines && AlienWays_On() && ReadyMap()
		&& theGuide.PlaceOf(WhoIs(alien)) == AlienWaysWatchPlace;
}

void AlienWays_WatchNote(STRATEGYBLOCK *alien, const char *note)
{
	// A frame's notes are a few short sentences, so this much is never reached; it only stops a fault from growing the line
	if (!note || !Watching(alien) || watchNotes.size() > 600)
	{
		return;
	}

	watchNotes += std::string(watchNotes.empty() ? "" : "; ") + note;
}

void AlienWays_WatchLine(STRATEGYBLOCK *alien, bool hasTarget, bool keptDriving)
{
	if (!Watching(alien) || !alien->DynPtr)
	{
		watchNotes.clear();
		return;
	}

	DYNAMICSBLOCK const *const dynamics = alien->DynPtr;
	AlienWaysXYZ const at = { dynamics->Position.vx, dynamics->Position.vy, dynamics->Position.vz };
	Watched const *const w = WatchedFor(WhoIs(alien), false);
	bool const asked = w && w->lastAsked == frameNumber;
	bool const answered = theGuide.GuidedThisFrame(WhoIs(alien));
	double const speed = sqrt(static_cast<double>(dynamics->LinVelocity.vx) * dynamics->LinVelocity.vx + static_cast<double>(dynamics->LinVelocity.vy) * dynamics->LinVelocity.vy
		+ static_cast<double>(dynamics->LinVelocity.vz) * dynamics->LinVelocity.vz) / 1000.0;
	char speedText[32];
	snprintf(speedText, sizeof(speedText), "%.1f", speed);

	// The marine scent Hunt follows (FarNPC_GetTargetAIModuleForGlobalHunt, bh_far.cpp): in its AI module, and in each one next to it, up to 8
	std::string scent = "marine scent: no AI module";

	if (alien->containingModule && alien->containingModule->m_aimodule && PherAls_ReadBuf)
	{
		AIMODULE const *const here = alien->containingModule->m_aimodule;
		scent = "marine scent here " + std::to_string(PherAls_ReadBuf[here->m_index]) + ", next door";
		int shown = 0;

		for (AIMODULE **next = here->m_link_ptrs; next && *next && shown < 8; next++, shown++)
		{
			scent += " " + std::to_string(PherAls_ReadBuf[(*next)->m_index]);
		}

		if (shown == 0)
		{
			scent += " none";
		}
	}

	LogString("alien ways watch: frame " + std::to_string(frameNumber) + ", place " + std::to_string(AlienWaysWatchPlace) + ", "
		+ AlienWaysStuck_StateName(StateOf(alien)) + " at " + Metres(at) + ": " + (watchNotes.empty() ? "its state said nothing" : watchNotes)
		+ "; the alien ways " + (asked ? (answered ? "were asked and steered it" : "were asked but didn't steer it") : "weren't asked")
		+ (keptDriving ? ", and the guide kept driving it" : "") + "; gravity " + (dynamics->UseStandardGravity ? "standard" : "off, holding on")
		+ "; " + speedText + " m/s; " + (hasTarget ? "has a target" : "no target") + "; " + scent);
	watchNotes.clear();
	watchLines++;

	if (watchLines == kAlienWaysMostWatchLines)
	{
		LogString("alien ways watch: that's this level's most, " + std::to_string(kAlienWaysMostWatchLines) + " lines, so the watch stops");
	}
}

size_t AlienWays_WaysToShow(AlienWaysShown *out, size_t most)
{
	if (!out || !AlienWays_On() || !ReadyMap())
	{
		return 0;
	}

	const AlienWaysPoint *points[kAlienWaysMostFollowers];
	int counts[kAlienWaysMostFollowers], headings[kAlienWaysMostFollowers];
	size_t const shown = theGuide.Shown(points, counts, headings, std::min(most, kAlienWaysMostFollowers));

	for (size_t i = 0; i < shown; i++)
	{
		out[i].points = points[i];
		out[i].count = counts[i];
		out[i].heading = headings[i];
	}

	return shown;
}

void AlienWays_LevelEnd()
{
	if (theBuild.State() == kAlienWaysBuilding)
	{
		LogString("alien ways: the level ended before the map was ready, so its build was stopped");
	}

	// The ways go before the map they're on
	theGuide.Clear();
	theBuild.Stop();
	std::vector<AlienWaysRange>().swap(theModules);
	std::vector<AlienWaysLevelDoor>().swap(theDoors);
	told = true;
	toldTurnedAway = false;
	watchLines = 0;
	watchNotes.clear();

	// Nothing of this level's aliens is kept for the next one
	for (Watched &w : watched)
	{
		w = Watched();
	}

	frameNumber = 0;
	letGoLines = 0;
	clockMs = 0;
	wouldHaveLines = 0;
	placesSecond = -1;
	missedFrames = 0;
	std::fill(missedByState, missedByState + kAlienWaysStateKinds, 0);
	std::fill(moverGoals, moverGoals + kMoverGoalKinds, 0);
	moverStraight = 0;
}

void AlienWays_LevelRestarts()
{
	// Every alien, its way and its claims go, and so does the stuck line's watch list, since a remade alien can have the same place and
	// name as an old one (Secondary's R-a). The build, the map, the modules' ranges and the doors stay; the log's counts and caps run on,
	// since it's the same level.
	theGuide.Clear();
	std::fill(std::begin(watched), std::end(watched), Watched());
	toldTurnedAway = false;
}
