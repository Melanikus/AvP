// The aliens' claims on the alien ways map (issue #138, lanes step 2). Written 2026-11-08 by Melanikus for the AvP port.
//
// WHAT IT'S FOR. The user's ruling for several aliens hunting together: "Different routes". Each alien should take a way the others aren't
// using, so they come from different directions where the level allows it. So each alien CLAIMS the polygons its way is on, and a little of
// what's next to them, and another alien's search pays a little more to cross a polygon that's claimed. A way the others are using costs
// more, so the search tends to find another, if there is one that isn't much longer.
//
// HOW MUCH, and why:
//   - a polygon the way is on: 2 claims;
//   - a polygon one join away from the way, and not on it: 1 claim, so a way right beside another counts as using it too, a little;
//   - a polygon's price is multiplied by 1 + 0.125 for each claim on it, but never by more than 1.5. Four claims (two aliens' ways, or one
//     way and two beside it) already reach that, and more aliens never make a polygon dearer than that, so a crowd can't wall a corridor off:
//     an alien that has no other way still takes it.
//   - a searching alien's OWN claims don't count against it, or it would steer off its own way every time it asked again.
//
//      alien A's way:     [2] [2] [2] [2]            the polygons next to it get [1]
//                     [1] [1] [1] [1] [1] [1]
//      alien B, searching, pays 1.25 x on A's way, 1.125 x beside it, and 1 x anywhere else
//
// WHO HOLDS WHAT. Each alien's claims are kept on their own, so taking an alien's away (it died, it found a new way, the level ended) takes
// off exactly what it put on, and nothing else. An alien is the guide's place for it, from 0 to kAlienWaysClaimsMostAliens - 1, which is the
// guide's kAlienWaysMostFollowers (alien_ways_guide.h; its test checks the two agree). The guide adds an alien's claims when it takes a way,
// takes them off when it drops the way or lets the alien go, and clears them all at the level's end.
//
// WHAT IT KNOWS. Nothing of the game: only polygon numbers, counted from 0, as the map numbers them, and the map's list of which polygon
// touches which, handed over as two plain lists with their lengths (polygon p's neighbours are joinOther[firstJoin[p]] up to
// joinOther[firstJoin[p + 1] - 1]; the map's firstJoin holds Polygons() + 2, because its search has one more node, the goal). Every number
// handed in is checked before it's used, and a number that's no polygon of the map, or a place outside a list, is left out and counted.
//
// MEMORY. Everything it will ever hold is made by Reset, once a level, when the map is ready: a count for each polygon, and room for every
// alien's claims. After that it never asks for memory, so it can't run out in the middle of a game. If Reset can't have it, it says so and
// holds nothing, and the aliens go without claims. Clear lets it all go.
//
// It's used from one thread, the game's. Nothing in it is shared.

#pragma once

#include <stddef.h>
#include <algorithm>
#include <new>
#include <vector>

int const kAlienWaysClaimsMostAliens = 64;              // the guide's kAlienWaysMostFollowers
size_t const kAlienWaysClaimsMostPolygons = 200000;     // the map's own most (AlienWays_UsualLimits' mostPolygons, alien_ways_map.cpp)
size_t const kAlienWaysClaimsMostPerAlien = 4096;       // polygons one alien can claim: a way of up to 256 polygons, and what's beside it
int const kAlienWaysClaimOnTheWay = 2;
int const kAlienWaysClaimBesideTheWay = 1;
double const kAlienWaysClaimPriceStep = 0.125;          // each claim on a polygon adds this much to its price, as a share of it
double const kAlienWaysClaimMostPriceFactor = 1.5;      // and no polygon costs more than this many times its price

// What Add did
enum AlienWaysClaimsAdded
{
	kAlienWaysClaimsAdded,          // every polygon it was given
	kAlienWaysClaimsAddedCutShort,  // as many as one alien can hold, kAlienWaysClaimsMostPerAlien, the way's own first
	kAlienWaysClaimsNotAnAlien,     // the alien's number isn't one: nothing was changed
	kAlienWaysClaimsNoMap           // there's no map to claim on (Reset hasn't been asked, or Clear has): nothing was changed
};

class AlienWaysClaims
{
public:
	AlienWaysClaims() : ignored(0), mine(kAlienWaysClaimsMostAliens) {}

	// A count for each of a map's polygons, all 0, room for every alien's claims, and no alien holding anything. For a map of more than
	// kAlienWaysClaimsMostPolygons, or with no memory to hold it all, it gives back false and keeps nothing at all.
	bool Reset(size_t polygons)
	{
		Clear();

		if (polygons > kAlienWaysClaimsMostPolygons)
		{
			return false;
		}

		try
		{
			counts.assign(polygons, 0);

			for (size_t a = 0; a < mine.size(); a++)
			{
				mine[a].reserve(kAlienWaysClaimsMostPerAlien);
			}
		}
		catch (const std::bad_alloc &)
		{
			Clear();
			return false;
		}

		return true;
	}

	// The level has ended: every count, and every alien's claims, are let go
	void Clear()
	{
		counts.clear();
		counts.shrink_to_fit();

		for (size_t a = 0; a < mine.size(); a++)
		{
			mine[a].clear();
			mine[a].shrink_to_fit();
		}

		ignored = 0;
	}

	size_t Polygons() const { return counts.size(); }

	// Polygon numbers handed in since Reset that weren't polygons of the map, or join places outside the list, and so were left out
	size_t NumbersLeftOut() const { return ignored; }

	// The alien's claims are now its way (wayCount polygons, in order) and what's one join away from it, in place of whatever it held before.
	// firstJoinCount and joinOtherCount are how many places the two lists hold.
	AlienWaysClaimsAdded Add(int alien, const int *wayPolygons, size_t wayCount, const int *firstJoin, size_t firstJoinCount, const int *joinOther, size_t joinOtherCount)
	{
		if (alien < 0 || alien >= kAlienWaysClaimsMostAliens)
		{
			return kAlienWaysClaimsNotAnAlien;
		}

		if (counts.empty())
		{
			return kAlienWaysClaimsNoMap;
		}

		Remove(alien);

		// The way's polygons first, each once however often the way comes back to it, then what's beside them. A polygon is found in what's
		// held so far by looking through it, which is quick enough for the few hundred one way has.
		std::vector<Claim> &held = mine[alien];
		bool cutShort = false;
		int const polygons = static_cast<int>(counts.size());

		for (size_t i = 0; wayPolygons && i < wayCount && !cutShort; i++)
		{
			int const polygon = wayPolygons[i];

			if (polygon < 0 || polygon >= polygons)
			{
				ignored++;
				continue;
			}

			cutShort = !Hold(held, polygon, kAlienWaysClaimOnTheWay);
		}

		for (size_t i = 0; wayPolygons && firstJoin && joinOther && i < wayCount && !cutShort; i++)
		{
			int const polygon = wayPolygons[i];

			if (polygon < 0 || polygon >= polygons)
			{
				continue;
			}

			// Both ends of its place in the join list have to be in firstJoin
			if (static_cast<size_t>(polygon) + 1 >= firstJoinCount)
			{
				ignored++;
				continue;
			}

			int const from = firstJoin[polygon], to = firstJoin[polygon + 1];

			for (int k = from; k < to && !cutShort; k++)
			{
				if (k < 0 || static_cast<size_t>(k) >= joinOtherCount)
				{
					ignored++;
					continue;
				}

				int const beside = joinOther[k];

				if (beside < 0 || beside >= polygons)
				{
					ignored++;
					continue;
				}

				cutShort = !Hold(held, beside, kAlienWaysClaimBesideTheWay);
			}
		}

		// Sorted by polygon, so the alien's own claim on one can be found quickly while it searches
		std::sort(held.begin(), held.end(), [](const Claim &a, const Claim &b) { return a.polygon < b.polygon; });

		for (size_t i = 0; i < held.size(); i++)
		{
			counts[held[i].polygon] += held[i].amount;
		}

		return cutShort ? kAlienWaysClaimsAddedCutShort : kAlienWaysClaimsAdded;
	}

	// Takes off exactly what the alien put on. An alien that holds nothing, or a number that isn't an alien, changes nothing.
	void Remove(int alien)
	{
		if (alien < 0 || alien >= kAlienWaysClaimsMostAliens)
		{
			return;
		}

		std::vector<Claim> &held = mine[alien];

		for (size_t i = 0; i < held.size(); i++)
		{
			if (held[i].polygon >= 0 && static_cast<size_t>(held[i].polygon) < counts.size())
			{
				counts[held[i].polygon] -= held[i].amount;
			}
		}

		held.clear();
	}

	// Every alien's claims on a polygon, together. 0 for a number that isn't a polygon of the map.
	int ClaimsOn(int polygon) const
	{
		return (polygon >= 0 && static_cast<size_t>(polygon) < counts.size()) ? counts[polygon] : 0;
	}

	// One alien's own claims on a polygon
	int OwnClaimOn(int alien, int polygon) const
	{
		if (alien < 0 || alien >= kAlienWaysClaimsMostAliens)
		{
			return 0;
		}

		const std::vector<Claim> &held = mine[alien];
		Claim const wanted = { polygon, 0 };
		const auto found = std::lower_bound(held.begin(), held.end(), wanted, [](const Claim &a, const Claim &b) { return a.polygon < b.polygon; });

		return (found != held.end() && found->polygon == polygon) ? found->amount : 0;
	}

	// How many times its price a polygon costs the searching alien: 1, plus kAlienWaysClaimPriceStep for each claim the OTHER aliens have on it,
	// but never more than kAlienWaysClaimMostPriceFactor. A searcher that isn't an alien's number counts every claim.
	double PriceFactor(int polygon, int searcher) const
	{
		int const others = (std::max)(0, ClaimsOn(polygon) - OwnClaimOn(searcher, polygon));

		return (std::min)(1.0 + kAlienWaysClaimPriceStep * others, kAlienWaysClaimMostPriceFactor);
	}

private:
	struct Claim
	{
		int polygon;
		int amount;
	};

	// Adds a claim to what one alien holds, or raises one it holds on that polygon to this amount. False, adding nothing, if it's full.
	static bool Hold(std::vector<Claim> &held, int polygon, int amount)
	{
		for (size_t i = 0; i < held.size(); i++)
		{
			if (held[i].polygon == polygon)
			{
				held[i].amount = (std::max)(held[i].amount, amount);
				return true;
			}
		}

		if (held.size() >= kAlienWaysClaimsMostPerAlien)
		{
			return false;
		}

		Claim const claim = { polygon, amount };
		held.push_back(claim);
		return true;
	}

	std::vector<int> counts;                  // every alien's claims on each polygon, together
	size_t ignored;
	std::vector<std::vector<Claim> > mine;    // what each alien put on, by polygon, so it can be taken off again
};
