// Tests for where an animated texture's corners are on its picture between two frames (issue #97, reported by Bobblen: the lava of
// Fury 161, the tunnel lights of Earthbound and the predalien sphere of Vaults jerked and shook). The first part tries
// texture_animation.cpp, which knows nothing about the game. The second part reads the game's own files, to see that the corners are
// worked out there and read in 16.16 where an animated polygon is drawn, and that the loader refuses places that wouldn't fit.
// Written 2026-11-08 by Melanikus for the AvP port.
#include "texture_animation.h"

#include <climits>
#include <fstream>
#include <sstream>
#include <stdio.h>
#include <string>

static int checksFailed = 0;

static void Check(bool good, const std::string &what)
{
	printf("%s  %s\n", good ? "ok    " : "FAILED", what.c_str());
	checksFailed += good ? 0 : 1;
}

static std::string ReadSourceFile(char const *name)
{
	// /FC in this test's settings makes __FILE__ the whole path of this file, so everything else can be found from it
	std::string path = __FILE__;
	std::string::size_type const tests = path.rfind("tests");

	if (tests == std::string::npos)
	{
		return "";
	}

	std::ifstream file((path.substr(0, tests) + name).c_str(), std::ios::binary);
	std::stringstream all;
	all << file.rdbuf();

	// Files here may have Windows' line ends, and what's looked for is written with plain ones
	std::string text = all.str(), plain;

	for (char const c : text)
	{
		if (c != '\r')
		{
			plain += c;
		}
	}

	return plain;
}

static bool Has(const std::string &text, const char *what)
{
	return text.find(what) != std::string::npos;
}

static int CountOf(const std::string &text, const char *what)
{
	int count = 0;

	for (std::string::size_type at = text.find(what); at != std::string::npos; at = text.find(what, at + 1))
	{
		count++;
	}

	return count;
}

static std::string Between(const std::string &text, const char *from, const char *to)
{
	std::string::size_type const start = text.find(from);
	std::string::size_type const end = start == std::string::npos ? start : text.find(to, start);

	return (start == std::string::npos || end == std::string::npos) ? std::string() : text.substr(start, end - start);
}

static bool ComesBefore(const std::string &text, const char *first, const char *second)
{
	std::string::size_type const a = text.find(first), b = text.find(second);

	return a != std::string::npos && b != std::string::npos && a < b;
}

static const int kOne = 65536;   // a texel, in 16.16

// One corner's place across the picture, between two frames that put it at first and second, alpha of the way from one to the other
static int PlaceBetween(int first, int second, int alpha, bool interpolate = true)
{
	int const firstFrame[2] = { first, 0 };
	int const secondFrame[2] = { second, 0 };
	int out[2] = { -1, -1 };

	TextureAnimation_Corners(firstFrame, secondFrame, 1, alpha, interpolate, out);

	return out[0];
}

static void TestTheCorners()
{
	// The bug, by Fury 161's own numbers: its molten lead's first corner sits at texel 24 across in a frame and at 24 in the next one too,
	// so it doesn't move at all. The old sum gave 23 for all but 8 of the 65,536 moments between the two.
	bool stillIsStill = true;

	for (int alpha = 0; alpha <= kOne; alpha++)
	{
		stillIsStill = stillIsStill && PlaceBetween(24, 24, alpha) == 24 * kOne;
	}

	Check(stillIsStill, "a corner both frames put at the same place stays exactly there, at every moment between them");

	// And the next corner of the same polygon flows from 24 to 20: it moves by the same small step at every moment, never a whole texel
	bool smooth = true;

	for (int alpha = 0; alpha <= kOne; alpha++)
	{
		smooth = smooth && PlaceBetween(24, 20, alpha) == 24 * kOne - 4 * alpha;
	}

	Check(smooth, "a corner moving from 24 to 20 moves smoothly, by a fraction of a texel at a time");
	Check(PlaceBetween(24, 20, kOne / 2) == 22 * kOne, "halfway, it's exactly halfway");
	Check(PlaceBetween(24, 20, 1) == 24 * kOne - 4, "and one moment in, it has moved by less than a texel");
	Check(PlaceBetween(24, 20, 0) == 24 * kOne && PlaceBetween(24, 20, kOne) == 20 * kOne, "it starts on the first frame and ends on the second");

	// Both numbers of every corner are worked out, across and down, for a polygon of three or four corners
	int const firstFrame[8] = { 61, 8, 24, 47, 61, 47, 10, 11 };
	int const secondFrame[8] = { 51, 3, 26, 48, 58, 43, 12, 13 };
	int out[8] = { 0 };
	TextureAnimation_Corners(firstFrame, secondFrame, 4, kOne / 2, true, out);

	bool everyNumber = true;

	for (int i = 0; i < 8; i++)
	{
		everyNumber = everyNumber && out[i] == (firstFrame[i] + secondFrame[i]) * (kOne / 2);
	}

	Check(everyNumber, "every corner's two numbers are worked out, for a polygon of four corners");

	int untouched[8] = { 7, 7, 7, 7, 7, 7, 7, 7 };
	TextureAnimation_Corners(firstFrame, secondFrame, 3, kOne / 2, true, untouched);
	Check(untouched[5] == (47 + 43) * (kOne / 2) && untouched[6] == 7 && untouched[7] == 7, "and for three corners, only those are written");

	// An animation that steps shows its first frame's places, whatever the moment
	Check(PlaceBetween(24, 20, kOne / 2, false) == 24 * kOne && PlaceBetween(24, 20, kOne - 1, false) == 24 * kOne, "an animation that steps shows the first frame's places");
	Check(PlaceBetween(127, 0, 12345, false) == 127 * kOne, "in 16.16 too");

	// The moment is kept between the two frames, whatever it's given
	Check(PlaceBetween(24, 20, -5) == 24 * kOne && PlaceBetween(24, 20, INT_MIN) == 24 * kOne, "a moment before the first frame is the first frame");
	Check(PlaceBetween(24, 20, kOne + 5) == 20 * kOne && PlaceBetween(24, 20, INT_MAX) == 20 * kOne, "and one after the second is the second");

	// The furthest places there can be still fit, and come out exactly
	int const furthest = kTextureAnimationFurthestTexel;
	Check(PlaceBetween(furthest, furthest, kOne / 3) == furthest * kOne, "the furthest place there can be comes out exactly");
	Check(PlaceBetween(-furthest, -furthest, kOne / 3) == -furthest * kOne, "and the furthest the other way");
	Check(PlaceBetween(furthest, -furthest, kOne / 2) == 0, "and halfway between the two is the middle");
	Check((long long)furthest * kOne * 2 <= INT_MAX, "and the game's clipping, which takes the difference between two corners, never runs over");
}

static void TestThePlaces()
{
	int const stock[6] = { 0, 8, 24, 47, 61, 127 };
	Check(TextureAnimation_PlacesFit(stock, 6), "places like the game's own, 0 to 127, fit");
	Check(TextureAnimation_PlacesFit(stock, 0), "and no places at all fit too");

	int const edges[2] = { kTextureAnimationFurthestTexel, -kTextureAnimationFurthestTexel };
	Check(TextureAnimation_PlacesFit(edges, 2), "the furthest places either way fit");

	int const tooFar[2] = { 0, kTextureAnimationFurthestTexel + 1 };
	int const tooFarBack[2] = { -kTextureAnimationFurthestTexel - 1, 0 };
	int const biggest[1] = { INT_MAX };
	int const smallest[1] = { INT_MIN };
	Check(!TextureAnimation_PlacesFit(tooFar, 2), "one texel further doesn't");
	Check(!TextureAnimation_PlacesFit(tooFarBack, 2), "nor does one texel further the other way");
	Check(!TextureAnimation_PlacesFit(biggest, 1) && !TextureAnimation_PlacesFit(smallest, 1), "nor do the biggest and smallest numbers there are");

	int longList[600];

	for (int i = 0; i < 600; i++)
	{
		longList[i] = i % 128;
	}

	Check(TextureAnimation_PlacesFit(longList, 600), "a long list of places that fit, fits");
	longList[599] = kTextureAnimationFurthestTexel + 1;
	Check(!TextureAnimation_PlacesFit(longList, 600), "and one too far at the very end of it is found");
	Check(TextureAnimation_PlacesFit(longList, 599), "but only the places counted are read");

	Check(!TextureAnimation_PlacesFit(0, 6), "no list at all isn't places that fit");
	Check(!TextureAnimation_PlacesFit(stock, -1), "and nor is a count below nothing");
}

// The game's own files, read as text
static void TestJoiningTheGame()
{
	std::string const shapes = ReadSourceFile("3dc/kshape.cpp");
	std::string const render = ReadSourceFile("3dc/d3d_render.cpp");
	std::string const loader = ReadSourceFile("3dc/win95/chunk_load.cpp");
	std::string const animationChunk = ReadSourceFile("3dc/win95/animation_chunk.cpp");

	Check(!shapes.empty() && !render.empty() && !loader.empty() && !animationChunk.empty(), "the game's files are there to read");

	// The corners are worked out here, and nowhere else, for an animated polygon
	std::string const corners = Between(shapes, "void CreateTxAnimUVArray(int *txa_data, int *uv_array, int *shapeitemptr)\n{", "/* Interpolate Orient and Scale */");
	Check(Has(corners, "TextureAnimation_Corners(txaf0->txf_uvdata, txaf1->txf_uvdata, txaf0->txf_numuvs, Alpha, (txah->txa_flags & txa_flag_interpolate_uvs) != 0, uv_array);"), "CreateTxAnimUVArray works the corners out with the rules above, from the two frames and the moment between them");
	Check(!Has(corners, "MUL_FIXED(txaf0_uv") && !Has(shapes, "txaf0_uv"), "and the old sum, which lost a texel, is gone");
	Check(CountOf(shapes, "CreateTxAnimUVArray(texture_defn_ptr, uv_array, (int *)polyPtr);") == 3, "the three kinds of polygon that can be animated all ask it");

	// Where an animated polygon is drawn, its corners are read in 16.16
	Check(Has(render, "static float CornerNumbersPerTexel(const POLYHEADER *polygon)\n{\n\treturn (polygon->PolyFlags & iflag_txanim) ? 65536.0f : 1.0f;\n}"), "an animated polygon's corners have 65536 to a texel, and every other polygon's 1");

	std::string const textured = Between(render, "void D3D_ZBufferedGouraudTexturedPolygon_Output(POLYHEADER *inputPolyPtr, RENDERVERTEX *renderVerticesPtr, int shapePolygon)", "void D3D_ZBufferedGouraudPolygon_Output(");
	std::string const cloaked = Between(render, "void D3D_ZBufferedCloakedPolygon_Output(POLYHEADER *inputPolyPtr, RENDERVERTEX *renderVerticesPtr)", "\n}\n");
	char const across[] = "float RecipW = 1.0f / ((float)texWidth * CornerNumbersPerTexel(inputPolyPtr));";
	char const down[] = "float RecipH = 1.0f / ((float)texHeight * CornerNumbersPerTexel(inputPolyPtr));";
	Check(Has(textured, across) && Has(textured, down), "the textured polygons' drawing reads them that way, the level's, the gun's and the see-through list's");
	Check(Has(cloaked, across) && Has(cloaked, down), "and so does the cloaked polygons'");
	Check(CountOf(render, "CornerNumbersPerTexel(inputPolyPtr)") == 4, "and nothing else does");
	Check(Has(render, "float RecipW = (1.0f / (float) texWidth)  / 65536.0f;"), "the sky's corners were already 16.16, and stay so");

	// The loader refuses places too far out, before the animation is set up
	std::string const setUp = Between(loader, "void SetupAnimatedTextures(", "//create conversion between unmerged poly nos and merged poly nos");
	Check(Has(setUp, "if (!TextureAnimation_PlacesFit(sequence->UVCoords, 2LL * animation->NumVerts * sequence->NumFrames))\n\t\t\t{\n\t\t\t\treturn;"), "the loader leaves still a shape with a place that doesn't fit, before anything is set up");
	Check(ComesBefore(setUp, "ImagesAreKnown(local_max_index, local_tex_index_nos)", "TextureAnimation_PlacesFit("), "as it does one with a picture that isn't there");
	Check(Has(animationChunk, "static int const animation_corners = 3;") && Has(animationChunk, "if (corners != animation_corners || sequences < 1)"), "and the count it reads is two numbers for each of 3 corners, which the chunk's own check has made sure of");

	// The game builds the rules
	Check(Has(ReadSourceFile("win32/VS2026/AvP.vcxproj"), "3dc\\texture_animation.cpp"), "the game builds these rules");
	Check(Has(ReadSourceFile("win32/VS2026/AvP.vcxproj.filters"), "3dc\\texture_animation.cpp"), "and the project's list of files shows them");
}

int main()
{
	TestTheCorners();
	TestThePlaces();
	TestJoiningTheGame();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");

	return checksFailed ? 1 : 0;
}
