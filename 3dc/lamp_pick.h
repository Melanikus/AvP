// Which other marines' lamps are drawn, and when an NPC marine turns his on. Written 2026-11-07 by Melanikus for the AvP port.
// Only numbers and rules, with nothing from the running game, so a test can check them without it.

#ifndef LAMP_PICK_H
#define LAMP_PICK_H

// How bright a place is, the way the game measures it (LightIntensityAtPoint, in kshape.cpp): 65536 is as bright as it counts, and nothing
// reads darker than 2560, or a level's own least light if that's more.
//
// Below this a place counts as dark, and an NPC marine standing there turns his lamp on. It's the game's own line for darkness: an alien
// standing in less light than this can't be seen by a marine (the 5000 in the alien's stealth check, in player.cpp). It's a first guess.
// A level where nowhere is this dark would never see an NPC marine's lamp. Measuring the light around a level shows which those are.
static const int kLampDarkBelow = 5000;

inline bool LampPick_IsDark(int light)
{
	return light < kLampDarkBelow;
}

// ---- When an NPC marine turns his lamp on: the user's "only in dark places"
//
// Above this he turns it off again. It's higher than kLampDarkBelow, so a marine standing right on the line doesn't switch it every time he decides.
static const int kLampLightAbove = 6500;

// How often he decides, in seconds. In between, his lamp stays as it is, so a flickering light nearby can't make it blink.
static const float kLampDecideEverySeconds = 2.0f;

// How many times in a row he has to find it clearly light before he turns it off. Once could be someone else's gun, a flare or a blast going off
// near him just as he decides, which would put his lamp out in a dark room in the middle of a fight.
static const int kLampLightTimesToTurnOff = 2;

// Whether his lamp is on once he's decided: on at once in the dark, and off only once he's found it clearly light kLampLightTimesToTurnOff times in
// a row. timesLight is his count of those, kept between decisions, and this brings it up to date. It stops at kLampLightTimesToTurnOff, so it can't
// grow without end. While he's firing nothing changes, not even the count, because then the light where he stands is his own gun's flash.
inline bool LampPick_MarineWantsItOn(bool onNow, int light, bool firing, int *timesLight)
{
	if (firing)
	{
		return onNow;
	}

	if (light < kLampLightAbove)
	{
		*timesLight = 0;
	}
	else if (*timesLight < kLampLightTimesToTurnOff)
	{
		(*timesLight)++;
	}

	return onNow ? *timesLight < kLampLightTimesToTurnOff : LampPick_IsDark(light);
}

// ---- Which lamps are shown
//
// Every other marine with his lamp on could be shown, but each one shown costs time every frame, so only the nearest few are. The lamps
// come from other machines too, so however many say they're on, no more than this are ever shown.

// How many other marines' lamps are shown at once, nearest first. A starting point, not a limit anyone chose: the user's ruling is "more, if the
// measurement allows", so this is raised as far as frame times measured with more and more marines' lamps in one room show it can be, with
// lights added per pixel.
static const int kLampMostShown = 6;

// How many of those can be real lights on a frame, with lights added per pixel. A real light casts shadows, and that's what costs. The same kind of number:
// a starting point, to be raised as far as a measurement allows. Any beyond these are a beam only.
static const int kLampMostReal = 4;

// How many are looked at in a frame at most. Any beyond this, which no level has, aren't shown.
static const int kLampMostCandidates = 64;

// A lamp that's shown keeps its place until one that isn't is nearer than it by this much, in millimetres. Without it, two lamps about as
// far away as each other would take turns, a frame each.
static const int kLampKeepsItsPlaceBy = 2000;

// How long a lamp takes to come fully on, or go fully off, in seconds. However often a lamp is switched, it can't flash faster than this.
static const float kLampFadeSeconds = 0.3f;

// One lamp that could be shown this frame: whose it is, as a number that stays the same from frame to frame, and how far it is from the camera.
struct LampCandidate
{
	unsigned int owner;
	int distance;
};

// Whether this owner is in the list
inline bool LampPick_Has(const unsigned int *owners, int count, unsigned int owner)
{
	for (int i = 0; i < count; i++)
	{
		if (owners[i] == owner)
		{
			return true;
		}
	}

	return false;
}

// Chooses which lamps are shown this frame: at most `most`, nearest first, and never more than kLampMostShown. One shown last frame keeps
// its place unless a lamp not shown is nearer than it by kLampKeepsItsPlaceBy. Writes the owners shown into `shown`, which needs room for
// kLampMostShown, and gives back how many. An owner listed twice counts once.
inline int LampPick_Choose(const LampCandidate *candidates, int count, const unsigned int *shownBefore, int countBefore, int most, unsigned int *shown)
{
	if (!candidates || !shown || count <= 0 || most <= 0)
	{
		return 0;
	}

	if (most > kLampMostShown)
	{
		most = kLampMostShown;
	}

	if (count > kLampMostCandidates)
	{
		count = kLampMostCandidates;
	}

	if (!shownBefore || countBefore < 0)
	{
		countBefore = 0;
	}

	// The candidates nearest first. A simple sort is enough for so few.
	LampCandidate sorted[kLampMostCandidates];

	for (int i = 0; i < count; i++)
	{
		int at = i;

		while (at > 0 && sorted[at - 1].distance > candidates[i].distance)
		{
			sorted[at] = sorted[at - 1];
			at--;
		}

		sorted[at] = candidates[i];
	}

	LampCandidate chosen[kLampMostShown];
	int numChosen = 0;

	// First, the ones shown last frame that are still here, nearest first
	for (int i = 0; i < count && numChosen < most; i++)
	{
		bool already = false;

		for (int j = 0; j < numChosen; j++)
		{
			already = already || chosen[j].owner == sorted[i].owner;
		}

		if (!already && LampPick_Has(shownBefore, countBefore, sorted[i].owner))
		{
			chosen[numChosen++] = sorted[i];
		}
	}

	// Then the rest, nearest first. One takes a free place, or else pushes out the furthest one shown, but only if it's nearer by the margin.
	for (int i = 0; i < count; i++)
	{
		bool already = false;

		for (int j = 0; j < numChosen; j++)
		{
			already = already || chosen[j].owner == sorted[i].owner;
		}

		if (already)
		{
			continue;
		}

		if (numChosen < most)
		{
			chosen[numChosen++] = sorted[i];
			continue;
		}

		int furthest = 0;

		for (int j = 1; j < numChosen; j++)
		{
			furthest = chosen[j].distance > chosen[furthest].distance ? j : furthest;
		}

		if ((long long)sorted[i].distance + kLampKeepsItsPlaceBy < (long long)chosen[furthest].distance)
		{
			chosen[furthest] = sorted[i];
		}
	}

	for (int i = 0; i < numChosen; i++)
	{
		shown[i] = chosen[i].owner;
	}

	return numChosen;
}

// A lamp's strength, from 0 to 1, moved towards 1 if it's shown and towards 0 if it isn't, by how long the frame took. A frame time that
// can't be right, below nothing or not a number at all, moves it not at all, and a long one at most all the way.
inline float LampPick_Fade(float strength, bool shown, float seconds)
{
	if (!(seconds > 0.0f))
	{
		seconds = 0.0f;
	}

	if (!(strength >= 0.0f))
	{
		strength = 0.0f;
	}

	float const step = seconds / kLampFadeSeconds;
	float const moved = shown ? strength + step : strength - step;

	return moved < 0.0f ? 0.0f : moved > 1.0f ? 1.0f : moved;
}

#endif
