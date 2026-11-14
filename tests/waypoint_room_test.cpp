// Tests the room an alien has to aim for inside a waypoint (3dc/waypoint_room.h). Written 2026-11-14 by Melanikus for the AvP port.
//
// WHY: an alien sent to a big waypoint aims for a random place inside it, cut down to fit the room on each axis by dividing by the room.
// The level reader now refuses a box with a room of 0, worked out at the level's scale as the game works it out, and the game takes the
// place without dividing by a room under 1.
//
// WHAT:
// - the room waypoint_room.h works out, against a copy of the game's own lines (TheGamesRoom below): for a box with no size on an axis at
//   its centre, as in three of the game's own level files (a room of -1), for boxes with a room of 0, and for many thousands of others, at
//   the level scales the game uses;
// - that a scale or a corner the sums can't use is refused, and that "can't use" is asked of the number's bits;
// - the place in the room, for rooms of 1 and more as before, and for rooms under 1, where it never divides;
// - by reading the source, that the game's lines are still the ones copied here, and that the game takes the place through the rule.
// The level reader's refusal itself is tested in rebeccas_test, section 41. tests/takeouts/waypoint_room_test.json proves each check can
// fail.

#include "waypoint_room.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include <fstream>
#include <sstream>
#include <string>

static int checks = 0;
static int failures = 0;

static void Check(bool good, const std::string &what)
{
	checks++;

	if (!good)
	{
		failures++;
		printf("  FAILED  %s\n", what.c_str());
	}
	else
	{
		printf("  ok  %s\n", what.c_str());
	}
}

// A file of the game's source, found from this test's own place, with its carriage returns taken out
static std::string ReadSource(const char *name)
{
	const std::string path = __FILE__;
	const std::string::size_type tests = path.rfind("tests");

	if (tests == std::string::npos)
	{
		return "";
	}

	std::ifstream in((path.substr(0, tests) + name).c_str(), std::ios::binary);
	std::stringstream all;
	all << in.rdbuf();
	std::string text = all.str();
	std::string plain;

	for (char c : text)
	{
		if (c != '\r')
		{
			plain += c;
		}
	}

	return plain;
}

static int Times(const std::string &text, const char *what)
{
	int times = 0;
	std::string const wanted(what);

	for (std::string::size_type at = text.find(wanted); at != std::string::npos; at = text.find(wanted, at + wanted.size()))
	{
		times++;
	}

	return times;
}

// One function of a source file: from its first line (with its opening brace, so a declaration of it doesn't count) to the brace that
// closes it, the first one alone at the start of a line. Empty if the first line isn't there. A newline goes on the end first, because
// GetTargetPositionInWaypoint is the last thing in bh_waypoint.cpp, and the file ends on its brace.
static std::string FunctionText(const std::string &source, const char *firstLine)
{
	std::string const text = source + "\n";
	std::string::size_type const start = text.find(firstLine);
	std::string::size_type const close = start == std::string::npos ? std::string::npos : text.find("\n}\n", start);

	return close == std::string::npos ? std::string() : text.substr(start, close + 3 - start);
}

// A double made from its bits, so this test can make infinity and "not a number" without any sum that makes them
static double FromBits(uint64_t bits)
{
	double number;
	memcpy(&number, &bits, sizeof(number));
	return number;
}

// The game's own lines for one axis, copied as they are. The source checks at the end make sure the game's lines are still these.
// - a corner times the scale (ChunkVectorInt's operator*, chunk_type.cpp): v.x =(int) (a.x * s);
// - the level setup (copy_rif_data, projload.cpp): wv->centre=cw->centre*local_scale; wv->min_extents=cw->min*local_scale;
//   SubVector(&wv->centre,&wv->min_extents); wv->max_extents=cw->max*local_scale; SubVector(&wv->centre,&wv->max_extents);
//   wv->max_extents.vx--; where SubVector takes its first from its second (maths.cpp);
// - GetTargetPositionInWaypoint (bh_waypoint.cpp): minx=waypoint->centre.vx+((waypoint->min_extents.vx*3)>>2); the same for maxx with
//   max_extents; rangex=maxx-minx;
// The game moves the centre once more before GetTargetPositionInWaypoint adds it, and that makes no difference to the room, so here it
// adds the scaled centre. Only boxes whose sums stay inside the game's whole numbers are given to it, so its sums never wrap round.
static int TheGamesRoom(int lowest, int highest, int centre, double scale)
{
	int const wvCentre = (int)(centre * scale);
	int minExtents = (int)(lowest * scale);
	minExtents -= wvCentre;
	int maxExtents = (int)(highest * scale);
	maxExtents -= wvCentre;
	maxExtents--;

	int const minx = wvCentre + ((minExtents * 3) >> 2);
	int const maxx = wvCentre + ((maxExtents * 3) >> 2);

	return maxx - minx;
}

// The room from waypoint_room.h, or a number no room can be, to show it was refused
static int64_t RoomFromTheRule(int lowest, int highest, int centre, double scale)
{
	int32_t room = 0;
	return WaypointRoom(lowest, highest, centre, scale, &room) ? room : INT64_MIN;
}

int main()
{
	setvbuf(stdout, NULL, _IONBF, 0);

	// 1, the game's own levels' 0.9 and 0.8 (ENVSDSCL), and others a level may give
	double const scales[] = { 1.0, 0.9, 0.8, 0.75, 0.5 };
	int const scaleCount = (int)(sizeof(scales) / sizeof(scales[0]));

	puts("The room, worked out as the game works it out:");

	// A box with no size on an axis, exactly at its centre: the kind three of the game's own level files have
	{
		int const centres[] = { 0, 1, -3, 1234, -98765, 3000000 };
		bool allMinusOne = true;
		bool sameAsTheGame = true;

		for (int s = 0; s < scaleCount; s++)
		{
			for (int c = 0; c < (int)(sizeof(centres) / sizeof(centres[0])); c++)
			{
				int const at = centres[c];
				allMinusOne = allMinusOne && RoomFromTheRule(at, at, at, scales[s]) == -1;
				sameAsTheGame = sameAsTheGame && TheGamesRoom(at, at, at, scales[s]) == -1;
			}
		}

		Check(allMinusOne, "a box with no size on an axis, at its centre, has a room of -1, as in the game's own levels, at every scale");
		Check(sameAsTheGame, "and the game's own lines give the same");
	}

	// A box with no size on an axis, a little way from its centre: from 4 below it to 4 above, at scale 1
	{
		int const expected[9] = { -1, 0, -1, -1, -1, 0, -1, -1, -1 };
		bool ruleGivesThem = true;
		bool gameGivesThem = true;

		for (int away = -4; away <= 4; away++)
		{
			ruleGivesThem = ruleGivesThem && RoomFromTheRule(away, away, 0, 1.0) == expected[away + 4];
			gameGivesThem = gameGivesThem && TheGamesRoom(away, away, 0, 1.0) == expected[away + 4];
		}

		Check(ruleGivesThem, "a box with no size on an axis, 3 below its centre or 1 above, has a room of 0, and the rest from 4 below to 4 above -1");
		Check(gameGivesThem, "and the game's own lines give those same rooms");

		Check(RoomFromTheRule(2, 2, 0, 0.8) == 0 && TheGamesRoom(2, 2, 0, 0.8) == 0,
			"and at a level's own scale, with what's after the point cut off, one 2 above its centre has a room of 0 at 0.8, in both");
		Check(RoomFromTheRule(1, 1, 0, 0.8) == -1 && TheGamesRoom(1, 1, 0, 0.8) == -1,
			"while one 1 above it, with a room of 0 at scale 1, has -1 at 0.8, in both");
	}

	// Boxes of every small size and place, at every scale, and some big ones
	{
		int const centres[] = { -7, 0, 5, 40000 };
		int boxes = 0;
		int different = 0;

		for (int s = 0; s < scaleCount; s++)
		{
			for (int c = 0; c < (int)(sizeof(centres) / sizeof(centres[0])); c++)
			{
				for (int lowest = -60; lowest <= 60; lowest++)
				{
					for (int highest = lowest - 3; highest <= lowest + 60; highest++)
					{
						boxes++;
						different += RoomFromTheRule(lowest, highest, centres[c], scales[s]) != TheGamesRoom(lowest, highest, centres[c], scales[s]) ? 1 : 0;
					}
				}
			}
		}

		int const big[][3] = { { -1000, 1000, 0 }, { -1000000, 1000000, 12345 }, { 50000000, 50000001, 50000000 }, { -300000000, 300000000, -5 } };

		for (int s = 0; s < scaleCount; s++)
		{
			for (int b = 0; b < (int)(sizeof(big) / sizeof(big[0])); b++)
			{
				boxes++;
				different += RoomFromTheRule(big[b][0], big[b][1], big[b][2], scales[s]) != TheGamesRoom(big[b][0], big[b][1], big[b][2], scales[s]) ? 1 : 0;
			}
		}

		Check(boxes > 150000 && different == 0, "and so do " + std::to_string(boxes) + " boxes of all sizes, places and scales: " + std::to_string(different) + " differ");
		Check(RoomFromTheRule(-1000, 1000, 0, 1.0) == 1499, "a box 2,000 wide has a room of 1,499, three quarters of it, less a step");
	}

	// The widest box the game's numbers hold: the sums wrap round as the game's do. Worked out by hand: the lowest, 2^31 below the centre,
	// times 3 wraps round to -2^31, and a quarter of that is -536,870,912; the highest, less 1, is 2,147,483,646, times 3 wraps round to
	// 2,147,483,642, and a quarter of that is 536,870,910. The room between them is 1,073,741,822.
	Check(RoomFromTheRule(INT_MIN, INT_MAX, 0, 1.0) == 1073741822, "the widest box there is wraps round in the sums, as it does in the game's");

	puts("What the sums refuse:");

	{
		double const notFinite[] =
		{
			FromBits(0x7FF0000000000000ull), FromBits(0xFFF0000000000000ull),   // infinity, and below every number
			FromBits(0x7FF8000000000000ull), FromBits(0x7FF0000000000001ull), FromBits(0xFFF8000000000000ull),   // three "not a number"s
		};
		bool noneUsable = true;
		bool boxesRefused = true;

		for (int i = 0; i < (int)(sizeof(notFinite) / sizeof(notFinite[0])); i++)
		{
			noneUsable = noneUsable && !WaypointNumberIsFinite(notFinite[i]);
			boxesRefused = boxesRefused && RoomFromTheRule(0, 0, 0, notFinite[i]) == INT64_MIN && RoomFromTheRule(-1000, 1000, 0, notFinite[i]) == INT64_MIN;
		}

		Check(!WaypointNumberIsFinite(notFinite[0]) && !WaypointNumberIsFinite(notFinite[1]), "infinity, either way, isn't a scale the sums can use");
		Check(noneUsable, "nor is any 'not a number'");
		Check(boxesRefused, "so a box at such a scale is refused, even one with all its corners at 0");

		double const finite[] = { 1.0, 0.8, -0.5, 0.0, FromBits(0x7FEFFFFFFFFFFFFFull), FromBits(0x0000000000000001ull) };
		bool allUsable = true;

		for (int i = 0; i < (int)(sizeof(finite) / sizeof(finite[0])); i++)
		{
			allUsable = allUsable && WaypointNumberIsFinite(finite[i]);
		}

		Check(allUsable, "every finite number is one, from the largest there is to the smallest above 0");
	}

	{
		int32_t kept = 0;
		Check(WaypointScaledCorner(INT_MAX, 1.0, &kept) && kept == INT_MAX && WaypointScaledCorner(INT_MIN, 1.0, &kept) && kept == INT_MIN,
			"a corner as far as the game's whole numbers go, either way, is kept as it is at scale 1");
		Check(WaypointScaledCorner(-7, 0.5, &kept) && kept == -3, "and a corner at a scale keeps the whole number, with what's after the point cut off");
		Check(!WaypointScaledCorner(2000000000, 1.5, &kept) && !WaypointScaledCorner(INT_MIN, 1.5, &kept),
			"one the scale makes bigger than the game's whole numbers go, either way, is refused");
		Check(!WaypointScaledCorner(2, FromBits(0x7FEFFFFFFFFFFFFFull), &kept), "and one the scale makes infinite");
		Check(RoomFromTheRule(0, 2000000000, 0, 1.5) == INT64_MIN, "so a box with such a corner is refused");
	}

	puts("The place in the room:");

	{
		int const randoms[] = { 0, 1, 2, 3, 12345, 65535, 1000000007, INT_MAX };
		int const randomCount = (int)(sizeof(randoms) / sizeof(randoms[0]));

		bool zeroGivesLowest = true;
		bool underOneGivesLowest = true;
		bool minusOneAsBefore = true;
		bool oneOrMoreAsBefore = true;

		int const underOne[] = { -1, -2, -3, -1000, INT_MIN };
		int const oneOrMore[] = { 1, 2, 3, 4, 1499, 65536, 1073741822, INT_MAX };

		for (int r = 0; r < randomCount; r++)
		{
			int const random = randoms[r];

			zeroGivesLowest = zeroGivesLowest && WaypointPlaceInRoom(random, 0) == 0;

			for (int u = 0; u < (int)(sizeof(underOne) / sizeof(underOne[0])); u++)
			{
				underOneGivesLowest = underOneGivesLowest && WaypointPlaceInRoom(random, underOne[u]) == 0;
			}

			minusOneAsBefore = minusOneAsBefore && WaypointPlaceInRoom(random, -1) == random % -1;

			for (int o = 0; o < (int)(sizeof(oneOrMore) / sizeof(oneOrMore[0])); o++)
			{
				int const room = oneOrMore[o];
				int const place = WaypointPlaceInRoom(random, room);
				oneOrMoreAsBefore = oneOrMoreAsBefore && place == random % room && place >= 0 && place < room;
			}
		}

		Check(zeroGivesLowest, "a room of 0 gives the lowest place, and this test is still running, so nothing divided by it");
		Check(underOneGivesLowest, "and so does every room under 1, down to the lowest there is");
		Check(minusOneAsBefore, "for a room of -1, as in the game's own levels, that's the place the game always gave");
		Check(oneOrMoreAsBefore, "a room of 1 or more gives what's left over after dividing by it, as before: from 0 to one less than the room");
	}

	puts("Where the game uses it:");

	{
		std::string const bh = ReadSource("3dc/avp/bh_waypoint.cpp");
		std::string const target = FunctionText(bh, "void GetTargetPositionInWaypoint(WAYPOINT_VOLUME *waypoint,VECTORCH *output) {");

		Check(!target.empty(), "bh_waypoint.cpp is there to read, with GetTargetPositionInWaypoint in it");
		Check(Times(bh, "#include \"waypoint_room.h\"") == 1, "it uses the rule");
		Check(Times(target, "minx=waypoint->centre.vx+((waypoint->min_extents.vx*3)>>2);") == 1
			&& Times(target, "maxx=waypoint->centre.vx+((waypoint->max_extents.vx*3)>>2);") == 1 && Times(target, "rangex=maxx-minx;") == 1
			&& Times(target, "miny=waypoint->centre.vy+((waypoint->min_extents.vy*3)>>2);") == 1
			&& Times(target, "maxy=waypoint->centre.vy+((waypoint->max_extents.vy*3)>>2);") == 1 && Times(target, "rangey=maxy-miny;") == 1
			&& Times(target, "minz=waypoint->centre.vz+((waypoint->min_extents.vz*3)>>2);") == 1
			&& Times(target, "maxz=waypoint->centre.vz+((waypoint->max_extents.vz*3)>>2);") == 1 && Times(target, "rangez=maxz-minz;") == 1,
			"its three quarters of each corner, and the room between them, are still the lines copied here");
		Check(Times(target, "output->vx=minx+WaypointPlaceInRoom(FastRandom(),rangex);") == 1
			&& Times(target, "output->vy=miny+WaypointPlaceInRoom(FastRandom(),rangey);") == 1
			&& Times(target, "output->vz=minz+WaypointPlaceInRoom(FastRandom(),rangez);") == 1,
			"and it takes the place on each axis through the rule, with a random number for each");
		Check(Times(target, "%") == 0, "and divides by nothing itself");

		std::string const projload = ReadSource("3dc/avp/win95/projload.cpp");
		Check(Times(projload, "wv->centre=cw->centre*local_scale;") == 1 && Times(projload, "wv->min_extents=cw->min*local_scale;") == 1
			&& Times(projload, "SubVector(&wv->centre,&wv->min_extents);") == 1 && Times(projload, "wv->max_extents=cw->max*local_scale;") == 1
			&& Times(projload, "SubVector(&wv->centre,&wv->max_extents);") == 1 && Times(projload, "wv->max_extents.vx--;") == 1
			&& Times(projload, "wv->max_extents.vy--;") == 1 && Times(projload, "wv->max_extents.vz--;") == 1,
			"the level setup still scales each corner, keeps them as how far from the centre, and takes 1 off the highest, as copied here");
		Check(Times(projload, "local_scale = GlobalScale;") == 1 && Times(projload, "local_scale *= ((Environment_Scale_Chunk *)pChunk)->scale;") == 1,
			"and the level's scale is still GlobalScale times the scale its environment gives");
		Check(Times(ReadSource("3dc/avp/equates.h"), "#define GlobalScale 1\n") == 1, "and GlobalScale is still 1, which the level reader takes it to be");

		std::string const types = ReadSource("3dc/win95/chunk_type.cpp");
		Check(Times(types, "v.x =(int) (a.x * s);") == 1 && Times(types, "v.y =(int) (a.y * s);") == 1 && Times(types, "v.z =(int) (a.z * s);") == 1,
			"a corner times the scale still keeps the whole number, with what's after the point cut off");

		std::string const subVector = FunctionText(ReadSource("3dc/maths.cpp"), "void SubVector(VECTORCH *v1, VECTORCH *v2)\n{");
		Check(Times(subVector, "v2->vx -= v1->vx;") == 1 && Times(subVector, "v2->vy -= v1->vy;") == 1 && Times(subVector, "v2->vz -= v1->vz;") == 1,
			"and SubVector still takes its first from its second");
	}

	printf("%s: %d checks, %d failed\n", failures ? "SOME CHECKS FAILED" : "All checks passed", checks, failures);
	return failures ? 1 : 0;
}
