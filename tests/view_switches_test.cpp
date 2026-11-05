// The test for 3dc/view_switches.h, the decisions behind the LIGHTING and SHADOWS commands. Written 2026-11-05 by Melanikus for the AvP port.
//
// tests/build.ps1 compiles this, and the program gives back a non-zero result if any check failed. It uses no graphics card, and runs none of the game: the
// decisions are all in the header. The last rows read the game's source files, to check they use those decisions the way the header says they do.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fstream>
#include <sstream>
#include <string>

#include "view_switches.h"

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

static void WordsForLighting()
{
	Check(LightingAskFor("") == kAskedNext, "LIGHTING alone goes to the next one");
	Check(LightingAskFor("  ") == kAskedNext, "and so does LIGHTING with only spaces after it");
	Check(LightingAskFor("1") == kLightingRelief, "1 is relief");
	Check(LightingAskFor("2") == kLightingPerPixel, "2 is per pixel");
	Check(LightingAskFor("3") == kLightingGouraud, "3 is Gouraud");
	Check(LightingAskFor("RELIEF") == kLightingRelief, "the name RELIEF");
	Check(LightingAskFor("PERPIXEL") == kLightingPerPixel, "the name PERPIXEL");
	Check(LightingAskFor("GOURAUD") == kLightingGouraud, "the name GOURAUD");
	Check(LightingAskFor("gouraud") == kLightingGouraud, "a name in small letters");
	Check(LightingAskFor("PerPixel\r") == kLightingPerPixel, "a name in mixed letters, at the end of a line from a file");
	Check(LightingAskFor("  relief  ") == kLightingRelief, "a name with spaces around it");

	char const *const refused[] = { "0", "4", "-1", "11", "1 2", "RELIE", "RELIEFS", "RELIEF 1", "PER PIXEL", "GOURAUD2", "ON", "OFF", "\xFF", "RELIEF\xFF" };

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
	// everything there
	Check(Lighting_AfterAsking(kLightingRelief, kAskedNext, true, true) == kLightingPerPixel, "relief goes to per pixel");
	Check(Lighting_AfterAsking(kLightingPerPixel, kAskedNext, true, true) == kLightingGouraud, "per pixel goes to Gouraud");
	Check(Lighting_AfterAsking(kLightingGouraud, kAskedNext, true, true) == kLightingRelief, "Gouraud goes back to relief");

	for (int choice = 0; choice < kLightingChoices; choice++)
	{
		Check(Lighting_AfterAsking(kLightingRelief, choice, true, true) == choice, std::string("from relief, you can jump to ") + Lighting_Word(choice));
		Check(Lighting_AfterAsking(kLightingGouraud, choice, true, true) == choice, std::string("from Gouraud, you can jump to ") + Lighting_Word(choice));
		Check(Lighting_AfterAsking(choice, kAskedNothing, true, true) == choice, std::string("nothing asked leaves ") + Lighting_Word(choice) + " alone");
		Check(Lighting_Drawn(choice, true, true) == choice, std::string(Lighting_Word(choice)) + " is drawn when it's there");
	}

	// relief wasn't built: -reliefoff, or a shader that couldn't be made
	Check(Lighting_AfterAsking(kLightingPerPixel, kAskedNext, false, true) == kLightingGouraud, "with no relief, per pixel goes to Gouraud");
	Check(Lighting_AfterAsking(kLightingGouraud, kAskedNext, false, true) == kLightingPerPixel, "and Gouraud goes past relief, to per pixel");
	Check(Lighting_AfterAsking(kLightingRelief, kAskedNext, false, true) == kLightingGouraud, "still asking for relief, a press moves on from what's really on the screen, which is per pixel");
	Check(Lighting_AfterAsking(kLightingGouraud, kLightingRelief, false, true) == kLightingGouraud, "asking for relief by name when it isn't there changes nothing");
	Check(Lighting_AfterAsking(kLightingGouraud, kLightingPerPixel, false, true) == kLightingPerPixel, "per pixel can still be asked for");
	Check(Lighting_Drawn(kLightingRelief, false, true) == kLightingPerPixel, "relief asked for and not there draws per pixel, as it always has");

	// the card can't draw per pixel at all, or not for this moment
	for (int choice = 0; choice < kLightingChoices; choice++)
	{
		Check(Lighting_Drawn(choice, true, false) == kLightingGouraud, std::string(Lighting_Word(choice)) + " asked for draws Gouraud on a card that can't draw per pixel");
		Check(Lighting_AfterAsking(choice, kAskedNext, true, false) == choice, std::string("a press on that card leaves ") + Lighting_Word(choice) + " asked for, for when it comes back");
		Check(Lighting_AfterAsking(choice, kAskedNext, false, false) == choice, std::string("with no relief either, it still leaves ") + Lighting_Word(choice));
	}

	Check(Lighting_AfterAsking(kLightingRelief, kLightingPerPixel, true, false) == kLightingRelief, "per pixel by name on that card changes nothing");
	Check(Lighting_AfterAsking(kLightingGouraud, kLightingRelief, true, false) == kLightingGouraud, "and nor does relief by name");
	Check(Lighting_AfterAsking(kLightingRelief, kLightingGouraud, true, false) == kLightingGouraud, "but Gouraud can always be asked for");

	// a press taken while the card has gone for a moment, then it's back
	int const during = Lighting_AfterAsking(kLightingRelief, kAskedNext, true, false);
	Check(Lighting_Drawn(during, true, true) == kLightingRelief, "after a device reset, the lighting chosen before it comes back");

	// a key bound twice
	int asked = kAskedNothing;
	SwitchWords_LightingAsk("", &asked);
	SwitchWords_LightingAsk("", &asked);
	Check(Lighting_AfterAsking(kLightingRelief, asked, true, true) == kLightingPerPixel, "a key bound twice moves on one, not two");

	Check(std::string(Lighting_Word(kLightingRelief)) == "RELIEF" && std::string(Lighting_Word(kLightingPerPixel)) == "PERPIXEL" && std::string(Lighting_Word(kLightingGouraud)) == "GOURAUD",
		"each is called what the command calls it");
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
	Check(View_AfterSwitches(kOff, kOff, false, false, &comeBack) == kOff, "and G's own OFF stays, while the switches aren't all off");
	Check(View_AfterSwitches(kOff, kOff, true, true, &comeBack) == kOff && comeBack == kFinal, "still all off, nothing moves");
	Check(View_AfterSwitches(kEdges, kOff, true, true, &comeBack) == kEdges && comeBack == kFinal, "and G's view stays too, while they are");
	Check(View_AfterSwitches(kEdges, kOff, true, false, &comeBack) == kEdges, "G moved it off OFF: turning a switch on leaves it where G put it");

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

	// the moves of G
	Check(View_AfterG(kOff, kViewCount, kFinal, false) == kFinal, "G goes from OFF to FINAL");
	Check(View_AfterG(kFinal, kViewCount, kFinal, false) == kEdges, "and on to the next");
	Check(View_AfterG(kPolygons, kViewCount, kFinal, false) == kOff, "and round from the last to OFF");
	Check(View_AfterG(kOff, kViewCount, kFinal, true) == kEdges, "with everything off, G goes past FINAL");
	Check(View_AfterG(kEdges, kViewCount, kFinal, true) == kPolygons, "the other views are as they were");
	Check(View_AfterG(kPolygons, kViewCount, kFinal, true) == kOff, "and it still comes round to OFF");
}

// Every one of the six, on a card that has everything
static void TheSixStates()
{
	int seen = 0;

	for (int shadows = 0; shadows <= 1; shadows++)
	{
		for (int choice = 0; choice < kLightingChoices; choice++)
		{
			int const lighting = Lighting_AfterAsking(kLightingRelief, choice, true, true);
			int const volumes = Shadows_AfterAsking(1, shadows);

			if (lighting == choice && volumes == shadows)
			{
				seen++;
			}
		}
	}

	Check(seen == 6, "all six can be reached");

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
	return all.str();
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
	std::string const applying = Between(render, "static void ViewSwitches_ApplyAsked()\r\n{", "static void ToyShadows_Draw()");
	std::string const keys = Between(applying, "if (DevMode", "bool const hasStencil");

	Check(CountOf(render, "ViewSwitches_ApplyAsked();") == 2, "the switches change from one place, named once ahead and called once");
	Check(Has(render, "ToyShadows_Draw();\r\n\t\tViewSwitches_ApplyAsked();"), "and that's straight after the frame's shadows");

	Check(Has(keys, "if (DevMode && IOFOCUS_AcceptControls() && !(IOFOCUS_Get() & (IOFOCUS_NEWCONSOLE | IOFOCUS_OSK)))"), "the keys need -dev, and do nothing while typing");
	Check(Has(keys, "DebouncedKeyboardInput[KEY_L]") && Has(keys, "lightingAsked = kAskedNext"), "L is the next lighting, once a press");
	Check(Has(keys, "DebouncedKeyboardInput[KEY_H]") && Has(keys, "shadowsAsked = kAskedNext"), "H switches the shadows, once a press");
	Check(CountOf(render, "KEY_L]") == 1 && CountOf(render, "KEY_H]") == 1, "and nothing else in the drawing code reads those keys");

	std::string const shadowsCommand = Between(render, "void ShadowsCommand(char *words)", "void LightingCommand(char *words)");
	std::string const lightingCommand = Between(render, "void LightingCommand(char *words)", "// Where LIGHTING and SHADOWS change");

	Check(Has(shadowsCommand, "SwitchWords_ShadowsAsk(words, &shadowsAsked)") && !Has(shadowsCommand, "ShadowVolumesOn"), "SHADOWS only notes what it's asked for");
	Check(Has(lightingCommand, "SwitchWords_LightingAsk(words, &lightingAsked)") && !Has(lightingCommand, "LightingNow ="), "LIGHTING only notes what it's asked for");

	Check(Has(render, "int LightingNow = kLightingRelief;"), "the game starts in relief");
	Check(Has(render, "int ShadowVolumesOn = 1;"), "with shadows on");
	Check(Has(render, "SurfacePictures_Draw(device, toyDepthSurface, worldAxes, LightingNow == kLightingRelief, useStatic)"), "the surface pass asks for relief only when that's the choice");
	Check(Has(render, "ToyShadowsMode = View_AfterG(ToyShadowsMode, kToyViewCount, kToyViewFinal, switchesWereAllOff);"), "G goes past FINAL when everything is off");

	std::string const addsPerPixel = Between(render, "static bool ViewAddsLightsPerPixel()", "}");

	Check(Has(addsPerPixel, "ToyShadowsMode == kToyViewFinal && LightingNow != kLightingGouraud"), "in FINAL, Gouraud keeps the live lights in the game's own lighting");
	Check(Has(addsPerPixel, "ToyShadowsMode == kToyViewMask") && Has(addsPerPixel, "ToyShadowsMode == kViewNormals"), "and the views for looking inside the pass take no notice of it");
	Check(Has(render, "bool const lightsOurs = ViewAddsLightsPerPixel()"), "the pass asks that before it takes any lights");
	Check(Has(applying, "if (!ViewAddsLightsPerPixel())\r\n\t{\r\n\t\tToyShadowsLightsByPixel = 0;"), "and a change hands them back in time for the very next frame");

	Check(Has(applying, "ToyShadowsMode = View_AfterSwitches(ToyShadowsMode, kToyViewOff, switchesWereAllOff, allOff, &viewToComeBackTo);"), "everything off goes to the OFF view");
	Check(Has(applying, "if (!hasStencil)\r\n\t{\r\n\t\tShadowVolumesOn = 0;\r\n\t\tLightingNow = kLightingGouraud;"), "with no stencil bits, both are held off every frame");

	// The tag is a fixed list of lines on the stack, and more lines than it has room for would write past its end.
	std::string const tag = Between(render, "enum { kMostLines = ", "int characters = 0;");
	int const room = atoi(tag.c_str() + (tag.empty() ? 0 : strlen("enum { kMostLines = ")));
	int const lines = CountOf(tag, "lines[numLines++]");
	Check(lines > 0 && lines <= room, "the tag has room for every line it can write, even if all of them come at once");

	std::string const commands = ReadSourceFile("3dc/avp/game_commands.cpp");
	std::string const ungated = Between(commands, "void CreateGameSpecificConsoleCommands(void)", "bool IsACheat");

	Check(Has(ungated, "\"LIGHTING\"") && Has(ungated, "LightingCommand"), "LIGHTING is made ahead of everything that needs a cheat or -debug");
	Check(Has(ungated, "\"SHADOWS\"") && Has(ungated, "ShadowsCommand"), "and so is SHADOWS");
	Check(!Has(ungated, "DebuggingCommandsActive"), "and nothing there holds them back");

	std::string const launch = ReadSourceFile("3dc/avp/win95/winmain.cpp");
	std::string const reliefOff = Between(launch, "\"-reliefoff\"", "}");
	std::string const shadowsOff = Between(launch, "\"-shadowsoff\"", "}");

	Check(Has(reliefOff, "LightingNow = kLightingPerPixel;"), "-reliefoff starts in per pixel");
	Check(Has(shadowsOff, "ToyShadowsMode = 0;") && Has(shadowsOff, "ShadowVolumesOn = 0;") && Has(shadowsOff, "LightingNow = kLightingGouraud;"), "-shadowsoff starts with everything off");
}

int main()
{
	WordsForShadows();
	WordsForLighting();
	ShadowsAnswers();
	TheCycle();
	TheView();
	TheSixStates();
	TheRealGame();

	if (checksFailed)
	{
		printf("FAILED: %d checks\n", checksFailed);
		return 1;
	}

	printf("all view switch checks passed\n");
	return 0;
}
