// The numbers for the Predator's staff, which the game takes from MelPyth M5 by Mupp. Only how it behaves was read from that
// mod, and this is written new. Written 2026-11-06 by Melanikus for the AvP port.
//
// How much it hurts and how far it reaches are in its ammunition row in equipment.cpp: 20 cutting, 20 penetrating, 4200.
//
// This file knows nothing about the game, so that tests/staff_test.cpp can try all of it.

#pragma once

// Times in the game's numbers, where 65536 is a second
enum
{
	kStaffSwapInTime = 65536,            // bringing it up
	kStaffSwapOutTime = 0x12333,         // putting it away, a little over a second
	kStaffSwingTime = 0x17000,           // one swing, nearly a second and a half
	kStaffThrowTime = 65536 * 2,         // the whole throwing move
	kStaffStandTime = 65536 * 5,         // the slow move of just standing with it
	kStaffFidgetTime = 65536 * 4,        // now and then, something to do with his hands
	kStaffGlideTime = 65536 / 16,        // gliding into a new move from the last one
	kStaffFlightTime = 65536 * 10,       // how long a thrown staff stays in the air before it's gone
	kStaffStayTime = 65536 * 20          // how long it stays once it has landed, or is in someone
};

// How fast a thrown staff leaves him. A rocket goes at much the same speed.
enum
{
	kStaffThrowSpeed = 60000,
	kStaffBackOffALanding = 100     // how far a landing staff comes back from what it hit, in millimetres, to be in the open
};

// The three stages of a thrown staff's life. weapon_staff.cpp says what each one means.
enum
{
	kThrownStaffFlying = 0,
	kThrownStaffLanded = 1,
	kThrownStaffInABody = 2
};

// A thrown staff reacts to what's on its path this frame, plus a little more so that it can't stop just short of it for ever.
inline bool ThrownStaff_ReachesIt(int distanceToIt, int lengthOfThisStep)
{
	return distanceToIt < lengthOfThisStep + 10;
}

// What a save file says about a thrown staff. A file can say anything, so these are checked before it's believed.
// Its stage has to be one of the three, and its time left has to be time that it could really have left.
inline bool ThrownStaff_SavedStageIsGood(int stage, int timeLeft)
{
	if (stage == kThrownStaffFlying)
	{
		return timeLeft > 0 && timeLeft <= kStaffFlightTime;
	}

	if (stage == kThrownStaffLanded || stage == kThrownStaffInABody)
	{
		return timeLeft > 0 && timeLeft <= kStaffStayTime;
	}

	return false;
}

// No place, speed or direction in the game is anywhere near this big: the game itself stops trusting a place past 1000000.
inline bool ThrownStaff_SavedNumberIsGood(int number)
{
	return number > -1000000 && number < 1000000;
}

// A swing is one of two moves, chosen at random: an odd number gives the second.
inline bool Staff_UseSecondSwing(int randomNumber)
{
	return (randomNumber & 1) != 0;
}
