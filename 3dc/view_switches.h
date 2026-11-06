// The two things a player can change while playing: how the world is lit, and whether shadows are drawn. Written 2026-11-05 by Melanikus for the AvP port.
//
// LIGHTING has four choices, and SHADOWS is on or off. Either can be set whatever the other says, so there are eight ways the world can look.
// SHADOWAMBIENT is a third word, and only changes which lights cast those shadows: see light_choice.h. It goes the same way as the other two.
// Both are console commands, so they can go on any key: BIND L LIGHTING. With -dev they also have keys of their own, L and H.
//
// WHEN THEY CHANGE: the drawing code reads these many times a frame, and a command can arrive at any moment. So a command only notes what it was asked for, and the
// change happens in one place, ViewSwitches_ApplyAsked in d3d_render.cpp, after one frame's shadows and before the next frame's lights. No frame sees half of a
// change. Only one thing is noted for each, so a key that's bound twice still changes things once a press. A batch file can easily leave a key bound twice.
//
// EVERYTHING OFF: with Gouraud and no shadows, the view itself goes to OFF, which is exactly how the game has always drawn. Turning either back on returns to the
// view it left, as long as nothing else has moved it. A new player starts here: see view_settings.cpp, which keeps what they choose.
//
// G, FOR DEVELOPERS: G picks what the shadow pass shows. LIGHTING and SHADOWS decide what its FINAL view draws, and the other views are for looking inside the pass, so
// they show what they're named for. With everything off, FINAL and OFF would be the same thing, so G goes straight past FINAL.
//
// This file holds only the decisions, with nothing of the game in it, so that tests/view_switches_test.cpp can run them.

#ifndef VIEW_SWITCHES_H
#define VIEW_SWITCHES_H

#include <stddef.h>

enum LightingChoice
{
	kLightingGouraud,   // the game's own lighting, worked out at each polygon's corners and blended across it. Where a new player starts, and -shadowsoff
	kLightingPerPixel,  // lights are added per pixel, on flat surfaces. Where -reliefoff starts
	kLightingNormals,   // lights are added per pixel, and each pixel's map tilts which way it faces
	kLightingRelief,    // the same, and a ray marched through each pixel's height makes the surface stand out from its polygon. See Lighting_WordsOnScreen for when it can't
	kLightingChoices
};

extern int LightingNow;      // one of the choices above. It's what was asked for, which a card may not be able to draw: see Lighting_Drawn
extern int ShadowVolumesOn;  // 1 draws shadows, 0 doesn't
extern int ShadowAmbientOn;  // 1 lets the map's lights with no corona cast shadows too, 0 doesn't. See light_choice.h

// The two commands, for the console to call
void ShadowsCommand(char *words);
void LightingCommand(char *words);
void ShadowAmbientCommand(char *words);

// For developers, to judge a depth map or a normal map on the spot: which way round is it, and how deep should it be? They do nothing without -dev, or in a network game, and the tag shows them.
//   RELIEFFLIP          swaps which of black and white is the polygon's level, in the depth map
//   TILTFLIP            swaps bumps and dents, in the normal map
//   RELIEFDEPTH 0.08    how deep the depth map's deepest part is, as a fraction of a picture's width
void ReliefFlipCommand();
void TiltFlipCommand();
void ShadowEvenGapCommand(char *words);
void ReliefDepthCommand(char *words);

extern bool ReliefHeightFlipped;
extern bool ReliefTiltFlipped;
extern float ReliefDepthNow;

float const kReliefDepthAtStart = 0.06f;
float const kReliefDepthMost = 0.2f;     // Past about this the walk's 16 steps are too far apart to land on anything thin.

// A depth somebody typed. Anything that isn't a number between the ends comes back as the nearest end, and a thing that isn't a number at all as no depth. So this is what makes
// the shader's depth safe to use, whatever is typed.
inline float ReliefDepth_Clamped(float asked)
{
	if (!(asked > 0.0f))
	{
		return 0.0f;
	}

	return asked < kReliefDepthMost ? asked : kReliefDepthMost;
}

// What a command can leave noted for the end of the frame. Anything else is the setting asked for: 0 or 1 for SHADOWS, or a LightingChoice.
enum
{
	kAskedNothing = -1,
	kAskedNext = -2     // SHADOWS: the opposite of what it is now. LIGHTING: the next choice along
};

inline bool SwitchWords_IsSpace(char c)
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

// Leaves out any spaces around the words. A line from a batch file can easily end with some.
inline void SwitchWords_Trim(char const *words, char const **start, size_t *length)
{
	while (SwitchWords_IsSpace(*words))
	{
		words++;
	}

	size_t size = 0;

	while (words[size])
	{
		size++;
	}

	while (size > 0 && SwitchWords_IsSpace(words[size - 1]))
	{
		size--;
	}

	*start = words;
	*length = size;
}

// Whether the words are this one, in capitals or not. wanted is in capitals.
inline bool SwitchWords_Are(char const *start, size_t length, char const *wanted)
{
	size_t i = 0;

	for (; i < length; i++)
	{
		char const c = start[i];
		char const capital = (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;

		if (!wanted[i] || capital != wanted[i])
		{
			return false;
		}
	}

	return wanted[i] == 0;
}

// The words after a command's name come from whoever typed them, or from a file, so only these exact ones count, and anything else is refused.
// A number isn't read with the usual functions, because to those "ABC" is 0, which would turn something off.
// Each of these gives back false for words it doesn't know, and then leaves asked as it was.

// SHADOWS: nothing switches it, 0 is off, and 1 is on
inline bool SwitchWords_ShadowsAsk(char const *words, int *asked)
{
	if (!words)
	{
		return false;
	}

	char const *start;
	size_t length;
	SwitchWords_Trim(words, &start, &length);

	if (length == 0)
	{
		*asked = kAskedNext;
	}
	else if (SwitchWords_Are(start, length, "0"))
	{
		*asked = 0;
	}
	else if (SwitchWords_Are(start, length, "1"))
	{
		*asked = 1;
	}
	else
	{
		return false;
	}

	return true;
}

// SHADOWAMBIENT: nothing switches it, and OFF or 0 is off, and ON or 1 is on. Shadows_AfterAsking below works out what that comes to.
inline bool SwitchWords_ShadowAmbientAsk(char const *words, int *asked)
{
	if (!words)
	{
		return false;
	}

	char const *start;
	size_t length;
	SwitchWords_Trim(words, &start, &length);

	if (length == 0)
	{
		*asked = kAskedNext;
	}
	else if (SwitchWords_Are(start, length, "0") || SwitchWords_Are(start, length, "OFF"))
	{
		*asked = 0;
	}
	else if (SwitchWords_Are(start, length, "1") || SwitchWords_Are(start, length, "ON"))
	{
		*asked = 1;
	}
	else
	{
		return false;
	}

	return true;
}

// LIGHTING: nothing goes to the next choice. The numbers are their places in the cycle, counting from 1, and the names work too.
inline bool SwitchWords_LightingAsk(char const *words, int *asked)
{
	if (!words)
	{
		return false;
	}

	char const *start;
	size_t length;
	SwitchWords_Trim(words, &start, &length);

	if (length == 0)
	{
		*asked = kAskedNext;
	}
	else if (SwitchWords_Are(start, length, "1") || SwitchWords_Are(start, length, "GOURAUD"))
	{
		*asked = kLightingGouraud;
	}
	else if (SwitchWords_Are(start, length, "2") || SwitchWords_Are(start, length, "PERPIXEL"))
	{
		*asked = kLightingPerPixel;
	}
	else if (SwitchWords_Are(start, length, "3") || SwitchWords_Are(start, length, "NORMALS"))
	{
		*asked = kLightingNormals;
	}
	else if (SwitchWords_Are(start, length, "4") || SwitchWords_Are(start, length, "RELIEF"))
	{
		*asked = kLightingRelief;
	}
	else
	{
		return false;
	}

	return true;
}

inline int Shadows_AfterAsking(int now, int asked)
{
	if (asked == kAskedNothing)
	{
		return now;
	}

	if (asked == kAskedNext)
	{
		return now ? 0 : 1;
	}

	return asked ? 1 : 0;
}

// Whether this choice reads the maps
inline bool Lighting_UsesMaps(int choice)
{
	return choice == kLightingNormals || choice == kLightingRelief;
}

// Whether this choice can be drawn right now. The game's own lighting always can. mapsThere says whether the pictures that hold the maps, and what draws them, are all there. marchThere
// says whether the shader that marches through a height picture could be made, which only Relief needs.
inline bool Lighting_IsThere(int choice, bool mapsThere, bool perPixelThere, bool marchThere = true)
{
	return choice == kLightingGouraud
		|| (choice == kLightingPerPixel && perPixelThere)
		|| (choice == kLightingNormals && perPixelThere && mapsThere)
		|| (choice == kLightingRelief && perPixelThere && mapsThere && marchThere);
}

// What really gets drawn when this is what was asked for
inline int Lighting_Drawn(int asked, bool mapsThere, bool perPixelThere, bool marchThere = true)
{
	if (Lighting_IsThere(asked, mapsThere, perPixelThere, marchThere))
	{
		return asked;
	}

	// Relief with no march still has its maps, so it's as near as it can get
	if (asked == kLightingRelief && perPixelThere && mapsThere)
	{
		return kLightingNormals;
	}

	return perPixelThere ? kLightingPerPixel : kLightingGouraud;
}

// What LightingNow becomes. If a choice asked for isn't there, it stays as it was.
//
// When a card can't draw per pixel, a press can't change anything anyway, so it also leaves what was asked for alone. That can be true for just a moment around a device
// reset, and this way the lighting a player chose comes back afterwards.
inline int Lighting_AfterAsking(int now, int asked, bool mapsThere, bool perPixelThere, bool marchThere = true)
{
	if (asked == kAskedNothing)
	{
		return now;
	}

	if (asked != kAskedNext)
	{
		return Lighting_IsThere(asked, mapsThere, perPixelThere, marchThere) ? asked : now;
	}

	if (!perPixelThere)
	{
		return now;
	}

	// From what's on the screen, so that a press never lands on what's already there. Gouraud is always there, so this always finds one.
	int choice = Lighting_Drawn(now, mapsThere, perPixelThere, marchThere);

	do
	{
		choice = (choice + 1) % kLightingChoices;
	}
	while (!Lighting_IsThere(choice, mapsThere, perPixelThere, marchThere));

	return choice;
}

// What the command calls each choice
inline char const *Lighting_Word(int choice)
{
	return (choice == kLightingRelief) ? "RELIEF" : (choice == kLightingNormals) ? "NORMALS" : (choice == kLightingPerPixel) ? "PERPIXEL" : "GOURAUD";
}

// What a saved setting calls a choice. Only the exact words count: no numbers, and nothing at all isn't a choice, because this comes from a file anyone can edit. Gives back false,
// and leaves choice alone, for anything else.
inline bool Lighting_FromSavedWord(char const *words, int *choice)
{
	if (!words)
	{
		return false;
	}

	char const *start;
	size_t length;
	SwitchWords_Trim(words, &start, &length);

	for (int candidate = 0; candidate < kLightingChoices; candidate++)
	{
		if (SwitchWords_Are(start, length, Lighting_Word(candidate)))
		{
			*choice = candidate;
			return true;
		}
	}

	return false;
}

// Where the menu's line goes, when the player has moved it from last to pressed. The line goes round and round, one step at a time, so it can only have gone one way. A choice that
// can't be drawn is passed over in that same direction, so the player can always get to everything that is there. Gouraud always is, so this always finds one.
inline int Lighting_MenuStep(int last, int pressed, bool mapsThere, bool perPixelThere, bool marchThere = true)
{
	int const step = ((last + 1) % kLightingChoices == pressed) ? 1 : kLightingChoices - 1;
	int choice = ((pressed % kLightingChoices) + kLightingChoices) % kLightingChoices;

	for (int tried = 0; tried < kLightingChoices; tried++)
	{
		if (Lighting_IsThere(choice, mapsThere, perPixelThere, marchThere))
		{
			return choice;
		}

		choice = (choice + step) % kLightingChoices;
	}

	return kLightingGouraud;
}

// What the screen calls it. Relief can be chosen and still not be marching: its shader may not have been made, or something on this frame stops it, such as a mirror or a vision mode. Then it
// draws what NORMALS draws, and it says so instead of claiming to be something it isn't.
inline char const *Lighting_WordsOnScreen(int choice, bool marching)
{
	return (choice == kLightingRelief && !marching) ? "RELIEF, DRAWN AS NORMALS" : Lighting_Word(choice);
}

// What the view becomes after the switches have changed. It only moves when everything goes off, or stops being off. If G has moved it away from OFF in between, it stays
// where G put it.
inline int View_AfterSwitches(int viewNow, int viewOff, bool wereAllOff, bool areAllOff, int *viewToComeBackTo)
{
	if (areAllOff && !wereAllOff)
	{
		if (viewNow != viewOff)
		{
			*viewToComeBackTo = viewNow;
		}

		return viewOff;
	}

	if (wereAllOff && !areAllOff && viewNow == viewOff)
	{
		return *viewToComeBackTo;
	}

	return viewNow;
}

// The view G moves on to. See the top of this file for why it can go past FINAL.
inline int View_AfterG(int viewNow, int viewCount, int viewFinal, bool allOff)
{
	int const next = (viewNow + 1) % viewCount;

	return (next == viewFinal && allOff) ? (next + 1) % viewCount : next;
}

#endif
