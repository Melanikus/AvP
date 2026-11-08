// How far the aliens another machine runs jump on this one when their news comes, by the surface they're on: floors, walls or ceilings.
// Written 2026-11-08 by Melanikus for the AvP port, for #120 (aliens on walls and ceilings in co-op and skirmish, part of #109).
//
// WHY: in co-op and skirmish the host runs the aliens, and every other machine draws a "ghost" of each one. Between two messages about an
// alien, this machine guesses where it has got to, using its own physics: the ghost carries on the way it faces, at the speed the last
// message gave, sticking to whatever surface it touches (engine/floor-is-lava-coop.md). When the next message comes, the ghost is put
// where the host has the alien. How far it jumps then is how wrong the guess was. On a floor the guess has an easy job. On a wall or a
// ceiling it can go wrong in more ways (a turn it didn't know about, a change of surface), and this line shows how much worse it is:
//
//     net: alien ghosts in the last 10 s: on floors 120 news, a jump of 35 mm on average and 410 mm at most; on walls 40 news, 80 mm
//          and 900 mm; on ceilings 12 news, 60 mm and 300 mm; of 52 news while crawling, 3 found the ghost's own down over 45 degrees off
//
// "Down" is the way the alien's feet point: the surface it's on. While it crawls, the ghost keeps its own down from what it touches, and
// the last part of the line counts the times that had drifted far from the host's.
//
// It's counted, never written news by news, so a busy game can't fill the log: one line every 10 seconds at most, and none when no alien
// ghost had news. pldnet.cpp counts each message about an alien and writes the line. tests/alien_ghost_jumps_test.cpp tries all of this.

#ifndef ALIEN_GHOST_JUMPS_H
#define ALIEN_GHOST_JUMPS_H

#include <math.h>
#include <stdio.h>

#include <string>

// How often the line is written, in milliseconds
static const unsigned int kAlienGhostJumpsEveryMs = 10000;

// The cosine of 45 degrees, in the game's fixed-point units (65536 is 1): 46341. A down within 45 degrees of straight down is a floor's,
// within 45 degrees of straight up is a ceiling's, and anything between is a wall's. Two downs more than 45 degrees apart are "off".
static const int kAlienGhostCos45 = 46341;

// The surfaces, in the order the line names them
enum AlienGhostSurface
{
	kAlienGhostOnFloor,
	kAlienGhostOnWall,
	kAlienGhostOnCeiling,
	kAlienGhostSurfaces
};

// Which surface a down is on, from how much of it points down. In the game, a larger y is lower, so a floor's down has a y near +65536
// and a ceiling's near -65536.
static inline AlienGhostSurface AlienGhost_SurfaceOf(int downY)
{
	if (downY > kAlienGhostCos45)
	{
		return kAlienGhostOnFloor;
	}

	if (downY < -kAlienGhostCos45)
	{
		return kAlienGhostOnCeiling;
	}

	return kAlienGhostOnWall;
}

// How far apart two places are, in millimetres (the game's unit), from the three differences. Squared in 64 bits, so even the
// farthest two places a message can carry (8.2 km apart on each axis) can't overflow.
static inline unsigned int AlienGhost_JumpMm(int dx, int dy, int dz)
{
	double const squared = (double)((long long)dx * dx) + (double)((long long)dy * dy) + (double)((long long)dz * dz);

	return (unsigned int)(sqrt(squared) + 0.5);
}

// Whether two downs, each of length 65536, are more than 45 degrees apart. Their dot product is the cosine of the angle between them
// times 65536 times 65536, worked out in 64 bits.
static inline bool AlienGhost_DownIsOff(const int a[3], const int b[3])
{
	long long const dot = (long long)a[0] * b[0] + (long long)a[1] * b[1] + (long long)a[2] * b[2];

	return dot < (long long)kAlienGhostCos45 * 65536;
}

// What's counted until the next line
struct AlienGhostJumps
{
	bool counting = false;                         // false until the first news, and again after each line
	unsigned int startMs = 0;                      // when this count started
	unsigned int news[kAlienGhostSurfaces] = {};   // how many messages, for each surface
	unsigned long long totalMm[kAlienGhostSurfaces] = {};
	unsigned int mostMm[kAlienGhostSurfaces] = {};
	unsigned int crawling = 0;                     // how many of all those said the alien was crawling
	unsigned int downOff = 0;                      // and how many of those found the ghost's own down more than 45 degrees off
};

// Counts one message about an alien: the surface it puts the alien on, how far the ghost jumped, whether the alien is crawling, and
// whether the ghost's own down was off. nowMs is the time on this machine's millisecond clock.
static inline void AlienGhostJumps_Note(AlienGhostJumps *jumps, AlienGhostSurface surface, unsigned int jumpMm, bool crawling, bool downOff, unsigned int nowMs)
{
	if (surface < 0 || surface >= kAlienGhostSurfaces)
	{
		return;
	}

	if (!jumps->counting)
	{
		jumps->counting = true;
		jumps->startMs = nowMs;
	}

	jumps->news[surface]++;
	jumps->totalMm[surface] += jumpMm;

	if (jumpMm > jumps->mostMm[surface])
	{
		jumps->mostMm[surface] = jumpMm;
	}

	if (crawling)
	{
		jumps->crawling++;

		if (downOff)
		{
			jumps->downOff++;
		}
	}
}

// Whether the count has gone on long enough for a line. The clock's subtraction is unsigned, so it comes out right when the clock wraps
// round (every 49 days).
static inline bool AlienGhostJumps_LineIsDue(const AlienGhostJumps &jumps, unsigned int nowMs)
{
	return jumps.counting && nowMs - jumps.startMs >= kAlienGhostJumpsEveryMs;
}

// The line, as in the example at the top, or nothing when no news was counted. The count covers nowMs - startMs, said in whole seconds.
// Only the surfaces that had news are named. The first one named has its numbers in full words, the others in short.
static inline std::string AlienGhostJumps_Line(const AlienGhostJumps &jumps, unsigned int nowMs)
{
	if (!jumps.counting)
	{
		return std::string();
	}

	static const char *const kSurfaceNames[kAlienGhostSurfaces] = { "floors", "walls", "ceilings" };
	char part[200];

	snprintf(part, sizeof(part), "net: alien ghosts in the last %u s:", (nowMs - jumps.startMs + 500) / 1000);
	std::string line = part;
	bool first = true;

	for (int s = 0; s < kAlienGhostSurfaces; s++)
	{
		if (jumps.news[s] == 0)
		{
			continue;
		}

		// The average, rounded to the nearest millimetre
		unsigned int const average = (unsigned int)((jumps.totalMm[s] + jumps.news[s] / 2) / jumps.news[s]);

		if (first)
		{
			snprintf(part, sizeof(part), " on %s %u news, a jump of %u mm on average and %u mm at most", kSurfaceNames[s], jumps.news[s], average, jumps.mostMm[s]);
		}
		else
		{
			snprintf(part, sizeof(part), "; on %s %u news, %u mm and %u mm", kSurfaceNames[s], jumps.news[s], average, jumps.mostMm[s]);
		}

		line += part;
		first = false;
	}

	if (jumps.crawling > 0)
	{
		snprintf(part, sizeof(part), "; of %u news while crawling, %u found the ghost's own down over 45 degrees off", jumps.crawling, jumps.downOff);
		line += part;
	}

	return line;
}

#endif
