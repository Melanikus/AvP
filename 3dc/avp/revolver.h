// The revolver's cylinder, as plain sums with nothing from the rest of the game in them, so that tests/revolver_test.cpp can run them.
// Written 2026-11-06 by Melanikus for the AvP port. The game takes the gun from MelPyth M5 by Mupp. Only how it behaves was read from that mod, and this is written new.
//
// WHAT THIS DESCRIBES
//
// The gun holds 5 rounds in 5 chambers, in a ring. Each chamber can be empty, hold a live round, or hold the spent case of a round that's been fired. A spent
// case stays in its chamber until the gun is reloaded, as it would in a real revolver.
//
// The cylinder turns one way as rounds are loaded and the other way as they're fired, so the rounds behave like a stack: the last one loaded is the first one
// fired. "place" is the chamber the next round loaded goes into, and the next shot comes from the chamber just behind it.
//
// NUMBERS WITH A POINT IN THEM
//
// The game keeps time and rounds as whole numbers with 65536 meaning 1, so half a second is 32768. Those are called "fixed" here.

#pragma once

enum { kRevolverChambers = 5 };

// 65536 in the game's numbers is 1 of whatever it's counting.
enum { kRevolverOne = 65536 };

// How much firing slows him down: a quarter of his usual speed, while the gun settles.
enum { kRevolverFiringSlowdown = kRevolverOne / 4 };

// How far along an animation's clock its very last moment is. The game's model files never quite reach 65536.
enum { kRevolverLastMoment = 65530 };

// What's in the gun, as it's saved with the game.
struct RevolverCylinder
{
	int place;                          // the chamber the next round loaded goes into, from 0 to 4
	int somethingIn[kRevolverChambers]; // 1 if a chamber holds a round or a spent case
	int live[kRevolverChambers];        // 1 if what's in a chamber hasn't been fired
};

// Moves a place round the ring. Going past the last chamber comes back to the first, and the other way round.
inline int Revolver_PlaceForward(int place, int steps)
{
	return (((place + steps) % kRevolverChambers) + kRevolverChambers) % kRevolverChambers;
}

inline int Revolver_PlaceBack(int place, int steps)
{
	return Revolver_PlaceForward(place, -(steps % kRevolverChambers));
}

// Every chamber empty. The cylinder doesn't turn.
inline void Revolver_EmptyChambers(RevolverCylinder *cylinder)
{
	for (int i = 0; i < kRevolverChambers; i++)
	{
		cylinder->somethingIn[i] = 0;
		cylinder->live[i] = 0;
	}
}

// Sets the chambers from how many rounds the gun holds: that many live rounds, in the chambers just behind the place, so the next shot comes from one of them.
// The number can come from a saved game, so anything outside what the gun can hold is treated as the nearest it can.
inline void Revolver_FillFromRounds(RevolverCylinder *cylinder, int rounds)
{
	Revolver_EmptyChambers(cylinder);

	for (int i = 1; i <= kRevolverChambers && i <= rounds; i++)
	{
		int const chamber = Revolver_PlaceBack(cylinder->place, i);

		cylinder->somethingIn[chamber] = 1;
		cylinder->live[chamber] = 1;
	}
}

// The chamber the next shot comes from.
inline int Revolver_ChamberToFire(RevolverCylinder const *cylinder)
{
	return Revolver_PlaceBack(cylinder->place, 1);
}

// A shot has gone: the cylinder turns back one place, and what's in that chamber is now a spent case. It stays where it is.
inline void Revolver_MarkShot(RevolverCylinder *cylinder)
{
	cylinder->place = Revolver_ChamberToFire(cylinder);
	cylinder->live[cylinder->place] = 0;
}

// One round has gone into the chamber at the place, and the cylinder turns on to the next.
inline void Revolver_MarkLoaded(RevolverCylinder *cylinder)
{
	cylinder->somethingIn[cylinder->place] = 1;
	cylinder->live[cylinder->place] = 1;
	cylinder->place = Revolver_PlaceForward(cylinder->place, 1);
}

// How many live rounds the chambers hold, to compare with the count the game keeps.
inline int Revolver_LiveRounds(RevolverCylinder const *cylinder)
{
	int rounds = 0;

	for (int i = 0; i < kRevolverChambers; i++)
	{
		rounds += cylinder->live[i] ? 1 : 0;
	}

	return rounds;
}

// Is there a spent case to throw out? If not, a reload has no need to tip out the rounds still in the gun.
inline bool Revolver_HasSpentCase(RevolverCylinder const *cylinder)
{
	for (int i = 0; i < kRevolverChambers; i++)
	{
		if (cylinder->somethingIn[i] && !cylinder->live[i])
		{
			return true;
		}
	}

	return false;
}

enum RevolverComesOut
{
	kRevolverNothingComesOut,
	kRevolverSpentCaseComesOut,
	kRevolverLiveRoundComesOut
};

// What falls out of a chamber when the gun is tipped open. A live round looks different from a spent case, so the game throws out a different shape for each.
inline RevolverComesOut Revolver_WhatComesOut(RevolverCylinder const *cylinder, int chamber)
{
	if (cylinder->live[chamber])
	{
		return kRevolverLiveRoundComesOut;
	}

	return cylinder->somethingIn[chamber] ? kRevolverSpentCaseComesOut : kRevolverNothingComesOut;
}

// Live rounds that fall out go back with the spare rounds. The spare count is a single byte in the player's data, so it stops at the most a byte can hold.
inline int Revolver_SpareAfterTippingOut(int spare, int rounds)
{
	int const most = 255;

	if (rounds < 0)
	{
		rounds = 0;
	}

	return (spare >= most - rounds) ? most : spare + rounds;
}

// How many rounds go in at the end of one loading movement. Usually one. A loading movement lasts 0.35 seconds, so if one frame took longer than that, the
// counter has run further past 0 and a round goes in for each whole second of it. It can't be more than there are spares, or more than there's room for.
//
// counter is the state's clock, which has just run out, so it's 0 or below. roundsInGun and mostInGun are fixed. spare is a plain count.
inline int Revolver_RoundsToLoad(int counter, int roundsInGun, int mostInGun, int spare)
{
	int const overrun = (counter < 0) ? -(counter / kRevolverOne) : 0;
	int const room = (mostInGun - roundsInGun) / kRevolverOne;
	int rounds = 1 + overrun;

	if (rounds > room)
	{
		rounds = room;
	}

	if (rounds > spare)
	{
		rounds = spare;
	}

	return (rounds < 0) ? 0 : rounds;
}

// Should the gun load another round? Not if it's full or there's nothing left to load. And not if the player has asked for something else: firing, the scope
// or another weapon all stop the loading after the round that's going in now.
inline bool Revolver_KeepsLoading(int roundsInGun, int mostInGun, int spare, bool stopAsked)
{
	return (roundsInGun < mostInGun) && (spare > 0) && !stopAsked;
}

// A loading movement's animation is driven by the state's clock, which counts down from 65536 to 0, so a slow frame can't leave the hand behind the rounds.
// This turns the clock into how far along the animation is, from 0 to its last moment.
inline int Revolver_AnimationTimeFromClock(int counter)
{
	if (counter <= 0)
	{
		return kRevolverLastMoment;
	}

	if (counter >= kRevolverOne)
	{
		return 0;
	}

	return (int)(((long long)kRevolverLastMoment * (kRevolverOne - counter)) / kRevolverOne);
}

// How many more rounds should appear in the cylinder now. loaded is how many have finished going in during this reload, and shown is how many are on screen.
// The round in the hand appears in its chamber when the animation gets to the moment it goes in, which is before the sums count it as loaded.
inline int Revolver_RoundsToShow(int loaded, int shown, int animationTime, int momentItGoesIn)
{
	if (loaded < shown)
	{
		return 0;
	}

	return (loaded - shown) + ((animationTime >= momentItGoesIn) ? 1 : 0);
}

// The cylinder on the model turns by an animation that makes one whole turn over its full length, so each chamber is a fifth of the way along it.
inline int Revolver_CylinderAnimationTime(int place)
{
	return (Revolver_PlaceForward(place, 0) * kRevolverLastMoment) / kRevolverChambers;
}

// Moves the view's zoom towards a target at a steady rate, and stops on it. Both zooms are ordinary numbers, 1 being no zoom. secondsThisFrame is how long the frame took.
inline float Revolver_ZoomTowards(float zoom, float target, float ratePerSecond, float secondsThisFrame)
{
	float const step = ratePerSecond * secondsThisFrame;

	if (zoom < target)
	{
		return (zoom + step > target) ? target : zoom + step;
	}

	return (zoom - step < target) ? target : zoom - step;
}

// A saved game comes from a file, so nothing in it is trusted. Every place has to be a chamber, every flag has to be 0 or 1, and a chamber can't hold a live
// round with nothing in it.
inline bool Revolver_SavedCylinderIsGood(RevolverCylinder const *cylinder)
{
	if (cylinder->place < 0 || cylinder->place >= kRevolverChambers)
	{
		return false;
	}

	for (int i = 0; i < kRevolverChambers; i++)
	{
		if (cylinder->somethingIn[i] != 0 && cylinder->somethingIn[i] != 1)
		{
			return false;
		}

		if (cylinder->live[i] != 0 && cylinder->live[i] != 1)
		{
			return false;
		}

		if (cylinder->live[i] && !cylinder->somethingIn[i])
		{
			return false;
		}
	}

	return true;
}
