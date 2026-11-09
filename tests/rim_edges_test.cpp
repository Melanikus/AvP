// The test of giving a picture's see-through texels their solid neighbours' colour (3dc/rim_edges.h, #148). Written 2026-11-08 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// RimEdges_ColourTheHoles on small made-up pictures: a hole beside solid texels takes their average colour and stays a hole; a hole with
// no solid neighbour, and every solid texel, are left exactly as they were; neighbours wrap round the edges, as the card reads them; the
// answer is the same whichever hole comes first; nothing past a row's width is touched; and sizes that can't be a picture are refused,
// with nothing changed. Then, reading the game's source as text, that Tex_CreateFromRIM does it after a picture has been decoded, and
// before the texture is let go of, for every picture but a film's.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this file on its own. The program gives back a non-zero result if anything failed.

#include <fstream>
#include <sstream>
#include <stdio.h>
#include <string>
#include <vector>

#include "rim_edges.h"

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

	std::string plain;

	for (char c : all.str())
	{
		if (c != '\r')
		{
			plain += c;
		}
	}

	return plain;
}

// A picture: 4 bytes a texel, blue, green, red, alpha, rows pitch bytes apart. Every texel starts as a hole of the "transparent colour",
// a bright pink, and the bytes past each row's width are filled with 0xAB, so a write there shows.
struct Picture
{
	uint32_t width, height, pitch;
	std::vector<uint8_t> bytes;

	Picture(uint32_t w, uint32_t h, uint32_t extra = 0) : width(w), height(h), pitch(w * 4 + extra), bytes((size_t)(w * 4 + extra) * h, 0xAB)
	{
		for (uint32_t y = 0; y < h; y++)
		{
			for (uint32_t x = 0; x < w; x++)
			{
				Set(x, y, 255, 0, 255, 0);
			}
		}
	}

	uint8_t *At(uint32_t x, uint32_t y) { return &bytes[(size_t)y * pitch + (size_t)x * 4]; }

	void Set(uint32_t x, uint32_t y, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
	{
		uint8_t *const t = At(x, y);
		t[0] = blue;
		t[1] = green;
		t[2] = red;
		t[3] = alpha;
	}

	bool Is(uint32_t x, uint32_t y, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
	{
		uint8_t const *const t = At(x, y);
		return t[0] == blue && t[1] == green && t[2] == red && t[3] == alpha;
	}

};

static bool ColourIt(Picture &p)
{
	return RimEdges_ColourTheHoles(&p.bytes[0], p.width, p.height, p.pitch);
}

static bool Has(std::string const &text, std::string const &what)
{
	return text.find(what) != std::string::npos;
}

static void TestTheHoles()
{
	puts("A picture's holes:");

	// One solid red texel in the middle of 5 x 5 holes
	{
		Picture p(5, 5);
		p.Set(2, 2, 200, 10, 20, 255);
		Check(ColourIt(p), "a picture is coloured");

		bool ring = true;

		for (uint32_t y = 1; y <= 3; y++)
		{
			for (uint32_t x = 1; x <= 3; x++)
			{
				if (x != 2 || y != 2)
				{
					ring = ring && p.Is(x, y, 200, 10, 20, 0);
				}
			}
		}

		Check(ring, "the eight holes round a solid texel take its colour, and stay holes, with an alpha of 0");
		Check(p.Is(2, 2, 200, 10, 20, 255), "the solid texel is left exactly as it was");
		Check(p.Is(0, 0, 255, 0, 255, 0) && p.Is(4, 4, 255, 0, 255, 0) && p.Is(0, 2, 255, 0, 255, 0),
			"a hole with no solid neighbour keeps its colour, since smooth filtering never mixes it with anything solid");
	}

	// A hole between two solid texels takes their average
	{
		Picture p(3, 3);
		p.Set(0, 1, 100, 0, 40, 255);
		p.Set(2, 1, 50, 200, 41, 255);
		ColourIt(p);
		Check(p.Is(1, 1, 75, 100, 40, 0), "a hole between two solid texels takes their average colour, rounded down");
	}

	// Two holes side by side in a row of four: each takes only its solid neighbour's colour, whichever is done first
	{
		Picture p(4, 1);
		p.Set(0, 0, 10, 20, 30, 255);
		p.Set(3, 0, 90, 80, 70, 255);
		ColourIt(p);
		// In a picture one row high, the rows above and below wrap round to the same row, so each solid texel counts three times
		Check(p.Is(1, 0, 10, 20, 30, 0) && p.Is(2, 0, 90, 80, 70, 0), "a hole never takes the colour another hole was just given, so the order doesn't matter");
	}

	// The neighbour across an edge is the one the card reads when the picture wraps
	{
		Picture p(4, 4);
		p.Set(3, 0, 1, 2, 3, 255);
		ColourIt(p);
		Check(p.Is(0, 0, 1, 2, 3, 0) && p.Is(0, 3, 1, 2, 3, 0) && p.Is(2, 3, 1, 2, 3, 0) && p.Is(3, 3, 1, 2, 3, 0),
			"neighbours wrap round the picture's edges and corners, as the card reads a picture drawn with wrap");
	}

	// A picture with no holes, and one with nothing solid, come out exactly as they went in
	{
		Picture solid(3, 2);

		for (uint32_t y = 0; y < 2; y++)
		{
			for (uint32_t x = 0; x < 3; x++)
			{
				solid.Set(x, y, (uint8_t)(x * 40), (uint8_t)(y * 90), 7, (uint8_t)(1 + x + y));
			}
		}

		std::vector<uint8_t> const before = solid.bytes;
		ColourIt(solid);
		Check(solid.bytes == before, "a picture with no holes is left exactly as it was, whatever its alphas");

		Picture empty(3, 3);
		std::vector<uint8_t> const emptyBefore = empty.bytes;
		ColourIt(empty);
		Check(empty.bytes == emptyBefore, "and so is one that is all holes");
	}

	// Rows longer than the picture, as a graphics card's can be
	{
		Picture p(2, 2, 8);
		p.Set(0, 0, 9, 9, 9, 255);
		ColourIt(p);

		bool padding = true;

		for (uint32_t y = 0; y < 2; y++)
		{
			for (uint32_t i = 8; i < p.pitch; i++)
			{
				padding = padding && p.bytes[(size_t)y * p.pitch + i] == 0xAB;
			}
		}

		Check(p.Is(1, 1, 9, 9, 9, 0), "a picture whose rows are longer than its width is still coloured");
		Check(padding, "and nothing past each row's width is read or written");
	}

	// Sizes that can't be a picture are refused, and nothing is touched
	{
		Picture p(4, 4);
		p.Set(1, 1, 50, 50, 50, 255);
		std::vector<uint8_t> const before = p.bytes;

		Check(!RimEdges_ColourTheHoles(0, 4, 4, 16), "no picture at all is refused");
		Check(!RimEdges_ColourTheHoles(&p.bytes[0], 0, 4, 16) && !RimEdges_ColourTheHoles(&p.bytes[0], 4, 0, 16), "and so is one of no width or no height");
		Check(!RimEdges_ColourTheHoles(&p.bytes[0], 4, 4, 15) && !RimEdges_ColourTheHoles(&p.bytes[0], 4, 4, 0),
			"and rows shorter than the picture's width, which would read and write past each row");
		Check(!RimEdges_ColourTheHoles(&p.bytes[0], 0x40000001u, 1, 16), "and a width so large that four bytes for each would overflow");
		Check(p.bytes == before, "and none of those changed a thing");
	}
}

static void TestTheGame()
{
	puts("The game does it to every picture it loads:");

	std::string const manager = ReadSourceFile("3dc/TextureManager.cpp");
	Check(Has(manager, "#include \"rim_edges.h\""), "TextureManager.cpp has it");

	size_t const start = manager.find("texID_t Tex_CreateFromRIM(");
	size_t const end = (start == std::string::npos) ? std::string::npos : manager.find("\n}\n", start);
	std::string const fromRim = (end == std::string::npos) ? "" : manager.substr(start, end - start);

	size_t const decode = fromRim.find("bool const decoded = newRim.Decode(dest, pitch);");
	size_t const colour = fromRim.find("if (decoded && usage == TextureUsage_Normal)\n\t{\n\t\tRimEdges_ColourTheHoles(dest, width, height, pitch);\n\t}");
	size_t const unlock = fromRim.find("R_UnlockTexture(newTexture);");

	Check(decode != std::string::npos && colour != std::string::npos && unlock != std::string::npos && decode < colour && colour < unlock,
		"a picture's holes are coloured after it's decoded, only if it decoded, and before the texture is let go of, for every picture but a film's");
	Check(fromRim.find("RimEdges_ColourTheHoles(") == fromRim.rfind("RimEdges_ColourTheHoles("), "once");
}

int main()
{
	TestTheHoles();
	TestTheGame();

	if (checksFailed)
	{
		printf("%d checks FAILED\n", checksFailed);
		return 1;
	}

	printf("All checks passed\n");
	return 0;
}
