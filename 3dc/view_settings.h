// The rendering page of the menus, and the settings it keeps. Written 2026-11-05 by Melanikus for the AvP port. See view_settings.cpp.

#ifndef VIEW_SETTINGS_H
#define VIEW_SETTINGS_H

// What the page's three lines are showing. The menu changes these directly when the player presses a key, so ViewSettings_MenuFrame is what notices.
extern int MenuRendering;      // one of the LightingChoice numbers
extern int MenuShadows;        // 0 or 1
extern int MenuAutoGenerate;   // 0 or 1

// Once the settings file has been read, and before anything looks at how to draw: starts the game the way the player last chose, or the way the game has always looked.
void ViewSettings_Load();

// Every frame a menu is showing: passes on what the player changed, saves it, and otherwise shows what's really being drawn, because a key or a console command can change it too,
// and a saved choice may be one this card can't draw.
void ViewSettings_MenuFrame();

#endif
