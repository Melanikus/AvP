// Checks on what a save file says. Written 2026-11-04 by Melanikus for the AvP port.
// save_checks.cpp explains why they're here.
#ifndef SAVE_CHECKS_H
#define SAVE_CHECKS_H

struct dynamicsblock;

// Are these the numbers of a game the player could really have saved?
bool Save_SlotNumbersAreValid(int species, int episode, int difficulty);

// Puts a saved dynamics block over an object's own, keeping out what a save must not set. live can be empty, and then nothing happens.
void Save_RestoreDynamics(struct dynamicsblock *live, const struct dynamicsblock *saved);

#endif
