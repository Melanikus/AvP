// The test of where a test copy's window goes with -dev -windowed (3dc/window_slot.cpp). Written 2026-11-09 by Melanikus for the AvP port.
//
// WHAT IT'S FOR: two copies of the game side by side on one PC, a host and a joiner. Each window has to fit in its own half of the screen,
// frame and all, so the two never overlap. Its picture has to be one of the screen's own modes, or the game falls back to 800 by 600 and the
// windows overlap after all. And anything that can't be a screen is refused, rather than making a window of no size or one far off the
// screen. Then, read and not run: the game only does this with -dev, and takes nothing from the command line but the word. A test copy keeps
// running behind the other's window, and takes no mouse moves or pad while it's there.
//
// Nothing here uses Windows or the game.

#include "window_slot.h"

#include <stdio.h>
#include <string.h>
#include <string>
#include "source_slice.h"

static int checksFailed = 0;

static void Check(bool passed, const char *what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what);

	if (!passed)
	{
		checksFailed++;
	}
}

// A source file, as text, from the top of the repository, with its CRs taken out. The names are this file's own, never from outside.
static std::string ReadSource(const char *name)
{
	FILE *file = fopen(name, "rb");
	std::string text;

	if (!file)
	{
		return text;
	}

	char block[4096];
	size_t got = 0;

	while ((got = fread(block, 1, sizeof(block), file)) > 0)
	{
		for (size_t i = 0; i < got; i++)
		{
			if (block[i] != '\r')
			{
				text += block[i];
			}
		}
	}

	fclose(file);
	return text;
}

static void FailedSlice(const std::string &why)
{
	Check(false, why.c_str());
}

// A screen's modes, as a card might list them
static const int kWidths[] = { 640, 800, 1024, 1152, 1280, 1280, 1280, 1366, 1600, 1680, 1920, 2560, 3840 };
static const int kHeights[] = { 480, 600, 768, 864, 720, 960, 1024, 768, 900, 1050, 1080, 1440, 2160 };
static const int kModes = (int)(sizeof(kWidths) / sizeof(kWidths[0]));

// A window's frame round its picture, as Windows 11 gives it for the game's window style: the edges and the caption
static const int kFrameWidth = 16, kFrameHeight = 39;

static bool Fits(const WindowSlot &slot, int areaLeft, int areaTop, int areaWidth, int areaHeight, bool second)
{
	int const half = areaWidth / 2;
	int const halfLeft = areaLeft + (second ? half : 0);

	return slot.left >= halfLeft && slot.left + slot.pictureWidth + kFrameWidth <= halfLeft + half && slot.top >= areaTop &&
		slot.top + slot.pictureHeight + kFrameHeight <= areaTop + areaHeight;
}

static bool IsAMode(const WindowSlot &slot)
{
	for (int i = 0; i < kModes; i++)
	{
		if (kWidths[i] == slot.pictureWidth && kHeights[i] == slot.pictureHeight)
		{
			return true;
		}
	}

	return false;
}

static void TestTheTwoHalves()
{
	puts("Two copies on one screen");

	WindowSlot first, second;

	// A 1920 by 1080 screen with a 40-pixel taskbar
	bool const a = WindowSlot_Work(0, 0, 1920, 1040, kFrameWidth, kFrameHeight, false, kWidths, kHeights, kModes, &first);
	bool const b = WindowSlot_Work(0, 0, 1920, 1040, kFrameWidth, kFrameHeight, true, kWidths, kHeights, kModes, &second);

	Check(a && b, "on a 1920 by 1040 work area, both copies get a window");
	Check(a && first.left == 0 && first.top == 0, "the first copy's window is at the top left");
	Check(b && second.left == 960 && second.top == 0, "the second copy's is at the top of the right half");
	Check(a && first.pictureWidth == 800 && first.pictureHeight == 600, "each draws at 800 by 600, the largest mode that fits 960 wide with its frame");
	Check(a && b && first.left + first.pictureWidth + kFrameWidth <= second.left, "the first window ends before the second begins: they never overlap");

	// Bigger screens, and one to the left of the main one, the way Windows numbers a screen placed there
	struct Screen { int left, top, width, height; };
	Screen const screens[] = { { 0, 0, 1366, 728 }, { 0, 0, 1920, 1040 }, { 0, 0, 2560, 1400 }, { 0, 0, 3840, 2100 }, { -2560, 0, 2560, 1400 } };
	bool allFit = true, allModes = true;

	for (Screen const &s : screens)
	{
		for (int copy = 0; copy < 2; copy++)
		{
			WindowSlot slot;

			if (!WindowSlot_Work(s.left, s.top, s.width, s.height, kFrameWidth, kFrameHeight, copy == 1, kWidths, kHeights, kModes, &slot))
			{
				allFit = false;
				continue;
			}

			allFit = allFit && Fits(slot, s.left, s.top, s.width, s.height, copy == 1);
			allModes = allModes && IsAMode(slot);
		}
	}

	Check(allFit, "on every screen size, each copy's window, frame and all, is inside its own half");
	Check(allModes, "and draws at one of the screen's own modes, so the game doesn't fall back to another size");

	WindowSlot big;
	WindowSlot_Work(0, 0, 3840, 2100, kFrameWidth, kFrameHeight, false, kWidths, kHeights, kModes, &big);
	Check(big.pictureWidth == 1680 && big.pictureHeight == 1050, "on a 3840-wide screen, the largest mode that fits its 1920 half is 1680 by 1050");

	// A wide, short screen: 800 by 600 would fit its half's height, but not with the caption above it
	WindowSlot flat;
	WindowSlot_Work(0, 0, 2400, 620, kFrameWidth, kFrameHeight, false, kWidths, kHeights, kModes, &flat);
	Check(flat.pictureWidth == 640 && flat.pictureHeight == 480, "on a wide, short screen the frame's height counts too: 800 by 600 and its caption don't fit 620 high");

	// A screen too small for two halves of any use: the smallest mode, and the game still opens
	WindowSlot tiny;
	bool const opens = WindowSlot_Work(0, 0, 1024, 728, kFrameWidth, kFrameHeight, true, kWidths, kHeights, kModes, &tiny);
	Check(opens && tiny.pictureWidth == 640 && tiny.pictureHeight == 480 && tiny.left == 512, "where no mode fits a half, the smallest mode is used, still on its own side");
}

static void TestRefusals()
{
	puts("What can't be a screen");

	WindowSlot slot;
	memset(&slot, 0x5A, sizeof(slot));
	WindowSlot const untouched = slot;

	Check(!WindowSlot_Work(0, 0, 0, 1040, kFrameWidth, kFrameHeight, false, kWidths, kHeights, kModes, &slot), "a work area of no width is refused");
	Check(!WindowSlot_Work(0, 0, 1920, 0, kFrameWidth, kFrameHeight, false, kWidths, kHeights, kModes, &slot), "nor of no height");
	Check(!WindowSlot_Work(0, 0, 200000, 1040, kFrameWidth, kFrameHeight, false, kWidths, kHeights, kModes, &slot), "nor one wider than any screen");
	Check(!WindowSlot_Work(500000, 0, 1920, 1040, kFrameWidth, kFrameHeight, false, kWidths, kHeights, kModes, &slot), "nor one placed further off than any screen");
	Check(!WindowSlot_Work(0, 0, 1920, 1040, 960, kFrameHeight, false, kWidths, kHeights, kModes, &slot), "a frame as wide as the half is refused");
	Check(!WindowSlot_Work(0, 0, 1920, 1040, kFrameWidth, 1040, false, kWidths, kHeights, kModes, &slot), "and one as tall as the screen");
	Check(!WindowSlot_Work(0, 0, 1920, 1040, -1, kFrameHeight, false, kWidths, kHeights, kModes, &slot), "a frame of a size below 0 is refused");
	Check(!WindowSlot_Work(0, 0, 1920, 1040, kFrameWidth, kFrameHeight, false, kWidths, kHeights, 0, &slot), "with no modes, there's no window to choose");
	Check(!WindowSlot_Work(0, 0, 1920, 1040, kFrameWidth, kFrameHeight, false, NULL, kHeights, kModes, &slot), "nor with no list of them");

	int const badWidths[] = { 0, -800, 200000 };
	int const badHeights[] = { 600, 600, 600 };
	Check(!WindowSlot_Work(0, 0, 1920, 1040, kFrameWidth, kFrameHeight, false, badWidths, badHeights, 3, &slot), "modes that can't be a screen's are left out, and with none left there's no window");
	Check(memcmp(&slot, &untouched, sizeof(slot)) == 0, "a refusal writes nothing");

	Check(!WindowSlot_Work(0, 0, 1920, 1040, kFrameWidth, kFrameHeight, false, kWidths, kHeights, kModes, NULL), "and nothing is written with nowhere to write it");
}

static void TestTheGameUsesIt()
{
	puts("Where the game uses it, by reading");

	std::string const winmain = ReadSource("3dc/avp/win95/winmain.cpp");
	std::string const windowed = SourceSlice(winmain, "\tif (CommandLineHasWord(command_line, \"-windowed\"))", "\n\t}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(!windowed.empty() && windowed.find("if (CommandLineHasWord(command_line, \"-dev\"))") < windowed.find("WindowSlot_TurnOn(secondCopy);"),
		"-windowed does anything only with -dev");
	Check(windowed.find("CommandLineFindSwitch") == std::string::npos && windowed.find("CommandLineValue") == std::string::npos,
		"and takes nothing from the command line but the word itself");

	std::string const device = ReadSource("win32/d3_func.cpp");
	Check(device.find("WindowSlot_ForThisScreen(") != std::string::npos && device.find("WindowSlot_ForThisScreen(") < device.find("// check that the resolution and colour depth the user wants is supported"),
		"the picture size is chosen before the game checks it's one of the screen's modes");
	Check(device.find("Config_SetInt(\"[VideoMode]\", \"Width\" , currentWidth);") != std::string::npos &&
		SourceSlice(device, "\tif (windowed)\n\t{\n\t\tint modeWidths", "\n\t}\n", kSliceEndAfterTheStart, FailedSlice).find("Config_Set") == std::string::npos,
		"and the player's settings aren't written with it");

	std::string const window = ReadSource("3dc/avp/win95/win_proj.cpp");
	std::string const sizing = SourceSlice(window, "void ChangeWindowsSize(uint32_t width, uint32_t height)", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(sizing.find("WindowSlot_Last(&slot)") != std::string::npos && sizing.find("WindowMode == WindowModeSubWindow") != std::string::npos,
		"the window goes in its half each time the game sizes it, and only in a window");

	// A test copy keeps running behind the other copy's window, so a host and a joiner side by side both go on (the first footage: a copy
	// behind stopped for up to 53 s). A normal game still waits while it's behind, as it always has.
	std::string const messages = ReadSource("3dc/win95/win_func.cpp");
	std::string const loop = SourceSlice(messages, "void CheckForWindowsMessages(void)", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	size_t const handled = loop.find("DispatchMessage(&msg);");
	size_t const goesOn = loop.find("if (WindowSlot_IsOn())\n\t\t{\n\t\t\treturn;\n\t\t}");
	size_t const waits = loop.find("if (!bActive)\n\t\t{\n\t\t\tResetFrameCounter();");
	Check(handled != std::string::npos && goesOn != std::string::npos && waits != std::string::npos && handled < loop.find("WindowSlot_IsOn()") &&
		goesOn < waits && loop.find("while (!bActive && bRunning);") != std::string::npos,
		"a test copy behind another window keeps running once its messages are handled, and a normal game still waits");

	std::string const game = ReadSource("3dc/window_slot_game.cpp");
	Check(SourceSlice(game, "bool WindowSlot_IsBehind(void)", "\n}\n", kSliceEndAfterTheStart, FailedSlice).find("return turnedOn && !bActive;") != std::string::npos,
		"behind means a test copy whose window isn't the one in front");

	// But a copy behind takes nothing the player does: the mouse's raw moves and the pads reach every program, in front or not, and they're
	// the player's, for the copy in front. (Keys only ever go to the window in front.)
	std::string const rawInput = SourceSlice(window, "\t\tcase WM_INPUT:\n\t\t{", "\n\t\t}\n", kSliceEndAfterTheStart, FailedSlice);
	size_t const leftAlone = rawInput.find("if (WindowSlot_IsBehind())\n\t\t\t{\n\t\t\t\treturn 0;\n\t\t\t}");
	size_t const moved = rawInput.find("xPosRelative += raw.data.mouse.lLastX;");
	Check(leftAlone != std::string::npos && moved != std::string::npos && leftAlone < moved, "a test copy behind takes no mouse moves");

	std::string const pads = SourceSlice(ReadSource("win32/di_func.cpp"), "int UpdateControllerState()", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(pads.find("bool const behind = WindowSlot_IsBehind();") != std::string::npos &&
		pads.find("dwResult = behind ? ERROR_DEVICE_NOT_CONNECTED : XInputGetState(i, &g_Controllers[i].state);") != std::string::npos,
		"nor any pad");

	// The mouse's raw data is copied only when Windows says it fits its room, and read as a mouse's only when it's as big as one, so
	// nothing is written past the room or read from beyond what Windows wrote (it was copied into 40 bytes whatever its size, with only an
	// assert, which the released game doesn't have)
	Check(rawInput.find("!= 0 || size > sizeof(raw) ||") != std::string::npos &&
		rawInput.find("::GetRawInputData((HRAWINPUT)lParam, RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER)) != size)") != std::string::npos &&
		rawInput.find("size >= offsetof(RAWINPUT, data) + sizeof(RAWMOUSE)") != std::string::npos && rawInput.find("lpb") == std::string::npos,
		"the mouse's data is only copied when it fits, and only read as much as Windows wrote");
}

int main()
{
	TestTheTwoHalves();
	TestRefusals();
	TestTheGameUsesIt();

	if (checksFailed)
	{
		printf("%d checks FAILED\n", checksFailed);
		return 1;
	}

	puts("All checks passed");
	return 0;
}
