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

	// The polygon nearest to point among the ranges given, of the kind given or of any kind if it's -1, or -1 if none is within
	// kAlienWaysNearEnough
	int NearestIn(const AlienWaysMap &map, AlienWaysXYZ point, const AlienWaysRange *ranges, int rangeCount, int kind)
	{
		int best = -1;
		double bestFar = DBL_MAX;

		for (int r = 0; r < rangeCount; r++)
		{
			double howFar = DBL_MAX;
			int const found = map.NearestPolygonOfKind(point, ranges[r].first, ranges[r].end, kAlienWaysNearEnough, &howFar, kind);

			if (found >= 0 && howFar < bestFar)
			{
				best = found;
				bestFar = howFar;
			}
		}

		return best;
	}

	// The kind of surface an alien is standing on, from which way is down for it: down is +y on a floor, -y on a ceiling, and across on a wall.
	// The line between them is the map's own, kAlienWaysFloorFacing. With no down given, -1: any kind.
	int KindUnder(AlienWaysXYZ down)
	{
		double const length = sqrt(static_cast<double>(down.x) * down.x + static_cast<double>(down.y) * down.y + static_cast<double>(down.z) * down.z);

		if (!(length > 0.0))
		{
			return -1;
		}

		double const downY = down.y / length;
		return downY >= kAlienWaysFloorFacing ? kAlienWaysFloor : (downY <= -kAlienWaysFloorFacing ? kAlienWaysCeiling : kAlienWaysWall);
	}
}

// Whether the alien is on the kind of surface a point needs. Where a way steps from one kind of surface to another, up or down, the step
// counts as made only once the alien is on the new surface, by its down. Until then it keeps heading for the point where the two meet, which
// is on the surface it's on, so it goes along that surface to the corner, touches the new one, and the game's own climbing turns it onto it.
// Without this:
// - stepping up, an alien on the floor heads straight for a point up the wall, the game steers it only along the floor, and it walks beside
//   the wall without ever touching it;
// - stepping down, an alien on a wall heads straight for a point out across the floor. The game steers it only along the wall, so if that
//   point is straight out from it, nothing of the direction is left: the film on Genshd1 found one stuck on a leaning wall, 0.9 m up, heading
//   for a goal 3 m out from it.
static bool OnTheKindNeeded(const AlienWaysPoint *points, int heading, AlienWaysXYZ down)
{
	if (heading < 1)
	{
		return true;
	}

	AlienWaysKind const before = points[heading - 1].kind, after = points[heading].kind;
	return before == after || KindUnder(down) == static_cast<int>(after);
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
	reportsRead = true;
	searchFrom = searchGoal = AlienWaysXYZ{ 0, 0, 0 };
	searchToTheTarget = false;
	searchFromKind = -1;
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
			follower.fresh = false;
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

bool AlienWaysGuide::GuidedThisFrame(const AlienWaysWho &who) const
{
	int const f = Find(who);
	return f >= 0 && followers[f].steered && followers[f].guided;
}

size_t AlienWaysGuide::GuidedPlaces(size_t *slots, AlienWaysXYZ *at, int *kinds, size_t most) const
{
	size_t found = 0;

	for (size_t f = 0; f < kAlienWaysMostFollowers && found < most; f++)
	{
		Follower const &follower = followers[f];

		if (follower.used && follower.guided)
		{
			slots[found] = f;
			at[found] = follower.last.at;
			kinds[found] = KindUnder(follower.last.down);
			found++;
		}
	}

	return found;
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
	// The reports of the last frame have been read. Those Steer added since, of aliens found stuck, stay, and this frame's searches go after.
	if (reportsRead)
	{
		reportCount = 0;
	}

	reportsRead = true;
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
		line.who = follower.who;
		line.from = follower.last.at;
		line.to = follower.last.goal;
		line.toTheTarget = follower.last.goalIsTheTarget;
		line.waitedMs = now - follower.askedAt;

		// It starts on a polygon of the kind it's standing on: an alien low on a wall starts on the wall, not the floor just below it.
		// If there's none of that kind near, the nearest of any kind.
		int from = NearestIn(*map, follower.last.at, &follower.last.atPolygons, 1, KindUnder(follower.last.down));

		if (from < 0)
		{
			from = NearestIn(*map, follower.last.at, &follower.last.atPolygons, 1, -1);
		}

		int const to = NearestIn(*map, follower.last.goal, follower.last.goalPolygons, 2, -1);
		line.fromKind = from >= 0 ? static_cast<int>(map->KindOf(from)) : -1;

		if (from < 0 || to < 0)
		{
			line.outcome = kAlienWaysOutcomeNoPolygon;
			AddReport(line);
			NoNewWay(&follower, follower.last.goal);
			continue;
		}

		const char *const why = map->StartSearch(from, follower.last.at, to, follower.last.goal, kAlienWaysFloorPrice);

		if (why)
		{
			line.outcome = kAlienWaysOutcomeRefused;
			line.why = why;
			AddReport(line);
			NoNewWay(&follower, follower.last.goal);
			continue;
		}

		searching = next;
		searchFrom = follower.last.at;
		searchGoal = follower.last.goal;
		searchToTheTarget = follower.last.goalIsTheTarget;
		searchFromKind = line.fromKind;
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
	line.who = follower.who;
	line.from = searchFrom;
	line.to = searchGoal;
	line.toTheTarget = searchToTheTarget;
	line.fromKind = searchFromKind;
	line.frames = searchFrames;
	line.waitedMs = now - searchAskedAt;
	line.microseconds = searchMicroseconds;
	line.looks = map->LastSearchLooks();
	searching = -1;

	if (result != kAlienWaysFound || map->Way().size() < 2)
	{
		line.outcome = kAlienWaysOutcomeNoWay;
		AddReport(line);
		NoNewWay(&follower, searchGoal);
		return;
	}

	std::vector<AlienWaysPoint> const &way = map->Way();
	size_t const count = std::min(way.size(), kAlienWaysMostWayPoints);

	for (size_t p = 0; p + 1 < count; p++)
	{
		double const length = Distance(AlienWaysXYZ{ way[p].x, way[p].y, way[p].z }, way[p + 1]);

		switch (way[p].kind)
		{
			case kAlienWaysFloor: line.floorMm += length; break;
			case kAlienWaysWall: line.wallMm += length; break;
			case kAlienWaysCeiling: line.ceilingMm += length; break;
		}
	}

	line.points = static_cast<int>(count);
	line.cutShort = map->WayCutShort();

	// The polygons the way is on, each once in a row: a wall's climb, along and down are three points on one polygon
	std::vector<int> const &onPolygons = map->WayPolygons();

	for (size_t p = 0; p < onPolygons.size() && p < count; p++)
	{
		if (line.polygonCount == 0 || line.polygons[line.polygonCount - 1] != onPolygons[p])
		{
			line.polygons[line.polygonCount++] = onPolygons[p];
		}
	}

	// A way that never leaves the floor is no better than the game's own steering, which curves at its target and climbs any wall it touches.
	// That's where the alien and its target are on one big floor polygon, which the map can't leave and come back to (one way onto each polygon).
	// The alien is on the floor then, so dropping the way it had lets nothing fall.
	if (line.wallMm <= 0.0 && line.ceilingMm <= 0.0)
	{
		line.outcome = kAlienWaysOutcomeAllFloor;
		AddReport(line);
		DropTheWay(&follower);
		follower.restUntil = now + kAlienWaysAskAgainAfter;
		return;
	}

	for (size_t p = 0; p < count; p++)
	{
		follower.points[p] = way[p];
	}

	follower.count = static_cast<int>(count);
	follower.heading = 1;
	follower.fresh = true;
	follower.wayAt = now;
	follower.wayGoal = searchGoal;
	follower.nearest = DBL_MAX;
	follower.headwayAt = now;

	line.outcome = kAlienWaysOutcomeFound;
	AddReport(line);
}

// A search gave no new way. An alien that's following one already keeps it, so a failed renewal never drops it off a wall, and it asks again
// in kAlienWaysAskAgainAfter. The goal it failed for counts as the way's goal, so a goal that can't be reached isn't searched for every frame.
// An alien with no way is left to the game's own code till then.
void AlienWaysGuide::NoNewWay(Follower *follower, AlienWaysXYZ goal)
{
	if (follower->count > 0)
	{
		follower->wayAt = now;
		follower->wayGoal = goal;
		return;
	}

	follower->restUntil = now + kAlienWaysAskAgainAfter;
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

	// A new way starts where the alien was when its search began. It may have gone on since, so it takes the way up at the stretch nearest to
	// where it is now, not at the first one, which could be behind it by now and make it look off the way.
	if (follower.count > 0 && follower.fresh)
	{
		follower.fresh = false;
		double nearestStretch = DBL_MAX;

		for (int k = 1; k < follower.count; k++)
		{
			double const howFar = DistanceToStretch(ask.at, follower.points[k - 1], follower.points[k]);

			if (howFar < nearestStretch)
			{
				nearestStretch = howFar;
				follower.heading = k;
			}
		}

		// Unless that stretch starts with a step from one surface to another that the alien hasn't made, onto a wall from the floor, say, or
		// down off one: then it heads for that step first, so it reaches the corner and turns onto the new surface, rather than heading for a
		// point on a surface it isn't on
		if (follower.heading > 1 && !OnTheKindNeeded(follower.points, follower.heading - 1, ask.down))
		{
			follower.heading--;
		}
	}

	if (follower.count > 0)
	{
		// Past every point it's reached. The last one reached means the way's used up.
		while (follower.heading < follower.count && Reached(ask.at, ask.down, follower.points[follower.heading]) && OnTheKindNeeded(follower.points, follower.heading, ask.down))
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
			// Stuck: the game's own code has it for a while, and nothing is asked till then. The log hears of it, with where it was heading.
			if (reportsRead)
			{
				reportCount = 0;
				reportsRead = false;
			}

			AlienWaysReport line = {};
			line.outcome = kAlienWaysOutcomeStuck;
			line.follower = static_cast<size_t>(f);
			line.who = follower.who;
			line.from = ask.at;
			line.to = AlienWaysXYZ{ follower.points[follower.heading].x, follower.points[follower.heading].y, follower.points[follower.heading].z };
			line.fromKind = KindUnder(ask.down);
			line.toTheTarget = ask.goalIsTheTarget;
			AddReport(line);
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
