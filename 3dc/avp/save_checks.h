// Checks on what a save file says. Written 2026-11-04 by Melanikus for the AvP port.
// save_checks.cpp explains why they're here.
#ifndef SAVE_CHECKS_H
#define SAVE_CHECKS_H

struct dynamicsblock;

// Are these the numbers of a game the player could really have saved?
bool Save_SlotNumbersAreValid(int species, int episode, int difficulty);

// Is this number one of the player's weapon slots? And a weapon's state, a vision mode, a kind of ammunition?
bool Save_WeaponSlotIsValid(int slot);
bool Save_WeaponStateIsValid(int state);
bool Save_VisionModeIsValid(int mode);
bool Save_AmmoIsValid(int ammo);

// A weapon being put away says which slot comes next. Can its state go with that number? Each is fine alone, but "no swap going on" isn't a slot.
bool Save_SwapFitsWeaponState(int state, int swapSlot);

// A saved place in a list of count entries, where -1 means none. Any other number that isn't in the list comes back as -1 too.
int Save_PlaceInListOrNone(int place, int count);

// Is this block a fixed part of fixedSize bytes and then a name, which fills the rest of the block and has its end mark? The length is only read
// if the block is big enough to hold it.
bool Save_NameFillsBlock(int blockSize, int fixedSize, const int *nameLength, const char *name);

// The frame a morph should be on: the saved one, or its own if the saved one isn't between 0 and maxFrames.
int Save_MorphFrameOrOwn(int saved, int own, int maxFrames);

// Is this one of the sequences in a texture animation's list? The list can be empty, and then nothing is.
bool Save_TextureSequenceIsValid(const int *animationList, int sequence);

// Is this number between these two, both included?
bool Save_NumberIsInRange(int number, int lowest, int highest);

// Puts a saved dynamics block over an object's own, keeping out what a save must not set. live can be empty, and then nothing happens.
void Save_RestoreDynamics(struct dynamicsblock *live, const struct dynamicsblock *saved);

// A target a loader found by a name from the save: kept only if it has a place in the world (its dynamics), which the NPC or the disc
// reads every frame it has a target. The name can be anything in the level's, such as a placed sound's, which has none. Nothing goes in,
// nothing comes out; a target with no place is counted as refused.
struct strategyblock *Save_TargetWithAPlace(struct strategyblock *found);

// WHAT WAS REFUSED: a game the player really saved has to load exactly as it did, so on a real save, every check has to pass. These count the ones that don't, for one line in the log at the end of each load, and
// that line is how a test with real saves shows that a check is too strict. check is a fixed piece of text naming it. A block the wrong size isn't counted: that's the game's own test, as old as the game.
void Save_StartCountingRefusals();
void Save_NoteRefused(const char *check);

// The line for the log, cut short if it doesn't fit.
void Save_DescribeRefusals(char *words, int size);

#endif
