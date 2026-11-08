// The test for 3dc/level_file_name.h, the check every file name from a level passes before it's opened. Written 2026-11-05 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// A level file can come from anyone, so each bad name here has to be refused for its own reason. A check that only asks whether a name was refused would let a later check catch what an earlier one missed, and hide
// that it's missing. The names the game's own levels really use have to go through. The last part reads the game's source, for what can't be run here: that both places a level's name reaches a file go through the check.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this, and the program gives back a non-zero result if any check failed. It needs no game files. /FC in this test's settings makes __FILE__ the whole path of this file, which is how the
// source files are found.

// Modified 2026-11-08 by Melanikus for the AvP port:
//   its Between is the shared one in source_slice.h, so a marker that isn't there is a failed check of its own (SOURCE-SLICE-1, #128)
#include <stdio.h>
#include <string.h>

#include <fstream>
#include <sstream>
#include <string>

#include "level_file_name.h"
#include "source_slice.h"

static int checksFailed = 0;

static void Check(bool good, const std::string &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static void Refused(const char *name, const char *reason)
{
	const char *const got = LevelFileName_WhyRefused(name);

	Check(got != 0 && strcmp(got, reason) == 0, std::string("[") + name + "] is refused: " + reason);
}

static void Fine(const char *name)
{
	Check(LevelFileName_WhyRefused(name) == 0, std::string("[") + name + "] may be opened");
}

static void TestTheGamesOwnNames()
{
	// real ones, one of each kind
	Fine("Envrnmts\\Derelict\\Frame & Panels.RIM");
	Fine("Envrnmts\\sulaco\\Pipe01-04@256.RIM");
	Fine("Envrnmts\\Station\\HANGAR&GRUNGE PIPE.RIM");
	Fine("mission pack env\\hadleyshope/wil_computer_readout_01_lp.wav");
	Fine("npc/marinevoice/Voice00_Angry01.wav");
	Fine("wil_alien_tailcrack.wav");
	Fine("a+b_c-d.rim");

	// nearly a device, and nearly going up
	Fine("console.rim");
	Fine("conx");
	Fine("auxiliary.rim");
	Fine("null.rim");
	Fine("com0");
	Fine("com10");
	Fine("lpt");
	Fine("comic.rim");
	Fine("a.b.c");
	Fine(".hidden");
	Fine("a/.b");

	std::string longest(kLevelFileNameMost, 'a');
	Fine(longest.c_str());
}

static void TestTheRefusals()
{
	Check(LevelFileName_WhyRefused(0) == kLevelNameEmpty, "no name at all is refused: " + std::string(kLevelNameEmpty));
	Refused("", kLevelNameEmpty);

	std::string tooLong(kLevelFileNameMost + 1, 'a');
	Refused(tooLong.c_str(), kLevelNameTooLong);

	Refused("/a.rim", kLevelNameStartsAtTheTop);
	Refused("\\a.rim", kLevelNameStartsAtTheTop);
	Refused("\\\\server\\share\\a.rim", kLevelNameStartsAtTheTop);
	Refused("\\\\?\\C:\\a.rim", kLevelNameStartsAtTheTop);
	Refused("/../a.rim", kLevelNameStartsAtTheTop);

	Refused("C:\\Users\\Public\\a.rim", kLevelNameCharacter);
	Refused("C:a.rim", kLevelNameCharacter);
	Refused("a.rim:hidden", kLevelNameCharacter);
	Refused("a.rim::$DATA", kLevelNameCharacter);
	Refused("a*.rim", kLevelNameCharacter);
	Refused("a?.rim", kLevelNameCharacter);
	Refused("a<b.rim", kLevelNameCharacter);
	Refused("a>b.rim", kLevelNameCharacter);
	Refused("a|b.rim", kLevelNameCharacter);
	Refused("a\"b.rim", kLevelNameCharacter);
	Refused("a\tb.rim", kLevelNameCharacter);
	Refused("a\nb.rim", kLevelNameCharacter);
	Refused("a\rb.rim", kLevelNameCharacter);
	Refused("CONIN$", kLevelNameCharacter);
	Refused("%TEMP%/a.rim", kLevelNameCharacter);
	Refused("~/a.rim", kLevelNameCharacter);
	Refused("a(1.rim", kLevelNameCharacter);
	Refused("a1).rim", kLevelNameCharacter);
	Refused("a\xC3\xA9.rim", kLevelNameCharacter);
	Refused("a\x7F.rim", kLevelNameCharacter);
	Refused("a\x01.rim", kLevelNameCharacter);

	Refused("..\\..\\..\\..\\Users\\Public\\a.rim", kLevelNameGoesUp);
	Refused("../a.rim", kLevelNameGoesUp);
	Refused("a/../b.rim", kLevelNameGoesUp);
	Refused("a\\..\\b.rim", kLevelNameGoesUp);
	Refused("a/..", kLevelNameGoesUp);
	Refused("..", kLevelNameGoesUp);
	Refused("...", kLevelNameGoesUp);
	Refused("a/.../b.rim", kLevelNameGoesUp);
	Refused("a..b.rim", kLevelNameGoesUp);

	Refused("a//b.rim", kLevelNameEmptyPart);
	Refused("a\\\\b.rim", kLevelNameEmptyPart);
	Refused("a/\\b.rim", kLevelNameEmptyPart);
	Refused("a/", kLevelNameEmptyPart);
	Refused("a\\", kLevelNameEmptyPart);

	Refused("a.", kLevelNamePartEnding);
	Refused("a ", kLevelNamePartEnding);
	Refused(".", kLevelNamePartEnding);
	Refused(" ", kLevelNamePartEnding);
	Refused("./a.rim", kLevelNamePartEnding);
	Refused("a./b.rim", kLevelNamePartEnding);
	Refused("a /b.rim", kLevelNamePartEnding);
	Refused(". ./a.rim", kLevelNamePartEnding);
	Refused(". /a.rim", kLevelNamePartEnding);
	Refused("a/b.", kLevelNamePartEnding);
	Refused("a\\b ", kLevelNamePartEnding);

	static const char *const kDevices[] = { "CON", "PRN", "AUX", "NUL", "COM1", "COM9", "LPT1", "LPT9", "con", "Prn", "aUx", "nul", "com5", "Lpt5" };

	for (size_t i = 0; i < sizeof(kDevices) / sizeof(kDevices[0]); i++)
	{
		std::string const device = kDevices[i];

		Refused(device.c_str(), kLevelNameDevice);
		Refused((device + ".rim").c_str(), kLevelNameDevice);
		Refused((device + " .rim").c_str(), kLevelNameDevice);
		Refused(("a/" + device).c_str(), kLevelNameDevice);
		Refused(("a\\" + device + ".wav").c_str(), kLevelNameDevice);
		Refused((device + "/a.rim").c_str(), kLevelNameDevice);
	}
}

static void TestTheLogLine()
{
	char line[8];

	LevelFileName_ForTheLog("abc", line, sizeof(line));
	Check(strcmp(line, "abc") == 0, "a plain name goes into the log as it is");

	LevelFileName_ForTheLog("a\nb:c", line, sizeof(line));
	Check(strcmp(line, "a?b?c") == 0, "and anything it shouldn't have shows as a question mark, so a name can't make a line of the log look like another");

	LevelFileName_ForTheLog("abcdefghijklmnop", line, sizeof(line));
	Check(strcmp(line, "abcdefg") == 0, "a long one is cut to fit, with its end marker");

	LevelFileName_ForTheLog(0, line, sizeof(line));
	Check(line[0] == 0, "no name at all is an empty line");

	line[0] = 'x';
	LevelFileName_ForTheLog("abc", line, 0);
	Check(line[0] == 'x', "and with no room, nothing is written");
}

// The text of a file in the repository. Windows line endings are taken out, because a copy of the repository can have them or not.
static std::string ReadSourceFile(const char *name)
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
	std::string::size_type place = 0;

	while ((place = text.find('\r', place)) != std::string::npos)
	{
		text.erase(place, 1);
	}

	return text;
}

// The text from one marker up to the next (source_slice.h). A marker that isn't there is a failed check of its own, named by the marker.
static std::string Between(const std::string &text, const char *from, const char *to)
{
	return SourceSlice(text, from, to, kSliceEndFromTheStart, [](const std::string &why) { Check(false, why.c_str()); });
}

static bool ComesBefore(const std::string &text, const char *first, const char *second)
{
	std::string::size_type const one = text.find(first);
	std::string::size_type const two = text.find(second);

	return one != std::string::npos && two != std::string::npos && one < two;
}

static bool Has(const std::string &text, const char *words)
{
	return text.find(words) != std::string::npos;
}

static int CountOf(const std::string &text, const char *words)
{
	int count = 0;

	for (std::string::size_type place = text.find(words); place != std::string::npos; place = text.find(words, place + 1))
	{
		count++;
	}

	return count;
}

static void TestTheRealGame()
{
	std::string const levelLoad = ReadSourceFile("3dc/avp/win95/projload.cpp");
	std::string const texture = Between(levelLoad, "texID_t load_rif_bitmap(char const * fname, BMPN_Flags flags)", "ingameTextureList.push_back(textureID);");

	Check(ComesBefore(texture, "LevelFileName_WhyRefused(fname)", "Tex_CreateFromRIM"), "a texture's name is checked before it's opened");
	Check(Has(texture, "whyRefused ? MISSING_TEXTURE : Tex_CreateFromRIM(\"graphics/\" + fileName)"), "and a refused one is a texture that isn't there, and is never opened");
	Check(Has(texture, "whyRefused ? \"\" : fname"), "and nothing goes on using its name");
	Check(Has(texture, "LevelFileName_ForTheLog(fname, safeName, sizeof(safeName));"), "and the log has a safe copy of it");
	Check(CountOf(levelLoad, "Tex_CreateFromRIM(") == 1, "and that's the only place the level loader opens a texture");

	std::string const sounds = ReadSourceFile("3dc/avp/win95/jsndsup.cpp");
	std::string const sound = Between(sounds, "LOADED_SOUND const * GetSound(char const *fname)", "int perm_snum");

	Check(ComesBefore(sound, "LevelFileName_WhyRefused(fname)", "strrchr"), "a sound's name is checked before anything else looks at it");
	Check(Has(Between(sound, "if (whyRefused)", "const char * wavname"), "return 0;"), "and a refused one is a sound that isn't there");
	Check(Has(sound, "LevelFileName_ForTheLog(fname, safeName, sizeof(safeName));"), "and the log has a safe copy of it");
	Check(CountOf(sounds, "FindAndLoadWavFile(") == 1, "and GetSound is the only way to a sound's file");

	std::string const npc = ReadSourceFile("3dc/avp/win95/npcsetup.cpp");

	Check(!Has(npc, "RIF_Child_Chunk") && !Has(npc, "rcc->"), "a name in a level's RIFCHILD chunk has no way to a file");
	Check(Has(ReadSourceFile("3dc/avp/win95/projload.cpp"), "if(phdc->hierarchy_index<0 || phdc->hierarchy_index>=NumPlacedHierarchy) continue;"), "and a placed hierarchy is only a number, into the game's own short list");
}

int main()
{
	printf("Which names from a level file may be opened:\n");

	TestTheGamesOwnNames();
	TestTheRefusals();
	TestTheLogLine();
	TestTheRealGame();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");

	return checksFailed ? 1 : 0;
}
