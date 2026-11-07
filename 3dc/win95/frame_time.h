// A rule for how long a frame can count as. Written 2026-11-06 by Melanikus for the AvP port.
//
// WHY THIS IS HERE: NormalFrameTime is how long the last frame took, and 65536 is one second. Code all over the game
// divides by it, and bh_particle_generator.cpp divides by it with nothing to catch a 0, which stops the game. A very
// short frame in slow motion can come out as 0: the debug TIMESCALE command scales the frame's length, and a small enough
// scale leaves nothing of it. A scale below zero would make it negative. The original code says it relies on this never
// being 0, in ResetFrameCounter in io.cpp, so this makes that true.
//
// This file knows nothing about the game, so tests/circle_tables_test.cpp can try it.

#ifndef FRAME_TIME_H
#define FRAME_TIME_H

// The shortest a frame may count as: one part in 65536 of a second.
inline int FrameTime_NeverNothing(int frameTime)
{
	return frameTime < 1 ? 1 : frameTime;
}

#endif
