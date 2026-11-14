// Tests for the marine's shoulder lamp. The first half tries shoulder_lamp.cpp, which knows nothing about the game. The second half reads the game's source, for the things that only mean something where they join the game: who can have a lamp, which
// key it's on, and how an old profile gets that key. Written 2026-11-05 by Melanikus for the AvP port.
// Modified 2026-11-08 by Melanikus for the AvP port:
//   a missing marker is a failed check of its own, and the network check's second half reads the part of the drawing it names, which
//   before was always empty, so that half could never fail (SOURCE-SLICE-1)
// Modified 2026-11-08 by Melanikus for the AvP port:
//   its Between is the shared one in source_slice.h, so a marker that isn't there is a failed check of its own (SOURCE-SLICE-1, #128)
#include "shoulder_lamp.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <math.h>
#include <sstream>
#include <stdio.h>
#include <string.h>
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

static bool Has(const std::string &text, const char *what)
{
	return text.find(what) != std::string::npos;
}

static int CountOf(const std::string &text, const char *what)
{
	int count = 0;

	for (std::string::size_type at = text.find(what); at != std::string::npos; at = text.find(what, at + 1))
	{
		count++;
	}

	return count;
}

// The text from one marker up to the next (source_slice.h). A marker that isn't there is a failed check of its own, named by the marker.
static std::string Between(const std::string &text, const char *from, const char *to)
{
	return SourceSlice(text, from, to, kSliceEndFromTheStart, [](const std::string &why) { Check(false, why.c_str()); });
}

static bool ComesBefore(const std::string &text, const char *first, const char *second)
{
	std::string::size_type const a = text.find(first), b = text.find(second);

	return a != std::string::npos && b != std::string::npos && a < b;
}

static void TestSwitching()
{
	Check(!ShoulderLamp_IsOn(), "it starts off");

	ShoulderLamp_Toggle();
	Check(ShoulderLamp_IsOn(), "one press turns it on");

	ShoulderLamp_Toggle();
	Check(!ShoulderLamp_IsOn(), "and the next turns it off");

	ShoulderLamp_Toggle();
	ShoulderLamp_TurnOff();
	Check(!ShoulderLamp_IsOn(), "and it can be turned off from on");

	ShoulderLamp_TurnOff();
	Check(!ShoulderLamp_IsOn(), "and from off it stays off");
}

static void TestAsking()
{
	long const before = ShoulderLamp_FramesDrawn();

	ShoulderLamp_TurnOff();
	ShoulderLamp_AskForThisFrame(true);
	Check(!ShoulderLamp_IsAskedForThisFrame() && !ShoulderLamp_CouldBeDrawn(), "off, it isn't asked for, and nothing extra is drawn for it");
	Check(!ShoulderLamp_TakeThisFramesAnswer(), "off, it isn't drawn");

	ShoulderLamp_Toggle();
	ShoulderLamp_AskForThisFrame(false);
	Check(!ShoulderLamp_CouldBeDrawn(), "on, but for someone who can't have one, nothing extra is drawn for it");
	Check(!ShoulderLamp_TakeThisFramesAnswer(), "on, but for someone who can't have one, it isn't drawn");
	Check(ShoulderLamp_FramesDrawn() == before, "and neither of those counts as a frame drawn");

	ShoulderLamp_AskForThisFrame(true);
	Check(ShoulderLamp_IsAskedForThisFrame(), "on, for a marine, it's asked for");
	Check(ShoulderLamp_CouldBeDrawn(), "and it could be drawn on the coming frame");
	Check(ShoulderLamp_IsAskedForThisFrame(), "and looking takes nothing away");
	Check(ShoulderLamp_TakeThisFramesAnswer(), "and it's drawn");
	Check(!ShoulderLamp_IsAskedForThisFrame(), "and after that it isn't asked for any more");
	Check(ShoulderLamp_FramesDrawn() == before + 1, "and that's counted");
	Check(!ShoulderLamp_TakeThisFramesAnswer(), "but only once for each time it's asked, so that a menu's frame can't draw it");
	Check(ShoulderLamp_FramesDrawn() == before + 1, "and that isn't counted");

	ShoulderLamp_AskForThisFrame(true);
	ShoulderLamp_TurnOff();
	ShoulderLamp_AskForThisFrame(false);
	Check(!ShoulderLamp_TakeThisFramesAnswer(), "the last thing asked is what counts");
}

static void TestRealLight()
{
	// Nothing in this build asks for the real light: the setting that does comes with the lamp work. So it's always the painted version.
	ShoulderLamp_TurnOff();
	ShoulderLamp_Toggle();
	ShoulderLamp_AskForThisFrame(true);
	Check(!ShoulderLamp_CanBeALight(), "on, for a marine, it's still the painted version, because nothing asks for the real light");

	float const *const place = ShoulderLamp_PlaceFromEyes();

	Check(place[0] == (float)kShoulderLampAcross && place[1] == (float)kShoulderLampUp && place[2] == (float)kShoulderLampAhead, "it starts at his shoulder");

	ShoulderLamp_TurnOff();
}

static void TestFlicker()
{
	float lowest = 2.0f, highest = -1.0f, biggestStep = 0.0f, last = ShoulderLamp_Flicker(0.0);
	double total = 0.0;
	int const samples = 10 * 60 * 60;

	for (int i = 1; i <= samples; i++)
	{
		float const now = ShoulderLamp_Flicker((double)i / 60.0);
		float const step = fabsf(now - last);

		lowest = now < lowest ? now : lowest;
		highest = now > highest ? now : highest;
		biggestStep = step > biggestStep ? step : biggestStep;
		total += now;
		last = now;
	}

	Check(lowest >= 0.64f, "over ten minutes it stays above about two thirds: " + std::to_string(lowest));
	Check(highest <= 1.0f, "and it never goes past full: " + std::to_string(highest));
	Check(highest - lowest > 0.2f, "and it really does move");
	Check(biggestStep < 0.07f, "and only ever smoothly from one frame to the next at 60 a second, so it can't strobe: " + std::to_string(biggestStep));
	Check(total / samples > 0.85, "and it's mostly near full, with brief dips: " + std::to_string(total / samples));
	Check(ShoulderLamp_Flicker(123.0) == ShoulderLamp_Flicker(123.0), "and it only depends on the time");
}

static void TestKeysFromProfiles()
{
	unsigned char const escape = 1, g = 34, noKey = 255, other = 77;

	Check(ShoulderLamp_KeyFromProfile(escape, escape, g, false, noKey) == g, "a profile from before the lamp gets the default key");
	Check(ShoulderLamp_KeyFromProfile(escape, escape, g, true, noKey) == noKey, "unless the player uses that key for something else, and then no key");
	Check(ShoulderLamp_KeyFromProfile(other, escape, g, false, noKey) == other, "a key the player chose stays");
	Check(ShoulderLamp_KeyFromProfile(other, escape, g, true, noKey) == other, "whatever else is going on");
	Check(ShoulderLamp_KeyFromProfile(noKey, escape, g, false, noKey) == noKey, "and a key the player cleared stays cleared");
	Check(ShoulderLamp_KeyFromProfile(g, escape, g, true, noKey) == g, "and G stays G if that's what the profile says");
}

// The view, turned this far to the right from straight ahead, and tipped this far up, in degrees
static void TellTheLamp(double turned, double tipped, double moved, double seconds)
{
	double const toRadians = 3.141592653589793 / 180.0;
	double const yaw = turned * toRadians, pitch = tipped * toRadians;
	double const look[3] = { sin(yaw) * cos(pitch), -sin(pitch), cos(yaw) * cos(pitch) };
	double const right[3] = { cos(yaw), 0.0, -sin(yaw) };
	double const down[3] = { sin(yaw) * sin(pitch), cos(pitch), cos(yaw) * sin(pitch) };

	ShoulderLamp_FollowTheView(look, right, down, moved, seconds);
}

static void SettleTheLamp(double turned, double tipped)
{
	for (int i = 0; i < 600; i++)
	{
		TellTheLamp(turned, tipped, 0.0, 1.0 / 60.0);
	}
}

static void TestTheRobotArm()
{
	TellTheLamp(0.0, 0.0, 0.0, 1.0 / 60.0);
	Check(ShoulderLamp_AimAcross() == 0.0f && ShoulderLamp_AimDown() == 0.0f, "the first frame there's nothing to catch up with");

	// That first frame is the only one like it in this program, so it has to be looked at here. Anything it left behind would only show on a turn far enough to reach the limit.
	TellTheLamp(40.0, 0.0, 0.0, 1.0 / 60.0);

	float const afterTheVeryFirstFrame = ShoulderLamp_AimAcross();

	SettleTheLamp(0.0, 0.0);
	TellTheLamp(40.0, 0.0, 0.0, 1.0 / 60.0);
	Check(fabsf(ShoulderLamp_AimAcross() - afterTheVeryFirstFrame) < 0.001f, "and it leaves nothing behind: the same turn gives the same aim as it does from a lamp that's settled, " + std::to_string(afterTheVeryFirstFrame) + " and " + std::to_string(ShoulderLamp_AimAcross()));

	SettleTheLamp(0.0, 0.0);
	Check(fabsf(ShoulderLamp_AimAcross()) < 0.0001f && fabsf(ShoulderLamp_AimDown()) < 0.0001f, "standing still, it's centred on what he's looking at");

	// A sudden turn of 20 degrees to the right, at three frame rates. tan 20 degrees is 0.364.
	int const rates[3] = { 30, 60, 240 };
	float atAQuarter[3], atAHalf[3];

	for (int rate = 0; rate < 3; rate++)
	{
		double const frame = 1.0 / rates[rate];
		std::string const name = std::to_string(rates[rate]) + " frames a second: ";

		SettleTheLamp(0.0, 0.0);
		TellTheLamp(20.0, 0.0, 0.0, frame);

		float const first = ShoulderLamp_AimAcross();

		Check(first < -0.3f && first > -0.37f, name + "turning right leaves it behind, to the left, by what he turned: " + std::to_string(first));
		Check(fabsf(ShoulderLamp_AimDown()) < 0.001f, name + "and not up or down");

		float last = first;
		bool wentBack = false, wentPast = false;
		int frames = (int)(1.0 / frame + 0.5);

		atAQuarter[rate] = atAHalf[rate] = 0.0f;

		for (int i = 1; i < frames; i++)
		{
			TellTheLamp(20.0, 0.0, 0.0, frame);

			float const now = ShoulderLamp_AimAcross();

			wentBack = wentBack || now < last - 0.0001f;
			wentPast = wentPast || now > 0.0001f;
			last = now;

			if (i + 1 == frames / 4)
			{
				atAQuarter[rate] = now;
			}

			if (i + 1 == frames / 2)
			{
				atAHalf[rate] = now;
			}
		}

		Check(fabsf(atAHalf[rate]) < fabsf(first) * 0.25f, name + "it's on its way, a good part of the way there after half a second: " + std::to_string(atAHalf[rate]));
		Check(fabsf(atAQuarter[rate]) > fabsf(first) * 0.4f, name + "and it isn't there at once, after a quarter of a second: " + std::to_string(atAQuarter[rate]));
		Check(!wentBack, name + "it only ever closes in");
		Check(!wentPast, name + "and it doesn't go past where he's looking");
		Check(fabsf(last) < fabsf(first) * 0.01f, name + "and within a second it's within 1% of there: " + std::to_string(last));
	}

	Check(fabsf(atAQuarter[0] - atAQuarter[2]) < 0.02f, "the same turn gets as far at 30 frames a second as at 240, after a quarter of a second");
	Check(fabsf(atAHalf[0] - atAHalf[2]) < 0.01f, "and after a half");

	// It starts gently: at first it hardly moves, and then it picks up
	SettleTheLamp(0.0, 0.0);
	TellTheLamp(20.0, 0.0, 0.0, 1.0 / 240.0);

	float const startedAt = ShoulderLamp_AimAcross();

	TellTheLamp(20.0, 0.0, 0.0, 1.0 / 240.0);

	float const firstMove = ShoulderLamp_AimAcross() - startedAt;

	for (int i = 0; i < 24; i++)
	{
		TellTheLamp(20.0, 0.0, 0.0, 1.0 / 240.0);
	}

	float const before = ShoulderLamp_AimAcross();

	TellTheLamp(20.0, 0.0, 0.0, 1.0 / 240.0);

	float const laterMove = ShoulderLamp_AimAcross() - before;

	Check(firstMove < laterMove * 0.5f, "it starts gently, and doesn't snap into moving: " + std::to_string(firstMove) + " and then " + std::to_string(laterMove));

	// Looking up
	SettleTheLamp(0.0, 0.0);
	TellTheLamp(0.0, 15.0, 0.0, 1.0 / 60.0);
	Check(ShoulderLamp_AimDown() > 0.2f && fabsf(ShoulderLamp_AimAcross()) < 0.001f, "looking up leaves it behind, lower on the screen, and not to one side: " + std::to_string(ShoulderLamp_AimDown()));

	// A spin all the way round, in a sixth of a turn a second more than anyone would. It stays at its limit, trailing him, and then catches up.
	SettleTheLamp(0.0, 0.0);

	float furthest = 0.0f;

	for (int i = 1; i <= 240; i++)
	{
		TellTheLamp(i * 6.0, 0.0, 0.0, 1.0 / 60.0);
		furthest = fabsf(ShoulderLamp_AimAcross()) > furthest ? fabsf(ShoulderLamp_AimAcross()) : furthest;
	}

	Check(furthest > 0.4f && furthest < 0.55f, "spinning fast, it only gets so far behind, and stays on the screen: " + std::to_string(furthest));
	Check(ShoulderLamp_AimAcross() < 0.0f, "and it's still trailing behind him");

	float const atTheLimit = fabsf(ShoulderLamp_AimAcross());
	float afterATenthOfASecond = 0.0f;

	for (int i = 0; i < 60; i++)
	{
		TellTheLamp(240.0 * 6.0, 0.0, 0.0, 1.0 / 60.0);

		if (i == 5)
		{
			afterATenthOfASecond = fabsf(ShoulderLamp_AimAcross());
		}
	}

	Check(afterATenthOfASecond > atTheLimit * 0.7f, "being held at its limit stores up no speed, so when he stops it starts back gently, and doesn't leap: " + std::to_string(atTheLimit) + " then " + std::to_string(afterATenthOfASecond));

	Check(fabsf(ShoulderLamp_AimAcross()) < 0.01f, "and within a second of stopping it has caught up: " + std::to_string(ShoulderLamp_AimAcross()));

	// Being put somewhere new, there's no chase
	SettleTheLamp(0.0, 0.0);
	TellTheLamp(30.0, 0.0, (double)kShoulderLampMostStep + 1.0, 1.0 / 60.0);
	Check(ShoulderLamp_AimAcross() == 0.0f, "moved further than anyone can walk in a frame, it's simply there");

	SettleTheLamp(0.0, 0.0);
	TellTheLamp(30.0, 0.0, (double)kShoulderLampMostStep, 1.0 / 60.0);
	Check(ShoulderLamp_AimAcross() < -0.1f, "but no further than that, and he's just walking");

	SettleTheLamp(0.0, 0.0);
	TellTheLamp(30.0, 0.0, 0.0, kShoulderLampLongFrame + 0.01);
	Check(ShoulderLamp_AimAcross() == 0.0f, "after a long frame, like a level loading, it's simply there");

	SettleTheLamp(0.0, 0.0);
	TellTheLamp(180.0, 0.0, 0.0, 1.0 / 60.0);
	Check(ShoulderLamp_AimAcross() == 0.0f, "the view turning 180 degrees in one frame has been put there, and there's no chase");

	SettleTheLamp(0.0, 0.0);
	TellTheLamp(90.0, 0.0, 0.0, 1.0 / 60.0);
	Check(ShoulderLamp_AimAcross() < -0.3f, "but a quarter turn in one frame is only someone turning fast");

	// A frame that took no time says nothing
	SettleTheLamp(0.0, 0.0);
	TellTheLamp(20.0, 0.0, 0.0, 1.0 / 60.0);

	float const across = ShoulderLamp_AimAcross();

	TellTheLamp(40.0, 0.0, 0.0, 0.0);
	Check(ShoulderLamp_AimAcross() == across, "a frame that took no time says nothing");

	SettleTheLamp(0.0, 0.0);
}

static bool FollowReads(const char *words, double wanted)
{
	double seconds = -1.0;

	return ShoulderLamp_FollowSecondsFromWords(words, (unsigned int)strlen(words), &seconds) && fabs(seconds - wanted) < 0.000001;
}

static bool FollowRefuses(const char *words)
{
	double seconds = -1.0;

	return !ShoulderLamp_FollowSecondsFromWords(words, (unsigned int)strlen(words), &seconds) && seconds == -1.0;
}

// How far behind the lamp still is, this many seconds after a sudden turn of 20 degrees
static float BehindAfter(double followSeconds, double seconds)
{
	double const frame = 1.0 / 240.0;

	ShoulderLamp_SetFollowSeconds(followSeconds);
	SettleTheLamp(0.0, 0.0);

	for (int i = 0; i < (int)(seconds / frame + 0.5); i++)
	{
		TellTheLamp(20.0, 0.0, 0.0, frame);
	}

	return fabsf(ShoulderLamp_AimAcross());
}

static void TestLampFollow()
{
	Check(ShoulderLamp_FollowSeconds() == 1.0 && kShoulderLampFollowSeconds == 1.0, "the lamp starts by taking a second, so nobody's look changes unless they ask");

	// the number typed
	Check(FollowReads("0", 0.0), "LAMPFOLLOW reads 0");
	Check(FollowReads("0.5", 0.5), "and 0.5");
	Check(FollowReads(".5", 0.5), "and .5");
	Check(FollowReads("1.25", 1.25), "and more than one place");
	Check(FollowReads("5", 5.0), "and the most it allows");
	Check(FollowReads("5.0", 5.0), "and that, written another way");
	Check(FollowReads("5.", 5.0), "and with nothing after the dot");
	Check(FollowReads("00.5", 0.5), "and zeros in front, which change nothing");

	char const *const refused[] = { "", ".", "..", "5.01", "6", "10", "-1", "+1", "1e9", "inf", "nan", "0x1", "0,5", "1.2.3", "0.5s", " 0.5", "0.5 ", "L", "123456789", "0.0000001" };

	for (char const *const words : refused)
	{
		Check(FollowRefuses(words), std::string("[") + words + "] is refused, and what was there is left alone");
	}

	double left = -1.0;
	Check(!ShoulderLamp_FollowSecondsFromWords(0, 3, &left) && left == -1.0, "and so are no words at all");
	Check(ShoulderLamp_FollowSecondsFromWords("0.5xxx", 3, &left) && left == 0.5, "only as much of the words as it's told about is read");

	// what it does to the arm
	float const atOneSecond = BehindAfter(1.0, 0.5);
	float const atHalfASecond = BehindAfter(0.5, 0.25);

	Check(fabsf(atOneSecond - atHalfASecond) < 0.001f, "taking half the time means being at the same place in half the time: " + std::to_string(atOneSecond) + " and " + std::to_string(atHalfASecond));
	Check(BehindAfter(0.5, 0.5) < 0.004f, "so 0.5 has caught up within half a second");
	Check(BehindAfter(5.0, 1.0) > 0.1f, "and 5 is still well behind after one");

	// stuck to the view
	ShoulderLamp_SetFollowSeconds(1.0);
	SettleTheLamp(0.0, 0.0);
	TellTheLamp(20.0, 0.0, 0.0, 1.0 / 60.0);
	Check(ShoulderLamp_AimAcross() < -0.3f, "a lamp that's behind");

	ShoulderLamp_SetFollowSeconds(0.0);
	TellTheLamp(20.0, 0.0, 0.0, 1.0 / 60.0);
	Check(ShoulderLamp_AimAcross() == 0.0f && ShoulderLamp_AimDown() == 0.0f, "0 sticks it to the view at once");

	TellTheLamp(50.0, 10.0, 0.0, 1.0 / 60.0);
	Check(ShoulderLamp_AimAcross() == 0.0f && ShoulderLamp_AimDown() == 0.0f, "and turning leaves it there, with no sum that divides by 0");

	ShoulderLamp_SetFollowSeconds(1.0);
	TellTheLamp(50.0, 10.0, 0.0, 1.0 / 60.0);
	Check(fabsf(ShoulderLamp_AimAcross()) < 0.001f && fabsf(ShoulderLamp_AimDown()) < 0.001f, "and coming back from it doesn't jump, because nothing was kept while it was stuck");

	SettleTheLamp(0.0, 0.0);

	// the game
	std::string const commands = ReadSourceFile("3dc/avp/game_commands.cpp");
	std::string const render = ReadSourceFile("3dc/d3d_render.cpp");
	std::string const command = Between(render, "void LampFollowCommand(char *words)", "\n}");

	Check(Has(commands, "\"LAMPFOLLOW\",") && Has(commands, "LampFollowCommand\n\t);"), "the console knows the word");
	Check(ComesBefore(commands, "\"LAMPFOLLOW\",", "bool IsACheat = true;"), "and it's with the player's settings, not the cheats");
	Check(!Has(command, "DevMode") && !Has(command, "I_No_Network"), "anyone can use it, in any game");
	Check(Has(command, "if (length > 0 && !ShoulderLamp_FollowSecondsFromWords(start, (unsigned int)length, &seconds))"), "the words go through the strict reader");
	Check(ComesBefore(command, "LAMPFOLLOW TAKES SECONDS FROM 0 TO 5, LIKE 0.5.", "ShoulderLamp_SetFollowSeconds(seconds);") && Has(command, "return;\n\t}\n\n\tShoulderLamp_SetFollowSeconds(seconds);"), "and a refusal says why, and changes nothing");
	Check(Has(command, "if (seconds > 0.0)") && Has(command, "LAMP IS STUCK TO THE VIEW"), "and it says what it is now");
	Check(kShoulderLampMostFollowSeconds == 5.0, "the limit it says is the limit there is");

	ShoulderLamp_SetFollowSeconds(kShoulderLampFollowSeconds);
}

static bool SizeReads(const char *words, double wanted)
{
	double size = -1.0;

	return ShoulderLamp_SpotSizeFromWords(words, (unsigned int)strlen(words), &size) && fabs(size - wanted) < 0.000001;
}

static bool SizeRefuses(const char *words)
{
	double size = -1.0;

	return !ShoulderLamp_SpotSizeFromWords(words, (unsigned int)strlen(words), &size) && size == -1.0;
}

static bool IntensityReads(const char *words, double wanted)
{
	double intensity = -1.0;

	return ShoulderLamp_IntensityFromWords(words, (unsigned int)strlen(words), &intensity) && fabs(intensity - wanted) < 0.000001;
}

static bool IntensityRefuses(const char *words)
{
	double intensity = -1.0;

	return !ShoulderLamp_IntensityFromWords(words, (unsigned int)strlen(words), &intensity) && intensity == -1.0;
}

static void TestTheSpot()
{
	Check(ShoulderLamp_SpotSize() == 0.21 && kShoulderLampSpotSize == 0.21, "the spot starts as built");
	Check(ShoulderLamp_Intensity() == 1.0 && kShoulderLampIntensity == 1.0, "and so does how strong it is");

	float numbers[4];

	ShoulderLamp_SpotNumbers(numbers);
	Check(fabs(numbers[0] - 0.21f) < 0.00001f && numbers[1] == 1.0f && numbers[2] == 1.0f && numbers[3] == 0.0f, "as built, what the shader's given changes nothing");

	// the numbers typed
	Check(SizeReads("0.05", 0.05), "LAMPSPOTSIZE reads the smallest it allows");
	Check(SizeReads("0.35", 0.35), "and the biggest");
	Check(SizeReads(".2", 0.2), "and with nothing before the dot");
	Check(IntensityReads("0.25", 0.25), "LAMPINTENSITY reads the least it allows");
	Check(IntensityReads("2", 2.0), "and the most");
	Check(IntensityReads("1.5", 1.5), "and between");

	char const *const notSizes[] = { "", ".", "0.04", "0.36", "0", "1", "-0.2", "+0.2", "1e9", "inf", "nan", "0x1", "0,21", "0.2.1", "0.20000000000", "big" };

	for (char const *const words : notSizes)
	{
		Check(SizeRefuses(words), std::string("[") + words + "] isn't a size, and what was there is left alone");
	}

	char const *const notIntensities[] = { "", ".", "0.24", "2.01", "0", "3", "-1", "+1", "1e9", "inf", "nan", "0x1", "0,5", "1.2.3", "1.00000000000", "lots" };

	for (char const *const words : notIntensities)
	{
		Check(IntensityRefuses(words), std::string("[") + words + "] isn't an intensity, and what was there is left alone");
	}

	double left = -1.0;

	Check(!ShoulderLamp_SpotSizeFromWords(0, 3, &left) && !ShoulderLamp_IntensityFromWords(0, 3, &left) && left == -1.0, "and so are no words at all");

	// what they do
	ShoulderLamp_SetSpotSize(0.35);
	ShoulderLamp_SetIntensity(2.0);
	ShoulderLamp_SpotNumbers(numbers);
	Check(fabs(numbers[0] - 0.35f) < 0.00001f, "a bigger spot tells the shader its size");
	Check(fabs(numbers[1] - 0.6f) < 0.00001f, "and how much nearer every distance now counts");
	Check(numbers[2] == 2.0f, "and how strong it is");
	Check(ShoulderLamp_SpotSize() == 0.35 && ShoulderLamp_Intensity() == 2.0, "and it says what it is now");

	// Past its limits it stops there, so that nothing can divide by nothing, or make a floodlight
	ShoulderLamp_SetSpotSize(0.0);
	Check(ShoulderLamp_SpotSize() == kShoulderLampFewestSpotSize, "no size at all comes back to the smallest");
	ShoulderLamp_SetSpotSize(100.0);
	Check(ShoulderLamp_SpotSize() == kShoulderLampMostSpotSize, "a huge one to the biggest");
	ShoulderLamp_SetIntensity(0.0);
	Check(ShoulderLamp_Intensity() == kShoulderLampFewestIntensity, "no strength at all to the least");
	ShoulderLamp_SetIntensity(100.0);
	Check(ShoulderLamp_Intensity() == kShoulderLampMostIntensity, "a huge one to the most");

	// A network game puts them back
	Check(ShoulderLamp_PutSpotBack(), "putting them back says if anything changed");
	Check(ShoulderLamp_SpotSize() == kShoulderLampSpotSize && ShoulderLamp_Intensity() == kShoulderLampIntensity, "and they're as built");
	Check(!ShoulderLamp_PutSpotBack(), "and if nothing has changed, it says so, so that the log only says it once");
	ShoulderLamp_SetSpotSize(0.3);
	Check(ShoulderLamp_PutSpotBack() && ShoulderLamp_SpotSize() == kShoulderLampSpotSize, "the size alone counts as a change");
	ShoulderLamp_SetIntensity(1.5);
	Check(ShoulderLamp_PutSpotBack() && ShoulderLamp_Intensity() == kShoulderLampIntensity, "and so does how strong it is alone");

	// the game, which only a run can really try
	std::string const commands = ReadSourceFile("3dc/avp/game_commands.cpp");
	std::string const render = ReadSourceFile("3dc/d3d_render.cpp");
	std::string const shader = ReadSourceFile("shaders/shoulderLamp.psh");
	std::string const sizeCommand = Between(render, "void LampSpotSizeCommand(char *words)", "\n}");
	std::string const intensityCommand = Between(render, "void LampIntensityCommand(char *words)", "\n}");
	std::string const followCommand = Between(render, "void LampFollowCommand(char *words)", "\n}");
	std::string const guard = Between(render, "static void KeepDeveloperToolsOutOfNetworkGames()", "\n}");
	std::string const refusing = Between(render, "static bool LampStaysAsBuiltHere(const char *why, size_t length)", "\n}");
	std::string const drawing = Between(render, "static void DrawShoulderLamp()\n{", "\n}\n");

	Check(Has(commands, "\"LAMPSPOTSIZE\",") && Has(commands, "LampSpotSizeCommand\n\t);"), "the console knows LAMPSPOTSIZE");
	Check(Has(commands, "\"LAMPINTENSITY\",") && Has(commands, "LampIntensityCommand\n\t);"), "and LAMPINTENSITY");
	Check(ComesBefore(commands, "\"LAMPSPOTSIZE\",", "bool IsACheat = true;") && ComesBefore(commands, "\"LAMPINTENSITY\",", "bool IsACheat = true;"), "and they're with the player's settings, not the cheats");

	Check(ComesBefore(sizeCommand, "LampStaysAsBuiltHere(", "ShoulderLamp_SetSpotSize(size);"), "a network game refuses a size before anything changes");
	Check(ComesBefore(intensityCommand, "LampStaysAsBuiltHere(", "ShoulderLamp_SetIntensity(intensity);"), "and a strength");
	Check(Has(sizeCommand, "LAMPSPOTSIZE IS OFF IN A NETWORK GAME: A BIGGER LAMP WOULD BE CHEATING"), "and says why");
	Check(Has(intensityCommand, "LAMPINTENSITY IS OFF IN A NETWORK GAME: A BRIGHTER LAMP WOULD BE CHEATING"), "both of them");
	Check(Has(refusing, "AvP.Network == I_No_Network || length == 0"), "but not playing alone, and on its own it can still say what it is");
	Check(!Has(followCommand, "LampStaysAsBuiltHere") && !Has(followCommand, "I_No_Network"), "LAMPFOLLOW stays allowed, because it only moves the lamp");
	Check(Has(guard, "if (ShoulderLamp_PutSpotBack())"), "a network game puts them back, whatever was set before it started");
	Check(ComesBefore(guard, "ShoulderLamp_PutSpotBack()", "if (!viewShowsTooMuch"), "before anything else there can end it early");

	Check(Has(sizeCommand, "if (length > 0 && !ShoulderLamp_SpotSizeFromWords(start, (unsigned int)length, &size))"), "the words go through the strict reader");
	Check(Has(intensityCommand, "if (length > 0 && !ShoulderLamp_IntensityFromWords(start, (unsigned int)length, &intensity))"), "both times");
	Check(Has(sizeCommand, "LAMPSPOTSIZE TAKES A NUMBER FROM 0.05 TO 0.35") && kShoulderLampFewestSpotSize == 0.05 && kShoulderLampMostSpotSize == 0.35, "the limits a size says are the limits there are");
	Check(Has(intensityCommand, "LAMPINTENSITY TAKES A NUMBER FROM 0.25 TO 2") && kShoulderLampFewestIntensity == 0.25 && kShoulderLampMostIntensity == 2.0, "and a strength");
	Check(Has(sizeCommand, "LIKE 0.21\");\n\t\treturn;") && Has(intensityCommand, "LIKE 1\");\n\t\treturn;"), "and something else is refused, not used");

	Check(Has(drawing, "ShoulderLamp_SpotNumbers(spot);\n\tdevice->SetPixelShaderConstantF(2, spot, 1);"), "the shader's told");
	Check(Has(shader, "float4 Spot : register(c2);"), "in the register it reads");
	Check(Has(shader, "static const float kSizeAtFarEnd = 0.21f;"), "and its size as built is the same number here");
	Check(Has(shader, "float fromAim = distance(here, aim) * Spot.y;"), "the spot grows with the size");
	Check(Has(shader, "lerp(kSizeAtShoulder, Spot.x, along)"), "and so does the cone where it arrives, so that they still meet");
	Check(Has(shader, "kLightInTheMiddle) * Spot.z;"), "how strong it is goes on the spot");
	Check(!Has(shader, "cone * Spot") && !Has(shader, "haze * Spot"), "and not on the haze, or the cone could turn into night vision");

	// There's no number in the shader for whether there are pictures any more: an empty stage reads as black. Swing.zw is where the cone starts now
	// (LAMPSHOULDER, R-LAMP-1), and that's the only thing it's used for. (Modified 2026-11-14 by Melanikus (Dietrich) for the AvP port.)
	Check(CountOf(shader, "Swing.z") == CountOf(shader, "Swing.zw") && CountOf(shader, "= Swing.zw;") == 1 && Has(shader, "float2 start = Swing.zw;"),
		"the shader has nothing left that asks if there are pictures: Swing's last two are only where the cone starts");
	Check(Has(drawing, "else\n\t{\n\t\t// An empty stage reads as black") && Has(drawing, "device->SetTexture(3, NULL);\n\t\tdevice->SetTexture(4, NULL);\n\t}\n\n\tChangeTranslucencyMode(TRANSLUCENCY_GLOWING);"), "so with none from this frame, the stages are made empty before it draws");

	ShoulderLamp_PutSpotBack();
}

// LAMPLOOK (shoulder_lamp.h). Added 2026-11-11 by Melanikus (Third) for the AvP port: how the player's own lamp looks when it could be a real
// light. As built it's the real light alone; 1 puts the painted spot over it; 2 keeps it painted.
static void TestTheLook()
{
	Check(ShoulderLamp_Look() == kShoulderLampLookAsBuilt && kShoulderLampLookAsBuilt == kShoulderLampLookBoth,
		"the lamp's look starts as built: the real light with the painted spot over it, as the owner asked");

	// What can be typed: one of the three, and nothing else
	int look = -1;
	Check(ShoulderLamp_LookFromWords("0", 1, &look) && look == 0 && ShoulderLamp_LookFromWords("1", 1, &look) && look == 1 && ShoulderLamp_LookFromWords("2", 1, &look) && look == 2
		&& ShoulderLamp_LookFromWords("3", 1, &look) && look == 3, "LAMPLOOK reads 0, 1, 2 and 3");
	char const *const notLooks[] = { "", "4", "-1", "01", "1.0", "x", "12", " 1" };

	for (char const *const words : notLooks)
	{
		look = 7;
		Check(!ShoulderLamp_LookFromWords(words, (unsigned int)strlen(words), &look) && look == 7, std::string("[") + words + "] isn't a look, and what was there is left alone");
	}

	Check(!ShoulderLamp_LookFromWords(0, 1, &look) && look == 7, "and no words at all aren't either");
	ShoulderLamp_SetLook(5);
	Check(ShoulderLamp_Look() == kShoulderLampLookAsBuilt, "a number that isn't one of the four changes nothing");

	// What it does to the real light: 2 keeps it painted, as "Lamps cast shadows" off does; 1 and 0 leave it a real light
	ShoulderLamp_TurnOff();
	ShoulderLamp_Toggle();
	ShoulderLamp_AskForThisFrame(true);
	ShoulderLamp_SetRealLightWanted(true);
	bool const asBuilt = ShoulderLamp_CanBeALight();
	ShoulderLamp_SetLook(kShoulderLampLookRealOnly);
	bool const realOnly = ShoulderLamp_CanBeALight() && ShoulderLamp_Look() == 0;
	ShoulderLamp_SetLook(kShoulderLampLookBoth);
	bool const both = ShoulderLamp_CanBeALight();
	ShoulderLamp_SetLook(kShoulderLampLookPainted);
	bool const painted = ShoulderLamp_CanBeALight();
	ShoulderLamp_SetLook(kShoulderLampLookBothFull);
	bool const bothFull = ShoulderLamp_CanBeALight();
	Check(asBuilt && realOnly && both && bothFull && !painted, "as built, and with the painted lamp over it either way, the lamp is a real light; painted only, it never is");

	// A network game puts it back with the spot, and that counts as a change
	Check(ShoulderLamp_PutSpotBack() && ShoulderLamp_Look() == kShoulderLampLookAsBuilt && ShoulderLamp_CanBeALight(), "putting the spot back puts the look back too, and says so");
	Check(!ShoulderLamp_PutSpotBack(), "and as built, nothing has changed");
	ShoulderLamp_SetRealLightWanted(false);
	ShoulderLamp_TurnOff();

	// The game, which only a run can really try
	std::string const commands = ReadSourceFile("3dc/avp/game_commands.cpp");
	std::string const render = ReadSourceFile("3dc/d3d_render.cpp");
	std::string const lookCommand = Between(render, "void LampLookCommand(char *words)", "\n}");
	std::string const drawing = Between(render, "static void DrawShoulderLamp()\n{", "\n}\n");
	Check(Has(commands, "\"LAMPLOOK\",") && Has(commands, "LampLookCommand\n\t);") && ComesBefore(commands, "\"LAMPLOOK\",", "bool IsACheat = true;"),
		"the console knows LAMPLOOK, with the player's settings, not the cheats");
	Check(ComesBefore(lookCommand, "LampStaysAsBuiltHere(", "ShoulderLamp_SetLook(look);") && Has(lookCommand, "LAMPLOOK IS OFF IN A NETWORK GAME"),
		"a network game refuses a look before anything changes, and says why");
	Check(Has(lookCommand, "if (length > 0 && !ShoulderLamp_LookFromWords(start, (unsigned int)length, &look))"), "the words go through the strict reader");
	Check(Has(drawing, "bool const blendedOver = isARealLight && ShoulderLamp_Look() == kShoulderLampLookBoth;")
		&& Has(drawing, "bool const fullOver = isARealLight && ShoulderLamp_Look() == kShoulderLampLookBothFull;")
		&& Has(drawing, "bool const paintedToo = blendedOver || fullOver;")
		&& Has(drawing, "\t\tif (!paintedToo)\n\t\t{\n\t\t\treturn;\n\t\t}"),
		"as a real light the painted lamp is drawn only when LAMPLOOK asks for it over the light");
	Check(Has(drawing, "float const brighten[4] = { blendedOver ? strength * kShoulderLampOverARealLight : strength, 0.0f, 0.0f, wide };")
		&& Has(drawing, "\tif (!blendedOver)\n\t{\n\t\tdevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, corners, sizeof(ORTHOVERTEX));\n\t}"),
		"blended over the real light it brightens once, at kShoulderLampOverARealLight, so the light isn't doubled");
	Check(Has(drawing, "bool const onWalls = (!isARealLight || fullOver) && surfacePicturesAreFromThisFrame && SurfacePictures_Colour() && toyDepthTexture;"),
		"at full over the real light it adds the walls' own colour too, as on its own; blended it doesn't");
	Check(kShoulderLampOverARealLight > 0.0f && kShoulderLampOverARealLight <= 0.5f, "and at most half as strongly as alone");
}

static void TestNetworkKeepsTheSpot()
{
	std::string const render = ReadSourceFile("3dc/d3d_render.cpp");
	std::string const drawing = Between(render, "static void DrawShoulderLamp()\n{", "template <class T> static void ToyRelease");
	std::string const shadowsDrawing = Between(render, "static void ToyShadows_Draw()\n{", "// F8 saves the next frames");

	Check(ComesBefore(shadowsDrawing, "if (!ToyDepthBufferHasStencil())", "KeepDeveloperToolsOutOfNetworkGames();") && Has(Between(shadowsDrawing, "if (!ToyDepthBufferHasStencil())", "KeepDeveloperToolsOutOfNetworkGames();"), "return;"), "a card with no stencil leaves before the network game's rules, which is why the drawing has to ask too");
	Check(Has(drawing, "if (AvP.Network != I_No_Network && ShoulderLamp_PutSpotBack())"), "the drawing puts the spot back in a network game");
	Check(ComesBefore(drawing, "ShoulderLamp_PutSpotBack()", "ShoulderLamp_SpotNumbers(spot);"), "before it reads the numbers");
}

static void TestJoiningTheGame()
{
	std::string const inputHeader = ReadSourceFile("3dc/avp/win95/user_io.h");
	std::string const input = ReadSourceFile("3dc/avp/win95/user_io.cpp");
	std::string const profile = ReadSourceFile("3dc/avp/win95/frontend/user_profile.cpp");
	std::string const menu = ReadSourceFile("3dc/avp/win95/frontend/menu_data.cpp");
	std::string const language = ReadSourceFile("3dc/avp/language.cpp");
	std::string const words = ReadSourceFile("3dc/avp/langenum.h");
	std::string const player = ReadSourceFile("3dc/avp/player.cpp");
	std::string const hud = ReadSourceFile("3dc/avp/hud.cpp");
	std::string const render = ReadSourceFile("3dc/d3d_render.cpp");
	std::string const shapes = ReadSourceFile("3dc/kshape.cpp");
	std::string const device = ReadSourceFile("win32/d3_func.cpp");

	// The byte, and what's kept the file the same
	Check(Has(inputHeader, "unsigned char Predator_Say;        // Predator\n\t\tunsigned char Marine_ShoulderLamp;"), "the key shares a byte with one the marine doesn't use, so a profile stays the same size");
	Check(CountOf(inputHeader, "#define NUMBER_OF_MARINE_INPUTS\t29") == 2, "and the marine has one more key");
	Check(Has(input, "static_assert(offsetof(PLAYER_INPUT_CONFIGURATION, Marine_ShoulderLamp) == NUMBER_OF_MARINE_INPUTS - 2,"), "the compiler checks that the menu, which counts bytes, reaches that byte");
	Check(Has(input, "static_assert(sizeof(PLAYER_INPUT_CONFIGURATION) == 32,"), "and that saved profiles still fit");

	// The defaults
	Check(CountOf(input, "\tKEY_G,\t\t\t// ShoulderLamp") == 5, "G is the default in every set of marine keys");
	Check(CountOf(input, "\tKEY_VOID,\t\t\t// ShoulderLamp") == 3, "and there's no second key by default, or on a pad");
	Check(CountOf(input, "KEY_G,") == 5, "and no other default key is G");
	Check(!Has(input, "KEY_J,"), "and none is J, which the developers' view moved to");

	// The key
	std::string const marineKeys = Between(input, "if (DebouncedKeyboardInput[primaryInput->ThrowFlare]", "#if !(MARINE_DEMO||DEATHMATCH_DEMO)");

	Check(Has(marineKeys, "if (DebouncedKeyboardInput[primaryInput->Marine_ShoulderLamp]\n\t\t\t\t ||DebouncedKeyboardInput[secondaryInput->Marine_ShoulderLamp])\n\t\t\t\t\tShoulderLamp_Toggle();"), "either of his keys switches it, once for each press, among the marine's own keys");
	Check(CountOf(input, "ShoulderLamp_Toggle();") == 1, "and nowhere else in the keys");

	// An old profile
	std::string const settling = Between(profile, "static void SettleMarineNewKeys", "extern int SaveUserProfile");

	Check(ComesBefore(profile, "MarineInputSecondaryConfig    = UserProfilePtr->MarineInputSecondaryConfig;", "SettleMarineNewKeys(&MarineInputPrimaryConfig, &MarineInputSecondaryConfig);"), "a profile's keys are settled once they've been read");
	Check(Has(settling, "MarineKeyIsTaken(primary, secondary, NUMBER_OF_MARINE_INPUTS - 2, KEY_G)"), "and looking for G takes in all his other keys, and not the lamp's own");
	Check(Has(profile, "primaryKeys[i] == key || secondaryKeys[i] == key"), "on either side");
	Check(Has(settling, "MarineKeyIsTaken(primary, secondary, NUMBER_OF_MARINE_INPUTS - 1, KEY_R)"), "and the reload key looks for R in all his others, the lamp's too");
	Check(Has(settling, "ShoulderLamp_KeyFromProfile(primary->Marine_Reload, KEY_ESCAPE, KEY_R, reloadDefaultIsTaken, KEY_VOID)"), "a profile from before it gets R, or no key if R is taken");
	Check(Has(settling, "ShoulderLamp_KeyFromProfile(secondary->Marine_Reload, KEY_ESCAPE, KEY_VOID, false, KEY_VOID)"), "and nothing on the second list");
	Check(Has(settling, "ShoulderLamp_KeyFromProfile(primary->Marine_ShoulderLamp, KEY_ESCAPE, KEY_G, lampDefaultIsTaken, KEY_VOID)"), "the first key becomes G");
	Check(Has(settling, "ShoulderLamp_KeyFromProfile(secondary->Marine_ShoulderLamp, KEY_ESCAPE, KEY_VOID, false, KEY_VOID)"), "and the second becomes no key");
	Check(Has(ReadSourceFile("3dc/avp/win95/frontend/menus.cpp"), "!(key == KEY_ESCAPE) &&"), "the menu refuses Escape, which is why it can only mean a profile that's never heard of the lamp");
	Check(Has(ReadSourceFile("3dc/win95/platform.h"), "enum KEY_ID\n{\n\tKEY_ESCAPE,"), "and Escape is key 0, which is what's in that byte in an old profile");
	Check(Has(ReadSourceFile("3dc/win95/platform.h"), "KEY_VOID=255,") && Has(ReadSourceFile("3dc/win95/platform.h"), "MAX_NUMBER_OF_INPUT_KEYS,"), "and the key arrays have a place for all 256 values a byte can hold, so no value in a file can read past them");

	// Its place on the page
	Check(Has(menu, "(j == NUMBER_OF_MARINE_INPUTS - 2) ? TEXTSTRING_MARINE_KEY_SHOULDER_LAMP : TEXTSTRING_MARINE_KEY_FORWARD+j"), "the page gives it its own name");
	Check(Has(words, "TEXTSTRING_RENDERING_AUTOGENERATE_NOTHING,\n\tTEXTSTRING_MARINE_KEY_SHOULDER_LAMP,"), "which comes after everything in the language file");
	Check(Has(language, "read maps.\",\n\t\"Shoulder lamp\",\n"), "and it's in the same place in our own text");

	// When it's on
	Check(Has(player, "void InitPlayer(STRATEGYBLOCK* sbPtr, int sb_type)\n{\n\t// The shoulder lamp starts off, every time\n\tShoulderLamp_TurnOff();"), "it starts off on every level");
	Check(Has(hud, "ShoulderLamp_AskForThisFrame(AvP.PlayerType == I_Marine && playerStatusPtr->IsAlive);"), "it's only for a marine who's alive");
	Check(ComesBefore(hud, "ShoulderLamp_AskForThisFrame(", "if (Observer)"), "and that's asked before the place where someone watching from outside returns, or they could never see it");
	Check(Has(hud, "ShoulderLamp_FollowTheView(look, right, down, (double)Approximate3dMagnitude(&moved), (double)NormalFrameTime / 65536.0);"), "and it's told every frame where he's looking");
	Check(Has(hud, "double const look[3] = { view.mat13 / 65536.0, view.mat23 / 65536.0, view.mat33 / 65536.0 };"), "which is the view's third direction");
	Check(ComesBefore(hud, "ShoulderLamp_FollowTheView(", "if (Observer)"), "whether someone is watching from outside or not");

	// Where it's drawn
	std::string const flush = Between(render, "// The marine's shoulder lamp, on the finished picture of the world", "// render any orthographic quads");
	std::string const drawing = Between(render, "static void DrawShoulderLamp()\n{", "ChangeZWriteEnable(ZWRITE_ENABLED);");

	Check(Has(flush, "if (toyFinalFlush && ShoulderLamp_TakeThisFramesAnswer())"), "it's drawn once a frame, on its last drawing");
	Check(ComesBefore(flush, "DrawShoulderLamp();", "weaponList->Draw();"), "before the gun, because it adds the colour of the wall behind, and over the gun that looks see-through");
	Check(ComesBefore(render, "weaponList->Draw();", "// render any orthographic quads"), "and all of that under the HUD");
	Check(Has(drawing, "d3d.shoulderLampEffect == kNullEffectID"), "and if its shader couldn't be made, there's simply no lamp");
	Check(!Has(Between(device, "// we should bail out if the shaders can't be loaded", "return false;"), "shoulderLampEffect"), "and that doesn't stop the game starting");
	// The only thing a network game changes here is that the spot's size and strength go back as built. The lamp itself stays on, whichever game it is.
	// The part after the size and strength is read from the game's file, up to where drawing ends, because drawing itself stops just before
	// that end, so a piece of drawing ending there would always be empty.
	Check(Has(drawing, "// A network game keeps these as built.") && !Has(Between(drawing, "static void DrawShoulderLamp()", "// A network game keeps these as built."), "Network") && !Has(Between(render, "// How big and how strong the spot is.", "ChangeZWriteEnable(ZWRITE_ENABLED);"), "Network"), "the user's ruling: it isn't turned off in a network game");
	// Over the real light the walls are lit only at LAMPLOOK 3, the painted lamp at full (modified 2026-11-11 by Melanikus (Third) for the AvP port)
	Check(Has(drawing, "onWalls = (!isARealLight || fullOver) && surfacePicturesAreFromThisFrame && SurfacePictures_Colour() && toyDepthTexture"), "walls are only lit from surface pictures of this frame");
	Check(Has(render, "void ThisFramesRenderingHasBegun()\n{\n\tsurfacePicturesAreFromThisFrame = false;"), "which is forgotten at the start of each frame");
	Check(Has(render, "surfacePicturesAreFromThisFrame = true;\n\t\t\tFrameTiming_Mark(kStageSurfacePictures);"), "and only set when they've been drawn");
	Check(Has(drawing, "device->SetTexture(3 + i, pictures[i]);") && Has(drawing, "if (onWalls)\n\t{\n\t\tdevice->SetTexture(3, NULL);\n\t\tdevice->SetTexture(4, NULL);\n"), "and it gives back the textures it borrows");

	// The user's ruling: it follows where he's looking, on a robot arm, and doesn't wiggle
	std::string const painted = ReadSourceFile("shaders/shoulderLamp.psh");
	std::string const shapesOfLight = ReadSourceFile("shaders/lightShapes.fxh");

	Check(Has(painted, "float2 aim = (kAim + Swing.xy * kFollowing) * wide;"), "the spot goes where the lamp is aimed");
	// The line from the shoulder to the aim is worked out by the game now, from the same aim (R-LAMP-1; modified 2026-11-14 by Melanikus (Dietrich))
	Check(Has(painted, "float2 beam = Beam.xy;") && Has(render, "ShoulderLamp_PaintedBeam(aim[0], aim[1], wide, beam);"), "and the cone still starts at his shoulder and ends there, so the beam swings like an arm");
	Check(Has(shapesOfLight, "normalize(lerp(float3(LampAim.x * kLampAimsAt, -LampAim.y * kLampAimsAt, kLampAimsAt) - light.xyz, LampAim.xyz, LampAim.w))"), "and the real light aims the same way, or another marine's lamp its own way");
	Check(Has(render, "ShoulderLamp_AimAcross() * d3d.matProjection._11 * 0.5f, ShoulderLamp_AimDown() * d3d.matProjection._22 * 0.5f,\n\t\t(float)ShoulderLamp_ShoulderAcross() * wide, (float)ShoulderLamp_ShoulderDown()"),
		"and the painted lamp is told where, as a place on the screen, and where its cone starts (modified 2026-11-14 by Melanikus (Dietrich), R-LAMP-1)");
	Check(Has(shapes, "out[kToyLightAim] = ShoulderLamp_AimAcross();\n\t\t\t\tout[kToyLightAim + 1] = ShoulderLamp_AimDown();"), "and so is the real one, in its place on the light list");
	Check(Has(ReadSourceFile("3dc/pixel_lights.cpp"), "device->SetPixelShaderConstantF(25, &light[kToyLightAim], 1);") && Has(ReadSourceFile("3dc/pixel_lights.cpp"), "if (perPixel && light[kLightLook] == kLookCone)\n\t\t{\n\t\t\tSetLampAimConstant(device, light);") && Has(ReadSourceFile("3dc/pixel_lights.cpp"), "SetLampAimConstant(device, light);\n\n\tReadPicture"), "and each lamp's own aim goes to both shaders, as each is drawn");
	Check(!Has(painted, "kSway") && !Has(shapesOfLight, "Sway") && !Has(ReadSourceFile("3dc/shoulder_lamp.h"), "Sway") && !Has(render, "ShoulderLamp_Sway"), "and the wiggle has gone altogether");
	Check(Has(painted, "kSizeAtFarEnd * kMiddleSize"), "and the middle layer is a part of the whole size, so a smaller spot keeps its shape");

	// The user's ruling: only the spot lights anything, or looking through the cone is night vision
	Check(Has(painted, "lerp(kBrighten * spot, (kGlow + onWall) * spot + haze, Lamp.y)"), "only the spot brightens or shows a wall's colour, and the cone only adds its haze");
	Check(Has(painted, "float3 haze = kHaze * cone;"), "and the haze is a colour of its own, with nothing of the wall in it");
	// The owner's ruling of 2026-11-11 ("Restore it, show me", "With shadows") turns the old one round: the painted lamp goes over the real light,
	// but only as LAMPLOOK says (modified 2026-11-11 by Melanikus (Third) for the AvP port)
	Check(Has(render, "if (isARealLight)\n\t{\n\t\tShoulderLamp_NoteARealLightFrame();\n\n\t\tif (!paintedToo)\n\t\t{\n\t\t\treturn;"),
		"and nothing painted goes over the real light unless LAMPLOOK asks for it");

	// The pictures it reads, in Gouraud
	std::string const forTheLamp = Between(render, "static void DrawSurfacePicturesForTheLamp()\n{", "// The stencil now marks the pixels");

	Check(Has(render, "if (ToyShadowsMode || ShoulderLamp_CouldBeDrawn())\n\t\t{\n\t\t\tSurfaceNormals_Lock();\n\t\t\tsurfaceNormalsWereWrittenThisFrame = true;"), "the normals are written while the lamp's on, whatever the view is");
	Check(Has(forTheLamp, "if (surfacePicturesAreFromThisFrame || !surfaceNormalsWereWrittenThisFrame || !ShoulderLamp_IsAskedForThisFrame())"), "the pictures are only drawn for the lamp if nothing else has, the normals are good, and the lamp is really asked for");
	Check(ComesBefore(forTheLamp, "savedCardSettings->Capture()", "ToyDrawPictures(device, worldAxes, false)") && ComesBefore(forTheLamp, "ToyDrawPictures(device, worldAxes, false)", "savedCardSettings->Apply();"), "and the card's settings are put back afterwards, because the game caches them");
	Check(Has(forTheLamp, "device->SetRenderTarget(0, screen);") && Has(forTheLamp, "screen->Release();"), "and the screen is drawn on again");
	Check(ComesBefore(render, "ToyShadows_Draw();\n\t\tViewSwitches_ApplyAsked();\n\t\tDrawSurfacePicturesForTheLamp();", "// The marine's shoulder lamp, on the finished picture"), "and it comes after the shadow pass, which may have drawn them already, and before the lamp reads them");

	// The real light
	std::string const considering = Between(shapes, "static void ConsiderTheShoulderLamp", "// Chooses this frame's lights.");

	Check(Has(considering, "if (!ToyShadowsLightsByPixel || !ToyShadows_VolumesInThisView() || !ShoulderLamp_CanBeALight() || !Player)"), "the real light only goes on the list when lights are added per pixel, or it would be a shadow with no light");
	Check(Has(considering, "ToyConsiderLight(&shoulderLampLight, Player, found, numFound, most, &lightsThisFrame.steadyInReach);"), "and as the player's, so it counts this close to the camera");
	Check(Has(shapes, "ConsiderTheShoulderLamp(steady, &numSteady, kLightChoiceMostCandidates);"), "and it's put forward with the steady lights");
	Check(Has(shapes, "if (light == &shoulderLampLight)\n\t\t\t{\n\t\t\t\tout[8] = 2.0f;\n\t\t\t\tshoulderLampIsChosen = true;"), "and it has its own look");
	Check(Has(render, "isARealLight = ToyShadows_ShoulderLampIsChosen() && surfacePicturesAreFromThisFrame"), "and the painted version stands aside only when it really was a light this frame");
	Check(ComesBefore(shapes, "\tshoulderLampIsChosen = false;\n", "shoulderLampIsChosen = true;") && Has(Between(shapes, "void ToyShadows_ChooseLights()", "ToyFoundLight steady[kLightChoiceMostCandidates];"), "shoulderLampIsChosen = false;"), "and whether it was is worked out again every frame, not kept from the first");
}

// THE LAMP'S LOOK, R-LAMP-1 (shoulder_lamp.h). Added 2026-11-14 by Melanikus (Dietrich) for the AvP port. The owner, 2026-11-11: "everything that
// changed between nov 7 and now is a variable yes? all of them" and "DEfaults from 7th, but all tunable". Each number the console takes comes from
// someone typing it, so it's hostile (Tao 3): every reader is tried at its limits and one step past them, and with words that aren't numbers, and each
// refusal says why it has to be refused.

typedef bool (*OneNumberReader)(const char *, unsigned int, double *);
typedef bool (*TwoNumberReader)(const char *, unsigned int, double *, double *);

static bool OneReads(OneNumberReader read, const char *words, double wanted)
{
	double number = -12345.0;

	return read(words, (unsigned int)strlen(words), &number) && fabs(number - wanted) < 0.0000001;
}

static bool OneRefuses(OneNumberReader read, const char *words)
{
	double number = -12345.0;

	return !read(words, (unsigned int)strlen(words), &number) && number == -12345.0;
}

static bool TwoRead(TwoNumberReader read, const char *words, double first, double second)
{
	double a = -12345.0;
	double b = -12345.0;

	return read(words, (unsigned int)strlen(words), &a, &b) && fabs(a - first) < 0.0000001 && fabs(b - second) < 0.0000001;
}

static bool TwoRefuse(TwoNumberReader read, const char *words)
{
	double a = -12345.0;
	double b = -12345.0;

	return !read(words, (unsigned int)strlen(words), &a, &b) && a == -12345.0 && b == -12345.0;
}

// The cone as it is now keeps both of the shader's divisions away from nothing
static bool ConeIsSafe()
{
	return ShoulderLamp_ConeInside() - ShoulderLamp_ConeOutside() >= kShoulderLampConeGap - 0.0000001
		&& ShoulderLamp_ConeInside() - kShoulderLampSpillEdge >= kShoulderLampConeGap
		&& ShoulderLamp_ConeInside() <= kShoulderLampMostConeInside && ShoulderLamp_ConeOutside() >= kShoulderLampFewestConeOutside;
}

// Words that are never a number for any of these, each with why
static const char *const kNeverANumber[][2] =
{
	{ "", "nothing at all" },
	{ "abc", "letters" },
	{ "1e1", "the library's way of writing ten, which the strict reader doesn't take" },
	{ "inf", "infinity, which would reach the shader as a number that isn't one" },
	{ "nan", "not a number" },
	{ "+1", "a plus sign" },
	{ "1.2.3", "two dots" },
	{ " 1", "a space in front, which the console takes off first" },
	{ "1 ", "a space after" },
	{ "123456789", "nine digits, longer than any of these needs, so the sum can't grow large" },
	{ "0x1", "a number written in hexadecimal" },
	{ "1,5", "a comma for the dot, as a Portuguese or German keyboard would type it" },
};

static void TestTheLampsLook()
{
	// As built: ce293c81's, the owner's "DEfaults from 7th"
	Check(ShoulderLamp_Strength() == 2.2 && ShoulderLamp_HalfDistance() == 9000.0 && ShoulderLamp_ConeInside() == 0.90 && ShoulderLamp_ConeOutside() == 0.86
		&& ShoulderLamp_Spill() == 0.0 && ShoulderLamp_ShoulderAcross() == 1.00 && ShoulderLamp_ShoulderDown() == -0.10 && ShoulderLamp_SpotHalfDistance() == 0.0,
		"R-LAMP-1: as built, the lamp's look is the owner's from 2026-11-05 (ce293c81): 2.2, 9000 mm, 0.90 and 0.86, no spill, the shoulder at (1.00, -0.10), and no fall on the spot");

	float light[8];

	ShoulderLamp_LightNumbers(light);
	Check(light[0] == 0.90f && light[1] == 0.86f && light[2] == 0.0f && light[3] == 2.2f, "the light shaders are told the cone, no spill and the strength, in that order");
	Check(fabs(light[4] * 9000.0f * 9000.0f - 1.0f) < 0.0001f && light[5] == 0.0f && light[6] == 0.0f && light[7] == 0.0f, "and one over the half-distance squared, with three spare 0s");

	float spot[4];

	ShoulderLamp_SpotNumbers(spot);
	Check(spot[3] == 0.0f, "and the painted spot is told it doesn't fall away, as on 2026-11-05");

	// LAMPSTRENGTH
	Check(OneReads(ShoulderLamp_StrengthFromWords, "0.1", 0.1) && OneReads(ShoulderLamp_StrengthFromWords, "4", 4.0) && OneReads(ShoulderLamp_StrengthFromWords, "2.2", 2.2), "LAMPSTRENGTH reads its least, its most and between");
	Check(OneRefuses(ShoulderLamp_StrengthFromWords, "0.09"), "LAMPSTRENGTH refuses 0.09: below its least, the lamp would hardly be there");
	Check(OneRefuses(ShoulderLamp_StrengthFromWords, "4.01"), "and 4.01: above its most it would be a floodlight, not a lamp");
	Check(OneRefuses(ShoulderLamp_StrengthFromWords, "-1"), "and -1: it reads no minus, and a light can't take light away");

	// LAMPHALFDISTANCE
	Check(OneReads(ShoulderLamp_HalfDistanceFromWords, "1000", 1000.0) && OneReads(ShoulderLamp_HalfDistanceFromWords, "20000", 20000.0) && OneReads(ShoulderLamp_HalfDistanceFromWords, "9000", 9000.0), "LAMPHALFDISTANCE reads its least, its most and between");
	Check(OneRefuses(ShoulderLamp_HalfDistanceFromWords, "999"), "LAMPHALFDISTANCE refuses 999: nearer than a metre the light would be gone at once");
	Check(OneRefuses(ShoulderLamp_HalfDistanceFromWords, "20001"), "and 20001: the lamp reaches no further than 20000 (kShoulderLampRange)");
	Check(OneRefuses(ShoulderLamp_HalfDistanceFromWords, "0"), "and 0, which the shader would take one over");
	Check(kShoulderLampMostHalfDistance == (double)kShoulderLampRange, "and its most is the lamp's reach");

	// LAMPCONE
	Check(TwoRead(ShoulderLamp_ConeFromWords, "0.90 0.86", 0.90, 0.86) && TwoRead(ShoulderLamp_ConeFromWords, "0.80 0.79", 0.80, 0.79) && TwoRead(ShoulderLamp_ConeFromWords, "0.995 0.985", 0.995, 0.985),
		"LAMPCONE reads inside then outside, at its limits and as built");
	Check(TwoRead(ShoulderLamp_ConeFromWords, "0.90   0.86", 0.90, 0.86), "with any number of spaces between them");
	Check(TwoRead(ShoulderLamp_ConeFromWords, "0.90 0.89", 0.90, 0.89), "and outside exactly the gap below inside");
	Check(TwoRefuse(ShoulderLamp_ConeFromWords, "0.90 0.895"), "LAMPCONE refuses an outside less than 0.01 below inside: the shader divides by their difference");
	Check(TwoRefuse(ShoulderLamp_ConeFromWords, "0.90 0.90"), "and the same two, where that difference is nothing");
	Check(TwoRefuse(ShoulderLamp_ConeFromWords, "0.86 0.90"), "and the two the wrong way round, where the edge would be inside out");
	Check(TwoRefuse(ShoulderLamp_ConeFromWords, "0.79 0.70"), "and an inside below 0.80: it has to stay well above the spill's edge, which the shader divides by too");
	Check(TwoRefuse(ShoulderLamp_ConeFromWords, "0.996 0.9"), "and an inside above 0.995, a cone so narrow it's a needle");
	Check(TwoRefuse(ShoulderLamp_ConeFromWords, "0.90 0.69"), "and an outside below 0.70, a cone too wide to be a lamp");
	Check(TwoRefuse(ShoulderLamp_ConeFromWords, "0.995 0.991"), "and an outside above 0.99");
	Check(TwoRefuse(ShoulderLamp_ConeFromWords, "0.90"), "and one number, as both are needed together: each one's limit is the other");
	Check(TwoRefuse(ShoulderLamp_ConeFromWords, "0.90 0.86 0.5"), "and three");
	Check(TwoRefuse(ShoulderLamp_ConeFromWords, "0.90 0.86 "), "and a space after, which the console takes off first");
	Check(TwoRefuse(ShoulderLamp_ConeFromWords, "0.90\t0.86") && TwoRefuse(ShoulderLamp_ConeFromWords, "0.90,0.86"), "and a tab or a comma between them");
	Check(kShoulderLampFewestConeInside - kShoulderLampSpillEdge >= kShoulderLampConeGap, "the least inside is far enough above the spill's edge");

	// LAMPSPILL
	Check(OneReads(ShoulderLamp_SpillFromWords, "0", 0.0) && OneReads(ShoulderLamp_SpillFromWords, "0.5", 0.5) && OneReads(ShoulderLamp_SpillFromWords, "0.12", 0.12), "LAMPSPILL reads none, its most and 2026-11-07's");
	Check(OneRefuses(ShoulderLamp_SpillFromWords, "0.51"), "LAMPSPILL refuses 0.51: a spill as strong as half the beam isn't faint any more");
	Check(OneRefuses(ShoulderLamp_SpillFromWords, "-0.1"), "and -0.1: it reads no minus, and a spill can't take light away");

	// LAMPSHOULDER
	Check(TwoRead(ShoulderLamp_ShoulderFromWords, "1 -0.1", 1.0, -0.1) && TwoRead(ShoulderLamp_ShoulderFromWords, "-0.5 1.5", -0.5, 1.5) && TwoRead(ShoulderLamp_ShoulderFromWords, "1.05 0.25", 1.05, 0.25),
		"LAMPSHOULDER reads across then down, with a minus, at its limits and 2026-11-07's place");
	Check(TwoRead(ShoulderLamp_ShoulderFromWords, "0.5 0.5", 0.5, 0.5), "and the middle of the screen, which the shader takes safely: see the beam's length below");
	Check(TwoRefuse(ShoulderLamp_ShoulderFromWords, "-0.51 0"), "LAMPSHOULDER refuses -0.51: more than half a screen off the edge the cone would never reach the picture");
	Check(TwoRefuse(ShoulderLamp_ShoulderFromWords, "0 1.51"), "and 1.51, on the other side");
	Check(TwoRefuse(ShoulderLamp_ShoulderFromWords, "1"), "and one number");
	Check(TwoRefuse(ShoulderLamp_ShoulderFromWords, "1 -0.1 0"), "and three");
	Check(TwoRefuse(ShoulderLamp_ShoulderFromWords, "--1 0") && TwoRefuse(ShoulderLamp_ShoulderFromWords, "- 0") && TwoRefuse(ShoulderLamp_ShoulderFromWords, "1 -"), "and a minus twice, or on its own");

	// LAMPSPOTHALFDISTANCE
	Check(OneReads(ShoulderLamp_SpotHalfDistanceFromWords, "0", 0.0) && OneReads(ShoulderLamp_SpotHalfDistanceFromWords, "0.0", 0.0), "LAMPSPOTHALFDISTANCE reads 0, no fall at all, as built");
	Check(OneReads(ShoulderLamp_SpotHalfDistanceFromWords, "1000", 1000.0) && OneReads(ShoulderLamp_SpotHalfDistanceFromWords, "20000", 20000.0) && OneReads(ShoulderLamp_SpotHalfDistanceFromWords, "7000", 7000.0), "and its least, its most and 2026-11-07's");
	Check(OneRefuses(ShoulderLamp_SpotHalfDistanceFromWords, "999") && OneRefuses(ShoulderLamp_SpotHalfDistanceFromWords, "1"), "LAMPSPOTHALFDISTANCE refuses anything between 0 and 1000: a spot whose light is gone at once isn't one");
	Check(OneRefuses(ShoulderLamp_SpotHalfDistanceFromWords, "20001"), "and 20001, past the lamp's reach");
	Check(OneRefuses(ShoulderLamp_SpotHalfDistanceFromWords, "-7000"), "and a minus");

	// Words that aren't a number, for every reader
	for (size_t i = 0; i < sizeof(kNeverANumber) / sizeof(kNeverANumber[0]); i++)
	{
		char const *const words = kNeverANumber[i][0];
		bool const refused = OneRefuses(ShoulderLamp_StrengthFromWords, words) && OneRefuses(ShoulderLamp_HalfDistanceFromWords, words) && OneRefuses(ShoulderLamp_SpillFromWords, words)
			&& OneRefuses(ShoulderLamp_SpotHalfDistanceFromWords, words) && TwoRefuse(ShoulderLamp_ConeFromWords, words) && TwoRefuse(ShoulderLamp_ShoulderFromWords, words);

		Check(refused, std::string("every R-LAMP-1 reader refuses [") + words + "]: " + kNeverANumber[i][1]);
	}

	double untouched = -12345.0;
	double alsoUntouched = -12345.0;

	Check(!ShoulderLamp_StrengthFromWords(0, 3, &untouched) && !ShoulderLamp_ConeFromWords(0, 3, &untouched, &alsoUntouched) && !ShoulderLamp_ShoulderFromWords(0, 3, &untouched, &alsoUntouched)
		&& untouched == -12345.0 && alsoUntouched == -12345.0, "and no words at all, where there's nothing to read");

	// The minus
	double signedNumber = 0.0;

	Check(ReadSignedNumber("-0.1", 4, &signedNumber) && signedNumber == -0.1 && ReadSignedNumber("0.1", 3, &signedNumber) && signedNumber == 0.1, "the signed reader reads a minus in front, and a number without");
	Check(!ReadSignedNumber("-", 1, &signedNumber) && !ReadSignedNumber("--0.1", 5, &signedNumber) && !ReadSignedNumber("-+1", 3, &signedNumber) && !ReadSignedNumber("0.1-", 4, &signedNumber), "and refuses a minus alone, twice, with a plus, or after");

	// How long a typed number can be (modified 2026-11-14 by Melanikus (Dietrich): the reader takes a longest, for the map editor; the
	// console's commands keep 8, so what they take doesn't change)
	double longNumber = 0.0;
	double unchanged = 0.0;
	double across = 0.0;

	Check(OneReads(ShoulderLamp_HalfDistanceFromWords, "00001000", 1000.0) && OneRefuses(ShoulderLamp_HalfDistanceFromWords, "000001000") && !ReadPlainNumber("123456789", 9, &longNumber),
		"the console's commands still refuse 9 characters or more, as before: their longest is 8");
	Check(ShoulderLamp_ShoulderFromWords("-0.123456 0", 11, &across, &unchanged) && fabs(across + 0.123456) < 0.0000001,
		"and LAMPSHOULDER still takes 8 after its minus, as it did before there was a longest");
	Check(ReadPlainNumber("32767.99998", 11, &longNumber, 11) && fabs(longNumber - 32767.99998) < 0.0000001, "a reader given 11 reads \"32767.99998\", as the editor's seconds need");
	Check(!ReadPlainNumber("32767.99998", 11, &longNumber, 10), "and one given 10 refuses it");
	Check(!ReadPlainNumber("123456789012", 12, &longNumber, 11) && ReadPlainNumber("123456789012", 11, &longNumber, 11) && longNumber == 12345678901.0,
		"text one character past its longest is refused before any of it is read, and the same text at its longest is read");
	Check(ReadPlainNumber("123456789012345678901234", 24, &longNumber, 100) && !ReadPlainNumber("1234567890123456789012345", 25, &longNumber, 100),
		"nobody gets a longest above kReadNumberMostLongest (24), however much is asked for, so the sum stays one a double holds");
	Check(ReadSignedNumber("-123456789.5", 12, &longNumber, 11) && longNumber == -123456789.5 && !ReadSignedNumber("-123456789.5", 12, &longNumber),
		"the signed reader passes its longest on, for the number after the minus");

	// The setters bring anything to the nearest limit, a number that isn't one too, so the shaders only ever get numbers inside them
	double const notANumber = std::numeric_limits<double>::quiet_NaN();

	ShoulderLamp_SetStrength(100.0);
	Check(ShoulderLamp_Strength() == kShoulderLampMostStrength, "a huge strength comes back to the most");
	ShoulderLamp_SetStrength(notANumber);
	Check(ShoulderLamp_Strength() == kShoulderLampFewestStrength, "and one that isn't a number to the least, so it can't reach a shader");
	ShoulderLamp_SetHalfDistance(1.0);
	Check(ShoulderLamp_HalfDistance() == kShoulderLampFewestHalfDistance, "a half-distance too near comes back to the least, so one over it is never huge");
	ShoulderLamp_SetHalfDistance(notANumber);
	Check(ShoulderLamp_HalfDistance() == kShoulderLampFewestHalfDistance, "and one that isn't a number too");
	ShoulderLamp_SetSpill(-1.0);
	Check(ShoulderLamp_Spill() == 0.0, "a spill below nothing is none");
	ShoulderLamp_SetSpill(1.0);
	Check(ShoulderLamp_Spill() == kShoulderLampMostSpill, "and a huge one the most");
	ShoulderLamp_SetShoulder(-9.0, 9.0);
	Check(ShoulderLamp_ShoulderAcross() == kShoulderLampFewestShoulder && ShoulderLamp_ShoulderDown() == kShoulderLampMostShoulder, "a shoulder far off the screen comes back to half a screen off");
	ShoulderLamp_SetShoulder(notANumber, notANumber);
	Check(ShoulderLamp_ShoulderAcross() == kShoulderLampFewestShoulder && ShoulderLamp_ShoulderDown() == kShoulderLampFewestShoulder, "and one that isn't a number too");
	ShoulderLamp_SetSpotHalfDistance(500.0);
	Check(ShoulderLamp_SpotHalfDistance() == kShoulderLampFewestSpotHalfDistance, "a spot's half-distance between 0 and the least comes to the least");
	ShoulderLamp_SetSpotHalfDistance(0.0);
	Check(ShoulderLamp_SpotHalfDistance() == 0.0, "and 0 stays 0, its own choice");
	ShoulderLamp_SetSpotHalfDistance(notANumber);
	Check(ShoulderLamp_SpotHalfDistance() == kShoulderLampFewestSpotHalfDistance, "and one that isn't a number is the least, never no fall by chance");

	// The cone, whatever it's given, keeps both divisions safe
	double const tries[][2] = { { 2.0, -1.0 }, { 0.80, 0.80 }, { 0.995, 0.99 }, { 0.0, 1.0 }, { 0.90, 0.86 } };
	bool alwaysSafe = true;

	for (size_t i = 0; i < sizeof(tries) / sizeof(tries[0]); i++)
	{
		ShoulderLamp_SetCone(tries[i][0], tries[i][1]);
		alwaysSafe = alwaysSafe && ConeIsSafe();
	}

	ShoulderLamp_SetCone(notANumber, notANumber);
	Check(alwaysSafe && ConeIsSafe(), "whatever cone it's given, numbers that aren't numbers too, inside stays the gap above outside and well above the spill's edge");
	ShoulderLamp_SetCone(0.995, 0.99);
	Check(ShoulderLamp_ConeInside() == 0.995 && fabs(ShoulderLamp_ConeOutside() - 0.985) < 0.0000001, "an outside too close to inside is brought down to the gap below it");

	// What the shaders are told follows
	ShoulderLamp_SetStrength(3.0);
	ShoulderLamp_SetHalfDistance(2000.0);
	ShoulderLamp_SetCone(0.95, 0.90);
	ShoulderLamp_SetSpill(0.2);
	ShoulderLamp_LightNumbers(light);
	Check(light[0] == 0.95f && light[1] == 0.90f && light[2] == 0.2f && light[3] == 3.0f && fabs(light[4] * 2000.0f * 2000.0f - 1.0f) < 0.0001f, "the light shaders are told what was set");
	ShoulderLamp_SetSpotHalfDistance(7000.0);
	ShoulderLamp_SpotNumbers(spot);
	Check(fabs(spot[3] * 7000.0f - 1.0f) < 0.0001f, "and the painted spot is told one over its half-distance, so the shader multiplies, and never divides");

	// The painted cone's line. As built, on a screen 1.6 times as wide as it's high, it goes from (1.6, -0.1) to the middle, (0.8, 0.5): (-0.8, 0.6), 1 long.
	ShoulderLamp_PutSpotBack();

	float beam[4];

	ShoulderLamp_PaintedBeam(0.0f, 0.0f, 1.6f, beam);
	Check(fabs(beam[0] + 0.8f) < 0.00001f && fabs(beam[1] - 0.6f) < 0.00001f && fabs(beam[2] - 1.0f) < 0.0001f && beam[3] == 0.0f, "the painted cone's line goes from the shoulder to the aim, and the shader is told one over its length squared");
	ShoulderLamp_PaintedBeam(0.1f, -0.2f, 1.6f, beam);
	Check(fabs(beam[0] - ((0.5f + 0.1f) * 1.6f - 1.6f)) < 0.00001f && fabs(beam[1] - (0.5f - 0.2f + 0.1f)) < 0.00001f, "and it follows the aim as the robot arm moves it");
	ShoulderLamp_SetShoulder(0.5, 0.5);
	ShoulderLamp_PaintedBeam(0.0f, 0.0f, 1.6f, beam);
	Check(beam[0] == 0.0f && beam[1] == 0.0f && std::isfinite(beam[2]) && fabs(beam[2] * (float)kShoulderLampShortestBeam - 1.0f) < 0.001f,
		"a shoulder put where the aim is makes a line of no length: it's taken as the shortest there can be, or the shader would divide by nothing");
	ShoulderLamp_PutSpotBack();

	// Putting it back
	ShoulderLamp_SetStrength(3.0);
	ShoulderLamp_SetHalfDistance(2000.0);
	ShoulderLamp_SetCone(0.95, 0.90);
	ShoulderLamp_SetSpill(0.2);
	ShoulderLamp_SetSpotHalfDistance(7000.0);
	Check(ShoulderLamp_PutSpotBack(), "putting the lamp back says something had changed");
	Check(ShoulderLamp_Strength() == kShoulderLampStrength && ShoulderLamp_HalfDistance() == kShoulderLampHalfDistance && ShoulderLamp_ConeInside() == kShoulderLampConeInside
		&& ShoulderLamp_ConeOutside() == kShoulderLampConeOutside && ShoulderLamp_Spill() == kShoulderLampSpill && ShoulderLamp_ShoulderAcross() == kShoulderLampShoulderAcross
		&& ShoulderLamp_ShoulderDown() == kShoulderLampShoulderDown && ShoulderLamp_SpotHalfDistance() == kShoulderLampSpotHalfDistance, "and every number of the lamp's look is as built");
	Check(!ShoulderLamp_PutSpotBack(), "and as built, nothing has changed");

	ShoulderLamp_SetStrength(3.0);
	bool const strengthCounts = ShoulderLamp_PutSpotBack();
	ShoulderLamp_SetHalfDistance(2000.0);
	bool const halfDistanceCounts = ShoulderLamp_PutSpotBack();
	ShoulderLamp_SetCone(0.95, 0.86);
	bool const insideCounts = ShoulderLamp_PutSpotBack();
	ShoulderLamp_SetCone(0.90, 0.80);
	bool const outsideCounts = ShoulderLamp_PutSpotBack();
	ShoulderLamp_SetSpill(0.2);
	bool const spillCounts = ShoulderLamp_PutSpotBack();
	ShoulderLamp_SetShoulder(1.05, -0.10);
	bool const acrossCounts = ShoulderLamp_PutSpotBack();
	ShoulderLamp_SetShoulder(1.00, 0.25);
	bool const downCounts = ShoulderLamp_PutSpotBack();
	ShoulderLamp_SetSpotHalfDistance(7000.0);
	bool const spotCounts = ShoulderLamp_PutSpotBack();

	Check(strengthCounts && halfDistanceCounts && insideCounts && outsideCounts && spillCounts && acrossCounts && downCounts && spotCounts,
		"each number alone counts as a change, so a network game puts back every one of them");

	// THE SHADERS. The real light's cone, from the game.
	std::string const shapes = ReadSourceFile("shaders/lightShapes.fxh");
	std::string const painted = ReadSourceFile("shaders/shoulderLamp.psh");
	std::string const glint = ReadSourceFile("shaders/lightLampShine.psh");

	Check(Has(shapes, "float4 LampCone : register(c26);") && Has(shapes, "float4 LampFall : register(c27);"), "SOURCE: the light shaders read the lamp's look from c26 and c27");
	Check(!Has(shapes, "kLampStrength") && !Has(shapes, "kLampHalfDistance") && !Has(shapes, "kLampFullInside") && !Has(shapes, "kLampNothingOutside") && !Has(shapes, "kLampSpillStrength"),
		"SOURCE: and none of the numbers it used to fix is left there to drift from the game's");
	Check(Has(shapes, "float inCone = smoothstep(kLampSpillOutside, LampCone.x, along) * LampCone.z + smoothstep(LampCone.y, LampCone.x, along);"), "SOURCE: the cone and the spill are the game's");
	Check(Has(shapes, "return LampCone.w * inCone * facing * (1.0f - reach * reach) / (1.0f + howFar * howFar * LampFall.x);"), "SOURCE: and so are the strength and the fall, by a multiplication");
	Check(Has(shapes, "static const float kLampSpillOutside = 0.77f;") && kShoulderLampSpillEdge == 0.77, "SOURCE: the spill's edge is the same number in the shader and in the limits that keep the cone above it");

	// Nothing else uses those two registers
	std::string path = __FILE__;
	std::string::size_type const tests = path.rfind("tests");
	int c26 = 0;
	int c27 = 0;
	int shaders = 0;

	if (tests != std::string::npos)
	{
		for (std::filesystem::directory_entry const &entry : std::filesystem::directory_iterator(path.substr(0, tests) + "shaders"))
		{
			std::string const name = entry.path().filename().string();
			std::string const text = ReadSourceFile(("shaders/" + name).c_str());

			shaders++;
			c26 += CountOf(text, "register(c26)");
			c27 += CountOf(text, "register(c27)");
		}
	}

	Check(shaders > 0 && c26 == 1 && c27 == 1, "SOURCE: of every shader, only lightShapes.fxh names c26 or c27");

	std::string const pixelLights = ReadSourceFile("3dc/pixel_lights.cpp");

	Check(CountOf(pixelLights, "SetPixelShaderConstantF(26, look, 2);") == 1 && CountOf(pixelLights, "SetPixelShaderConstantF(26") == 1 && CountOf(pixelLights, "SetPixelShaderConstantF(27") == 0,
		"SOURCE: the game fills them in one place");
	Check(Has(pixelLights, "SetActive(d3d.lightTotalEffect);\n\tSetScreenConstant(device);\n\tSetLampLookConstants(device);")
		&& Has(pixelLights, "SetActive(d3d.lightShareEffect);\n\tSetScreenConstant(device);\n\tSetLampLookConstants(device);")
		&& Has(pixelLights, "SetActive(d3d.lightOneEffect);\n\tSetScreenConstant(device);\n\tSetLampLookConstants(device);"),
		"SOURCE: for each of the three light shaders that draws a lamp's cone, as it's made ready");
	Check(!Has(ReadSourceFile("3dc/d3d_render.cpp"), "SetPixelShaderConstantF(26") && !Has(ReadSourceFile("3dc/d3d_render.cpp"), "SetPixelShaderConstantF(27")
		&& !Has(ReadSourceFile("3dc/surface_pictures.cpp"), "SetPixelShaderConstantF(26"), "SOURCE: and nowhere else");

	// The painted lamp
	Check(!Has(painted, "kShoulder =") && !Has(painted, "kShoulder *") && Has(painted, "float2 start = Swing.zw;"), "SOURCE: the painted cone starts where the game says, with no fixed place left");
	Check(Has(painted, "float4 Beam : register(c3);") && Has(painted, "float2 beam = Beam.xy;") && Has(painted, "float along = saturate(dot(here - start, beam) * Beam.z);"),
		"SOURCE: the cone's line comes from the game, with one over its length squared, so the full shader doesn't divide at all");
	Check(Has(painted, "static const float2 kAim = float2(0.50f, 0.50f);") && kShoulderLampPaintedAim == 0.5 && Has(painted, "static const float kFollowing = 1.0f;") && kShoulderLampPaintedFollowing == 1.0,
		"SOURCE: and the game works out the line from the same aim the shader puts the spot at, so the cone ends at the spot");
	Check(Has(Between(ReadSourceFile("3dc/d3d_render.cpp"), "static void DrawShoulderLamp()\n{", "template <class T> static void ToyRelease"), "ShoulderLamp_PaintedBeam(aim[0], aim[1], wide, beam);\n\tdevice->SetPixelShaderConstantF(3, beam, 1);"),
		"SOURCE: and the drawing sends it, from the same aim and shoulder it sends in Swing");
	Check(!Has(painted, "kHalfDistance") && Has(painted, "float howFar = tex2D(SurfaceDistance, In.Place).r * Spot.w;"), "SOURCE: the spot's fall is the game's, by a multiplication, and 0 is none");
	Check(!Has(glint, "kLampHalfDistance") && Has(glint, "float  away    = howFar * Spot.w;"), "SOURCE: and the glint falls away as its spot does");

	// The drawing
	std::string const render = ReadSourceFile("3dc/d3d_render.cpp");
	std::string const drawing = Between(render, "static void DrawShoulderLamp()\n{", "template <class T> static void ToyRelease");
	std::string const glinting = Between(render, "static void DrawLampGlint(float strength, float wide, const float *aim, const float *spot)\n{", "\n}\n");

	Check(ComesBefore(drawing, "ShoulderLamp_PutSpotBack()", "device->SetPixelShaderConstantF(1, aim, 1);"), "SOURCE: in a network game everything is put back before the shoulder's place is sent");
	Check(Has(glinting, "spot[0], spot[1], spot[2], spot[3],"), "SOURCE: and the glint is sent its spot's fall");

	// The commands: the console knows each, with the player's settings, a network game refuses a number, the words go through the strict reader, and the limits
	// each one says are the limits there are
	std::string const commands = ReadSourceFile("3dc/avp/game_commands.cpp");
	std::string const declared = ReadSourceFile("3dc/view_switches.h");
	std::string const opening = Between(render, "static bool LampLookWords(char *words, const char *name, char const **start, size_t *length)\n{", "\n}");

	Check(Has(opening, "return !LampStaysAsBuiltHere(why, *length);") && Has(opening, "\"%s IS OFF IN A NETWORK GAME: THE LAMP STAYS AS BUILT\", name"), "SOURCE: every one of them refuses a number in a network game, and says why");

	struct LampCommand
	{
		const char *name;
		const char *function;
		const char *reading;
		const char *setting;
		const char *limits;
		bool limitsAreTheLimits;
	};

	LampCommand const lampCommands[] =
	{
		{ "LAMPSTRENGTH", "LampStrengthCommand", "!ShoulderLamp_StrengthFromWords(start, (unsigned int)length, &strength)", "ShoulderLamp_SetStrength(strength);",
			"FROM 0.1 TO 4", kShoulderLampFewestStrength == 0.1 && kShoulderLampMostStrength == 4.0 },
		{ "LAMPHALFDISTANCE", "LampHalfDistanceCommand", "!ShoulderLamp_HalfDistanceFromWords(start, (unsigned int)length, &millimetres)", "ShoulderLamp_SetHalfDistance(millimetres);",
			"FROM 1000 TO 20000", kShoulderLampFewestHalfDistance == 1000.0 && kShoulderLampMostHalfDistance == 20000.0 },
		{ "LAMPCONE", "LampConeCommand", "!ShoulderLamp_ConeFromWords(start, (unsigned int)length, &inside, &outside)", "ShoulderLamp_SetCone(inside, outside);",
			"INSIDE FROM 0.80 TO 0.995, OUTSIDE FROM 0.70 TO 0.99 AND 0.01 OR MORE BELOW INSIDE", kShoulderLampFewestConeInside == 0.80 && kShoulderLampMostConeInside == 0.995
			&& kShoulderLampFewestConeOutside == 0.70 && kShoulderLampMostConeOutside == 0.99 && kShoulderLampConeGap == 0.01 },
		{ "LAMPSPILL", "LampSpillCommand", "!ShoulderLamp_SpillFromWords(start, (unsigned int)length, &spill)", "ShoulderLamp_SetSpill(spill);",
			"FROM 0 TO 0.5", kShoulderLampMostSpill == 0.5 },
		{ "LAMPSHOULDER", "LampShoulderCommand", "!ShoulderLamp_ShoulderFromWords(start, (unsigned int)length, &across, &down)", "ShoulderLamp_SetShoulder(across, down);",
			"EACH FROM -0.5 TO 1.5", kShoulderLampFewestShoulder == -0.5 && kShoulderLampMostShoulder == 1.5 },
		{ "LAMPSPOTHALFDISTANCE", "LampSpotHalfDistanceCommand", "!ShoulderLamp_SpotHalfDistanceFromWords(start, (unsigned int)length, &millimetres)", "ShoulderLamp_SetSpotHalfDistance(millimetres);",
			"0 FOR NO FALL, OR MILLIMETRES FROM 1000 TO 20000", kShoulderLampFewestSpotHalfDistance == 1000.0 && kShoulderLampMostSpotHalfDistance == 20000.0 },
	};

	for (size_t i = 0; i < sizeof(lampCommands) / sizeof(lampCommands[0]); i++)
	{
		LampCommand const &command = lampCommands[i];
		std::string const body = Between(render, (std::string("void ") + command.function + "(char *words)\n{").c_str(), "\n}");
		std::string const named = std::string("\"") + command.name + "\",";
		std::string const called = std::string(command.function) + "\n\t);";

		Check(Has(commands, named.c_str()) && Has(commands, called.c_str()) && ComesBefore(commands, named.c_str(), "bool IsACheat = true;"),
			std::string("SOURCE: the console knows ") + command.name + ", with the player's settings, not the cheats");
		Check(Has(declared, (std::string("void ") + command.function + "(char *words);").c_str()), std::string("SOURCE: and ") + command.function + " is declared for it");
		Check(ComesBefore(body, (std::string("LampLookWords(words, \"") + command.name + "\"").c_str(), command.setting), std::string("SOURCE: ") + command.name + " is refused in a network game before anything changes");
		Check(Has(body, command.reading) && Has(body, "\");\n\t\treturn;"), std::string("SOURCE: ") + command.name + "'s words go through the strict reader, and anything else is refused, not used");
		Check(Has(body, command.limits) && command.limitsAreTheLimits, std::string("SOURCE: the limits ") + command.name + " says are the limits there are");
	}

	ShoulderLamp_PutSpotBack();
}

int main()
{
	TestSwitching();
	TestAsking();
	TestRealLight();
	TestFlicker();
	TestKeysFromProfiles();
	TestTheRobotArm();
	TestLampFollow();
	TestTheSpot();
	TestTheLook();
	TestNetworkKeepsTheSpot();
	TestTheLampsLook();
	TestJoiningTheGame();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");

	return checksFailed ? 1 : 0;
}
