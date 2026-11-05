// The two things a player can change while playing: how the world is lit, and whether shadows are drawn. Written 2026-11-05 by Melanikus for the AvP port.
//
// LIGHTING has three choices, and SHADOWS is on or off. Either can be set whatever the other says, so there are six ways the world can look.
// Both are console commands, so they can go on any key: BIND L LIGHTING. With -dev they also have keys of their own, L and H.
//
// WHEN THEY CHANGE: the drawing code reads these many times a frame, and a command can arrive at any moment. So a command only notes what it was asked for, and the
// change happens in one place, ViewSwitches_ApplyAsked in d3d_render.cpp, after one frame's shadows and before the next frame's lights. No frame sees half of a
// change. Only one thing is noted for each, so a key that's bound twice still changes things once a press. A batch file can easily leave a key bound twice.
//
// EVERYTHING OFF: with Gouraud and no shadows, the view itself goes to OFF, which is exactly how the game has always drawn. Turning either back on returns to the
// view it left, as long as nothing else has moved it.
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
	kLightingRelief,    // lights are added per pixel, on surfaces given depth by their relief pictures. The game starts here
	kLightingPerPixel,  // lights are added per pixel, on flat surfaces. Where -reliefoff starts
	kLightingGouraud,   // the game's own lighting, worked out at each polygon's corners and blended across it. Where -shadowsoff starts
	kLightingChoices
};

extern int LightingNow;      // one of the choices above. It's what was asked for, which a card may not be able to draw: see Lighting_Drawn
extern int ShadowVolumesOn;  // 1 draws shadows, 0 doesn't

// The two commands, for the console to call
void ShadowsCommand(char *words);
void LightingCommand(char *words);

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

// LIGHTING: nothing goes to the next choice. The numbers are their places in the cycle, and the names work too.
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
	else if (SwitchWords_Are(start, length, "1") || SwitchWords_Are(start, length, "RELIEF"))
	{
		*asked = kLightingRelief;
	}
	else if (SwitchWords_Are(start, length, "2") || SwitchWords_Are(start, length, "PERPIXEL"))
	{
		*asked = kLightingPerPixel;
	}
	else if (SwitchWords_Are(start, length, "3") || SwitchWords_Are(start, length, "GOURAUD"))
	{
		*asked = kLightingGouraud;
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

// Whether this choice can be drawn right now. The game's own lighting always can.
inline bool Lighting_IsThere(int choice, bool reliefThere, bool perPixelThere)
{
	return choice == kLightingGouraud
		|| (choice == kLightingPerPixel && perPixelThere)
		|| (choice == kLightingRelief && perPixelThere && reliefThere);
}

// What really gets drawn when this is what was asked for
inline int Lighting_Drawn(int asked, bool reliefThere, bool perPixelThere)
{
	if (Lighting_IsThere(asked, reliefThere, perPixelThere))
	{
		return asked;
	}

	return perPixelThere ? kLightingPerPixel : kLightingGouraud;
}

// What LightingNow becomes. If a choice asked for isn't there, it stays as it was.
//
// When a card can't draw per pixel, a press can't change anything anyway, so it also leaves what was asked for alone. That can be true for just a moment around a device
// reset, and this way the lighting a player chose comes back afterwards.
inline int Lighting_AfterAsking(int now, int asked, bool reliefThere, bool perPixelThere)
{
	if (asked == kAskedNothing)
	{
		return now;
	}

	if (asked != kAskedNext)
	{
		return Lighting_IsThere(asked, reliefThere, perPixelThere) ? asked : now;
	}

	if (!perPixelThere)
	{
		return now;
	}

	// From what's on the screen, so that a press never lands on what's already there. Gouraud is always there, so this always finds one.
	int choice = Lighting_Drawn(now, reliefThere, perPixelThere);

	do
	{
		choice = (choice + 1) % kLightingChoices;
	}
	while (!Lighting_IsThere(choice, reliefThere, perPixelThere));

	return choice;
}

inline char const *Lighting_Word(int choice)
{
	return (choice == kLightingRelief) ? "RELIEF" : (choice == kLightingPerPixel) ? "PERPIXEL" : "GOURAUD";
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
