// The rendering page of the menus, and the settings it keeps. Written 2026-11-05 by Melanikus for the AvP port. See view_settings.cpp.

#ifndef VIEW_SETTINGS_H
#define VIEW_SETTINGS_H

#include "view_keys.h"

// What the page's first three lines are showing. The menu changes these directly when the player presses a key, so ViewSettings_MenuFrame is what notices.
extern int MenuRendering;      // one of the LightingChoice numbers
extern int MenuShadows;        // 0 or 1
extern int MenuAutoGenerate;   // 0 or 1

// Once the settings file has been read, and before anything looks at how to draw: starts the game the way the player last chose, or the way the game has always looked.
void ViewSettings_Load();

// Every frame a menu is showing: passes on what the player changed, saves it, and otherwise shows what's really being drawn, because a key or a console command can change it too,
// and a saved choice may be one this card can't draw.
void ViewSettings_MenuFrame();

// The page's other lines: the key for each of the things in ViewKeyLine. A key is the game's number for it, and KEY_VOID means none.
int ViewSettings_KeyOnLine(int line);

// The player has started choosing a key for a line, so what became of the last one no longer matters.
void ViewSettings_ChoosingKey(int line);

// The player pressed this key for this line, or KEY_VOID to have none. The rules in view_keys.h decide, a key that's taken is saved at once, and the line's help says what happened.
void ViewSettings_PressedKeyForLine(int line, int key);

// Does a control of any species have this line's key? Then the key does nothing while playing that species, and the menu shows it.
bool ViewSettings_LineIsHeldByAControl(int line);

// Does any of the lines have this key? The controls pages show that.
bool ViewSettings_KeyIsOnALine(int key);

// During a level: was this line's key pressed just now, and does it get to do something? A control of whoever's being played wins. What else has to be true for each key is decided by whoever asks, in d3d_render.cpp.
bool ViewSettings_KeyPressed(int line);

#endif
