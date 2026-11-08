// The test for 3dc/lamp_pick.h: which other marines' lamps are shown, and how each fades. Written 2026-11-07 by Melanikus for the AvP port.
//
// The lamps come from other machines too, so a hostile one can say any number are on, as often as it likes. These are the rules that keep
// that cheap and steady: only the nearest few are shown, one already shown isn't pushed out by another about as near, and a lamp switched
// on and off as fast as messages come can't flash faster than its fade.

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <fstream>
#include <sstream>
#include <string>

#include "lamp_pick.h"

static int checksFailed = 0;

static void Check(bool good, const char *what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what);

	if (!good)
	{
		checksFailed++;
	}
}

static bool SameOwners(const unsigned int *got, int count, const unsigned int *expected, int expectedCount)
{
	if (count != expectedCount)
	{
		return false;
	}

	for (int i = 0; i < expectedCount; i++)
	{
		if (!LampPick_Has(got, count, expected[i]))
		{
			return false;
		}
	}

	return true;
}

static void TestChoosing()
{
	unsigned int shown[kLampMostShown];
	LampCandidate const three[3] = { { 1, 9000 }, { 2, 3000 }, { 3, 6000 } };

	Check(LampPick_Choose(three, 3, 0, 0, 6, shown) == 3, "three lamps, room for six: all three are shown");
	{
		unsigned int const nearest[2] = { 2, 3 };
		Check(SameOwners(shown, LampPick_Choose(three, 3, 0, 0, 2, shown), nearest, 2), "room for two: the nearest two");
	}

	// Eight lamps, one a metre further than the last
	LampCandidate eight[8];

	for (int i = 0; i < 8; i++)
	{
		eight[i].owner = 100 + i;
		eight[i].distance = 1000 * (i + 1);
	}

	Check(kLampMostReal <= kLampMostShown, "no more can be real lights than are shown");
	Check(LampPick_Choose(eight, 8, 0, 0, 100, shown) == kLampMostShown, "asking for more than the most ever shown still shows only that many");
	Check(LampPick_Choose(eight, 8, 0, 0, 0, shown) == 0, "asking for none shows none");
	Check(LampPick_Choose(eight, 0, 0, 0, 6, shown) == 0, "and no lamps shows none");
	Check(LampPick_Choose(0, 8, 0, 0, 6, shown) == 0, "and nor does no list");

	// The stay rule
	{
		unsigned int const before[1] = { 107 };   // 8 metres away
		LampCandidate const two[2] = { { 107, 8000 }, { 100, 6500 } };
		unsigned int const kept[1] = { 107 };
		unsigned int const swapped[1] = { 100 };

		Check(SameOwners(shown, LampPick_Choose(two, 2, before, 1, 1, shown), kept, 1), "one shown last frame stays, when the other is nearer by less than the margin");

		LampCandidate const twoFar[2] = { { 107, 8000 }, { 100, 5999 } };
		Check(SameOwners(shown, LampPick_Choose(twoFar, 2, before, 1, 1, shown), swapped, 1), "but goes when the other is nearer by more");

		LampCandidate const gone[1] = { { 100, 9000 } };
		Check(SameOwners(shown, LampPick_Choose(gone, 1, before, 1, 1, shown), swapped, 1), "and one that isn't there any more isn't kept");
	}

	// Kept ones come first even when there are more than fit
	{
		unsigned int const before[2] = { 102, 101 };
		unsigned int const expected[2] = { 101, 102 };

		Check(SameOwners(shown, LampPick_Choose(eight, 8, before, 2, 2, shown), expected, 2), "two kept, room for two: the nearest new one is nearer than the further kept one by only the margin, so both stay");
	}

	// An owner listed twice counts once
	{
		LampCandidate const twice[3] = { { 7, 1000 }, { 7, 1000 }, { 8, 2000 } };
		unsigned int const expected[2] = { 7, 8 };

		Check(SameOwners(shown, LampPick_Choose(twice, 3, 0, 0, 6, shown), expected, 2), "an owner listed twice is shown once");
	}

	// More than are ever looked at: the rest aren't shown, and nothing is read past the list
	{
		static LampCandidate many[kLampMostCandidates + 10];

		for (int i = 0; i < kLampMostCandidates + 10; i++)
		{
			many[i].owner = 1000 + i;
			many[i].distance = i < kLampMostCandidates ? 50000 : 10;   // the nearest are past the end of what's looked at
		}

		int const count = LampPick_Choose(many, kLampMostCandidates + 10, 0, 0, 6, shown);
		bool noneFromPastTheEnd = true;

		for (int i = 0; i < count; i++)
		{
			noneFromPastTheEnd = noneFromPastTheEnd && shown[i] < 1000 + kLampMostCandidates;
		}

		Check(count == 6 && noneFromPastTheEnd, "only the first lamps in a very long list are looked at");
	}
}

static void TestFading()
{
	Check(LampPick_Fade(0.0f, true, kLampFadeSeconds) == 1.0f, "a lamp comes fully on in its fade time");
	Check(LampPick_Fade(1.0f, false, kLampFadeSeconds) == 0.0f, "and goes fully off in it");
	Check(fabsf(LampPick_Fade(0.0f, true, kLampFadeSeconds / 2.0f) - 0.5f) < 0.001f, "half the time, half on");
	Check(LampPick_Fade(1.0f, true, 1.0f) == 1.0f && LampPick_Fade(0.0f, false, 1.0f) == 0.0f, "it never goes past on or off");
	Check(LampPick_Fade(0.5f, true, 60.0f) == 1.0f, "a long frame moves it at most all the way");
	Check(LampPick_Fade(0.5f, true, -1.0f) == 0.5f, "a frame time below nothing moves it not at all");
	Check(LampPick_Fade(0.5f, true, nanf("")) == 0.5f, "nor does one that isn't a number");
	Check(LampPick_Fade(nanf(""), true, 0.0f) == 0.0f, "a strength that isn't a number starts again from off");

	// Switched every frame at 60 frames a second, it never gets far
	float strength = 0.0f;
	float most = 0.0f;

	for (int frame = 0; frame < 600; frame++)
	{
		strength = LampPick_Fade(strength, frame % 2 == 0, 1.0f / 60.0f);
		most = strength > most ? strength : most;
	}

	Check(most < 0.1f, "a lamp switched on and off every frame stays nearly dark: it can't flash");
}

static void TestDarkness()
{
	Check(LampPick_IsDark(kLampDarkBelow - 1) && !LampPick_IsDark(kLampDarkBelow), "darker than the line is dark, and the line itself isn't");
	Check(LampPick_IsDark(2560), "the darkest the game ever reads is dark");

	// An NPC marine's lamp. times is his count of clearly light decisions in a row.
	int times = 0;

	Check(LampPick_MarineWantsItOn(false, kLampDarkBelow - 1, false, &times) && times == 0, "a marine with his lamp off turns it on in the dark, at once");
	Check(!LampPick_MarineWantsItOn(false, kLampDarkBelow, false, &times), "but not on the line");
	times = 0;
	Check(LampPick_MarineWantsItOn(true, kLampLightAbove - 1, false, &times) && times == 0, "with it on, he keeps it on until it's clearly light, so he doesn't switch it on the line");
	Check(LampPick_MarineWantsItOn(true, kLampLightAbove, false, &times) && times == 1, "once clearly light, he keeps it on, because that could be someone else's flash");
	Check(!LampPick_MarineWantsItOn(true, kLampLightAbove, false, &times) && times == 2, "and turns it off the second time in a row");
	times = 1;
	Check(LampPick_MarineWantsItOn(true, kLampDarkBelow - 1, false, &times) && times == 0, "a dark decision in between starts the count again");
	times = 0;
	for (int i = 0; i < 10; i++)
	{
		LampPick_MarineWantsItOn(false, 60000, false, &times);
	}
	Check(times == kLampLightTimesToTurnOff, "and the count stops there, however long it stays light");
	Check(kLampLightTimesToTurnOff >= 2, "one clearly light decision is never enough to turn it off");
	Check(kLampLightAbove > kLampDarkBelow, "the light that turns it off is brighter than the dark that turns it on");
	times = 1;
	Check(LampPick_MarineWantsItOn(true, 60000, true, &times) && times == 1 && !LampPick_MarineWantsItOn(false, 0, true, &times) && times == 1, "while he's firing nothing changes, not even the count, because the light is his own flash");
	Check(kLampDecideEverySeconds >= 1.0f, "he decides no more often than once a second, so it can't blink");
}

// What only the game can run, read from its source. /FC makes __FILE__ the whole path, which is how the files are found.
static std::string ReadSourceFile(const char *name)
{
	std::string path = __FILE__;
	std::string::size_type const tests = path.rfind("tests");
	std::string const root = tests == std::string::npos ? std::string() : path.substr(0, tests);
	std::ifstream file(root + name, std::ios::binary);
	std::stringstream text;

	text << file.rdbuf();

	std::string out = text.str();
	std::string::size_type at;

	while ((at = out.find("\r\n")) != std::string::npos)
	{
		out.erase(at, 1);
	}

	return out;
}

// How many times words are in text
static int CountOf(const std::string &text, const char *words)
{
	int count = 0;

	for (std::string::size_type at = text.find(words); at != std::string::npos; at = text.find(words, at + 1))
	{
		count++;
	}

	return count;
}

static bool Has(const std::string &text, const char *words)
{
	return text.find(words) != std::string::npos;
}

static void TestTheGamesSide()
{
	std::string const lamps = ReadSourceFile("3dc/avp/other_lamps.cpp");
	std::string const shapes = ReadSourceFile("3dc/kshape.cpp");
	std::string const marine = ReadSourceFile("3dc/avp/bh_marine.cpp");
	std::string const ghosts = ReadSourceFile("3dc/avp/win95/npc_ghosts.cpp");

	Check(!lamps.empty(), "SOURCE: other_lamps.cpp can be read");
	Check(Has(lamps, "bool const allowed = Global_VDB_Ptr && (AvP.Network == I_No_Network || SameAsSinglePlayer());"), "SOURCE: lamps are only gathered in single player and the mode that plays as it");
	Check(Has(lamps, "return (marine->lampOn && block->SBDamageBlock.Health > 0) ? &marine->HModelController : 0;"), "SOURCE: a marine's lamp only while he's alive");
	Check(Has(lamps, "if (ghost->type == I_BehaviourMarinePlayer && MultiplayerObservedPlayer && ghost->playerId == MultiplayerObservedPlayer)"), "SOURCE: not the lamp of the player this machine watches through the eyes of");
	Check(Has(lamps, "LampPick_Choose(candidates, numCandidates, shownLastFrame, numShownLastFrame, kLampMostShown, shown)"), "SOURCE: only the nearest few are shown, by the rule tested above");
	Check(Has(lamps, "if (numCandidates < kLampMostCandidates)"), "SOURCE: and no more are looked at than the rule allows for");
	Check(Has(marine, "if (marineIsNear && marineStatusPointer->My_Weapon->ARealMarine && marineStatusPointer->behaviourState != MBS_Dying\n\t\t&& (AvP.Network == I_No_Network || AvP.NetworkAIServer))"), "SOURCE: a near, living, real marine decides, and only where the NPCs are run");
	Check(Has(ghosts, "if (ghostData->type == I_BehaviourMarine && !NpcGhosts_MarineIsAnAndroid(state->model)\n\t\t&& timeGetTime() - ghostData->lampDecidedAt >= (unsigned int)(kLampDecideEverySeconds * 1000.0f))"), "SOURCE: every other machine decides for its copies of them, every couple of seconds, and never for an android");
	Check(Has(ghosts, "ghostData->lampOn = LampPick_MarineWantsItOn(ghostData->lampOn != 0, LightIntensityAtPoint(position), (state->flags & kNpcFlagFiring) != 0,\n\t\t\t&ghostData->lampLightTimes) ? 1 : 0;"), "SOURCE: from the light where it shows him, firing as the host says");
	Check(Has(marine, "marineStatusPointer->lampDecideIn = (int)(kLampDecideEverySeconds * ONE_FIXED);\n\t\t\tmarineStatusPointer->lampOn = LampPick_MarineWantsItOn(marineStatusPointer->lampOn != 0, LightIntensityAtPoint(&sbPtr->DynPtr->Position),\n\t\t\t\tmarineStatusPointer->myGunFlash != NULL, &marineStatusPointer->lampLightTimes) ? 1 : 0;"), "SOURCE: every couple of seconds, by the light where he stands, firing while his gun's flash shows");
	Check(CountOf(marine, "marineStatus->lampOn = 0;\n\t\tmarineStatus->lampDecideIn = 0;\n\t\tmarineStatus->lampLightTimes = 0;") == 3, "SOURCE: a new marine decides on his first frame, with no count, wherever he's made");
	Check(Has(shapes, "OtherLamps_Gather((float)NormalFrameTime / 65536.0f);\n\tLampBeams_ForThisFrame();\n\n\tif (!ToyShadowsMode || CurrentVisionMode != VISION_MODE_NORMAL)"), "SOURCE: they're gathered every frame, before the shadow pass can leave early, so Gouraud has them too");
}

int main()
{
	printf("Which other marines' lamps are shown:\n");

	TestChoosing();
	TestFading();
	TestDarkness();
	TestTheGamesSide();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");

	return checksFailed ? 1 : 0;
}
