// See alien_ways_guide.h. Written 2026-11-08 by Melanikus for the AvP port.
#include "alien_ways_guide.h"

#include <float.h>
#include <math.h>

#include <algorithm>
#include <chrono>

namespace
{
	double Distance(AlienWaysXYZ a, AlienWaysXYZ b)
	{
		double const dx = static_cast<double>(a.x) - b.x, dy = static_cast<double>(a.y) - b.y, dz = static_cast<double>(a.z) - b.z;
		return sqrt(dx * dx + dy * dy + dz * dz);
	}

	double Distance(AlienWaysXYZ a, const AlienWaysPoint &b)
	{
		AlienWaysXYZ const there = { b.x, b.y, b.z };
		return Distance(a, there);
	}

	// How far p is from the straight stretch from a to b
	double DistanceToStretch(AlienWaysXYZ p, const AlienWaysPoint &a, const AlienWaysPoint &b)
	{
		double const abx = static_cast<double>(b.x) - a.x, aby = static_cast<double>(b.y) - a.y, abz = static_cast<double>(b.z) - a.z;
		double const apx = static_cast<double>(p.x) - a.x, apy = static_cast<double>(p.y) - a.y, apz = static_cast<double>(p.z) - a.z;
		double const length = abx * abx + aby * aby + abz * abz;
		double t = length > 0.0 ? (apx * abx + apy * aby + apz * abz) / length : 0.0;
		t = std::max(0.0, std::min(1.0, t));
		double const dx = apx - t * abx, dy = apy - t * aby, dz = apz - t * abz;
		return sqrt(dx * dx + dy * dy + dz * dz);
	}

	// Whether an alien at at, with down its down, has reached point. Across the surface it's on, it has to be within kAlienWaysReached. Up and
	// down from the surface, it may be as far as kAlienWaysBodyHeight, because the game keeps an alien's position at its middle, not its feet,
	// and the way's points are on the surfaces themselves.
	bool Reached(AlienWaysXYZ at, AlienWaysXYZ down, const AlienWaysPoint &point)
	{
		double const dx = static_cast<double>(point.x) - at.x, dy = static_cast<double>(point.y) - at.y, dz = static_cast<double>(point.z) - at.z;
		double const downLength = sqrt(static_cast<double>(down.x) * down.x + static_cast<double>(down.y) * down.y + static_cast<double>(down.z) * down.z);
		double const along = downLength > 0.0 ? (dx * down.x + dy * down.y + dz * down.z) / downLength : 0.0;
		double const acrossSquared = std::max(0.0, dx * dx + dy * dy + dz * dz - along * along);
		return acrossSquared <= static_cast<double>(kAlienWaysReached) * kAlienWaysReached && fabs(along) <= kAlienWaysBodyHeight;
	}

	bool SameWho(const AlienWaysWho &a, const AlienWaysWho &b)
	{
		return a.place == b.place && a.name == b.name;
	}

	// The polygon nearest to point among the ranges given, or -1 if none is within kAlienWaysNearEnough
	int NearestIn(const AlienWaysMap &map, AlienWaysXYZ point, const AlienWaysRange *ranges, int rangeCount)
	{
		int best = -1;
		double bestFar = DBL_MAX;

		for (int r = 0; r < rangeCount; r++)
		{
			double howFar = DBL_MAX;
			int const found = map.NearestPolygon(point, ranges[r].first, ranges[r].end, kAlienWaysNearEnough, &howFar);

			if (found >= 0 && howFar < bestFar)
			{
				best = found;
				bestFar = howFar;
			}
		}

		return best;
	}
}

AlienWaysGuide::AlienWaysGuide()
{
	Clear();
}

void AlienWaysGuide::Clear()
{
	now = 0;
	turnedAway = 0;
	searching = -1;
	searchFrom = searchGoal = AlienWaysXYZ{ 0, 0, 0 };
	searchAskedAt = searchMicroseconds = 0;
	searchFrames = 0;
	reportCount = 0;

	for (size_t f = 0; f < kAlienWaysMostFollowers; f++)
	{
		followers[f].used = false;
		followers[f].steered = false;
		followers[f].guided = false;
		followers[f].queued = false;
		followers[f].count = 0;
	}
}

int AlienWaysGuide::Find(const AlienWaysWho &who) const
{
	for (size_t f = 0; f < kAlienWaysMostFollowers; f++)
	{
		if (followers[f].used && SameWho(followers[f].who, who))
		{
			return static_cast<int>(f);
		}
	}

	return -1;
}

int AlienWaysGuide::Take(const AlienWaysWho &who)
{
	for (size_t f = 0; f < kAlienWaysMostFollowers; f++)
	{
		Follower &follower = followers[f];

		if (!follower.used)
		{
			follower.used = true;
			follower.who = who;
			follower.steered = false;
			follower.guided = false;
			follower.queued = false;
			follower.askedAt = 0;
			follower.restUntil = 0;
			follower.count = 0;
			follower.heading = 0;
			return static_cast<int>(f);
		}
	}

	turnedAway++;
	return -1;
}

void AlienWaysGuide::DropTheWay(Follower *follower)
{
	follower->count = 0;
	follower->heading = 0;
}

bool AlienWaysGuide::Guided(const AlienWaysWho &who) const
{
	int const f = Find(who);
	return f >= 0 && followers[f].guided;
}

size_t AlienWaysGuide::Shown(const AlienWaysPoint **points, int *counts, int *headings, size_t most) const
{
	size_t shown = 0;

	for (size_t f = 0; f < kAlienWaysMostFollowers && shown < most; f++)
	{
		Follower const &follower = followers[f];

		if (follower.used && follower.guided && follower.count > 0)
		{
			points[shown] = follower.points;
			counts[shown] = follower.count;
			headings[shown] = follower.heading;
			shown++;
		}
	}

	return shown;
}

void AlienWaysGuide::AddReport(const AlienWaysReport &line)
{
	if (reportCount < kAlienWaysMostFollowers)
	{
		report[reportCount] = line;
		reportCount++;
	}
}

size_t AlienWaysGuide::Frame(AlienWaysMap *map, int stepMs, int looksAllowed, AlienWaysWho *lapsed, size_t mostLapsed)
{
	reportCount = 0;
	now += std::max(0, stepMs);

	// Every alien nothing asked about since the last frame is let go. If it was being guided, the game is told, to give it its gravity back.
	size_t lapsedCount = 0;

	for (size_t f = 0; f < kAlienWaysMostFollowers; f++)
	{
		Follower &follower = followers[f];

		if (!follower.used)
		{
			continue;
		}

		if (follower.steered)
		{
			follower.steered = false;
			continue;
		}

		if (follower.guided && lapsedCount < mostLapsed)
		{
			lapsed[lapsedCount] = follower.who;
			lapsedCount++;
		}

		if (searching == static_cast<int>(f))
		{
			searching = -1;
		}

		follower.used = false;
		follower.guided = false;
		follower.queued = false;
		DropTheWay(&follower);
	}

	if (!map || !map->Ready())
	{
		searching = -1;
		return lapsedCount;
	}

	// The searches, one at a time, first asked first served, inside the frame's allowance
	int left = std::max(0, looksAllowed);

	while (left > 0)
	{
		if (searching < 0)
		{
			StartNext(map);

			if (searching < 0)
			{
				break;
			}
		}

		int const before = map->LastSearchLooks();
		auto const started = std::chrono::steady_clock::now();
		const AlienWaysSearch result = map->Work(left);
		searchMicroseconds += std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started).count();
		searchFrames++;

		// Work never looks at more than it's allowed. At least 1 comes off, so this loop always ends.
		left -= std::max(1, map->LastSearchLooks() - before);

		if (result == kAlienWaysSearching)
		{
			break;
		}

		Finish(map, result);
	}

	return lapsedCount;
}

// Starts the search for whichever alien has waited longest, if any has. One whose polygons can't be found, or whose search is refused, is told
// so in the log, and left to the game's own code for a while before it asks again.
void AlienWaysGuide::StartNext(AlienWaysMap *map)
{
	while (searching < 0)
	{
		int next = -1;

		for (size_t f = 0; f < kAlienWaysMostFollowers; f++)
		{
			Follower const &follower = followers[f];

			if (follower.used && follower.queued && (next < 0 || follower.askedAt < followers[next].askedAt))
			{
				next = static_cast<int>(f);
			}
		}

		if (next < 0)
		{
			return;
		}

		Follower &follower = followers[next];
		follower.queued = false;

		AlienWaysReport line = {};
		line.follower = static_cast<size_t>(next);
		line.from = follower.last.at;
		line.to = follower.last.goal;
		line.waitedMs = now - follower.askedAt;

		int const from = NearestIn(*map, follower.last.at, &follower.last.atPolygons, 1);
		int const to = NearestIn(*map, follower.last.goal, follower.last.goalPolygons, 2);

		if (from < 0 || to < 0)
		{
			line.outcome = kAlienWaysOutcomeNoPolygon;
			AddReport(line);
			DropTheWay(&follower);
			follower.restUntil = now + kAlienWaysAskAgainAfter;
			continue;
		}

		const char *const why = map->StartSearch(from, follower.last.at, to, follower.last.goal, kAlienWaysFloorPrice);

		if (why)
		{
			line.outcome = kAlienWaysOutcomeRefused;
			line.why = why;
			AddReport(line);
			DropTheWay(&follower);
			follower.restUntil = now + kAlienWaysAskAgainAfter;
			continue;
		}

		searching = next;
		searchFrom = follower.last.at;
		searchGoal = follower.last.goal;
		searchAskedAt = follower.askedAt;
		searchMicroseconds = 0;
		searchFrames = 0;
	}
}

// The search has ended: the alien it was for gets the way, or is told there's none
void AlienWaysGuide::Finish(AlienWaysMap *map, AlienWaysSearch result)
{
	Follower &follower = followers[searching];
	AlienWaysReport line = {};
	line.follower = static_cast<size_t>(searching);
	line.from = searchFrom;
	line.to = searchGoal;
	line.frames = searchFrames;
	line.waitedMs = now - searchAskedAt;
	line.microseconds = searchMicroseconds;
	line.looks = map->LastSearchLooks();
	searching = -1;

	if (result != kAlienWaysFound || map->Way().size() < 2)
	{
		line.outcome = kAlienWaysOutcomeNoWay;
		AddReport(line);
		DropTheWay(&follower);
		follower.restUntil = now + kAlienWaysAskAgainAfter;
		return;
	}

	std::vector<AlienWaysPoint> const &way = map->Way();
	size_t const count = std::min(way.size(), kAlienWaysMostWayPoints);

	for (size_t p = 0; p < count; p++)
	{
		follower.points[p] = way[p];

		if (p + 1 < count)
		{
			double const length = Distance(AlienWaysXYZ{ way[p].x, way[p].y, way[p].z }, way[p + 1]);

			switch (way[p].kind)
			{
				case kAlienWaysFloor: line.floorMm += length; break;
				case kAlienWaysWall: line.wallMm += length; break;
				case kAlienWaysCeiling: line.ceilingMm += length; break;
			}
		}
	}

	line.points = static_cast<int>(count);
	line.cutShort = map->WayCutShort();

	// A way that never leaves the floor is no better than the game's own steering, which curves at its target and climbs any wall it touches.
	// That's where the alien and its target are on one big floor polygon, which the map can't leave and come back to (one way onto each polygon).
	if (line.wallMm <= 0.0 && line.ceilingMm <= 0.0)
	{
		line.outcome = kAlienWaysOutcomeAllFloor;
		AddReport(line);
		DropTheWay(&follower);
		follower.restUntil = now + kAlienWaysAskAgainAfter;
		return;
	}

	follower.count = static_cast<int>(count);
	follower.heading = 1;
	follower.wayAt = now;
	follower.wayGoal = searchGoal;
	follower.nearest = DBL_MAX;
	follower.headwayAt = now;

	line.outcome = kAlienWaysOutcomeFound;
	AddReport(line);
}

bool AlienWaysGuide::Steer(const AlienWaysMap *map, const AlienWaysAsk &ask, AlienWaysXYZ *towards)
{
	if (!map || !map->Ready())
	{
		return false;
	}

	int f = Find(ask.who);

	if (f < 0)
	{
		f = Take(ask.who);

		if (f < 0)
		{
			return false;
		}
	}

	Follower &follower = followers[f];
	follower.steered = true;
	follower.guided = false;
	follower.last = ask;

	if (now < follower.restUntil)
	{
		return false;
	}

	if (follower.count > 0)
	{
		// Past every point it's reached. The last one reached means the way's used up.
		while (follower.heading < follower.count && Reached(ask.at, ask.down, follower.points[follower.heading]))
		{
			follower.heading++;
			follower.nearest = DBL_MAX;
			follower.headwayAt = now;
		}

		if (follower.heading >= follower.count)
		{
			DropTheWay(&follower);
		}
	}

	if (follower.count > 0 && DistanceToStretch(ask.at, follower.points[follower.heading - 1], follower.points[follower.heading]) > kAlienWaysOffTheWay)
	{
		DropTheWay(&follower);
	}

	if (follower.count > 0)
	{
		double const toNext = Distance(ask.at, follower.points[follower.heading]);

		if (toNext + kAlienWaysHeadway <= follower.nearest)
		{
			follower.nearest = toNext;
			follower.headwayAt = now;
		}
		else if (now - follower.headwayAt >= kAlienWaysNoHeadwayFor)
		{
			// Stuck: the game's own code has it for a while, and nothing is asked till then
			DropTheWay(&follower);
			follower.queued = false;
			follower.restUntil = now + kAlienWaysRestAfterStuck;
			return false;
		}
	}

	bool const wantsASearch = follower.count == 0 || now - follower.wayAt >= kAlienWaysAskAgainAfter || Distance(ask.goal, follower.wayGoal) > kAlienWaysGoalMoved;

	if (wantsASearch && !follower.queued && searching != f)
	{
		follower.queued = true;
		follower.askedAt = now;
	}

	if (follower.count == 0)
	{
		return false;
	}

	AlienWaysPoint const &next = follower.points[follower.heading];
	*towards = AlienWaysXYZ{ next.x, next.y, next.z };
	follower.guided = true;
	return true;
}
