// The place in ArcTanTable for an angle, worked out so it's always in the table. Written 2026-11-08 by Melanikus for the AvP port.
//
// ArcTan (maths.cpp) finds an angle from two sides: it looks up the shorter side over the longer one, times 256, in ArcTanTable's 256
// entries. It used to do that sum in 32 bits, where a side over 2^23 (8.4 km) times 256 doesn't fit, and the size of the most negative
// int isn't a positive int at all. Both made a negative place, and a read before the table. Positions in a saved game aren't bounded, so
// a save could reach it. Here the sizes are 64 bits, where nothing an int holds overflows. Every answer that fitted before is the same.
#ifndef ARCTAN_INDEX_H
#define ARCTAN_INDEX_H

// How long a side is, whatever its sign, as a 64-bit number: the most negative int's size fits too
inline long long ArcTan_Size(int side)
{
	return side < 0 ? -static_cast<long long>(side) : static_cast<long long>(side);
}

// The place in ArcTanTable for the shorter side over the longer one. The longer has to be longer than the shorter (ArcTan gives the
// angle for sides of one size itself), so the place is from 0 to 255; anything else gives 0.
inline int ArcTan_TableIndex(long long shorter, long long longer)
{
	if (!(shorter >= 0 && longer > shorter))
	{
		return 0;
	}

	return static_cast<int>((shorter << 8) / longer);
}

#endif
