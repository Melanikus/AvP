// The tester's mark key. Written 2026-11-07 by Melanikus for the AvP port. mark_files.h says what a mark is and why.
//
// Modified 2026-11-09 by Melanikus for the AvP port:
//   Mark_ThisPage, a mark of a menu page
#ifndef MARK_KEY_H
#define MARK_KEY_H

#include <d3d9.h>

// The key: F2. No species' default controls use it, and the game uses it for nothing else. If a player has given F2 to one of their
// controls, it does that instead, and marks nothing.
// Called once a frame while a level is being played, after the game has moved on: notes the moment if the key has just been pressed.
void Mark_ReadKey();

// A mark of a menu page, where there's no player: its line names the page, the time and its picture, which is taken as the frame finishes,
// as a level's is. page must be fixed words of the game's own, kept for as long as the game runs: online_screens_page.cpp's screen names.
// Within a second of the last mark, or with a mark already waiting, it does nothing, and gives back false.
bool Mark_ThisPage(const char *page);

// Called once a frame, once the frame is drawn and before it's shown: saves the picture of a moment Mark_ReadKey noted, then writes its
// line to the log and says on the screen that it's saved. The picture is taken before the words are on the screen.
void Mark_EndFrame(LPDIRECT3DDEVICE9 device);

#endif
