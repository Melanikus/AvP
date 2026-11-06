// The test for how many lights can cast shadows, the light total's passes, and what a card that can't do them gets. Written 2026-11-06 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// The sums, which run here for real: which picture a card can keep the light total in, and which part of the screen a light can reach. A rectangle
// that's too small would cut off part of a light, so thousands of lights and points are tried against it.
//
// The rest needs a graphics card, so this reads the source and only shows that the lines are there: the limits, each light adding itself to the
// total, the road a card takes when it can't do that, and the log's counts. What they do on a card needs a run of the game. -shadownofloatblend is
// there so that a run can try that road on any card. See tests/takeouts/README.md.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this, and the program gives back a non-zero result if any check failed. It reads the source from the repository, and needs no game files.

#include <stdio.h>

#include <fstream>
#include <sstream>
#include <string>

#include "light_reach.h"
#include "light_total_picture.h"

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
	// /FC in this test's settings makes __FILE__ the whole path of this file, so the repository's top is found from it
	std::string path = __FILE__;
	std::string::size_type const tests = path.rfind("tests");

	path = path.substr(0, tests) + name;

	std::ifstream file(path.c_str(), std::ios::binary);
	std::stringstream all;
	all << file.rdbuf();

	if (all.str().empty())
	{
		Check(false, std::string("couldn't read ") + name);
	}

	std::string text;

	for (char const c : all.str())
	{
		if (c != '\r')
		{
			text += c;
		}
	}

	return text;
}

static std::string Between(std::string const &text, char const *from, char const *to)
{
	std::string::size_type const start = text.find(from);

	if (start == std::string::npos)
	{
		return "";
	}

	std::string::size_type const end = text.find(to, start + 1);

	return text.substr(start, (end == std::string::npos) ? std::string::npos : end - start);
}

static bool Has(std::string const &text, char const *words)
{
	return text.find(words) != std::string::npos;
}

static bool ComesBefore(std::string const &text, char const *first, char const *second)
{
	std::string::size_type const a = text.find(first);
	std::string::size_type const b = text.find(second);

	return a != std::string::npos && b != std::string::npos && a < b;
}

static unsigned int randomState = 1999;

static float RandomBetween(float lowest, float highest)
{
	randomState = randomState * 1103515245u + 12345u;

	float const fraction = (float)((randomState >> 8) & 0xFFFFFF) / 16777216.0f;

	return lowest + (highest - lowest) * fraction;
}

static void CheckTheBestPicture()
{
	Check(LightTotal_BestPicture(true, true) == kLightTotalTwoNumbers, "a card that can add on both kinds of picture gets the smaller");
	Check(LightTotal_BestPicture(true, false) == kLightTotalTwoNumbers, "and so does one that can only on that");
	Check(LightTotal_BestPicture(false, true) == kLightTotalFourNumbers, "a card that can only add on four numbers gets that");
	Check(LightTotal_BestPicture(false, false) == kLightTotalNoPicture, "and a card that can't on either gets none");
}

static void CheckTheReachOfALight()
{
	float const nearest = 64.0f;
	float const across = 1.0f, up = 1.777f;
	LightReachBox box;

	Check(!LightReach_Box(0.0f, 0.0f, -2000.0f, 1000.0f, nearest, across, up, &box), "a light whose whole ball is behind the camera reaches nothing");
	Check(LightReach_Box(0.0f, 0.0f, -500.0f, 1000.0f, nearest, across, up, &box), "one behind the camera whose ball comes round in front of it does");
	Check(box.left == -1.0f && box.right == 1.0f && box.bottom == -1.0f && box.top == 1.0f, "and gets the whole screen, because part of the ball is level with the camera");
	Check(LightReach_Box(0.0f, 0.0f, 500.0f, 1000.0f, nearest, across, up, &box) && box.left == -1.0f && box.top == 1.0f, "and so does the camera inside a light's ball");

	Check(!LightReach_Box(90000.0f, 0.0f, 10000.0f, 1000.0f, nearest, across, up, &box), "a light far off to the right reaches nothing");
	Check(!LightReach_Box(-90000.0f, 0.0f, 10000.0f, 1000.0f, nearest, across, up, &box), "nor far off to the left");
	Check(!LightReach_Box(0.0f, 90000.0f, 10000.0f, 1000.0f, nearest, across, up, &box), "nor far above");
	Check(!LightReach_Box(0.0f, -90000.0f, 10000.0f, 1000.0f, nearest, across, up, &box), "nor far below");

	Check(LightReach_Box(0.0f, 0.0f, 10000.0f, 1000.0f, nearest, across, up, &box), "a small light straight ahead reaches the screen");
	Check(box.left > -1.0f && box.right < 1.0f && box.bottom > -1.0f && box.top < 1.0f, "and only some of it, or the rectangle saves nothing");
	Check(box.left < 0.0f && box.right > 0.0f && box.bottom < 0.0f && box.top > 0.0f, "with the light inside its own rectangle");

	Check(LightReach_Box(9000.0f, 0.0f, 10000.0f, 2000.0f, nearest, across, up, &box) && box.right == 1.0f, "a rectangle going off the screen stops at its edge");
	Check(LightReach_Box(-9000.0f, 0.0f, 10000.0f, 2000.0f, nearest, across, up, &box) && box.left == -1.0f, "on either side");
	Check(LightReach_Box(0.0f, 7000.0f, 10000.0f, 2000.0f, nearest, across, up, &box) && box.top == 1.0f, "or at the top");
	Check(LightReach_Box(0.0f, -7000.0f, 10000.0f, 2000.0f, nearest, across, up, &box) && box.bottom == -1.0f, "or the bottom");

	// Every point a light can reach, that the camera can see and that's on the screen, has to be inside the light's rectangle.
	int pointsTried = 0, pointsOutside = 0, lightsCalledUnreachable = 0;

	for (int i = 0; i < 20000; i++)
	{
		float const x = RandomBetween(-30000.0f, 30000.0f);
		float const y = RandomBetween(-30000.0f, 30000.0f);
		float const z = RandomBetween(-20000.0f, 30000.0f);
		float const range = RandomBetween(100.0f, 25000.0f);
		float const magnify = RandomBetween(0.5f, 3.0f);
		bool const reaches = LightReach_Box(x, y, z, range, nearest, magnify, magnify * 1.777f, &box);

		for (int j = 0; j < 20; j++)
		{
			float const dx = RandomBetween(-1.0f, 1.0f), dy = RandomBetween(-1.0f, 1.0f), dz = RandomBetween(-1.0f, 1.0f);
			float const longest = dx * dx + dy * dy + dz * dz;

			if (longest > 1.0f)
			{
				continue;
			}

			float const pointZ = z + dz * range;

			if (pointZ < nearest)
			{
				continue;
			}

			float const onScreenX = magnify * (x + dx * range) / pointZ;
			float const onScreenY = magnify * 1.777f * (y + dy * range) / pointZ;

			if (onScreenX < -1.0f || onScreenX > 1.0f || onScreenY < -1.0f || onScreenY > 1.0f)
			{
				continue;
			}

			pointsTried++;

			if (!reaches)
			{
				lightsCalledUnreachable++;
			}
			else if (onScreenX < box.left - 0.0001f || onScreenX > box.right + 0.0001f || onScreenY < box.bottom - 0.0001f || onScreenY > box.top + 0.0001f)
			{
				pointsOutside++;
			}
		}
	}

	Check(pointsTried > 10000, "enough points in view were tried: " + std::to_string(pointsTried));
	Check(lightsCalledUnreachable == 0, "no light was said to reach nothing when it reached a point on the screen");
	Check(pointsOutside == 0, "no point a light reaches on the screen was outside its rectangle");
}

static void CheckTheLimits()
{
	std::string const limits = ReadSourceFile("3dc/d3d_render.h");
	std::string const render = ReadSourceFile("3dc/d3d_render.cpp");
	std::string const switches = ReadSourceFile("3dc/avp/win95/winmain.cpp");
	std::string const shapes = ReadSourceFile("3dc/kshape.cpp");

	Check(Has(limits, "kToyMostSteadyLights = 20,"), "20 steady lights can cast shadows");
	Check(Has(limits, "kToyMostBriefLights = 12,"), "and 12 brief ones");
	Check(Has(render, "int ToyShadowsMaxLights = kToyMostSteadyLights;"), "and the game uses all of those unless it's told otherwise");
	Check(Has(render, "int ToyShadowsMaxBrief = kToyMostBriefLights;"), "of both kinds");
	Check(Has(switches, "ToyShadowsMaxLights = (asked < 1) ? 1 : (asked > 20) ? 20 : asked;"), "-shadowlights goes from 1 to 20");
	Check(Has(switches, "ToyShadowsMaxBrief = (asked < 0) ? 0 : (asked > 12) ? 12 : asked;"), "-shadowbrief goes from 0 to 12");
	Check(Has(shapes, "(ToyShadowsMaxLights < kToyMostSteadyLights) ? ToyShadowsMaxLights : kToyMostSteadyLights"), "and whatever those say, the lists can't be overrun");
	Check(Has(shapes, "(ToyShadowsMaxBrief < kToyMostBriefLights) ? ToyShadowsMaxBrief : kToyMostBriefLights"), "of either kind");
	Check(Has(render, "static_assert(sizeof(kToyLightColours) / sizeof(kToyLightColours[0]) == kToyMostLights"), "every place on the list has a colour, or the build stops");
	Check(Has(render, "static_assert(kToyMostLights <= kShadowVolumeMostLights"), "and the store of kept prisms has room for them all, or the build stops");
	Check(Has(render, "int ShadowVolumesOn = 0;") && Has(render, "int LightingNow = kLightingGouraud;"), "a fresh install is still Gouraud with shadows off");
	Check(Has(render, "for (int i = 0; i < report.lights; i++)\n\t\t\t{\n\t\t\t\tchar number[16];"), "the log's pixels are written for each light on the list, not for a fixed number of them");
}

static void CheckTheTotalsPasses()
{
	std::string const shader = ReadSourceFile("shaders/lightTotal.psh");
	std::string const lights = ReadSourceFile("3dc/pixel_lights.cpp");
	std::string const render = ReadSourceFile("3dc/d3d_render.cpp");
	std::string const total = Between(lights, "float PixelLights_DrawTotal(", "HRESULT PixelLights_AddShareToMask(");

	Check(Has(shader, "float4 ThisLight  : register(c1);") && Has(shader, "float4 ThisWeight : register(c2);"), "the total's shader is given one light");
	Check(!Has(shader, "for ("), "with no list to go through, which is what cost every pixel every light");
	Check(Has(shader, "amount * (1.0f - ThisWeight.z), amount * ThisWeight.z"), "and it's in one total or the other, never both");

	Check(ComesBefore(total, "D3DRS_SCISSORTESTENABLE, FALSE", "device->Clear(0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0);"), "every pixel starts with no light on it, all over the picture");
	Check(ComesBefore(total, "device->Clear(0, NULL, D3DCLEAR_TARGET", "for (int i = 0;"), "before any light is added");
	Check(Has(total, "SetRectangleState(device, false, true);"), "what a light writes goes on top of what's there");
	Check(Has(total, "device->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);"), "and only on its own part of the screen");
	Check(Has(total, "device->SetScissorRect(&reach[i]);"), "which is its own, for each light");
	Check(Has(total, "device->SetScissorRect(&reach[i]);\n\t\tdevice->SetPixelShaderConstantF(1, constants, 2);\n\t\tDrawRectangle(device);"), "and each light is drawn on its own, with both lines of its numbers");
	Check(Has(total, "LightOnListIsDark(light) || !reachesScreen[i]"), "a dark place, or a light that reaches nothing, is left out");
	Check(Has(total, "reach[i].right <= reach[i].left || reach[i].bottom <= reach[i].top"), "and so is a rectangle with nothing in it");
	Check(Has(total, "Largest(light[kLightRed], light[kLightGreen], light[kLightBlue]) : (light[kLightRed] + light[kLightGreen] + light[kLightBlue]) / 3.0f"), "a light added per pixel counts by its brightest colour, and a baked one by the average");
	Check(ComesBefore(total, "for (int i = 0;", "device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);\n\tStopReadingPictures(device);"), "the scissor is off again afterwards");

	std::string const rectangle = Between(render, "static bool ToyLightRectangle(", "// Turns the stencil's marks for this flare");

	Check(Has(rectangle, "LightReach_Box(x, y, z, range, zNear, d3d.matProjection._11, d3d.matProjection._22, &box)"), "the rectangle comes from the sum tried above, with the real view");
	Check(Has(rectangle, "(LONG)floor((box.left + 1.0f)") && Has(rectangle, "(LONG)ceil((box.right + 1.0f)"), "and a part of a pixel counts as all of it");
	Check(ComesBefore(render, "lightReachesScreen[light] = ToyLightRectangle(device,", "report.screensOfTotal = PixelLights_DrawTotal(device, lights, report.lights, lightReach, lightReachesScreen, toyDepthTexture);"), "the shadow pass works them out and hands them over");
}

// With no float blending the game has to go on. There's no way to take a card's abilities away in a test, so this follows the road the answer takes,
// from the card's answer to the things that ask. A run with -shadownofloatblend tries the same road on a real card.
static void CheckTheCardThatCantAdd()
{
	std::string const lights = ReadSourceFile("3dc/pixel_lights.cpp");
	std::string const render = ReadSourceFile("3dc/d3d_render.cpp");
	std::string const switches = ReadSourceFile("3dc/avp/win95/winmain.cpp");
	std::string const best = Between(lights, "static D3DFORMAT BestTotalFormat(", "bool PixelLights_Ready(");
	std::string const ready = Between(lights, "bool PixelLights_Ready(", "static void SetScreenConstant(");
	std::string const mask = Between(render, "static D3DFORMAT ToyBestMaskFormat(", "static bool ToyMakeTargets(");

	Check(Has(best, "D3DUSAGE_RENDERTARGET | D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING"), "the card is asked whether it can add on the picture, not just draw on it");
	Check(Has(best, "LightTotal_BestPicture(canAdd[0], canAdd[1])"), "and its answers go through the sum tried above");
	Check(Has(best, "default:\n\t\t\treturn D3DFMT_UNKNOWN;"), "which can come to no picture");
	Check(Has(best, "!ShadowsPretendNoFloatBlend && direct3D->CheckDeviceFormat("), "-shadownofloatblend makes every card give that answer");
	Check(Has(mask, "!ShadowsPretendNoFloatBlend && direct3D->CheckDeviceFormat("), "for the mask's picture too, as a real card that can't would");

	Check(ComesBefore(ready, "if (format == D3DFMT_UNKNOWN)", "device->CreateTexture("), "no picture is asked for when there's none it can add on");
	Check(Has(ready, "LogString(\"pixel lights: the card can't add to a float picture, so lights stay in the game's own lighting, and shadows are drawn without the surface pictures\");"), "the log says why");
	Check(Has(Between(ready, "if (format == D3DFMT_UNKNOWN)", "device->CreateTexture("), "totalFailed = true;"), "it's only worked out once, so the log says it once");
	Check(Has(Between(ready, "if (format == D3DFMT_UNKNOWN)", "device->CreateTexture("), "return false;"), "and whoever asked hears no");

	Check(Has(render, "return d3d.shadowBlurEffect != kNullEffectID && PixelLights_Ready(device) && ToyMakeTargets(device);"), "the shadow pass asks that before it uses the pictures");
	Check(Has(render, "&& ToyWorldWasDrawn() && ToyPicturesCanRun(d3d.lpD3DDevice);"), "with a no, lights stay in the game's own lighting, so nothing goes dark");
	Check(Has(render, "if (darkening && ToyPicturesCanRun(device)\n"), "and shadows take the road they take with no pictures");
	Check(Has(render, "the pictures hold last frame's surfaces, so this frame takes the flat shadows"), "which is there, and darkens by a flat share");

	Check(Has(switches, "devSwitch(\"-shadownofloatblend\")"), "the switch needs -dev");
	Check(Has(render, "{ &ShadowsPretendNoFloatBlend, ShadowsPretendNoFloatBlend },"), "and a network game puts it back");
}

static void CheckTheLog()
{
	std::string const shapes = ReadSourceFile("3dc/kshape.cpp");
	std::string const render = ReadSourceFile("3dc/d3d_render.cpp");
	std::string const consider = Between(shapes, "static void ToyConsiderLight(", "// Up to 20 steady lights");
	std::string const say = Between(shapes, "void ShadowLights_SayCounts(", "// Gives the shadow pass this frame's list");
	std::string const note = Between(shapes, "void ShadowLights_NoteFrame(", "// Two lines for each report");
	std::string const choosing = Between(shapes, "void ToyShadows_ChooseLights()", "int placeOf[kToyMostLights];");

	Check(Has(consider, "lightsThisFrame.dark++;"), "a dark light is counted");
	Check(Has(consider, "lightsThisFrame.atTheCamera++;"), "and one at the camera");
	Check(Has(consider, "lightsThisFrame.outOfView++;"), "and one that can't reach the view");
	Check(Has(consider, "(*inReach)++;"), "and one that competes for a place");
	Check(Has(consider, "lightsThisFrame.notAVisibleLamp++;"), "and one of the map's lights held back for showing no corona");
	Check(ComesBefore(consider, "lightsThisFrame.notAVisibleLamp++;", "lightsThisFrame.outOfView++;"), "ahead of the other reasons, so the count says how many the rule held back");
	Check(Has(consider, "lightsThisFrame.notAVisibleLampInView++;"), "and how many of those could have reached the view");
	Check(Has(consider, "lightsThisFrame.lampsInReach++;"), "and how many lamps with a corona are competing");
	Check(ComesBefore(consider, "(*inReach)++;", "lightsThisFrame.lampsInReach++;"), "which is only those that got past the other reasons");
	Check(ComesBefore(consider, "(*inReach)++;", "int place = *numFound;"), "whether or not it gets one");

	Check(ComesBefore(choosing, "memset(&lightsThisFrame, 0, sizeof(lightsThisFrame));", "if (!ToyShadowsMode || CurrentVisionMode != VISION_MODE_NORMAL)"), "the counts start afresh every frame, even one with nothing to choose");
	Check(Has(choosing, "lightsThisFrame.steadyChosen = numSteady;") && Has(choosing, "lightsThisFrame.briefChosen = numBrief;"), "and what was chosen is noted");

	Check(Has(say, "lightsThisFrame.steadyInReach - lightsThisFrame.steadyChosen, lightsThisFrame.briefInReach - lightsThisFrame.briefChosen"), "passed over with no place left is what competed and didn't get one");
	Check(Has(say, "%d in reach; chosen %d steady and %d brief, of %d and %d allowed"), "each report says how many were in reach and chosen");
	Check(Has(say, "%d steady and %d brief with no place left, %d out of view, %d at the camera, %d dark"), "and which were passed over and why");
	Check(Has(say, "held back: %d of the map's lights with no corona, %d of them in view (SHADOWAMBIENT %s), %d lamps with a corona in reach"), "and how many the rule held back");
	Check(Has(say, "lightsThisFrame.notAVisibleLamp, lightsThisFrame.notAVisibleLampInView, ShadowAmbientOn ? \"ON\" : \"OFF\", lightsThisFrame.lampsInReach,"), "with the right numbers in the right places");
	Check(Has(say, "%d shadow passes, the light total covered %.1f screens, %.1f ms on the processor, %.1f ms a frame"), "and what it cost");
	Check(Has(say, "the most in one frame so far: %d in reach (frame %d, room %s), %d chosen, %d shadow passes, %.1f ms on the processor"), "and the most there have been");

	Check(Has(note, "if (inReach > mostInReach)") && Has(note, "(chosen > mostChosen)") && Has(note, "(passes > mostPasses)") && Has(note, "(milliseconds > mostMilliseconds)"), "each of the most goes up on its own");
	Check(Has(render, "ShadowLights_NoteFrame(report.frame, passes, report.milliseconds);"), "they're noted on every frame the pass finishes");
	Check(Has(render, "ShadowLights_NoteFrame(report.frame, passes, report.milliseconds);\n\n\tif (logThisFrame)\n\t{\n\t\tToyLogFrame(report);"), "not only on a report");
	Check(ComesBefore(render, "if (volumesThisFrame && !LightOnListIsDark(&lights[i * kToyLightFloats]))", "ShadowLights_NoteFrame(report.frame, passes, report.milliseconds);"), "with this frame's passes counted first");
	Check(Has(Between(render, "static void ToyLogFrame(", "static void ToySetVolumeState("), "ShadowLights_SayCounts(report.frame, report.passes, report.screensOfTotal, report.milliseconds, report.frameMilliseconds);"), "every report says them, even with no light chosen");
}

// The word only reads the source here: what it does on a card needs a run. The words it takes are tried in tests/view_switches_test.cpp, as SHADOWAMBIENT's.
static void CheckTheEvenGapWord()
{
	std::string const render = ReadSourceFile("3dc/d3d_render.cpp");
	std::string const commands = ReadSourceFile("3dc/avp/game_commands.cpp");
	std::string const word = Between(render, "void ShadowEvenGapCommand(char *words)", "void ReliefDepthCommand(char *words)");

	Check(Has(commands, "ConsoleCommand::Make(\"SHADOWEVENGAP\""), "the console has the word");
	Check(ComesBefore(word, "ReliefToolsCanAct(\"SHADOWEVENGAP\")", "ShadowsEvenGap ="), "it needs -dev and no network game before it changes anything");
	Check(ComesBefore(word, "SwitchWords_ShadowAmbientAsk(words, &asked)", "ShadowsEvenGap ="), "a word it doesn't know changes nothing");
	Check(Has(word, "SHADOWEVENGAP TAKES ON OR OFF, OR NOTHING TO SWITCH IT"), "and says what it takes");
	Check(Has(word, "ShadowsEvenGap = Shadows_AfterAsking(ShadowsEvenGap, asked);"), "alone it switches, and ON or OFF says which");
	Check(Has(word, "ShadowVolumes_Empty();"), "kept prisms made the old way don't stay");
	Check(Has(word, "SaySwitch(ShadowsEvenGap ? \"SHADOWEVENGAP: ON\" : \"SHADOWEVENGAP: OFF\");"), "the screen says which it is now");
	Check(Has(render, "\"EVEN GAP %s, %sBIAS %d\", ShadowsEvenGap ? \"ON\" : \"OFF\""), "and the tag says so all the time");
	Check(!Has(Between(render, "static void ShadowStatus_Draw()", "kToyViewNames[ToyShadowsMode]);"), "if (ShadowsDepthGap || ShadowsEvenGap || ShadowBiasSteps)"), "not only when it's on");
	Check(Has(render, "{ &ShadowsEvenGap, ShadowsEvenGap },"), "a network game puts it back");
}

int main()
{
	CheckTheBestPicture();
	CheckTheReachOfALight();
	CheckTheLimits();
	CheckTheTotalsPasses();
	CheckTheCardThatCantAdd();
	CheckTheLog();
	CheckTheEvenGapWord();

	printf("%s\n", checksFailed ? "FAILED" : "all passed");

	return checksFailed ? 1 : 0;
}
