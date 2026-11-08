// See alien_ways.h. Written 2026-11-08 by Melanikus for the AvP port.
// Modified 2026-11-08 by Melanikus for the AvP port:
//   an alien found stuck has its state, its last frames' states, its speed, gravity and body's down, its obstruction count, its module and
//   the game's own goal for it in the log, and an alien let go has its state (alien_ways_stuck.h). One function finds a live alien by who it is.
// Modified 2026-11-08 by Melanikus for the AvP port:
//   the map is told each frame which of the level's doors won't let an alien through, so a way never goes through a locked door (#118)
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

#include "alien_ways.h"
#include "alien_ways_build.h"
#include "alien_ways_guide.h"
#include "alien_ways_level.h"
#include "alien_ways_stuck.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

extern int NormalFrameTime;

int AlienWaysSwitchedOn = 0;

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

	// What the game says about an alien found stuck, after the line's first part (alien_ways_stuck.h)
	std::string details;

	switch (line.outcome)
	{
		case kAlienWaysOutcomeFound:
		case kAlienWaysOutcomeAllFloor:
			snprintf(text, sizeof(text), "alien ways: alien %u %s: %s%d points, %.1f m of floor, %.1f m of wall, %.1f m of ceiling%s. "
				"Searched in %d frame%s, %.2f ms, %d polygons looked at, %lld ms after it asked",
				static_cast<unsigned>(line.follower), where.c_str(),
				line.outcome == kAlienWaysOutcomeAllFloor ? "a way all on the floor, so the game steers it as always: " : "a way of ",
				line.points, line.floorMm / 1000.0, line.wallMm / 1000.0, line.ceilingMm / 1000.0, line.cutShort ? ", cut short there" : "",
				line.frames, line.frames == 1 ? "" : "s", line.microseconds / 1000.0, line.looks, line.waitedMs);
			break;

		case kAlienWaysOutcomeNoWay:
			snprintf(text, sizeof(text), "alien ways: alien %u %s: no way joins up. Searched in %d frame%s, %.2f ms, %d polygons looked at",
				static_cast<unsigned>(line.follower), where.c_str(), line.frames, line.frames == 1 ? "" : "s", line.microseconds / 1000.0, line.looks);
			break;

		case kAlienWaysOutcomeNoPolygon:
			snprintf(text, sizeof(text), "alien ways: alien %u %s: not searched, as it or where it's going is more than %.1f m from any polygon it could start on",
				static_cast<unsigned>(line.follower), where.c_str(), kAlienWaysNearEnough / 1000.0);
			break;

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

	LogString(std::string(text) + details);
}

void AlienWays_LevelStart()
{
	// Whatever was left of the last level goes first
	AlienWays_LevelEnd();

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

	theBuild.Start(std::move(polygons));
	told = false;
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
	size_t const lapsedCount = theGuide.Frame(ReadyMap(), stepMs, kAlienWaysLooksPerFrame, lapsed, kAlienWaysMostFollowers);

	for (size_t i = 0; i < lapsedCount; i++)
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

	if (!toldTurnedAway && theGuide.TurnedAway() > 0)
	{
		toldTurnedAway = true;
		LogString("alien ways: more than " + std::to_string(kAlienWaysMostFollowers) + " aliens wanted a way at once. The rest are steered as they always were.");
	}
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

	AlienWaysXYZ towards;

	if (!theGuide.Steer(map, ask, &towards))
	{
		return false;
	}

	// The direction, worked out in doubles so no distance in the level can overflow it, and made 65536 long, as the game's directions are
	double const dx = static_cast<double>(towards.x) - ask.at.x, dy = static_cast<double>(towards.y) - ask.at.y, dz = static_cast<double>(towards.z) - ask.at.z;
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

	// Nothing of this level's aliens is kept for the next one
	for (Watched &w : watched)
	{
		w = Watched();
	}

	frameNumber = 0;
	letGoLines = 0;
	clockMs = 0;
	wouldHaveLines = 0;
}
