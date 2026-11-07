// How much light a per-pixel lamp gives a surface past the edge, where its stored normal says it's turned away. Written
// 2026-11-06 by Melanikus for the AvP port.
//
// WHAT WENT WRONG: with per-pixel lighting, big black patches showed on ceilings and panels that the game's own lighting
// lights (card DARK-PATCH-1). A polygon's stored normals can lean up to 30 degrees, because a corner's normal is shared
// with the polygons that meet there. With a lamp hanging just under a flat ceiling the true angle is tiny, so the lean
// outweighs it, part of the ceiling looks turned away, and the sum gave it nothing. The game gives such a point an eighth.
//
// HOW THIS TESTS A SHADER WITHOUT A GRAPHICS CARD: shaders are nearly C, so the lines of LightShape are written once
// below, and used twice. They're compiled here as C++, and run. And they're looked for, in order, in the text of
// shaders/lightShapes.fxh. So if the shader's sum changes, the text check fails until these lines match it, and then
// the numbers are testing what the shader really says.
//
// WHAT THIS CAN'T SHOW: that a real card works out ddx and ddy the same, or what it all looks like. That needs the game.
// Whether the shader compiles is shader_test's job.

#include <math.h>
#include <stdio.h>

#include <fstream>
#include <sstream>
#include <string>

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static bool Near(float a, float b, float within = 0.0001f)
{
	return fabsf(a - b) <= within;
}

// ---- Just enough of the shader language for those lines to compile as C++

struct float3
{
	float x, y, z;
};

struct float4
{
	union
	{
		struct
		{
			float x, y, z;
		};
		float3 xyz;
	};
	float w;
};

static float3 operator-(float3 a, float3 b)
{
	float3 const made = { a.x - b.x, a.y - b.y, a.z - b.z };
	return made;
}

static float3 operator/(float3 a, float b)
{
	float3 const made = { a.x / b, a.y / b, a.z / b };
	return made;
}

static float dot(float3 a, float3 b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

static float3 cross(float3 a, float3 b)
{
	float3 const made = { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
	return made;
}

static float length(float3 a)
{
	return sqrtf(dot(a, a));
}

static float max(float a, float b)
{
	return (a > b) ? a : b;
}

static float saturate(float a)
{
	return (a < 0.0f) ? 0.0f : (a > 1.0f) ? 1.0f : a;
}

// ddx and ddy say how something changes from this pixel to the one beside it and the one below it. A card knows that
// because it works on pixels in groups. Here each scene says what the answer is, and only for the position, so a check
// below notices if the sum ever asks about anything else.
static float3 thePosition, thePixelAcross, thePixelDown;
static bool askedAboutSomethingElse = false;

static bool Same(float3 a, float3 b)
{
	return a.x == b.x && a.y == b.y && a.z == b.z;
}

static float3 ddx(float3 of)
{
	askedAboutSomethingElse = askedAboutSomethingElse || !Same(of, thePosition);
	return thePixelAcross;
}

static float3 ddy(float3 of)
{
	askedAboutSomethingElse = askedAboutSomethingElse || !Same(of, thePosition);
	return thePixelDown;
}

// ---- THE SUM. These lines are the body of LightShape in shaders/lightShapes.fxh, in the same order, with the same words

#define THE_SUM(LINE) \
	LINE(float3 toLight = light.xyz - position;) \
	LINE(float howFar = length(toLight);) \
	LINE(float reach = saturate(howFar * light.w);) \
	LINE(float turned = dot(normal, toLight / max(howFar, 1.0f));) \
	LINE(float slant = (turned + 0.25f) * 0.5f;) \
	LINE(float3 flat = cross(ddx(position), ddy(position));) \
	LINE(float eyesSide = (dot(flat, position) > 0.0f) ? -1.0f : 1.0f;) \
	LINE(float lampsSide = eyesSide * dot(flat, toLight) / max(length(flat) * howFar, 0.0001f);) \
	LINE(float pastTheEdge = 0.125f * saturate(0.5f + lampsSide * 114.0f);) \
	LINE(if (shape < 0.5f) { return cos(1.5707963f * reach) * saturate(turned); }) \
	LINE(if (shape < 1.5f) { return (1.0f - reach) * ((turned > 0.0f) ? slant : (turned == 0.0f) ? 0.125f : pastTheEdge); }) \
	LINE(return (1.0f - reach * reach) * ((turned > 0.0f) ? slant : pastTheEdge);)

#define AS_CODE(...) __VA_ARGS__
#define AS_TEXT(...) #__VA_ARGS__,

static float LightShape(float4 light, float shape, float3 position, float3 normal)
{
	THE_SUM(AS_CODE)
}

static char const *const theSumsLines[] = { THE_SUM(AS_TEXT) };

// The shapes a light can take, as lightShapes.fxh numbers them
static float const kBaked = 0.0f;
static float const kLive = 1.0f;
static float const kFlare = 2.0f;

// ---- The scenes

// A camera at 0 0 0 looking along z, widening evenly with distance, as PositionHere works it out. This finds the spot on a
// flat surface seen at one place on the screen, then the spots at the pixels beside and below it, as a card would.
static float3 SpotOnSurface(float3 surfaceHolds, float3 surfaceFaces, float across, float up)
{
	float3 const ray = { across, up, 1.0f };
	float const distance = dot(surfaceFaces, surfaceHolds) / dot(surfaceFaces, ray);
	float3 const spot = { across * distance, up * distance, distance };
	return spot;
}

static void LookAt(float3 surfaceHolds, float3 surfaceFaces, float across, float up)
{
	float const onePixel = 0.001f;

	thePosition = SpotOnSurface(surfaceHolds, surfaceFaces, across, up);
	thePixelAcross = SpotOnSurface(surfaceHolds, surfaceFaces, across + onePixel, up) - thePosition;

	// A picture's rows count downwards, so the next row is lower in the view
	thePixelDown = SpotOnSurface(surfaceHolds, surfaceFaces, across, up - onePixel) - thePosition;
}

// A lamp with this range, this far from a spot, along this direction
static float4 LampAt(float3 spot, float3 offset, float range)
{
	float4 lamp;
	lamp.x = spot.x + offset.x;
	lamp.y = spot.y + offset.y;
	lamp.z = spot.z + offset.z;
	lamp.w = 1.0f / range;
	return lamp;
}

static float3 const kCeilingHolds = { 0.0f, 200.0f, 0.0f };
static float3 const kCeilingFaces = { 0.0f, -1.0f, 0.0f };
static float3 const kWallHolds = { 0.0f, 0.0f, 1000.0f };
static float3 const kWallFaces = { 0.0f, 0.0f, -1.0f };
static float3 const kNoDirection = { 0.0f, 0.0f, 0.0f };

// The lamp really measured on Colony: 83 below a flat ceiling, about 6194 along it from the spot that went black. There
// the true angle gave +0.0134, and the stored normal leaned far enough to make it -0.0283.
static void TestALampJustUnderACeiling()
{
	puts("a lamp just under a ceiling, on a corner leaning away from it");

	LookAt(kCeilingHolds, kCeilingFaces, 0.0f, 0.2f);

	float3 const offset = { 0.0f, -83.0f, 6194.0f };
	float const range = 10000.0f;
	float4 const lamp = LampAt(thePosition, offset, range);

	float const lean = 0.0417f;
	float3 const leaning = { 0.0f, -cosf(lean), -sinf(lean) };

	float3 const toLamp = { offset.x, offset.y, offset.z };
	float const howFar = length(toLamp);
	float const turned = dot(leaning, toLamp / howFar);
	Check(Near(turned, -0.0283f, 0.0005f), "the corner's stored normal really does look turned away, by the amount measured");

	float const reach = howFar / range;
	Check(Near(LightShape(lamp, kLive, thePosition, leaning), (1.0f - reach) * 0.125f), "it gets the game's eighth, faded by distance, and isn't black");
	Check(Near(LightShape(lamp, kFlare, thePosition, leaning), (1.0f - reach * reach) * 0.125f), "and a flare's fade with the same eighth");
	Check(Near(LightShape(lamp, kBaked, thePosition, leaning), 0.0f), "a baked light adds nothing past the edge, as before");
	Check(!askedAboutSomethingElse, "the sum only asks how the position changes");
}

// What keeps the fix from lighting everything: the side of a wall away from a lamp, which the wall's shadow doesn't cover
static void TestTheBackOfAWall()
{
	puts("the back of a wall");

	LookAt(kWallHolds, kWallFaces, 0.1f, 0.1f);

	float3 const behind = { 100.0f, 50.0f, 500.0f };
	float4 const lamp = LampAt(thePosition, behind, 5000.0f);

	Check(LightShape(lamp, kLive, thePosition, kWallFaces) == 0.0f, "a live lamp behind it gives exactly nothing");
	Check(LightShape(lamp, kFlare, thePosition, kWallFaces) == 0.0f, "and so does a flare");

	// The same wall with a stored normal leaning the other way can't help, because only the surface itself decides
	float3 const leaning = { 0.5f, 0.0f, -0.866f };
	float const turnedHere = dot(leaning, behind / length(behind));
	Check(turnedHere < 0.0f, "a leaning normal that still looks turned away");
	Check(LightShape(lamp, kLive, thePosition, leaning) == 0.0f, "gives nothing too");

	float3 const leansToIt = { 0.0f, 0.0f, 1.0f };
	Check(LightShape(lamp, kLive, thePosition, leansToIt) > 0.0f, "but a stored normal facing the lamp is still believed, as it always was");

	Check(LightShape(lamp, kFlare, thePosition, kNoDirection) == 0.0f, "a flare behind a wall gives a surface with no direction of its own nothing either");
}

static void TestFacingTheLamp()
{
	puts("a surface really facing the lamp");

	LookAt(kWallHolds, kWallFaces, 0.0f, 0.0f);

	float3 const inFront = { 0.0f, 0.0f, -500.0f };
	float4 const lamp = LampAt(thePosition, inFront, 2000.0f);

	Check(Near(LightShape(lamp, kLive, thePosition, kWallFaces), (1.0f - 0.25f) * 0.625f), "a live lamp's sum is the game's own");
	Check(Near(LightShape(lamp, kFlare, thePosition, kWallFaces), (1.0f - 0.0625f) * 0.625f), "a flare's is the look chosen for flares");
	Check(Near(LightShape(lamp, kBaked, thePosition, kWallFaces), cosf(1.5707963f * 0.25f)), "and a baked light's is a cosine fade");
}

// The patches had hard edges because the light jumped from an eighth to nothing where a stored normal crossed side-on
static void TestNoStepAtTheEdge()
{
	puts("no step where the stored normal crosses side-on");

	LookAt(kWallHolds, kWallFaces, 0.0f, 0.0f);

	float3 const inFront = { 0.0f, 0.0f, -1000.0f };
	float4 const lamp = LampAt(thePosition, inFront, 100000.0f);

	float const tiny = 0.0005f;
	float3 const justFacing = { 0.0f, sqrtf(1.0f - tiny * tiny), -tiny };
	float3 const justTurned = { 0.0f, sqrtf(1.0f - tiny * tiny), tiny };

	float const live1 = LightShape(lamp, kLive, thePosition, justFacing);
	float const live2 = LightShape(lamp, kLive, thePosition, justTurned);
	Check(Near(live1, live2, 0.001f) && live2 > 0.1f, "a live lamp, either side of it");

	float const flare1 = LightShape(lamp, kFlare, thePosition, justFacing);
	float const flare2 = LightShape(lamp, kFlare, thePosition, justTurned);
	Check(Near(flare1, flare2, 0.001f) && flare2 > 0.1f, "a flare, either side of it");
}

// How far past the surface's own plane the lamp is decides how much of the eighth it gets. A lamp lying in the plane mustn't
// flip with the smallest rounding, so it goes smoothly from none to all across a quarter of a degree each way.
static void TestALampInTheSurfacesPlane()
{
	puts("a lamp lying in the surface's own plane");

	// Further off than the other scenes, so that the pixel steps here aren't one unit long. The answer mustn't depend on that.
	float3 const fartherWall = { 0.0f, 0.0f, 3000.0f };
	LookAt(fartherWall, kWallFaces, 0.0f, 0.0f);

	float const range = 100000.0f;
	float const away = 2000.0f;

	// Measured at an angle from the plane, towards the eye when above 0
	struct Row { float degrees; float low; float high; char const *what; };
	Row const rows[] =
	{
		{  0.0f,  0.0620f, 0.0630f, "exactly in the plane, it gets half the eighth" },
		{  0.1f,  0.0860f, 0.0880f, "a tenth of a degree towards the eye, a little more" },
		{ -0.1f,  0.0370f, 0.0390f, "and a tenth the other way, a little less" },
		{  0.3f,  0.1240f, 0.1260f, "by a third of a degree it has all of it" },
		{ -0.3f, -0.0001f, 0.0001f, "and on the far side, none" },
	};

	for (Row const &row : rows)
	{
		float const radians = row.degrees * 3.14159265f / 180.0f;
		float3 const offset = { -away * cosf(radians), 0.0f, -away * sinf(radians) };
		float4 const lamp = LampAt(thePosition, offset, range);

		// The normal has to look turned away from this lamp for the sum to get as far as this
		float3 const facingNot = { 0.5f, 0.0f, 0.866f };
		float const got = LightShape(lamp, kFlare, thePosition, facingNot) / (1.0f - (away / range) * (away / range));
		Check(got >= row.low && got <= row.high, row.what);
	}
}

// Which way round the two pixel steps come out depends on the surface, not on the lamp. The eye's side settles it.
static void TestSeenFromEitherSide()
{
	puts("the eye's side of the surface");

	float3 const floorHolds = { 0.0f, -200.0f, 0.0f };
	float3 const floorFaces = { 0.0f, 1.0f, 0.0f };
	float3 const leaningDown = { 0.0f, -0.5f, -0.866f };
	float3 const overIt = { 0.0f, 83.0f, 3000.0f };
	float3 const underIt = { 0.0f, -83.0f, 3000.0f };

	LookAt(floorHolds, floorFaces, 0.0f, -0.2f);
	float const overTheFloor = LightShape(LampAt(thePosition, overIt, 100000.0f), kFlare, thePosition, leaningDown);
	float const underTheFloor = LightShape(LampAt(thePosition, underIt, 100000.0f), kFlare, thePosition, leaningDown);
	Check(overTheFloor > 0.12f, "a floor looked down on, with a lamp above it, gets the eighth");
	Check(underTheFloor == 0.0f, "and with a lamp below it, nothing");

	float3 const leaningUp = { 0.0f, 0.5f, -0.866f };
	LookAt(kCeilingHolds, kCeilingFaces, 0.0f, 0.2f);
	float const underTheCeiling = LightShape(LampAt(thePosition, underIt, 100000.0f), kFlare, thePosition, leaningUp);
	float const overTheCeiling = LightShape(LampAt(thePosition, overIt, 100000.0f), kFlare, thePosition, leaningUp);
	Check(underTheCeiling > 0.12f, "a ceiling looked up at, with a lamp below it, gets the eighth");
	Check(overTheCeiling == 0.0f, "and with a lamp above it, nothing");

	// The same surface with the two steps the other way round has to give the same answer
	float3 const step = thePixelAcross;
	thePixelAcross = thePixelDown;
	thePixelDown = step;
	Check(LightShape(LampAt(thePosition, underIt, 100000.0f), kFlare, thePosition, leaningUp) == underTheCeiling, "swapping the two pixel steps changes nothing");
}

static void TestOddPixels()
{
	puts("pixels with nothing to go on");

	// Neighbouring pixels at the same place: a flat picture of nothing. There's no surface to say which side the lamp is on.
	float3 const here = { 10.0f, 20.0f, 1000.0f };
	float3 const none = { 0.0f, 0.0f, 0.0f };
	thePosition = here;
	thePixelAcross = none;
	thePixelDown = none;

	float3 const nearby = { 100.0f, 0.0f, 0.0f };
	float4 const lamp = LampAt(here, nearby, 1000.0f);
	float3 const turnedAway = { -1.0f, 0.0f, 0.0f };

	float const got = LightShape(lamp, kFlare, here, turnedAway);
	Check(got == got, "steps of no length don't divide by nothing");
	Check(Near(got, (1.0f - 0.01f) * 0.0625f), "and with no surface to say, it's half the eighth");

	float3 const onTheLamp = { 0.0f, 0.0f, 0.0f };
	float4 const lampHere = LampAt(here, onTheLamp, 1000.0f);
	float const atLive = LightShape(lampHere, kLive, here, turnedAway);
	float const atFlare = LightShape(lampHere, kFlare, here, turnedAway);
	Check(atLive == atLive && atFlare == atFlare, "nor does a lamp exactly on the pixel");

	// A shape can really have no normal. A live lamp lights that as the game does, whichever side it's on.
	LookAt(kWallHolds, kWallFaces, 0.0f, 0.0f);
	float3 const behind = { 0.0f, 0.0f, 500.0f };
	Check(Near(LightShape(LampAt(thePosition, behind, 1000.0f), kLive, thePosition, kNoDirection), 0.5f * 0.125f), "a live lamp gives a surface with no direction of its own the eighth, even from behind");
}

// ---- The shader's own text

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

	return all.str();
}

// A function's text with its comments gone, and any run of spaces and new lines made one space
static std::string PlainText(std::string const &text)
{
	std::string plain;
	bool inComment = false;

	for (size_t i = 0; i < text.size(); i++)
	{
		char const c = text[i];

		if (c == '\n' || c == '\r')
		{
			inComment = false;
		}
		else if (c == '/' && i + 1 < text.size() && text[i + 1] == '/')
		{
			inComment = true;
		}

		if (inComment)
		{
			continue;
		}

		bool const space = (c == ' ' || c == '\t' || c == '\n' || c == '\r');

		if (!space)
		{
			plain += c;
		}
		else if (!plain.empty() && plain.back() != ' ')
		{
			plain += ' ';
		}
	}

	return plain;
}

static void TestTheShaderSaysTheSame()
{
	puts("the shader's own text");

	std::string const shader = ReadSourceFile("shaders/lightShapes.fxh");
	std::string::size_type const start = shader.find("float LightShape(");
	Check(start != std::string::npos, "the shader has LightShape");

	if (start == std::string::npos)
	{
		return;
	}

	std::string const body = PlainText(shader.substr(start));
	std::string::size_type place = 0;

	for (char const *line : theSumsLines)
	{
		std::string::size_type const found = body.find(line, place);
		Check(found != std::string::npos, std::string("it says, in this order: ") + line);

		if (found != std::string::npos)
		{
			place = found + 1;
		}
	}
}

int main()
{
	TestALampJustUnderACeiling();
	TestTheBackOfAWall();
	TestFacingTheLamp();
	TestNoStepAtTheEdge();
	TestALampInTheSurfacesPlane();
	TestSeenFromEitherSide();
	TestOddPixels();
	TestTheShaderSaysTheSame();

	if (checksFailed)
	{
		printf("FAILED: %d checks\n", checksFailed);
		return 1;
	}

	puts("All checks passed");
	return 0;
}
