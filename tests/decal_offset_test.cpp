// The test of how a placed decal's texture offset is read from a level file and written back. Written 2026-11-07 by
// Melanikus for the AvP port. The fix follows Bobblen's avp-forever (282ebb1, 8a0e408), rewritten.
//
// WHAT IT TESTS
//
// 3dc/avp/decal_offset.h turns a level file's 16.16 offset into pixels and back, and refuses one that isn't a picture of
// the decal's kind. It holds nothing but plain numbers, so this test tries it with good and bad values: every offset
// the stock levels use, zero, negatives, parts of a pixel, and the largest numbers a file can hold.
//
// The loader and the writer, in projload.cpp, bring most of the game with them, so this reads them as text instead:
// each has to ask decal_offset.h. It also reads the two places that decide what an offset means, kshape.cpp (where it's
// added to the picture's box, in pixels) and paintball.cpp (where the game picks a picture itself), so a change to
// either is noticed. tests/takeouts/decal_offset_test.json proves each check can fail.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this with the settings in tests/decal_offset_test.args, and the program gives back a
// non-zero result if any check failed. It needs no game files. /FC in its settings is how the source files are found.

#include <limits.h>
#include <stdio.h>

#include <fstream>
#include <sstream>
#include <string>

#include "decal_offset.h"

static int checksFailed = 0;

static void Check(bool good, const std::string &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// One source file, with Windows line ends made plain so the text below can say what it means.
static std::string ReadSourceFile(char const *name)
{
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

	for (char const letter : text)
	{
		if (letter != '\r')
		{
			plain += letter;
		}
	}

	return plain;
}

// The text from one mark up to the next, or nothing if either isn't there.
static std::string Between(std::string const &text, std::string const &from, std::string const &to)
{
	std::string::size_type const start = text.find(from);

	if (start == std::string::npos)
	{
		return "";
	}

	std::string::size_type const end = text.find(to, start + from.size());

	return end == std::string::npos ? "" : text.substr(start, end - start);
}

static bool Has(std::string const &text, std::string const &part)
{
	return text.find(part) != std::string::npos;
}

// Is the first one there, the second one there too, and the first earlier?
static bool ComesBefore(std::string const &text, std::string const &first, std::string const &second)
{
	std::string::size_type const a = text.find(first);
	std::string::size_type const b = text.find(second);

	return a != std::string::npos && b != std::string::npos && a < b;
}

// The decal kinds the stock levels place, as decal.cpp's table has them: the step between pictures, and the last picture.
static int const kBloodStep = 32;
static int const kHumanBloodLast = 4;    // five splashes: 0, 32, 64, 96 and 128 pixels
static int const kPredatorBloodLast = 3; // four: 0, 32, 64 and 96 (android blood is the same)
static int const kOnePicture = 0;        // scorch marks, bullet holes and alien blood have one picture, at 0

// What the loader does with a level file's number: the pixels, or -1 if the decal is left out.
static int Read(int fileNumber, int step, int lastPicture)
{
	int pixels = -1;
	return DecalOffset_FromLevelFile(fileNumber, step, lastPicture, &pixels) ? pixels : -1;
}

static void TestTheStockOffsets()
{
	puts("Every offset the stock levels use, as a level file keeps it:");

	// From a read of all 86 stock levels: the only numbers their 16,378 placed decals hold
	Check(Read(0, kBloodStep, kHumanBloodLast) == 0, "0 is the first picture");
	Check(Read(2097152, kBloodStep, kHumanBloodLast) == 32, "2,097,152 is 32 pixels, human blood's second splash");
	Check(Read(4194304, kBloodStep, kHumanBloodLast) == 64, "4,194,304 is 64 pixels");
	Check(Read(6291456, kBloodStep, kHumanBloodLast) == 96, "6,291,456 is 96 pixels");
	Check(Read(8388608, kBloodStep, kHumanBloodLast) == 128, "8,388,608 is 128 pixels, its last splash");
	Check(Read(6291456, kBloodStep, kPredatorBloodLast) == 96, "96 pixels is predator blood's last splash");
	Check(Read(0, 0, kOnePicture) == 0, "and a kind with one picture takes 0");
}

static void TestTheOffsetsThatAreLeftOut()
{
	puts("Offsets that aren't a picture of their kind:");

	Check(Read(10485760, kBloodStep, kHumanBloodLast) == -1, "160 pixels, one past human blood's last splash, is left out");
	Check(Read(8388608, kBloodStep, kPredatorBloodLast) == -1, "and 128 pixels for predator blood, which has four");
	Check(Read(1048576, kBloodStep, kHumanBloodLast) == -1, "16 pixels, between two splashes, is left out");
	Check(Read(2097152, 0, kOnePicture) == -1, "32 pixels for a kind with one picture is left out");
	Check(Read(32, kBloodStep, kHumanBloodLast) == -1, "32 written as a plain number is a part of a pixel, and left out");
	Check(Read(2097153, kBloodStep, kHumanBloodLast) == -1, "and so is one 65,536th past 32 pixels");
	Check(Read(32768, kBloodStep, kHumanBloodLast) == -1, "and half a pixel");

	puts("Negative offsets:");

	Check(Read(-2097152, kBloodStep, kHumanBloodLast) == -1, "minus 32 pixels is left out, though it's a whole step");
	Check(Read(-65536, kBloodStep, kHumanBloodLast) == -1, "and minus one pixel");
	Check(Read(INT_MIN, kBloodStep, kHumanBloodLast) == -1, "and the most negative number a file can hold, a whole -32,768 pixels");

	puts("The largest numbers a file can hold:");

	Check(Read(INT_MAX, kBloodStep, kHumanBloodLast) == -1, "the largest is a part of a pixel, and left out");
	Check(Read(0x7fff0000, kBloodStep, kHumanBloodLast) == -1, "the largest whole one, 32,767 pixels, is left out");
	Check(Read(0x7fe00000, 32736, 1) == 32736, "a large whole number is still read exactly, for a kind whose picture is there: nothing overflows");

	puts("A kind's numbers that make no sense:");

	Check(Read(0, -32, kHumanBloodLast) == -1, "a negative step is refused");
	Check(Read(0, kBloodStep, -1) == -1, "and a negative last picture");

	int pixels = 77;
	Check(!DecalOffset_FromLevelFile(10485760, kBloodStep, kHumanBloodLast, &pixels) && pixels == 77, "a refused offset leaves the answer alone");
}

static void TestTheWriter()
{
	puts("Writing an offset back, as the debug decal save does:");

	Check(DecalOffset_ToLevelFile(32, kBloodStep, kHumanBloodLast) == 2097152, "32 pixels is written as 2,097,152");
	Check(DecalOffset_ToLevelFile(0, 0, kOnePicture) == 0, "0 is written as 0");

	bool allComeBack = true;

	for (int last = 0; last <= kHumanBloodLast; last++)
	{
		for (int picture = 0; picture <= last; picture++)
		{
			int const pixels = picture * kBloodStep;
			allComeBack = allComeBack && Read(DecalOffset_ToLevelFile(pixels, kBloodStep, last), kBloodStep, last) == pixels;
		}
	}

	Check(allComeBack, "every picture of every blood kind is read back as it was written");

	Check(DecalOffset_ToLevelFile(160, kBloodStep, kHumanBloodLast) == 0, "an offset that isn't a picture is written as 0, so the file still loads");
	Check(DecalOffset_ToLevelFile(-32, kBloodStep, kHumanBloodLast) == 0, "and so is a negative one");
	Check(DecalOffset_ToLevelFile(16, kBloodStep, kHumanBloodLast) == 0, "and one between pictures");
	Check(DecalOffset_ToLevelFile(32768, 32768, 1) == 0, "and a picture too far along for the file's number to hold, which would overflow");
	Check(DecalOffset_ToLevelFile(32736, 32736, 1) == 0x7fe00000, "while the largest the file can hold is written as it is");
}

static void TestTheLoaderAndTheWriter()
{
	puts("The level loader and the debug decal save, in projload.cpp:");

	std::string const projload = ReadSourceFile("3dc/avp/win95/projload.cpp");
	std::string const loader = Between(projload, "void setup_preplaced_decals(File_Chunk* fc,Environment_Data_Chunk* edc)\n{", "\nint get_object_index_from_module_index(");
	std::string const writer = Between(projload, "static void Fill_Decal_Data(AVP_Decal* decals, List<Object_Chunk*>& ob_list)\n{", "\nstatic AVP_Decal_Chunk* Make_Decal_Chunk(");

	Check(!loader.empty() && !writer.empty(), "the test finds the loader and the writer");
	Check(Has(projload, "#include \"decal_offset.h\""), "projload.cpp uses decal_offset.h");

	std::string const ask = "if (!DecalOffset_FromLevelFile(ad->UOffset, kind.UOffsetForSubclass, kind.MaxSubclassNumber, &offsetInPixels))";
	Check(ComesBefore(loader, "if (ad->DecalID < 0 || ad->DecalID >= MAX_NO_OF_DECAL_IDS) continue;", "DECAL_DESC const &kind = DecalDescription[ad->DecalID];"),
		"the loader only looks up a decal's kind after checking it's in the table");
	Check(ComesBefore(loader, "DECAL_DESC const &kind = DecalDescription[ad->DecalID];", ask), "and asks with that kind's step and last picture");
	Check(Has(loader, ask + "\n\t\t{\n\t\t\toffsetsRefused++;\n\t\t\tcontinue;\n\t\t}"), "a decal whose offset is refused is left out");
	Check(ComesBefore(loader, ask, "fd->UOffset=offsetInPixels;"), "and the decal is given the offset in pixels, after it's checked");
	Check(!Has(loader, "fd->UOffset=ad->UOffset;"), "not the file's number as it was");
	Check(Has(loader, "LogErrorString(\"level file: \" + Util::IntToString(offsetsRefused) + \" placed decals left out, as their texture offset isn't one of their kind's pictures\");"),
		"the log says how many were left out, once for the level");

	Check(Has(writer, "? DecalOffset_ToLevelFile(fd->UOffset, DecalDescription[kindNumber].UOffsetForSubclass, DecalDescription[kindNumber].MaxSubclassNumber)"),
		"the writer turns the offset back into the file's 16.16, with the decal's own kind");
	Check(Has(writer, "ad->UOffset = (kindNumber >= 0 && kindNumber < MAX_NO_OF_DECAL_IDS)\n\t\t\t?"), "and only looks the kind up when it's in the table");
	Check(!Has(writer, "ad->UOffset=fd->UOffset;"), "not the pixels as they are");
}

static void TestWhatAnOffsetMeans()
{
	puts("What the game does with an offset elsewhere:");

	std::string const kshape = ReadSourceFile("3dc/kshape.cpp");
	Check(Has(kshape, "VerticesBuffer[0].U = decalDescPtr->StartU + decalPtr->UOffset;") && Has(kshape, "VerticesBuffer[1].U = decalDescPtr->EndU + decalPtr->UOffset;"),
		"a decal's offset is added to its kind's box in pixels, which is why the loader turns it into pixels");

	std::string const paintball = ReadSourceFile("3dc/avp/paintball.cpp");
	Check(Has(paintball, "CurrentDecal.UOffset = PaintBallMode.CurrentDecalSubclass*decalDescPtr->UOffsetForSubclass;"),
		"the game makes an offset itself as a picture's number times its kind's step");
	Check(Has(paintball, "if (PaintBallMode.CurrentDecalSubclass>decalDescPtr->MaxSubclassNumber)"),
		"with pictures numbered up to the kind's last one, which is the rule decal_offset.h checks");
}

int main()
{
	TestTheStockOffsets();
	TestTheOffsetsThatAreLeftOut();
	TestTheWriter();
	TestTheLoaderAndTheWriter();
	TestWhatAnOffsetMeans();

	puts(checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
