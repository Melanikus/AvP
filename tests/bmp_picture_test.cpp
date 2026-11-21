// The .BMP reader for the level editor's Prefer .BMP over .RIM (3dc/bmp_picture.h), and where the level's loader uses it. Written 2026-11-18 by Melanikus
// (Frost) for the AvP port.
//
// WHY: a picture beside a level comes from anyone, so bmp_picture.h reads it itself instead of handing it to D3DX, and must refuse what it doesn't take
// (Tao 3: input is hostile). Each picture here is made up byte by byte, so the answer is known: the kinds it takes give exactly their texels, top row first,
// opaque; every kind it refuses is refused with its reason, and never with the texels touched. Then load_rif_bitmap and Tex_CreateFromBMP are read as text.

#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include "../3dc/bmp_picture.h"

static int checksFailed = 0;

static void Check(bool passed, const std::string &what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what.c_str());

	if (!passed)
	{
		checksFailed++;
	}
}

static void Put32(std::vector<uint8_t> &b, size_t at, uint32_t v)
{
	b[at] = (uint8_t)v;
	b[at + 1] = (uint8_t)(v >> 8);
	b[at + 2] = (uint8_t)(v >> 16);
	b[at + 3] = (uint8_t)(v >> 24);
}

static void Put16(std::vector<uint8_t> &b, size_t at, uint16_t v)
{
	b[at] = (uint8_t)v;
	b[at + 1] = (uint8_t)(v >> 8);
}

// A file of a picture: the two headers, the palette (8 bits), then the rows as given (already padded)
static std::vector<uint8_t> MakeBmp(int32_t width, int32_t height, uint16_t bits, uint32_t compression, std::vector<uint8_t> const &palette, uint32_t coloursUsed,
	std::vector<uint8_t> const &rows)
{
	std::vector<uint8_t> b(14 + 40 + palette.size() + rows.size(), 0);
	b[0] = 'B';
	b[1] = 'M';
	Put32(b, 2, (uint32_t)b.size());
	Put32(b, 10, (uint32_t)(14 + 40 + palette.size()));
	Put32(b, 14, 40);
	Put32(b, 18, (uint32_t)width);
	Put32(b, 22, (uint32_t)height);
	Put16(b, 26, 1);
	Put16(b, 28, bits);
	Put32(b, 30, compression);
	Put32(b, 46, coloursUsed);
	memcpy(&b[54], palette.data(), palette.size());
	memcpy(&b[54 + palette.size()], rows.data(), rows.size());
	return b;
}

static bool Texel(std::vector<uint8_t> const &t, uint32_t width, uint32_t x, uint32_t y, uint8_t blue, uint8_t green, uint8_t red)
{
	size_t const at = ((size_t)y * width + x) * 4;
	return at + 3 < t.size() && t[at] == blue && t[at + 1] == green && t[at + 2] == red && t[at + 3] == 255;
}

static void TestWhatItTakes()
{
	puts("The pictures it takes:");
	uint32_t w = 0, h = 0;
	std::vector<uint8_t> t;
	char const *why = 0;

	// 2 x 2, 24 bits, bottom row first: each row 6 bytes + 2 of padding. Bottom: blue, green; top: red, white
	std::vector<uint8_t> const rows24 = { 255, 0, 0, 0, 255, 0, 0, 0, 0, 0, 255, 255, 255, 255, 0, 0 };
	std::vector<uint8_t> const file24 = MakeBmp(2, 2, 24, 0, {}, 0, rows24);
	Check(BmpPicture_Decode(file24.data(), file24.size(), &w, &h, &t, &why) && w == 2 && h == 2 && t.size() == 16, "a 2 x 2 picture of 24 bits is taken");
	Check(Texel(t, 2, 0, 0, 0, 0, 255) && Texel(t, 2, 1, 0, 255, 255, 255) && Texel(t, 2, 0, 1, 255, 0, 0) && Texel(t, 2, 1, 1, 0, 255, 0),
		"its texels come top row first, blue, green, red, and opaque");

	// the same, stored top row first (a negative height)
	std::vector<uint8_t> const rowsTop = { 0, 0, 255, 255, 255, 255, 0, 0, 255, 0, 0, 0, 255, 0, 0, 0 };
	std::vector<uint8_t> const fileTop = MakeBmp(2, -2, 24, 0, {}, 0, rowsTop);
	Check(BmpPicture_Decode(fileTop.data(), fileTop.size(), &w, &h, &t, &why) && h == 2 && Texel(t, 2, 0, 0, 0, 0, 255) && Texel(t, 2, 1, 1, 0, 255, 0),
		"a picture stored top row first gives the same texels");

	// 3 x 1, 8 bits, a palette of 2 colours: indices 1, 0, 1 (+1 byte of padding)
	std::vector<uint8_t> const palette = { 10, 20, 30, 0, 40, 50, 60, 0 };
	std::vector<uint8_t> const file8 = MakeBmp(3, 1, 8, 0, palette, 2, { 1, 0, 1, 0 });
	Check(BmpPicture_Decode(file8.data(), file8.size(), &w, &h, &t, &why) && w == 3 && h == 1 && Texel(t, 3, 0, 0, 40, 50, 60) && Texel(t, 3, 1, 0, 10, 20, 30),
		"a picture of 8 bits takes its colours from its palette");

	// 1 x 1, 32 bits: its fourth byte is not used as see-through
	std::vector<uint8_t> const file32 = MakeBmp(1, 1, 32, 0, {}, 0, { 7, 8, 9, 0 });
	Check(BmpPicture_Decode(file32.data(), file32.size(), &w, &h, &t, &why) && Texel(t, 1, 0, 0, 7, 8, 9), "a picture of 32 bits is taken, opaque whatever its fourth byte says");
}

static void Refused(std::vector<uint8_t> const &file, char const *what)
{
	uint32_t w = 77, h = 88;
	std::vector<uint8_t> t(3, 0xab);
	char const *why = 0;
	bool const taken = BmpPicture_Decode(file.data(), file.size(), &w, &h, &t, &why);
	Check(!taken && why && w == 77 && h == 88 && t.size() == 3 && t[0] == 0xab, std::string(what) + " is refused, untouched (" + (why ? why : "no reason") + ")");
}

static void TestWhatItRefuses()
{
	puts("The pictures it refuses:");
	std::vector<uint8_t> const rows = { 1, 2, 3, 0 };
	std::vector<uint8_t> good = MakeBmp(1, 1, 24, 0, {}, 0, rows);

	std::vector<uint8_t> notBm = good;
	notBm[0] = 'X';
	Refused(notBm, "a file that doesn't start with BM");

	std::vector<uint8_t> oldHeader = good;
	Put32(oldHeader, 14, 12);
	Refused(oldHeader, "an OS/2 header of 12 bytes");

	Refused(MakeBmp(1, 1, 8, 1, std::vector<uint8_t>(8, 0), 2, rows), "a run-length picture (compression 1)");
	Refused(MakeBmp(1, 1, 32, 3, {}, 0, rows), "a picture with bit fields (compression 3)");
	Refused(MakeBmp(1, 1, 16, 0, {}, 0, rows), "a picture of 16 bits");
	Refused(MakeBmp(1, 1, 4, 0, std::vector<uint8_t>(64, 0), 16, rows), "a picture of 4 bits");
	Refused(MakeBmp(0, 1, 24, 0, {}, 0, rows), "a picture 0 texels wide");
	Refused(MakeBmp(4097, 1, 24, 0, {}, 0, rows), "a picture 4097 texels wide");
	Refused(MakeBmp(1, (int32_t)0x80000000u, 24, 0, {}, 0, rows), "a height of -2147483648");
	Refused(MakeBmp(1, 4097, 24, 0, {}, 0, rows), "a picture 4097 texels high whose rows aren't there");
	Refused(MakeBmp(1, 1, 8, 0, std::vector<uint8_t>(8, 0), 300, rows), "a palette of 300 colours");
	Refused(MakeBmp(1, 1, 8, 0, std::vector<uint8_t>(8, 0), 2, { 5, 0, 0, 0 }), "a texel naming colour 5 of a palette of 2");
	Refused(MakeBmp(2, 2, 24, 0, {}, 0, rows), "a picture whose rows run past the end of the file");

	std::vector<uint8_t> texelsInHeader = good;
	Put32(texelsInHeader, 10, 20);
	Refused(texelsInHeader, "texels said to start inside the header");

	std::vector<uint8_t> texelsPast = good;
	Put32(texelsPast, 10, 0xfffffff0u);
	Refused(texelsPast, "texels said to start past the end (a number that would wrap)");

	Refused(std::vector<uint8_t>(good.begin(), good.begin() + 30), "a file cut short in its header");
}

static std::string ReadSource(const char *name)
{
	FILE *file = fopen(name, "rb");
	std::string text;

	if (!file)
	{
		return text;
	}

	char block[4096];
	size_t got = 0;

	while ((got = fread(block, 1, sizeof(block), file)) > 0)
	{
		for (size_t i = 0; i < got; i++)
		{
			if (block[i] != '\r')
			{
				text += block[i];
			}
		}
	}

	fclose(file);
	return text;
}

static std::string Slice(std::string const &text, char const *from, char const *to)
{
	size_t const a = text.find(from);
	size_t const b = a == std::string::npos ? a : text.find(to, a);
	return b == std::string::npos ? std::string() : text.substr(a, b - a);
}

static void TestWhereItIsUsed()
{
	puts("Where the level's loader uses it, by reading:");
	std::string const load = ReadSource("3dc/avp/win95/projload.cpp");
	std::string const textures = ReadSource("3dc/TextureManager.cpp");
	Check(load.size() > 10000 && textures.size() > 10000, "projload.cpp and TextureManager.cpp were found and read, from the top of the repository");

	std::string const loader = Slice(load, "texID_t load_rif_bitmap(char const * fname, BMPN_Flags flags)\n{", "\n}\n");
	Check(load.find("\nstatic bool editorPreferBmp = false;\n") != std::string::npos, "the editor's Prefer .BMP starts off, so the game never reads a .bmp");
	Check(loader.find("if (!whyRefused && editorPreferBmp)") != std::string::npos, "the .bmp is tried only while the editor asks, and only for a name the level file's check took");
	Check(loader.find("std::string::size_type const dot = fileName.rfind('.');") != std::string::npos &&
		loader.find("Tex_CreateFromBMP(\"graphics/\" + fileName.substr(0, dot) + \".bmp\")") != std::string::npos, "with the ending after the name's last '.' made .bmp, as inteng's FUN_0046df30");
	Check(loader.find("if (textureID == MISSING_TEXTURE)") != std::string::npos && loader.find("Tex_CreateFromRIM(\"graphics/\" + fileName)") != std::string::npos,
		"and the .RIM is taken when there's no .bmp, or it is refused");

	std::string const maker = Slice(textures, "texID_t Tex_CreateFromBMP(const std::string &fileName)\n{", "\n}\n");
	Check(!maker.empty() && maker.find("BmpPicture_Decode(") != std::string::npos && maker.find("D3DX") == std::string::npos && maker.find("R_CreateTextureFromFile") == std::string::npos,
		"Tex_CreateFromBMP decodes with bmp_picture.h and hands nothing of the file to D3DX");
	Check(maker.find("size > kBmpPictureMostBytes + 2048") != std::string::npos, "and reads no file larger than a picture can be");
}

int main()
{
	TestWhatItTakes();
	TestWhatItRefuses();
	TestWhereItIsUsed();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
