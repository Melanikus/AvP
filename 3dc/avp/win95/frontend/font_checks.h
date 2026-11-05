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
// A pixel's place in the picture comes from x + y * width, so that sum is what has to stay inside the picture. A place past the end of a row just lands on the next row, so that alone isn't
// refused. The demo's own font does that, being 128 wide with a grid that goes to 256.
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

#endif
