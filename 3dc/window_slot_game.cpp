// The game's side of -dev -windowed: asking Windows for the screen and the window's frame. Written 2026-11-09 by Melanikus for the AvP
// port. window_slot.h says what it's for; window_slot.cpp does the choosing.

#include <windows.h>

#include "window_slot.h"

static bool turnedOn = false;
static bool isTheSecondCopy = false;
static bool worked = false;
static WindowSlot last;

void WindowSlot_TurnOn(bool secondCopy)
{
	turnedOn = true;
	isTheSecondCopy = secondCopy;
}

bool WindowSlot_IsOn(void)
{
	return turnedOn;
}

bool WindowSlot_ForThisScreen(unsigned long windowStyle, const int *modeWidths, const int *modeHeights, int count, WindowSlot *slot)
{
	if (!turnedOn || !slot)
	{
		return false;
	}

	// The main screen, without its taskbar
	RECT area = { 0, 0, 0, 0 };

	if (!SystemParametersInfoA(SPI_GETWORKAREA, 0, &area, 0))
	{
		return false;
	}

	// The frame round a picture of no size, for a window of this style: what the caption and edges add to the picture's size
	RECT frame = { 0, 0, 0, 0 };

	if (!AdjustWindowRect(&frame, (DWORD)windowStyle, FALSE))
	{
		return false;
	}

	if (!WindowSlot_Work(area.left, area.top, area.right - area.left, area.bottom - area.top, frame.right - frame.left, frame.bottom - frame.top,
		isTheSecondCopy, modeWidths, modeHeights, count, slot))
	{
		return false;
	}

	last = *slot;
	worked = true;
	return true;
}

bool WindowSlot_Last(WindowSlot *slot)
{
	if (!turnedOn || !worked || !slot)
	{
		return false;
	}

	*slot = last;
	return true;
}
