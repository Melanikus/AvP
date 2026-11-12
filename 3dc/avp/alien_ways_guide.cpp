// See alien_ways_guide.h. Written 2026-11-08 by Melanikus for the AvP port.
// Modified 2026-11-08 by Melanikus for the AvP port:
//   Drive keeps an alien going along its way on a frame its state didn't ask, without counting as asking (#109)
//   the searches go to the grid while the game hands one over (UseGrid; #145, for #109)
#include "alien_ways_guide.h"
#include "npc_grid.h"

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

	// How far p is from the straight stretch from a to b across the surface an alien with that down is on, leaving out how far it is up
	// from the surface: the game keeps an alien's place at its box's foot, half a metre or so out from a wall it clings to (THE SLIDE).
	// Added 2026-11-11 by Melanikus (Third) for the AvP port, #109.
	double AcrossToStretch(AlienWaysXYZ p, AlienWaysXYZ down, const AlienWaysPoint &a, const AlienWaysPoint &b)
	{
		double const abx = static_cast<double>(b.x) - a.x, aby = static_cast<double>(b.y) - a.y, abz = static_cast<double>(b.z) - a.z;
		double const apx = static_cast<double>(p.x) - a.x, apy = static_cast<double>(p.y) - a.y, apz = static_cast<double>(p.z) - a.z;
		double const length = abx * abx + aby * aby + abz * abz;
		double t = length > 0.0 ? (apx * abx + apy * aby + apz * abz) / length : 0.0;
		t = std::max(0.0, std::min(1.0, t));
		double const dx = apx - t * abx, dy = apy - t * aby, dz = apz - t * abz;
		double const downLength = sqrt(static_cast<double>(down.x) * down.x + static_cast<double>(down.y) * down.y + static_cast<double>(down.z) * down.z);
		double const upFromIt = downLength > 0.0 ? (dx * down.x + dy * down.y + dz * down.z) / downLength : 0.0;
		return sqrt(std::max(0.0, dx * dx + dy * dy + dz * dz - upFromIt * upFromIt));
	}

	// Where an alien's box touches what it's on, from its place, which is the box's foot: on the floor, the place itself; on a wall, 450 mm
	// on along its down, the box's half-width; on a ceiling, 900 mm, its height crouched (CE_ALIEN, extents.cpp). For THE SLIDE's gap
	// (Secondary's review, S1). Added 2026-11-11 by Melanikus (Third) for the AvP port, #109.
	AlienWaysXYZ TouchPoint(AlienWaysXYZ at, AlienWaysXYZ down, int kindUnder)
	{
		double const length = sqrt(static_cast<double>(down.x) * down.x + static_cast<double>(down.y) * down.y + static_cast<double>(down.z) * down.z);
		double const by = kindUnder == kAlienWaysCeiling ? 900.0 : (kindUnder == kAlienWaysWall ? 450.0 : 0.0);

		if (!(length > 0.0))
		{
			return at;
		}

		return AlienWaysXYZ{ static_cast<int>(lround(at.x + down.x / length * by)), static_cast<int>(lround(at.y + down.y / length * by)),
			static_cast<int>(lround(at.z + down.z / length * by)) };
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

	return AlienWays_StepMade(points[heading - 1], points[heading], down);
}

bool AlienWays_StepMade(const AlienWaysPoint &from, const AlienWaysPoint &onto, AlienWaysXYZ down)
{
	// One surface: there's no step to make (alien_ways_map.h; the aim asks the same, so it never looks past a step not made here)
	if (AlienWays_SameSurface(from, onto))
	{
		return true;
	}

	double const ox = onto.facing[0], oy = onto.facing[1], oz = onto.facing[2];
	double const oLength = sqrt(ox * ox + oy * oy + oz * oz);
	double const pi = 3.14159265358979323846;

	// Both facings known, and another plane: the step is made once the alien clings to the new polygon
	if (AlienWays_FacingKnown(from) && AlienWays_FacingKnown(onto))
	{
		double const dLength = sqrt(static_cast<double>(down.x) * down.x + static_cast<double>(down.y) * down.y + static_cast<double>(down.z) * down.z);

		if (!(dLength > 0.0))
		{
			return false;
		}

		// Down points into the surface the alien clings to, and a facing points out of it: on the new polygon, the two point opposite ways
		double const on = -(down.x * ox + down.y * oy + down.z * oz) / (dLength * oLength);
		return on >= cos(kAlienWaysOnThePlaneDegrees * pi / 180.0);
	}

	// Either facing not known, and the kinds differ: made once the alien is on the new kind, by its down
	return KindUnder(down) == static_cast<int>(onto.kind);
}

bool AlienWays_DoorLetsAnAlienThrough(AlienWaysDoorKind kind, bool locked, bool open)
{
	switch (kind)
	{
		case kAlienWaysProximityDoor:
			return !locked;
		case kAlienWaysLiftDoor:
		case kAlienWaysSwitchDoor:
			return open;
	}

	// Not one of the kinds: taken as shut, so no way is planned through it
	return false;
}

AlienWaysGuide::AlienWaysGuide()
	: grid(nullptr), searchOnGrid(false), searchTooFar(false), aimAheadMm(0), slidesOn(false)
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
	searchFlanker = false;
	searchNearFloor = false;
	grid = nullptr;
	searchOnGrid = false;
	searchTooFar = false;
	reportCount = 0;
	missedCount = 0;
	claims.Clear();
	claimsTried = claimsOn = false;

	// No alien of the last level is remembered (THE WATCHDOG'S MEMORY; added 2026-11-11 by Melanikus (Third) for the AvP port, #109)
	for (size_t m = 0; m < kAlienWaysMostFollowers; m++)
	{
		remembered[m].until = 0;
	}

	for (size_t f = 0; f < kAlienWaysMostFollowers; f++)
	{
		followers[f].used = false;
		followers[f].steered = false;
		followers[f].steeredAt = 0;
		followers[f].guided = false;
		followers[f].queued = false;
		followers[f].count = 0;
		followers[f].keptCount = 0;
		followers[f].ranked = false;
		followers[f].flanker = false;
		followers[f].mapNext = false;
		followers[f].restUntil = 0;
		followers[f].stuckUntil = 0;
		followers[f].handedOver = false;
		followers[f].sliding = false;
		followers[f].slideTriedFor = -1;
		followers[f].refusedValid = false;
		followers[f].clings = true;
		ForgetTheWatchdog(&followers[f]);
	}
}

// Modified 2026-11-11 by Melanikus (Third) for the AvP port: THE WATCHDOG (alien_ways_guide.h; #109).
// Nothing shut for it and no re-plans counted, as for an alien the guide has never seen: when it's let go, and at a level's start and end.
// Those are the only ways a place is freed, so an alien that takes one always finds it so. A place's time of 0 is free from the start,
// because the guide's time starts at 0 and only goes on.
void AlienWaysGuide::ForgetTheWatchdog(Follower *follower)
{
	for (size_t s = 0; s < kAlienWaysMostShut; s++)
	{
		follower->shutPolygon[s] = -1;
		follower->shutUntil[s] = 0;
	}

	for (int r = 0; r < kAlienWaysMostReplans; r++)
	{
		follower->replanCountsUntil[r] = 0;
	}

	follower->replanning = false;
	follower->passedAPoint = false;
	follower->headwayFrom = AlienWaysXYZ{ 0, 0, 0 };
	follower->restFrom = AlienWaysXYZ{ 0, 0, 0 };
	follower->restMovedAt = 0;
}

// The polygon is shut for this alien, and only it, for kAlienWaysShutFor from now. One shut already is shut for longer. Otherwise it goes
// in a free place, or, with none free, in the place of the one shut longest ago: every shut lasts as long, so that's the one that ends first.
// A polygon of -1, not known, shuts nothing.
void AlienWaysGuide::ShutForIt(Follower *follower, int polygon)
{
	if (polygon < 0)
	{
		return;
	}

	size_t place = 0;

	for (size_t s = 0; s < kAlienWaysMostShut; s++)
	{
		if (now < follower->shutUntil[s] && follower->shutPolygon[s] == polygon)
		{
			follower->shutUntil[s] = now + kAlienWaysShutFor;
			return;
		}

		if (follower->shutUntil[s] < follower->shutUntil[place])
		{
			place = s;
		}
	}

	follower->shutPolygon[place] = polygon;
	follower->shutUntil[place] = now + kAlienWaysShutFor;
}

// THE WATCHDOG'S MEMORY (alien_ways_guide.h): an alien let go, with anything still shut for it or a re-plan still counting, is remembered by
// who it is, until the last of them ends. A memory of the same alien is written over; otherwise a free one is taken, or, with none free,
// the one ending first. Added 2026-11-11 by Melanikus (Third) for the AvP port, #109.
void AlienWaysGuide::Remember(const Follower &follower)
{
	long long until = 0;

	for (size_t s = 0; s < kAlienWaysMostShut; s++)
	{
		until = std::max(until, follower.shutUntil[s]);
	}

	for (int r = 0; r < kAlienWaysMostReplans; r++)
	{
		until = std::max(until, follower.replanCountsUntil[r]);
	}

	if (until <= now)
	{
		return;
	}

	size_t place = kAlienWaysMostFollowers;

	for (size_t m = 0; m < kAlienWaysMostFollowers && place == kAlienWaysMostFollowers; m++)
	{
		if (now < remembered[m].until && SameWho(remembered[m].who, follower.who))
		{
			place = m;
		}
	}

	if (place == kAlienWaysMostFollowers)
	{
		place = 0;

		for (size_t m = 1; m < kAlienWaysMostFollowers; m++)
		{
			if (remembered[m].until < remembered[place].until)
			{
				place = m;
			}
		}
	}

	Remembered &memory = remembered[place];
	memory.who = follower.who;
	memory.until = until;

	for (size_t s = 0; s < kAlienWaysMostShut; s++)
	{
		memory.shutPolygon[s] = follower.shutPolygon[s];
		memory.shutUntil[s] = follower.shutUntil[s];
	}

	for (int r = 0; r < kAlienWaysMostReplans; r++)
	{
		memory.replanCountsUntil[r] = follower.replanCountsUntil[r];
	}
}

// An alien taking a place gets back what it had when it was let go, if it's remembered still, and the memory is free again. Added
// 2026-11-11 by Melanikus (Third) for the AvP port, #109.
void AlienWaysGuide::Recall(Follower *follower)
{
	for (size_t m = 0; m < kAlienWaysMostFollowers; m++)
	{
		Remembered &memory = remembered[m];

		if (now < memory.until && SameWho(memory.who, follower->who))
		{
			for (size_t s = 0; s < kAlienWaysMostShut; s++)
			{
				follower->shutPolygon[s] = memory.shutPolygon[s];
				follower->shutUntil[s] = memory.shutUntil[s];
			}

			for (int r = 0; r < kAlienWaysMostReplans; r++)
			{
				follower->replanCountsUntil[r] = memory.replanCountsUntil[r];
			}

			memory.until = 0;
			return;
		}
	}
}

// THE CAP: whether this alien may re-plan now. It may if fewer than kAlienWaysMostReplans of its re-plans were in the last
// kAlienWaysReplansWithin. If so, this one is counted, and the answer is which one it is, from 1, for the log. 0 means its re-plans are used up.
int AlienWaysGuide::TakeAReplan(Follower *follower)
{
	int freePlace = -1;
	int counted = 0;

	for (int r = 0; r < kAlienWaysMostReplans; r++)
	{
		if (now < follower->replanCountsUntil[r])
		{
			counted++;
		}
		else if (freePlace < 0)
		{
			freePlace = r;
		}
	}

	if (freePlace < 0)
	{
		return 0;
	}

	follower->replanCountsUntil[freePlace] = now + kAlienWaysReplansWithin;
	return counted + 1;
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
			follower.stuckUntil = 0;
			follower.handedOver = false;
			follower.sliding = false;
			follower.slideTriedFor = -1;
			follower.refusedValid = false;
			follower.clings = true;
			follower.count = 0;
			follower.keptCount = 0;
			follower.heading = 0;
			follower.fresh = false;
			follower.ranked = false;
			follower.flanker = false;
			follower.mapNext = false;

			// One let go lately and back has what was shut for it and its re-plans again (THE WATCHDOG'S MEMORY; added 2026-11-11 by
			// Melanikus (Third) for the AvP port, #109)
			Recall(&follower);
			return static_cast<int>(f);
		}
	}

	turnedAway++;
	return -1;
}

void AlienWaysGuide::DropTheWay(Follower *follower)
{
	// Its way's claims go with it, so the others' searches no longer pay to go round it
	claims.Remove(static_cast<int>(follower - followers));
	follower->count = 0;
	follower->heading = 0;

	// A slide is along its way, so it goes with it (THE SLIDE; added 2026-11-11 by Melanikus (Third), #109)
	follower->sliding = false;
}

// Frees an alien's place: its way, its search if one is running, and everything the guide knew about it
void AlienWaysGuide::LetGo(int f)
{
	Follower &follower = followers[f];

	if (searching == f)
	{
		searching = -1;
	}

	follower.ranked = false;
	follower.used = false;
	follower.guided = false;
	follower.queued = false;
	follower.keptCount = 0;
	DropTheWay(&follower);

	// What the watchdog kept for it goes from its place too (modified 2026-11-11 by Melanikus (Third) for the AvP port, #109), so an alien
	// later given this place starts with nothing shut; but it's remembered by who it is, for if it comes back (THE WATCHDOG'S MEMORY)
	Remember(follower);
	ForgetTheWatchdog(&follower);
}

bool AlienWaysGuide::Forget(const AlienWaysWho &who)
{
	int const f = Find(who);

	if (f < 0)
	{
		return false;
	}

	bool const wasGuided = followers[f].guided;
	LetGo(f);
	return wasGuided;
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

bool AlienWaysGuide::RestingAfterStuck(const AlienWaysWho &who) const
{
	int const f = Find(who);
	return f >= 0 && (now < followers[f].stuckUntil || (followers[f].replanning && followers[f].count == 0));
}

// Added 2026-11-11 by Melanikus (Third) for the AvP port: THE HAND-OVER (alien_ways_guide.h; #109)
bool AlienWaysGuide::HandedOver(const AlienWaysWho &who) const
{
	int const f = Find(who);
	return f >= 0 && followers[f].handedOver;
}

// Added 2026-11-11 by Melanikus (Third) for the AvP port: THE SLIDE (alien_ways_guide.h; #109)
bool AlienWaysGuide::Sliding(const AlienWaysWho &who, AlienWaysPoint *to, AlienWaysXYZ *from, AlienWaysXYZ *next) const
{
	int const f = Find(who);

	if (f < 0 || !followers[f].sliding || followers[f].slidePoint < 1 || followers[f].slidePoint >= followers[f].count || !to || !from || !next)
	{
		return false;
	}

	Follower const &follower = followers[f];
	*to = follower.slideTo;   // THE SLIDE'S OWN POINT (modified 2026-11-11 by Melanikus (Third), #109)
	to->x += follower.slidePush.x;   // PUSH CLEAR: where its end was pushed to, if it was (added 2026-11-11 by Melanikus (Third), #109)
	to->y += follower.slidePush.y;
	to->z += follower.slidePush.z;
	*from = follower.slideFrom;
	*next = AlienWaysXYZ{ follower.slideNext.x, follower.slideNext.y, follower.slideNext.z };
	return true;
}

void AlienWaysGuide::SlideEnded(const AlienWaysWho &who, bool arrived)
{
	int const f = Find(who);

	if (f < 0 || !followers[f].sliding)
	{
		return;
	}

	Follower &follower = followers[f];
	follower.sliding = false;

	// Refused: no other slide is tried till it makes headway again, and the watchdog has it, its clock running on
	if (!arrived)
	{
		follower.slideTriedFor = follower.headwayAt;

		// A REFUSED POINT (THE SLIDE, ONCE IT GOES): not slid to again till a re-plan's way comes (added 2026-11-11 by Melanikus (Third))
		follower.refusedTo = AlienWaysXYZ{ follower.slideTo.x, follower.slideTo.y, follower.slideTo.z };
		follower.refusedValid = true;
		return;
	}

	// Arrived: it has reached that point of its way, and heads for the next, its clock started again there
	AlienWaysPoint const &reached = follower.points[follower.slidePoint];
	follower.heading = follower.slidePoint + 1;
	follower.fresh = false;
	follower.passedAPoint = true;
	follower.nearest = DBL_MAX;
	follower.headwayAt = now;
	follower.headwayFrom = AlienWaysXYZ{ reached.x, reached.y, reached.z };

	if (follower.heading >= follower.count)
	{
		DropTheWay(&follower);
	}
}

// Added 2026-11-11 by Melanikus (Third) for the AvP port (#109): PUSH CLEAR (alien_ways_guide.h)
bool AlienWaysGuide::PushSlide(const AlienWaysWho &who, AlienWaysXYZ push, const char **why)
{
	int const f = Find(who);
	const char *said = 0;

	if (f < 0 || !followers[f].sliding || followers[f].slidePoint < 1 || followers[f].slidePoint >= followers[f].count)
	{
		said = "it isn't sliding";
	}
	else
	{
		Follower &follower = followers[f];
		AlienWaysPoint const &point = follower.slideTo;
		AlienWaysXYZ const pushed = { point.x + push.x, point.y + push.y, point.z + push.z };
		double const by = sqrt(static_cast<double>(push.x) * push.x + static_cast<double>(push.y) * push.y + static_cast<double>(push.z) * push.z);
		AlienWaysXYZ const touched = TouchPoint(follower.slideFrom, follower.slideDown, KindUnder(follower.slideDown));

		if (by > kAlienWaysMostPush)
		{
			said = "its end would be pushed too far to be clear of the level";
		}
		else if (!(Distance(touched, pushed) < kAlienWaysMostSlide))
		{
			said = "pushed clear of the level, its gap would be too wide";
		}
		else if (grid)
		{
			// Where it ends: the cell under the pushed point on its point's own face (that cell's sheet), as it clings there once it's back,
			// and at a corner the nearest cell of all can be on the face beside it (Secondary's read of a923cf1f, P3; modified 2026-11-11 by
			// Melanikus (Third), #109). The nearest of all only when its point's cell isn't known.
			uint32_t const fromCell = grid->NearestCell(follower.slideFrom.x, follower.slideFrom.y, follower.slideFrom.z, kNpcGridAlien, kNpcGridReach);
			uint32_t const pointCell = follower.pointCells[follower.slidePoint];
			uint32_t const onItsFace = pointCell != kNpcGridNoCell ? grid->CellOver(grid->SheetOf(pointCell), pushed.x, pushed.y, pushed.z) : kNpcGridNoCell;
			uint32_t const toCell = onItsFace != kNpcGridNoCell ? onItsFace : grid->NearestCell(pushed.x, pushed.y, pushed.z, kNpcGridAlien, kNpcGridReach);

			if (fromCell != kNpcGridNoCell && toCell != kNpcGridNoCell && grid->JoinOf(fromCell, toCell) == kNpcGridJoinThrough)
			{
				said = "pushed clear of the level, it would go onto a face hidden behind the one it's on";
			}
		}

		if (!said)
		{
			follower.slidePush = push;
		}
	}

	if (why)
	{
		*why = said;
	}

	return !said;
}

// Added 2026-11-11 by Melanikus (Third) for the AvP port (#109): PUSH CLEAR's sum (alien_ways_guide.h)
void AlienWays_PushOutOf(const AlienWaysBoxHit *hits, int count, double push[3])
{
	double most[3] = { 0.0, 0.0, 0.0 }, least[3] = { 0.0, 0.0, 0.0 };

	for (int h = 0; hits && h < count; h++)
	{
		AlienWaysBoxHit const &hit = hits[h];
		double out[3] = { -hit.way[0], -hit.way[1], -hit.way[2] };
		double by = hit.inBy;
		double const facingLength = sqrt(hit.facing[0] * hit.facing[0] + hit.facing[1] * hit.facing[1] + hit.facing[2] * hit.facing[2]);

		// The face's facing, turned toward the box's middle (against the ray), and how squarely the ray met it. A facing along the face
		// itself says nothing of which way is out, and the ray's own way back is kept.
		if (facingLength > 0.5)
		{
			double n[3] = { hit.facing[0] / facingLength, hit.facing[1] / facingLength, hit.facing[2] / facingLength };
			double along = n[0] * hit.way[0] + n[1] * hit.way[1] + n[2] * hit.way[2];

			if (along > 0.0)
			{
				n[0] = -n[0];
				n[1] = -n[1];
				n[2] = -n[2];
				along = -along;
			}

			if (along < -1e-3)
			{
				out[0] = n[0];
				out[1] = n[1];
				out[2] = n[2];
				by = hit.inBy * -along;
			}
		}

		by += kAlienWaysSlideClear;

		for (int k = 0; k < 3; k++)
		{
			most[k] = std::max(most[k], out[k] * by);
			least[k] = std::min(least[k], out[k] * by);
		}
	}

	for (int k = 0; k < 3; k++)
	{
		push[k] = most[k] + least[k];
	}
}

// Added 2026-11-11 by Melanikus (Third) for the AvP port (#109): KeepsClinging (alien_ways_guide.h)
bool AlienWaysGuide::KeepsClinging(const AlienWaysWho &who) const
{
	int const f = Find(who);
	return f >= 0 && followers[f].clings;
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

bool AlienWaysGuide::WhoAt(size_t slot, AlienWaysWho *who) const
{
	if (slot >= kAlienWaysMostFollowers || !followers[slot].used || !who)
	{
		return false;
	}

	*who = followers[slot].who;
	return true;
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

size_t AlienWaysGuide::Kept(const AlienWaysPoint **points, int *counts, size_t most) const
{
	size_t kept = 0;

	for (size_t f = 0; f < kAlienWaysMostFollowers && kept < most; f++)
	{
		Follower const &follower = followers[f];

		// One Shown gives isn't kept as well: only a way its alien isn't following now
		if (follower.used && follower.keptCount > 0 && !(follower.guided && follower.count > 0))
		{
			points[kept] = follower.points;
			counts[kept] = follower.keptCount;
			kept++;
		}
	}

	return kept;
}

int AlienWaysGuide::WayOf(size_t place, const AlienWaysPoint **points) const
{
	*points = nullptr;

	if (place >= kAlienWaysMostFollowers || !followers[place].used || followers[place].count <= 0)
	{
		return 0;
	}

	*points = followers[place].points;
	return followers[place].count;
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

	// Every alien nothing has asked about for kAlienWaysForgetAfter is let go. If it was being guided, the game is told, to give it its
	// gravity back. One that has missed only a frame or a few is kept, way and all, and named in missed.
	size_t lapsedCount = 0;
	missedCount = 0;

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

		if (now - follower.steeredAt < kAlienWaysForgetAfter)
		{
			missed[missedCount] = follower.who;
			missedCount++;
			continue;
		}

		if (follower.guided && lapsedCount < mostLapsed)
		{
			lapsed[lapsedCount] = follower.who;
			lapsedCount++;
		}

		LetGo(static_cast<int>(f));
	}

	if (!map || !map->Ready())
	{
		searching = -1;
		return lapsedCount;
	}

	// The claims, once a level, the first frame there's a map. Tried only once: if there's no memory for them, the searches go without.
	if (!claimsTried)
	{
		claimsTried = true;
		claimsOn = claims.Reset(map->Polygons());
	}

	// The searches, one at a time, first asked first served, inside the frame's allowance: in cells on the grid, in polygons on the map
	int left = std::max(0, grid ? kAlienWaysGridLooksPerFrame : looksAllowed);

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

		int const before = searchOnGrid ? static_cast<int>(grid->LastSearchLooks()) : map->LastSearchLooks();
		auto const started = std::chrono::steady_clock::now();
		const AlienWaysSearch result = searchOnGrid ? WorkOnGrid(left) : map->Work(std::min(left, std::max(0, looksAllowed)));
		searchMicroseconds += std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started).count();
		searchFrames++;

		// Work never looks at more than it's allowed. At least 1 comes off, so this loop always ends.
		int const after = searchOnGrid ? static_cast<int>(grid->LastSearchLooks()) : map->LastSearchLooks();
		left -= std::max(1, after - before);

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
		line.afterReplan = follower.replanning;

		// It starts on a polygon of the kind it's standing on: an alien low on a wall starts on the wall, not the floor just below it.
		// If there's none of that kind near, the nearest of any kind. Its own module's polygons are looked through first. If none of them is
		// near, every polygon of the map is, the same two ways: what it stands on can be a still object, which is no module's, or a
		// neighbouring module's floor.
		AlienWaysRange const everywhere = { 0, map->PolygonsGiven() };
		int from = NearestIn(*map, follower.last.at, &follower.last.atPolygons, 1, KindUnder(follower.last.down));

		if (from < 0)
		{
			from = NearestIn(*map, follower.last.at, &follower.last.atPolygons, 1, -1);
		}

		if (from < 0)
		{
			from = NearestIn(*map, follower.last.at, &everywhere, 1, KindUnder(follower.last.down));
		}

		if (from < 0)
		{
			from = NearestIn(*map, follower.last.at, &everywhere, 1, -1);
		}

		// The goal's polygon: in the two modules it's looked for in first, then anywhere. At Third's 5-alien spot (2026-11-08) every way to
		// the target was refused, because no polygon of the target's own room, nor of the alien's, was within 2 m of where it stood.
		int to = NearestIn(*map, follower.last.goal, follower.last.goalPolygons, 2, -1);

		if (to < 0)
		{
			to = NearestIn(*map, follower.last.goal, &everywhere, 1, -1);
		}

		line.fromKind = from >= 0 ? static_cast<int>(map->KindOf(from)) : -1;
		line.toKind = to >= 0 ? static_cast<int>(map->KindOf(to)) : -1;

		// On the grid, unless its last search there looked at too many cells: then this one goes to the map, as with no grid at all
		// (Primary's fallback). On the grid the polygons above only name, in the log, what the alien and its goal are on: the grid finds
		// the cells under them itself, within kNpcGridReach, and says so if it can't.
		bool const onGrid = grid && !follower.mapNext;
		follower.mapNext = false;

		if (!onGrid && (from < 0 || to < 0))
		{
			line.outcome = kAlienWaysOutcomeNoPolygon;
			AddReport(line);
			NoNewWay(&follower, follower.last.goal);
			continue;
		}

		// Its place among the aliens coming for the same target, taken at its first search for that target and kept while it's coming for
		// it. Past the first kAlienWaysDirectAliens it's a flanker: the others' claims weigh more for it, and it may go further round (FLANKERS).
		if (!follower.last.hasTarget)
		{
			follower.ranked = false;
			follower.flanker = false;
		}
		else if (!follower.ranked || !SameWho(follower.target, follower.last.target))
		{
			int before = 0;

			for (size_t g = 0; g < kAlienWaysMostFollowers; g++)
			{
				if (static_cast<int>(g) != next && followers[g].used && followers[g].ranked && SameWho(followers[g].target, follower.last.target))
				{
					before++;
				}
			}

			follower.ranked = true;
			follower.target = follower.last.target;
			follower.flanker = before >= kAlienWaysDirectAliens;
		}

		bool const flanker = follower.ranked && follower.flanker;
		const char *why = nullptr;

		// What the search keeps off: the doors shut now, and the polygons shut for this alien alone (THE WATCHDOG). Both searches take it.
		// Modified 2026-11-11 by Melanikus (Third) for the AvP port (#109): the map's search takes it too, so a re-plan's way keeps off the
		// polygons shut for it whichever search finds it.
		const std::vector<uint8_t> *const shut = ShutForTheSearch(*map, follower, from, to);

		// THE NEAR FLOOR (alien_ways_guide.h): near the target itself the floor costs what a wall does, in either search
		int const floorPrice = AlienWays_FloorPriceFor(follower.last.at, follower.last.goal, follower.last.goalIsTheTarget);
		line.nearFloor = floorPrice != kAlienWaysFloorPrice;

		if (onGrid)
		{
			why = grid->StartSearch(follower.last.at, follower.last.goal, kNpcGridAlien, floorPrice, kAlienWaysGridMostLooks,
				claimsOn ? &claims : nullptr, next, shut,
				flanker ? kAlienWaysFlankerClaimStep : kAlienWaysClaimPriceStep, flanker ? kAlienWaysFlankerMostPriceFactor : kAlienWaysClaimMostPriceFactor);
		}
		else
		{
			why = map->StartSearch(from, follower.last.at, to, follower.last.goal, floorPrice, claimsOn ? &claims : nullptr, next,
				flanker ? kAlienWaysFlankerClaimStep : kAlienWaysClaimPriceStep, flanker ? kAlienWaysFlankerMostPriceFactor : kAlienWaysClaimMostPriceFactor,
				shut);
		}

		line.onGrid = onGrid;

		if (why)
		{
			line.outcome = kAlienWaysOutcomeRefused;
			line.why = why;
			AddReport(line);

			// The grid refused it ("no cell near the start", or the goal): the same alien is searched for again at once, on the map, as with no
			// grid at all. On BAR-01 (2026-11-08) 12 of 68 searches were refused so, three of them for aliens 8 m from the marine, on walls.
			// The loop comes back to it straight away, as it has waited longest; mapNext is cleared then, so it can't come round again.
			if (onGrid)
			{
				follower.mapNext = true;
				follower.queued = true;
				continue;
			}

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
		searchFlanker = flanker;
		searchNearFloor = line.nearFloor;
		searchOnGrid = onGrid;
		searchTooFar = false;
	}
}

// THE NEAR FLOOR (alien_ways_guide.h). Added 2026-11-11 by Melanikus (Third) for the AvP port, #109.
int AlienWays_FloorPriceFor(AlienWaysXYZ at, AlienWaysXYZ goal, bool goalIsTheTarget)
{
	return goalIsTheTarget && Distance(at, goal) <= kAlienWaysNearTarget ? kAlienWaysNearFloorPrice : kAlienWaysFloorPrice;
}

// THE POUNCE (alien_ways_guide.h). Added 2026-11-11 by Melanikus (Third) for the AvP port, #109.
bool AlienWays_PouncesNow(int distanceMm, AlienWaysXYZ down, bool touching, long long msSinceLastPounce)
{
	int const kind = KindUnder(down);
	bool const onAFloorOrAWall = kind == kAlienWaysFloor || kind == kAlienWaysWall;
	bool const rested = msSinceLastPounce < 0 || msSinceLastPounce >= kAlienWaysPounceCoolDown;
	return distanceMm >= kAlienWaysPounceNearest && distanceMm <= kAlienWaysPounceRange && onAFloorOrAWall && touching && rested;
}

void AlienWaysGuide::UseGrid(NpcGrid *to)
{
	if (to != grid && searching >= 0)
	{
		followers[searching].queued = true;
		searching = -1;
	}

	grid = to;
}

// One go at the grid's search, inside the frame's allowance, said as the map's search says it. Looking at too many cells counts as no way,
// and the line says which it was.
AlienWaysSearch AlienWaysGuide::WorkOnGrid(int looksAllowed)
{
	NpcGridSearch const result = grid->Work(static_cast<size_t>(std::max(0, looksAllowed)));
	searchTooFar = result == kNpcGridTooFar;

	if (result == kNpcGridSearching)
	{
		return kAlienWaysSearching;
	}

	return result == kNpcGridFound ? kAlienWaysFound : kAlienWaysNoWay;
}

// What a search keeps off, as both searches take it: a byte for each polygon of the map, not 0 where the search keeps off that polygon.
// - The doors shut now. The map keeps whether a door is shut by the door's number (Secondary's catch), so each polygon's door is looked up.
// - Modified 2026-11-11 by Melanikus (Third) for the AvP port (THE WATCHDOG, alien_ways_guide.h; #109): the polygons shut for this alien
//   alone, and not yet open again, but never from or to, the polygons it and its goal are on. Shut, the one it's on would leave its start
//   boxed in on the grid, and its goal's would leave nowhere to arrive.
// Made when a search starts, and kept untouched until the next one starts, as both searches need. Nothing, when nothing is kept off.
const std::vector<uint8_t> *AlienWaysGuide::ShutForTheSearch(const AlienWaysMap &map, const Follower &follower, int from, int to)
{
	size_t const polygons = map.Polygons();

	// Whether the alien's place s holds a polygon this search keeps off: still shut, one of the map's, and neither from nor to
	auto const keptOff = [&](size_t s)
	{
		int const polygon = follower.shutPolygon[s];
		return now < follower.shutUntil[s] && polygon >= 0 && static_cast<size_t>(polygon) < polygons && polygon != from && polygon != to;
	};

	bool itsOwn = false;

	for (size_t s = 0; s < kAlienWaysMostShut; s++)
	{
		itsOwn = itsOwn || keptOff(s);
	}

	if (map.Doors() <= 0 && !itsOwn)
	{
		return nullptr;
	}

	searchShut.assign(polygons, 0);

	for (size_t p = 0; p < searchShut.size() && map.Doors() > 0; p++)
	{
		searchShut[p] = map.DoorShut(map.DoorOf(static_cast<int>(p))) ? 1 : 0;
	}

	for (size_t s = 0; s < kAlienWaysMostShut; s++)
	{
		if (keptOff(s))
		{
			searchShut[static_cast<size_t>(follower.shutPolygon[s])] = 1;
		}
	}

	return &searchShut;
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
	line.looks = searchOnGrid ? static_cast<int>(grid->LastSearchLooks()) : map->LastSearchLooks();
	line.flanker = searchFlanker;
	line.nearFloor = searchNearFloor;
	line.cost = searchOnGrid ? -1.0 : map->LastSearchCost();
	line.arrivalDegrees = -1;
	line.onGrid = searchOnGrid;
	line.tooFar = searchOnGrid && searchTooFar;
	line.afterReplan = follower.replanning;
	searching = -1;

	std::vector<AlienWaysPoint> const &way = searchOnGrid ? grid->Way() : map->Way();

	if (result != kAlienWaysFound || way.size() < 2)
	{
		line.outcome = kAlienWaysOutcomeNoWay;
		AddReport(line);

		// No way on the grid, or it looked at too many cells: the same alien is searched for again straight away, on the map (Primary's
		// fallback). On BAR-01 one alien's search found no way 7 times running, from cells that don't join its goal's, 125 ms each.
		if (searchOnGrid)
		{
			follower.mapNext = true;
			follower.queued = true;
			return;
		}

		NoNewWay(&follower, searchGoal);
		return;
	}

	size_t const count = std::min(way.size(), kAlienWaysMostWayPoints);

	// Where it comes into its goal from, seen from above: from the goal towards the last point of the way that isn't straight above or below it
	for (size_t p = way.size() - 1; p > 0 && line.arrivalDegrees < 0; p--)
	{
		double const dx = static_cast<double>(way[p - 1].x) - way.back().x, dz = static_cast<double>(way[p - 1].z) - way.back().z;

		if (dx * dx + dz * dz > 0.0)
		{
			double degrees = atan2(dz, dx) * 180.0 / 3.14159265358979323846;
			line.arrivalDegrees = static_cast<int>(lround(degrees < 0.0 ? degrees + 360.0 : degrees)) % 360;
		}
	}

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
	line.cutShort = searchOnGrid ? grid->WayCutShort() : map->WayCutShort();

	// The polygons the way is on, each once in a row: a wall's climb, along and down are three points on one polygon. On the grid, the map
	// polygon under every cell of the way, each once in a row, for the claims; and for the line, the cell of each point (Third's reader).
	if (searchOnGrid)
	{
		gridWayPolygons.clear();

		for (uint32_t const cell : grid->WayPath())
		{
			int const polygon = grid->PolygonOf(cell);

			if (polygon >= 0 && (gridWayPolygons.empty() || gridWayPolygons.back() != polygon))
			{
				gridWayPolygons.push_back(polygon);
			}
		}

		std::vector<uint32_t> const &cells = grid->WayCells();

		for (size_t p = 0; p < cells.size() && p < count; p++)
		{
			line.polygons[line.polygonCount++] = static_cast<int>(cells[p]);
		}
	}
	else
	{
		std::vector<int> const &onPolygons = map->WayPolygons();

		for (size_t p = 0; p < onPolygons.size() && p < count; p++)
		{
			if (line.polygonCount == 0 || line.polygons[line.polygonCount - 1] != onPolygons[p])
			{
				line.polygons[line.polygonCount++] = onPolygons[p];
			}
		}
	}

	// HELD FROM A NEW WAY (THE SLIDE, ONCE IT GOES, alien_ways_guide.h; added 2026-11-11 by Melanikus (Third) for the AvP port, #109): one that
	// comes while the alien slides isn't taken, so the slide goes where its checks said. It asks again the moment the slide is over. A
	// re-plan's way is never held: being found stuck drops the slide, and none starts while a re-plan waits.
	if (follower.sliding)
	{
		line.outcome = kAlienWaysOutcomeFound;
		line.heldForSlide = true;
		AddReport(line);
		follower.wayAt = now - kAlienWaysAskAgainAfter;
		return;
	}

	// A way that never leaves the floor is no better than the game's own steering, which curves at its target and climbs any wall it touches.
	// That's where the alien and its target are on one big floor polygon, which the map can't leave and come back to (one way onto each polygon).
	// The alien is on the floor then, so dropping the way it had lets nothing fall.
	// But not a way to the target itself: the game's own hunting can flip between Hunt and Wander every frame and never close in, as one alien
	// did a metre from the marine at Third's 5-alien spot (2026-11-08). That way is followed, all floor as it is, for the last metres.
	if (!searchToTheTarget && line.wallMm <= 0.0 && line.ceilingMm <= 0.0)
	{
		line.outcome = kAlienWaysOutcomeAllFloor;
		AddReport(line);
		DropTheWay(&follower);
		follower.restUntil = now + kAlienWaysAskAgainAfter;
		follower.replanning = false;
		return;
	}

	// THE CLOCK (alien_ways_guide.h): a way that comes while the alien is following one keeps the no-headway clock going, unless the alien
	// has moved kAlienWaysMovedOn since it last made headway. Worked out before the new way takes the old one's place. Added 2026-11-11 by
	// Melanikus (Third) for the AvP port, #109.
	bool const keepTheClock = follower.count > 0 && Distance(follower.last.at, follower.headwayFrom) < kAlienWaysMovedOn;

	for (size_t p = 0; p < count; p++)
	{
		follower.points[p] = way[p];
	}

	// The map polygon at each point, for the watchdog to shut the one at a point the alien couldn't reach: on the map, the polygon the way
	// is on from that point on; on the grid, the one under the point's cell. -1 where it isn't known. Added 2026-11-11 by Melanikus (Third)
	// for the AvP port, #109.
	std::vector<uint32_t> const *const cells = searchOnGrid ? &grid->WayCells() : nullptr;
	std::vector<int> const *const onPolygons = searchOnGrid ? nullptr : &map->WayPolygons();

	for (size_t p = 0; p < count; p++)
	{
		if (cells)
		{
			follower.pointPolygons[p] = p < cells->size() ? grid->PolygonOf((*cells)[p]) : -1;
			follower.pointCells[p] = p < cells->size() ? (*cells)[p] : kNpcGridNoCell;
		}
		else
		{
			follower.pointPolygons[p] = p < onPolygons->size() ? (*onPolygons)[p] : -1;
			follower.pointCells[p] = kNpcGridNoCell;
		}
	}

	follower.count = static_cast<int>(count);
	follower.keptCount = follower.count;
	follower.heading = 1;
	follower.fresh = true;
	follower.wayAt = now;
	follower.wayGoal = searchGoal;
	follower.nearest = DBL_MAX;
	follower.passedAPoint = false;

	// A re-plan's way clears the refused point: it goes round the polygon that was shut there (A REFUSED POINT; added 2026-11-11 by
	// Melanikus (Third) for the AvP port, #109)
	if (follower.replanning)
	{
		follower.refusedValid = false;
	}

	follower.replanning = false;

	if (!keepTheClock)
	{
		follower.headwayAt = now;
		follower.headwayFrom = follower.last.at;
	}

	// The way's claims, in place of the old way's, so the other aliens' searches pay more where it goes
	if (claimsOn)
	{
		std::vector<int> const &onPolygons = searchOnGrid ? gridWayPolygons : map->WayPolygons();
		std::vector<int> const &firstJoins = map->FirstJoins();
		std::vector<int> const &joinOthers = map->JoinOthers();
		claims.Add(static_cast<int>(line.follower), onPolygons.data(), onPolygons.size(), firstJoins.data(), firstJoins.size(), joinOthers.data(), joinOthers.size());
	}

	line.outcome = kAlienWaysOutcomeFound;
	AddReport(line);
}

// A search gave no new way. An alien that's following one already keeps it, so a failed renewal never drops it off a wall, and it asks again
// in kAlienWaysAskAgainAfter. The goal it failed for counts as the way's goal, so a goal that can't be reached isn't searched for every frame.
// An alien with no way is left to the game's own code till then.
void AlienWaysGuide::NoNewWay(Follower *follower, AlienWaysXYZ goal)
{
	// A re-plan that found nothing is over (modified 2026-11-11 by Melanikus (Third) for the AvP port, #109)
	follower->replanning = false;

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
	follower.steeredAt = now;
	follower.guided = false;
	follower.last = ask;

	// THE SLIDE (alien_ways_guide.h): while it slides, it heads for the point it slides to, and nothing else is looked at, the hand-over
	// below included, which waits for it to get there (THE SLIDE, ONCE IT GOES; modified 2026-11-11 by Melanikus (Third), #109). One that
	// takes too long is over, its point counts as refused, and the watchdog has it from here, its clock started again.
	if (follower.sliding)
	{
		if (now - follower.slideAt <= kAlienWaysMostSlideMs)
		{
			*towards = AlienWaysXYZ{ follower.slideTo.x, follower.slideTo.y, follower.slideTo.z };
			follower.guided = true;
			return true;
		}

		follower.sliding = false;
		follower.slideTriedFor = now;
		follower.refusedTo = AlienWaysXYZ{ follower.slideTo.x, follower.slideTo.y, follower.slideTo.z };
		follower.refusedValid = true;
		follower.headwayAt = now;
		follower.nearest = DBL_MAX;
	}

	// THE HAND-OVER (alien_ways_guide.h): its target in clear sight and near, the last stretch is the game's own, as in 1999, a rest or
	// not. Taken back only further off. The log hears of each hand-over once. Added 2026-11-11 by Melanikus (Third) for the AvP port (#109).
	bool const wasHandedOver = follower.handedOver;
	double const toTarget = Distance(ask.at, ask.goal);
	follower.handedOver = ask.goalIsTheTarget && ask.targetInSight && toTarget <= (wasHandedOver ? kAlienWaysTakeBackRange : kAlienWaysHandOverRange);

	// Only the game's own curve at its target, within kAlienWaysHandOverRange and in clear sight, decides its gravity; otherwise one the
	// guide has keeps clinging (KeepsClinging; added 2026-11-11 by Melanikus (Third) for the AvP port, #109)
	follower.clings = !(follower.handedOver && toTarget <= kAlienWaysHandOverRange);

	if (follower.handedOver)
	{
		if (!wasHandedOver)
		{
			if (reportsRead)
			{
				reportCount = 0;
				reportsRead = false;
			}

			AlienWaysReport line = {};
			line.outcome = kAlienWaysOutcomeHandedOver;
			line.follower = static_cast<size_t>(f);
			line.who = follower.who;
			line.from = ask.at;
			line.to = ask.goal;
			line.fromKind = KindUnder(ask.down);
			line.toTheTarget = true;
			line.shutPolygon = -1;
			AddReport(line);
		}

		// Its way and its claims go, and it asks for nothing: when it's taken back it asks again, from wherever it is then
		DropTheWay(&follower);
		follower.queued = false;

		if (searching == f)
		{
			searching = -1;
		}

		return false;
	}

	if (now < follower.restUntil)
	{
		// THE REST, HELD FROM AVOIDING (alien_ways_guide.h): resting after a stuck, held back from avoiding, an alien that hasn't moved on
		// for a while ends its rest and re-plans now, if THE CAP allows. A rest that follows no stuck, after a search that found nothing,
		// is left as it is. Added 2026-11-11 by Melanikus (Third) for the AvP port (#109).
		if (now < follower.stuckUntil)
		{
			if (Distance(ask.at, follower.restFrom) >= kAlienWaysRestMovedOn)
			{
				follower.restFrom = ask.at;
				follower.restMovedAt = now;
			}
			else if (now - follower.restMovedAt >= kAlienWaysRestStillFor)
			{
				int const replan = TakeAReplan(&follower);

				// With its re-plans used up it rests on, and is looked at again a while on, rather than every frame
				follower.restMovedAt = now;

				if (replan > 0)
				{
					if (reportsRead)
					{
						reportCount = 0;
						reportsRead = false;
					}

					AlienWaysReport line = {};
					line.outcome = kAlienWaysOutcomeRestEnded;
					line.follower = static_cast<size_t>(f);
					line.who = follower.who;
					line.from = ask.at;
					line.to = ask.goal;
					line.fromKind = KindUnder(ask.down);
					line.toTheTarget = ask.goalIsTheTarget;
					line.replan = replan;
					line.shutPolygon = -1;
					AddReport(line);

					follower.restUntil = follower.stuckUntil = now;
					follower.queued = true;
					follower.askedAt = now;
					follower.replanning = true;

					if (searching == f)
					{
						searching = -1;
					}
				}
			}
		}

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
			follower.headwayFrom = ask.at;
			follower.passedAPoint = true;
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

		// Modified 2026-11-11 by Melanikus (Third) for the AvP port (THE CLOCK, alien_ways_guide.h; #109): the first measure on a way, or
		// since the last point reached, only says where headway counts from. It isn't headway itself, or a new way would restart the clock.
		if (follower.nearest >= DBL_MAX)
		{
			follower.nearest = toNext;
		}
		else if (toNext + kAlienWaysHeadway <= follower.nearest)
		{
			follower.nearest = toNext;
			follower.headwayAt = now;
			follower.headwayFrom = ask.at;
		}
		else if (slidesOn && now - follower.headwayAt >= kAlienWaysSlideAfterStuck && follower.slideTriedFor != follower.headwayAt &&
			!follower.replanning && !(follower.refusedValid && Distance(follower.refusedTo, follower.points[follower.heading]) < kAlienWaysHeadway) &&
			Distance(TouchPoint(ask.at, ask.down, KindUnder(ask.down)), follower.points[follower.heading]) < kAlienWaysMostSlide &&
			AcrossToStretch(ask.at, ask.down, follower.points[follower.heading - 1], follower.points[follower.heading]) <= kAlienWaysMostOffTheWayToSlide &&
			!(grid && follower.pointCells[follower.heading] != kNpcGridNoCell &&
				grid->JoinOf(grid->NearestCell(ask.at.x, ask.at.y, ask.at.z, kNpcGridAlien, kNpcGridReach), follower.pointCells[follower.heading]) == kNpcGridJoinThrough))
		{
			// THE SLIDE (alien_ways_guide.h): to the point it's heading for, and no other, at once. The log hears of it. Added 2026-11-11 by
			// Melanikus (Third) for the AvP port (#109; the owner's rulings R-LAVA-4b and R-LAVA-7).
			if (reportsRead)
			{
				reportCount = 0;
				reportsRead = false;
			}

			AlienWaysPoint const &to = follower.points[follower.heading];
			AlienWaysReport line = {};
			line.outcome = kAlienWaysOutcomeSlide;
			line.follower = static_cast<size_t>(f);
			line.who = follower.who;
			line.from = ask.at;
			line.to = AlienWaysXYZ{ to.x, to.y, to.z };
			line.fromKind = KindUnder(ask.down);
			line.toKind = static_cast<int>(to.kind);
			line.toTheTarget = ask.goalIsTheTarget;
			line.shutPolygon = -1;
			AddReport(line);

			follower.sliding = true;
			follower.slidePoint = follower.heading;
			follower.slideTo = to;
			follower.slideNext = follower.points[follower.heading + 1 < follower.count ? follower.heading + 1 : follower.heading];
			follower.slideAt = now;
			follower.slideFrom = ask.at;
			follower.slideDown = ask.down;
			follower.slidePush = AlienWaysXYZ{ 0, 0, 0 };
			follower.slideTriedFor = follower.headwayAt;
			*towards = line.to;
			follower.guided = true;
			return true;
		}
		else if (now - follower.headwayAt >= kAlienWaysNoHeadwayFor)
		{
			// Stuck. The log hears of it, with where it was heading and what the watchdog does next.
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

			// THE WATCHDOG (alien_ways_guide.h). Modified 2026-11-11 by Melanikus (Third) for the AvP port (#109; R-LAVA-P3).
			// The polygon at the point it couldn't reach is shut for it alone, whatever it does next, a rest included.
			line.shutPolygon = follower.pointPolygons[follower.heading];
			ShutForIt(&follower, line.shutPolygon);
			DropTheWay(&follower);

			// Stuck before reaching any point of this way, it's stuck where the way starts, and a search from here would start on the same
			// cell: it rests first, while the game's own code moves it. Otherwise it re-plans at once, if the cap allows.
			line.replan = follower.passedAPoint ? TakeAReplan(&follower) : 0;
			line.afterStuck = !follower.passedAPoint ? kAlienWaysRestsAtTheStart : (line.replan > 0 ? kAlienWaysReplans : kAlienWaysRestsCapped);
			AddReport(line);

			if (line.afterStuck == kAlienWaysReplans)
			{
				// A search for this alien that's running started before the polygon was shut, so it's dropped, and the new one starts now
				if (searching == f)
				{
					searching = -1;
				}

				follower.queued = true;
				follower.askedAt = now;
				follower.replanning = true;
				return false;
			}

			// The rest, as before the watchdog: the game's own code has it for a while, and nothing is asked till then. Where it is now is
			// where it starts from, for THE REST, HELD FROM AVOIDING (added 2026-11-11 by Melanikus (Third) for the AvP port, #109).
			follower.queued = false;
			follower.restUntil = now + kAlienWaysRestAfterStuck;
			follower.stuckUntil = follower.restUntil;
			follower.restFrom = ask.at;
			follower.restMovedAt = now;
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

	// It aims a little further along the way than the next point, so it curves round bends (alien_ways_lookahead.h), but never past a step
	// onto another kind of surface it hasn't made, nor round a sharp bend. With the setting at 0, or a way it can't use, it's the next point.
	AlienWaysPoint const &next = follower.points[follower.heading];
	AlienWaysXYZ aim = { next.x, next.y, next.z };
	AlienWays_AimAlong(follower.points, follower.count, follower.heading, ask.at, aimAheadMm, &aim);
	*towards = aim;
	follower.guided = true;
	return true;
}

// Modified 2026-11-08 by Melanikus for the AvP port: Drive, for a frame the alien's state didn't ask (alien_ways_guide.h).
bool AlienWaysGuide::Drive(const AlienWaysMap *map, const AlienWaysWho &who, AlienWaysXYZ at, AlienWaysXYZ down, AlienWaysXYZ *towards)
{
	if (!map || !map->Ready() || !towards)
	{
		return false;
	}

	int const f = Find(who);

	if (f < 0)
	{
		return false;
	}

	Follower &follower = followers[f];

	// A slide goes on through a frame nothing asked, to the same point (THE SLIDE; added 2026-11-11 by Melanikus (Third), #109)
	if (follower.sliding && !follower.steered)
	{
		*towards = AlienWaysXYZ{ follower.slideTo.x, follower.slideTo.y, follower.slideTo.z };
		return true;
	}

	// Only on a frame Steer wasn't asked, and with a way it has taken up. Nothing here touches steered or steeredAt, so Drive never makes it
	// look asked. Two things need no test of their own: Frame lets an alien go once half a second has passed since it was asked, so one found
	// here is always within that; and one resting after being stuck has no way, because being found stuck drops it.
	if (follower.steered || follower.count <= 0 || follower.fresh)
	{
		return false;
	}

	// Past every point it has reached, as Steer goes, so it heads on rather than back at a point it's already at
	while (follower.heading < follower.count && Reached(at, down, follower.points[follower.heading]) && OnTheKindNeeded(follower.points, follower.heading, down))
	{
		follower.heading++;
		follower.nearest = DBL_MAX;
		follower.headwayAt = now;
		follower.headwayFrom = at;
		follower.passedAPoint = true;
	}

	// Used up, or off the way: the next Steer drops it. Until then the game steers it as it always did.
	if (follower.heading >= follower.count || follower.heading < 1)
	{
		return false;
	}

	double const offTheWayBy = DistanceToStretch(at, follower.points[follower.heading - 1], follower.points[follower.heading]);

	if (offTheWayBy > kAlienWaysOffTheWay)
	{
		return false;
	}

	AlienWaysPoint const &next = follower.points[follower.heading];
	AlienWaysXYZ aim = { next.x, next.y, next.z };
	AlienWays_AimAlong(follower.points, follower.count, follower.heading, at, aimAheadMm, &aim);
	*towards = aim;
	return true;
}
