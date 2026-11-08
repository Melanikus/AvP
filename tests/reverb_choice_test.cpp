// The test for 3dc/reverb_choice.h, how loud the room echo comes out, and for the Room echo setting (REVERB-TOGGLE-1, issue #93).
// Written 2026-11-08 by Melanikus for the AvP port.
//
// tests/build.ps1 compiles this, and the program gives back a non-zero result if any check failed. It uses no sound device and runs
// none of the game: the decision is all in the header. The rows marked SOURCE read the game's files, to check that the sound code, the
// menus and the text use the decision and the setting the way the header says.

#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>

#include "reverb_choice.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static void TheGain()
{
	float const notANumber = std::numeric_limits<float>::quiet_NaN();
	float const endless = std::numeric_limits<float>::infinity();

	// Off is off, whatever the level asks for
	Check(Reverb_SlotGain(false, 0.5f) == 0.0f, "off: no echo, in a room that asks for half");
	Check(Reverb_SlotGain(false, 1.0f) == 0.0f, "off: none in a room that asks for all of it");
	Check(Reverb_SlotGain(false, -1.0f) == 0.0f, "off: none in a room that leaves it to distance");
	Check(Reverb_SlotGain(false, notANumber) == 0.0f, "off: none, even when the amount isn't a number");

	// On, the level's own amount
	Check(Reverb_SlotGain(true, 0.0f) == 0.0f, "on: a room that asks for none gets none");
	Check(Reverb_SlotGain(true, 0.25f) == 0.25f, "on: a room that asks for a quarter gets a quarter");
	Check(Reverb_SlotGain(true, 1.0f) == 1.0f, "on: a room that asks for all of it gets all of it");
	Check(Reverb_SlotGain(true, -1.0f) == 1.0f, "on: -1 leaves it to distance, so the echo is used in full");

	// Anything else from a level file counts as -1
	Check(Reverb_SlotGain(true, 1.0001f) == 1.0f, "on: a little over 1 isn't louder than all of it");
	Check(Reverb_SlotGain(true, 1000.0f) == 1.0f, "on: nor is a lot over 1");
	Check(Reverb_SlotGain(true, -0.5f) == 1.0f, "on: another negative number counts as -1");
	Check(Reverb_SlotGain(true, notANumber) == 1.0f, "on: something that isn't a number counts as -1");
	Check(Reverb_SlotGain(true, endless) == 1.0f && Reverb_SlotGain(true, -endless) == 1.0f, "on: endless numbers count as -1");

	Check(strcmp(kReverbHeading, "[Audio]") == 0 && strcmp(kReverbName, "Reverb") == 0, "the setting is [Audio] Reverb in the settings file");
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

	// A file has one kind of line ending or the other depending on how it was cloned, and these rows shouldn't care
	std::string text = all.str();
	text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
	return text;
}

static bool Has(std::string const &text, std::string const &words)
{
	return text.find(words) != std::string::npos;
}

static int CountOf(std::string const &text, std::string const &words)
{
	int count = 0;

	for (std::string::size_type place = text.find(words); place != std::string::npos; place = text.find(words, place + 1))
	{
		count++;
	}

	return count;
}

// The text from one marker up to the next one after it. Empty if either isn't there, so a row looking inside it fails instead of passing
// on nothing.
static std::string Between(std::string const &text, std::string const &from, std::string const &to)
{
	std::string::size_type const start = text.find(from);

	if (start == std::string::npos)
	{
		return "";
	}

	std::string::size_type const end = text.find(to, start + 1);

	return (end == std::string::npos) ? "" : text.substr(start, end - start);
}

static void TheSoundCode()
{
	std::string const sound = ReadSourceFile("3dc/avp/win95/openal_audio.cpp");
	Check(!sound.empty(), "SOURCE: openal_audio.cpp is read");

	std::string const start = Between(sound, "int PlatStartSoundSys()", "device = alcOpenDevice(NULL);");
	Check(Has(start, "ReverbIsOn = reverbSaved = Config_GetBool(kReverbHeading, kReverbName, true) ? 1 : 0;"), "SOURCE: the setting is read from the settings file, on unless it says false, before the device is opened");
	Check(Has(sound, "LogString(std::string(\"sound: the room echo (reverb) is \") + (ReverbIsOn ? \"on\" : \"off\")"), "SOURCE: and the log says which, as sound starts");

	std::string const apply = Between(sound, "static void Reverb_ApplyGain()", "void Reverb_MenuFrame()");
	Check(Has(apply, "float const gain = Reverb_SlotGain(ReverbIsOn != 0, SoundConfig.reverb_mix);"), "SOURCE: the gain is worked out from the setting and the room's amount");
	Check(Has(apply, "alAuxiliaryEffectSlotf(effectSlot, AL_EFFECTSLOT_GAIN, gain);"), "SOURCE: and it's given to the one effect slot every sound's echo goes through");
	Check(Has(sound, "alAuxiliaryEffectSlotf = (LPALAUXILIARYEFFECTSLOTF)alGetProcAddress(\"alAuxiliaryEffectSlotf\");"), "SOURCE: which OpenAL is asked for, with the other effect calls");

	std::string const room = Between(sound, "void PlatSetEnviroment(unsigned int env_index, float reverb_mix)", "void ResetEaxEnvironment()");
	Check(Has(room, "\tReverb_ApplyGain();\n}"), "SOURCE: every frame of play sets it, so a change is heard at once");
	Check(CountOf(room, "return;") == 1, "SOURCE: and nothing in between stops before it, but a device with no echo at all");

	std::string const saving = Between(sound, "void Reverb_MenuFrame()", "int PlatStartSoundSys()");
	Check(Has(saving, "Config_SetBool(kReverbHeading, kReverbName, now != 0);\n\t\tConfig_Save();"), "SOURCE: a change on the page is written to the settings file at once");
}

static void TheMenus()
{
	std::string const menus = ReadSourceFile("3dc/avp/win95/frontend/menus.cpp");
	Check(CountOf(menus, "ViewSettings_MenuFrame();\n\t\tReverb_MenuFrame();") == 2, "SOURCE: both kinds of menu notice the Room echo row change, with the rendering page's rows");

	std::string const pages = ReadSourceFile("3dc/avp/win95/frontend/menu_data.cpp");
	std::string const row = "{AVPMENU_ELEMENT_TEXTSLIDER,\t{TEXTSTRING_AVOPTIONS_ROOMECHO}, {1}, {&ReverbIsOn},\t{TEXTSTRING_NO},\tTEXTSTRING_AVOPTIONS_ROOMECHO_HELP},";
	Check(Has(Between(pages, "static AVPMENU_ELEMENT AvPMenu_InGameAVOptions[]", "AVPMENU_ELEMENT_ENDOFMENU"), row), "SOURCE: the sound page in the game has the row, No or Yes");
	Check(Has(Between(pages, "static AVPMENU_ELEMENT AvPMenu_MainMenuAVOptions[]", "AVPMENU_ELEMENT_ENDOFMENU"), row), "SOURCE: and so does the one in the main menus");
	Check(CountOf(pages, "{&ReverbIsOn}") == 2, "SOURCE: and nothing else changes the setting");

	// The ids come last among ours, and the words last in the list, so each id gets its own words. The compiler checks how many there are.
	std::string const ids = ReadSourceFile("3dc/avp/langenum.h");
	std::string const words = ReadSourceFile("3dc/avp/language.cpp");
	Check(Has(ids, "\tTEXTSTRING_AVOPTIONS_ROOMECHO,      // the sound pages' Room echo row, and its help line (REVERB-TOGGLE-1)\n\tTEXTSTRING_AVOPTIONS_ROOMECHO_HELP,\n\n\tMAX_NO_OF_TEXTSTRINGS"), "SOURCE: the row's two ids are the last of ours");
	Check(Has(words, "\t\"Room echo\",\n\t\"The echo each room adds to sounds (EAX reverb). Turn it off for dry sound, if it's too strong on your speakers or headphones.\",\n};"), "SOURCE: and its words are the last two in the list");
}

int main()
{
	TheGain();
	TheSoundCode();
	TheMenus();

	if (checksFailed)
	{
		printf("FAILED: %d checks\n", checksFailed);
		return 1;
	}

	printf("all room echo checks passed\n");
	return 0;
}
