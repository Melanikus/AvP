// The test of the aliens' claims on the alien ways map, in 3dc/avp/alien_ways_claims.h (issue #138, lanes step 2). Written 2026-11-08 by
// Melanikus for the AvP port.
//
// It makes a small map of its own: polygons 0 to 9 in a row, each touching the ones either side, and 10 to 14 another row branching off
// polygon 3. Aliens claim ways along it, and the test checks what each polygon then costs, and that taking the claims away leaves exactly
// what was there before. Every check has a row in tests/takeouts/alien_ways_claims_test.json that breaks it, to show the check notices.
//
// tests/build.ps1 compiles this on its own. The program gives back a non-zero result if any check failed. It runs none of the game.

#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

#include "alien_ways_claims.h"
#include "alien_ways_guide.h"
#include "memory_wall.h"

// This test is built with the standard library's own bounds checks on (/D_MSVC_STL_HARDENING=1 in tests/alien_ways_claims_test.args). With
// them, a count looked up past the end of its list stops the program at once, every time. Without them, it would read or write whatever
// happens to be in memory there, and a check could pass by luck. So the test refuses to build without the checks.
#if !defined(_MSVC_STL_HARDENING_VECTOR) || !_MSVC_STL_HARDENING_VECTOR
#error "alien_ways_claims_test needs the standard library's bounds checks: build it with /D_MSVC_STL_HARDENING=1"
#endif

// An alien's number is the guide's place for it, so the two have to hold the same number of aliens
static_assert(kAlienWaysClaimsMostAliens == static_cast<int>(kAlienWaysMostFollowers), "the claims hold as many aliens as the guide has places");

static int checksFailed = 0;

static void Check(bool good, const std::string &what)
{
	printf("%s  %s\n", good ? "ok    " : "FAILED", what.c_str());
	checksFailed += good ? 0 : 1;
}

// The made-up map's join lists, laid out as the map lays out its own: polygon p's neighbours are joinOther[firstJoin[p]] up to
// joinOther[firstJoin[p + 1] - 1], and firstJoin has one more place than the map has polygons, plus one for the search's goal.
struct Joins
{
	std::vector<int> firstJoin, joinOther;
};

static Joins TheMap()
{
	std::vector<std::vector<int> > beside(15);

	for (int p = 0; p + 1 < 10; p++)
	{
		beside[p].push_back(p + 1);
		beside[p + 1].push_back(p);
	}

	beside[3].push_back(10);
	beside[10].push_back(3);

	for (int p = 10; p + 1 < 15; p++)
	{
		beside[p].push_back(p + 1);
		beside[p + 1].push_back(p);
	}

	Joins joins;

	for (int p = 0; p < 15; p++)
	{
		joins.firstJoin.push_back(static_cast<int>(joins.joinOther.size()));
		joins.joinOther.insert(joins.joinOther.end(), beside[p].begin(), beside[p].end());
	}

	joins.firstJoin.push_back(static_cast<int>(joins.joinOther.size()));
	joins.firstJoin.push_back(static_cast<int>(joins.joinOther.size()));   // the goal's place, with no joins
	return joins;
}

static AlienWaysClaimsAdded Claim(AlienWaysClaims &claims, const Joins &joins, int alien, const std::vector<int> &way)
{
	return claims.Add(alien, way.data(), way.size(), joins.firstJoin.data(), joins.firstJoin.size(), joins.joinOther.data(), joins.joinOther.size());
}

static std::vector<int> Counts(const AlienWaysClaims &claims)
{
	std::vector<int> out;

	for (int p = 0; p < 15; p++)
	{
		out.push_back(claims.ClaimsOn(p));
	}

	return out;
}

static void TheClaims()
{
	puts("what a way claims");

	Joins const joins = TheMap();
	AlienWaysClaims claims;

	Check(claims.Reset(15) && claims.Polygons() == 15, "a map's worth of counts");
	Check(Counts(claims) == std::vector<int>(15, 0) && claims.PriceFactor(3, 1) == 1.0, "all 0, and nothing costs more");

	// Alien 0's way is 2, 3 and 4: 2 claims each. Beside it are 1 and 5, and 10 off polygon 3: 1 claim each.
	std::vector<int> const way = { 2, 3, 4 };
	Check(Claim(claims, joins, 0, way) == kAlienWaysClaimsAdded, "a way is claimed");
	std::vector<int> const afterZero = { 0, 1, 2, 2, 2, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0 };
	Check(Counts(claims) == afterZero, "2 on each polygon of the way, 1 on each one beside it, and a polygon both on it and beside it counts as on it");

	// The way comes back to a polygon: it's still claimed once
	Check(Claim(claims, joins, 0, { 2, 3, 2, 4 }) == kAlienWaysClaimsAdded && Counts(claims) == afterZero, "a polygon the way comes back to is claimed once, and claiming again replaces what the alien held");

	// What the claims cost another alien, and the alien itself
	Check(claims.PriceFactor(3, 1) == 1.25 && claims.PriceFactor(1, 1) == 1.125 && claims.PriceFactor(6, 1) == 1.0, "another alien pays 1.25 times on the way, 1.125 times beside it, and nothing more anywhere else");
	Check(claims.PriceFactor(3, 0) == 1.0 && claims.PriceFactor(1, 0) == 1.0 && claims.PriceFactor(10, 0) == 1.0, "an alien's own claims don't raise its own price");
	Check(claims.OwnClaimOn(0, 3) == 2 && claims.OwnClaimOn(0, 10) == 1 && claims.OwnClaimOn(1, 3) == 0, "each alien's own share is kept");

	// A second and a third alien stack on top
	Check(Claim(claims, joins, 1, { 3, 4, 5 }) == kAlienWaysClaimsAdded && claims.ClaimsOn(3) == 4 && claims.ClaimsOn(6) == 1, "a second alien's claims add to the first's");
	Check(claims.PriceFactor(3, 2) == 1.5, "four claims cost 1.5 times");
	Check(Claim(claims, joins, 2, { 3 }) == kAlienWaysClaimsAdded && claims.ClaimsOn(3) == 6 && claims.PriceFactor(3, 7) == 1.5, "and no more than that, however many there are");
	// Polygon 4: alien 0's way (2), alien 1's way (2) and beside alien 2's (1) make 5, and alien 1's own 2 come off
	Check(claims.PriceFactor(3, 1) == 1.5 && claims.PriceFactor(4, 1) == 1.375, "the second alien pays only for the others' claims");

	// Taking them away gives back exactly what was there
	claims.Remove(1);
	claims.Remove(2);
	Check(Counts(claims) == afterZero, "taking two aliens' claims away leaves the first's exactly as they were");
	claims.Remove(1);
	Check(Counts(claims) == afterZero, "and taking away an alien that holds nothing changes nothing");
	claims.Remove(0);
	Check(Counts(claims) == std::vector<int>(15, 0), "taking the last away leaves every count at 0");

	// Each alien's own share has to be found whatever order its way came in
	Check(Claim(claims, joins, 4, { 13, 12, 11, 10, 3, 2 }) == kAlienWaysClaimsAdded, "a way given in any order");
	bool ownFound = true;

	for (int p = 0; p < 15; p++)
	{
		ownFound = ownFound && claims.OwnClaimOn(4, p) == claims.ClaimsOn(p) && claims.PriceFactor(p, 4) == 1.0;
	}

	Check(ownFound, "has every one of its own claims found, so none costs it more");
}

static void TheLimits()
{
	puts("the limits");

	Joins const joins = TheMap();
	AlienWaysClaims claims;
	claims.Reset(15);
	Claim(claims, joins, 0, { 2, 3, 4 });
	std::vector<int> const before = Counts(claims);

	Check(Claim(claims, joins, -1, { 5 }) == kAlienWaysClaimsNotAnAlien && Claim(claims, joins, kAlienWaysClaimsMostAliens, { 5 }) == kAlienWaysClaimsNotAnAlien, "an alien's number below 0 or past the most is refused");
	claims.Remove(kAlienWaysClaimsMostAliens);
	Check(Counts(claims) == before && claims.OwnClaimOn(kAlienWaysClaimsMostAliens, 3) == 0, "and changes nothing");
	Check(Claim(claims, joins, kAlienWaysClaimsMostAliens - 1, { 7 }) == kAlienWaysClaimsAdded && claims.ClaimsOn(7) == 2, "the last alien's number is one");
	claims.Remove(kAlienWaysClaimsMostAliens - 1);

	// Numbers that aren't polygons of the map
	size_t const leftOutBefore = claims.NumbersLeftOut();
	Check(Claim(claims, joins, 1, { 15, -1, 99999 }) == kAlienWaysClaimsAdded && Counts(claims) == before, "polygon numbers that aren't on the map claim nothing");
	Check(claims.NumbersLeftOut() == leftOutBefore + 3 && claims.ClaimsOn(99999) == 0 && claims.ClaimsOn(-1) == 0, "and each is counted as left out");

	// Join lists that point outside themselves. Each is at the very end of readable memory, so reading past it would stop the test.
	{
		int const firstJoins[] = { 0, 2, 99, 99 };   // polygon 1's joins are places 2 to 98, all past the end of joinOther
		int const joinOthers[] = { 1, 0 };
		int *const firstAtTheWall = reinterpret_cast<int *>(MemoryEndingAtAWall(sizeof(firstJoins) + sizeof(joinOthers)));
		int *const othersAtTheWall = firstAtTheWall + 4;
		memcpy(firstAtTheWall, firstJoins, sizeof(firstJoins));
		memcpy(othersAtTheWall, joinOthers, sizeof(joinOthers));
		AlienWaysClaims tinyMap;
		tinyMap.Reset(3);
		std::vector<int> const wayOne = { 1 };
		Check(tinyMap.Add(0, wayOne.data(), 1, firstAtTheWall, 4, othersAtTheWall, 2) == kAlienWaysClaimsAdded && tinyMap.ClaimsOn(1) == 2 && tinyMap.ClaimsOn(0) == 0, "a join's place past the end of the join list is never read");
		Check(tinyMap.NumbersLeftOut() == 97, "and each place past it is counted as left out");
	}

	{
		int const joinOthers[] = { 7, -2 };          // neighbours that aren't polygons of the map
		int *const othersAtTheWall = reinterpret_cast<int *>(MemoryEndingAtAWall(sizeof(joinOthers)));
		memcpy(othersAtTheWall, joinOthers, sizeof(joinOthers));
		int const firstJoins[] = { 0, 2, 2, 2 };
		AlienWaysClaims tinyMap;
		tinyMap.Reset(3);
		std::vector<int> const wayZero = { 0 };
		Check(tinyMap.Add(0, wayZero.data(), 1, firstJoins, 4, othersAtTheWall, 2) == kAlienWaysClaimsAdded && tinyMap.ClaimsOn(0) == 2 && tinyMap.NumbersLeftOut() == 2, "a neighbour that isn't a polygon of the map is left out and counted");
	}

	{
		// A first-join list too short for the polygon: its end would be read past the list's end, which sits at the wall
		int *const firstAtTheWall = reinterpret_cast<int *>(MemoryEndingAtAWall(2 * sizeof(int)));
		firstAtTheWall[0] = 0;
		firstAtTheWall[1] = 0;
		int const joinOthers[] = { 0 };
		AlienWaysClaims tinyMap;
		tinyMap.Reset(3);
		std::vector<int> const wayTwo = { 2 };
		Check(tinyMap.Add(0, wayTwo.data(), 1, firstAtTheWall, 2, joinOthers, 1) == kAlienWaysClaimsAdded && tinyMap.ClaimsOn(2) == 2 && tinyMap.NumbersLeftOut() == 1, "a polygon whose place in the first-join list isn't there is claimed on its own, and counted");
	}

	// One alien can't hold more than the most
	{
		size_t const many = kAlienWaysClaimsMostPerAlien + 10;
		std::vector<int> way, noJoins(many + 2, 0);

		for (size_t p = 0; p < many; p++)
		{
			way.push_back(static_cast<int>(p));
		}

		AlienWaysClaims big;
		big.Reset(many);
		int const none = 0;
		Check(big.Add(0, way.data(), way.size(), noJoins.data(), noJoins.size(), &none, 0) == kAlienWaysClaimsAddedCutShort, "a way with more polygons than one alien can hold says it was cut short");
		int total = 0;

		for (size_t p = 0; p < many; p++)
		{
			total += big.ClaimsOn(static_cast<int>(p));
		}

		Check(total == 2 * static_cast<int>(kAlienWaysClaimsMostPerAlien) && big.ClaimsOn(0) == 2 && big.ClaimsOn(static_cast<int>(many) - 1) == 0, "and holds the way's first polygons, as many as it can");
	}
}

static void TheLevelsEnd()
{
	puts("a level's end, and a map too big");

	Joins const joins = TheMap();
	AlienWaysClaims claims;
	claims.Reset(15);
	Claim(claims, joins, 0, { 2, 3, 4 });
	claims.Clear();
	Check(claims.Polygons() == 0 && claims.ClaimsOn(3) == 0 && claims.PriceFactor(3, 1) == 1.0 && claims.OwnClaimOn(0, 3) == 0, "the level's end lets every claim go");
	Check(Claim(claims, joins, 0, { 2 }) == kAlienWaysClaimsNoMap && claims.ClaimsOn(2) == 0, "and nothing can be claimed until the next map");

	Check(claims.Reset(15) && Counts(claims) == std::vector<int>(15, 0) && claims.OwnClaimOn(0, 3) == 0, "the next map starts with nothing claimed");
	Check(!claims.Reset(kAlienWaysClaimsMostPolygons + 1) && claims.Polygons() == 0, "a map past the most is refused, and nothing is kept");
	Check(Claim(claims, joins, 0, { 2 }) == kAlienWaysClaimsNoMap, "so nothing can be claimed on it");
}

int main()
{
	TheClaims();
	TheLimits();
	TheLevelsEnd();

	printf("\n%s: %d check(s) failed\n", checksFailed ? "FAILED" : "PASSED", checksFailed);
	return checksFailed ? 1 : 0;
}
