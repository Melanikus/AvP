// Turning what someone typed at the console into a variable's value, safely. Written 2026-11-07 by Melanikus for the AvP port.
//
// The console's variables (TIMESCALE, SKY_RED and the rest, gamevars.cpp) each have a smallest and a largest value. The old code worked
// the typed number out first and kept it in range afterwards, so a number too big for the sum went wrong before the range was ever
// checked: "TIMESCALE 40000" multiplied 40000 by 65536, which doesn't fit, and "SKY_RED 1e40." or "nan." turned a number with no whole
// value into one. C++ doesn't say what happens then. Typed text is outside input, so the range comes first now, and the sum after, in
// numbers big enough to hold it.
//
// Nothing here touches the game, so tests/debug_commands_test.cpp can try any number.
#ifndef CONSOLE_VALUE_H
#define CONSOLE_VALUE_H

#include <limits.h>

// A typed whole number, as an int. strtoll gives back the largest or smallest it can hold for digits that go on too long, so this only
// has to bring that into an int's range.
inline int ConsoleValue_ToInt(long long typed)
{
	if (typed > INT_MAX)
	{
		return INT_MAX;
	}

	if (typed < INT_MIN)
	{
		return INT_MIN;
	}

	return (int)typed;
}

// A whole number, kept between the smallest and the largest
inline int ConsoleValue_InRange(long long value, int minVal, int maxVal)
{
	if (value > maxVal)
	{
		return maxVal;
	}

	if (value < minVal)
	{
		return minVal;
	}

	return (int)value;
}

// A typed whole number of units, as a fixed-point value, where one unit is 65536. Any int times 65536 fits in 64 bits, so the sum can't
// go wrong before the range is checked.
inline int ConsoleValue_WholeAsFixed(int typed, int minVal, int maxVal)
{
	return ConsoleValue_InRange((long long)typed * 65536, minVal, maxVal);
}

// A typed number with a decimal point, times scale (1 for a whole-number variable, 65536 for a fixed-point one). Gives back false, and
// leaves the value alone, for "not a number", which is neither in nor out of range. Anything else is kept in range while it's still a
// decimal, and only then made whole, so a huge or endless one becomes the largest value and not something undefined.
inline bool ConsoleValue_FromDecimal(double typed, double scale, int minVal, int maxVal, int *value)
{
	if (typed != typed)
	{
		return false;
	}

	// scale is 1 or 65536, so an ordinary or endless number times it is never "not a number"
	double scaled = typed * scale;

	if (scaled > (double)maxVal)
	{
		scaled = (double)maxVal;
	}

	if (scaled < (double)minVal)
	{
		scaled = (double)minVal;
	}

	*value = (int)scaled;
	return true;
}

#endif
