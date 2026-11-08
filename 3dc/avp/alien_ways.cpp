// See alien_ways.h. Written 2026-11-08 by Melanikus for the AvP port.
#include "3dc.h"
#include "module.h"
#include "gamedef.h"
#include "logString.h"

#include "alien_ways.h"
#include "alien_ways_build.h"
#include "alien_ways_level.h"

#include <string>
#include <utility>
#include <vector>

int AlienWaysSwitchedOn = 0;

namespace
{
	// The one build, for the level that's loaded. A thing of its own and not made with new, so it's stopped and waited for as the program ends
	// too, even if a level is still loading then.
	AlienWaysBuild theBuild;

	// Whether the log has been told how the build went
	bool told = true;
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
	const char *const why = AlienWaysLevel_List(&polygons, &counts);

	LogString("alien ways: listed " + std::to_string(counts.polygons) + " polygons from " + std::to_string(counts.modules) + " modules and "
		+ std::to_string(counts.objects) + " still objects. Left out: " + std::to_string(counts.badCorner) + " with a corner that isn't one of its shape's, "
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

void AlienWays_Frame()
{
	if (told)
	{
		return;
	}

	AlienWaysBuildState const state = theBuild.State();

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

// NOT WRITTEN YET: it gives back nothing until the hook that makes aliens follow ways is in. It's here so the developer view can be written against it.
size_t AlienWays_WaysToShow(AlienWaysShown *out, size_t most)
{
	(void)out;
	(void)most;
	return 0;
}

void AlienWays_LevelEnd()
{
	if (theBuild.State() == kAlienWaysBuilding)
	{
		LogString("alien ways: the level ended before the map was ready, so its build was stopped");
	}

	theBuild.Stop();
	told = true;
}
