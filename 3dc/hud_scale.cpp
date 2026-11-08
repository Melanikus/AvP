// How big the HUD is drawn. Written 2026-11-08 by Melanikus for the AvP port (card HUD-SCALE-1). See hud_scale.h.
// Reported by Bobblen; follows his avp-forever (commit a53f482), rewritten.
#include "hud_scale.h"

// The same as the game's MUL_FIXED (mathline.cpp): a whole number times a fixed-point one, with the fraction dropped. It's written out
// here so this file needs nothing from the game. For a positive scale, dropping the fraction rounds down. A scale of nothing or less
// draws no bar at all (HUDAlienBar_Place), so what it gives for one doesn't matter.
static int TimesFixed(int number, int fixed)
{
	return static_cast<int>((static_cast<long long>(number) * fixed) >> 16);
}

// Brings a number inside the sizes the player can have, and to the nearest step. A half goes up: 55 is 60.
static int NearestSize(int number)
{
	if (number < kHUDSizeSmallest)
	{
		number = kHUDSizeSmallest;
	}

	if (number > kHUDSizeLargest)
	{
		number = kHUDSizeLargest;
	}

	return kHUDSizeSmallest + ((number - kHUDSizeSmallest + kHUDSizeStep / 2) / kHUDSizeStep) * kHUDSizeStep;
}

int HUDScale_ForScreen(int screenHeight, int size)
{
	if (screenHeight < 1)
	{
		screenHeight = 1;
	}

	if (screenHeight > kHUDTallestScreen)
	{
		screenHeight = kHUDTallestScreen;
	}

	size = NearestSize(size);

	// Worked out in one step, so it's rounded only once, at the end: height / 480, times the usual size, times the player's percentage.
	// At most 16384 * 61440 * 150, which needs more room than an int has, so it's worked out in a long long. The answer, at most
	// 3,145,728, fits an int easily.
	return static_cast<int>((static_cast<long long>(screenHeight) * kHUDUsualSize * size) / (480LL * 100));
}

int HUDSize_AtSliderPlace(int place)
{
	if (place < 0)
	{
		place = 0;
	}

	if (place > kHUDSizeSliderPlaces)
	{
		place = kHUDSizeSliderPlaces;
	}

	return kHUDSizeSmallest + place * kHUDSizeStep;
}

int HUDSize_SliderPlace(int size)
{
	return (NearestSize(size) - kHUDSizeSmallest) / kHUDSizeStep;
}

bool HUDSize_FromSavedWord(const char *word, int *size)
{
	*size = kHUDSizeUsual;

	if (!word)
	{
		return false;
	}

	// Only digits count, and at most three of them: a sign, a space, a fraction or anything after the number makes it not a number at
	// all. Four digits stop the reading straight away, so a word of any length is read only as far as its fourth character.
	int number = 0;
	int digits = 0;

	for (const char *character = word; *character; character++)
	{
		if (*character < '0' || *character > '9' || digits == 3)
		{
			return false;
		}

		number = number * 10 + (*character - '0');
		digits++;
	}

	if (digits == 0)
	{
		return false;
	}

	*size = NearestSize(number);

	// Written plainly, as the menu writes it, the word is the size itself, with no 0 in front: every size there is starts with 5 to 9 or 1
	return number == *size && word[0] != '0';
}

HUDBar HUDAlienBar_Place(int screenWidth, int screenHeight, int scale, unsigned int health)
{
	HUDBar bar = { 0, 0, 0, 0 };

	// The width of a 4:3 screen of this height, but never wider than the screen: a 5:4 screen, 1280 by 1024, is narrower than 4:3
	long long fullLength = static_cast<long long>(screenHeight) * 4 / 3;

	if (fullLength > screenWidth)
	{
		fullLength = screenWidth;
	}

	long long const fullThickness = TimesFixed(kAlienBarThickness, scale);

	// No screen, or no bar to draw: nothing to work out, and it keeps the sums below from dividing by nothing
	if (fullLength <= 0 || fullThickness <= 0)
	{
		return bar;
	}

	long long const length = (health >= 100) ? fullLength : fullLength * health / 100;

	// How far in the shorter bar starts from each end of the whole bar
	long long const inset = (fullLength - length) / 2;

	// The bar keeps its shape as it shrinks: it gets thinner in step with getting shorter, and it moves down by the same share of its
	// thickness as it moves in from each end, so it stays on the whole bar's middle line. These are the sums the game did in 1999
	// (thickness = length / 32, top = bottom - whole thickness + inset / 32, d3d_hud.cpp), with the old 1 / 32 written as the whole bar's
	// thickness / its length. On a 1280 by 1024 screen at the usual size that's 40 / 1280, which is 1 / 32 exactly, so there every health
	// gives exactly the bar it always had, to the pixel.
	bar.length = static_cast<int>(length);
	bar.thickness = static_cast<int>(length * fullThickness / fullLength);
	bar.left = static_cast<int>((screenWidth - length) / 2);
	bar.top = static_cast<int>(screenHeight - fullThickness + inset * fullThickness / fullLength);

	return bar;
}
