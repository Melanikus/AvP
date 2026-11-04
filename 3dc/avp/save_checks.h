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

// A saved place in a list of count entries, where -1 means none. Any other number that isn't in the list comes back as -1 too.
int Save_PlaceInListOrNone(int place, int count);

// Is this block a fixed part of fixedSize bytes and then a name, which fills the rest of the block and has its end mark? The length is only read
// if the block is big enough to hold it.
bool Save_NameFillsBlock(int blockSize, int fixedSize, const int *nameLength, const char *name);

// The frame a morph should be on: the saved one, or its own if the saved one isn't between 0 and maxFrames.
int Save_MorphFrameOrOwn(int saved, int own, int maxFrames);

// Is this one of the sequences in a texture animation's list? The list can be empty, and then nothing is.
bool Save_TextureSequenceIsValid(const int *animationList, int sequence);

// Puts a saved dynamics block over an object's own, keeping out what a save must not set. live can be empty, and then nothing happens.
void Save_RestoreDynamics(struct dynamicsblock *live, const struct dynamicsblock *saved);

#endif
