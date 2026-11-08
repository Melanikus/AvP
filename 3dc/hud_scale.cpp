// How big the HUD is drawn. Written 2026-11-08 by Melanikus for the AvP port (card HUD-SCALE-1). See hud_scale.h.
// Reported by Bobblen; follows his avp-forever (commit a53f482), rewritten.
#include "hud_scale.h"

// The same as the game's MUL_FIXED (mathline.cpp): a whole number times a fixed-point one, with the fraction dropped. It's written out
// here so this file needs nothing from the game. Every number it's given here is positive, so dropping the fraction rounds down.
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
	HUDBar bar;

	// The width of a 4:3 screen of this height, but never wider than the screen: a 5:4 screen, 1280 by 1024, is narrower than 4:3
	long long fullLength = static_cast<long long>(screenHeight) * 4 / 3;

	if (fullLength > screenWidth)
	{
		fullLength = screenWidth;
	}

	if (fullLength < 0)
	{
		fullLength = 0;
	}

	int const fullThickness = TimesFixed(kAlienBarThickness, scale);

	if (health >= 100)
	{
		bar.length = static_cast<int>(fullLength);
		bar.thickness = fullThickness;
	}
	else
	{
		bar.length = static_cast<int>(fullLength * health / 100);
		bar.thickness = static_cast<int>(static_cast<long long>(fullThickness) * health / 100);
	}

	// Centred across the screen. Down the screen, the whole bar's bottom edge is the screen's bottom, and a shorter one sits on the whole
	// bar's middle line, so it shrinks towards that line as well as towards the middle.
	bar.left = (screenWidth - bar.length) / 2;
	bar.top = screenHeight - (fullThickness + bar.thickness) / 2;

	return bar;
}
