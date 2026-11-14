// See shoulder_lamp.h. Written 2026-11-05 by Melanikus for the AvP port.
#include "shoulder_lamp.h"

#include <math.h>

static bool lampIsOn = false;
static bool lampIsAskedFor = false;
static long framesDrawn = 0;
static long framesAsARealLight = 0;
static bool playerCouldHaveALamp = false;
static bool realLightIsWanted = false;
static bool beamsAreWanted = true;

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
static int lampLook = kShoulderLampLookAsBuilt;   // LAMPLOOK (shoulder_lamp.h; added 2026-11-11 by Melanikus (Third))

// The lamp's look, R-LAMP-1 (shoulder_lamp.h). Added 2026-11-14 by Melanikus (Dietrich) for the AvP port.
static double lampStrength = kShoulderLampStrength;
static double lampHalfDistance = kShoulderLampHalfDistance;
static double coneInside = kShoulderLampConeInside;
static double coneOutside = kShoulderLampConeOutside;
static double lampSpill = kShoulderLampSpill;
static double shoulderAcross = kShoulderLampShoulderAcross;
static double shoulderDown = kShoulderLampShoulderDown;
static double spotHalfDistance = kShoulderLampSpotHalfDistance;
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

// LAMPLOOK 2 keeps it painted, as "Lamps cast shadows" off does (modified 2026-11-11 by Melanikus (Third) for the AvP port)
bool ShoulderLamp_CanBeALight()
{
	return lampIsOn && playerCouldHaveALamp && realLightIsWanted && lampLook != kShoulderLampLookPainted;
}

void ShoulderLamp_SetRealLightWanted(bool wanted)
{
	realLightIsWanted = wanted;
}

bool ShoulderLamp_RealLightIsWanted()
{
	return realLightIsWanted;
}

void ShoulderLamp_SetBeamsWanted(bool wanted)
{
	beamsAreWanted = wanted;
}

bool ShoulderLamp_BeamsAreWanted()
{
	return beamsAreWanted;
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

// The lamp's look, R-LAMP-1 (shoulder_lamp.h). Added 2026-11-14 by Melanikus (Dietrich) for the AvP port.

// A number brought inside its limits. A number that isn't a number at all (one that compares false with everything) comes out as the
// least, so even that can't reach a shader.
static double Within(double number, double least, double most)
{
	if (!(number >= least))
	{
		return least;
	}

	return number > most ? most : number;
}

double ShoulderLamp_Strength()
{
	return lampStrength;
}

void ShoulderLamp_SetStrength(double strength)
{
	lampStrength = Within(strength, kShoulderLampFewestStrength, kShoulderLampMostStrength);
}

double ShoulderLamp_HalfDistance()
{
	return lampHalfDistance;
}

void ShoulderLamp_SetHalfDistance(double millimetres)
{
	lampHalfDistance = Within(millimetres, kShoulderLampFewestHalfDistance, kShoulderLampMostHalfDistance);
}

double ShoulderLamp_ConeInside()
{
	return coneInside;
}

double ShoulderLamp_ConeOutside()
{
	return coneOutside;
}

// The two together, because each one's limit depends on the other: outside has to stay at least kShoulderLampConeGap below inside, or
// the shader would divide by nothing. Inside comes first, and outside is then brought under it.
void ShoulderLamp_SetCone(double inside, double outside)
{
	coneInside = Within(inside, kShoulderLampFewestConeInside, kShoulderLampMostConeInside);

	double const mostOutside = coneInside - kShoulderLampConeGap < kShoulderLampMostConeOutside ? coneInside - kShoulderLampConeGap : kShoulderLampMostConeOutside;

	coneOutside = Within(outside, kShoulderLampFewestConeOutside, mostOutside);
}

double ShoulderLamp_Spill()
{
	return lampSpill;
}

void ShoulderLamp_SetSpill(double spill)
{
	lampSpill = Within(spill, 0.0, kShoulderLampMostSpill);
}

double ShoulderLamp_ShoulderAcross()
{
	return shoulderAcross;
}

double ShoulderLamp_ShoulderDown()
{
	return shoulderDown;
}

void ShoulderLamp_SetShoulder(double across, double down)
{
	shoulderAcross = Within(across, kShoulderLampFewestShoulder, kShoulderLampMostShoulder);
	shoulderDown = Within(down, kShoulderLampFewestShoulder, kShoulderLampMostShoulder);
}

double ShoulderLamp_SpotHalfDistance()
{
	return spotHalfDistance;
}

// 0 is its own choice, no fall at all. Anything else is brought inside the limits, so a number between 0 and the least becomes the least.
void ShoulderLamp_SetSpotHalfDistance(double millimetres)
{
	spotHalfDistance = millimetres == 0.0 ? 0.0 : Within(millimetres, kShoulderLampFewestSpotHalfDistance, kShoulderLampMostSpotHalfDistance);
}

bool ReadSignedNumber(const char *words, unsigned int length, double *out)
{
	if (words && length > 1 && words[0] == '-')
	{
		double number = 0.0;

		if (!ReadPlainNumber(words + 1, length - 1, &number))
		{
			return false;
		}

		*out = -number;
		return true;
	}

	return ReadPlainNumber(words, length, out);
}

// Splits what was typed in two at its first spaces: the first word, and everything after the spaces. There must be something on each side.
// The second part isn't searched for more spaces, because the number readers refuse a space anyway, so three words, or a space at the end,
// are refused when the second part is read. Anything else gives back false.
static bool TwoWords(const char *words, unsigned int length, unsigned int *firstLength, const char **second, unsigned int *secondLength)
{
	if (!words)
	{
		return false;
	}

	unsigned int first = 0;

	while (first < length && words[first] != ' ')
	{
		first++;
	}

	unsigned int gapEnd = first;

	while (gapEnd < length && words[gapEnd] == ' ')
	{
		gapEnd++;
	}

	if (first == 0 || gapEnd == first || gapEnd == length)
	{
		return false;
	}

	*firstLength = first;
	*second = words + gapEnd;
	*secondLength = length - gapEnd;
	return true;
}

// One plain number inside its limits, as LAMPSPOTSIZE's is read
static bool PlainNumberWithin(const char *words, unsigned int length, double least, double most, double *out)
{
	double number = 0.0;

	if (!ReadPlainNumber(words, length, &number) || number < least || number > most)
	{
		return false;
	}

	*out = number;
	return true;
}

bool ShoulderLamp_StrengthFromWords(const char *words, unsigned int length, double *strength)
{
	return PlainNumberWithin(words, length, kShoulderLampFewestStrength, kShoulderLampMostStrength, strength);
}

bool ShoulderLamp_HalfDistanceFromWords(const char *words, unsigned int length, double *millimetres)
{
	return PlainNumberWithin(words, length, kShoulderLampFewestHalfDistance, kShoulderLampMostHalfDistance, millimetres);
}

bool ShoulderLamp_ConeFromWords(const char *words, unsigned int length, double *inside, double *outside)
{
	unsigned int firstLength = 0;
	const char *second = 0;
	unsigned int secondLength = 0;
	double in = 0.0;
	double out = 0.0;

	if (!TwoWords(words, length, &firstLength, &second, &secondLength) ||
		!PlainNumberWithin(words, firstLength, kShoulderLampFewestConeInside, kShoulderLampMostConeInside, &in) ||
		!PlainNumberWithin(second, secondLength, kShoulderLampFewestConeOutside, kShoulderLampMostConeOutside, &out))
	{
		return false;
	}

	// Outside has to be at least the gap below inside. The sum is done on the numbers as read, so 0.90 and 0.89 are just far enough apart.
	if (in - out < kShoulderLampConeGap - 0.0000001)
	{
		return false;
	}

	*inside = in;
	*outside = out;
	return true;
}

bool ShoulderLamp_SpillFromWords(const char *words, unsigned int length, double *spill)
{
	return PlainNumberWithin(words, length, 0.0, kShoulderLampMostSpill, spill);
}

bool ShoulderLamp_ShoulderFromWords(const char *words, unsigned int length, double *across, double *down)
{
	unsigned int firstLength = 0;
	const char *second = 0;
	unsigned int secondLength = 0;
	double x = 0.0;
	double y = 0.0;

	if (!TwoWords(words, length, &firstLength, &second, &secondLength) ||
		!ReadSignedNumber(words, firstLength, &x) || !ReadSignedNumber(second, secondLength, &y) ||
		x < kShoulderLampFewestShoulder || x > kShoulderLampMostShoulder || y < kShoulderLampFewestShoulder || y > kShoulderLampMostShoulder)
	{
		return false;
	}

	*across = x;
	*down = y;
	return true;
}

bool ShoulderLamp_SpotHalfDistanceFromWords(const char *words, unsigned int length, double *millimetres)
{
	double number = 0.0;

	if (!ReadPlainNumber(words, length, &number))
	{
		return false;
	}

	if (number != 0.0 && (number < kShoulderLampFewestSpotHalfDistance || number > kShoulderLampMostSpotHalfDistance))
	{
		return false;
	}

	*millimetres = number;
	return true;
}

void ShoulderLamp_PaintedBeam(float aimAcross, float aimDown, float wide, float *four)
{
	// The same sums the shader does for the spot: where it's aimed, measured across in screen heights. Then the line from the shoulder to there.
	double const aimX = (kShoulderLampPaintedAim + aimAcross * kShoulderLampPaintedFollowing) * wide;
	double const aimY = kShoulderLampPaintedAim + aimDown * kShoulderLampPaintedFollowing;
	double const beamX = aimX - shoulderAcross * wide;
	double const beamY = aimY - shoulderDown;
	double const lengthSquared = beamX * beamX + beamY * beamY;

	four[0] = (float)beamX;
	four[1] = (float)beamY;
	four[2] = (float)(1.0 / (lengthSquared > kShoulderLampShortestBeam ? lengthSquared : kShoulderLampShortestBeam));
	four[3] = 0.0f;
}

void ShoulderLamp_LightNumbers(float *eight)
{
	eight[0] = (float)coneInside;
	eight[1] = (float)coneOutside;
	eight[2] = (float)lampSpill;
	eight[3] = (float)lampStrength;
	eight[4] = (float)(1.0 / (lampHalfDistance * lampHalfDistance));
	eight[5] = 0.0f;
	eight[6] = 0.0f;
	eight[7] = 0.0f;
}

bool ShoulderLamp_PutSpotBack()
{
	bool const wasChanged = spotSize != kShoulderLampSpotSize || spotIntensity != kShoulderLampIntensity || lampLook != kShoulderLampLookAsBuilt ||
		lampStrength != kShoulderLampStrength || lampHalfDistance != kShoulderLampHalfDistance || coneInside != kShoulderLampConeInside ||
		coneOutside != kShoulderLampConeOutside || lampSpill != kShoulderLampSpill || shoulderAcross != kShoulderLampShoulderAcross ||
		shoulderDown != kShoulderLampShoulderDown || spotHalfDistance != kShoulderLampSpotHalfDistance;

	spotSize = kShoulderLampSpotSize;
	spotIntensity = kShoulderLampIntensity;
	lampLook = kShoulderLampLookAsBuilt;
	lampStrength = kShoulderLampStrength;
	lampHalfDistance = kShoulderLampHalfDistance;
	coneInside = kShoulderLampConeInside;
	coneOutside = kShoulderLampConeOutside;
	lampSpill = kShoulderLampSpill;
	shoulderAcross = kShoulderLampShoulderAcross;
	shoulderDown = kShoulderLampShoulderDown;
	spotHalfDistance = kShoulderLampSpotHalfDistance;

	return wasChanged;
}

// Added 2026-11-11 by Melanikus (Third) for the AvP port: LAMPLOOK (shoulder_lamp.h)
int ShoulderLamp_Look()
{
	return lampLook;
}

void ShoulderLamp_SetLook(int look)
{
	if (look == kShoulderLampLookRealOnly || look == kShoulderLampLookBoth || look == kShoulderLampLookPainted || look == kShoulderLampLookBothFull)
	{
		lampLook = look;
	}
}

bool ShoulderLamp_LookFromWords(const char *words, unsigned int length, int *look)
{
	if (!words || length != 1 || words[0] < '0' || words[0] > '3')
	{
		return false;
	}

	*look = words[0] - '0';
	return true;
}

void ShoulderLamp_SpotNumbers(float *four)
{
	four[0] = (float)spotSize;
	four[1] = (float)(kShoulderLampSpotSize / spotSize);
	four[2] = (float)spotIntensity;
	four[3] = spotHalfDistance == 0.0 ? 0.0f : (float)(1.0 / spotHalfDistance);   // R-LAMP-1 (modified 2026-11-14 by Melanikus (Dietrich))
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
