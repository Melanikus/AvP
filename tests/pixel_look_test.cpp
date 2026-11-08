// Tests for the three numbers that change how a lamp's light looks per pixel. The first half tries pixel_look.cpp. The second half reads the source, because what a word does on a card needs a run
// of the game. Written 2026-11-06 by Melanikus for the AvP port.
// Modified 2026-11-08 by Melanikus for the AvP port:
//   its Between is the shared one in source_slice.h, so a marker that isn't there is a failed check of its own (SOURCE-SLICE-1, #128)
#include "pixel_look.h"

#include <fstream>
#include <stdio.h>
#include <string.h>
#include <sstream>
#include <string>
#include "source_slice.h"

static int checksFailed = 0;

static void Check(bool good, const std::string &what)
{
	printf("%s  %s\n", good ? "ok    " : "FAILED", what.c_str());
	checksFailed += good ? 0 : 1;
}

static std::string ReadSourceFile(char const *name)
{
	// /FC in this test's settings makes __FILE__ the whole path of this file, so everything else can be found from it
	std::string path = __FILE__;
	std::string::size_type const tests = path.rfind("tests");

	if (tests == std::string::npos)
	{
		return "";
	}

	std::ifstream file(path.substr(0, tests) + name, std::ios::binary);
	std::stringstream all;
	all << file.rdbuf();

	std::string text = all.str(), out;

	for (char c : text)
	{
		if (c != '\r')
		{
			out += c;
		}
	}

	return out;
}

static bool Has(const std::string &text, const char *what)
{
	return text.find(what) != std::string::npos;
}

// The text from one marker up to the next (source_slice.h). A marker that isn't there is a failed check of its own, named by the marker.
static std::string Between(const std::string &text, const char *from, const char *to)
{
	return SourceSlice(text, from, to, kSliceEndAfterTheStart, [](const std::string &why) { Check(false, why.c_str()); });
}

static bool Reads(PixelLookNumber which, char const *words, double wanted)
{
	return PixelLook_FromWords(which, words, (unsigned int)strlen(words)) && PixelLook_Now(which) == wanted;
}

static bool Refuses(PixelLookNumber which, char const *words)
{
	double const before = PixelLook_Now(which);

	return !PixelLook_FromWords(which, words, (unsigned int)strlen(words)) && PixelLook_Now(which) == before;
}

static void CheckTheNumbers()
{
	Check(kPixelGlowAsBuilt == 0.25 && PixelLook_Now(kPixelLookGlow) == 0.25, "the glow starts at the quarter the game has always had");
	Check(kPixelCurveAsBuilt == 1.0 && PixelLook_Now(kPixelLookCurve) == 1.0, "the curve starts at 1, which changes nothing");
	Check(kPixelLevelAsBuilt == 1.0 && PixelLook_Now(kPixelLookLevel) == 1.0, "and the level starts at 1, which changes nothing");
	Check(!PixelLook_PutBack(), "so at the start there's nothing to put back");

	float four[4] = { 9.0f, 9.0f, 9.0f, 9.0f };
	PixelLook_Numbers(four);
	Check(four[0] == 0.25f && four[1] == 1.0f && four[2] == 1.0f && four[3] == 0.0f, "the shader gets the three in order");

	Check(Reads(kPixelLookGlow, "0", 0.0), "PIXELGLOW takes the least it allows");
	Check(Reads(kPixelLookGlow, "1", 1.0), "and the most");
	Check(Refuses(kPixelLookGlow, "1.01"), "and nothing past that");
	Check(Reads(kPixelLookCurve, "0.25", 0.25), "PIXELCURVE takes the least it allows");
	Check(Reads(kPixelLookCurve, "4", 4.0), "and the most");
	Check(Refuses(kPixelLookCurve, "0.24"), "and nothing under that, because a curve near 0 turns everything white");
	Check(Refuses(kPixelLookCurve, "4.01"), "or over that");
	Check(Reads(kPixelLookLevel, "0", 0.0), "PIXELLEVEL takes the least it allows");
	Check(Reads(kPixelLookLevel, "2", 2.0), "and the most");
	Check(Refuses(kPixelLookLevel, "2.01"), "and nothing past that");

	Check(Reads(kPixelLookLevel, "0.75", 0.75), "a number with a dot is read exactly");
	Check(Refuses(kPixelLookLevel, "abc"), "a word isn't a number, and changes nothing");
	Check(Refuses(kPixelLookLevel, "-1"), "nor is a minus sign");
	Check(Refuses(kPixelLookLevel, "1e0"), "or a power of ten");
	Check(Refuses(kPixelLookLevel, "1.2.3"), "or two dots");
	Check(Refuses(kPixelLookLevel, ""), "or nothing");
	Check(!PixelLook_FromWords(kPixelLookLevel, 0, 3), "or no words at all");
	Check(Refuses(kPixelLookLevel, "0.0000000001"), "or something longer than anyone needs");

	float changed[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	PixelLook_Numbers(changed);
	Check(changed[0] == 1.0f && changed[1] == 4.0f && changed[2] == 0.75f, "what's been typed reaches the shader");

	Check(PixelLook_PutBack(), "putting them back says they'd been changed");
	Check(PixelLook_Now(kPixelLookGlow) == 0.25 && PixelLook_Now(kPixelLookCurve) == 1.0 && PixelLook_Now(kPixelLookLevel) == 1.0, "and all three are as built");
	Check(!PixelLook_PutBack(), "and a second time there's nothing to say");

	Check(Reads(kPixelLookCurve, "2", 2.0) && PixelLook_PutBack(), "any one of them changed is enough, the curve");
	Check(Reads(kPixelLookLevel, "2", 2.0) && PixelLook_PutBack(), "and the level");
}

static void CheckTheSource()
{
	std::string const render = ReadSourceFile("3dc/d3d_render.cpp");
	std::string const commands = ReadSourceFile("3dc/avp/game_commands.cpp");
	std::string const pixels = ReadSourceFile("3dc/pixel_lights.cpp");
	std::string const one = ReadSourceFile("shaders/lightOne.psh");
	std::string const total = ReadSourceFile("shaders/lightTotal.psh");
	std::string const share = ReadSourceFile("shaders/lightShare.psh");
	std::string const word = Between(render, "static void PixelLookCommand(", "\n}");

	Check(Has(commands, "\"PIXELGLOW\"") && Has(commands, "PixelGlowCommand"), "the console has PIXELGLOW");
	Check(Has(commands, "\"PIXELCURVE\"") && Has(commands, "PixelCurveCommand"), "and PIXELCURVE");
	Check(Has(commands, "\"PIXELLEVEL\"") && Has(commands, "PixelLevelCommand"), "and PIXELLEVEL");
	Check(word.find("ReliefToolsCanAct(name)") < word.find("PixelLook_FromWords"), "a word needs -dev and no network game before it changes anything");
	Check(word.find("if (!ReliefToolsCanAct(name))\n\t{\n\t\treturn;") != std::string::npos, "and stops there if it can't act");
	Check(Has(word, "length > 0 && !PixelLook_FromWords(which, start, (unsigned int)length)"), "with no number it changes nothing");
	Check(Has(word, "SaySwitch(refused);\n\t\treturn;"), "a number it can't take says what it takes, and stops");
	Check(Has(word, "\"%s IS %.2f\", name, PixelLook_Now(which)"), "and it always says what the number is now");
	Check(Has(render, "void PixelGlowCommand(char *words)\n{\n\tPixelLookCommand(kPixelLookGlow,"), "PIXELGLOW changes the glow");
	Check(Has(render, "void PixelCurveCommand(char *words)\n{\n\tPixelLookCommand(kPixelLookCurve,"), "PIXELCURVE changes the curve");
	Check(Has(render, "void PixelLevelCommand(char *words)\n{\n\tPixelLookCommand(kPixelLookLevel,"), "PIXELLEVEL changes the level");
	Check(Has(render, "if (PixelLook_PutBack())"), "a network game puts them back");
	Check(render.find("if (PixelLook_PutBack())") < render.find("bool const viewShowsTooMuch"), "before anything can leave that function early");
	Check(Has(render, "\"PIXEL GLOW %.2f, CURVE %.2f, LEVEL %.2f\", PixelLook_Now(kPixelLookGlow), PixelLook_Now(kPixelLookCurve), PixelLook_Now(kPixelLookLevel)"), "the tag always shows all three");

	Check(Has(pixels, "PixelLook_Numbers(look);"), "the numbers are fetched for each light");
	Check(Has(pixels, "look[0], look[1], look[2], 0.0f,"), "and go to the shader");
	Check(Has(pixels, "SetPixelShaderConstantF(1, constants, 5);"), "all five rows of them, c1 to c5");
	Check(Has(one, "float4    Look   : register(c5);"), "c5 is where the shader reads them");

	Check(Has(one, "pow(max(saturate(lit), 0.0001f), lerp(Look.y, 1.0f, Mine.z))"), "the curve is a power on the light, and the shoulder lamp keeps its own");
	Check(Has(one, "saturate(lit * Look.x) * (1.0f - Mine.z)"), "the glow comes from the number, and the lamp still has none");
	Check(Has(one, "* lerp(Look.z, 1.0f, Mine.z);"), "and the level from its number, except on the lamp");
	Check(!Has(one, "lit * 0.25f"), "the quarter isn't written in the shader any more");
	Check(Has(one, "return float4(lerp(flare, game, Mine.y) * share + glint, 0.0f);"), "a flare doesn't take any of them, and nor does the glint");
	Check(Has(one, "share    = (all > 0.0001f) ? saturate(mine / all) : 0.0f;"), "a light's share is worked out as before");
	Check(!Has(total, "Look") && !Has(share, "Look"), "the total and the shares don't read them, so no shadow moves");
}

int main()
{
	CheckTheNumbers();
	CheckTheSource();

	printf("%s\n", checksFailed ? "FAILED" : "all pixel look checks passed");

	return checksFailed ? 1 : 0;
}
