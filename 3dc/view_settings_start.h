// How the world is drawn when the game starts: what a new settings file gets, and what a file from an earlier build gets, once. Written 2026-11-08
// by Melanikus for the AvP port.
//
// THE RULING: "after release beta.0 everyone starts with shadows on and pixel shaders" (the user, 2026-11-08, for beta.1). So:
//   - A NEW SETTINGS FILE starts with the lighting per pixel and the shadows on.
//   - A FILE FROM AN EARLIER BUILD, which started everyone with both off, gets them switched on once, the first time this build starts. The
//     file then says which version of these settings it has had, so it never happens again.
//   - AFTER THAT, the player's own choice is kept. A player who turns them off keeps them off.
//
// HOW AN EARLIER FILE IS KNOWN: by its VideoSettingsVersion line. No line means a file from before there was one, so version 0. A new file has no
// line either, but it has no Rendering line, because every build since the rendering page (2026-11-05) writes one the first time it starts.
//
// A CARD THAT CAN'T DRAW THEM: that's decided as each frame is drawn, not here. A card that can't draw lighting per pixel draws the game's own
// (Lighting_Drawn, in view_switches.h), and one with no stencil draws no shadows (ViewSwitches_ApplyAsked, in d3d_render.cpp). Asking for them is
// always safe.
//
// THE FILE CAN SAY ANYTHING: it's a text file anyone can edit. A version line that isn't a plain number is refused, the log says so, and nothing is
// switched on, because a file that has a line at all has been through this build's start before.
//
// This file holds only the decisions, with nothing of the game in it, so that tests/view_settings_start_test.cpp can run them. view_settings.cpp
// reads the file and does what these say.

#ifndef VIEW_SETTINGS_START_H
#define VIEW_SETTINGS_START_H

#include "view_switches.h"

// The version of these settings this build writes. A file with a smaller number gets each change since, once. When a later build adds one, it
// gets the next number and a line here.
//   1: 2026-11-08, beta.1. Lighting per pixel and shadows switched on.
int const kViewSettingsVersion = 1;

// The most a version line may say: four digits
int const kViewSettingsMostVersion = 9999;

// What a saved version line says. Only digits count, and at most four of them: a sign, a space, a fraction or anything after the number makes it
// not a number at all, and then this gives back false and leaves version alone. A word of any length is read only as far as its fifth character.
inline bool ViewSettings_VersionFromSavedWord(char const *word, int *version)
{
	if (!word || !*word)
	{
		return false;
	}

	int number = 0;
	int digits = 0;

	for (char const *character = word; *character; character++)
	{
		if (*character < '0' || *character > '9' || digits == 4)
		{
			return false;
		}

		number = number * 10 + (*character - '0');
		digits++;
	}

	*version = number;
	return true;
}

// What happened at the start, for the log
enum ViewSettingsStart
{
	kViewSettingsNewFile,      // no Rendering line: a new file, which starts with the lighting per pixel and the shadows on
	kViewSettingsSwitchedOn,   // a file from before version 1: the lighting per pixel and the shadows switched on, this once
	kViewSettingsAsSaved       // a file that has had this already: as the player left it
};

// The lighting and the shadows the game starts with. fileHadRendering says whether the file had a Rendering line, savedVersion is what its
// version line said (0 for no line), and lighting and shadows come in as the file had them and go out as the game should start. Lighting already
// per pixel, or better (normals, relief), is left as it is: switching on never takes a player down a step.
inline ViewSettingsStart ViewSettings_LightsAtStart(bool fileHadRendering, int savedVersion, int *lighting, bool *shadows)
{
	if (savedVersion >= kViewSettingsVersion)
	{
		return kViewSettingsAsSaved;
	}

	if (!fileHadRendering)
	{
		*lighting = kLightingPerPixel;
		*shadows = true;
		return kViewSettingsNewFile;
	}

	if (*lighting == kLightingGouraud)
	{
		*lighting = kLightingPerPixel;
	}

	*shadows = true;
	return kViewSettingsSwitchedOn;
}

#endif
