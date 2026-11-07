// The test for 3dc/surface_tangent.cpp, which works out which way a polygon's texture runs across it.
// Written 2026-11-04 by Melanikus for the AvP port.
//
// It lays textures on made-up polygons and checks the answer against what was laid. tests/build.ps1 compiles this with the real surface_tangent.cpp,
// and the program gives back a non-zero result if any check failed. It uses no files and none of the game.

#include <math.h>
#include <stdio.h>

#include "surface_tangent.h"

static int checksFailed = 0;

static void Check(bool good, const char *what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what);

	if (!good)
	{
		checksFailed++;
	}
}

static bool Near(float a, float b)
{
	return fabsf(a - b) < 0.0001f;
}

// A number between lowest and highest. It's the same numbers on every run, so that a failure can be seen again.
static unsigned int randomState = 12345;

static float RandomBetween(float lowest, float highest)
{
	randomState = randomState * 1103515245u + 12345u;

	float const fraction = (float)((randomState >> 8) & 0xFFFFFF) / 16777216.0f;

	return lowest + (highest - lowest) * fraction;
}

static void CheckByHand()
{
	// a floor, with the texture laid square on it: U along x, V along z
	float const floorCorners[9] = { 0.0f, 0.0f, 0.0f,   1000.0f, 0.0f, 0.0f,   0.0f, 0.0f, 1000.0f };
	float const floorTexture[6] = { 0.0f, 0.0f,   256.0f, 0.0f,   0.0f, 256.0f };
	SurfaceTangent tangent;

	SurfaceTangent_Work(floorCorners, floorTexture, &tangent);
	Check(Near(tangent.x, 1.0f) && Near(tangent.y, 0.0f) && Near(tangent.z, 0.0f), "a texture laid square: U runs along x");

	float const turnedTexture[6] = { 0.0f, 0.0f,   0.0f, 256.0f,   256.0f, 0.0f };
	SurfaceTangent_Work(floorCorners, turnedTexture, &tangent);
	Check(Near(tangent.x, 0.0f) && Near(tangent.y, 0.0f) && Near(tangent.z, 1.0f), "the same texture turned a quarter: U runs along z");

	float const backwardsTexture[6] = { 256.0f, 0.0f,   0.0f, 0.0f,   256.0f, 256.0f };
	SurfaceTangent_Work(floorCorners, backwardsTexture, &tangent);
	Check(Near(tangent.x, -1.0f), "a texture running the other way: the arrow turns round");

	float const lineInTexture[6] = { 0.0f, 0.0f,   100.0f, 100.0f,   200.0f, 200.0f };
	SurfaceTangent_Work(floorCorners, lineInTexture, &tangent);
	Check(tangent.x == 0.0f && tangent.y == 0.0f && tangent.z == 0.0f && tangent.flip == 0.0f, "corners in a line in the texture: all zero");

	float const onePoint[9] = { 5.0f, 5.0f, 5.0f,   5.0f, 5.0f, 5.0f,   5.0f, 5.0f, 5.0f };
	SurfaceTangent_Work(onePoint, floorTexture, &tangent);
	Check(tangent.x == 0.0f && tangent.y == 0.0f && tangent.z == 0.0f && tangent.flip == 0.0f, "a polygon with no size: all zero");

	// A level file can hold anything in a float. Dividing zero by zero makes a value that isn't a number, and dividing one by zero makes infinity.
	float const zero = RandomBetween(0.0f, 0.0f);
	float const notANumber = zero / zero;
	float const infinity = 1.0f / zero;
	float const hostile[3] = { notANumber, infinity, -infinity };
	bool allEmpty = true;

	for (int value = 0; value < 3; value++)
	{
		for (int place = 0; place < 6; place++)
		{
			float coordinates[6] = { 0.0f, 0.0f,   256.0f, 0.0f,   0.0f, 256.0f };
			coordinates[place] = hostile[value];
			SurfaceTangent_Work(floorCorners, coordinates, &tangent);

			if (tangent.x != 0.0f || tangent.y != 0.0f || tangent.z != 0.0f || tangent.flip != 0.0f)
			{
				allEmpty = false;
			}
		}
	}

	Check(allEmpty, "a texture coordinate that isn't a real number: all zero, wherever it is");

	float const stretched[6] = { 0.0f, 0.0f,   256.0f, 0.0f,   0.0f, 8.0f };
	SurfaceTangent_Work(floorCorners, stretched, &tangent);
	Check(Near(tangent.x, 1.0f) && Near(tangent.z, 0.0f), "a texture stretched one way: the direction doesn't change");
}

// Picks an arrow, and a second one square to it, then lays a texture along those. The answer has to be the first arrow, and rebuilding V from the
// normal has to give the second, the right way round.
static void CheckMadeUpPolygons()
{
	const int kPolygons = 500000;
	int wrongArrow = 0, wrongSide = 0, tried = 0;

	for (int polygon = 0; polygon < kPolygons; polygon++)
	{
		float u[3], v[3];

		for (int axis = 0; axis < 3; axis++)
		{
			u[axis] = RandomBetween(-1.0f, 1.0f);
			v[axis] = RandomBetween(-1.0f, 1.0f);
		}

		float const uLength = sqrtf(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);

		if (uLength < 0.1f)
		{
			continue;
		}

		float const along = (u[0] * v[0] + u[1] * v[1] + u[2] * v[2]) / (uLength * uLength);

		for (int axis = 0; axis < 3; axis++)
		{
			u[axis] /= uLength;
			v[axis] -= u[axis] * along * uLength;
		}

		float const vLength = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);

		if (vLength < 0.1f)
		{
			continue;
		}

		for (int axis = 0; axis < 3; axis++)
		{
			v[axis] /= vLength;
		}

		// three places in the texture, a whole triangle's worth apart, and half the textures laid on backwards
		float const reversed = (polygon & 1) ? -1.0f : 1.0f;
		float const start[3] = { RandomBetween(-100000.0f, 100000.0f), RandomBetween(-100000.0f, 100000.0f), RandomBetween(-100000.0f, 100000.0f) };
		float coordinates[6] = { RandomBetween(0.0f, 256.0f), RandomBetween(0.0f, 256.0f) };

		coordinates[2] = coordinates[0] + RandomBetween(50.0f, 200.0f);
		coordinates[3] = coordinates[1] + RandomBetween(-20.0f, 20.0f);
		coordinates[4] = coordinates[0] + RandomBetween(-20.0f, 20.0f);
		coordinates[5] = coordinates[1] + RandomBetween(50.0f, 200.0f);

		float corners[9];
		const float kScale = 20.0f;

		for (int corner = 0; corner < 3; corner++)
		{
			float const stepU = (coordinates[corner * 2] - coordinates[0]) * kScale;
			float const stepV = (coordinates[corner * 2 + 1] - coordinates[1]) * kScale * reversed;

			for (int axis = 0; axis < 3; axis++)
			{
				corners[corner * 3 + axis] = start[axis] + u[axis] * stepU + v[axis] * stepV;
			}
		}

		SurfaceTangent tangent;
		SurfaceTangent_Work(corners, coordinates, &tangent);
		tried++;

		// a small allowance: the corners were rounded to floats at sizes up to 100,000
		float const agree = tangent.x * u[0] + tangent.y * u[1] + tangent.z * u[2];

		if (agree < 0.999f)
		{
			wrongArrow++;
		}

		// rebuild V as the shader will, from a normal made the way the game's is, and compare
		float const a[3] = { corners[3] - corners[0], corners[4] - corners[1], corners[5] - corners[2] };
		float const b[3] = { corners[6] - corners[0], corners[7] - corners[1], corners[8] - corners[2] };
		float const n[3] = { a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0] };
		float const cross[3] =
		{
			n[1] * tangent.z - n[2] * tangent.y,
			n[2] * tangent.x - n[0] * tangent.z,
			n[0] * tangent.y - n[1] * tangent.x
		};
		float const side = (cross[0] * v[0] + cross[1] * v[1] + cross[2] * v[2]) * tangent.flip * reversed;

		if (side <= 0.0f)
		{
			wrongSide++;
		}
	}

	char line[160];

	snprintf(line, sizeof(line), "U's direction is right on made-up polygons: %d tried, %d wrong", tried, wrongArrow);
	Check(tried > kPolygons / 2 && wrongArrow == 0, line);

	snprintf(line, sizeof(line), "V comes back on the right side, textures laid backwards too: %d wrong", wrongSide);
	Check(wrongSide == 0, line);
}

int main()
{
	CheckByHand();
	CheckMadeUpPolygons();

	if (checksFailed)
	{
		printf("FAILED: %d checks\n", checksFailed);
		return 1;
	}

	printf("all surface tangent checks passed\n");
	return 0;
}
