// How the game reads its sine and cosine tables. Written 2026-11-06 by Melanikus for the AvP port.
//
// WHY THIS IS HERE: a circle in this game is 4096 steps, numbered 0 to 4095, and each table has one entry for each
// step. Step 4096 is the same as step 0, because it's a whole turn. But two places could ask for step 4096 itself, and
// the table has no entry there, so the game read whatever happened to be in memory after the table:
// - an NPC's turn, in NPCOrientateToVector in bh_ais.cpp, when its frame is so short that its turn rounds down to nothing
//   and it's turning left. A xenoborg, which turns slowly, does that above 128 frames a second: its turn is 128 steps a
//   second, and MUL_FIXED drops what's left over, so on a frame shorter than 7.8 ms it comes to nothing (tests/circle_tables_test.cpp
//   works it out from the game's own numbers). Corrected 2026-11-08: this said about 143. In tables.cpp the table written after
//   cosine is ArcCosTable, so if the program keeps that order, step 4096's "cosine" was about 2,000 times too big, and the
//   xenoborg's turn scaled it up by that each frame. That may be the picture breaking when a xenoborg attacks, which Bobblen's
//   avp-forever reports (#96).
// - the player's turn in pmove.cpp, which adds one step to a turn right, so that a turn already at step 4095 comes to 4096.
//
// So these two wrap any number round the circle first. Step 4096 reads step 0, step -1 reads step 4095, and no number at
// all can read outside a table.
//
// This file knows nothing about the game, so tests/circle_tables_test.cpp can try it with the real tables.

#ifndef CIRCLE_TABLES_H
#define CIRCLE_TABLES_H

// All the bits of a step number from 0 to 4095. Keeping only these wraps a number round the circle, negative ones too.
#define kCircleStepMask 4095

#define GetSin(a) sine[(a) & kCircleStepMask]
#define GetCos(a) cosine[(a) & kCircleStepMask]

#endif
