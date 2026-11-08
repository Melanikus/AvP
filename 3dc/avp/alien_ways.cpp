// See alien_ways.h. Written 2026-11-08 by Melanikus for the AvP port.
#include "3dc.h"
#include "module.h"
#include "strategy_def.h"
#include "gamedef.h"
#include "bh_types.h"
#include "dynblock.h"
#include "logString.h"

#include "alien_ways.h"
#include "alien_ways_build.h"
#include "alien_ways_guide.h"
#include "alien_ways_level.h"

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

	// Every alien's way. Kept here, not made with new, for the same reason as the build.
	AlienWaysGuide theGuide;

	// Whether the log has been told how the build went, and that aliens were turned away for want of room
	bool told = true;
	bool toldTurnedAway = false;
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

// An alien that's no longer guided gets the game's usual gravity back. It's only touched if it's still one of the game's live objects, the same
// one, and still an alien: something dead or gone is never read.
static void GiveGravityBack(const AlienWaysWho &who)
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
			object->DynPtr->UseStandardGravity = 1;
		}

		return;
	}
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

		case kAlienWaysOutcomeRefused:
		default:
			snprintf(text, sizeof(text), "alien ways: alien %u %s: the search was refused: %s", static_cast<unsigned>(line.follower), where.c_str(),
				line.why ? line.why : "no reason given");
			break;
	}

	LogString(text);
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
	const char *const why = AlienWaysLevel_List(&polygons, &theModules, &counts);

	LogString("alien ways: listed " + std::to_string(counts.polygons) + " polygons from " + std::to_string(counts.modules) + " modules and "
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

	// Every alien that stopped being guided since the last frame gets its usual gravity back, before any alien moves this frame
	AlienWaysWho lapsed[kAlienWaysMostFollowers];
	size_t const lapsedCount = theGuide.Frame(ReadyMap(), stepMs, kAlienWaysLooksPerFrame, lapsed, kAlienWaysMostFollowers);

	for (size_t i = 0; i < lapsedCount; i++)
	{
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

	// When the target can be seen from where the alien is, the way goes all the way to it. The game's own code heads for the way into the
	// next module instead whenever the player is in another one, and a way that short is mostly floor: the user's first play showed that.
	if (target && target->DynPtr && InView(alien->containingModule, target->containingModule))
	{
		ask.goal = AlienWaysXYZ{ target->DynPtr->Position.vx, target->DynPtr->Position.vy, target->DynPtr->Position.vz };
		ask.goalIsTheTarget = true;
	}

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

bool AlienWays_Following(STRATEGYBLOCK *alien)
{
	return alien && AlienWays_On() && ReadyMap() && theGuide.Guided(WhoIs(alien));
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
	told = true;
	toldTurnedAway = false;
}
