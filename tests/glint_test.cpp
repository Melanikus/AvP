// The test for the wet glint, the highlight that shaders/lightShapes.fxh works out in ShineHere. Written 2026-11-06 by Melanikus for the AvP port.
//
// tests/build.ps1 compiles this, and the program gives back a non-zero result if any check failed. It uses no graphics card. A shader can't be run without one, so this has the same sum written out in C++,
// checks that the shader still says it, and tries it with numbers. The last rows read the game's source, to check the glint is used the way its comments say. What it looks like on a card, only a run can show.

// Modified 2026-11-08 by Melanikus for the AvP port:
//   its Between is the shared one in source_slice.h, so a marker that isn't there is a failed check of its own (SOURCE-SLICE-1, #128)
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <sstream>
#include <string>
#include "source_slice.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
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

	std::ifstream file((path.substr(0, tests) + name).c_str(), std::ios::binary);
	std::stringstream all;
	all << file.rdbuf();

	// Files here have Windows' line ends, and what's looked for is written with plain ones
	std::string text = all.str(), plain;

	for (char const c : text)
	{
		if (c != '\r')
		{
			plain += c;
		}
	}

	return plain;
}

static bool Has(std::string const &text, char const *what)
{
	return text.find(what) != std::string::npos;
}

// The text from one marker up to the next (source_slice.h). A marker that isn't there is a failed check of its own, named by the marker.
static std::string Between(std::string const &text, char const *from, char const *to)
{
	return SourceSlice(text, from, to, kSliceEndFromTheStart, [](const std::string &why) { Check(false, why.c_str()); });
}

static bool ComesBefore(std::string const &text, char const *first, char const *second)
{
	std::string::size_type const a = text.find(first), b = text.find(second);

	return a != std::string::npos && b != std::string::npos && a < b;
}

struct Three
{
	float x, y, z;
};

static Three Minus(Three a, Three b)
{
	Three const out = { a.x - b.x, a.y - b.y, a.z - b.z };
	return out;
}

static Three Over(Three a, float by)
{
	Three const out = { a.x / by, a.y / by, a.z / by };
	return out;
}

static float Dot(Three a, Three b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

static float Length(Three a)
{
	return sqrtf(Dot(a, a));
}

static float AtLeast(float value, float least)
{
	return (value > least) ? value : least;
}

// ShineHere in shaders/lightShapes.fxh, as it's written there
static float Shine(Three toLight, Three position, Three normal, float shininess)
{
	Three const toLightNow = Over(toLight, AtLeast(Length(toLight), 1.0f));

	if (Dot(normal, toLightNow) <= 0.0f)
	{
		return 0.0f;
	}

	Three const halfway = Minus(toLightNow, Over(position, AtLeast(Length(position), 1.0f)));

	return powf(AtLeast(Dot(normal, Over(halfway, AtLeast(Length(halfway), 0.0001f))), 0.0001f), shininess);
}

// The eye is at 0 0 0 in the shader's space. To move the eye, everything else moves the other way.
static float ShineOnAWall(float along, float eyeAlong, float lightAlong, Three normal, float shininess)
{
	Three const position = { along - eyeAlong, 0.0f, 2000.0f };
	Three const light = { lightAlong - eyeAlong, 0.0f, 0.0f };

	return Shine(Minus(light, position), position, normal, shininess);
}

// Where along a long wall the glint is brightest
static float BrightestAlong(float eyeAlong, float lightAlong, Three normal, float shininess)
{
	float best = -1.0f, bestAt = 0.0f;

	for (float along = -6000.0f; along <= 6000.0f; along += 10.0f)
	{
		float const shine = ShineOnAWall(along, eyeAlong, lightAlong, normal, shininess);

		if (shine > best)
		{
			best = shine;
			bestAt = along;
		}
	}

	return bestAt;
}

// How much of a long wall has a glint of at least half its strength
static float WidthOfGlint(float shininess)
{
	Three const facing = { 0.0f, 0.0f, -1.0f };
	float width = 0.0f;

	for (float along = -6000.0f; along <= 6000.0f; along += 10.0f)
	{
		if (ShineOnAWall(along, 0.0f, 0.0f, facing, shininess) >= 0.5f)
		{
			width += 10.0f;
		}
	}

	return width;
}

static bool Near(float a, float b, float within)
{
	return fabsf(a - b) <= within;
}

static void TheSum()
{
	Three const facing = { 0.0f, 0.0f, -1.0f };
	Three const away = { 0.0f, 0.0f, 1.0f };
	Three const sideways = { 1.0f, 0.0f, 0.0f };
	Three const here = { 0.0f, 0.0f, 2000.0f };
	Three const toEye = { 0.0f, 0.0f, -2000.0f };
	Three const nowhere = { 0.0f, 0.0f, 0.0f };
	float const shininess = 40.0f;

	// The strongest it can be, and where
	Check(Near(Shine(toEye, here, facing, shininess), 1.0f, 0.001f), "a surface facing a light that's at the eye shines fully");
	Check(Shine(toEye, here, away, shininess) == 0.0f, "a surface turned away from the light doesn't shine at all");

	// What's seen as you move: the mirror's place, halfway between the eye and the light, when both are the same distance from the wall
	Check(Near(BrightestAlong(0.0f, 2000.0f, facing, shininess), 1000.0f, 20.0f), "on a flat wall the glint is halfway between the eye and the light");
	Check(Near(BrightestAlong(-1000.0f, 2000.0f, facing, shininess), 500.0f, 20.0f), "when the eye moves, the glint slides, by half as far");
	Check(Near(BrightestAlong(0.0f, 4000.0f, facing, shininess), 2000.0f, 20.0f), "and when the light moves");

	// The bumps come from the normal
	{
		float const lean = 0.1f;
		Three const leaning = { lean / sqrtf(1.0f + lean * lean), 0.0f, -1.0f / sqrtf(1.0f + lean * lean) };

		Check(!Near(BrightestAlong(0.0f, 2000.0f, leaning, shininess), 1000.0f, 100.0f), "a bump's tilted normal moves the glint");
	}

	// The number that says how tight
	Check(WidthOfGlint(80.0f) < WidthOfGlint(40.0f) && WidthOfGlint(40.0f) < WidthOfGlint(10.0f), "a higher shininess is a smaller glint");
	Check(WidthOfGlint(40.0f) > 0.0f, "and it's there at all");

	// Numbers that could go wrong. In a shader, a direction with no length gives something that isn't a number, and a pixel with that in it can come out any colour.
	{
		float const lightOnTheSpot = Shine(nowhere, here, facing, shininess);
		Three const beyond = { 0.0f, 0.0f, 3000.0f };
		float const lightStraightBehind = Shine(Minus(beyond, here), here, away, shininess);
		float const atTheEye = Shine(toEye, nowhere, facing, shininess);

		Check(lightOnTheSpot == lightOnTheSpot && lightOnTheSpot >= 0.0f && lightOnTheSpot <= 1.0f, "a light exactly on the spot gives a real number");
		Check(lightStraightBehind == lightStraightBehind && lightStraightBehind >= 0.0f && lightStraightBehind <= 1.0f, "so does a light straight behind the surface, where halfway has no length");
		Check(atTheEye == atTheEye && atTheEye >= 0.0f && atTheEye <= 1.0f, "and a spot exactly at the eye");
	}

	Check(Shine(toEye, here, sideways, shininess) == 0.0f, "edge on to the light, it doesn't shine");

	// Never out of its range, wherever anything is
	{
		bool inRange = true;

		for (float along = -6000.0f; along <= 6000.0f; along += 250.0f)
		{
			for (float lightAlong = -6000.0f; lightAlong <= 6000.0f; lightAlong += 1000.0f)
			{
				float const shine = ShineOnAWall(along, 0.0f, lightAlong, facing, shininess);

				inRange = inRange && shine == shine && shine >= 0.0f && shine <= 1.0f;
			}
		}

		Check(inRange, "it's always from 0 to 1");
	}
}

static void TheShaders()
{
	std::string const shapes = ReadSourceFile("shaders/lightShapes.fxh");
	std::string const shine = Between(shapes, "float ShineHere(float3 toLight, float3 position, float3 normal)", "// How much light one light gives this spot");
	std::string const lightShape = Between(shapes, "float LightShape(float4 light, float shape, float3 position, float3 normal)", "\n}\n");
	std::string const one = ReadSourceFile("shaders/lightOne.psh");
	std::string const lampShine = ReadSourceFile("shaders/lightLampShine.psh");

	// If the shader's sum changes, the sum above has to change with it
	Check(Has(shine, "float3 toLightNow = toLight / max(length(toLight), 1.0f);"), "the shader still has the direction to the light that's tried above");
	Check(Has(shine, "if (dot(normal, toLightNow) <= 0.0f)\n    {\n        return 0.0f;"), "and turning away from it");
	Check(Has(shine, "float3 halfway = toLightNow - position / max(length(position), 1.0f);"), "and halfway between the light and the eye");
	Check(Has(shine, "return pow(max(dot(normal, halfway / max(length(halfway), 0.0001f)), 0.0001f), kShininess);"), "and the sum itself, which never raises 0 to a power");

	// The two numbers the user wanted, at the top, each one a number that makes sense
	{
		std::string::size_type const strengthAt = shapes.find("static const float kShineStrength = ");
		std::string::size_type const shininessAt = shapes.find("static const float kShininess = ");
		float const strength = (strengthAt == std::string::npos) ? -1.0f : (float)atof(shapes.c_str() + strengthAt + 36);
		float const shininess = (shininessAt == std::string::npos) ? -1.0f : (float)atof(shapes.c_str() + shininessAt + 32);

		Check(strength > 0.0f && strength <= 4.0f, "how strong it is, is a number, and not nothing");
		Check(shininess >= 1.0f && shininess <= 512.0f, "and so is how tight");
		Check(strengthAt != std::string::npos && strengthAt < shapes.find("float ShineHere("), "they're above the sum, where someone would look");
	}

	// The shadows depend on this
	Check(!lightShape.empty() && !Has(lightShape, "ShineHere") && !Has(lightShape, "kShine"), "the glint isn't part of how much light a light gives");
	Check(!Has(ReadSourceFile("shaders/lightTotal.psh"), "ShineHere"), "so it can't be in the total, which the shadows divide by");
	Check(!Has(ReadSourceFile("shaders/lightShare.psh"), "ShineHere"), "or in a light's share");

	// The lights
	Check(Has(one, "float  open     = saturate(1.0f - BlockedHere(place.xy, distance));\n\n    share *= open;"), "what a shadow takes away is worked out once, and takes from the share");
	Check(Has(one, "float3 glint = Colour.rgb * mine * open * kShineStrength * ShineHere(Light.xyz - position, position, normal);"), "and from the glint, which comes in the light's own colour, as far as the light reaches");
	Check(Has(one, "return float4(lerp(flare, game, Mine.y) * share + glint, 0.0f);"), "and goes on top of the light");

	// The painted lamp
	Check(Has(lampShine, "clip(tex2Dlod(Colours, place).a - 0.5f);"), "the lamp's glint isn't on what has no surface picture");
	Check(Has(lampShine, "ShineHere(LampPlace.xyz - position, position, normal)"), "it comes from where the lamp is");
	Check(Has(lampShine, "kLampGlint * shine * kShineStrength * inSpot * Lamp.x * Spot.z / (1.0f + away * away)"), "only inside the spot, as bright as the lamp is, and fading with distance");
	Check(Has(lampShine, "saturate((Spot.x - fromAim) / (Spot.x * kGlintEdge))"), "it has a soft edge");
}

static void TheRealGame()
{
	std::string const render = ReadSourceFile("3dc/d3d_render.cpp");
	std::string const lights = ReadSourceFile("3dc/pixel_lights.cpp");
	std::string const cards = ReadSourceFile("win32/d3_func.cpp");
	std::string const lamp = Between(render, "static void DrawShoulderLamp()\n{", "ChangeZWriteEnable(ZWRITE_ENABLED);");
	std::string const glint = Between(render, "static void DrawLampGlint(float strength, float wide, const float *aim, const float *spot)\n{", "\n}\n");
	std::string const adding = Between(lights, "void PixelLights_AddLampGlint(", "\n}\n");

	Check(Has(lamp, "if (SurfacePictures_Normal())\n\t\t{\n\t\t\tDrawLampGlint(strength, wide, aim, spot);"), "the lamp asks for its glint only if there's a picture of which way things face");
	Check(ComesBefore(lamp, "device->SetTexture(4, NULL);", "DrawLampGlint(strength, wide, aim, spot);"), "after giving back the textures it borrowed");
	Check(Has(glint, "if (SUCCEEDED(savedCardSettings ? savedCardSettings->Capture() : device->CreateStateBlock(D3DSBT_ALL, &savedCardSettings)))"), "the card's settings are kept first, because the game caches them");
	Check(ComesBefore(glint, "PixelLights_AddLampGlint(device, toyDepthTexture, numbers);", "savedCardSettings->Apply();"), "and put back after");
	Check(Has(glint, "from[0], -from[1], from[2], 0.0f,"), "the lamp's place is turned the way up that the light list has it");
	Check(Has(adding, "if (d3d.lightLampShineEffect == kNullEffectID)\n\t{\n\t\treturn;"), "a shader that failed to compile means no glint, and no drawing with nothing");
	Check(Has(adding, "device->SetPixelShaderConstantF(1, lampSwingSpotPlace, 4);"), "the four rows of numbers go to the four the shader reads");
	Check(ComesBefore(adding, "DrawRectangle(device);", "StopReadingPictures(device);"), "and it stops reading the pictures when it's done");
	Check(Has(cards, "d3d.lightLampShineEffect = d3d.effectSystem->Add(\"lightLampShine\", \"shadowBlur.vsh\", \"lightLampShine.psh\", d3d.mainDecl);"), "the game loads the shader");
}

int main()
{
	TheSum();
	TheShaders();
	TheRealGame();

	if (checksFailed)
	{
		printf("FAILED: %d checks\n", checksFailed);
		return 1;
	}

	printf("all glint checks passed\n");
	return 0;
}
