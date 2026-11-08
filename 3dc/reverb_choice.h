// How loud the room echo (reverb) comes out. Written 2026-11-08 by Melanikus for the AvP port (REVERB-TOGGLE-1, issue #93). Reported
// by Bobblen; the setting to turn it off follows the idea in his avp-forever (commit ccb453b), rewritten.
//
// Each level says how much echo each of its rooms adds to sounds: an amount from 0 (none) to 1 (all of it), or -1, which leaves it to
// distance. The original game gave every sound that amount. This port's OpenAL code kept the number and never used it, so every room
// had its full echo, whatever the level asked for. Now the one place all the echo goes through, OpenAL's effect slot, is turned up or
// down to the amount. That's the same as giving each sound the amount, because the game uses one amount at a time: the one for the room
// the player is in.
//
// The player can also turn the echo off: the Room echo row on the sound pages, kept as [Audio] Reverb in the settings file.

#pragma once

// The setting's place in the settings file
static const char kReverbHeading[] = "[Audio]";
static const char kReverbName[] = "Reverb";

// How loud the echo comes out, from 0 (none) to 1 (all of it). reverbOn is the player's setting, and levelAmount is what the level asks
// for in the room the player is in.
static inline float Reverb_SlotGain(bool reverbOn, float levelAmount)
{
	if (!reverbOn)
	{
		return 0.0f;
	}

	// An amount from 0 to 1 is used as it is
	if (levelAmount >= 0.0f && levelAmount <= 1.0f)
	{
		return levelAmount;
	}

	// -1 leaves it to distance: the echo is used in full, and a far sound's echo is quieter because the sound itself is quieter. The
	// amount comes from a level file, which anyone could have changed (Tao 3), so any other number counts as -1 too. Something that isn't
	// a number at all fails both tests above, so it ends up here as well.
	return 1.0f;
}
