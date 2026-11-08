// The test of putting a polygon's corners in the middle of the texels they name (3dc/texel_centre.h, #148). Written 2026-11-08 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// TexelCentre itself, on real sizes: the corner that names texel n lands in the middle of texel n, for corners in whole texels and in
// 16.16 fixed point alike, so a polygon that uses a whole texture never reaches either of its edges. Then, reading the game's source as
// text, that every 3D path that turns a corner into a texture coordinate goes through it, in its own units: the textured, cloaked,
// decal and sky polygons in d3d_render.cpp, and the still level's corners in static_level.cpp. A path that went back to the old sum
// would put that polygon's edge on the line between two texels again, and smooth filtering would mix the texture's other edge in.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this file on its own. The program gives back a non-zero result if anything failed.

#include <fstream>
#include <sstream>
#include <stdio.h>
#include <string>

#include "texel_centre.h"

static int checksFailed = 0;

static void Check(bool passed, std::string const &what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what.c_str());

	if (!passed)
	{
		checksFailed++;
	}
}

// A file of the repository as text, with Windows line ends taken out, since a copy of the repository can have them or not
static std::string ReadSourceFile(char const *name)
{
	// /FC in this test's settings makes __FILE__ the whole path of this file, so the repository's top is found from it
	std::string path = __FILE__;
	std::string::size_type const tests = path.rfind("tests");

	if (tests == std::string::npos)
	{
		return "";
	}

	std::ifstream file((path.substr(0, tests) + name).c_str(), std::ios::binary);
	std::stringstream all;
	all << file.rdbuf();

	std::string text = all.str();
	std::string plain;

	for (char c : text)
	{
		if (c != '\r')
		{
			plain += c;
		}
	}

	return plain;
}

static bool Has(std::string const &text, std::string const &what)
{
	return text.find(what) != std::string::npos;
}

static int CountOf(std::string const &text, std::string const &what)
{
	int count = 0;

	for (size_t at = text.find(what); at != std::string::npos; at = text.find(what, at + 1))
	{
		count++;
	}

	return count;
}

// A function's text, from "name(" where it's defined to the first closing brace at the left margin after it. Empty if it isn't there,
// so a check looking inside it fails instead of passing on nothing.
static std::string FunctionBody(std::string const &text, std::string const &start)
{
	size_t const from = text.find(start);

	if (from == std::string::npos)
	{
		return "";
	}

	size_t const to = text.find("\n}\n", from);
	return (to == std::string::npos) ? "" : text.substr(from, to - from);
}

static void TestTheSum()
{
	puts("Where a corner lands:");

	// Powers of two, so every answer here is exact in a float and can be compared with ==
	Check(TexelCentre(0, 1.0f, 1.0f / 128.0f) == 0.5f / 128.0f, "the first texel's corner is half a texel in from the edge, not on it");
	Check(TexelCentre(127, 1.0f, 1.0f / 128.0f) == 127.5f / 128.0f, "and the last texel's is half a texel short of the far edge");
	Check(TexelCentre(192, 1.0f, 1.0f / 256.0f) == 192.5f / 256.0f && TexelCentre(255, 1.0f, 1.0f / 256.0f) == 255.5f / 256.0f,
		"a polygon using texels 192 to 255 of a 256-wide texture shows the middles of 192 and 255, both ends included, as the game means");
	Check(TexelCentre(-1, 1.0f, 1.0f / 64.0f) == -0.5f / 64.0f, "a corner before the texture, on a texture that repeats, is still the middle of the texel it names");

	bool inside = true;
	bool inItsOwnTexel = true;

	for (int width = 1; width <= 1024; width *= 2)
	{
		float const recip = 1.0f / (float)width;

		for (int texel = 0; texel < width; texel++)
		{
			float const u = TexelCentre(texel, 1.0f, recip);

			inside = inside && u > 0.0f && u < 1.0f;
			inItsOwnTexel = inItsOwnTexel && u > (float)texel * recip && u < (float)(texel + 1) * recip;
		}
	}

	Check(inside, "for every texture from 1 to 1024 wide, no texel's corner lands on either edge, so nothing is read from the other edge");
	Check(inItsOwnTexel, "and every corner lands inside the very texel it names");

	// 16.16 fixed point: 65536 of the corner's numbers to a texel, and the drawing code's recip already has the 65536 in it
	float const recip1616 = (1.0f / 64.0f) / 65536.0f;
	Check(TexelCentre(5 << 16, 65536.0f, recip1616) == 5.5f / 64.0f, "a corner in 16.16 gets half a texel of its own numbers, 32768, not half of one of them");
	Check(TexelCentre((5 << 16) + 16384, 65536.0f, recip1616) == 5.75f / 64.0f, "and a corner a quarter of a texel on keeps its quarter");
}

static void TestTheGame()
{
	puts("Every path that makes a texture coordinate goes through it:");

	std::string const render = ReadSourceFile("3dc/d3d_render.cpp");
	Check(!render.empty(), "d3d_render.cpp is there to read");
	Check(Has(render, "#include \"texel_centre.h\""), "and it has the sum");

	std::string const textured = FunctionBody(render, "void D3D_ZBufferedGouraudTexturedPolygon_Output(");
	Check(Has(textured, "mainVertex[vb].u = TexelCentre(vertices->U, 1.0f, RecipW);\n\t\tmainVertex[vb].v = TexelCentre(vertices->V, 1.0f, RecipH);"),
		"the level's and the models' textured polygons, in whole texels");

	std::string const cloaked = FunctionBody(render, "void D3D_ZBufferedCloakedPolygon_Output(");
	Check(Has(cloaked, "mainVertex[vb].u = TexelCentre(vertices->U, 1.0f, RecipW);\n\t\tmainVertex[vb].v = TexelCentre(vertices->V, 1.0f, RecipH);"),
		"a cloaked Predator's polygons, in whole texels");

	std::string const decal = FunctionBody(render, "void D3D_Decal_Output(");
	Check(Has(decal, "decalVertex[decalVb].u = TexelCentre(vertices->U, 1.0f, RecipW);\n\t\tdecalVertex[decalVb].v = TexelCentre(vertices->V, 1.0f, RecipH);"),
		"decals, in whole texels");

	std::string const sky = FunctionBody(render, "void D3D_SkyPolygon_Output(");
	Check(Has(sky, "mainVertex[vb].u = TexelCentre(vertices->U, 65536.0f, RecipW);\n\t\tmainVertex[vb].v = TexelCentre(vertices->V, 65536.0f, RecipH);")
		&& Has(sky, "float RecipW = (1.0f / (float) texWidth)  / 65536.0f;"),
		"and the sky, whose corners are in 16.16, with half a texel of its own numbers");

	Check(CountOf(render, "(float)(vertices->U) * RecipW") == 0 && CountOf(render, "((float)vertices->U) * RecipW") == 0
		&& CountOf(render, "(float)(vertices->V) * RecipH") == 0 && CountOf(render, "((float)vertices->V) * RecipH") == 0,
		"and none of them is left on the old sum, which put a polygon's edge on the line between two texels");

	std::string const still = ReadSourceFile("3dc/static_level.cpp");
	Check(Has(still, "#include \"texel_centre.h\"")
		&& Has(still, "vertex.u = textured ? TexelCentre(coordinates[corner * 2 + 0], 1.0f, placed.recipWidth) : 0.0f;\n\t\tvertex.v = textured ? TexelCentre(coordinates[corner * 2 + 1], 1.0f, placed.recipHeight) : 0.0f;"),
		"the still level's corners, sent to the card once, are the same sum, so they match the game's to the last digit");
}

int main()
{
	TestTheSum();
	TestTheGame();

	if (checksFailed)
	{
		printf("%d checks FAILED\n", checksFailed);
		return 1;
	}

	printf("All checks passed\n");
	return 0;
}
