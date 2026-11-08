// The sniper rifle's thinking: how far its scope is zoomed, how much that slows the player, and what it does next.
// Written 2026-11-06 by Melanikus for the AvP port. Behaviour worked out from Mupp's M5 version of the game.
// Modified 2026-11-07 by Melanikus for the AvP port:
//   declarations written so the security scanner can read this file (CODING-STANDARD.md, rule 1)
//
// This file knows nothing about the rest of the game, so tests/weapon_sniper_test.cpp can try every path through it
// without a level, a model or a graphics card. weapon_sniper.cpp is what joins it to the game.
//
// HOW THE RIFLE IS USED:
//   - Tap secondary fire: the scope comes up, at no zoom.
//   - Hold secondary fire: the scope comes up and zooms in for as long as it's held, up to 20 times. Let go and it stays there.
//   - Press secondary fire with the scope up: it zooms out and comes down.
//   - Each shot needs the bolt worked. If the scope's up it comes down first, and goes back up to the same zoom afterwards.
//     So that a shot can be watched land, the scope stays up until the trigger's let go.
//   - A reload also brings the scope down and puts it back afterwards.
//   - The further it's zoomed, the slower the player moves.

#ifndef WEAPON_SNIPER_SUMS_H
#define WEAPON_SNIPER_SUMS_H

// How fast the zoom moves, in times-magnified per second, and how far it goes
static float const kSniperZoomInSpeed = 10.0f;
static float const kSniperZoomQuickSpeed = 70.0f;
static float const kSniperLeastZoom = 1.0f;
static float const kSniperMostZoom = 20.0f;

// How much of the player's normal speed is left at full zoom, in the game's numbers, where 65536 is all of it
static int const kSniperFullZoomMoving = 13107;    // a fifth
static int const kSniperFullZoomTurning = 6553;    // a tenth
static int const kSniperFullZoomJumping = 32768;   // a half

// What the rifle can be busy doing. "Idle" and the reload and putting-away steps are the game's own for every weapon.
enum SniperStep
{
	kSniperIdle,
	kSniperWorkingBolt,        // 0.7 seconds, and a spent case flies out if there is one
	kSniperScopeRising,        // 0.1 seconds
	kSniperScopeLowering,      // 0.1 seconds
	kSniperWaitingForTrigger,  // the shot's over, and the scope stays up until the trigger's let go
	kSniperZoomingIn,          // for as long as secondary fire is held
	kSniperZoomingBack,        // quickly back to the zoom it had
	kSniperZoomingOut,         // quickly down to no zoom
	kSniperReloading,
	kSniperPuttingAway
};

// All the rifle has to remember. It goes into a saved game just as it is, so it's plain numbers, and 1 or 0 for yes or no.
struct SniperRifleState
{
	float zoom;                  // how many times magnified, now
	float zoomToGoBackTo;        // the zoom the player chose
	int scopeIsUp;
	int scopeShouldComeBack;     // it came down for a bolt or a reload, and goes back up afterwards
	int afterLowering;           // what the scope is coming down for: one of the steps
	int spentCaseToThrow;        // the bolt has a case to throw out. After a reload from empty, there's none in there.
	int reloadBeganEmpty;        // so a bolt has to be worked to get the first round in
	int boltOwedWhenDrawn;       // it was put away before that bolt could be worked
};

static inline void SniperRifle_Start(struct SniperRifleState *rifle)
{
	rifle->zoom = kSniperLeastZoom;
	rifle->zoomToGoBackTo = kSniperLeastZoom;
	rifle->scopeIsUp = false;
	rifle->scopeShouldComeBack = false;
	rifle->afterLowering = kSniperIdle;
	rifle->spentCaseToThrow = false;
	rifle->reloadBeganEmpty = false;
	rifle->boltOwedWhenDrawn = false;
}

// ---- The zoom. frameTime is in the game's numbers, where 65536 is one second.

static inline float SniperZoom_In(float zoom, int frameTime)
{
	zoom += kSniperZoomInSpeed * (float)frameTime / 65536.0f;
	return (zoom > kSniperMostZoom) ? kSniperMostZoom : zoom;
}

static inline float SniperZoom_Out(float zoom, int frameTime)
{
	zoom -= kSniperZoomQuickSpeed * (float)frameTime / 65536.0f;
	return (zoom < kSniperLeastZoom) ? kSniperLeastZoom : zoom;
}

static inline float SniperZoom_Back(float zoom, float zoomToGoBackTo, int frameTime)
{
	zoom += kSniperZoomQuickSpeed * (float)frameTime / 65536.0f;
	return (zoom > zoomToGoBackTo) ? zoomToGoBackTo : zoom;
}

// What the game's camera zoom should be: one over how many times magnified
static inline float SniperZoom_ViewScale(float zoom)
{
	return 1.0f / zoom;
}

// A zoom read from a saved game can't be trusted. Written this way round, a number that isn't a number fails the test too.
static inline float SniperZoom_FromSave(float zoom)
{
	if (zoom >= kSniperLeastZoom && zoom <= kSniperMostZoom)
	{
		return zoom;
	}

	return kSniperLeastZoom;
}

// A saved game's copy of the rifle's state: is every number in it one the rifle could really have?
static inline bool SniperRifle_StateIsGood(struct SniperRifleState const *rifle)
{
	bool const zoomsAreGood = SniperZoom_FromSave(rifle->zoom) == rifle->zoom && SniperZoom_FromSave(rifle->zoomToGoBackTo) == rifle->zoomToGoBackTo;

	bool const yesesAndNosAreGood = (rifle->scopeIsUp == 0 || rifle->scopeIsUp == 1)
		&& (rifle->scopeShouldComeBack == 0 || rifle->scopeShouldComeBack == 1)
		&& (rifle->spentCaseToThrow == 0 || rifle->spentCaseToThrow == 1)
		&& (rifle->reloadBeganEmpty == 0 || rifle->reloadBeganEmpty == 1)
		&& (rifle->boltOwedWhenDrawn == 0 || rifle->boltOwedWhenDrawn == 1);

	// The scope only comes down for these
	bool const reasonIsGood = rifle->afterLowering == kSniperIdle || rifle->afterLowering == kSniperWorkingBolt
		|| rifle->afterLowering == kSniperReloading || rifle->afterLowering == kSniperPuttingAway;

	return zoomsAreGood && yesesAndNosAreGood && reasonIsGood;
}

// How much of a speed is left at this zoom: all of "free" at no zoom, down to "fullZoom" at the most zoom, evenly between
static inline int SniperZoom_Slowed(int free, int fullZoom, float zoom)
{
	float const howFar = (zoom - kSniperLeastZoom) / (kSniperMostZoom - kSniperLeastZoom);

	return free - (int)((float)(free - fullZoom) * howFar);
}

// How many rounds a reload takes from his spares, which this rifle counts singly: enough to fill it, or as many as he has. The
// rounds are in the game's numbers, where 65536 is one round, and the spares are whole rounds. A saved game can hold any of
// these, so a gun with more in it than it should hold takes none, rather than a number below nothing.
static inline int Sniper_RoundsToLoad(unsigned int roundsInGun, unsigned int fullMagazine, int spares)
{
	unsigned int const holds = fullMagazine >> 16;
	unsigned int const has = roundsInGun >> 16;
	int const room = (has >= holds) ? 0 : (int)(holds - has);

	if (spares <= 0)
	{
		return 0;
	}

	return (room < spares) ? room : spares;
}

// ---- May this step end yet? The ones with a timer just run out, so they aren't asked about here.

static inline bool SniperStep_MayEnd(struct SniperRifleState const *rifle, enum SniperStep step, bool primaryHeld, bool secondaryHeld)
{
	switch (step)
	{
		case kSniperWaitingForTrigger:
			return !primaryHeld;

		case kSniperZoomingIn:
			return !secondaryHeld;

		case kSniperZoomingBack:
			return rifle->zoom == rifle->zoomToGoBackTo;

		case kSniperZoomingOut:
			return rifle->zoom == kSniperLeastZoom;

		default:
			return true;
	}
}

// ---- What comes next, when something ends or the player asks for something

// The recoil from a shot has ended
static inline enum SniperStep Sniper_AfterRecoil(struct SniperRifleState *rifle)
{
	if (rifle->scopeIsUp)
	{
		return kSniperWaitingForTrigger;
	}

	rifle->spentCaseToThrow = true;
	return kSniperWorkingBolt;
}

// The trigger's been let go with the scope up: down it comes for the bolt, and it goes back up afterwards
static inline enum SniperStep Sniper_AfterWaitingForTrigger(struct SniperRifleState *rifle)
{
	rifle->spentCaseToThrow = true;
	rifle->scopeShouldComeBack = true;
	rifle->afterLowering = kSniperWorkingBolt;
	return kSniperZoomingOut;
}

// A reload is about to begin. If the scope's up it comes down first, and the reload starts again after that.
static inline enum SniperStep Sniper_BeforeReload(struct SniperRifleState *rifle, bool roundsLeftInTheGun)
{
	if (rifle->scopeIsUp)
	{
		rifle->scopeShouldComeBack = true;
		rifle->afterLowering = kSniperReloading;
		return kSniperZoomingOut;
	}

	rifle->reloadBeganEmpty = !roundsLeftInTheGun;
	return kSniperReloading;
}

// A reload has ended, and a change of weapon may be waiting
static inline enum SniperStep Sniper_AfterReload(struct SniperRifleState *rifle, bool changingWeapon)
{
	if (changingWeapon)
	{
		if (rifle->reloadBeganEmpty)
		{
			rifle->boltOwedWhenDrawn = true;
			rifle->reloadBeganEmpty = false;
		}

		return kSniperPuttingAway;
	}

	if (rifle->reloadBeganEmpty)
	{
		rifle->reloadBeganEmpty = false;
		rifle->spentCaseToThrow = false;
		return kSniperWorkingBolt;
	}

	return rifle->scopeShouldComeBack ? kSniperScopeRising : kSniperIdle;
}

// The player's changing weapon. If the scope's up it comes down first.
static inline enum SniperStep Sniper_BeforePuttingAway(struct SniperRifleState *rifle)
{
	if (rifle->scopeIsUp)
	{
		rifle->afterLowering = kSniperPuttingAway;
		return kSniperZoomingOut;
	}

	return kSniperPuttingAway;
}

// The rifle has been brought out. Asks whether it owes a bolt from a reload it was put away after.
static inline enum SniperStep Sniper_WhenDrawn(struct SniperRifleState *rifle)
{
	if (rifle->boltOwedWhenDrawn)
	{
		rifle->boltOwedWhenDrawn = false;
		rifle->spentCaseToThrow = false;
		return kSniperWorkingBolt;
	}

	return kSniperIdle;
}

// The scope has finished rising
static inline enum SniperStep Sniper_AfterScopeRose(struct SniperRifleState *rifle, bool secondaryHeld)
{
	rifle->scopeIsUp = true;

	if (rifle->scopeShouldComeBack)
	{
		return kSniperZoomingBack;
	}

	if (secondaryHeld)
	{
		return kSniperZoomingIn;
	}

	rifle->zoomToGoBackTo = kSniperLeastZoom;
	return kSniperIdle;
}

// Secondary fire's been let go
static inline enum SniperStep Sniper_AfterZoomingIn(struct SniperRifleState *rifle)
{
	rifle->zoomToGoBackTo = rifle->zoom;
	return kSniperIdle;
}

// It's back to no zoom, so the scope comes down
static inline enum SniperStep Sniper_AfterZoomingOut(struct SniperRifleState *rifle)
{
	rifle->scopeIsUp = false;
	return kSniperScopeLowering;
}

// The scope's down, and this is what it came down for
static inline enum SniperStep Sniper_AfterScopeLowered(struct SniperRifleState *rifle)
{
	const SniperStep next = (enum SniperStep)rifle->afterLowering;
	rifle->afterLowering = kSniperIdle;
	return next;
}

// With nothing else going on: the scope goes back up if it should, or secondary fire was just pressed. If the gun's empty
// and there's another magazine, the reload comes first, because it would only bring the scope straight back down.
static inline enum SniperStep Sniper_WhenIdle(struct SniperRifleState *rifle, bool secondaryJustPressed, bool reloadIsDue)
{
	if (reloadIsDue)
	{
		return kSniperIdle;
	}

	if (rifle->scopeShouldComeBack && !rifle->scopeIsUp)
	{
		return kSniperScopeRising;
	}

	if (!secondaryJustPressed)
	{
		return kSniperIdle;
	}

	if (rifle->scopeIsUp)
	{
		rifle->scopeShouldComeBack = false;
		rifle->afterLowering = kSniperIdle;
		return kSniperZoomingOut;
	}

	return kSniperScopeRising;
}

// Each frame, the zoom moves as the step it's in says. Every other step leaves it where it is.
static inline void Sniper_MoveZoom(struct SniperRifleState *rifle, enum SniperStep step, int frameTime)
{
	switch (step)
	{
		case kSniperZoomingIn:
			rifle->zoom = SniperZoom_In(rifle->zoom, frameTime);
			break;

		case kSniperZoomingBack:
			rifle->zoom = SniperZoom_Back(rifle->zoom, rifle->zoomToGoBackTo, frameTime);
			break;

		case kSniperZoomingOut:
			rifle->zoom = SniperZoom_Out(rifle->zoom, frameTime);
			break;

		default:
			break;
	}
}

#endif
