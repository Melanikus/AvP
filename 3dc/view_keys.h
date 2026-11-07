// The rules for the keys on the rendering page of the menus: which key each of its lines may have. Written 2026-11-05 by Melanikus for the AvP port.
//
// WHY THERE ARE RULES: pressing one key shouldn't do two jobs, and the settings file can't be trusted, because anyone can edit it. So a line only gets a key that nothing else has. That's asked when the
// player chooses a key in the menu, and when the game reads the settings file.
//
// WHEN ONE KEY STILL ENDS UP WITH TWO JOBS: a player can give one of these keys to a control afterwards, on a controls page, or can have a profile older than these lines. Then the control wins, and the
// line's key does nothing. The menus show that key in yellow, on both pages, so that it doesn't happen without them saying so.
//
// This file needs nothing from the rest of the game, so that tests/view_keys_test.cpp can try every case. view_settings.cpp is what joins it to the game. A key here is the game's number for that key,
// from KEY_ID in platform.h, the same number a player's profile keeps.

#ifndef VIEW_KEYS_H
#define VIEW_KEYS_H

// The page's lines, in the order they're shown
enum ViewKeyLine
{
	kViewKeyRendering,   // goes on to the next way of lighting the world
	kViewKeyShadows,     // turns shadows on and off
	kViewKeyDevViews,    // goes on to the next developer's view. Only with -dev, and never in a network game
	kViewKeyLines
};

// What became of a key someone tried to give a line
enum ViewKeyAnswer
{
	kViewKeyTaken,            // the line has it now
	kViewKeyNotAKey,          // no key on any keyboard has this number
	kViewKeyTheGameKeepsIt,   // the game reads this key for itself, whatever the controls say
	kViewKeyIsOnAnotherLine,  // another line on the page has it
	kViewKeyIsAControl        // one of the player's controls has it
};

// What the game knows that these rules need
struct ViewKeyRules
{
	int noKey;                       // the number that means a line has no key
	int lastRealKey;                 // the highest number a real key can have
	const unsigned char *gameKeeps;  // the keys the game reads for itself
	int gameKeepsCount;
	const unsigned char *controls;   // the keys the player's controls have
	int controlsCount;
};

bool ViewKey_IsInList(int key, const unsigned char *list, int count);

// May this line have this key, given the keys the page's lines have now? Having no key is always allowed.
ViewKeyAnswer ViewKey_Judge(int line, int key, const int keys[kViewKeyLines], const ViewKeyRules &rules);

// Reads a key's number from the settings file. Only plain digits count, with no spaces, no sign and no zeros in front, up to 255. Anything else gives back false and leaves key alone.
bool ViewKey_FromSavedWord(const char *word, int *key);

// Works out every line's key from what the settings file said, one line at a time from the top. A line whose word isn't a number, or whose key it may not have, goes back to its usual key. If something
// above has taken that too, it has no key. wasRefused says which lines were sent back, so that the log can say so and the file can be put right.
void ViewKey_SettleSaved(const char *const words[kViewKeyLines], const int usual[kViewKeyLines], const ViewKeyRules &rules, int keys[kViewKeyLines], bool wasRefused[kViewKeyLines]);

// During a level: does pressing this line's key do something? Not if the line has no key, and not if a control has the same key, because a player's control always wins.
bool ViewKey_Acts(int key, bool pressed, const ViewKeyRules &rules);

#endif
