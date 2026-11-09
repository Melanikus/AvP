// The test for 3dc/reverb_choice.h, how loud the room echo comes out, and for the Room echo setting (REVERB-TOGGLE-1, issue #93).
// Written 2026-11-08 by Melanikus for the AvP port.
//
// tests/build.ps1 compiles this, and the program gives back a non-zero result if any check failed. It uses no sound device and runs
// none of the game: the decision is all in the header. The rows marked SOURCE read the game's files, to check that the sound code, the
// menus and the text use the decision and the setting the way the header says.

// Modified 2026-11-08 by Melanikus for the AvP port:
//   its Between is the shared one in source_slice.h, so a marker that isn't there is a failed check of its own (SOURCE-SLICE-1, #128)
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>

#include "reverb_choice.h"
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

static void TheRoomAndItsLine()
{
	float const notANumber = std::numeric_limits<float>::quiet_NaN();
	float const endless = std::numeric_limits<float>::infinity();

	// The amount kept for a room: the same room always gives the same number, so a change of room can be told by comparing
	Check(Reverb_RoomAmount(0.0f) == 0.0f && Reverb_RoomAmount(0.2f) == 0.2f && Reverb_RoomAmount(1.0f) == 1.0f, "a room's amount from 0 to 1 is kept as it is");
	Check(Reverb_RoomAmount(-1.0f) == -1.0f, "-1 is kept as -1");
	Check(Reverb_RoomAmount(1.5f) == -1.0f && Reverb_RoomAmount(-5.0f) == -1.0f, "a number out of range is kept as -1");
	Check(Reverb_RoomAmount(notANumber) == -1.0f, "so is something that isn't a number, which would differ from itself on every frame");
	Check(Reverb_RoomAmount(endless) == -1.0f && Reverb_RoomAmount(-endless) == -1.0f, "and endless numbers");

	Check(strcmp(Reverb_RoomName(0), "generic") == 0, "room 0 is generic");
	Check(strcmp(Reverb_RoomName(10), "hangar") == 0, "room 10 is a hangar");
	Check(strcmp(Reverb_RoomName(21), "sewer pipe") == 0, "room 21 is a sewer pipe");
	Check(strcmp(Reverb_RoomName(25), "psychotic") == 0, "room 25 is the last, psychotic");
	Check(strcmp(Reverb_RoomName(26), "unknown") == 0 && strcmp(Reverb_RoomName(1000), "unknown") == 0, "a number past the list has no name, and nothing past it is read");

	Check(Reverb_RoomLine(10, -1.0f, true) == "sound: room echo now hangar (10) at full", "the log's line for a hangar at full");
	Check(Reverb_RoomLine(21, 0.2f, true) == "sound: room echo now sewer pipe (21) at 0.20", "and for a sewer pipe at 0.2");
	Check(Reverb_RoomLine(3, -1.0f, false) == "sound: room echo now bathroom (3) at full, but Room echo is off, so none is heard", "and when Room echo is off, it says none is heard");
	Check(Reverb_RoomLine(1000, notANumber, true) == "sound: room echo now unknown (1000) at full", "a room and an amount from nowhere still make a plain line");
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

// The text from one marker up to the next (source_slice.h). A marker that isn't there is a failed check of its own, named by the marker.
static std::string Between(std::string const &text, std::string const &from, std::string const &to)
{
	return SourceSlice(text, from, to, kSliceEndFromOneOn, [](const std::string &why) { Check(false, why.c_str()); });
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
	Check(Has(room, "float const amount = Reverb_RoomAmount(reverb_mix);\n\n\tif (amount != SoundConfig.reverb_mix)"), "SOURCE: a room's amount is compared as it's kept");
	Check(Has(room, "SoundConfig.env_index = env_index;\n\t\troomChanged = true;") && Has(room, "SoundConfig.reverb_changed = true;\n\t\troomChanged = true;"), "SOURCE: a new kind of room or a new amount counts as a change of room");
	Check(Has(room, "if (roomChanged)\n\t{\n\t\tLogString(Reverb_RoomLine(SoundConfig.env_index, SoundConfig.reverb_mix, ReverbIsOn != 0));"), "SOURCE: and only then is it said in the log");
	Check(Has(sound, "static_assert(kMaxReverbPresets == kReverbRoomKinds,"), "SOURCE: the compiler checks the names are as many as the echo settings");

	// The names are in the same order as the echo settings: the i-th EFX_REVERB_PRESET_ is the i-th name, written in capitals with no spaces
	std::string const list = Between(sound, "static Environment Sound_Enviroments[] =", "};");
	bool sameOrder = !list.empty();
	std::string::size_type place = 0;

	for (unsigned int kind = 0; kind < (unsigned int)kReverbRoomKinds && sameOrder; kind++)
	{
		place = list.find("EFX_REVERB_PRESET_", place);

		if (place == std::string::npos)
		{
			sameOrder = false;
			break;
		}

		place += strlen("EFX_REVERB_PRESET_");
		std::string expected;

		for (const char *letter = Reverb_RoomName(kind); *letter; letter++)
		{
			if (*letter != ' ')
			{
				expected += (char)(*letter - 'a' + 'A');
			}
		}

		sameOrder = list.compare(place, expected.size(), expected) == 0 && list[place + expected.size()] == ',';
	}

	Check(sameOrder && list.find("EFX_REVERB_PRESET_", place) == std::string::npos, "SOURCE: each name is the kind of room its number picks, and there are no more settings than names");

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
	Check(Has(ids, "\tTEXTSTRING_COOP_MAP_COMPLETE,\n\tTEXTSTRING_AVOPTIONS_ROOMECHO,      // the sound pages' Room echo row, and its help line (REVERB-TOGGLE-1)\n\tTEXTSTRING_AVOPTIONS_ROOMECHO_HELP,\n"),
		"SOURCE: the row's two ids come right after the co-op pages' ones");
	Check(Has(words, "\t\"Map complete\",\n\t\"Room echo\",\n\t\"The echo each room adds to sounds (EAX reverb). Turn it off for dry sound, if it's too strong on your speakers or headphones.\",\n"),
		"SOURCE: and its words come right after theirs too, so each id has its own words");
}

int main()
{
	TheGain();
	TheRoomAndItsLine();
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
