// Where the letters are in a font picture, and whether a picture is big enough to hold them. Written 2026-11-05 by Melanikus for the AvP port.

#ifndef FONT_CHECKS_H
#define FONT_CHECKS_H

#include <stdint.h>

// A font picture has no list of where its letters are. They sit in a fixed grid, 16 pixels a letter and 16 letters a row, starting with letter 32, the space.
inline int FontChecks_LetterX(int letter)
{
	return 1 + ((letter - 32) & 15) * 16;
}

inline int FontChecks_LetterY(int letter)
{
	return 1 + ((letter - 32) >> 4) * 16;
}

// Whether every letter from 33 up to the last one can be read, with each being letterWidth wide and letterHeight tall. Letters are read from one column past their left edge.
//
// A pixel's place in the picture comes from x + y * width, so that sum is what has to stay inside the picture. A place past the end of a row carries on into what follows, inside the same
// picture, so that alone isn't refused. The demo's own font does that, being 128 wide with a grid that goes to 256. FontChecks_LetterWidth needs each row to be at least width pixels long, which
// the game checks when it has the picture in its hands.
inline bool FontChecks_LettersFit(uint32_t width, uint32_t height, int lettersToRead, int letterWidth, int letterHeight)
{
	uint64_t const pixels = (uint64_t)width * height;

	for (int letter = 33; letter < lettersToRead; letter++)
	{
		uint64_t const lastX = (uint64_t)FontChecks_LetterX(letter) + letterWidth;
		uint64_t const lastY = (uint64_t)FontChecks_LetterY(letter) + letterHeight - 1;

		if (lastX + lastY * width >= pixels)
		{
			return false;
		}
	}

	return true;
}

// How wide a letter is, looking from its right edge for the last column with anything bright in it. pixels is four bytes a pixel, blue first, and a row takes pitch bytes. Only for a picture
// that's passed the check above. widest is the width of a letter with something in its last column.
inline int FontChecks_LetterWidth(uint8_t const *pixels, uint32_t pitch, int letter, int letterWidth, int letterHeight, int widest)
{
	int const left = FontChecks_LetterX(letter);
	int const top = FontChecks_LetterY(letter);
	int width = widest;

	for (int x = left + letterWidth; x > left; x--)
	{
		for (int y = top; y < top + letterHeight; y++)
		{
			if (pixels[(size_t)y * pitch + (size_t)x * 4] >= 0x80)
			{
				return width;
			}
		}

		width--;
	}

	return width;
}

#endif
