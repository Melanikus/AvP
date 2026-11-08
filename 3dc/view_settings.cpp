// The rendering page of the menus, and the settings it keeps. Written 2026-11-05 by Melanikus for the AvP port.
//
// WHAT THE PLAYER GETS: three lines in the menus: how the world is lit, whether things cast shadows, and whether maps are worked out for textures with none. A new player gets the
// game as it has always looked. All the new drawing is off, so none of it runs in a frame, and the pictures and prisms it needs aren't made. Loading a level still does a little for it,
// working out each shape's normals, so that it can be turned on during a level. What they choose is kept in AliensVsPredator.cfg, not in their profile, because it belongs to this
// computer and this graphics card, not to who's playing.
//
// THE FILE CAN SAY ANYTHING: it's a text file anyone can edit, so only the exact words count. Anything else is treated as if it wasn't there, the log says so, and what it said is
// never repeated there.
//
// ONE WAY IN: the menu doesn't change how anything is drawn. It asks, the same way the LIGHTING and SHADOWS commands do, and the change happens at the one safe moment in a frame:
// see ViewSwitches_ApplyAsked in d3d_render.cpp. The commands only change this run. Only the menu saves.
//
// THE KEYS: three more lines say which key changes the lighting (L), the shadows (H) and the developers' views (J). They're kept in the same file, as numbers. The rules for which key a line may have
// are in view_keys.h, and this is what joins them to the game. A key already in use is refused, and the line's help says why. A control that gets one of these keys afterwards wins, and both pages show it.
//
// THE HUD SIZE: the last line is a slider for how big the HUD is drawn, 50% to 150% of the usual size, kept in the same file as HUDSize. The rules are in hud_scale.h. The HUD takes
// a new size from the next frame of a level: HUD_FollowTheScreen, in d3d_hud.cpp, asks ViewSettings_HUDSize every frame.
//
// Modified 2026-11-08 by Melanikus for the AvP port:
//   the HUD size line, its slider and its word in the settings file (HUD-SCALE-1). Reported by Bobblen; follows his avp-forever (commit a53f482), rewritten

#include "3dc.h"
#include "module.h"
#include "inline.h"
#include "strategy_def.h"
#include "gamedef.h"
#include "input.h"
#include "user_io.h"
#include "ConfigFile.h"
#include "logString.h"
#include "texture_relief.h"
#include "view_settings.h"
#include "view_switches.h"
#include "hud_scale.h"

#include <string.h>
#include <string>

// In d3d_render.cpp, beside the rest of what happens to these two switches
void ViewSwitches_AskLighting(int choice);
void ViewSwitches_AskShadows(int on);
bool ViewSwitches_SomethingAsked();
void ViewSwitches_SetAtStart(int lighting, int volumes);
void ViewSwitches_WhatsThere(bool *mapsThere, bool *perPixelThere, bool *marchThere);
int ViewSwitches_DrawnForTheMenu();
bool ViewSwitches_ShadowsCanBeOn();
void RenderingPage_SayWhatLinesDo(bool shadowsCanBeOn, bool mapsAreRead);   // menu_data.cpp
void RenderingPage_SayWhatKeyLineDoes(int line, ViewKeyAnswer lastAnswer, bool heldByAControl);   // menu_data.cpp

static const char kHeading[] = "[Video]";
static const char kRendering[] = "Rendering";
static const char kShadows[] = "Shadows";
static const char kAutoGenerate[] = "AutoGenerateMaps";
static const char kHUDSizeSetting[] = "HUDSize";

// The HUD size the player chose, a percentage of the usual size, and where the page's slider shows it, from 0 to kHUDSizeSliderPlaces
static int hudSize = kHUDSizeUsual;
int MenuHUDSize = kHUDSizeUsualPlace;
static int lastHUDSize = kHUDSizeUsualPlace;

int MenuRendering = kLightingGouraud;
int MenuShadows = 0;
int MenuAutoGenerate = 0;

// What the page was showing at the end of the last menu frame. Anything different has been changed by the player.
static int lastRendering = kLightingGouraud;
static int lastShadows = 0;
static int lastAutoGenerate = 0;

// The three keys. Where each is kept in the settings file, and the one it has until the player says otherwise.
static const char *const kKeySettings[kViewKeyLines] = { "RenderingKey", "ShadowsKey", "DevViewsKey" };
static const int kUsualKeys[kViewKeyLines] = { KEY_L, KEY_H, KEY_J };

// The keys the game reads for itself, whatever the controls say, so a line can't have one. Pressing one of these would do its own job as well.
static const unsigned char kKeysTheGameKeeps[] =
{
	KEY_ESCAPE, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9, KEY_0,   // the menu and the weapons: FixedInputConfig in user_io.cpp
	KEY_GRAVE,                                                                          // the console, in user_io.cpp
	KEY_JOYSTICK_BUTTON_9,                                                              // a game pad's menu button, in user_io.cpp
	KEY_F5, KEY_F7, KEY_F8, KEY_F9, KEY_F10                                             // the developers' tools, in ViewSwitches_ApplyAsked in d3d_render.cpp
};

static int viewKeys[kViewKeyLines] = { KEY_L, KEY_H, KEY_J };

// What became of the last key each line was asked to take, for the line's help to say
static ViewKeyAnswer lastAnswer[kViewKeyLines] = { kViewKeyTaken, kViewKeyTaken, kViewKeyTaken };

static ViewKeyRules TheRules(const unsigned char *controls, int controlsCount)
{
	ViewKeyRules rules;

	rules.noKey = KEY_VOID;
	rules.lastRealKey = KEY_DIACRITIC_UMLAUT;
	rules.gameKeeps = kKeysTheGameKeeps;
	rules.gameKeepsCount = sizeof(kKeysTheGameKeeps);
	rules.controls = controls;
	rules.controlsCount = controlsCount;

	return rules;
}

// A set of controls is 32 keys, one byte each. The menus' controls pages treat it that way too: see CheckForKeysWithMultipleAssignments.
static_assert(sizeof(PLAYER_INPUT_CONFIGURATION) == 32, "a set of controls is a list of 32 keys");

static const int kControlsInASet = sizeof(PLAYER_INPUT_CONFIGURATION);

// The controls of all three species, each with its two columns. A key chosen in the menu mustn't clash with whichever of them is played next.
static void EveryControl(unsigned char controls[kControlsInASet * 6])
{
	memcpy(controls + kControlsInASet * 0, &MarineInputPrimaryConfig, kControlsInASet);
	memcpy(controls + kControlsInASet * 1, &MarineInputSecondaryConfig, kControlsInASet);
	memcpy(controls + kControlsInASet * 2, &PredatorInputPrimaryConfig, kControlsInASet);
	memcpy(controls + kControlsInASet * 3, &PredatorInputSecondaryConfig, kControlsInASet);
	memcpy(controls + kControlsInASet * 4, &AlienInputPrimaryConfig, kControlsInASet);
	memcpy(controls + kControlsInASet * 5, &AlienInputSecondaryConfig, kControlsInASet);
}

// The controls of whoever is being played right now, the same two sets that user_io.cpp reads.
static void TheControlsInUse(unsigned char controls[kControlsInASet * 2])
{
	switch (AvP.PlayerType)
	{
		case I_Predator:
			memcpy(controls, &PredatorInputPrimaryConfig, kControlsInASet);
			memcpy(controls + kControlsInASet, &PredatorInputSecondaryConfig, kControlsInASet);
			break;

		case I_Alien:
			memcpy(controls, &AlienInputPrimaryConfig, kControlsInASet);
			memcpy(controls + kControlsInASet, &AlienInputSecondaryConfig, kControlsInASet);
			break;

		case I_Marine:
		default:
			memcpy(controls, &MarineInputPrimaryConfig, kControlsInASet);
			memcpy(controls + kControlsInASet, &MarineInputSecondaryConfig, kControlsInASet);
			break;
	}
}

void ViewSettings_Load()
{
	// Asking for a setting that isn't there puts it there, with this as its value. That has to happen, because setting one later only changes one that's already there.
	std::string const rendering = Config_GetString(kHeading, kRendering, Lighting_Word(kLightingGouraud));
	bool const shadows = Config_GetBool(kHeading, kShadows, false);
	bool const autoGenerate = Config_GetBool(kHeading, kAutoGenerate, false);

	int choice = kLightingGouraud;

	if (!Lighting_FromSavedWord(rendering.c_str(), &choice))
	{
		LogErrorString("the settings file's Rendering line isn't one of GOURAUD, PERPIXEL, NORMALS or RELIEF, so it's GOURAUD");
		choice = kLightingGouraud;
		Config_SetString(kHeading, kRendering, Lighting_Word(kLightingGouraud));
	}

	// Everything off is the game as it has always been drawn, from the very first frame. See view_switches.h.
	ViewSwitches_SetAtStart(choice, shadows ? 1 : 0);
	textureRelief.SetAutoGenerate(autoGenerate);

	MenuRendering = lastRendering = choice;
	MenuShadows = lastShadows = shadows ? 1 : 0;
	MenuAutoGenerate = lastAutoGenerate = autoGenerate ? 1 : 0;

	// The keys. The first read of a file from before these lines gets the usual ones, and gets the lines. They're read as words, because the file's own reader of numbers takes anything. No profile has
	// been read yet, so the controls aren't known here. That's sorted out during a level, where a control wins.
	std::string words[kViewKeyLines];
	char const *wordsToRead[kViewKeyLines];
	bool wasRefused[kViewKeyLines];

	for (int line = 0; line < kViewKeyLines; line++)
	{
		words[line] = Config_GetString(kHeading, kKeySettings[line], std::to_string(kUsualKeys[line]));
		wordsToRead[line] = words[line].c_str();
	}

	ViewKey_SettleSaved(wordsToRead, kUsualKeys, TheRules(0, 0), viewKeys, wasRefused);

	for (int line = 0; line < kViewKeyLines; line++)
	{
		if (wasRefused[line])
		{
			LogErrorString(std::string("the settings file's ") + kKeySettings[line] + " line isn't a key that line can have, so it has gone back");
			Config_SetString(kHeading, kKeySettings[line], std::to_string(viewKeys[line]));
		}
	}

	// The HUD size, read as a word for the same reason as the keys. A file from before this line gets the usual size, and gets the line.
	std::string const hudSizeWord = Config_GetString(kHeading, kHUDSizeSetting, std::to_string(kHUDSizeUsual));

	if (!HUDSize_FromSavedWord(hudSizeWord.c_str(), &hudSize))
	{
		LogErrorString("the settings file's HUDSize line isn't one of the HUD sizes, 50 to 150 in steps of 10, so it's " + std::to_string(hudSize));
		Config_SetString(kHeading, kHUDSizeSetting, std::to_string(hudSize));
	}

	MenuHUDSize = lastHUDSize = HUDSize_SliderPlace(hudSize);
}

void ViewSettings_MenuFrame()
{
	bool changed = false;

	// A change that's been asked for and hasn't happened yet: the frame it was waiting for couldn't start. The page keeps showing what the player chose until it has.
	if (!ViewSwitches_SomethingAsked())
	{
		if (MenuRendering != lastRendering)
		{
			bool mapsThere = false;
			bool perPixelThere = false;
			bool marchThere = false;

			ViewSwitches_WhatsThere(&mapsThere, &perPixelThere, &marchThere);

			int const choice = Lighting_MenuStep(lastRendering, MenuRendering, mapsThere, perPixelThere, marchThere);

			// Nothing else may be there to move to, and then there's nothing to ask for, and nothing to save.
			if (choice != lastRendering)
			{
				ViewSwitches_AskLighting(choice);
				Config_SetString(kHeading, kRendering, Lighting_Word(choice));
				changed = true;
			}

			lastRendering = MenuRendering = choice;
		}
		else
		{
			lastRendering = MenuRendering = ViewSwitches_DrawnForTheMenu();
		}

		if (MenuShadows != lastShadows)
		{
			// A card that can't draw them refuses, so the line goes back, and what was refused isn't saved as if it had been chosen.
			if (MenuShadows && !ViewSwitches_ShadowsCanBeOn())
			{
				MenuShadows = 0;
			}
			else
			{
				ViewSwitches_AskShadows(MenuShadows);
				Config_SetBool(kHeading, kShadows, MenuShadows != 0);
				changed = true;
			}

			lastShadows = MenuShadows;
		}
		else
		{
			lastShadows = MenuShadows = ShadowVolumesOn;
		}
	}

	RenderingPage_SayWhatLinesDo(ViewSwitches_ShadowsCanBeOn(), Lighting_UsesMaps(MenuRendering));

	for (int line = 0; line < kViewKeyLines; line++)
	{
		RenderingPage_SayWhatKeyLineDoes(line, lastAnswer[line], ViewSettings_LineIsHeldByAControl(line));
	}

	// Turning it off takes effect at once, and on from the next level, because maps are only worked out while a level loads. TextureRelief knows that, and the page says so.
	if (MenuAutoGenerate != lastAutoGenerate)
	{
		lastAutoGenerate = MenuAutoGenerate;
		textureRelief.SetAutoGenerate(lastAutoGenerate != 0);
		Config_SetBool(kHeading, kAutoGenerate, lastAutoGenerate != 0);
		changed = true;
	}

	// The HUD size changes from the next frame of a level, which asks for it every frame. The slider only moves one place at a time between
	// its ends, and the size is still worked out from its place through the rules, so it can only ever be one of the sizes there are.
	if (MenuHUDSize != lastHUDSize)
	{
		hudSize = HUDSize_AtSliderPlace(MenuHUDSize);
		MenuHUDSize = lastHUDSize = HUDSize_SliderPlace(hudSize);
		Config_SetString(kHeading, kHUDSizeSetting, std::to_string(hudSize));
		changed = true;
	}

	if (changed)
	{
		Config_Save();
	}
}

int ViewSettings_HUDSize()
{
	return hudSize;
}

int ViewSettings_KeyOnLine(int line)
{
	return (line < 0 || line >= kViewKeyLines) ? KEY_VOID : viewKeys[line];
}

void ViewSettings_ChoosingKey(int line)
{
	if (line >= 0 && line < kViewKeyLines)
	{
		lastAnswer[line] = kViewKeyTaken;
	}
}

void ViewSettings_PressedKeyForLine(int line, int key)
{
	if (line < 0 || line >= kViewKeyLines)
	{
		return;
	}

	unsigned char controls[kControlsInASet * 6];

	EveryControl(controls);
	lastAnswer[line] = ViewKey_Judge(line, key, viewKeys, TheRules(controls, sizeof(controls)));

	if (lastAnswer[line] == kViewKeyTaken)
	{
		viewKeys[line] = key;
		Config_SetString(kHeading, kKeySettings[line], std::to_string(key));
		Config_Save();
	}
}

bool ViewSettings_LineIsHeldByAControl(int line)
{
	if (line < 0 || line >= kViewKeyLines || viewKeys[line] == KEY_VOID)
	{
		return false;
	}

	unsigned char controls[kControlsInASet * 6];

	EveryControl(controls);

	return ViewKey_IsInList(viewKeys[line], controls, sizeof(controls));
}

bool ViewSettings_KeyIsOnALine(int key)
{
	return key != KEY_VOID && (key == viewKeys[kViewKeyRendering] || key == viewKeys[kViewKeyShadows] || key == viewKeys[kViewKeyDevViews]);
}

bool ViewSettings_KeyPressed(int line)
{
	if (line < 0 || line >= kViewKeyLines)
	{
		return false;
	}

	// Every key here has been through the rules, so it's inside the game's table of keys
	int const key = viewKeys[line];
	unsigned char controls[kControlsInASet * 2];

	TheControlsInUse(controls);

	return ViewKey_Acts(key, DebouncedKeyboardInput[key] != 0, TheRules(controls, sizeof(controls)));
}
