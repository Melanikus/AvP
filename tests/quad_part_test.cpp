// The test of where a quad showing part of a texture reads from (3dc/quad_part.h, used by d3d_render.cpp DrawQuadPart). Written 2026-11-23 by Melanikus (Frost)
// for the AvP port.
//
// WHAT IT TESTS
//
//   (1) a part inside the texture: its places, fractions of what the card made (which can be larger than what was asked for)
//   (2) the whole texture, and a part touching its right and bottom edges: given (the edges belong to it)
//   (3) refused, uv untouched: a part of no width or no height; one starting past the texture, or running past its right or bottom edge (also by an
//       amount that would wrap round in 32 bits); a texture of no real size; one said to hold more than the card made
//
// HOW IT RUNS
//
// tests/build.ps1 compiles it on its own (quad_part.h is all in the header). The program gives back a non-zero result if anything failed.
//
// RED FIRST: on the stand-in that refuses every part, (1) and (2) fail.

#include <math.h>
#include <stdio.h>
#include <string>

#include "quad_part.h"

static int checksFailed = 0;

static void Check(bool passed, std::string const &what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what.c_str());

	if (!passed)
	{
		checksFailed++;
	}
}

static bool Near(float a, float b)
{
	return fabsf(a - b) < 1e-6f;
}

static std::string Places(float const uv[4])
{
	char text[96];
	snprintf(text, sizeof(text), "%.6f %.6f %.6f %.6f", uv[0], uv[1], uv[2], uv[3]);
	return text;
}

int main()
{
	printf("quad_part_test: where a quad showing part of a texture reads from\n");

	// ---- (1) a part inside: an atlas asked 2000 x 1000, made 2048 x 1024; the part at (192, 96), 96 x 96
	float uv[4] = { -1.0f, -1.0f, -1.0f, -1.0f };
	bool const inside = QuadPart_UVs(2000, 1000, 2048, 1024, 192, 96, 96, 96, uv);

	Check(inside && Near(uv[0], 192.0f / 2048) && Near(uv[1], 96.0f / 1024) && Near(uv[2], 288.0f / 2048) && Near(uv[3], 192.0f / 1024),
		"(1) a part inside: its places are fractions of what the card made (" + Places(uv) + ")");

	// ---- (2) the whole texture, and a part on its right and bottom edges
	float whole[4] = { -1.0f, -1.0f, -1.0f, -1.0f };
	float edge[4] = { -1.0f, -1.0f, -1.0f, -1.0f };
	bool const wholeGiven = QuadPart_UVs(2000, 1000, 2048, 1024, 0, 0, 2000, 1000, whole);
	bool const edgeGiven = QuadPart_UVs(2000, 1000, 2048, 1024, 1904, 904, 96, 96, edge);

	Check(wholeGiven && Near(whole[0], 0.0f) && Near(whole[1], 0.0f) && Near(whole[2], 2000.0f / 2048) && Near(whole[3], 1000.0f / 1024),
		"(2) the whole of what the texture holds (" + Places(whole) + ")");
	Check(edgeGiven && Near(edge[2], 2000.0f / 2048) && Near(edge[3], 1000.0f / 1024), "(2) a part touching its right and bottom edges (" + Places(edge) + ")");

	// ---- (3) refused, uv untouched
	struct Refused
	{
		char const *what;
		uint32_t width, height, realWidth, realHeight, u, v, partWidth, partHeight;
	};

	Refused const refused[] =
	{
		{ "no width", 2000, 1000, 2048, 1024, 0, 0, 0, 96 },
		{ "no height", 2000, 1000, 2048, 1024, 0, 0, 96, 0 },
		{ "starting past the right", 2000, 1000, 2048, 1024, 2001, 0, 1, 1 },
		{ "starting past the bottom", 2000, 1000, 2048, 1024, 0, 1001, 1, 1 },
		{ "running past the right by one", 2000, 1000, 2048, 1024, 1905, 0, 96, 96 },
		{ "running past the bottom by one", 2000, 1000, 2048, 1024, 0, 905, 96, 96 },
		{ "running past by an amount that wraps in 32 bits", 2000, 1000, 2048, 1024, 100, 0, 0xFFFFFFF0u, 96 },
		{ "a texture of no real size", 2000, 1000, 0, 0, 0, 0, 96, 96 },
		{ "a texture said to hold more than the card made", 4096, 1000, 2048, 1024, 0, 0, 96, 96 },
	};

	for (Refused const &one : refused)
	{
		float left[4] = { -1.0f, -2.0f, -3.0f, -4.0f };
		bool const given = QuadPart_UVs(one.width, one.height, one.realWidth, one.realHeight, one.u, one.v, one.partWidth, one.partHeight, left);

		Check(!given && left[0] == -1.0f && left[1] == -2.0f && left[2] == -3.0f && left[3] == -4.0f, std::string("(3) refused, the places untouched: ") + one.what);
	}

	printf(checksFailed ? "quad_part_test: %d checks FAILED\n" : "quad_part_test: all checks passed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
