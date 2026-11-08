// See shoulder_lamp.h. Written 2026-11-05 by Melanikus for the AvP port.
#include "shoulder_lamp.h"

#include <math.h>

static bool lampIsOn = false;
static bool lampIsAskedFor = false;
static long framesDrawn = 0;
static long framesAsARealLight = 0;
static bool playerCouldHaveALamp = false;
// Nothing in the game asks for the real light, so the lamp is the painted version. Only a Hudson run can ask for it, through
// ShoulderLamp_SetRealLightWanted, to try its look.
static bool realLightIsWanted = false;

// The robot arm. See ShoulderLamp_FollowTheView.
static bool aimIsKnown = false;
static double lookBefore[3] = { 0.0, 0.0, 0.0 };     // where he was looking on the last frame
static double behind[3] = { 0.0, 0.0, 0.0 };         // how far the lamp's aim is from where he's looking, as a direction in the world
static double catchingUp[3] = { 0.0, 0.0, 0.0 };     // how fast that's changing
static double aimAcross = 0.0;
static double aimDown = 0.0;
static double followSeconds = kShoulderLampFollowSeconds;
static double spotSize = kShoulderLampSpotSize;
static double spotIntensity = kShoulderLampIntensity;
static float placeFromEyes[3] = { (float)kShoulderLampAcross, (float)kShoulderLampUp, (float)kShoulderLampAhead };

void ShoulderLamp_Toggle()
{
	lampIsOn = !lampIsOn;
}

bool ShoulderLamp_IsOn()
{
	return lampIsOn;
}

void ShoulderLamp_TurnOff()
{
	lampIsOn = false;
}

void ShoulderLamp_AskForThisFrame(bool playerCanHaveALamp)
{
	lampIsAskedFor = lampIsOn && playerCanHaveALamp;
	playerCouldHaveALamp = playerCanHaveALamp;
}

bool ShoulderLamp_IsAskedForThisFrame()
{
	return lampIsAskedFor;
}

bool ShoulderLamp_CouldBeDrawn()
{
	return lampIsOn && playerCouldHaveALamp;
}

bool ShoulderLamp_TakeThisFramesAnswer()
{
	bool const answer = lampIsAskedFor;

	lampIsAskedFor = false;

	if (answer)
	{
		framesDrawn++;
	}

	return answer;
}

long ShoulderLamp_FramesDrawn()
{
	return framesDrawn;
}

bool ShoulderLamp_CanBeALight()
{
	return lampIsOn && playerCouldHaveALamp && realLightIsWanted;
}

void ShoulderLamp_SetRealLightWanted(bool wanted)
{
	realLightIsWanted = wanted;
}

const float *ShoulderLamp_PlaceFromEyes()
{
	return placeFromEyes;
}

void ShoulderLamp_SetPlaceFromEyes(float across, float up, float ahead)
{
	placeFromEyes[0] = across;
	placeFromEyes[1] = up;
	placeFromEyes[2] = ahead;
}

static double DotProductOf(const double *a, const double *b)
{
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

void ShoulderLamp_FollowTheView(const double *look, const double *right, const double *down, double moved, double seconds)
{
	if (seconds <= 0.0)
	{
		return;
	}

	bool const putSomewhereNew = !aimIsKnown || seconds > kShoulderLampLongFrame || moved > (double)kShoulderLampMostStep || DotProductOf(look, lookBefore) < kShoulderLampSnapTurn;

	// The view has moved on, and the lamp's aim hasn't, so how far behind it is grows by just that much
	for (int i = 0; i < 3; i++)
	{
		behind[i] = putSomewhereNew ? 0.0 : behind[i] + lookBefore[i] - look[i];
		catchingUp[i] = putSomewhereNew ? 0.0 : catchingUp[i];
		lookBefore[i] = look[i];
	}

	aimIsKnown = true;

	// No time at all to catch up means it's stuck to the view
	if (followSeconds <= 0.0)
	{
		for (int i = 0; i < 3; i++)
		{
			behind[i] = 0.0;
			catchingUp[i] = 0.0;
		}

		aimAcross = 0.0;
		aimDown = 0.0;
		return;
	}

	// A spring that's strong enough to stop exactly where it's going without going past. This is the exact sum for how far it gets in this much time, so a long frame gets just as far as many short ones. Starting from still, it's within 1% after 7 divided by this number of seconds.
	double const strength = 7.0 / followSeconds;
	double const dyingAway = exp(-strength * seconds);

	for (int i = 0; i < 3; i++)
	{
		double const pull = catchingUp[i] + strength * behind[i];

		behind[i] = (behind[i] + pull * seconds) * dyingAway;
		catchingUp[i] = (catchingUp[i] - strength * pull * seconds) * dyingAway;
	}

	// It can only get so far behind
	double const howFarBehind = sqrt(DotProductOf(behind, behind));

	if (howFarBehind > kShoulderLampMostBehind)
	{
		for (int i = 0; i < 3; i++)
		{
			behind[i] *= kShoulderLampMostBehind / howFarBehind;
			catchingUp[i] = 0.0;
		}
	}

	// Where that puts the lamp's aim, as seen from the view. The limit keeps it well in front of him, so this can't divide by nothing.
	double const aim[3] = { look[0] + behind[0], look[1] + behind[1], look[2] + behind[2] };
	double const ahead = DotProductOf(aim, look);

	aimAcross = DotProductOf(aim, right) / ahead;
	aimDown = DotProductOf(aim, down) / ahead;
}

double ShoulderLamp_FollowSeconds()
{
	return followSeconds;
}

void ShoulderLamp_SetFollowSeconds(double seconds)
{
	followSeconds = seconds;
}

// A plain number, typed by a person. Only digits, with one dot at most. See ShoulderLamp_FollowSecondsFromWords in the header for why it's read by hand.
bool ReadPlainNumber(const char *words, unsigned int length, double *out)
{
	// Longer than anyone needs for any of these, and it means the sum below can't get large
	if (!words || length == 0 || length > 8)
	{
		return false;
	}

	// All the digits make one whole number, and it's divided at the end. Adding them up a place at a time would make 0.35 come out a hair over 0.35, and the top of a range would be refused.
	double allTheDigits = 0.0;
	double place = 1.0;
	bool seenDot = false;
	int digits = 0;

	for (unsigned int i = 0; i < length; i++)
	{
		char const c = words[i];

		if (c == '.' && !seenDot)
		{
			seenDot = true;
		}
		else if (c >= '0' && c <= '9')
		{
			digits++;
			allTheDigits = allTheDigits * 10.0 + (c - '0');

			if (seenDot)
			{
				place *= 10.0;
			}
		}
		else
		{
			return false;
		}
	}

	if (digits == 0)
	{
		return false;
	}

	*out = allTheDigits / place;
	return true;
}

bool ShoulderLamp_FollowSecondsFromWords(const char *words, unsigned int length, double *seconds)
{
	double number = 0.0;

	if (!ReadPlainNumber(words, length, &number) || number > kShoulderLampMostFollowSeconds)
	{
		return false;
	}

	*seconds = number;
	return true;
}

double ShoulderLamp_SpotSize()
{
	return spotSize;
}

void ShoulderLamp_SetSpotSize(double size)
{
	spotSize = size < kShoulderLampFewestSpotSize ? kShoulderLampFewestSpotSize : size > kShoulderLampMostSpotSize ? kShoulderLampMostSpotSize : size;
}

double ShoulderLamp_Intensity()
{
	return spotIntensity;
}

void ShoulderLamp_SetIntensity(double intensity)
{
	spotIntensity = intensity < kShoulderLampFewestIntensity ? kShoulderLampFewestIntensity : intensity > kShoulderLampMostIntensity ? kShoulderLampMostIntensity : intensity;
}

bool ShoulderLamp_SpotSizeFromWords(const char *words, unsigned int length, double *size)
{
	double number = 0.0;

	if (!ReadPlainNumber(words, length, &number) || number < kShoulderLampFewestSpotSize || number > kShoulderLampMostSpotSize)
	{
		return false;
	}

	*size = number;
	return true;
}

bool ShoulderLamp_IntensityFromWords(const char *words, unsigned int length, double *intensity)
{
	double number = 0.0;

	if (!ReadPlainNumber(words, length, &number) || number < kShoulderLampFewestIntensity || number > kShoulderLampMostIntensity)
	{
		return false;
	}

	*intensity = number;
	return true;
}

bool ShoulderLamp_PutSpotBack()
{
	bool const wasChanged = spotSize != kShoulderLampSpotSize || spotIntensity != kShoulderLampIntensity;

	spotSize = kShoulderLampSpotSize;
	spotIntensity = kShoulderLampIntensity;

	return wasChanged;
}

void ShoulderLamp_SpotNumbers(float *four)
{
	four[0] = (float)spotSize;
	four[1] = (float)(kShoulderLampSpotSize / spotSize);
	four[2] = (float)spotIntensity;
	four[3] = 0.0f;
}

float ShoulderLamp_AimAcross()
{
	return (float)aimAcross;
}

float ShoulderLamp_AimDown()
{
	return (float)aimDown;
}

void ShoulderLamp_NoteARealLightFrame()
{
	framesAsARealLight++;
}

long ShoulderLamp_FramesAsARealLight()
{
	return framesAsARealLight;
}

unsigned char ShoulderLamp_KeyFromProfile(unsigned char stored, unsigned char neverSet, unsigned char defaultKey, bool defaultIsTaken, unsigned char noKey)
{
	if (stored != neverSet)
	{
		return stored;
	}

	return defaultIsTaken ? noKey : defaultKey;
}

float ShoulderLamp_Flicker(double seconds)
{
	double const turn = 6.283185307179586 * seconds;

	// Waves that don't line up with each other, so the pattern doesn't look as if it repeats. None is faster than 11 times a second.
	double const middle = sin(turn * 1.7);
	double const quick = sin(turn * 11.0) * sin(turn * 0.37);

	// Two slow waves multiplied are only near their top now and then, and that's when it dips
	double const slow = sin(turn * 0.23) * sin(turn * 0.61);
	double const dip = slow > 0.55 ? (slow - 0.55) / 0.45 : 0.0;

	return (float)(0.92 + 0.04 * middle + 0.03 * quick - 0.20 * dip);
}
