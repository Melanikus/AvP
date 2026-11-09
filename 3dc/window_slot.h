// Where a test copy's window goes, with -dev -windowed: each copy of the game on one PC in a half of the screen of its own, so two can run
// side by side. Written 2026-11-09 by Melanikus for the AvP port.
//
// WHY: two copies on one PC, a host and a joiner, for trying online play by hand. The second can't open its graphics while the first holds
// the screen in full-screen mode (D3DERR_DEVICELOST), so both open in a window. Each window is placed by a fixed rule, so they never overlap:
// the first copy on the left half, the second copy (-secondcopy, save_folder.h) on the right.
//
// The window's size isn't taken from the command line or the settings: it's chosen here, from the screen. The game only draws at sizes
// that are one of the screen's own modes (d3_func.cpp falls back to 800 by 600 otherwise), so the picture is the largest of those that fits
// in its half with the window's frame around it. The player's own video settings aren't changed, so the next normal start is as it was.
//
// Nothing here touches Windows: winmain.cpp, win_proj.cpp and d3_func.cpp do, through window_slot_game.cpp. tests/window_slot_test.cpp
// checks the choosing.

#ifndef WINDOW_SLOT_H
#define WINDOW_SLOT_H

// One window: where its outside goes on the screen, and the size of the picture inside it (the game's own drawing size)
struct WindowSlot
{
	int left, top;              // the window's top-left corner, frame and all
	int pictureWidth, pictureHeight;
};

// The window for a copy, in the screen's work area (the part the taskbar leaves free: areaLeft, areaTop, areaWidth, areaHeight), with a
// window frame of frameWidth by frameHeight round the picture (the caption and the edges, as Windows gives them), and the screen's modes
// (modeWidths and modeHeights, count of them). secondCopy says the right half; otherwise the left.
//
// The picture is the largest mode, by its area, that fits in the half with its frame. If none does, it's the smallest mode there is: the
// two windows may overlap then, but the game still opens. False, with nothing written, when there are no modes, or the area or frame can't
// be a screen's (nothing as big as a pixel, a frame bigger than the half, or numbers no screen has).
bool WindowSlot_Work(int areaLeft, int areaTop, int areaWidth, int areaHeight, int frameWidth, int frameHeight, bool secondCopy,
	const int *modeWidths, const int *modeHeights, int count, WindowSlot *slot);

// ---- The game's side (window_slot_game.cpp, which asks Windows)

// winmain.cpp, for -dev -windowed, before the window is made. secondCopy is whether this copy is the second (-dev -secondcopy).
void WindowSlot_TurnOn(bool secondCopy);

// The test window, worked out now: from the main screen's work area, the frame a window of this style (GWL_STYLE) has round its picture,
// and the screen's modes. False when -windowed isn't on, or Windows gives nothing usable: then the game's own size and place are used, as
// before.
bool WindowSlot_ForThisScreen(unsigned long windowStyle, const int *modeWidths, const int *modeHeights, int count, WindowSlot *slot);

// The place worked out last, for win_proj.cpp's ChangeWindowsSize, which sizes the window each time the game sets its picture size. False
// when there's none.
bool WindowSlot_Last(WindowSlot *slot);

#endif
