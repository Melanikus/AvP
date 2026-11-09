// The test for 3dc/view_switches.h, the decisions behind the LIGHTING and SHADOWS commands. Written 2026-11-05 by Melanikus for the AvP port.
//
// tests/build.ps1 compiles this, and the program gives back a non-zero result if any check failed. It uses no graphics card, and runs none of the game: the
// decisions are all in the header. The last rows read the game's source files, to check they use those decisions the way the header says they do.

// Modified 2026-11-08 by Melanikus for the AvP port:
//   its Between is the shared one in source_slice.h, so a marker that isn't there is a failed check of its own (SOURCE-SLICE-1, #128)
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include "shadow_tuning.h"

#include "view_switches.h"
#include "light_choice.h"
#include "source_slice.h"

// The switches themselves live in d3d_render.cpp. The header only has to know their names, and nothing here uses them.

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// What these words ask of SHADOWS. kNotUnderstood means refused, with what was noted left alone.
static int const kNotUnderstood = -100;
static int const kWasNotedBefore = -50;

static int ShadowsAskFor(char const *words)
{
	int asked = kWasNotedBefore;

	if (!SwitchWords_ShadowsAsk(words, &asked))
	{
		return (asked == kWasNotedBefore) ? kNotUnderstood : -999;
	}

	return asked;
}

static int LightingAskFor(char const *words)
{
	int asked = kWasNotedBefore;

	if (!SwitchWords_LightingAsk(words, &asked))
	{
		return (asked == kWasNotedBefore) ? kNotUnderstood : -999;
	}

	return asked;
}

static void WordsForShadows()
{
	Check(ShadowsAskFor("") == kAskedNext, "SHADOWS alone switches it");
	Check(ShadowsAskFor("   ") == kAskedNext, "and so does SHADOWS with only spaces after it");
	Check(ShadowsAskFor("0") == 0, "SHADOWS 0 is off");
	Check(ShadowsAskFor("1") == 1, "SHADOWS 1 is on");
	Check(ShadowsAskFor(" 0 ") == 0, "spaces around the 0 don't count");
	Check(ShadowsAskFor("1\r") == 1, "nor does what's left at the end of a line from a file");
	Check(ShadowsAskFor("\t1") == 1, "nor a tab");

	char const *const refused[] = { "2", "-1", "01", "00", "10", "1 1", "0 1", "1x", "x1", "on", "off", "ABC", "1.0", "0x1", "\xFF", "1\xFF" };

	for (char const *words : refused)
	{
		Check(ShadowsAskFor(words) == kNotUnderstood, std::string("SHADOWS refuses '") + words + "', and leaves what was noted alone");
	}

	Check(ShadowsAskFor(0) == kNotUnderstood, "SHADOWS refuses no words at all");
	Check(ShadowsAskFor(std::string(300, '1').c_str()) == kNotUnderstood, "and a batch file's longest line");
}

static int AmbientAskFor(char const *words)
{
	int asked = kWasNotedBefore;

	if (!SwitchWords_ShadowAmbientAsk(words, &asked))
	{
		return (asked == kWasNotedBefore) ? kNotUnderstood : -999;
	}

	return asked;
}

static void WordsForAmbient()
{
	Check(AmbientAskFor("") == kAskedNext, "SHADOWAMBIENT alone switches it, so one key can flip it");
	Check(AmbientAskFor("   ") == kAskedNext, "and so does SHADOWAMBIENT with only spaces after it");
	Check(AmbientAskFor("0") == 0, "SHADOWAMBIENT 0 is off");
	Check(AmbientAskFor("1") == 1, "SHADOWAMBIENT 1 is on");
	Check(AmbientAskFor("OFF") == 0, "SHADOWAMBIENT OFF is off");
	Check(AmbientAskFor("ON") == 1, "SHADOWAMBIENT ON is on");
	Check(AmbientAskFor("off") == 0, "in small letters too");
	Check(AmbientAskFor("On\r") == 1, "and in mixed letters, at the end of a line from a file");
	Check(AmbientAskFor("  ON  ") == 1, "and with spaces around it");

	char const *const refused[] = { "2", "-1", "01", "00", "10", "1 1", "1x", "x1", "O", "OF", "OFFF", "ONN", "ON OFF", "OFF ON", "ABC", "1.0", "0x1", "\xFF", "ON\xFF" };

	for (char const *words : refused)
	{
		Check(AmbientAskFor(words) == kNotUnderstood, std::string("SHADOWAMBIENT refuses '") + words + "', and leaves what was noted alone");
	}

	Check(AmbientAskFor(0) == kNotUnderstood, "SHADOWAMBIENT refuses no words at all");
	Check(AmbientAskFor(std::string(300, '1').c_str()) == kNotUnderstood, "and a batch file's longest line");
}

static void TheMapLightsRule()
{
	Check(!LightChoice_MayCast(kLightFromTheLevel, false), "with SHADOWAMBIENT off, a level light takes no place");
	Check(!LightChoice_MayCast(kLampWithNoCorona, false), "and nor does a lamp with no corona");
	Check(LightChoice_MayCast(kLampWithACorona, false), "a lamp with a corona does");
	Check(LightChoice_MayCast(kLightOfAnyOtherKind, false), "and so does everything that isn't the map's own, corona or not");
	Check(LightChoice_MayCast(kLightFromTheLevel, true), "with SHADOWAMBIENT on, a level light competes again");
	Check(LightChoice_MayCast(kLampWithNoCorona, true), "and so does a lamp with no corona");
	Check(LightChoice_MayCast(kLampWithACorona, true), "and a lamp with a corona stays in");
	Check(LightChoice_MayCast(kLightOfAnyOtherKind, true), "and so does everything else");
}

static void WordsForLighting()
{
	Check(LightingAskFor("") == kAskedNext, "LIGHTING alone goes to the next one");
	Check(LightingAskFor("  ") == kAskedNext, "and so does LIGHTING with only spaces after it");
	Check(LightingAskFor("1") == kLightingGouraud, "1 is Gouraud");
	Check(LightingAskFor("2") == kLightingPerPixel, "2 is per pixel");
	Check(LightingAskFor("3") == kLightingNormals, "3 is normals");
	Check(LightingAskFor("4") == kLightingRelief, "4 is relief");
	Check(LightingAskFor("GOURAUD") == kLightingGouraud, "the name GOURAUD");
	Check(LightingAskFor("PERPIXEL") == kLightingPerPixel, "the name PERPIXEL");
	Check(LightingAskFor("NORMALS") == kLightingNormals, "the name NORMALS");
	Check(LightingAskFor("RELIEF") == kLightingRelief, "the name RELIEF");
	Check(LightingAskFor("gouraud") == kLightingGouraud, "a name in small letters");
	Check(LightingAskFor("PerPixel\r") == kLightingPerPixel, "a name in mixed letters, at the end of a line from a file");
	Check(LightingAskFor("  relief  ") == kLightingRelief, "a name with spaces around it");

	char const *const refused[] = { "0", "5", "-1", "11", "1 2", "RELIE", "RELIEFS", "RELIEF 1", "PER PIXEL", "NORMAL", "NORMALS2", "GOURAUD2", "ON", "OFF", "\xFF", "RELIEF\xFF" };

	for (char const *words : refused)
	{
		Check(LightingAskFor(words) == kNotUnderstood, std::string("LIGHTING refuses '") + words + "', and leaves what was noted alone");
	}

	Check(LightingAskFor(0) == kNotUnderstood, "LIGHTING refuses no words at all");
	Check(LightingAskFor(std::string(300, 'R').c_str()) == kNotUnderstood, "and a batch file's longest line");
}

static void ShadowsAnswers()
{
	Check(Shadows_AfterAsking(1, kAskedNext) == 0, "on, switched, is off");
	Check(Shadows_AfterAsking(0, kAskedNext) == 1, "off, switched, is on");
	Check(Shadows_AfterAsking(1, 1) == 1 && Shadows_AfterAsking(0, 1) == 1, "asked for on is on, from either");
	Check(Shadows_AfterAsking(1, 0) == 0 && Shadows_AfterAsking(0, 0) == 0, "asked for off is off, from either");
	Check(Shadows_AfterAsking(1, kAskedNothing) == 1 && Shadows_AfterAsking(0, kAskedNothing) == 0, "nothing asked changes nothing");

	// Two bindings of one key note the same thing twice, in one frame, so there's only one change to make.
	int asked = kAskedNothing;
	SwitchWords_ShadowsAsk("", &asked);
	SwitchWords_ShadowsAsk("", &asked);
	Check(Shadows_AfterAsking(1, asked) == 0, "a key bound twice still switches it, instead of switching it there and back");

	asked = kAskedNothing;
	SwitchWords_ShadowsAsk("0", &asked);
	SwitchWords_ShadowsAsk("1", &asked);
	Check(Shadows_AfterAsking(0, asked) == 1, "of two different things asked in one frame, the last one counts");

	asked = 1;
	SwitchWords_ShadowsAsk("bad", &asked);
	Check(Shadows_AfterAsking(0, asked) == 1, "refused words don't cancel something already asked");
}

static void TheCycle()
{
	// everything there, in the order the player sees them
	Check(Lighting_AfterAsking(kLightingGouraud, kAskedNext, true, true) == kLightingPerPixel, "Gouraud goes to per pixel");
	Check(Lighting_AfterAsking(kLightingPerPixel, kAskedNext, true, true) == kLightingNormals, "per pixel goes to normals");
	Check(Lighting_AfterAsking(kLightingNormals, kAskedNext, true, true) == kLightingRelief, "normals goes to relief");
	Check(Lighting_AfterAsking(kLightingRelief, kAskedNext, true, true) == kLightingGouraud, "relief goes back to Gouraud");

	for (int choice = 0; choice < kLightingChoices; choice++)
	{
		Check(Lighting_AfterAsking(kLightingRelief, choice, true, true) == choice, std::string("from relief, you can jump to ") + Lighting_Word(choice));
		Check(Lighting_AfterAsking(kLightingGouraud, choice, true, true) == choice, std::string("from Gouraud, you can jump to ") + Lighting_Word(choice));
		Check(Lighting_AfterAsking(choice, kAskedNothing, true, true) == choice, std::string("nothing asked leaves ") + Lighting_Word(choice) + " alone");
		Check(Lighting_Drawn(choice, true, true) == choice, std::string(Lighting_Word(choice)) + " is drawn when it's there");
	}

	Check(!Lighting_UsesMaps(kLightingGouraud) && !Lighting_UsesMaps(kLightingPerPixel), "Gouraud and per pixel don't read the maps");
	Check(Lighting_UsesMaps(kLightingNormals) && Lighting_UsesMaps(kLightingRelief), "normals and relief do");

	// no maps: -reliefoff, or a shader that couldn't be made
	Check(Lighting_AfterAsking(kLightingPerPixel, kAskedNext, false, true) == kLightingGouraud, "with no maps, per pixel goes past both that need them, to Gouraud");
	Check(Lighting_AfterAsking(kLightingGouraud, kAskedNext, false, true) == kLightingPerPixel, "and Gouraud goes to per pixel");
	Check(Lighting_AfterAsking(kLightingNormals, kAskedNext, false, true) == kLightingGouraud, "still asking for normals, a press moves on from what's really on the screen, which is per pixel");
	Check(Lighting_AfterAsking(kLightingRelief, kAskedNext, false, true) == kLightingGouraud, "and the same from relief");
	Check(Lighting_AfterAsking(kLightingGouraud, kLightingNormals, false, true) == kLightingGouraud, "asking for normals by name when it isn't there changes nothing");
	Check(Lighting_AfterAsking(kLightingGouraud, kLightingRelief, false, true) == kLightingGouraud, "and nor does relief");
	Check(Lighting_AfterAsking(kLightingGouraud, kLightingPerPixel, false, true) == kLightingPerPixel, "per pixel can still be asked for");
	Check(Lighting_Drawn(kLightingNormals, false, true) == kLightingPerPixel, "normals asked for and not there draws per pixel");
	Check(Lighting_Drawn(kLightingRelief, false, true) == kLightingPerPixel, "and so does relief");

	// the shader that marches couldn't be made: only relief needs it
	Check(!Lighting_IsThere(kLightingRelief, true, true, false), "with no march, relief isn't there");
	Check(Lighting_IsThere(kLightingNormals, true, true, false) && Lighting_IsThere(kLightingPerPixel, true, true, false), "and everything else still is");
	Check(Lighting_Drawn(kLightingRelief, true, true, false) == kLightingNormals, "a saved relief with no march draws normals, as near as it can get");
	Check(Lighting_Drawn(kLightingRelief, false, true, false) == kLightingPerPixel, "and with no maps either, per pixel");
	Check(Lighting_AfterAsking(kLightingNormals, kAskedNext, true, true, false) == kLightingGouraud, "the cycle goes past it");
	Check(Lighting_AfterAsking(kLightingGouraud, kLightingRelief, true, true, false) == kLightingGouraud, "asking for it by name changes nothing");
	Check(Lighting_MenuStep(kLightingNormals, kLightingRelief, true, true, false) == kLightingGouraud, "the menu's line goes past it, forward");
	Check(Lighting_MenuStep(kLightingGouraud, kLightingRelief, true, true, false) == kLightingNormals, "and back");

	// the card can't draw per pixel at all, or not for this moment
	for (int choice = 0; choice < kLightingChoices; choice++)
	{
		Check(Lighting_Drawn(choice, true, false) == kLightingGouraud, std::string(Lighting_Word(choice)) + " asked for draws Gouraud on a card that can't draw per pixel");
		Check(Lighting_AfterAsking(choice, kAskedNext, true, false) == choice, std::string("a press on that card leaves ") + Lighting_Word(choice) + " asked for, for when it comes back");
		Check(Lighting_AfterAsking(choice, kAskedNext, false, false) == choice, std::string("with no maps either, it still leaves ") + Lighting_Word(choice));
	}

	Check(Lighting_AfterAsking(kLightingRelief, kLightingPerPixel, true, false) == kLightingRelief, "per pixel by name on that card changes nothing");
	Check(Lighting_AfterAsking(kLightingGouraud, kLightingNormals, true, false) == kLightingGouraud, "and nor does normals by name");
	Check(Lighting_AfterAsking(kLightingGouraud, kLightingRelief, true, false) == kLightingGouraud, "or relief");
	Check(Lighting_AfterAsking(kLightingRelief, kLightingGouraud, true, false) == kLightingGouraud, "but Gouraud can always be asked for");

	// a press taken while the card has gone for a moment, then it's back
	int const during = Lighting_AfterAsking(kLightingRelief, kAskedNext, true, false);
	Check(Lighting_Drawn(during, true, true) == kLightingRelief, "after a device reset, the lighting chosen before it comes back");

	// a key bound twice
	int asked = kAskedNothing;
	SwitchWords_LightingAsk("", &asked);
	SwitchWords_LightingAsk("", &asked);
	Check(Lighting_AfterAsking(kLightingPerPixel, asked, true, true) == kLightingNormals, "a key bound twice moves on one, not two");

	Check(std::string(Lighting_Word(kLightingRelief)) == "RELIEF" && std::string(Lighting_Word(kLightingNormals)) == "NORMALS"
		&& std::string(Lighting_Word(kLightingPerPixel)) == "PERPIXEL" && std::string(Lighting_Word(kLightingGouraud)) == "GOURAUD",
		"each is called what the command calls it");

	// Relief mustn't claim what it doesn't do. These lengths are the room the game leaves: 39 characters of a message, and 47 of a line of the tag.
	Check(std::string(Lighting_WordsOnScreen(kLightingRelief, false)).find("AS NORMALS") != std::string::npos, "relief that isn't marching says it's drawn as normals");
	Check(std::string(Lighting_WordsOnScreen(kLightingRelief, true)) == "RELIEF", "and when it's marching, it's just relief");

	for (int choice = 0; choice < kLightingChoices; choice++)
	{
		size_t const words = strlen(Lighting_WordsOnScreen(choice, false));

		Check(strlen("LIGHTING: ") + words <= 39, std::string(Lighting_Word(choice)) + " fits in its message");
		Check(strlen("LIGHTING , SHADOWS OFF") + words <= 47, "and in its line of the tag");
	}
}

static int SavedWordGives(char const *words)
{
	int choice = kWasNotedBefore;

	return Lighting_FromSavedWord(words, &choice) ? choice : ((choice == kWasNotedBefore) ? kNotUnderstood : -999);
}

// What a settings file can say, which anyone can edit
static void TheSavedWord()
{
	for (int choice = 0; choice < kLightingChoices; choice++)
	{
		Check(SavedWordGives(Lighting_Word(choice)) == choice, std::string("a saved ") + Lighting_Word(choice) + " comes back as itself");
	}

	Check(SavedWordGives("gouraud") == kLightingGouraud, "in small letters");
	Check(SavedWordGives(" NORMALS\r") == kLightingNormals, "with spaces, and what's left at the end of a line");

	char const *const refused[] = { "", "   ", "0", "1", "2", "3", "4", "-1", "RELIE", "RELIEFS", "RELIEF 1", "PER PIXEL", "NORMAL", "true", "\xFF", "RELIEF\xFF" };

	for (char const *words : refused)
	{
		Check(SavedWordGives(words) == kNotUnderstood, std::string("a saved '") + words + "' is refused, and leaves the choice alone");
	}

	Check(SavedWordGives(0) == kNotUnderstood, "and so is nothing at all");
	Check(SavedWordGives(std::string(100000, 'R').c_str()) == kNotUnderstood, "and a very long line");
}

// The menu's line goes round one step at a time
static void TheMenuStep()
{
	for (int choice = 0; choice < kLightingChoices; choice++)
	{
		int const next = (choice + 1) % kLightingChoices;
		int const before = (choice + kLightingChoices - 1) % kLightingChoices;

		Check(Lighting_MenuStep(choice, next, true, true) == next, std::string("with everything there, the line goes forward from ") + Lighting_Word(choice));
		Check(Lighting_MenuStep(choice, before, true, true) == before, std::string("and back from ") + Lighting_Word(choice));
		Check(Lighting_MenuStep(choice, next, true, false) == kLightingGouraud, std::string("on a card that can't draw per pixel, forward from ") + Lighting_Word(choice) + " is Gouraud");
		Check(Lighting_MenuStep(choice, before, true, false) == kLightingGouraud, std::string("and so is back from ") + Lighting_Word(choice));
	}

	Check(Lighting_MenuStep(kLightingPerPixel, kLightingNormals, false, true) == kLightingGouraud, "with no maps, forward from per pixel goes past both that need them, to Gouraud");
	Check(Lighting_MenuStep(kLightingGouraud, kLightingRelief, false, true) == kLightingPerPixel, "and back from Gouraud goes past both, to per pixel");
	Check(Lighting_MenuStep(kLightingGouraud, kLightingPerPixel, false, true) == kLightingPerPixel, "and per pixel is still there");
	Check(Lighting_MenuStep(kLightingPerPixel, kLightingGouraud, false, true) == kLightingGouraud, "and so is Gouraud");
	Check(Lighting_MenuStep(kLightingGouraud, 99, true, true) >= 0 && Lighting_MenuStep(kLightingGouraud, 99, true, true) < kLightingChoices, "a number no menu could send still gives a choice");
	Check(Lighting_MenuStep(kLightingGouraud, -7, true, true) >= 0 && Lighting_MenuStep(kLightingGouraud, -7, true, true) < kLightingChoices, "and so does one below zero");
}

// The same numbers as the game's views, enough for these rows
enum { kOff, kFinal, kEdges, kPolygons, kViewCount };

static void TheView()
{
	int comeBack = kFinal;

	Check(View_AfterSwitches(kFinal, kOff, false, true, &comeBack) == kOff, "everything going off goes to OFF");
	Check(comeBack == kFinal, "and remembers FINAL");
	Check(View_AfterSwitches(kOff, kOff, true, false, &comeBack) == kFinal, "either coming back returns to FINAL");

	comeBack = kFinal;
	Check(View_AfterSwitches(kEdges, kOff, false, true, &comeBack) == kOff && comeBack == kEdges, "going off from another view remembers that view");
	Check(View_AfterSwitches(kOff, kOff, true, false, &comeBack) == kEdges, "and returns to it");

	comeBack = kEdges;
	Check(View_AfterSwitches(kOff, kOff, false, true, &comeBack) == kOff && comeBack == kEdges, "already at OFF, it doesn't remember OFF as the place to come back to");

	comeBack = kFinal;
	Check(View_AfterSwitches(kPolygons, kOff, false, false, &comeBack) == kPolygons && comeBack == kFinal, "not everything off, nothing moves");
	Check(View_AfterSwitches(kOff, kOff, false, false, &comeBack) == kOff, "and J's own OFF stays, while the switches aren't all off");
	Check(View_AfterSwitches(kOff, kOff, true, true, &comeBack) == kOff && comeBack == kFinal, "still all off, nothing moves");
	Check(View_AfterSwitches(kEdges, kOff, true, true, &comeBack) == kEdges && comeBack == kFinal, "and J's view stays too, while they are");
	Check(View_AfterSwitches(kEdges, kOff, true, false, &comeBack) == kEdges, "J moved it off OFF: turning a switch on leaves it where J put it");

	// no stencil: the switches stay all off, so nothing ever brings the view back
	comeBack = kFinal;
	int view = kOff;
	bool wereAllOff = false;

	for (int frame = 0; frame < 3; frame++)
	{
		view = View_AfterSwitches(view, kOff, wereAllOff, true, &comeBack);
		wereAllOff = true;
	}

	Check(view == kOff && comeBack == kFinal, "a card with no stencil stays at OFF, frame after frame");

	// the moves of J
	Check(View_AfterViewKey(kOff, kViewCount, kFinal, false) == kFinal, "J goes from OFF to FINAL");
	Check(View_AfterViewKey(kFinal, kViewCount, kFinal, false) == kEdges, "and on to the next");
	Check(View_AfterViewKey(kPolygons, kViewCount, kFinal, false) == kOff, "and round from the last to OFF");
	Check(View_AfterViewKey(kOff, kViewCount, kFinal, true) == kEdges, "with everything off, J goes past FINAL");
	Check(View_AfterViewKey(kEdges, kViewCount, kFinal, true) == kPolygons, "the other views are as they were");
	Check(View_AfterViewKey(kPolygons, kViewCount, kFinal, true) == kOff, "and it still comes round to OFF");
}

// Every one of the eight, on a card that has everything
static void TheEightStates()
{
	int seen = 0;

	for (int shadows = 0; shadows <= 1; shadows++)
	{
		for (int choice = 0; choice < kLightingChoices; choice++)
		{
			int const lighting = Lighting_AfterAsking(kLightingNormals, choice, true, true);
			int const volumes = Shadows_AfterAsking(1, shadows);

			if (lighting == choice && volumes == shadows)
			{
				seen++;
			}
		}
	}

	Check(seen == 8, "all eight can be reached");

	// SHADOWS changing doesn't touch LIGHTING, and the other way round: they're separate numbers, with nothing shared.
	Check(Lighting_AfterAsking(kLightingPerPixel, kAskedNothing, true, true) == kLightingPerPixel && Shadows_AfterAsking(0, kAskedNothing) == 0, "what isn't asked about stays");
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

// The text from one marker up to the next (source_slice.h). A marker that isn't there is a failed check of its own, named by the marker.
static std::string Between(std::string const &text, char const *from, char const *to)
{
	return SourceSlice(text, from, to, kSliceEndFromOneOn, [](const std::string &why) { Check(false, why.c_str()); });
}

static bool Has(std::string const &text, char const *words)
{
	return text.find(words) != std::string::npos;
}

// The number an enum's value stands for, read from the start of value: written there as a number, or as the name of another enum's
// number, which is followed once to "enum { <name> = <number> };", written exactly once in source. 0, which no room check accepts, for
// anything else: a name written nowhere or twice, or a sum. Added 2026-11-09 by Melanikus for the AvP port.
static int EnumNumber(std::string const &source, std::string const &value)
{
	size_t length = 0;

	while (length < value.size() && (isalnum(static_cast<unsigned char>(value[length])) || value[length] == '_'))
	{
		length++;
	}

	// What comes after the word has to end it: a comma, or the enum's closing brace, so "name + 1" is a sum, and refused
	bool const ends = length < value.size() &&
		(value[length] == ',' || value[length] == '}' || value.compare(length, 2, " }") == 0);

	if (length == 0 || !ends)
	{
		return 0;
	}

	std::string const word = value.substr(0, length);

	if (isdigit(static_cast<unsigned char>(word[0])))
	{
		return word.find_first_not_of("0123456789") == std::string::npos ? atoi(word.c_str()) : 0;
	}

	std::string const definition = "enum { " + word + " = ";
	size_t const at = source.find(definition);

	if (at == std::string::npos || source.find(definition, at + 1) != std::string::npos)
	{
		return 0;
	}

	size_t const digits = at + definition.size();
	size_t const end = source.find_first_not_of("0123456789", digits);

	if (end == digits || end == std::string::npos || source.compare(end, 3, " };") != 0)
	{
		return 0;
	}

	return atoi(source.substr(digits, end - digits).c_str());
}

// Whether a key is named, as itself: KEY_L isn't there just because KEY_LEFT is.
static bool NamesKey(std::string const &text, std::string const &key)
{
	std::string::size_type place = 0;

	while ((place = text.find(key, place)) != std::string::npos)
	{
		place += key.size();

		char const after = (place < text.size()) ? text[place] : ' ';

		if (!((after >= 'A' && after <= 'Z') || (after >= '0' && after <= '9') || after == '_'))
		{
			return true;
		}
	}

	return false;
}

static int CountOf(std::string const &text, std::string const &words)
{
	int count = 0;
	std::string::size_type place = 0;

	while ((place = text.find(words, place)) != std::string::npos)
	{
		count++;
		place += words.size();
	}

	return count;
}

static void TheRealGame()
{
	std::string const controls = ReadSourceFile("3dc/avp/win95/user_io.cpp");

	char const *const tables[] = { "DefaultMarineInputPrimaryConfig", "DefaultPredatorInputPrimaryConfig", "DefaultAlienInputPrimaryConfig", "DefaultMarineInputSecondaryConfig", "DefaultPredatorInputSecondaryConfig", "DefaultAlienInputSecondaryConfig" };

	for (char const *table : tables)
	{
		Check(Has(controls, table), std::string("the default controls are in this file: ") + table);
	}

	Check(NamesKey(controls, "KEY_Q"), "this search does find a key the default controls use");
	Check(!NamesKey(controls, "KEY_L"), "no default control uses L, for any species");
	Check(!NamesKey(controls, "KEY_H"), "no default control uses H, for any species");

	std::string const render = ReadSourceFile("3dc/d3d_render.cpp");
	std::string const applying = Between(render, "static void ViewSwitches_ApplyAsked()\n{", "static void ToyShadows_Draw()");
	std::string const keys = Between(applying, "if (PlayerKeysCanAct())", "bool const hasStencil");

	Check(CountOf(render, "ViewSwitches_ApplyAsked();") == 2, "the switches change from one place, named once ahead and called once");
	Check(Has(render, "ToyShadows_Draw();\n\t\tViewSwitches_ApplyAsked();"), "and that's straight after the frame's shadows");

	std::string const canAct = Between(render, "static bool DevKeysCanAct()", "}");
	std::string const playerCanAct = Between(render, "static bool PlayerKeysCanAct()", "}");

	Check(Has(canAct, "DevMode"), "the keys need -dev");
	Check(Has(canAct, "AvP.Network == I_No_Network"), "and do nothing in a network game, where they'd show what walls hide");

	std::string const keepingOut = Between(render, "static void KeepDeveloperToolsOutOfNetworkGames()", "static void ToyShadows_Draw()");

	// A player shouldn't be able to reach a route nobody has proved, or change how a network game looks to them.
	std::string const switches = ReadSourceFile("3dc/avp/win95/winmain.cpp");
	char const *const devToggles[] = { "-shadowevengap", "-staticsurface", "-staticscene", "-staticnolivelights", "-staticcompare", "-staticcomparedrop", "-keptprismson", "-shadowdepthgap", "-keptsnapfar", "-keptsnap", "-shadowtest", "-shadowflicker", "-shadowshoot", "-shadowreport", "-shadownofloatblend" };
	char const *const devNumbers[] = { "-shadowbiassteps=", "-shadowdark=", "-shadowsoft=", "-shadowpositions=", "-shadownearlights=", "-shadowlampsize=", "-shadowflaresize=", "-shadowmodelgap=", "-shadowflarereach=", "-shadowflare=", "-shadowworld=", "-shadowlights=", "-shadowbrief=", "-shadowcasters=" };

	// A switch read any of these ways, and not through devSwitch or devNumber, would get round -dev
	auto const readWithoutDev = [&](char const *name)
	{
		return Has(switches, (std::string("strstr(command_line, \"") + name + "\")").c_str())
			|| Has(switches, (std::string("CommandLineHasWord(command_line, \"") + name + "\")").c_str())
			|| Has(switches, (std::string("CommandLineFindSwitch(command_line, \"") + name + "\")").c_str());
	};

	for (char const *const name : devToggles)
	{
		Check(Has(switches, (std::string("devSwitch(\"") + name + "\")").c_str()), std::string(name) + " needs -dev");
		Check(!readWithoutDev(name), std::string(name) + " has no way round that");
	}

	for (char const *const name : devNumbers)
	{
		Check(Has(switches, (std::string("devNumber(\"") + name + "\")").c_str()), std::string(name) + " needs -dev");
		Check(!readWithoutDev(name), std::string(name) + " has no way round that");
	}

	Check(Has(switches, "LogString(std::string(\"shadows: \") + name + \" needs -dev, so it's off\");"), "asking without -dev says why it's off");
	Check(Has(Between(switches, "auto const devNumber", "};"), "devSwitch(name)"), "and one with a number goes through the same door");
	Check(Has(switches, "CommandLineHasWord(command_line, \"-shadowsoff\")") && Has(switches, "CommandLineHasWord(command_line, \"-reliefoff\")"), "turning the new look off is still for anyone");

	char const *const tuned[] = { "ShadowsEvenGap", "StaticSurfaceOn", "StaticSceneOn", "StaticSceneNoLiveLights", "StaticSurfaceCompare", "KeptPrismsOff", "ShadowsDepthGap", "ShadowBiasSteps", "KeptSnap", "ToyShadowsDarkness", "ToyShadowsSoft", "ToyShadowsPositions", "ToyShadowsNearLights", "ToyShadowsLampSize", "ToyShadowsFlareSize", "ToyShadowsModelGap", "ToyShadowsFlareReach", "ToyShadowsFlareCap", "ToyShadowsWorld", "ToyShadowsTest", "ToyShadowsMaxLights", "ToyShadowsMaxBrief", "ShadowsPretendNoFloatBlend", "ToyShadowsCasters" };

	for (char const *const name : tuned)
	{
		Check(Has(render, (std::string("{ &") + name + ", " + name + " }").c_str()), std::string("a network game puts ") + name + " back");
	}

	Check(Has(keepingOut, "ShadowTuning_PutBack(shadowTuning,"), "and it does that for the whole list");
	Check(Has(keepingOut, "ShadowSurfaceBias = shadowSurfaceBiasAsBuilt;"), "and for the one that isn't a whole number");
	Check(Has(keepingOut, "and what the command line changed about how shadows and lights are drawn is back as it was built"), "and the log says that's why");
	Check(render.find("shadowSurfaceBiasAsBuilt = ShadowSurfaceBias;") != std::string::npos, "what it goes back to is what the game was built with");

	Check(Has(keepingOut, "if (AvP.Network == I_No_Network)\n\t{\n\t\treturn;"), "a game that isn't a network game keeps its tools");
	Check(Has(keepingOut, "ToyShadowsMode != kToyViewOff && ToyShadowsMode != kToyViewFinal"), "in one that is, only OFF and FINAL are allowed");
	Check(Has(keepingOut, "ToyShadowsMode = kToyViewFinal;"), "anything else goes back to FINAL");
	Check(Has(keepingOut, "ShadowShootOn = 0;") && Has(keepingOut, "ShadowFlickerOn = 0;") && Has(keepingOut, "ShadowsFullReport = 0;"), "and the tools that say the same in words go off");
	Check(Has(keepingOut, "LogString("), "and the log says why");

	std::string::size_type const viewKey = render.find("if (DevKeysCanAct() && ViewSettings_KeyPressed(kViewKeyDevViews))");
	std::string::size_type const keptOut = render.find("KeepDeveloperToolsOutOfNetworkGames();");

	Check(viewKey != std::string::npos && keptOut != std::string::npos && viewKey < keptOut && keptOut < render.find("bool const lightsOurs"), "that's asked every frame, after G and before anything is drawn");
	Check(Has(canAct, "PlayerKeysCanAct()"), "and everything a key that anyone may press needs");
	Check(!Has(playerCanAct, "DevMode") && !Has(playerCanAct, "I_No_Network"), "which is neither of those two");
	Check(Has(playerCanAct, "AvP.MainLoopRunning"), "but a level being played, so a letter typed into a main menu doesn't press them");
	Check(Has(playerCanAct, "!InGameMenusAreRunning()"), "and no menu open in that level");
	Check(Has(playerCanAct, "IOFOCUS_AcceptControls() && !(IOFOCUS_Get() & (IOFOCUS_NEWCONSOLE | IOFOCUS_OSK))"), "and do nothing while typing");
	Check(Has(keys, "if (PlayerKeysCanAct())"), "the rendering and shadows keys ask that, so they work for everyone");
	Check(Has(render, "if (DevKeysCanAct() && ViewSettings_KeyPressed(kViewKeyDevViews))"), "and the dev views' key asks for -dev as well");
	Check(Has(render, "Not while typing, like J.\n\tif (DevKeysCanAct())"), "and so do F7, F8 and the shoot tool's keys");
	Check(CountOf(render, "DebouncedKeyboardInput[KEY_F7]") == 1 && CountOf(render, "DebouncedKeyboardInput[KEY_J]") == 0 && CountOf(render, "DebouncedKeyboardInput[KEY_G]") == 0, "and nothing else in the drawing code reads those keys, and G is the marine's now");
	Check(Has(keys, "ViewSettings_KeyPressed(kViewKeyRendering)") && Has(keys, "lightingAsked = kAskedNext"), "the rendering key is the next lighting, once a press");
	Check(Has(keys, "ViewSettings_KeyPressed(kViewKeyShadows)") && Has(keys, "shadowsAsked = kAskedNext"), "the shadows key switches the shadows, once a press");
	Check(CountOf(render, "KEY_L]") == 0 && CountOf(render, "KEY_H]") == 0, "and the drawing code reads no fixed letter for them, because the player chooses: tests/view_keys_test.cpp");

	std::string const shadowsCommand = Between(render, "void ShadowsCommand(char *words)", "void LightingCommand(char *words)");
	std::string const lightingCommand = Between(render, "void LightingCommand(char *words)", "// The menu asks the same way the commands do");

	Check(Has(shadowsCommand, "SwitchWords_ShadowsAsk(words, &shadowsAsked)") && !Has(shadowsCommand, "ShadowVolumesOn"), "SHADOWS only notes what it's asked for");
	Check(Has(lightingCommand, "SwitchWords_LightingAsk(words, &lightingAsked)") && !Has(lightingCommand, "LightingNow ="), "LIGHTING only notes what it's asked for");

	Check(Has(render, "int LightingNow = kLightingGouraud;"), "with nothing saved, the game starts in Gouraud");
	Check(Has(render, "int ShadowVolumesOn = 0;"), "with shadows off");
	Check(Has(render, "SurfacePictures_Draw(device, toyDepthSurface, worldAxes, Lighting_UsesMaps(LightingNow), useStatic, MainDrawUsesHeights(), backwards)"), "the surface pass only asks for the maps when the choice reads them");
	Check(Has(render, "ToyShadowsMode = View_AfterViewKey(ToyShadowsMode, kToyViewCount, kToyViewFinal, switchesWereAllOff);"), "J goes past FINAL when everything is off");

	std::string const addsPerPixel = Between(render, "static bool ViewAddsLightsPerPixel()", "}");

	Check(Has(addsPerPixel, "ToyShadowsMode == kToyViewFinal && LightingNow != kLightingGouraud"), "in FINAL, Gouraud keeps the live lights in the game's own lighting");
	Check(Has(addsPerPixel, "ToyShadowsMode == kToyViewMask") && Has(addsPerPixel, "ToyShadowsMode == kViewNormals"), "and the views for looking inside the pass take no notice of it");
	Check(Has(render, "bool const lightsOurs = ViewAddsLightsPerPixel()"), "the pass asks that before it takes any lights");
	Check(Has(applying, "if (!ViewAddsLightsPerPixel())\n\t{\n\t\tToyShadowsLightsByPixel = 0;"), "and a change hands them back in time for the very next frame");

	// These three only show the lines are there. Whether they look right on a card needs a run.
	std::string const shapes = ReadSourceFile("3dc/kshape.cpp");

	Check(Has(render, "if (light[7] != 0.0f && fabsf(centreFacing) < kToyFlareTouching)"), "the floor under a resting flare doesn't block it, whichever way it's lit");
	Check(Has(shapes, "toyChosenLights[placeOf[i] * kToyLightFloats + 7] != 0.0f"), "only lamps are picked to be drawn from several positions");
	Check(Has(shapes, "(chosen[i].order < 0 && light->LightBright > flareCap) ? flareCap : light->LightBright"), "a flare in the game's lighting counts only what its cap lets it add");

	Check(Has(applying, "ToyShadowsMode = View_AfterSwitches(ToyShadowsMode, kToyViewOff, switchesWereAllOff, allOff, &viewToComeBackTo);"), "everything off goes to the OFF view");
	Check(Has(applying, "if (!hasStencil)\n\t{\n\t\tShadowVolumesOn = 0;\n\t\tLightingNow = kLightingGouraud;"), "with no stencil bits, both are held off every frame");

	// The developers' relief switches: what's typed can't reach the shader unchecked, they can't run in a network game, and a player who hasn't asked for maps doesn't read any.
	Check(ReliefDepth_Clamped(0.1f) == 0.1f, "a depth in range stays as it is");
	Check(ReliefDepth_Clamped(0.0f) == 0.0f && ReliefDepth_Clamped(-1.0f) == 0.0f, "no depth and less than none both come to none");
	Check(ReliefDepth_Clamped(1000.0f) == kReliefDepthMost, "too deep comes back to the most");

	{
		float volatile zero = 0.0f;
		float const notANumber = zero / zero;
		Check(ReliefDepth_Clamped(notANumber) == 0.0f, "and something that isn't a number comes to none, not on to the shader");
	}

	std::string const reliefSwitches = Between(render, "static bool ReliefToolsCanAct(char const *name)", "extern int DrawFullBright;");
	Check(Has(reliefSwitches, "if (DevMode && AvP.Network == I_No_Network)"), "the relief switches need -dev, and not in a network game");
	Check(CountOf(reliefSwitches, "if (ReliefToolsCanAct(") == 3, "and all three ask");
	Check(Has(reliefSwitches, "ReliefDepth_Clamped((float)atof(words))"), "and the depth typed goes through the clamp");

	std::string const loader = ReadSourceFile("3dc/avp/win95/projload.cpp");
	Check(Has(loader, "if (DevMode || Lighting_UsesMaps(LightingNow))\n\t{\n\t\ttextureRelief.Build(textureID);"), "a level loaded with a lighting that draws no maps reads no map");
	Check(CountOf(loader, "textureRelief.Build(") == 1, "and that's the only way in");

	// The tag is a fixed list of lines on the stack, and more lines than it has room for would write past its end.
	// Modified 2026-11-09 by Melanikus for the AvP port: the room can be written as a name, since the relay's line in play started under the
	// tag (kMostLines = kShadowStatusMostLines, the number the two share), and is then followed to where it's written as a number.
	std::string const tag = Between(render, "enum { kMostLines = ", "int characters = 0;");
	int const room = EnumNumber(render, tag.empty() ? std::string() : tag.substr(strlen("enum { kMostLines = ")));
	int const lines = CountOf(tag, "lines[numLines++]");
	Check(room > 0, "the tag's room is a number this test can read, or a name it can follow to one");
	Check(lines > 0 && lines <= room, "the tag has room for every line it can write, even if all of them come at once");

	std::string const commands = ReadSourceFile("3dc/avp/game_commands.cpp");
	std::string const ungated = Between(commands, "void CreateGameSpecificConsoleCommands(void)", "bool IsACheat");

	Check(Has(ungated, "\"LIGHTING\"") && Has(ungated, "LightingCommand"), "LIGHTING is made ahead of everything that needs a cheat or -debug");
	Check(Has(ungated, "\"SHADOWS\"") && Has(ungated, "ShadowsCommand"), "and so is SHADOWS");
	Check(!Has(ungated, "DebuggingCommandsActive"), "and nothing there holds them back");

	std::string const launch = ReadSourceFile("3dc/avp/win95/winmain.cpp");
	std::string const reliefOff = Between(launch, "\"-reliefoff\"", "}");
	std::string const shadowsOff = Between(launch, "\"-shadowsoff\"", "}");

	Check(Has(reliefOff, "if (Lighting_UsesMaps(LightingNow))\n\t\t{\n\t\t\tLightingNow = kLightingPerPixel;"), "-reliefoff only moves a choice that reads maps, to per pixel");

	std::string::size_type const settingsRead = launch.find("Config_Load();\n\tViewSettings_Load();");

	Check(settingsRead != std::string::npos && settingsRead < launch.find("\"-reliefoff\"") && settingsRead < launch.find("\"-shadowsoff\""), "the settings are read straight after the file, ahead of the switches that change them for one run");

	// a new player's game: nothing written for what's off
	Check(Has(render, "if (ToyShadowsMode || ShoulderLamp_CouldBeDrawn())\n\t\t{\n\t\t\tSurfaceNormals_Lock();"), "with the view off and the shoulder lamp off, the normals aren't opened, so none are written");
	Check(CountOf(render, "SurfaceNormals_Lock();") == 1, "and that's the only place they're opened");

	std::string const saying = Between(render, "static void SaySwitch(const char *words)", "// A reason is worth hearing once");

	Check(Has(saying, "if (AvP.MainLoopRunning)\n\t{\n\t\tNewOnScreenMessage(message);"), "no message goes to a HUD that isn't there, in the main menus");

	std::string const settings = ReadSourceFile("3dc/view_settings.cpp");
	std::string const loading = Between(settings, "void ViewSettings_Load()", "void ViewSettings_MenuFrame()");

	Check(Has(loading, "Lighting_FromSavedWord(rendering.c_str(), &choice)"), "the saved choice goes through the strict reader");
	Check(Has(loading, "Config_GetString(kHeading, kRendering, Lighting_Word(kLightingGouraud))"), "with nothing saved it's Gouraud");
	Check(Has(loading, "Config_GetBool(kHeading, kShadows, false)"), "shadows off");
	Check(Has(loading, "Config_GetBool(kHeading, kAutoGenerate, false)"), "and no maps worked out");
	Check(Has(loading, "textureRelief.SetAutoGenerate(autoGenerate);"), "which the picture code is told");
	Check(Has(loading, "ViewSwitches_SetAtStart(choice, shadows ? 1 : 0);"), "and the switches and the view are set together");

	std::string const settingAtStart = Between(render, "void ViewSwitches_SetAtStart(int lighting, int volumes)", "}");

	Check(Has(settingAtStart, "switchesWereAllOff = lighting == kLightingGouraud && !volumes;"), "so everything off is known to be off, and coming back from it returns to a view");
	Check(Has(settingAtStart, "ToyShadowsMode = switchesWereAllOff ? kToyViewOff : kToyViewFinal;"), "and everything off is the OFF view from the first frame, and anything else isn't");
	std::string::size_type const menuFrameStarts = settings.find("void ViewSettings_MenuFrame()");
	std::string const menuFrame = (menuFrameStarts == std::string::npos) ? "" : settings.substr(menuFrameStarts);

	Check(Has(menuFrame, "ViewSwitches_AskLighting(choice);") && !Has(menuFrame, "LightingNow ="), "after loading, only the one safe moment changes the lighting: the menu asks");
	Check(Has(menuFrame, "ViewSwitches_AskShadows(MenuShadows);") && !Has(menuFrame, "ShadowVolumesOn ="), "and the same for shadows");
	Check(Has(settings, "Config_Save();"), "the menu saves");

	// the text
	std::string const language = ReadSourceFile("3dc/avp/language.cpp");
	std::string const names = ReadSourceFile("3dc/avp/langenum.h");

	Check(Has(language, "for (int i = 1; i < TEXTSTRING_RENDERING_TITLE; i++)"), "only what's in a stock language file is read from it, so a stock file doesn't count as short");
	Check(Has(language, "TextStringPtr[i] = OurTextStrings[i - TEXTSTRING_RENDERING_TITLE];"), "and the rest comes from the code");
	Check(CountOf(language, "static_assert(sizeof(OurTextStrings)") == 1, "and the compiler checks there's one for each");

	std::string::size_type const gouraudName = names.find("TEXTSTRING_RENDERING_GOURAUD,");
	std::string::size_type const perPixelName = names.find("TEXTSTRING_RENDERING_PERPIXEL,");
	std::string::size_type const normalsName = names.find("TEXTSTRING_RENDERING_NORMALS,");
	std::string::size_type const reliefName = names.find("TEXTSTRING_RENDERING_RELIEF,");

	Check(gouraudName != std::string::npos && gouraudName < perPixelName && perPixelName < normalsName && normalsName < reliefName, "the menu's names are in the order of the choices");
	Check(kLightingGouraud == 0 && kLightingPerPixel == 1 && kLightingNormals == 2 && kLightingRelief == 3, "and those are numbered from 0, as the menu's line is");

	// the menus: each table is read by position, and a new page has to be in both
	std::string const menuData = ReadSourceFile("3dc/avp/win95/frontend/menu_data.cpp");
	std::string const menuNames = ReadSourceFile("3dc/avp/win95/frontend/menus.h");
	std::string const menus = ReadSourceFile("3dc/avp/win95/frontend/menus.cpp");

	Check(menuNames.find("AVPMENU_INGAMEVIDEOMODE,") < menuNames.find("AVPMENU_RENDERING,") && menuNames.find("AVPMENU_RENDERING,") < menuNames.find("AVPMENU_INGAMERENDERING"), "the two new pages come last among the page names");
	Check(menuData.find("// AVPMENU_INGAMEVIDEOMODE") < menuData.find("// AVPMENU_RENDERING") && menuData.find("// AVPMENU_RENDERING") < menuData.find("// AVPMENU_INGAMERENDERING"), "and in the same order in the table");
	Check(Has(menuData, "{AVPMENU_INGAMERENDERING},\t{0},\t{0},\tTEXTSTRING_RENDERING_TITLE_HELP}"), "a level's menus lead to the in-level page");
	Check(Has(menuData, "{AVPMENU_RENDERING},\t{0},\t{0},\tTEXTSTRING_RENDERING_TITLE_HELP}"), "and the main menus to the other");
	Check(Has(menuData, "{3},\t{&MenuRendering},\t\t{TEXTSTRING_RENDERING_GOURAUD}"), "the rendering line has four choices, starting at the first name");
	Check(CountOf(menus, "ViewSettings_MenuFrame();") == 2, "both kinds of menu notice a change");

	std::string const project = ReadSourceFile("win32/VS2026/AvP.vcxproj");

	Check(Has(project, "3dc\\view_settings.cpp"), "and the game builds the file that does it");

	// the page tells the truth
	Check(Has(menuFrame, "lastRendering = MenuRendering = ViewSwitches_DrawnForTheMenu();"), "the page shows what's really being drawn");
	Check(Has(menuFrame, "if (choice != lastRendering)\n\t\t\t{\n\t\t\t\tViewSwitches_AskLighting(choice);"), "nothing is asked for or saved when there's nowhere to move to");
	Check(Has(menuFrame, "if (MenuShadows && !ViewSwitches_ShadowsCanBeOn())\n\t\t\t{\n\t\t\t\tMenuShadows = 0;"), "a refused Shadows goes back");

	std::string::size_type const refused = menuFrame.find("MenuShadows = 0;");
	std::string::size_type const savedShadows = menuFrame.find("Config_SetBool(kHeading, kShadows");

	Check(refused != std::string::npos && savedShadows != std::string::npos && Has(menuFrame.substr(refused, savedShadows - refused), "else"), "and isn't saved");
	Check(Has(menuFrame, "RenderingPage_SayWhatLinesDo(ViewSwitches_ShadowsCanBeOn(), Lighting_UsesMaps(MenuRendering));"), "and a line says when it does nothing");
	Check(Has(menuData, "shadowsCanBeOn ? TEXTSTRING_RENDERING_SHADOWS_HELP : TEXTSTRING_RENDERING_SHADOWS_CANT"), "for shadows");
	Check(Has(menuData, "mapsAreRead ? TEXTSTRING_RENDERING_AUTOGENERATE_HELP : TEXTSTRING_RENDERING_AUTOGENERATE_NOTHING"), "and for maps");

	// Whether lamps cast shadows: the user's choice, a line on this page, on by default
	Check(Has(settings, "bool const lampsReal = Config_GetBool(kHeading, kLampsReal, true);"), "lamps are real lights unless the player has said otherwise");
	Check(Has(settings, "ShoulderLamp_SetRealLightWanted(lampsReal);"), "what's saved is what the game starts with");
	Check(Has(menuFrame, "ShoulderLamp_SetRealLightWanted(lastLampsReal != 0);\n\t\tConfig_SetBool(kHeading, kLampsReal, lastLampsReal != 0);\n\t\tchanged = true;"), "the line takes effect, and it's saved");
	Check(Has(menuFrame, "RenderingPage_SayWhatLampLineDoes(MenuRendering != kLightingGouraud && MenuShadows != 0);"), "and it says when it does nothing");
	Check(Has(menuData, "AvPMenu_Rendering[3].HelpString = lampsCanBeReal ? TEXTSTRING_RENDERING_LAMPSREAL_HELP : TEXTSTRING_RENDERING_LAMPSREAL_NOTHING;"), "on its own line");
	Check(Has(menuData, "{&MenuLampsReal},") && Has(menuData, "static const int kFirstViewKeyElement = 5;"), "which comes before the key lines");

	// Whether other marines' lamps show their beam in the air: the user's choice, the next line, on by default
	Check(Has(settings, "bool const lampBeams = Config_GetBool(kHeading, kLampBeams, true);"), "beams show unless the player has said otherwise");
	Check(Has(settings, "static const char kLampBeams[] = \"LampBeams\";"), "under their own word in the settings file");
	Check(Has(settings, "ShoulderLamp_SetBeamsWanted(lampBeams);"), "what's saved is what the game starts with");
	Check(Has(menuFrame, "ShoulderLamp_SetBeamsWanted(lastLampBeams != 0);\n\t\tConfig_SetBool(kHeading, kLampBeams, lastLampBeams != 0);\n\t\tchanged = true;"), "the line takes effect, and it's saved");
	Check(Has(menuFrame, "RenderingPage_SayWhatBeamLineDoes(MenuRendering != kLightingGouraud);"), "and it says when it does nothing: with Gouraud lights the beam always shows");
	Check(Has(menuData, "AvPMenu_Rendering[4].HelpString = beamsCanBeLeftOut ? TEXTSTRING_RENDERING_LAMPBEAMS_HELP : TEXTSTRING_RENDERING_LAMPBEAMS_NOTHING;"), "on its own line");
	Check(Has(menuData, "{&MenuLampsReal},\t\t{TEXTSTRING_NO},\t\t\t\tTEXTSTRING_RENDERING_LAMPSREAL_HELP},\n\t{AVPMENU_ELEMENT_TEXTSLIDER,\t{TEXTSTRING_RENDERING_LAMPBEAMS},\t\t{1},\t{&MenuLampBeams},"), "right after the lamps' shadows");

	// the user's names, in the language table's order
	char const *const labels[] = { "\"Gouraud\"", "\"Per-pixel (flat)\"", "\"Per-pixel + normals\"", "\"Relief map\"" };
	std::string::size_type last = 0;

	for (char const *label : labels)
	{
		std::string::size_type const at = language.find(label);
		Check(at != std::string::npos && at > last, std::string("the page says ") + label);
		last = at;
	}

	Check(!Has(language, "Classic"), "and Classic has gone from the words");
	Check(Has(language, "Maps are read when a level loads"), "and the page says when maps come");
	Check(Has(shadowsOff, "ToyShadowsMode = 0;") && Has(shadowsOff, "ShadowVolumesOn = 0;") && Has(shadowsOff, "LightingNow = kLightingGouraud;"), "-shadowsoff starts with everything off");

	// A new page is only as good as the line that gets people to it, and the file has to be in the filters too, or Visual Studio won't show it.
	Check(Has(ReadSourceFile("win32/VS2026/AvP.vcxproj.filters"), "3dc\\view_settings.cpp"), "and the project's list of files shows it");
}

// SHADOWS 0: nothing is collected for casting, no prism is built or drawn, and the lights carry on. These rows only show that the lines are there, because the renderer
// doesn't link into a test. What they do on a card needs a run. The world's dim has no row on purpose: whether it stays with the volumes off is still a question for the user.
static void TheVolumesSwitch()
{
	std::string const render = ReadSourceFile("3dc/d3d_render.cpp");
	std::string const shapes = ReadSourceFile("3dc/kshape.cpp");
	std::string const applying = Between(render, "static void ViewSwitches_ApplyAsked()\n{", "static void ToyShadows_Draw()");

	// the commands reach the switches, once each
	Check(Has(applying, "if (hasStencil)\n\t\t{\n\t\t\tShadowVolumesOn = Shadows_AfterAsking(ShadowVolumesOn, shadowsAsked);"), "SHADOWS changes the switch, where there are stencil bits");
	Check(Has(applying, "LightingNow = Lighting_AfterAsking(LightingNow, lightingAsked, mapsThere, perPixelThere, reliefShaderIsThere);"), "LIGHTING changes the choice");
	Check(Has(applying, "\t\tshadowsAsked = kAskedNothing;"), "what SHADOWS asked for is done once, not again every frame");
	Check(Has(applying, "\t\tlightingAsked = kAskedNothing;"), "and the same for LIGHTING");
	Check(Has(render, "\"LIGHTING %s, SHADOWS %s\", Lighting_WordsOnScreen(LightingReallyDrawn(), mainDrawUsedHeights), ShadowVolumesOn ? \"ON\" : \"OFF\""), "the tag shows both, and relief says what it really draws");
	Check(Has(applying, "\"LIGHTING: %s\", Lighting_WordsOnScreen(drawn, reliefShaderIsThere)"), "and so does the message when it changes");

	// The main draw: the old route has to stay as it was, so this all hangs on one question.
	std::string const usesHeights = Between(render, "static bool MainDrawUsesHeights()\n{", "// The solid polygons, by the shader");
	std::string const drawsHeights = Between(render, "static void DrawSolidsWithHeights()\n{", "// Asking makes the pictures if they aren't there yet");
	std::string const buffer = Between(render, "static bool ExecuteBuffer()\n{", "// render any particles");

	Check(Has(usesHeights, "LightingNow == kLightingRelief && ToyShadowsMode == kToyViewFinal && d3d.reliefMainEffect != kNullEffectID && SurfacePictures_ReliefCanRun()"), "the heights are only for Relief, in the real picture, with its shader and maps there");
	Check(Has(usesHeights, "!MirroringActive") && Has(usesHeights, "!MIRROR_CHEATMODE") && Has(usesHeights, "CurrentVisionMode == VISION_MODE_NORMAL"), "and not when something moves corners or flips the view");
	Check(Has(buffer, "if (solidsUseHeights)\n\t\t{\n\t\t\tDrawSolidsWithHeights();\n\t\t}\n\t\telse"), "the main draw takes that way only when asked, and the game's own way otherwise");
	Check(Has(buffer, "mainList->Draw((seeThroughLast || wallsAreDue) ? RenderList::kSolid : RenderList::kEverything);"), "and the game's own way is as it was");
	Check(Has(buffer, "if (solidsUseHeights && !seeThroughLast && !wallsAreDue && mainList->GetSize())"), "and an early drawing still draws its glass");
	Check(Has(drawsHeights, "SurfaceNormals_UseList();\n\tSurfaceNormals_UseTangents();\n\n\tmainList->Draw(RenderList::kSolid, d3d.surfaceReliefDecl);\n\n\tSurfaceNormals_StopUsingTangents();\n\tSurfaceNormals_StopUsingList();"), "the two lists are attached for that one draw, and taken off straight after");
	Check(Has(render, "ShadowReportIsDue()) && LightingNow != kLightingRelief;") && Has(render, "UNDERWATER_CHEATMODE || LightingNow == kLightingRelief)"), "and the fixed buffer stays out of this mode");

	std::string const lists = ReadSourceFile("3dc/RenderList.cpp");
	std::string const drawn = Between(lists, "void RenderList::Draw(Part part, VertexDeclaration *declarationWithHeights)", "// ");

	Check(Has(lists, "if (declarationWithHeights)\n\t{\n\t\tdeclarationWithHeights->Set();"), "with heights the list uses the other declaration");
	Check(Has(lists, "else\n\t{\n\t\t(**decl).Set();"), "without them, its own");
	Check(Has(lists, "R_SetTexture(2, textureRelief.HeightFor(get_texID(it->sortKey)));"), "each polygon gets its own height picture");
	Check(CountOf(lists, "R_ForgetTexture(2);") == 2, "and what's remembered about that stage is forgotten before and after");
	Check(Has(applying, "SayReasonOnce(&saidReliefIsNormals,"), "and the log says why, once");

	std::string const reliefMain = ReadSourceFile("shaders/reliefMain.psh");
	std::string const reliefMainVertex = ReadSourceFile("shaders/reliefMain.vsh");

	Check(Has(reliefMain, "return tex2Dgrad(Tex0, hit.place, hit.dx, hit.dy) * In.Diffuse + In.Specular;"), "Relief's main draw lights a pixel as pixel.psh does, so it shows under baked light");
	Check(Has(reliefMainVertex, "(In.Normal.w < 0.5f) ? float4(0.0f, 0.0f, 0.0f, 0.0f) : In.Tangent"), "and a polygon nobody wrote down stays where it is");

	// the old shaders aren't touched
	Check(!Has(ReadSourceFile("shaders/pixel.psh"), "Normal") && !Has(ReadSourceFile("shaders/vertex.vsh"), "Tangent"), "the game's own main shaders know nothing of any of this");

	std::string const inThisView = Between(render, "bool ToyShadows_VolumesInThisView()", "}");

	Check(Has(inThisView, "return ShadowVolumesOn || ToyShadowsMode != kToyViewFinal;"), "only FINAL obeys the switch, and the other views go on showing the shadow code");

	// collecting
	std::string const collecting = Between(shapes, "void ToyShadows_Collect(DISPLAYBLOCK *dptr, SHAPEHEADER *shapePtr, int kind)\n{", "int const whatItMayDo");
	std::string::size_type const lastTest = collecting.find("ShapeFlag_Sprite");
	std::string::size_type const gate = collecting.find("ToyShadows_VolumesInThisView");

	Check(Has(collecting, "if (!ToyShadows_VolumesInThisView())\n\t{\n\t\tToyShadows_NoteWorldDrawn();\n\t\treturn;"), "with the volumes off, nothing is collected, and the world is noted as drawn");
	Check(lastTest != std::string::npos && gate != std::string::npos && lastTest < gate, "after the other tests, so a frame counts as the same kind of frame either way");

	std::string const walls = Between(shapes, "void ToyShadows_CollectStaticModule(DISPLAYBLOCK *dptr, SHAPEHEADER *shapePtr)\n{", "int ToyShadows_PolygonInWorld");

	Check(Has(walls, "if (ToyShadows_VolumesInThisView())\n\t\t{\n\t\t\tShadowGroup_StartWalked("), "a wall from the fixed buffer only makes its group with the volumes on");
	Check(Has(walls, "else\n\t\t{\n\t\t\tToyShadows_NoteWorldDrawn();"), "and otherwise notes that the world was drawn");

	// the lights still know the world was drawn
	std::string const forgetting = Between(render, "static void ToyForgetCasters()", "earlyFlushThisFrame = false;");

	Check(Has(forgetting, "worldDrawnWithoutCasters = false;"), "that note only lasts the frame");
	Check(Has(render, "return toyNumCasters > 0 || numWalkedGroups > 0 || worldDrawnWithoutCasters;"), "and it counts as the world being drawn");
	Check(Has(render, "bool const lightsOurs = ViewAddsLightsPerPixel()\n\t\t&& ToyWorldWasDrawn() && ToyPicturesCanRun(d3d.lpD3DDevice);"), "so lights stay added per pixel with nothing on the caster list");
	Check(Has(render, "report.lights = ToyWorldWasDrawn() ? ToyShadows_ChosenLights(lights, lightOwners) : 0;"), "and the pass still goes through them");

	// the pass
	Check(Has(render, "bool const volumesThisFrame = ToyShadows_VolumesInThisView();"), "the pass asks the same question");
	Check(Has(render, "StartKeptPrisms(lights, report.lights);\n\n\t// These go after"), "the store still hears which lights there are, so it knows what stood still when the volumes come back");
	Check(Has(render, "if (volumesThisFrame)\n\t{\n\t\tMakeShadowReachView();"), "walls out of view are only looked for with the volumes on");
	Check(Has(render, "int const lightsThatMark = volumesThisFrame ? report.lights : 0;"), "with them off, no light marks anything");
	Check(Has(render, "for (int light = 0; light < lightsThatMark; light++)"), "and that's the loop that builds prisms and marks the stencil");
	Check(Has(render, "if (volumesThisFrame)\n\t\t\t{\n\t\t\t\tToyApplySoftenedMask(device, ToyShadowsMode == kToyViewMask);"), "an empty mask isn't drawn");
	Check(Has(render, "if (!volumesThisFrame)\n\t\t\t\t{\n\t\t\t\t\tToyAddLight(device, thisLight);"), "a light added per pixel goes straight on, with no prisms or mask of its own");
	Check(Has(render, "float const perPass = passesHere ? (float)pixelsByLight[i] / (float)passesHere : 0.0f;"), "the report doesn't divide by no passes");
}

// The rule in light_choice.h, as the game really uses it. These only read the source, so what they say on a graphics card needs a run of the game.
static void TheMapLightsInTheGame()
{
	std::string const shapes = ReadSourceFile("3dc/kshape.cpp");
	std::string const render = ReadSourceFile("3dc/d3d_render.cpp");
	std::string const setup = ReadSourceFile("3dc/avp/win95/object_setup.cpp");
	std::string const commands = ReadSourceFile("3dc/avp/game_commands.cpp");
	std::string const launch = ReadSourceFile("3dc/avp/win95/winmain.cpp");

	// Which kind of light is which
	// A light's kind comes from two real fields, and both start out in the level file: has_corona, which InitPlacedLight sets from the polygon flag iflag_light_corona, and LFlag_AbsPos, which
	// setup_light_data gives a level light, on top of flags copied from the file. A crafted level can say anything in them, and all that can do is say yes or no to a shadow on one player's screen.
	std::string const kinds = Between(shapes, "static LightKindForShadows KindOfLight(", "// What became of every light the choosing looked at");

	Check(Has(kinds, "if (light->LightFlags & LFlag_AbsPos)\n\t{\n\t\treturn kLightFromTheLevel;"), "a light with LFlag_AbsPos is a level light");
	Check(Has(kinds, "owner->I_SBtype == I_BehaviourPlacedLight && owner->SBdataptr"), "only a placed light's data is read as a placed light's");
	Check(Has(kinds, "has_corona ? kLampWithACorona : kLampWithNoCorona"), "and its corona says which kind of lamp it is");
	Check(Has(kinds, "return kLightOfAnyOtherKind;"), "and anything else is left alone");

	std::string::size_type const placedIsLookedFor = kinds.find("I_BehaviourPlacedLight");
	std::string::size_type const flagIsLookedFor = kinds.find("LFlag_AbsPos");

	Check(placedIsLookedFor != std::string::npos && flagIsLookedFor != std::string::npos && placedIsLookedFor < flagIsLookedFor, "a placed light is looked for first, so a flag crafted in the level file can't change what a lamp counts as");
	Check(Has(kinds, "block ? block->ObStrategyBlock : 0"), "a light element, which is on no block, is safe to ask about");

	std::string const levelLights = Between(setup, "void setup_light_data (LIGHTBLOCK * lPtr, Light_Chunk * lc)\n{", "void setup_placed_light_data");
	std::string const placedLights = Between(setup, "void setup_placed_light_data (LIGHTBLOCK * lPtr, Placed_Object_Light_Chunk * lc)\n{", "void SetUpRunTimeLights ()");

	Check(Has(levelLights, "LFlag_AbsPos"), "a level light does get LFlag_AbsPos");
	Check(!Has(placedLights, "LFlag_AbsPos"), "and a placed light doesn't, so the flag tells them apart");

	// Where it's asked
	std::string const considering = Between(shapes, "static void ToyConsiderLight(", "int const distance = ToyDistanceToCamera(light);");

	Check(Has(considering, "if (!LightChoice_MayCast(kind, ShadowAmbientOn != 0))"), "every light on its way to the list is asked about, with what SHADOWAMBIENT says");
	Check(Has(considering, "LightKindForShadows const kind = KindOfLight(light, block);"), "by the block that carries it");

	// How it's set
	Check(Has(render, "int ShadowAmbientOn = 0;"), "SHADOWAMBIENT starts off");

	std::string const ungated = Between(commands, "void CreateGameSpecificConsoleCommands(void)", "bool IsACheat");

	Check(Has(ungated, "\"SHADOWAMBIENT\"") && Has(ungated, "ShadowAmbientCommand"), "the command is made ahead of everything that needs a cheat or -debug");

	std::string const command = Between(render, "void ShadowAmbientCommand(char *words)", "// The menu asks the same way the commands do.");

	Check(Has(command, "SwitchWords_ShadowAmbientAsk(words, &shadowAmbientAsked)"), "the command notes what it's asked for");
	Check(Has(command, "SaySwitch(\"SHADOWAMBIENT TAKES ON OR OFF, OR NOTHING TO SWITCH IT\")"), "and says so if it doesn't know the words");

	std::string const applying = Between(render, "static void ViewSwitches_ApplyAsked()\n{", "static void ToyShadows_Draw()");
	std::string const ambientPart = Between(applying, "if (shadowAmbientAsked != kAskedNothing)", "if (lightingAsked != kAskedNothing)");

	Check(Has(ambientPart, "ShadowAmbientOn = Shadows_AfterAsking(ShadowAmbientOn, shadowAmbientAsked);"), "it only changes between frames, with the other switches");
	Check(Has(ambientPart, "SaySwitch(ShadowAmbientOn ? \"SHADOWAMBIENT: ON\" : \"SHADOWAMBIENT: OFF\");"), "and the screen says which it is now");
	Check(Has(ambientPart, "shadowAmbientAsked = kAskedNothing;"), "and one press only counts once");

	std::string const atStart = Between(launch, "\"-shadowambient\"", "}");

	Check(Has(launch, "CommandLineHasWord(command_line, \"-shadowambient\")"), "-shadowambient is there without -dev, because it only changes this player's own screen");
	Check(Has(atStart, "ShadowAmbientOn = 1;"), "and it turns it on");

	// A network game leaves it alone, because all it changes is what one player's own screen draws
	std::string const tuningTable = Between(render, "static const ShadowTuningNumber shadowTuning[] =", "static float const shadowSurfaceBiasAsBuilt");

	Check(!Has(tuningTable, "ShadowAmbientOn"), "a network game doesn't put it back");
}

static void PuttingTheTuningBack()
{
	int dark = 85;
	int world = 100;
	ShadowTuningNumber const numbers[] = { { &dark, 85 }, { &world, 70 } };

	Check(ShadowTuning_PutBack(numbers, 2), "a number that was changed is noticed");
	Check(dark == 85 && world == 70, "and it goes back to what the game was built with, and one that wasn't changed stays");
	Check(!ShadowTuning_PutBack(numbers, 2), "and the next look finds nothing to do, so the log only says it once");
	Check(!ShadowTuning_PutBack(numbers, 0), "and no numbers is nothing to do");
}

int main()
{
	PuttingTheTuningBack();
	WordsForShadows();
	WordsForAmbient();
	TheMapLightsRule();
	WordsForLighting();
	ShadowsAnswers();
	TheCycle();
	TheSavedWord();
	TheMenuStep();
	TheView();
	TheEightStates();
	TheRealGame();
	TheVolumesSwitch();
	TheMapLightsInTheGame();

	if (checksFailed)
	{
		printf("FAILED: %d checks\n", checksFailed);
		return 1;
	}

	printf("all view switch checks passed\n");
	return 0;
}
