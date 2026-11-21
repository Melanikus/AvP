// Telling a real number from "not a number" (NaN) and from endless (infinity), by its bits. Written 2026-11-14 by Melanikus for the AvP
// port, with the same tests as AVP_OpenInteng's src/common/real_numbers.h (the map editor's), so the game and the editor agree.
//
// The game's Release build uses the compiler's fast rules for fractions (/fp:fast, win32/VS2026/AvP.vcxproj). Under those, the compiler is
// allowed to assume no value is NaN or endless, so isnan(), isfinite(), "x != x" and comparisons aren't promised to catch one. A fraction
// read from a level file, or worked out from one, can be either. So every check in the game that has to know asks these, which look at
// the bits themselves, and can't be assumed away.
//
// A double's 11 exponent bits (52 to 62) are all set exactly when it's NaN or endless, and a float's 8 (23 to 30) the same.
//
// For C++ only: these are overloaded for float and double.

#ifndef AVPX_REAL_NUMBERS_H
#define AVPX_REAL_NUMBERS_H

#include <stdint.h>
#include <string.h>

inline bool IsNaNOrEndless(double value)
{
	uint64_t bits;
	memcpy(&bits, &value, sizeof(bits));
	return ((bits >> 52) & 0x7FF) == 0x7FF;
}

inline bool IsNaNOrEndless(float value)
{
	uint32_t bits;
	memcpy(&bits, &value, sizeof(bits));
	return ((bits >> 23) & 0xFF) == 0xFF;
}

// NaN alone: all exponent bits set, and some fraction bit set. (With no fraction bit set, it's endless instead.)
inline bool IsNaN(double value)
{
	uint64_t bits;
	memcpy(&bits, &value, sizeof(bits));
	return ((bits >> 52) & 0x7FF) == 0x7FF && (bits & 0x000FFFFFFFFFFFFFull) != 0;
}

inline bool IsNaN(float value)
{
	uint32_t bits;
	memcpy(&bits, &value, sizeof(bits));
	return ((bits >> 23) & 0xFF) == 0xFF && (bits & 0x007FFFFFu) != 0;
}

// True for a real number: not NaN and not endless.
inline bool IsRealNumber(double value)
{
	return !IsNaNOrEndless(value);
}

inline bool IsRealNumber(float value)
{
	return !IsNaNOrEndless(value);
}

// True for a real number from lowest to highest, both included. Once a value is known to be a real number, comparing it is safe even under
// the fast rules, so the range is asked only then.
inline bool IsRealNumberFrom(double value, double lowest, double highest)
{
	return IsRealNumber(value) && value >= lowest && value <= highest;
}

inline bool IsRealNumberFrom(float value, float lowest, float highest)
{
	return IsRealNumber(value) && value >= lowest && value <= highest;
}

#endif
