// Hudson's navigation: the sums, with nothing of the game in them, so that a test can run them. Written 2026-11-05 by Melanikus for the AvP port. hudson_nav.cpp says what navigation is.

#include "hudson_nav_plan.h"

#include <math.h>
#include <stdint.h>

// How much of a whole length is left along each axis after turning 45 degrees: one over the square root of two.
static int const kOneEighthOfATurn = 46341;

const char *const kHudsonNavRouteTooLong = "it's longer than any route can be";
const char *const kHudsonNavRouteEmpty = "it has no steps in it";
const char *const kHudsonNavRouteTooManySteps = "it has more steps than a route can have";
const char *const kHudsonNavRouteLineTooLong = "a line is longer than a line can be";
const char *const kHudsonNavRouteUnknownWord = "a line starts with a word that isn't walk, press, walkinto or other";
const char *const kHudsonNavRouteBadNumber = "a line's place isn't three whole numbers within the world";
const char *const kHudsonNavRouteBadName = "a line's name is empty, too long, or has a character a name can't have";

int HudsonNav_NextModule(const std::vector<std::vector<int> > &links, int from, const std::vector<char> &beenThere)
{
	int const count = (int)links.size();

	if (from < 0 || from >= count || beenThere.size() != links.size())
	{
		return -1;
	}

	// For each module reached, which neighbour of "from" the way there starts with. -1 means it hasn't been reached yet, and each module joins the list only once, so this always ends.
	std::vector<int> startedWith(links.size(), -1);
	std::vector<int> toLookAt;

	toLookAt.reserve(links.size());
	startedWith[from] = from;
	toLookAt.push_back(from);

	// The list only grows at its end, so the modules come in order of how far they are from where he stands: nearest first.
	for (size_t next = 0; next < toLookAt.size(); next++)
	{
		int const module = toLookAt[next];

		for (size_t i = 0; i < links[module].size(); i++)
		{
			int const neighbour = links[module][i];

			if (neighbour < 0 || neighbour >= count || startedWith[neighbour] != -1)
			{
				continue;
			}

			startedWith[neighbour] = (module == from) ? neighbour : startedWith[module];

			if (!beenThere[neighbour])
			{
				return startedWith[neighbour];
			}

			toLookAt.push_back(neighbour);
		}
	}

	return -1;
}

void HudsonNav_StartProgress(HudsonNavProgress *progress, int x, int y, int z)
{
	progress->x = x;
	progress->y = y;
	progress->z = z;
	progress->timeHere = 0;
}

bool HudsonNav_IsStuck(HudsonNavProgress *progress, int x, int y, int z, int frameTime)
{
	// As doubles, because the square of a long way wouldn't fit in a whole number, even 64 bits of one. They're exact for any distance near enough to matter here.
	double const dx = (double)x - progress->x;
	double const dy = (double)y - progress->y;
	double const dz = (double)z - progress->z;
	double const reach = (double)kHudsonNavProgressIsAtLeast * kHudsonNavProgressIsAtLeast;

	if (dx * dx + dy * dy + dz * dz >= reach)
	{
		HudsonNav_StartProgress(progress, x, y, z);
		return false;
	}

	// A frame can't take away from the time, and the time stops growing once it's said he's stuck, so it can't overflow however long this goes on.
	if (frameTime > 0 && progress->timeHere < kHudsonNavStuckAfter)
	{
		progress->timeHere += (frameTime < kHudsonNavStuckAfter) ? frameTime : kHudsonNavStuckAfter;
	}

	return progress->timeHere >= kHudsonNavStuckAfter;
}

bool HudsonNav_IsAtSpot(int dx, int dy, int dz, int within)
{
	double const x = dx;
	double const y = dy / 4.0;
	double const z = dz;

	return x * x + y * y + z * z <= (double)within * within;
}

bool HudsonNav_UprightTurn(int facingX, int facingZ, int *turn)
{
	double const x = facingX;
	double const z = facingZ;
	double const length = sqrt(x * x + z * z);

	if (length <= 0.0)
	{
		return false;
	}

	// Rounded, not cut off, so that it doesn't always come out a little short
	int const forwardX = (int)floor(x * kHudsonNavOneWhole / length + 0.5);
	int const forwardZ = (int)floor(z * kHudsonNavOneWhole / length + 0.5);

	// His right, then down, then forward. y goes down in this game, so straight down never changes, and his right is straight ahead turned a quarter of the way round.
	turn[0] = forwardZ;
	turn[1] = 0;
	turn[2] = -forwardX;
	turn[3] = 0;
	turn[4] = kHudsonNavOneWhole;
	turn[5] = 0;
	turn[6] = forwardX;
	turn[7] = 0;
	turn[8] = forwardZ;

	return true;
}

// One whole number, moving *place on past it and the spaces after it. False if it isn't one, or is outside the world.
static bool ReadPlaceNumber(const char **place, const char *end, int *number)
{
	const char *here = *place;
	bool const negative = here < end && *here == '-';

	if (negative)
	{
		here++;
	}

	if (here >= end || *here < '0' || *here > '9')
	{
		return false;
	}

	int64_t value = 0;

	while (here < end && *here >= '0' && *here <= '9')
	{
		value = value * 10 + (*here - '0');

		// stops at once, so that any number of digits can't overflow
		if (value > kHudsonNavFurthestPlace)
		{
			return false;
		}

		here++;
	}

	if (here < end && *here != ' ')
	{
		return false;
	}

	while (here < end && *here == ' ')
	{
		here++;
	}

	*number = (int)(negative ? -value : value);
	*place = here;
	return true;
}

static bool NameIsGood(const char *name, const char *end)
{
	size_t const length = (size_t)(end - name);

	if (length == 0 || length > kHudsonNavLongestStepName)
	{
		return false;
	}

	for (const char *c = name; c < end; c++)
	{
		bool const fine = (*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') || *c == ' ' || *c == '-' || *c == '_' || *c == '.';

		if (!fine)
		{
			return false;
		}
	}

	return true;
}

const char *HudsonNav_ReadRoute(const char *text, size_t size, std::vector<HudsonNavStep> *steps)
{
	steps->clear();

	if (size > kHudsonNavMostRouteBytes)
	{
		return kHudsonNavRouteTooLong;
	}

	const char *place = text;
	const char *const finish = text + size;

	while (place < finish)
	{
		const char *lineEnd = place;

		while (lineEnd < finish && *lineEnd != '\n')
		{
			lineEnd++;
		}

		const char *const nextLine = (lineEnd < finish) ? lineEnd + 1 : lineEnd;

		// A file written on Windows has this at the end of each line
		if (lineEnd > place && lineEnd[-1] == '\r')
		{
			lineEnd--;
		}

		if ((size_t)(lineEnd - place) > kHudsonNavLongestRouteLine)
		{
			return kHudsonNavRouteLineTooLong;
		}

		if (lineEnd == place || *place == '#')
		{
			place = nextLine;
			continue;
		}

		if (steps->size() >= kHudsonNavMostRouteSteps)
		{
			return kHudsonNavRouteTooManySteps;
		}

		const char *wordEnd = place;

		while (wordEnd < lineEnd && *wordEnd != ' ')
		{
			wordEnd++;
		}

		std::string const word(place, wordEnd);
		const char *rest = wordEnd;

		while (rest < lineEnd && *rest == ' ')
		{
			rest++;
		}

		HudsonNavStep step;

		step.x = step.y = step.z = 0;

		if (word == "other")
		{
			step.kind = kHudsonNavStepOther;
		}
		else
		{
			if (word == "walk")
			{
				step.kind = kHudsonNavStepWalk;
			}
			else if (word == "press")
			{
				step.kind = kHudsonNavStepPress;
			}
			else if (word == "walkinto")
			{
				step.kind = kHudsonNavStepWalkInto;
			}
			else
			{
				return kHudsonNavRouteUnknownWord;
			}

			if (!ReadPlaceNumber(&rest, lineEnd, &step.x) || !ReadPlaceNumber(&rest, lineEnd, &step.y) || !ReadPlaceNumber(&rest, lineEnd, &step.z))
			{
				return kHudsonNavRouteBadNumber;
			}
		}

		if (!NameIsGood(rest, lineEnd))
		{
			return kHudsonNavRouteBadName;
		}

		step.name.assign(rest, lineEnd);
		steps->push_back(step);

		place = nextLine;
	}

	return steps->empty() ? kHudsonNavRouteEmpty : 0;
}

HudsonNavRayKind HudsonNav_ReadRay(bool hitSomething, bool hitADoor, int normalY)
{
	if (!hitSomething)
	{
		return kHudsonNavRayOpen;
	}

	if (hitADoor)
	{
		return kHudsonNavRayDoor;
	}

	if (normalY <= -kHudsonNavFlatFloor)
	{
		return kHudsonNavRayFlatFloor;
	}

	if (normalY <= -kHudsonNavWalkable)
	{
		return kHudsonNavRaySlope;
	}

	if (normalY <= -kHudsonNavUpright)
	{
		return kHudsonNavRayTooSteep;
	}

	return (normalY < kHudsonNavUpright) ? kHudsonNavRayWall : kHudsonNavRayCeiling;
}

const char *HudsonNav_RayWord(HudsonNavRayKind kind)
{
	switch (kind)
	{
		case kHudsonNavRayOpen:      return "open";
		case kHudsonNavRayFlatFloor: return "flat";
		case kHudsonNavRaySlope:     return "slope";
		case kHudsonNavRayTooSteep:  return "too_steep";
		case kHudsonNavRayWall:      return "wall";
		case kHudsonNavRayCeiling:   return "ceiling";
		case kHudsonNavRayDoor:      return "door";
		default:                     return "?";
	}
}

void HudsonNav_TurnEighths(int x, int z, int eighths, int *turnedX, int *turnedZ)
{
	// How much of the old direction stays, and how much goes across, for each eighth of a turn
	static int const stays[8] = { kHudsonNavOneWhole, kOneEighthOfATurn, 0, -kOneEighthOfATurn, -kHudsonNavOneWhole, -kOneEighthOfATurn, 0, kOneEighthOfATurn };
	static int const across[8] = { 0, kOneEighthOfATurn, kHudsonNavOneWhole, kOneEighthOfATurn, 0, -kOneEighthOfATurn, -kHudsonNavOneWhole, -kOneEighthOfATurn };

	// What's left over after dividing by 8, made to come out from 0 to 7 for a number below zero as well
	int const which = ((eighths % 8) + 8) % 8;

	*turnedX = (int)(((int64_t)x * stays[which] + (int64_t)z * across[which]) / kHudsonNavOneWhole);
	*turnedZ = (int)(((int64_t)z * stays[which] - (int64_t)x * across[which]) / kHudsonNavOneWhole);
}

void HudsonNav_SpiralPlace(int number, int *acrossX, int *acrossZ)
{
	static int const wayX[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
	static int const wayZ[8] = { 1, 1, 0, -1, -1, -1, 0, 1 };

	if (number <= 0)
	{
		*acrossX = 0;
		*acrossZ = 0;
		return;
	}

	int const ring = (number - 1) / 8 + 1;
	int const way = (number - 1) % 8;

	*acrossX = wayX[way] * ring * kHudsonNavSpiralStep;
	*acrossZ = wayZ[way] * ring * kHudsonNavSpiralStep;
}
