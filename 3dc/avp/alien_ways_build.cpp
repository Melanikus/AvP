// See alien_ways_build.h. Written 2026-11-08 by Melanikus for the AvP port.
// Modified 2026-11-08 by Melanikus for the AvP port: the grid is built after the map when asked (#145, for #109).
#include "alien_ways_build.h"

#include <chrono>
#include <system_error>

AlienWaysBuild::AlienWaysBuild()
	: state(kAlienWaysNothing), why(0), milliseconds(0), gridWanted(false), gridWhy(0), gridMilliseconds(0), ownThread(false), noThreadForATest(false)
{
}

AlienWaysBuild::~AlienWaysBuild()
{
	Stop();
}

void AlienWaysBuild::Start(std::vector<AlienWaysPolygon> &&given, bool withGrid)
{
	Stop();

	polygons = std::move(given);
	gridWanted = withGrid;
	why = 0;
	milliseconds = 0;
	state.store(kAlienWaysBuilding, std::memory_order_release);

	if (!noThreadForATest)
	{
		try
		{
			thread = std::thread(&AlienWaysBuild::Run, this);
			ownThread = true;
			return;
		}
		catch (const std::system_error &)
		{
			// No thread to be had: build here instead, below
		}
	}

	ownThread = false;
	Run();
}

void AlienWaysBuild::Run()
{
	auto const started = std::chrono::steady_clock::now();
	const char *const said = map.Build(polygons);
	auto const mapDone = std::chrono::steady_clock::now();

	// The grid, from the same polygons and the map, only once the map is made. Under its own usual limits, the 64 MB cap among them.
	if (gridWanted)
	{
		if (said)
		{
			gridWhy = "the polygon map it's built from was refused";
		}
		else
		{
			grid.SetLimits(NpcGrid_UsualLimits(false));
			gridWhy = grid.Build(polygons, map);
		}

		gridMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - mapDone).count();
	}

	// The copy isn't needed once the map and the grid are made, or refused
	std::vector<AlienWaysPolygon>().swap(polygons);

	why = said;
	milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(mapDone - started).count();

	// Last of all, so that everything above is done before the game's thread can see it's finished
	state.store(said ? kAlienWaysRefused : kAlienWaysReady, std::memory_order_release);
}

void AlienWaysBuild::Stop()
{
	map.AskToStop();
	grid.AskToStop();

	if (thread.joinable())
	{
		thread.join();
	}

	// Clear also forgets the stop, ready for the next build
	map.Clear();
	grid.Clear();
	std::vector<AlienWaysPolygon>().swap(polygons);
	why = 0;
	milliseconds = 0;
	gridWanted = false;
	gridWhy = 0;
	gridMilliseconds = 0;
	state.store(kAlienWaysNothing, std::memory_order_release);
}
