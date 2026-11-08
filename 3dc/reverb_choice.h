// How loud the room echo (reverb) comes out. Written 2026-11-08 by Melanikus for the AvP port (REVERB-TOGGLE-1, issue #93). Reported
// by Bobblen; the setting to turn it off follows the idea in his avp-forever (commit ccb453b), rewritten.
//
// Each level says how much echo each of its rooms adds to sounds: an amount from 0 (none) to 1 (all of it), or -1, which leaves it to
// distance. The original game gave every sound that amount. This port's OpenAL code kept the number and never used it, so every room
// had its full echo, whatever the level asked for. Now the one place all the echo goes through, OpenAL's effect slot, is turned up or
// down to the amount. That's the same as giving each sound the amount, because the game uses one amount at a time: the one for the room
// the player is in.
//
// The player can also turn the echo off: the Room echo row on the sound pages, kept as [Audio] Reverb in the settings file. And the log
// says when the player goes into a room with a different echo, so a run can tell where it was.

#pragma once

#include <stdio.h>

#include <string>

// The setting's place in the settings file
static const char kReverbHeading[] = "[Audio]";
static const char kReverbName[] = "Reverb";

// The amount the game keeps for a room: what the level asks for if it's from 0 to 1, and otherwise -1, which leaves it to distance. The
// amount comes from a level file, which anyone could have changed (Tao 3), so any other number counts as -1 too, and something that isn't
// a number fails the test, so it does as well. Kept this way, a room always gives the same number, so a change of room can be told by
// comparing with what was kept. (Compared as it came, a -5 or a not-a-number differs from the -1 kept for it every time.)
static inline float Reverb_RoomAmount(float levelAmount)
{
	return (levelAmount >= 0.0f && levelAmount <= 1.0f) ? levelAmount : -1.0f;
}

// How loud the echo comes out, from 0 (none) to 1 (all of it). reverbOn is the player's setting, and levelAmount is what the level asks
// for in the room the player is in.
static inline float Reverb_SlotGain(bool reverbOn, float levelAmount)
{
	if (!reverbOn)
	{
		return 0.0f;
	}

	// An amount from 0 to 1 is used as it is. -1 leaves it to distance: the echo is used in full, and a far sound's echo is quieter
	// because the sound itself is quieter.
	float const kept = Reverb_RoomAmount(levelAmount);
	return (kept < 0.0f) ? 1.0f : kept;
}

// The 26 kinds of room, in the order of the game's list of echo settings (Sound_Enviroments in openal_audio.cpp, whose length is
// checked against this). A level picks one by its number.
static const int kReverbRoomKinds = 26;

static inline const char *Reverb_RoomName(unsigned int room)
{
	static const char *const names[kReverbRoomKinds] = {
		"generic", "padded cell", "room", "bathroom", "living room", "stone room", "auditorium", "concert hall", "cave", "arena",
		"hangar", "carpeted hallway", "hallway", "stone corridor", "alley", "forest", "city", "mountains", "quarry", "plain",
		"parking lot", "sewer pipe", "underwater", "drugged", "dizzy", "psychotic"
	};

	return (room < (unsigned int)kReverbRoomKinds) ? names[room] : "unknown";
}

// The log's line when the player goes into a room with a different echo, so a run can say where it was: the kind of room, its number,
// and the amount kept for it (Reverb_RoomAmount), with -1 said as "full"
static inline std::string Reverb_RoomLine(unsigned int room, float amount, bool reverbOn)
{
	char number[16] = "full";

	if (amount >= 0.0f && amount <= 1.0f)
	{
		snprintf(number, sizeof(number), "%.2f", amount);
	}

	return std::string("sound: room echo now ") + Reverb_RoomName(room) + " (" + std::to_string(room) + ") at " + number
		+ (reverbOn ? "" : ", but Room echo is off, so none is heard");
}
