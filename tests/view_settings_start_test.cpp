// The test for 3dc/view_settings_start.h: what a new settings file starts with, and what a file from an earlier build gets, once. Written 2026-11-08
// by Melanikus for the AvP port.
//
// tests/build.ps1 compiles this, and the program gives back a non-zero result if any check failed. It uses no graphics card, and runs none of the
// game: the decisions are all in the header. The last rows read view_settings.cpp, to check the game does what the header says, in that order.
//
// The three cases the user's ruling asks for, and PO asked to see tested:
//   1. a new settings file: lighting per pixel and shadows on;
//   2. a file from an earlier build, which has both off: switched on, once;
//   3. that same file after its owner has turned them off again: they stay off.

#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include "view_settings_start.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// What a version word reads as, or -1 if it's refused. Refused, the version must be left as it was, so it starts at 77 to see that.
static int VersionOf(char const *word)
{
	int version = 77;

	if (!ViewSettings_VersionFromSavedWord(word, &version))
	{
		return version == 77 ? -1 : -2;
	}

	return version;
}

static void TheVersionWord()
{
	printf("The version line\n");
	Check(VersionOf("0") == 0 && VersionOf("1") == 1 && VersionOf("42") == 42, "plain numbers are read");
	Check(VersionOf("9999") == 9999 && VersionOf("0001") == 1, "up to four digits");
	Check(VersionOf("10000") == -1 && VersionOf("00001") == -1, "five digits are refused, whatever they say");
	Check(VersionOf("") == -1 && VersionOf(nullptr) == -1, "nothing at all is refused");
	Check(VersionOf(" 1") == -1 && VersionOf("1 ") == -1 && VersionOf("-1") == -1 && VersionOf("+1") == -1, "so are a space and a sign");
	Check(VersionOf("1.0") == -1 && VersionOf("1x") == -1 && VersionOf("x1") == -1 && VersionOf("0x1") == -1, "and a fraction, or anything after or before the number");
	Check(VersionOf("\xEF\xBC\x91") == -1, "and a digit from another alphabet");
	Check(VersionOf("-") != -2 && VersionOf("12345") != -2, "and a refused word leaves the version as it was");
}

// One start of the game: what the file had, and what the game starts with
struct Start
{
	ViewSettingsStart what;
	int lighting;
	bool shadows;
};

static Start StartWith(bool fileHadRendering, int version, int lighting, bool shadows)
{
	Start s;
	s.lighting = lighting;
	s.shadows = shadows;
	s.what = ViewSettings_LightsAtStart(fileHadRendering, version, &s.lighting, &s.shadows);
	return s;
}

static void TheThreeCases()
{
	printf("The three cases\n");

	// 1. A new file. view_settings.cpp reads a missing Shadows line as on and starts the lighting at per pixel, but the decision doesn't lean on that.
	Start s = StartWith(false, 0, kLightingGouraud, false);
	Check(s.what == kViewSettingsNewFile && s.lighting == kLightingPerPixel && s.shadows, "a new file starts with the lighting per pixel and the shadows on");

	// 2. A file from an earlier build: everything off, and no version line
	s = StartWith(true, 0, kLightingGouraud, false);
	Check(s.what == kViewSettingsSwitchedOn && s.lighting == kLightingPerPixel && s.shadows, "a file from an earlier build gets them switched on");

	// 3. The same file once it's had that, and its owner has turned them off again
	s = StartWith(true, kViewSettingsVersion, kLightingGouraud, false);
	Check(s.what == kViewSettingsAsSaved && s.lighting == kLightingGouraud && !s.shadows, "after that, a player who turned them off keeps them off");
}

static void OnceOnly()
{
	printf("Once, and only once\n");

	// The game's own order of starts. Each start's file is what the last one left: the version line written, and what the player chose since.
	int version = 0, lighting = kLightingGouraud;
	bool shadows = false;

	ViewSettingsStart what = ViewSettings_LightsAtStart(true, version, &lighting, &shadows);
	version = kViewSettingsVersion;
	Check(what == kViewSettingsSwitchedOn && lighting == kLightingPerPixel && shadows, "the first start of this build switches them on");

	what = ViewSettings_LightsAtStart(true, version, &lighting, &shadows);
	Check(what == kViewSettingsAsSaved && lighting == kLightingPerPixel && shadows, "the next start leaves them as they are");

	lighting = kLightingGouraud;
	shadows = false;
	what = ViewSettings_LightsAtStart(true, version, &lighting, &shadows);
	Check(what == kViewSettingsAsSaved && lighting == kLightingGouraud && !shadows, "turned off, they stay off at the start after");

	what = ViewSettings_LightsAtStart(true, version, &lighting, &shadows);
	Check(what == kViewSettingsAsSaved && lighting == kLightingGouraud && !shadows, "and at every start after that");
}

static void TheEdges()
{
	printf("The edges\n");

	// Switching on never takes a player down a step
	Start s = StartWith(true, 0, kLightingNormals, false);
	Check(s.what == kViewSettingsSwitchedOn && s.lighting == kLightingNormals && s.shadows, "an old file with normals keeps normals, and gets shadows");
	s = StartWith(true, 0, kLightingRelief, false);
	Check(s.lighting == kLightingRelief && s.shadows, "one with relief keeps relief");
	s = StartWith(true, 0, kLightingPerPixel, true);
	Check(s.what == kViewSettingsSwitchedOn && s.lighting == kLightingPerPixel && s.shadows, "one that had both already keeps both");
	s = StartWith(true, 0, kLightingGouraud, true);
	Check(s.lighting == kLightingPerPixel && s.shadows, "one with shadows but not per pixel gets per pixel");

	// A file from a later build than this one has had everything this one would do, and maybe more
	s = StartWith(true, kViewSettingsVersion + 1, kLightingGouraud, false);
	Check(s.what == kViewSettingsAsSaved && s.lighting == kLightingGouraud && !s.shadows, "a file from a later build is left as it is");
	s = StartWith(true, kViewSettingsMostVersion, kLightingGouraud, false);
	Check(s.what == kViewSettingsAsSaved && !s.shadows, "even at the most a version can say");

	// A file that has had this, but has lost its Rendering line since: what the game reads for a missing line stands
	s = StartWith(false, kViewSettingsVersion, kLightingPerPixel, false);
	Check(s.what == kViewSettingsAsSaved && s.lighting == kLightingPerPixel && !s.shadows, "a file at this version with no Rendering line isn't taken for a new one");

	Check(kViewSettingsVersion >= 1 && kViewSettingsVersion <= kViewSettingsMostVersion, "this build's version is one a file can say");
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

	// A file has one kind of line ending or the other depending on how it was cloned, and these rows shouldn't care.
	std::string text = all.str();
	text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
	return text;
}

// The text from one marker up to the next one after it. Empty if either isn't there, so a row looking inside it fails instead of passing on nothing.
static std::string Between(std::string const &text, char const *from, char const *to)
{
	std::string::size_type const start = text.find(from);

	if (start == std::string::npos)
	{
		return "";
	}

	std::string::size_type const end = text.find(to, start + 1);

	return (end == std::string::npos) ? "" : text.substr(start, end - start);
}

static bool Has(std::string const &text, char const *words)
{
	return text.find(words) != std::string::npos;
}

static void TheGame()
{
	printf("What the game does with it (view_settings.cpp, read as text)\n");
	std::string const settings = ReadSourceFile("3dc/view_settings.cpp");
	std::string const loading = Between(settings, "void ViewSettings_Load()", "void ViewSettings_MenuFrame()");

	Check(!loading.empty(), "ViewSettings_Load is there to read");
	Check(Has(settings, "#include \"view_settings_start.h\"") && Has(settings, "static const char kVersionSetting[] = \"VideoSettingsVersion\";"),
		"it uses these decisions, and the file's line is VideoSettingsVersion");

	// Reading the file
	Check(Has(loading, "Config_GetString(kHeading, kRendering, \"\")") && Has(loading, "bool const fileHadRendering = !rendering.empty();"),
		"a new file is known by having no Rendering line");
	Check(Has(loading, "Config_GetString(kHeading, kVersionSetting, \"0\")"), "a file with no version line is version 0");
	Check(Has(loading, "ViewSettings_VersionFromSavedWord(versionWord.c_str(), &version)"), "the version line goes through the strict reader");
	Check(Has(loading, "\t\tversion = kViewSettingsVersion;\n\t\tConfig_SetString(kHeading, kVersionSetting, std::to_string(kViewSettingsVersion));"),
		"a version line changed by hand switches nothing on, and is put right");
	Check(Has(loading, "so it's PERPIXEL\");\n\t\tchoice = kLightingPerPixel;"), "a Rendering line that isn't a choice is taken as the new start, per pixel");

	// Deciding, then starting
	std::string::size_type const decided = loading.find("ViewSettingsStart const start = ViewSettings_LightsAtStart(fileHadRendering, version, &choice, &shadows);");
	std::string::size_type const started = loading.find("ViewSwitches_SetAtStart(choice, shadows ? 1 : 0);");
	Check(decided != std::string::npos && started != std::string::npos && decided < started, "the start is decided before the game is told how to draw");

	// The log, and the file
	Check(Has(loading, "LogString(\"settings: a new settings file, so the lighting starts per pixel and the shadows start on\");"), "the log says when a new file starts them on");
	Check(Has(loading, "this once. From now on they stay as the player chooses"), "and when an old one has them switched on");
	Check(Has(loading, "if (start != kViewSettingsAsSaved || !fileHadRendering)\n\t{\n\t\tConfig_SetString(kHeading, kRendering, Lighting_Word(choice));\n\t\tConfig_SetBool(kHeading, kShadows, shadows);"),
		"the file is told what the game starts with");
	Check(Has(loading, "if (version < kViewSettingsVersion)\n\t{\n\t\tConfig_SetString(kHeading, kVersionSetting, std::to_string(kViewSettingsVersion));\n\t\tConfig_Save();"),
		"and that it's had this, saved at once, so it can't happen twice");
}

int main()
{
	TheVersionWord();
	TheThreeCases();
	OnceOnly();
	TheEdges();
	TheGame();

	if (checksFailed)
	{
		printf("FAILED: %d checks\n", checksFailed);
		return 1;
	}

	printf("all view settings start checks passed\n");
	return 0;
}
