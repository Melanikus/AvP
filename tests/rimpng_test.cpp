// The test of tools/rimpng, the tool for making hand-made maps. Written 2026-11-05 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// That the tool still builds, because the automatic checks build this with it, and nothing else would notice if it stopped. Then a whole trip through it, the way someone making a map would go:
// a picture to a PNG, and the PNG to a map, which is then checked with the same code the game checks it with. And what it does when it's asked for something it can't do, or runs out of memory.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this with the tool's own source, which leaves its main out when RIMPNG_TEST is set. The program gives back a non-zero result if anything failed.

#include "rimpng.cpp"

#include <new>

static int checksFailed = 0;

static void Check(bool passed, std::string const &what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what.c_str());

	if (!passed)
	{
		checksFailed++;
	}
}

// Runs the tool as if from the command line
static int Run(std::vector<std::string> words)
{
	words.insert(words.begin(), "rimpng");

	std::vector<char *> argv;

	for (size_t i = 0; i < words.size(); i++)
	{
		argv.push_back(&words[i][0]);
	}

	return RunTool((int)argv.size(), &argv[0]);
}

static std::string Folder()
{
	static std::string folder;

	if (folder.empty())
	{
		char *name = _tempnam(NULL, "rimpngtest");
		folder = name ? name : "rimpngtest";
		free(name);
		_mkdir(folder.c_str());

		for (size_t i = 0; i < folder.size(); i++)
		{
			folder[i] = (folder[i] == '\\') ? '/' : folder[i];
		}
	}

	return folder;
}

static void Add(std::vector<uint8_t> &to, char const *text)
{
	to.insert(to.end(), text, text + strlen(text));
}

static void Add32(std::vector<uint8_t> &to, uint32_t value)
{
	for (int shift = 24; shift >= 0; shift -= 8)
	{
		to.push_back((uint8_t)(value >> shift));
	}
}

// A picture of one colour, plain, with one plane and a table of two colours, so every pixel is the second colour
static std::vector<uint8_t> PictureOfOneColour(uint32_t width, uint32_t height, uint8_t red, uint8_t green, uint8_t blue, uint32_t packing)
{
	std::vector<uint8_t> file;
	uint32_t const rowBytes = (width + 7) / 8;

	Add(file, "FORM");
	Add32(file, 0);
	Add(file, "ILBMBMHD");
	Add32(file, 20);
	file.push_back((uint8_t)(width >> 8));
	file.push_back((uint8_t)width);
	file.push_back((uint8_t)(height >> 8));
	file.push_back((uint8_t)height);
	Add32(file, 0);
	file.push_back(1);
	file.push_back(0);
	file.push_back((uint8_t)packing);
	file.push_back(0);
	Add32(file, 0);
	Add32(file, 0);
	Add(file, "CMAP");
	Add32(file, 6);
	file.insert(file.end(), 3, 0);
	file.push_back(red);
	file.push_back(green);
	file.push_back(blue);
	Add(file, "BODY");

	if (packing == 0)
	{
		Add32(file, rowBytes * height);
		file.insert(file.end(), (size_t)rowBytes * height, 0xFF);
	}
	else
	{
		// the longest run there is, as few as can make each row: 2 bytes for each 128
		uint32_t const pairs = (rowBytes + 127) / 128;
		Add32(file, pairs * 2 * height);

		for (uint32_t i = 0; i < pairs * height; i++)
		{
			file.push_back((uint8_t)(int8_t)-127);
			file.push_back(0xFF);
		}
	}

	return file;
}

static int ThrowsOutOfMemory(int, char **)
{
	throw std::bad_alloc();
}

static int GivesBackSeven(int, char **)
{
	return 7;
}

int main()
{
	std::string const folder = Folder();
	std::string const picture = folder + "/wall.rim";
	std::string const png = folder + "/wall.png";
	std::string const normalMap = folder + "/wall_Normal.rim";
	std::string const depthMap = folder + "/wall_Depth.rim";
	std::vector<uint8_t> const pictureBytes = PictureOfOneColour(8, 8, 128, 128, 255, 0);

	printf("A trip through it:\n");

	Check(WriteWholeFile(picture, &pictureBytes[0], pictureBytes.size()), "a picture to start from");
	Check(Run({ "topng", picture, png }) == 0, "a picture becomes a PNG");

	std::vector<uint8_t> bytes;
	Check(ReadWholeFile(png, bytes) && bytes.size() > 8 && bytes[0] == 0x89 && bytes[1] == 'P' && bytes[2] == 'N' && bytes[3] == 'G', "and it really is one");

	Check(Run({ "tomap", png, normalMap }) == 0, "a PNG becomes a relief map");
	bytes.clear();
	Check(ReadWholeFile(normalMap, bytes), "which is there");

	ReliefMapRim rim;
	ReliefMapProblem problem = bytes.empty() ? kMapNoHeading : ReliefMap_FindRimParts(&bytes[0], bytes.size(), &rim);
	Check(problem == kMapFine && rim.side == 8, "and the game's own check takes it, at its own size");

	std::vector<uint8_t> pixels((size_t)8 * 8 * 4);
	Check(problem == kMapFine && ReliefMap_DecodeRim(rim, &pixels[0]) == kMapFine, "all the way through");

	Check(Run({ "tomap", png, depthMap }) == 0, "a PNG becomes a depth map");
	bytes.clear();
	Check(ReadWholeFile(depthMap, bytes), "which is there");
	problem = bytes.empty() ? kMapNoHeading : ReliefMap_FindRimParts(&bytes[0], bytes.size(), &rim);
	Check(problem == kMapFine && rim.side == 8, "and the game's own check takes that too");

	printf("What it can't do:\n");

	Check(Run({}) == 1, "with nothing asked, it says how to use it, and gives back a failure");
	Check(Run({ "nonsense", "a", "b" }) == 1, "and with something it doesn't know");
	Check(Run({ "topng", folder + "/not-there.rim", png }) == 1, "a picture that isn't there fails");
	Check(Run({ "tomap", folder + "/not-there.png", normalMap }) == 1, "and so does a PNG that isn't there");
	Check(Run({ "tomap", png, folder + "/wall.txt" }) == 1, "a map that isn't named as one is refused");
	Check(Run({ "tomap", png, folder + "/wall_Depth.dds" }) == 1, "and so is a depth map as a .dds, which the game doesn't read");
	Check(Run({ "tomap", png, normalMap, "--nonsense" }) == 1, "and an option it doesn't know");
	Check(Run({ "tomap", png, normalMap, "--side", "nonsense" }) == 1, "and a side that isn't a number");

	printf("Running out of memory:\n");

	char word[] = "rimpng";
	char *argv[] = { word };
	Check(RunWithMemoryCheck(GivesBackSeven, 1, argv) == 7, "what the tool gives back comes through");
	Check(RunWithMemoryCheck(ThrowsOutOfMemory, 1, argv) == 1, "and running out of memory says so and fails, where it used to stop with no word");

	printf("Reading many pictures:\n");

	// A level's limit on its pictures means nothing here, so use it all up, and the tool should still work
	std::vector<uint8_t> const large = PictureOfOneColour(2048, 2048, 1, 2, 3, 1);
	std::string const largeName = folder + "/large.rim";
	Check(WriteWholeFile(largeName, &large[0], large.size()), "a picture as large as there can be");

	RimLoader_NewLevel();

	for (int i = 0; i < 17; i++)
	{
		RimLoader spent;
		spent.Open(largeName);
	}

	Check(RimLoader_BytesThisLevel() == (uint64_t)256 * 1024 * 1024, "uses up a level's allowance");
	Check(Run({ "topng", picture, png }) == 0, "and the tool still reads a picture after that, because its work isn't a level");

	remove(picture.c_str());
	remove(png.c_str());
	remove(normalMap.c_str());
	remove(depthMap.c_str());
	remove(largeName.c_str());
	_rmdir(folder.c_str());

	if (checksFailed)
	{
		printf("%d checks FAILED\n", checksFailed);
		return 1;
	}

	printf("All checks passed\n");
	return 0;
}
