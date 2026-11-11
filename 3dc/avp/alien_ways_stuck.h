// What the log says about an alien the alien ways guide finds stuck, or lets go. Written 2026-11-08 by Melanikus for the AvP port (#109).
//
// WHY: in the user's first play an alien sat still on its way, and every 5 seconds the guide said it was stuck, dropped its way, and found
// it the same one again (Primary's read of that play's log, 2026-11-08). Reading the code gives several things that could hold an alien:
// its speed coming out as nothing once its own down is taken off, an obstruction test sending it to Avoidance, another state taking it off
// its way, or its gravity pointing somewhere it isn't. This says which, from the game, at the moment it happens:
//
//     alien ways: alien 3 stuck at (-4.6, 1.9, 19.5), heading for (-6.4, 0.4, 19.5) (2.4 m away), with no headway for 1.5 s: the game's own
//          code has it for 3.0 s. It's in Approach now; over the last 90 frames: Approach 41, Avoidance 49. It's moving at 0.0 m/s, standard
//          gravity off, touching the floor yes; its gravity (0.00, 1.00, 0.00) and its body's down (0.00, 1.00, 0.00), 0 degrees apart;
//          obstructed 11 frames running; in module genent03; the game's own goal (1.4, 1.9, 27.3)
//
//     alien ways: an alien at (-4.6, 1.9, 19.5) stopped being guided, in Avoidance
//
// The first part of the stuck line is alien_ways.cpp's, and this gives the rest. The game's numbers are copied into plain ones first
// (AlienWaysStuckFacts), so this knows nothing of the game and tests/alien_ways_stuck_test.cpp can try every word of it.

#ifndef ALIEN_WAYS_STUCK_H
#define ALIEN_WAYS_STUCK_H

#include <math.h>
#include <stdio.h>

#include <string>

// The alien's states, in the order bh_alien.h's AlienBhState lists them. tests/alien_ways_stuck_test.cpp checks the order against that file.
static const char *const kAlienWaysStateNames[] = { "Wait", "Approach", "Hunt", "Wander", "Retreat", "Attack", "Avoidance", "Dying", "Pounce",
	"Jump", "Dormant", "Awakening", "Taunting" };
static const int kAlienWaysStateKinds = static_cast<int>(sizeof(kAlienWaysStateNames) / sizeof(kAlienWaysStateNames[0]));

// How many frames of an alien's states are kept: a second and a half at 60 a second, as long as the guide waits before calling it stuck
static const int kAlienWaysStateFrames = 90;

// How many lines a level gets about aliens let go. An alien can be let go every few seconds, so after this many the rest aren't written.
static const int kAlienWaysMostLetGoLines = 100;

// A state's name, or the words for a number that isn't one of them
static inline const char *AlienWaysStuck_StateName(int state)
{
	return state >= 0 && state < kAlienWaysStateKinds ? kAlienWaysStateNames[state] : "an unknown state";
}

// The last frames' states of one alien. A state that isn't one of the list is kept as kAlienWaysStateKinds, and counted as unknown.
struct AlienWaysStateHistory
{
	unsigned char states[kAlienWaysStateFrames] = {};
	int next = 0;      // where the next frame's state goes
	int filled = 0;    // how many frames are kept, up to kAlienWaysStateFrames
};

static inline void AlienWaysStateHistory_Note(AlienWaysStateHistory *history, int state)
{
	history->states[history->next] = static_cast<unsigned char>(state >= 0 && state < kAlienWaysStateKinds ? state : kAlienWaysStateKinds);
	history->next = (history->next + 1) % kAlienWaysStateFrames;

	if (history->filled < kAlienWaysStateFrames)
	{
		history->filled++;
	}
}

// "Approach 41, Avoidance 49": how many of the kept frames were in each state, in the list's order, leaving out the states it was never in
static inline std::string AlienWaysStateHistory_Counts(const AlienWaysStateHistory &history)
{
	int counts[kAlienWaysStateKinds + 1] = {};

	for (int i = 0; i < history.filled; i++)
	{
		counts[history.states[i] <= kAlienWaysStateKinds ? history.states[i] : kAlienWaysStateKinds]++;
	}

	std::string words;
	char part[48];

	for (int s = 0; s <= kAlienWaysStateKinds; s++)
	{
		if (counts[s] == 0)
		{
			continue;
		}

		snprintf(part, sizeof(part), "%s%s %d", words.empty() ? "" : ", ", s < kAlienWaysStateKinds ? kAlienWaysStateNames[s] : "unknown", counts[s]);
		words += part;
	}

	return words.empty() ? std::string("none kept") : words;
}

// What the game says about a stuck alien, copied out of its own blocks by alien_ways.cpp
struct AlienWaysStuckFacts
{
	bool found = false;               // it's still one of the game's live aliens; nothing else here counts if not
	int stateNow = -1;
	AlienWaysStateHistory history;
	double velocity[3] = {};          // mm a second
	bool standardGravity = false;
	bool onFloor = false;
	double gravity[3] = {};           // its gravity's direction, 65536 long
	double bodyDown[3] = {};          // its body's own down (OrientMat row 2), 65536 long
	int obstructedFrames = 0;         // the obstruction test's count of frames in a row
	const char *module = 0;           // the name of the module it's in, or none
	bool hasGoal = false;
	double goal[3] = {};              // the point the game's own code picked for it, in mm
};

// A direction of length 65536, as two-place numbers
static inline std::string AlienWaysStuck_Direction(const double d[3])
{
	char text[64];

	snprintf(text, sizeof(text), "(%.2f, %.2f, %.2f)", d[0] / 65536.0, d[1] / 65536.0, d[2] / 65536.0);
	return text;
}

// The angle between two directions in whole degrees, or -1 if either has no length
static inline int AlienWaysStuck_DegreesApart(const double a[3], const double b[3])
{
	double const lengths = sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]) * sqrt(b[0] * b[0] + b[1] * b[1] + b[2] * b[2]);

	if (!(lengths > 0.0))
	{
		return -1;
	}

	double cosine = (a[0] * b[0] + a[1] * b[1] + a[2] * b[2]) / lengths;
	cosine = cosine > 1.0 ? 1.0 : (cosine < -1.0 ? -1.0 : cosine);

	return static_cast<int>(acos(cosine) * 180.0 / 3.14159265358979323846 + 0.5);
}

// The rest of the stuck line, after alien_ways.cpp's first part, as in the example at the top
static inline std::string AlienWaysStuck_Details(const AlienWaysStuckFacts &facts)
{
	if (!facts.found)
	{
		return ". It's no longer one of the game's live aliens, so nothing more can be said";
	}

	double const speed = sqrt(facts.velocity[0] * facts.velocity[0] + facts.velocity[1] * facts.velocity[1] + facts.velocity[2] * facts.velocity[2]) / 1000.0;
	int const apart = AlienWaysStuck_DegreesApart(facts.gravity, facts.bodyDown);
	char text[512];

	snprintf(text, sizeof(text), ". It's in %s now; over the last %d frames: %s. It's moving at %.1f m/s, standard gravity %s, touching the floor %s; "
		"its gravity %s and its body's down %s, ",
		AlienWaysStuck_StateName(facts.stateNow), facts.history.filled, AlienWaysStateHistory_Counts(facts.history).c_str(), speed,
		facts.standardGravity ? "on" : "off", facts.onFloor ? "yes" : "no", AlienWaysStuck_Direction(facts.gravity).c_str(),
		AlienWaysStuck_Direction(facts.bodyDown).c_str());

	std::string line = text;

	if (apart < 0)
	{
		line += "which can't be compared";
	}
	else
	{
		snprintf(text, sizeof(text), "%d degrees apart", apart);
		line += text;
	}

	snprintf(text, sizeof(text), "; obstructed %d frames running; in module %s", facts.obstructedFrames, facts.module && facts.module[0] ? facts.module : "(none)");
	line += text;

	if (facts.hasGoal)
	{
		snprintf(text, sizeof(text), "; the game's own goal (%.1f, %.1f, %.1f)", facts.goal[0] / 1000.0, facts.goal[1] / 1000.0, facts.goal[2] / 1000.0);
		line += text;
	}

	return line;
}

// The line for an alien the guide stops guiding, which happens when it leaves the state that asks for a way
static inline std::string AlienWaysStuck_LetGoLine(int stateNow, double x, double y, double z)
{
	char text[160];

	snprintf(text, sizeof(text), "alien ways: an alien at (%.1f, %.1f, %.1f) stopped being guided, in %s", x / 1000.0, y / 1000.0, z / 1000.0,
		AlienWaysStuck_StateName(stateNow));
	return text;
}

// The line once a level has had kAlienWaysMostLetGoLines of them
static inline std::string AlienWaysStuck_LetGoLinesStop()
{
	char text[128];

	snprintf(text, sizeof(text), "alien ways: %d aliens let go in this level, so no more lines about that until the next one", kAlienWaysMostLetGoLines);
	return text;
}

#endif
