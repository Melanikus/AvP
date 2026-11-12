// Builds an alien ways map (alien_ways_map.h) on a thread of its own, so the game's frames go on while it's made. Written 2026-11-08 by
// Melanikus for the AvP port (#109, #118).
//
// WHY A THREAD. Joining a big level's polygons up takes up to about a second (measured offline: 0.7 s on average, 1.5 s at most, in
// JavaScript). The game shouldn't stop for that, and doesn't need to: until the map is ready, aliens move as they always have.
//
// WHO TOUCHES WHAT. The thread only ever touches its own copy of the polygons, and the map, until it says it's finished. The game's thread
// asks State, and only once that says kAlienWaysReady does it use Map. So nothing is shared while the thread is working, and nothing needs a
// lock. The state is the one thing both read, and it's an atomic, written last by the thread and read first by the game.
//
//      game's thread                                 the build's thread
//      -------------                                 ------------------
//      Start(polygons) ---------- starts ---------> Build(polygons): joins them up, inside the map's limits
//      State() = building, every frame                    |
//      ... aliens move as today ...                        |
//      State() = ready   <----- says so, last ---------+
//      Map() for searching
//
//      level's end: Stop() --> asks the map to stop, waits for the thread to end, lets go of everything
//
// Modified 2026-11-08 by Melanikus for the AvP port: the grid too (#145, for #109).
// THE GRID (npc_grid.h), when Start is asked for it: small cells on every floor, wall and ceiling, built on the same thread right after the
// map, from the same polygons and the map itself. The state says ready only once both are done, so the game never sees one half made. If the
// grid is refused (its own limits, or the map was), the map is still ready, and the aliens search it as before.
//
// IF NO THREAD CAN BE STARTED, the build happens straight away on the caller's thread, which holds up one moment of the level's loading,
// and the log says so (BuiltOnItsOwnThread).
//
// NOTHING HERE KNOWS ABOUT THE GAME, so tests/alien_ways_build_test.cpp can try every part of it.
#ifndef ALIEN_WAYS_BUILD_H
#define ALIEN_WAYS_BUILD_H

#include <atomic>
#include <thread>
#include <vector>

#include "alien_ways_map.h"
#include "npc_grid.h"

enum AlienWaysBuildState
{
	kAlienWaysNothing,     // nothing has been started, or it's been stopped
	kAlienWaysBuilding,    // the thread is working: don't touch the map
	kAlienWaysReady,       // the map is ready to search
	kAlienWaysRefused      // the map couldn't be made: Why says why
};

class AlienWaysBuild
{
public:
	AlienWaysBuild();

	// Stops a build that's running and waits for it, so the program never ends with the thread still going.
	~AlienWaysBuild();

	// Starts building a map from these polygons, on a thread of its own, and with withGrid the grid after it. Whatever there was before, map,
	// grid or build, is stopped and let go first.
	void Start(std::vector<AlienWaysPolygon> &&polygons, bool withGrid = false);

	// Asks a build that's running to stop, waits for its thread to end, and lets go of the map. Safe to call at any time, as often as wanted.
	void Stop();

	AlienWaysBuildState State() const { return state.load(std::memory_order_acquire); }

	// Only for use once State says kAlienWaysReady, and only on the thread that called Start.
	AlienWaysMap &Map() { return map; }

	// Why the map was refused, in words, once State says kAlienWaysRefused. Otherwise nothing.
	const char *Why() const { return State() == kAlienWaysRefused ? why : 0; }

	// How long the build took, in milliseconds, once it's finished
	long long Milliseconds() const { return State() == kAlienWaysReady || State() == kAlienWaysRefused ? milliseconds : 0; }

	// The grid, once State says kAlienWaysReady and the grid was asked for and built. Otherwise nothing. Only on the thread that called Start.
	NpcGrid *Grid() { return State() == kAlienWaysReady && gridWanted && !gridWhy && grid.Ready() ? &grid : 0; }

	// Whether the last Start asked for the grid; why it was refused, once the build has finished (nothing if it was built, or not asked for);
	// and how long it took on its own, in milliseconds. Milliseconds above is the map's alone.
	bool GridWanted() const { return gridWanted; }
	const char *GridWhy() const { return State() == kAlienWaysReady || State() == kAlienWaysRefused ? gridWhy : 0; }
	long long GridMilliseconds() const { return State() == kAlienWaysReady || State() == kAlienWaysRefused ? gridMilliseconds : 0; }

	// Whether the last build ran on a thread of its own, or had to run on the caller's
	bool BuiltOnItsOwnThread() const { return ownThread; }

	// So a test can make Start build on the caller's thread, as it does when no thread can be had. Nothing else calls this.
	void NoThreadForATest(bool noThread) { noThreadForATest = noThread; }

	// So a test can lower the map's limits, or ask it to stop part way, before Start. Nothing else calls this.
	AlienWaysMap &MapForATest() { return map; }

private:
	void Run();

	AlienWaysMap map;
	NpcGrid grid;
	std::vector<AlienWaysPolygon> polygons;
	std::thread thread;
	std::atomic<AlienWaysBuildState> state;
	const char *why;
	long long milliseconds;
	bool gridWanted;
	const char *gridWhy;
	long long gridMilliseconds;
	bool ownThread;
	bool noThreadForATest;
};

#endif
