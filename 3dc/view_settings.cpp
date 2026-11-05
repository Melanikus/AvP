// The rendering page of the menus, and the settings it keeps. Written 2026-11-05 by Melanikus for the AvP port.
//
// WHAT THE PLAYER GETS: three lines in the menus: how the world is lit, whether things cast shadows, and whether maps are worked out for textures with none. A new player gets the
// game as it has always looked, with all the new drawing off, and it's only when they choose otherwise that any of it is built. What they choose is kept in AliensVsPredator.cfg,
// not in their profile, because it belongs to this computer and this graphics card, not to who's playing.
//
// THE FILE CAN SAY ANYTHING: it's a text file anyone can edit, so only the exact words count. Anything else is treated as if it wasn't there, the log says so, and what it said is
// never repeated there.
//
// ONE WAY IN: the menu doesn't change how anything is drawn. It asks, the same way the LIGHTING and SHADOWS commands do, and the change happens at the one safe moment in a frame:
// see ViewSwitches_ApplyAsked in d3d_render.cpp. The commands only change this run. Only the menu saves.

#include "3dc.h"
#include "ConfigFile.h"
#include "logString.h"
#include "texture_relief.h"
#include "view_settings.h"
#include "view_switches.h"

// In d3d_render.cpp, beside the rest of what happens to these two switches
void ViewSwitches_AskLighting(int choice);
void ViewSwitches_AskShadows(int on);
bool ViewSwitches_SomethingAsked();
void ViewSwitches_SetAtStart(int lighting, int volumes);
void ViewSwitches_WhatsThere(bool *mapsThere, bool *perPixelThere);

static const char kHeading[] = "[Video]";
static const char kRendering[] = "Rendering";
static const char kShadows[] = "Shadows";
static const char kAutoGenerate[] = "AutoGenerateMaps";

int MenuRendering = kLightingGouraud;
int MenuShadows = 0;
int MenuAutoGenerate = 0;

// What the page was showing at the end of the last menu frame. Anything different has been changed by the player.
static int lastRendering = kLightingGouraud;
static int lastShadows = 0;
static int lastAutoGenerate = 0;

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

			ViewSwitches_WhatsThere(&mapsThere, &perPixelThere);

			lastRendering = MenuRendering = Lighting_MenuStep(lastRendering, MenuRendering, mapsThere, perPixelThere);
			ViewSwitches_AskLighting(lastRendering);
			Config_SetString(kHeading, kRendering, Lighting_Word(lastRendering));
			changed = true;
		}
		else
		{
			lastRendering = MenuRendering = LightingNow;
		}

		if (MenuShadows != lastShadows)
		{
			lastShadows = MenuShadows;
			ViewSwitches_AskShadows(lastShadows);
			Config_SetBool(kHeading, kShadows, lastShadows != 0);
			changed = true;
		}
		else
		{
			lastShadows = MenuShadows = ShadowVolumesOn;
		}
	}

	// Turning it off takes effect at once, and on from the next level, because maps are only worked out while a level loads. TextureRelief knows that, and the page says so.
	if (MenuAutoGenerate != lastAutoGenerate)
	{
		lastAutoGenerate = MenuAutoGenerate;
		textureRelief.SetAutoGenerate(lastAutoGenerate != 0);
		Config_SetBool(kHeading, kAutoGenerate, lastAutoGenerate != 0);
		changed = true;
	}

	if (changed)
	{
		Config_Save();
	}
}
