// The test of refusing a texture too small for what's going to be written into it. Written 2026-11-04 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// R_CreateTexture asks Direct3D's helper about a texture of the size a picture or film claims, and the helper can answer with a smaller one. Writing the claimed size into that goes past the
// end of the texture's memory. The check is in 3dc/texture_size_check.h, and this runs it on real sizes. The rest needs a graphics card, so this reads the game's source as text, to see the
// check is where it has to be: after the helper has answered, and before the texture is made.
//
// What the helper really answers was measured on one card with AvP-docs\engine\relief-study\d3dx-size-check.cpp, and those numbers are the first rows here.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this file on its own. The program gives back a non-zero result if anything failed.

#include <fstream>
#include <sstream>
#include <stdio.h>
#include <string>

#include "texture_size_check.h"

static int checksFailed = 0;

static void Check(bool passed, std::string const &what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what.c_str());

	if (!passed)
	{
		checksFailed++;
	}
}

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
	return all.str();
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

// What's between "name(" where it's defined and the next function at the left margin
static std::string FunctionBody(std::string const &text, std::string const &start)
{
	size_t const from = text.find(start);

	if (from == std::string::npos)
	{
		return "";
	}

	size_t const to = text.find("\n}", from);
	return text.substr(from, (to == std::string::npos) ? std::string::npos : to - from);
}

// A film has to stop right where it finds out, so the same words further down its file wouldn't count
static bool StopsWhenItHasNoTexture(std::string const &film)
{
	size_t const made = film.find("Tex_Create(");
	size_t const missing = (made == std::string::npos) ? made : film.find("== MISSING_TEXTURE", made);
	size_t const stops = (missing == std::string::npos) ? missing : film.find("return FMV_ERROR", missing);
	return stops != std::string::npos && stops - missing < 200;
}

static bool InOrder(std::string const &text, char const *const *pieces, int count)
{
	size_t at = 0;

	for (int i = 0; i < count; i++)
	{
		at = text.find(pieces[i], at);

		if (at == std::string::npos)
		{
			return false;
		}

		at++;
	}

	return true;
}

int main()
{
	printf("What the helper answered, measured on a card with a limit of 16384:\n");
	Check(!TextureSize_IsEnough(20000, 1, 32, 16384, 1, 32), "asked for 20000 x 1, offered 16384 x 1: refused");
	Check(!TextureSize_IsEnough(16385, 1, 32, 16384, 1, 32), "one pixel too wide: refused");
	Check(!TextureSize_IsEnough(65535, 1, 32, 16384, 1, 32), "as wide as a .RIM can say: refused");
	Check(!TextureSize_IsEnough(1, 20000, 32, 1, 16384, 32), "too tall: refused");
	Check(!TextureSize_IsEnough(1, 65535, 32, 1, 16384, 32), "as tall as a .RIM can say: refused");
	Check(TextureSize_IsEnough(16384, 1, 32, 16384, 1, 32), "as wide as the card allows: fine");
	Check(TextureSize_IsEnough(1, 16384, 32, 1, 16384, 32), "as tall as the card allows: fine");
	Check(TextureSize_IsEnough(0, 0, 32, 1, 1, 32), "no size at all, answered with 1 x 1: fine, because nothing gets written");
	Check(TextureSize_IsEnough(0, 5, 32, 1, 5, 32), "no width, answered with 1 x 5: fine for the same reason");

	printf("The game's own pictures and films:\n");
	Check(TextureSize_IsEnough(640, 480, 32, 640, 480, 32), "the largest stock picture");
	Check(TextureSize_IsEnough(128, 128, 32, 128, 128, 32), "a stock video screen");
	Check(TextureSize_IsEnough(640, 480, 8, 640, 480, 8), "the bright part of a film, a byte to a pixel");
	Check(TextureSize_IsEnough(320, 240, 8, 320, 240, 8), "the colour parts of a film, at half size");

	printf("Offers other than exactly what was asked for:\n");
	Check(TextureSize_IsEnough(640, 480, 32, 1024, 512, 32), "a card that wants powers of two gives more room, and that's fine");
	Check(TextureSize_IsEnough(100, 300, 32, 300, 300, 32), "a card that wants squares gives more room too");
	Check(!TextureSize_IsEnough(640, 480, 32, 640, 479, 32), "one row short: refused");
	Check(!TextureSize_IsEnough(640, 480, 32, 639, 480, 32), "one column short: refused");
	Check(!TextureSize_IsEnough(640, 480, 32, 640, 480, 16), "the right size in a format with smaller pixels: refused");
	Check(TextureSize_IsEnough(640, 480, 8, 640, 480, 32), "a format with bigger pixels leaves room, and that's fine");
	Check(!TextureSize_IsEnough(65536, 1, 32, 65536, 1, 32), "wider than the game can keep in its numbers, even if a card offers it: refused");
	Check(!TextureSize_IsEnough(1, 65536, 32, 1, 65536, 32), "taller, the same");
	Check(TextureSize_IsEnough(65535, 65535, 32, 65535, 65535, 32), "the largest it can keep: fine");

	printf("The bits in each format:\n");
	Check(TextureSize_BitsPerPixel(D3DFMT_A8R8G8B8) == 32, "the format the game asks for, for 32 bits");
	Check(TextureSize_BitsPerPixel(D3DFMT_A1R5G5B5) == 16, "the format the game asks for, for 16 bits");
	Check(TextureSize_BitsPerPixel(D3DFMT_L8) == 8, "the format the game asks for, for 8 bits");
	Check(TextureSize_BitsPerPixel(D3DFMT_X8R8G8B8) == 32 && TextureSize_BitsPerPixel(D3DFMT_A8B8G8R8) == 32 && TextureSize_BitsPerPixel(D3DFMT_X8B8G8R8) == 32, "the other 32-bit ones");
	Check(TextureSize_BitsPerPixel(D3DFMT_X1R5G5B5) == 16 && TextureSize_BitsPerPixel(D3DFMT_R5G6B5) == 16 && TextureSize_BitsPerPixel(D3DFMT_A4R4G4B4) == 16 && TextureSize_BitsPerPixel(D3DFMT_A8L8) == 16, "the other 16-bit ones");
	Check(TextureSize_BitsPerPixel(D3DFMT_DXT1) == 0 && TextureSize_BitsPerPixel(D3DFMT_UNKNOWN) == 0, "and anything not listed counts as too small");

	printf("Reading the game's own code:\n");
	std::string const device = ReadSourceFile("win32/d3_func.cpp");
	std::string const creator = FunctionBody(device, "bool R_CreateTexture(uint32_t width");
	Check(creator.size() > 100, "R_CreateTexture was found");

	char const *const order[] = { "D3DXCheckTextureRequirements(", "TextureSize_IsEnough(width, height, bitsPerPixel, realWidth, realHeight, TextureSize_BitsPerPixel(textureFormat))", "return false", "->CreateTexture(realWidth, realHeight," };
	Check(InOrder(creator, order, 4), "it asks the helper, checks the answer against what it was asked for, gives up if that's not enough, and only then makes the texture");
	Check(CountOf(device, "D3DXCheckTextureRequirements(") == 1, "and nothing else in that file asks the helper, so nothing else takes its answer unchecked");

	// Each of these has its size in the code, not from a file, and R_CreateTexture is the only one with a size from outside. A new one has to be looked at, so this counts them.
	Check(CountOf(device, "lpD3DDevice->CreateTexture(") == 5, "and it's still one of 5 places that make a texture: the menu font's at 512 x 512, two of 1 x 1, a square hand-made map, which checks for itself what it got, and this");

	std::string const manager = ReadSourceFile("3dc/TextureManager.cpp");
	std::string const fromRim = FunctionBody(manager, "texID_t Tex_CreateFromRIM(");
	char const *const fromRimOrder[] = { "R_CreateTexture(width, height, bitsPerPixel, usage, newTexture)", "return MISSING_TEXTURE", "R_LockTexture(", "R_ReleaseTexture(newTexture)", "return MISSING_TEXTURE", ".Decode(dest, pitch)" };
	Check(InOrder(fromRim, fromRimOrder, 6), "a picture stops if there's no texture, and lets go of the texture if it can't get at its memory, before anything is written");

	Check(StopsWhenItHasNoTexture(ReadSourceFile("3dc/video/BinkPlayback.cpp")), "a Bink film stops if there's no texture");
	Check(StopsWhenItHasNoTexture(ReadSourceFile("3dc/video/TheoraPlayback.cpp")), "and so does a Theora film");

	if (checksFailed)
	{
		printf("%d checks FAILED\n", checksFailed);
		return 1;
	}

	printf("All checks passed\n");
	return 0;
}
