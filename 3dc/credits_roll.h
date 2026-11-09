// When the credits roll stops early. Written 2026-11-08 by Melanikus for the AvP port (CREDITS-1).
//
// The credits roll (DoCredits in menus.cpp) runs until the text has gone by, or until the player presses something. "Something" used to
// mean the moment the game's one "any key" flag went from off to on. That flag is on while any key, button or gamepad button is held,
// so one that's held all along, or that reads as held (a gamepad button stuck down, a key from the menu not let go), keeps it on, and
// then nothing else pressed could ever stop the roll. So now each key is looked at on its own: the roll stops as soon as any one of them
// is pressed that wasn't pressed the frame before, whatever else is held.
//
// F8 takes a screenshot, and the game has never counted it as "any key" (DirectReadKeyboard in di_func.cpp), so it doesn't stop the roll.
//
// This file touches nothing in the game, so tests/credits_roll_test.cpp can try any keys.
#ifndef CREDITS_ROLL_H
#define CREDITS_ROLL_H

// Whether a key was pressed this frame that wasn't pressed last frame, other than leaveOut. justPressed has one entry for each key, how
// DirectReadKeyboard writes DebouncedKeyboardInput: not 0 for a key pressed this frame and not the one before. It reads only the first
// count entries, and none if there's no list.
inline bool CreditsRoll_AKeyWasJustPressed(const unsigned char *justPressed, int count, int leaveOut)
{
	if (!justPressed)
	{
		return false;
	}

	for (int key = 0; key < count; key++)
	{
		if (justPressed[key] && key != leaveOut)
		{
			return true;
		}
	}

	return false;
}

#endif
