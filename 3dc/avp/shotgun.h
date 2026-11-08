// The sums for the marine's shotgun, which the game takes from MelPyth M5 by Mupp. Only how the gun behaves was read from that mod, and this is written new. Written 2026-11-06 by Melanikus for the AvP port.
//
// WHAT IT DOES: one press fires 32 pellets at once, each making its own hit. Each goes a little off from where he's aiming, up or down and left or right, and each of the two is chosen at random. Eleven pellets stay close to the middle, eleven go wider and ten go widest, so the shot makes a tight group with a looser ring round it. M5 fires 12 at 10 impact and 10 penetrating each; this game chose more, weaker pellets, for a fuller spray with about the same damage if they all land.
//
// THE SLOWDOWN: firing slows him right down, to a quarter of his speed, for moving, turning and jumping. That's as M5 has it, and it's kept on purpose: this is what makes the gun feel heavy.
//
// LOADING IT: shells go in one at a time, until the gun's full, or he has no more, or he presses fire. See weapon_shotgun.cpp.
//
// THE STOCK: secondary fire opens the stock, for a shot at longer range. He looks along the gun, so the view zooms in a little, the pellets stay closer together, and he moves more slowly. Secondary fire again closes it.
//
// This file knows nothing about the game, so that tests/shotgun_test.cpp can try all of it.

#pragma once

// How many pellets one shot fires, in its three rings from tightest to widest, and how many shells the gun holds
enum
{
	kShotgunPelletsTight = 11,
	kShotgunPelletsMiddle = 11,
	kShotgunPelletsWide = 10,
	kShotgunPellets = kShotgunPelletsTight + kShotgunPelletsMiddle + kShotgunPelletsWide,
	kShotgunShells = 12
};

// What one pellet does, at every difficulty. The table of ammunition in equipment.cpp reads these. Impact is what armour stops; penetrating mostly goes through it.
enum
{
	kShotgunPelletImpact = 4,
	kShotgunPelletPenetrating = 4
};

// A quarter of his speed, in the game's numbers, where 65536 is all of it
enum
{
	kShotgunFiringSlowdown = 65536 / 4
};

// A full circle is this many steps in the game's angles
enum
{
	kAngleStepsInACircle = 4096
};

// How wide each pellet may go, in those steps, from one side of the aim to the other. 100 steps is nearly 9 degrees across, and with the stock open it's half that.
inline int Shotgun_SpreadOfPellet(int pellet, bool stockOpen)
{
	static const int kSpread[3] = { 32, 70, 100 };
	static const int kSpreadStockOpen[3] = { 17, 35, 50 };

	if (pellet < 0 || pellet >= kShotgunPellets)
	{
		return 0;
	}

	int const ring = pellet < kShotgunPelletsTight ? 0 : pellet < kShotgunPelletsTight + kShotgunPelletsMiddle ? 1 : 2;

	return stockOpen ? kSpreadStockOpen[ring] : kSpread[ring];
}

// How far the view is zoomed: 1 is as it always is, and smaller is closer in.
const float kShotgunZoomStockClosed = 1.0f;
const float kShotgunZoomStockOpen = 0.65f;

// How much of that it can change in a second
const float kShotgunZoomChangeEachSecond = 2.0f;

// How fast he goes with the stock open, in the game's numbers, where 65536 is all of it
enum
{
	kShotgunStockOpenMoving = 65536 / 2,
	kShotgunStockOpenTurning = 65536 / 4,
	kShotgunStockOpenJumping = 65536 * 3 / 4
};

// One frame's step of the zoom towards where it's going, without going past
inline float Shotgun_ZoomTowards(float now, float wanted, float secondsThisFrame)
{
	float const step = kShotgunZoomChangeEachSecond * secondsThisFrame;

	if (now < wanted)
	{
		return now + step < wanted ? now + step : wanted;
	}

	return now - step > wanted ? now - step : wanted;
}

// One of a pellet's two turns away from the aim, as an angle for the game to use, from a random number. It's as likely to go one way as the other, so half the spread either side. A turn the wrong way comes out as nearly a whole circle, because that's how the game's angles work.
inline int Shotgun_PelletTurn(int randomNumber, int spread)
{
	if (spread <= 0)
	{
		return 0;
	}

	// As a number with no sign, so that a random number below 0 can't give a remainder below 0
	int const along = (int)((unsigned int)randomNumber % (unsigned int)spread);

	return (along - spread / 2) & (kAngleStepsInACircle - 1);
}

// How many shells go in as one loading state ends. It's one, unless the game is running so slowly that its timer has gone a whole state's length past 0, and then one more for each, so that loading takes the same time however slow the game is. It's never more than he has, or more than there's room for.
// timerLeft is the state's timer, which has reached 0 or gone past it. 65536 is a state's whole length.
inline int Shotgun_ShellsThatGoIn(int timerLeft, int shellsInGun, int spareShells)
{
	int shells = timerLeft < 0 ? 1 + (-(timerLeft / 65536)) : 1;
	int const room = kShotgunShells - shellsInGun;

	if (shells > room)
	{
		shells = room;
	}

	if (shells > spareShells)
	{
		shells = spareShells;
	}

	return shells < 0 ? 0 : shells;
}

// Is that the end of loading? Yes if the gun is full, or he has no more. Pressing fire stops it too, as long as there's a shell in the gun to fire.
inline bool Shotgun_LoadingIsOver(int shellsInGun, int spareShells, bool heWantsToFire)
{
	return shellsInGun >= kShotgunShells || spareShells <= 0 || (heWantsToFire && shellsInGun > 0);
}

// What the shotgun remembers for itself, and what goes into a save: plain numbers, each 0 or 1.
struct ShotgunOwnState
{
	// Was the gun empty when he began to load it? Then it needs its pump worked at the end.
	int EmptyWhenLoadingBegan;

	// Has he pressed fire while loading?
	int StopLoading;

	// Is the stock open?
	int StockOpen;
};

// A save is a file that anybody could have changed, so each number is checked.
inline bool Shotgun_OwnStateIsGood(const ShotgunOwnState &state)
{
	return (state.EmptyWhenLoadingBegan == 0 || state.EmptyWhenLoadingBegan == 1)
		&& (state.StopLoading == 0 || state.StopLoading == 1)
		&& (state.StockOpen == 0 || state.StockOpen == 1);
}
