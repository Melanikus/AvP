// Making a direction one unit long. Written 2026-11-08 by Melanikus for the AvP port (NORMALISE-ZERO-1).
//
// Normalise and the three like it (plspecfn.cpp) keep a vector pointing the same way and make it one unit long: 65536 in the game's
// fixed point, or 1.0 for floats. They work out its length and divide by it. A vector of nothing, (0,0,0), points nowhere, and has no
// length to divide by:
// - in whole numbers that came to 65536 / 0, which is infinity, times 0, which is "not a number", and an int can't hold that, so each
//   part came back as -2147483648, the lowest an int can be. That's a huge vector pointing nowhere in particular, and whatever was built
//   from it, a matrix, an aim, a push, took it as a real direction;
// - in floats each part came back as "not a number", which then spreads into every sum it's part of.
//
// Now a vector with no direction stays a vector of nothing. For floats, a length that isn't a real number above 0 counts as no
// direction too: a vector of nothing, or one that already holds "not a number" or infinity. The two functions that need a real direction
// to make a matrix, MakeMatrixFromDirection (weapons.cpp) and MatrixFromZVector (maths.cpp), give back the matrix that turns nothing for
// a vector of nothing.
//
// For every other vector the sums are the ones the game always did, written the same way, so the answers are the same to the last bit.
//
// This file touches nothing in the game, so tests/normalise_test.cpp can try any vector.
#ifndef NORMALISE_H
#define NORMALISE_H

#include <float.h>
#include <math.h>

// A float length that can be divided by: a real number above 0, and not infinity. "Not a number" is never above 0, so it fails too.
inline bool Normalise_LengthIsReal(float length)
{
	return length > 0.0f && length <= FLT_MAX;
}

// Normalise: three whole numbers, made 65536 long, or left as nothing
inline void Normalise_Whole3(int *x, int *y, int *z)
{
	if (*x == 0 && *y == 0 && *z == 0)
	{
		return;
	}

	float const nx = (float)*x;
	float const ny = (float)*y;
	float const nz = (float)*z;
	float const m = 65536.0f / sqrt((nx * nx) + (ny * ny) + (nz * nz));

	// Kept as a float before it's cut to a whole number, as the game's f2i does
	float const ax = nx * m;
	float const ay = ny * m;
	float const az = nz * m;

	*x = (int)ax;
	*y = (int)ay;
	*z = (int)az;
}

// Normalise2d: two whole numbers, made 65536 long, or left as nothing
inline void Normalise_Whole2(int *x, int *y)
{
	if (*x == 0 && *y == 0)
	{
		return;
	}

	float const nx = (float)*x;
	float const ny = (float)*y;
	float const m = sqrt((nx * nx) + (ny * ny));

	*x = (int)((nx * 65536) / m);
	*y = (int)((ny * 65536) / m);
}

// FNormalise: three floats, made 1.0 long, or made nothing if they have no length that can be divided by
inline void Normalise_Float3(float *x, float *y, float *z)
{
	float const m = sqrt((*x * *x) + (*y * *y) + (*z * *z));

	if (!Normalise_LengthIsReal(m))
	{
		*x = 0.0f;
		*y = 0.0f;
		*z = 0.0f;
		return;
	}

	*x /= m;
	*y /= m;
	*z /= m;
}

// FNormalise2d: two floats, made 1.0 long, or made nothing if they have no length that can be divided by
inline void Normalise_Float2(float *x, float *y)
{
	float const m = sqrt((*x * *x) + (*y * *y));

	if (!Normalise_LengthIsReal(m))
	{
		*x = 0.0f;
		*y = 0.0f;
		return;
	}

	*x /= m;
	*y /= m;
}

#endif
