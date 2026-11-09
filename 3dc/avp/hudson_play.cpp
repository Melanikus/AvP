// Hudson playing a level on foot. hudson_play.h says what it's for, and hudson_play_steps.h holds the thinking that needs no game.
//
// HOW IT GOES. He has a list of steps for the level. For each one he finds his way on the 300 mm grid, walks there, and does what the step says, the way
// anyone would: the use key, looking at a switch; walking onto something; firing the gun in his hands. If something that can hurt him comes into sight, he deals
// with that first. The level ends when the game says so, and nothing here ever says so.
//
// HOW HE WALKS. The way is a line, and he follows the line: each frame he finds where he is along it and heads for a place a metre further on, with forward and sidestep together, so that his
// feet go where the line goes whichever way he's facing, and his view swings round after them. Only a way that's far from him is looked for again.
//
// WHAT NEVER HAPPENS HERE: no switch or door is ever told anything directly. If he gets through a level with no cheats, a player can.
//
// CHEATS. The point of watching him is to see the whole level work, so he's kept going when a player's feet or aim wouldn't be enough. He's hopped along his way when he's
// stuck, or put at the step when the grid knows no way there or a step takes too long. He's kept alive when his health runs out. What he can't kill in time is killed for him.
// Each of these is a "play: cheat" line in the log, saying where he was and why, and the run's result counts them. A level finished with any cheats ends with its own exit
// code, because it doesn't show a player can finish it. It does show where a player, or this walker, would have stopped. Being stuck isn't proof a player would be.
//
// A STEP THAT DOESN'T WORK doesn't stop him, because there may be another way. It's counted, and the log says why, and the end of the run says how many there were.

#include "3dc.h"
#include "inline.h"
#include "module.h"
#include "strategy_def.h"
#include "gamedef.h"
#include "logString.h"
#include "bh_types.h"
#include "dynblock.h"
#include "dynamics.h"
#include "bh_alien.h"
#include "pvisible.h"
#include "weapons.h"
#include "pldghost.h"
#include "bh_platform_lift.h"
#include "hudson.h"
#include "hudson_grid.h"
#include "hudson_nav.h"
#include "hudson_strategies.h"
#include "hudson_play.h"
#include "hudson_play_steps.h"
#include "hudson_play_ribbon.h"

#include <stdio.h>
#include <map>
#include <string>
#include <vector>

extern DISPLAYBLOCK *Player;
extern int NormalFrameTime;
extern int NumActiveStBlocks;
extern STRATEGYBLOCK *ActiveStBlockList[];
extern VIEWDESCRIPTORBLOCK *Global_VDB_Ptr;
extern int IsThisObjectVisibleFromThisPosition_WithIgnore(DISPLAYBLOCK *objectPtr, DISPLAYBLOCK *ignoredObjectPtr, VECTORCH *positionPtr, int maxRange);
extern void GetTargetingPointOfObject_Far(STRATEGYBLOCK *sbPtr, VECTORCH *targetPtr); // targeting.cpp: where the game's own soldiers aim at someone, the chest if there is one
extern void Hudson_MoveHimOnFoot(const VECTORCH &place); // hudson.cpp
extern DAMAGE_PROFILE certainDeath; // equipment.cpp: more damage than anything can take
extern bool Coop_LevelChangeIsDue(void); // pldnet.cpp
extern void Coop_LevelChangeHasStarted(void);

// pmove.cpp's TURNSCALE: how fast he turns with the control at full
static const int kTurnsAtFull = 2000;

// The use key works on what's near the middle of his view, up to 3 m ahead and 1 m to either side (triggers.cpp). If that's further, the log says so.
static const int kUseKeyReachesAhead = 3000;

// How close counts as there, in mm
static const int kPickedUpFrom = 400;
static const int kShootFrom = 6000;
static const int kFightsFrom = 12000;
static const int kClawsReach = 2500;

// How long each thing gets, in milliseconds
static const DWORD kWaitForTheLevelToEnd = 60000;
static const DWORD kGiveUpOnAKill = 30000;
static const DWORD kGiveUpOnAFight = 30000;
static const DWORD kGiveUpOnShootingSomething = 20000;
static const DWORD kAStepHasTakenTooLong = 180000;
static DWORD aStepHasTakenTooLong = kAStepHasTakenTooLong;   // the same, or stepseconds='s (HudsonPlay_SetStepSeconds)
static const DWORD kLeastTimeBetweenFillingHisHealth = 1000;
static const DWORD kStandOnAPlaceFor = 1500;
static const DWORD kWaitToPickUp = 6000;
static const DWORD kTimeBetweenPresses = 2000;
static const int kMostPresses = 3;
static const DWORD kRescueLasts = 500;
static const DWORD kSayWhereHeIsEvery = 10000;
static const int kPlacesToSearchEachFrame = 3000;

// How far above the floor he's put when he's moved, in mm, so that he's never put through it. He drops the rest of the way.
static const int kPutAboveTheFloor = 100;

// How far off his way he has to be, in mm, before he looks for a new way from where he is, and how often he does that. A fight or a fall can leave him far from it.
static const int kLookAgainIfThisFarOff = 3000;
static const DWORD kLookAgainNoMoreOftenThan = 2000;

// The game's angle for a quarter of a circle, and for an eighth: past the first he's turned a long way from where he's going, and it's over when he's back within the second
static const int kBigTurnIsOver = 1024;
static const int kBigTurnEndsWithin = 512;

// What the ribbon shows. hudson_play_ribbon.h says what it is. The route is where he means to walk and the trail is where he's been. A step he's done goes dark.
static const HudsonPlayRibbonColour kRouteColour = { 255, 170, 30 };
static const HudsonPlayRibbonColour kTrailColour = { 40, 200, 255 };
static const HudsonPlayRibbonColour kStepToDoColour = { 60, 255, 90 };
static const HudsonPlayRibbonColour kStepDoneColour = { 30, 110, 50 };
static const HudsonPlayRibbonColour kCheatColour = { 255, 30, 30 };
static const int kRouteOffTheFloor = 90;   // above the trail, so that the route shows where they lie together
static const int kTrailOffTheFloor = 50;   // high enough off the floor that the two never fight over which is in front
static const int kTrailPointEvery = 500;
static const size_t kTrailMostPoints = 8000;
static const int kStepBlockHalfWidth = 300;
static const int kStepBlockHeight = 1000;
static const int kCheatPostHalfWidth = 200;
static const int kCheatPostHeight = 2500;

// A second Hudson, following the first, side by side, as the user asked: "2 players side by side completing the map". How close is close enough, in mm, how often he looks for him again,
// and how far behind he has to be before he can be moved up to him, which is a cheat. When the first is moved, by a cheat of his own, the second comes too at once: a jump in where the first is,
// further than anyone runs in a frame, is that.
static const int kCloseEnoughToHim = 2500;
static const int kTooFarBehind = 12000;
static const DWORD kLookForHimEvery = 1500;
// A message that comes late, or several that come at once, makes the first Hudson jump on this machine as well, by as far as he ran meanwhile. In the first lift pair a stall made one of
// 8.7 m, and a hop of 18 m came after he'd stood still, so neither speed nor a small distance tells the two apart. Only a jump bigger than any stall makes counts.
static const int kHeWasMovedFurtherThan = 15000;

// The user's rule for the two: "the follower getting close but never on top of him". The second aims for a spot this far behind the first and this far to one side, so that he can see past
// him. He stops walking when he's at that spot or this near the first, and if he's ever nearer than kNeverNearerThan, he steps back. A hop puts him at the same spot, on floor the grid has.
static const int kBehindHimBy = 2000;
static const int kToOneSideBy = 800;
static const int kAtTheSpotWithin = 600;
static const int kStopsWithin = 1500;
static const int kNeverNearerThan = 1000;

// On a lift's car the two keep a metre between their squares. The first stands on the far side of the middle from where he got on, and the second gets on only if there's room for that.
static const int kGapOnACar = 1000;

// The second gets on a car with the first only if he's this near its edge when the first is wholly on it. The car goes 1.5 seconds after the first touches it, so from further away he
// wouldn't be on before it went, and he waits for the next trip instead.
static const int kJoinTheRideFromWithin = 3000;

// When the grid knows no way from the second to the first, but the first is this near and on much the same level, the second walks straight at him, as a player would. A lift's car is such a place.
static const int kWalkStraightAtHimWithin = 8000;
static const int kSameLevelWithin = 1000;

// Waiting for each other, the user's rule: "They should cheat the least amount possible, and if not hard, make them wait for each other". The first stops for the second when the second is
// far behind, and before a switch, until the second is beside him, so that both go through together. He waits this long at most and then goes on. The second is only moved up to him after
// being too far behind for longer than that, so that a wait always comes first.
static const int kWaitWhenFurtherThan = 8000;
static const int kBesideHim = 4000;    // the user's "within about 4 m", before each new leg
static const int kWaitBeforeTheStepWithin = 6000;
static const DWORD kWaitAtMost = 20000;
static const DWORD kWaitAgainAfter = 10000;
static const DWORD kCatchUpAfter = kWaitAtMost + 5000;

// How often each machine looks at the step's switches and the lifts, to say when one changes. The two logs say the same if the two games agree.
static const DWORD kLookAtWhatsWatchedEvery = 250;

// A fall further than this is said, because a fall down a lift's shaft is what the lift test looks for
static const int kSayAFallOfMoreThan = 2000;

// Riding a lift. See "Riding a lift" further down for how it goes.
static const int kMostLifts = 64;                    // more than any stock level has: invasion has 5
static const int kLandingWithin = 500;               // how near in height a landing's floor has to be to where the top of the car stops
static const int kLookForTheLandingPastTheEdge = 700;
static const int kLandingClearOfTheCar = 200;        // between his square and the car's, on a landing
static const int kTheCarIsLevelWithin = 60;          // how near its end the car has to be to count as stopped there. It snaps to its end when it stops
static const int kMostLiftsInARow = 3;               // how many lifts one plan can chain together
static const int kMostRidesForAStep = 6;
static const DWORD kWaitForTheCarAtMost = 40000;
static const DWORD kGetOnAtMost = 6000;
static const DWORD kRideAtMost = 30000;
static const DWORD kGetOffAtMost = 6000;
static const DWORD kTheSecondPlansNoMoreOftenThan = 5000;

// When he has to be put somewhere because the grid knows no way, and a lift goes on from there, he's put this far back from the lift at least, and no further than the second, so that the
// lift itself is still ridden and the cheat isn't at the lift
static const int kPutBackFromALift = 8000;
static const int kPutNoFurtherFromALift = 20000;

// The lift test's lines (AvP-docs, game/lift-test-pass-rules.md): one for each body near a lift this often. Near is above its shaft with this much to spare on every side, and from this far
// over its top stop to this far under its bottom one.
static const DWORD kSayWhereEachIsNearALiftEvery = 100;
static const int kNearALiftAcross = 3000;
static const int kNearALiftUpOrDown = 2000;

enum Phase
{
	kWaitingForTheGrid,
	kStartingAStep,
	kFindingTheWay,
	kWalking,
	kDoingTheStep,
	kFinishedTheList,
	kStopped
};

static std::vector<HudsonPlayStep> steps;
static size_t stepAt = 0;
static Phase phase = kWaitingForTheGrid;
static std::vector<HudsonGridPoint> way;
static size_t wayAt = 0;
static bool wayGoesAllTheWay = false;
static DWORD phaseStartedAt = 0;
static DWORD shootingStartedAt = 0;
static std::string stateBefore;
static int pressesTried = 0;
static DWORD lastPressAt = 0;
static int stepsThatWorked = 0;

static VECTORCH placeLastSeenAt = { 0, 0, 0 };
static DWORD placeLastChangedAt = 0;
static int timesStuckHere = 0;
static HudsonPlayRescue rescue = kHudsonPlayFindTheWayAgain;
static DWORD rescueUntil = 0;
static DWORD saidWhereAt = 0;

static bool following = false;
static const STRATEGYBLOCK *otherPlayer = 0;
static DWORD lookedForHimAt = 0;
static DWORD farBehindSince = 0;      // 0 while he isn't too far behind
static bool walkingStraightAtHim = false;
static bool lookAgainNextFrame = false;   // he's been moved, at the end of this frame, and the way on is looked for from where he lands

// The first Hudson waiting for the second. waitingSince is 0 while he isn't. After a wait that ran out he doesn't wait again until the second has been beside him, or he'd wait for ever.
static DWORD waitingSince = 0;
static bool legWaitIsDue = false;        // the first waits for the second to be beside him before he sets off on a new leg: each step, after a ride, after he's moved
static bool switchWaitIsDue = false;     // and before the step's switch, once
static DWORD firstLastMovedAt = 0;       // when the first Hudson last moved, as this machine sees him
static VECTORCH rideSpot = { 0, 0, 0 };  // where on the car he stands to ride
static bool mayWaitAgain = true;
static DWORD waitRanOutAt = 0;
static int waitsDone = 0;
static DWORD waitedMilliseconds = 0;

// What each machine watches, to say when it changes: the switch or whatever else each step is about, and every lift. The second Hudson has the list only for this.
static std::vector<HudsonPlayStep> watchedSteps;
static std::map<std::string, std::string> watchedStates;
static DWORD lookedAtWhatsWatchedAt = 0;
static bool saidHesOnALift = false;
static bool heWasJustMoved = false;   // the first Hudson was moved by a cheat, and the second hasn't done anything about it yet
static int gunToHold = 0;             // playgun=: the slot of the gun he's given, or 0
static bool gunGiven = false;
static bool gunInHand = false;
static int gunSlotSaid = -2;

// Each platform lift in the level, as the riding needs it. Its car's x and z never change, only its height (bh_platform_lift.cpp). The end at the top is 0 and the bottom 1.
struct LiftEnd
{
	int carY;                     // the car's own y when it's stopped at this end
	int standY;                   // where his feet are on top of it then: the car's y plus the top of its shape
	bool hasLanding;
	HudsonGridPoint landing;      // the floor beside the shaft at this end
};

struct Lift
{
	STRATEGYBLOCK const *car;
	int middleX, middleZ;         // the middle of the car seen from above, from its shape. Its turn, if it has one, is left out
	int halfWidth;                // half of its narrower side
	int halfX, halfZ;             // half of each of its sides: a car is a box, so its corners reach further than halfWidth
	LiftEnd ends[2];
};

static std::vector<Lift> lifts;

// Further down, with the rest of the lifts
static bool OverTheCar(Lift const &lift, VECTORCH const &place, int toSpare = 0);
static bool OverACarThatIsntLevelWith(VECTORCH const &place, int feetY);
static bool liftsAreKnown = false;
static std::map<int, std::vector<uint8_t> > reachedFromALanding;   // what can be walked to from each landing, by lift number times 2 plus the end. The grid never changes, so neither does this

enum RideStage
{
	kNotRiding,
	kWalkingToTheLift,
	kWaitingForTheCar,
	kGettingOn,
	kRiding,
	kGettingOff
};

static RideStage rideStage = kNotRiding;
static int rideLift = -1;
static int rideFrom = 0;              // the end he gets on at
static DWORD rideStageAt = 0;
static bool noLiftHelps = false;      // the last plan found none. The first doesn't plan again on this step, and the second waits a while
static DWORD plannedARideAt = 0;
static int ridesThisStep = 0;
static int ridesDone = 0;

static bool ribbonIsOn = false;
static std::vector<HudsonGridPoint> trail;
static std::vector<HudsonGridPoint> cheatPlaces;

static bool walkedLastFrame = false;
static int offTheWayNow = 0;
static bool onTheWayNow = true;
static int bigTurns = 0;
static bool inABigTurn = false;
static DWORD lookedAgainBecauseOffAt = 0;
// How well he's following his way, for the run's result
static double strayAdded = 0.0;
static int strayFrames = 0;
static int strayMost = 0;
static int stuckTimes = 0;
static DWORD stuckMilliseconds = 0;
static DWORD lastFrameAt = 0;

static DWORD saidThisSecondAt = 0;
static int secondsSaid = 0;
static VECTORCH placeASecondAgo = { 0, 0, 0 };

static int cheatsUsed = 0;
static DWORD stepStartedAt = 0;
static bool movedToThisStep = false;
static bool moveIsWanted = false;
static VECTORCH moveHimTo = { 0, 0, 0 };
static int healthAtTheStart = 0;
static DWORD filledHisHealthAt = 0;

static int endCode = 0;
static const char *endWords = "";

static std::string RoomOf(const STRATEGYBLOCK *block)
{
	return block && block->containingModule && block->containingModule->name ? block->containingModule->name : "(no room)";
}

static void SayAboutTheStep(std::string const &what)
{
	char line[300];
	VECTORCH const here = Player->ObStrategyBlock->DynPtr->Position;
	int const stepNumber = stepAt < steps.size() ? (int)stepAt + 1 : (int)steps.size();

	if (following)
	{
		_snprintf(line, sizeof(line) - 1, "play: following: %s, at %d %d %d in room ", what.c_str(), here.vx, here.vy, here.vz);
	}
	else
	{
		_snprintf(line, sizeof(line) - 1, "play: step %d of %d: %s, at %d %d %d in room ", stepNumber, (int)steps.size(), what.c_str(), here.vx, here.vy, here.vz);
	}

	line[sizeof(line) - 1] = 0;
	LogString(std::string(line) + RoomOf(Player->ObStrategyBlock));
}

static char const *KindWord(HudsonPlayKind kind)
{
	switch (kind)
	{
		case kHudsonPlayPress: return "press";
		case kHudsonPlayWalkInto: return "walk into";
		case kHudsonPlayPickUp: return "pick up";
		case kHudsonPlayDestroy: return "destroy";
		default: return "kill";
	}
}

static std::string StepTitle()
{
	if (following)
	{
		return "the other Hudson";
	}

	return std::string(KindWord(steps[stepAt].kind)) + " " + steps[stepAt].name;
}

static void Stop(int code, const char *words)
{
	phase = kStopped;
	endCode = code;
	endWords = words;
}

static void NextStep(bool itWorked)
{
	if (itWorked)
	{
		stepsThatWorked++;
	}

	stepAt++;
	phase = stepAt < steps.size() ? kStartingAStep : kFinishedTheList;
	phaseStartedAt = GetTickCount();
}

static int DistanceTo(VECTORCH const &from, VECTORCH const &to)
{
	double const x = (double)to.vx - from.vx, y = (double)to.vy - from.vy, z = (double)to.vz - from.vz;

	return (int)sqrt(x * x + y * y + z * z);
}

static VECTORCH StepPlace()
{
	HudsonPlayStep const &step = steps[stepAt];
	VECTORCH const place = { step.x, step.y, step.z };

	return place;
}

// Where something is now. What's being drawn has an exact place, what can move has one in the game's sums, and failing both there's the place on the list.
static VECTORCH WhereItIs(const STRATEGYBLOCK *target, VECTORCH const &fromTheList)
{
	if (target && target->SBdptr)
	{
		return target->SBdptr->ObWorld;
	}

	if (target && target->DynPtr)
	{
		return target->DynPtr->Position;
	}

	return fromTheList;
}

// ---- Cheats. Each one is counted and logged, with where he was and why.

static void SayTheCheat(std::string const &what, std::string const &why)
{
	char number[20];
	VECTORCH const placeNow = Player->ObStrategyBlock->DynPtr->Position;
	HudsonGridPoint const cheatPlace = { placeNow.vx, placeNow.vy, placeNow.vz };

	cheatPlaces.push_back(cheatPlace);
	cheatsUsed++;
	_snprintf(number, sizeof(number) - 1, "%d", cheatsUsed);
	number[sizeof(number) - 1] = 0;
	SayAboutTheStep(std::string("cheat ") + number + ": " + what + ", because " + why + ", t=" + std::to_string((unsigned long)timeGetTime()));
}

// Asks for him to be moved to a place on the floor. It happens at the end of the frame, because moving him changes which rooms are loaded, and that can't happen while
// the game is part way through running everything in them.
static void MoveHimToTheFloorAt(HudsonGridPoint const &point, std::string const &why, DWORD now)
{
	VECTORCH const place = { point.x, point.y - kPutAboveTheFloor, point.z };

	SayTheCheat("he's moved " + std::to_string(DistanceTo(Player->ObStrategyBlock->DynPtr->Position, place)) + " mm to " + std::to_string(place.vx) + " " + std::to_string(place.vy) + " " + std::to_string(place.vz), why);
	moveHimTo = place;
	moveIsWanted = true;
	legWaitIsDue = true;
	placeLastSeenAt = place;
	placeLastChangedAt = now;
	rescueUntil = 0;
}

// Finds the floor nearest a place: asked for a way from there to itself, the grid gives back that bit of floor. False if it has none near. It's there already, so this takes one look, and
// the limit is only so that nothing here can run for ever.
static bool FindTheFloorNear(VECTORCH const &place, HudsonGridPoint *floor)
{
	HudsonGrid &grid = HudsonNav_TheGridToSearch();
	HudsonGridSearch search = kHudsonGridSearching;

	grid.StartSearch(place.vx, place.vy, place.vz, place.vx, place.vy, place.vz);

	for (int tries = 0; tries < 100 && search == kHudsonGridSearching; tries++)
	{
		search = grid.Work(kPlacesToSearchEachFrame);
	}

	if (search != kHudsonGridFound || grid.Way().empty())
	{
		return false;
	}

	*floor = grid.Way().back();
	return true;
}

// Puts him on the floor nearest the step, once for each step. If there's none, he's left where he is, because being put at the place on the list could put him inside a wall. A walk-in
// switch is different: it's a box of air, and that place is inside it, as long as it's in a room. He just falls from there.
static void MoveHimToTheStep(std::string const &why, DWORD now)
{
	movedToThisStep = true;

	HudsonPlayStep const &step = steps[stepAt];
	VECTORCH place = step.kind == kHudsonPlayWalkInto ? StepPlace() : WhereItIs(HudsonStrategies_Find(step.name.c_str()), StepPlace());
	HudsonGridPoint floor;

	if (FindTheFloorNear(place, &floor))
	{
		MoveHimToTheFloorAt(floor, why + ", for " + StepTitle(), now);
	}
	else if (step.kind == kHudsonPlayWalkInto && ModuleFromPosition(&place, (MODULE *)0))
	{
		// The height is given as if it were a floor, because being put above a floor is taken off again
		HudsonGridPoint const insideTheBox = { place.vx, place.vy + kPutAboveTheFloor, place.vz };

		MoveHimToTheFloorAt(insideTheBox, why + ", and into " + StepTitle() + ", which has no floor near it", now);
	}
	else
	{
		SayAboutTheStep("he'd have been moved to " + StepTitle() + ", because " + why + ", but the grid has no floor near it");
		return;
	}

	way.clear();
	wayAt = 0;
	wayGoesAllTheWay = true;
}

// Stuck, and everything else has failed: a hop along his way. He's only stuck while he has a way to walk, so there's always a point to hop to.
static void HopHimAlongHisWay(std::string const &why, DWORD now)
{
	wayAt = HudsonPlay_PointToHopTo(way, wayAt, kHudsonPlayHopAtLeast);
	MoveHimToTheFloorAt(way[wayAt], why, now);
}

// Kills it where it stands
static void KillItForHim(STRATEGYBLOCK *target, std::string const &name, std::string const &why)
{
	SayTheCheat(name + " is killed for him", why);
	CauseDamageToObject(target, &certainDeath, ONE_FIXED, NULL);
}

// He can't die in this run. A player would have, so every time his health runs out it's counted and logged, and it's filled up again. In something that kills every frame, it's
// only counted once a second, so that the log can still be read.
static void KeepHimAlive(PLAYER_STATUS *status, DWORD now)
{
	STRATEGYBLOCK *const me = Player->ObStrategyBlock;

	status->IsImmortal = 1;

	if (healthAtTheStart <= 0)
	{
		healthAtTheStart = me->SBDamageBlock.Health;
	}

	if (me->SBDamageBlock.Health <= 0 && now - filledHisHealthAt >= kLeastTimeBetweenFillingHisHealth)
	{
		filledHisHealthAt = now;
		SayTheCheat("his health is filled up again", "it ran out, and a player would be dead here");
		me->SBDamageBlock.Health = healthAtTheStart;
		status->Health = healthAtTheStart;
	}
}

// ---- What he does with his hands and feet. These are the fields the keyboard and mouse fill in.

static void TurnTowards(PLAYER_STATUS *status, DYNAMICSBLOCK const *dyn, int toX, int toZ)
{
	int const angle = HudsonPlay_AngleToTurn(dyn->OrientMat.mat31, dyn->OrientMat.mat33, toX, toZ);
	int const control = HudsonPlay_TurnControl(angle, kTurnsAtFull, NormalFrameTime);

	status->Mvt_TurnIncrement = control;

	if (control > 0)
	{
		status->Mvt_InputRequests.Flags.Rqst_TurnRight = 1;
	}
	else if (control < 0)
	{
		status->Mvt_InputRequests.Flags.Rqst_TurnLeft = 1;
	}
}

// Tilts his view at a point, measured from the camera, which is where the use key and the gun look from. The game keeps the view's tilt in this one number, the same way a mouse moves it.
static void LookUpOrDownAt(PLAYER_STATUS *status, VECTORCH const &point)
{
	VECTORCH const eyes = Global_VDB_Ptr->VDB_World;
	float const acrossX = (float)(point.vx - eyes.vx), acrossZ = (float)(point.vz - eyes.vz);
	float const across = sqrtf(acrossX * acrossX + acrossZ * acrossZ);

	status->ViewPanX = ((int)(atan2f((float)(point.vy - eyes.vy), across) * (float)kHudsonPlayWholeTurn / 6.2831853f)) & (kHudsonPlayWholeTurn - 1);
}

static void CrouchInADuct(PLAYER_STATUS *status)
{
	if (Player->ObStrategyBlock->containingModule && (Player->ObStrategyBlock->containingModule->m_flags & MODULEFLAG_AIRDUCT))
	{
		status->Mvt_InputRequests.Flags.Rqst_Crouch = 1;
	}
}

// The game walks at a run unless a key says otherwise, so this is all a walk needs. How hard: see HudsonPlay_WalkControl.
static void WalkForward(PLAYER_STATUS *status, int howHard = ONE_FIXED)
{
	status->Mvt_InputRequests.Flags.Rqst_Forward = 1;
	status->Mvt_MotionIncrement = howHard;
	walkedLastFrame = true;
	CrouchInADuct(status);
}

// He goes where the way goes, with forward and sidestep together, whichever way he's facing, and his view swings round after him. So putting his feet right doesn't need him to look round.
static void SteerAlongTheWay(PLAYER_STATUS *status, DYNAMICSBLOCK const *dyn, HudsonPlayAim const &aim)
{
	double const toX = (double)aim.x - dyn->Position.vx, toZ = (double)aim.z - dyn->Position.vz;
	double const length = sqrt(toX * toX + toZ * toZ);

	if (length < 1.0)
	{
		return;
	}

	// How much of the direction is in front of him and how much to his right, from which way his body points. The game's sums use 65536 for one.
	double const ahead = (toX * dyn->OrientMat.mat31 + toZ * dyn->OrientMat.mat33) / (65536.0 * length);
	double const side = (toX * dyn->OrientMat.mat11 + toZ * dyn->OrientMat.mat13) / (65536.0 * length);

	// He slows for a bend coming up, by how far the way ahead turns from where he's heading now
	int const bend = HudsonPlay_AngleToTurn((int)(toX * 65536.0 / length), (int)(toZ * 65536.0 / length), aim.furtherX - aim.x, aim.furtherZ - aim.z);
	int forwardControl = 0, sideControl = 0;

	HudsonPlay_WalkControls(ahead, side, HudsonPlay_WalkControl(bend), &forwardControl, &sideControl);

	status->Mvt_MotionIncrement = forwardControl;
	status->Mvt_SideStepIncrement = sideControl;

	if (forwardControl > 0)
	{
		status->Mvt_InputRequests.Flags.Rqst_Forward = 1;
	}
	else if (forwardControl < 0)
	{
		status->Mvt_InputRequests.Flags.Rqst_Backward = 1;
	}

	if (sideControl > 0)
	{
		status->Mvt_InputRequests.Flags.Rqst_SideStepRight = 1;
	}
	else if (sideControl < 0)
	{
		status->Mvt_InputRequests.Flags.Rqst_SideStepLeft = 1;
	}

	CrouchInADuct(status);
	walkedLastFrame = true;

	// His view goes after where the way is heading, a share each frame
	int const lookX = aim.furtherX - dyn->Position.vx, lookZ = aim.furtherZ - dyn->Position.vz;
	int const toLook = HudsonPlay_AngleToTurn(dyn->OrientMat.mat31, dyn->OrientMat.mat33, lookX, lookZ);
	int const control = HudsonPlay_TurnControl(toLook / kHudsonPlayViewTurnsOneIn, kTurnsAtFull, NormalFrameTime);

	status->Mvt_TurnIncrement = control;

	if (control > 0)
	{
		status->Mvt_InputRequests.Flags.Rqst_TurnRight = 1;
	}
	else if (control < 0)
	{
		status->Mvt_InputRequests.Flags.Rqst_TurnLeft = 1;
	}

	status->ViewPanX = 0;

	// For the run's result: how many times he's been facing more than a quarter of a circle from where he's going
	int const offHeading = toLook < 0 ? -toLook : toLook;

	if (!inABigTurn && offHeading > kBigTurnIsOver)
	{
		inABigTurn = true;
		bigTurns++;
	}
	else if (inABigTurn && offHeading < kBigTurnEndsWithin)
	{
		inABigTurn = false;
	}
}

// ---- A fight. Anything that could hurt him, that he can see, comes before the step.

// What a second player sees of anything the host runs is a ghost of it, and the ghost knows what it's a ghost of
static AVP_BEHAVIOUR_TYPE WhatItReallyIs(const STRATEGYBLOCK *block)
{
	return block->I_SBtype == I_BehaviourNetGhost && block->SBdataptr ? ((NETGHOSTDATABLOCK const *)block->SBdataptr)->type : block->I_SBtype;
}

static bool IsAgainstHim(const STRATEGYBLOCK *block)
{
	switch (WhatItReallyIs(block))
	{
		case I_BehaviourAlien:
		case I_BehaviourPredatorAlien:
		case I_BehaviourFaceHugger:
		case I_BehaviourQueenAlien:
			return AvP.PlayerType != I_Alien;

		case I_BehaviourMarine:
		case I_BehaviourSeal:
			return AvP.PlayerType != I_Marine;

		case I_BehaviourPredator:
			return AvP.PlayerType != I_Predator;

		case I_BehaviourXenoborg:
		case I_BehaviourAutoGun:
			return true;

		default:
			return false;
	}
}

static bool IsAsleepOrDead(const STRATEGYBLOCK *block)
{
	if (block->SBflags.please_destroy_me)
	{
		return true;
	}

	// How a ghost is doing is known on the host, so as long as it's there it counts as up
	if (block->I_SBtype == I_BehaviourNetGhost)
	{
		return false;
	}

	if (block->SBDamageBlock.Health <= 0)
	{
		return true;
	}

	if (block->I_SBtype == I_BehaviourAlien)
	{
		ALIEN_STATUS_BLOCK const *const alien = (ALIEN_STATUS_BLOCK *)block->SBdataptr;

		return alien->BehaviourState == ABS_Dormant || alien->BehaviourState == ABS_Awakening;
	}

	return false;
}

// The nearest he can see, but he stays on the one he's fighting for as long as he can see it. Someone who keeps swapping between two hits neither, and the time he's allowed on one would never run out.
static STRATEGYBLOCK *NearestThreat(const STRATEGYBLOCK *leftAlone, const STRATEGYBLOCK *fightingNow)
{
	VECTORCH const here = Player->ObStrategyBlock->DynPtr->Position;
	STRATEGYBLOCK *nearest = 0;
	int nearestDistance = kFightsFrom;

	for (int i = 0; i < NumActiveStBlocks; i++)
	{
		STRATEGYBLOCK *const block = ActiveStBlockList[i];

		if (!block || block == leftAlone || !block->SBdptr || !block->DynPtr || !IsAgainstHim(block) || IsAsleepOrDead(block))
		{
			continue;
		}

		int const distance = DistanceTo(here, block->DynPtr->Position);
		bool const isTheOneHesFighting = block == fightingNow;

		if (distance >= nearestDistance && !isTheOneHesFighting)
		{
			continue;
		}

		// Seen from their chest, because a line from where they stand starts at their feet and can hit the floor
		VECTORCH chest;

		GetTargetingPointOfObject_Far(block, &chest);

		if (IsThisObjectVisibleFromThisPosition_WithIgnore(Player, block->SBdptr, &chest, kFightsFrom))
		{
			if (isTheOneHesFighting)
			{
				return block;
			}

			nearest = block;
			nearestDistance = distance;
		}
	}

	return nearest;
}

static bool GunInHandIsMelee(PLAYER_STATUS const *status)
{
	PLAYER_WEAPON_DATA const *const weapon = &status->WeaponSlot[status->SelectedWeaponSlot];

	return (int)weapon->WeaponIDNumber >= 0 && TemplateWeapon[weapon->WeaponIDNumber].PrimaryIsMeleeWeapon;
}

// Turns to it and fires, and for claws or blades walks up to it first
static void FireAt(PLAYER_STATUS *status, VECTORCH const &point)
{
	DYNAMICSBLOCK const *const dyn = Player->ObStrategyBlock->DynPtr;
	int const toX = point.vx - dyn->Position.vx, toZ = point.vz - dyn->Position.vz;
	int const angle = HudsonPlay_AngleToTurn(dyn->OrientMat.mat31, dyn->OrientMat.mat33, toX, toZ);
	bool const melee = GunInHandIsMelee(status);
	int const distance = DistanceTo(dyn->Position, point);

	TurnTowards(status, dyn, toX, toZ);
	LookUpOrDownAt(status, point);

	if (melee && distance > kClawsReach && HudsonPlay_MayWalk(angle))
	{
		WalkForward(status);
	}

	if (HudsonPlay_AimedWell(angle) && (!melee || distance <= kClawsReach))
	{
		status->Mvt_InputRequests.Flags.Rqst_FirePrimaryWeapon = 1;
	}
}

// True if he's fighting this frame. Something he can't hurt can't hold him for ever: if firing at it hasn't hurt it at all after a while, he
// can't hit it from here, and he leaves it alone and walks on (hudson_play_steps.h, HudsonPlay_CantHurtIt). Something he does hurt, but
// that won't go down, is killed for him after longer, and if even that doesn't work he leaves it alone.
static bool Fighting(PLAYER_STATUS *status, DWORD now)
{
	static const STRATEGYBLOCK *lastThreat = 0;
	static const STRATEGYBLOCK *leftAlone = 0;
	static DWORD fightingItSince = 0;
	static DWORD lastSawItAt = 0;     // when he last saw the one he's fighting, so a moment out of sight doesn't start the fight again
	static int healthAtStart = 0;     // its health when the fight began, to tell whether he's hurting it at all
	STRATEGYBLOCK *const threat = NearestThreat(leftAlone, lastThreat);

	if (!threat)
	{
		// Out of sight for a moment is still the same fight, on the same clock: only after a while is it over. Meanwhile he walks.
		if (lastThreat && !HudsonPlay_IsTheSameFight(true, now - lastSawItAt))
		{
			SayAboutTheStep("nothing he can see is against him now");
			lastThreat = 0;
		}

		return false;
	}

	if (!HudsonPlay_IsTheSameFight(threat == lastThreat, now - lastSawItAt))
	{
		lastThreat = threat;
		fightingItSince = now;
		healthAtStart = threat->SBDamageBlock.Health;
		SayAboutTheStep("he fights " + HudsonStrategies_Title(threat));
	}

	lastSawItAt = now;

	if (HudsonPlay_CantHurtIt(now - fightingItSince, healthAtStart, threat->SBDamageBlock.Health))
	{
		SayAboutTheStep("he's fired at " + HudsonStrategies_Title(threat) + " for " + std::to_string((int)(kHudsonPlayCantHurtItAfter / 1000)) +
			" seconds and it isn't hurt at all, so he can't hit it from here: he leaves it alone and walks on");
		leftAlone = threat;
		lastThreat = 0;
		return false;
	}

	if (now - fightingItSince >= kGiveUpOnAFight)
	{
		// What a second player sees is only a ghost, and killing it is up to the host
		if (threat->I_SBtype == I_BehaviourNetGhost)
		{
			SayAboutTheStep("he's fought " + HudsonStrategies_Title(threat) + " for a long while and it's still up, so he leaves it to the host");
		}
		else
		{
			KillItForHim(threat, HudsonStrategies_Title(threat), "he's fought it for " + std::to_string((int)(kGiveUpOnAFight / 1000)) + " seconds and it's still up");
		}

		leftAlone = threat;
		lastThreat = 0;
		return false;
	}

	VECTORCH chest;

	GetTargetingPointOfObject_Far(threat, &chest);
	FireAt(status, chest);

	// While he's fighting, the time he's standing still doesn't count against him
	placeLastChangedAt = now;
	return true;
}

// ---- Walking

// Starts looking for the way. A switch stays where the list says, but something that's been moved or has moved is looked for where it is now.
// Where the second wants to be: behind the first and a little to one side. The way the first faces is the third row of his body's turn, and his side the first row, with 65536 for one.
static VECTORCH TheSpotBehindHim()
{
	DYNAMICSBLOCK const *const his = otherPlayer->DynPtr;
	VECTORCH spot = his->Position;

	spot.vx += MUL_FIXED(his->OrientMat.mat11, kToOneSideBy) - MUL_FIXED(his->OrientMat.mat31, kBehindHimBy);
	spot.vz += MUL_FIXED(his->OrientMat.mat13, kToOneSideBy) - MUL_FIXED(his->OrientMat.mat33, kBehindHimBy);
	return spot;
}

// How far apart two places are, seen from above
static int AcrossBetween(VECTORCH const &one, VECTORCH const &other)
{
	double const acrossX = (double)one.vx - other.vx, acrossZ = (double)one.vz - other.vz;

	return (int)sqrt(acrossX * acrossX + acrossZ * acrossZ);
}

static void StartFindingTheWay(VECTORCH const &here)
{
	// On his way to a lift's landing, that's where the way goes, whoever he is
	if (rideStage == kWalkingToTheLift && rideLift >= 0 && rideLift < (int)lifts.size())
	{
		HudsonGridPoint const &landing = lifts[(size_t)rideLift].ends[rideFrom].landing;

		lookedForHimAt = GetTickCount();
		HudsonNav_TheGridToSearch().StartSearch(here.vx, here.vy, here.vz, landing.x, landing.y, landing.z);
		phase = kFindingTheWay;
		return;
	}

	if (following)
	{
		VECTORCH const there = TheSpotBehindHim();

		lookedForHimAt = GetTickCount();
		HudsonNav_TheGridToSearch().StartSearch(here.vx, here.vy, here.vz, there.vx, there.vy, there.vz);
		phase = kFindingTheWay;
		return;
	}

	HudsonPlayStep const &step = steps[stepAt];
	bool const staysPut = step.kind == kHudsonPlayPress || step.kind == kHudsonPlayWalkInto;
	VECTORCH const place = staysPut ? StepPlace() : WhereItIs(HudsonStrategies_Find(step.name.c_str()), StepPlace());

	HudsonNav_TheGridToSearch().StartSearch(here.vx, here.vy, here.vz, place.vx, place.vy, place.vz);
	phase = kFindingTheWay;
}

static void SayWhatsHoldingHim()
{
	DYNAMICSBLOCK const *const dyn = Player->ObStrategyBlock->DynPtr;
	int shown = 0;

	for (COLLISIONREPORT const *report = dyn->CollisionReportPtr; report && shown < 4; report = report->NextCollisionReportPtr, shown++)
	{
		std::string what = report->ObstacleSBPtr ? HudsonStrategies_Title(report->ObstacleSBPtr) + ", which is " + HudsonStrategies_State(report->ObstacleSBPtr) : std::string("the level itself");
		char line[100];

		_snprintf(line, sizeof(line) - 1, ", facing %d %d %d", report->ObstacleNormal.vx, report->ObstacleNormal.vy, report->ObstacleNormal.vz);
		line[sizeof(line) - 1] = 0;
		LogString("play: stuck, touching " + what + line);
	}

	if (shown == 0)
	{
		LogString("play: stuck, touching nothing");
	}
}

// Has he got anywhere lately? He's stuck if he hasn't, and what he tries next depends how many times that's happened here.
static void CheckHesGettingSomewhere(DWORD now)
{
	VECTORCH const here = Player->ObStrategyBlock->DynPtr->Position;

	if (DistanceTo(here, placeLastSeenAt) > kHudsonPlayStuckWithin)
	{
		placeLastSeenAt = here;
		placeLastChangedAt = now;
		timesStuckHere = 0;
		return;
	}

	if (now - placeLastChangedAt < kHudsonPlayStuckAfter)
	{
		return;
	}

	placeLastChangedAt = now;
	stuckTimes++;
	rescue = HudsonPlay_NextRescue(timesStuckHere++);
	SayAboutTheStep(std::string("stuck, on the way to ") + StepTitle() + ", so " + HudsonPlay_RescueWords(rescue));
	SayWhatsHoldingHim();

	if (rescue == kHudsonPlayTeleport)
	{
		HopHimAlongHisWay("he's stuck, and nothing else got him out", now);
		return;
	}

	if (rescue == kHudsonPlayFindTheWayAgain)
	{
		StartFindingTheWay(here);
		return;
	}

	rescueUntil = now + kRescueLasts;
}

// What he's trying to get out of being stuck by doing, for as long as it lasts
static bool DoingARescue(PLAYER_STATUS *status, DWORD now)
{
	if (now >= rescueUntil)
	{
		return false;
	}

	switch (rescue)
	{
		case kHudsonPlayBackUp:
			status->Mvt_InputRequests.Flags.Rqst_Backward = 1;
			status->Mvt_MotionIncrement = -ONE_FIXED;
			break;

		case kHudsonPlayStepLeft:
			status->Mvt_InputRequests.Flags.Rqst_SideStepLeft = 1;
			status->Mvt_SideStepIncrement = -ONE_FIXED;
			break;

		case kHudsonPlayStepRight:
			status->Mvt_InputRequests.Flags.Rqst_SideStepRight = 1;
			status->Mvt_SideStepIncrement = ONE_FIXED;
			break;

		case kHudsonPlayCrouch:
			status->Mvt_InputRequests.Flags.Rqst_Crouch = 1;
			WalkForward(status);
			break;

		case kHudsonPlayJump:
			status->Mvt_InputRequests.Flags.Rqst_Jump = 1;
			WalkForward(status);
			break;

		default:
			break;
	}

	return true;
}

// A walk-in switch is a box of air, and the place on the list is only its middle, which may be somewhere nobody can stand. So what counts is the switch going off, the
// moment it does, wherever he is. True if it has, and he's moved on.
static bool TheWalkInSwitchWentOff()
{
	HudsonPlayStep const &step = steps[stepAt];

	if (step.kind != kHudsonPlayWalkInto)
	{
		return false;
	}

	STRATEGYBLOCK const *const target = HudsonStrategies_Find(step.name.c_str());

	if (!target)
	{
		return false;
	}

	std::string const state = HudsonStrategies_State(target);

	if (state == stateBefore)
	{
		return false;
	}

	SayAboutTheStep(step.name + " went off: it was " + stateBefore + ", and is now " + state);
	NextStep(true);
	return true;
}

// Someone to kill or something to destroy only has to be in sight and in range, and it isn't wise to walk right up to what might blow up
static bool ItsInSightAndRange(STRATEGYBLOCK *target)
{
	if (!target || !target->SBdptr)
	{
		return false;
	}

	VECTORCH place;

	GetTargetingPointOfObject_Far(target, &place);

	return DistanceTo(Player->ObStrategyBlock->DynPtr->Position, place) <= kShootFrom && IsThisObjectVisibleFromThisPosition_WithIgnore(Player, target->SBdptr, &place, kShootFrom);
}

static void GetToDoingTheStep(DWORD now, std::string const &why)
{
	phase = kDoingTheStep;
	phaseStartedAt = now;
	pressesTried = 0;
	lastPressAt = 0;
	SayAboutTheStep(why + StepTitle());
}

// One frame of walking along the way. False if he's at the end of it.
static bool WalkOn(PLAYER_STATUS *status, DWORD now)
{
	DYNAMICSBLOCK const *const dyn = Player->ObStrategyBlock->DynPtr;

	if (DoingARescue(status, now))
	{
		return true;
	}

	HudsonPlayAim const aim = HudsonPlay_AimAlongTheWay(way, wayAt, dyn->Position.vx, dyn->Position.vy, dyn->Position.vz);

	wayAt = aim.nextPoint;
	offTheWayNow = aim.off;
	onTheWayNow = aim.onTheWay;

	if (wayAt >= way.size())
	{
		return false;
	}

	// A fall or a fight can leave him well off the way, and then the sensible way on starts from where he is
	if ((!aim.onTheWay || aim.off > kLookAgainIfThisFarOff) && now - lookedAgainBecauseOffAt >= kLookAgainNoMoreOftenThan)
	{
		lookedAgainBecauseOffAt = now;
		StartFindingTheWay(dyn->Position);
		return true;
	}

	// The grid has the bottom of a lift's shaft as floor, and dropping into it as a way, so a way can go over the edge where a car isn't. Where it does, he stands at the edge, and that isn't
	// being stuck. Once the car is there and level with him, the way's fine: he walks onto it.
	VECTORCH const aimingAt = { aim.x, dyn->Position.vy, aim.z };

	if (OverACarThatIsntLevelWith(aimingAt, dyn->Position.vy))
	{
		placeLastChangedAt = now;
		return true;
	}

	SteerAlongTheWay(status, dyn, aim);
	CheckHesGettingSomewhere(now);
	return true;
}

// Further down, with the rest of the waiting and the lifts
static bool WaitingForTheOther(PLAYER_STATUS *status, DWORD now, bool wantsHimBeside);
static bool PutHimWhereALiftGoesFrom(VECTORCH const &there, DWORD now);

static void WalkAlongTheWay(PLAYER_STATUS *status, DWORD now)
{
	HudsonPlayStep const &step = steps[stepAt];

	if (TheWalkInSwitchWentOff())
	{
		return;
	}

	// He was moved at the end of the last frame, so the way on starts from where he is now
	if (lookAgainNextFrame)
	{
		lookAgainNextFrame = false;
		StartFindingTheWay(Player->ObStrategyBlock->DynPtr->Position);
		return;
	}

	if ((step.kind == kHudsonPlayDestroy || step.kind == kHudsonPlayKill) && ItsInSightAndRange(HudsonStrategies_Find(step.name.c_str())))
	{
		GetToDoingTheStep(now, "it's in sight, for ");
		return;
	}

	bool const aSwitchIsNear = (step.kind == kHudsonPlayPress || step.kind == kHudsonPlayWalkInto) && DistanceTo(Player->ObStrategyBlock->DynPtr->Position, StepPlace()) <= kWaitBeforeTheStepWithin;
	bool const wantsHimBeside = legWaitIsDue || (switchWaitIsDue && aSwitchIsNear);

	if (WaitingForTheOther(status, now, wantsHimBeside))
	{
		return;
	}

	// Done with, whether he came or the wait ran out
	if (wantsHimBeside)
	{
		legWaitIsDue = false;
		switchWaitIsDue = switchWaitIsDue && !aSwitchIsNear;
	}

	if (!movedToThisStep && now - stepStartedAt >= aStepHasTakenTooLong)
	{
		MoveHimToTheStep("this step has taken " + std::to_string((int)(aStepHasTakenTooLong / 1000)) + " seconds", now);
		return;
	}

	if (WalkOn(status, now))
	{
		return;
	}

	if (!wayGoesAllTheWay && !movedToThisStep)
	{
		bool const staysPut = step.kind == kHudsonPlayPress || step.kind == kHudsonPlayWalkInto;

		if (!PutHimWhereALiftGoesFrom(staysPut ? StepPlace() : WhereItIs(HudsonStrategies_Find(step.name.c_str()), StepPlace()), now))
		{
			MoveHimToTheStep("the grid knows no way there from here", now);
		}

		return;
	}

	GetToDoingTheStep(now, wayGoesAllTheWay ? "he's there, for " : "he's got as near as the grid knows, for ");
}

// The door nearest the straight line between two places, within a few metres of it and between them, and what it is on this machine now. A door shut here and open on the host is what
// would leave the second Hudson behind, and a catch-up past it would hide that, so every catch-up says this. It's only what lies along a straight line: the way round can be longer.
static const int kADoorIsOnTheLineWithin = 3000;

static std::string TheDoorBetween(VECTORCH const &from, VECTORCH const &to)
{
	double const lineX = (double)to.vx - from.vx, lineZ = (double)to.vz - from.vz;
	double const lengthSquared = lineX * lineX + lineZ * lineZ;
	STRATEGYBLOCK const *nearest = 0;
	double nearestOff = 0.0;

	for (int i = 0; i < NumActiveStBlocks; i++)
	{
		STRATEGYBLOCK const *const block = ActiveStBlockList[i];

		if (!block || !block->DynPtr || (block->I_SBtype != I_BehaviourProximityDoor && block->I_SBtype != I_BehaviourLiftDoor && block->I_SBtype != I_BehaviourSwitchDoor))
		{
			continue;
		}

		VECTORCH const place = WhereItIs(block, block->DynPtr->Position);
		double const hisX = (double)place.vx - from.vx, hisZ = (double)place.vz - from.vz;
		double const along = lengthSquared > 0.0 ? (hisX * lineX + hisZ * lineZ) / lengthSquared : 0.0;

		if (along < 0.0 || along > 1.0)
		{
			continue;
		}

		double const offX = hisX - along * lineX, offZ = hisZ - along * lineZ;
		double const off = sqrt(offX * offX + offZ * offZ);

		if (off <= kADoorIsOnTheLineWithin && (!nearest || off < nearestOff))
		{
			nearest = block;
			nearestOff = off;
		}
	}

	if (!nearest)
	{
		return "no door near the straight line between them";
	}

	return HudsonStrategies_Title(nearest) + ", which is " + HudsonStrategies_State(nearest) + " here, " + std::to_string((int)nearestOff) + " mm off the straight line between them";
}

// ---- Riding a lift. The grid leaves every lift's car out, because a car is never where the level file has it (hudson_grid_level.cpp). So when the grid knows no way, he looks for a lift
// that joins where he is to where he's going: one whose landing he can walk to, and from whose other landing he can walk on, or take another lift. Then he walks to the landing, waits for the
// car to be stopped level with it, gets on, stands still while it moves, and gets off at the other end. A car goes to its other end 1.5 seconds after someone stands on it, or when someone
// waits at that end, and it never stops part way (bh_platform_lift.cpp). He only walks onto it while it's stopped level with his landing, so he never walks into the shaft. On a joiner's
// machine his copy of the car can lag the host's, or be at the wrong end, and then he just waits. Every lift line also goes in the log as "lift:", for the lift test.

static char const *LiftStateWords(int state)
{
	switch (state)
	{
		case PLBS_AtRest: return "stopped";
		case PLBS_Activating: return "about to move";
		case PLBS_GoingUp: return "going up";
		case PLBS_GoingDown: return "going down";
		default: return "in a state this can't read";
	}
}

static int AcrossTo(int x, int z)
{
	DYNAMICSBLOCK const *const dyn = Player->ObStrategyBlock->DynPtr;
	double const acrossX = (double)x - dyn->Position.vx, acrossZ = (double)z - dyn->Position.vz;

	return (int)sqrt(acrossX * acrossX + acrossZ * acrossZ);
}

static char const *EndWord(int end)
{
	return end == 0 ? "top" : "bottom";
}

static void SayAboutTheLift(Lift const &lift, std::string const &what)
{
	char line[60];

	_snprintf(line, sizeof(line) - 1, "lift: t=%lu me ", (unsigned long)timeGetTime());
	line[sizeof(line) - 1] = 0;
	LogString(std::string(line) + HudsonStrategies_Title(lift.car) + ": " + what);
}

// The floor beside the shaft at one end: looked for a little past the car's edge all the way round, at the height his feet would be on the car. False if there's none there.
static bool FindTheLanding(Lift const &lift, int standY, HudsonGridPoint *landing)
{
	static const int kRound[8][2] = { { 10, 0 }, { -10, 0 }, { 0, 10 }, { 0, -10 }, { 7, 7 }, { 7, -7 }, { -7, 7 }, { -7, -7 } };
	int const out = (lift.halfX > lift.halfZ ? lift.halfX : lift.halfZ) + kHudsonGridMarineHalfWidth + kLookForTheLandingPastTheEdge;
	bool found = false;
	int nearest = 0;

	for (int i = 0; i < 8; i++)
	{
		VECTORCH const probe = { lift.middleX + kRound[i][0] * out / 10, standY - kPutAboveTheFloor, lift.middleZ + kRound[i][1] * out / 10 };
		HudsonGridPoint floor;

		if (!FindTheFloorNear(probe, &floor) || floor.y - standY > kLandingWithin || standY - floor.y > kLandingWithin)
		{
			continue;
		}

		double const acrossX = (double)floor.x - lift.middleX, acrossZ = (double)floor.z - lift.middleZ;
		int const across = (int)sqrt(acrossX * acrossX + acrossZ * acrossZ);

		// He stands on a landing with his own square clear of the car's: anywhere nearer, the car would carry him off, which in the first lift pair it did. A floor under the car is
		// the bottom of the shaft, and not a landing either.
		VECTORCH const standing = { floor.x, floor.y, floor.z };

		if (OverTheCar(lift, standing, kHudsonGridMarineHalfWidth + kLandingClearOfTheCar))
		{
			continue;
		}

		if (!found || across < nearest)
		{
			found = true;
			nearest = across;
			*landing = floor;
		}
	}

	return found;
}

// Finds each platform lift once, with its two landings, and says what it found
static void FindTheLifts()
{
	if (liftsAreKnown || !HudsonNav_TheGrid().Ready())
	{
		return;
	}

	liftsAreKnown = true;
	lifts.clear();

	for (int i = 0; i < NumActiveStBlocks && (int)lifts.size() < kMostLifts; i++)
	{
		STRATEGYBLOCK const *const block = ActiveStBlockList[i];

		if (!block || block->I_SBtype != I_BehaviourPlatform || !block->SBdataptr || !block->DynPtr)
		{
			continue;
		}

		PLATFORMLIFT_BEHAVIOUR_BLOCK const *const data = (PLATFORMLIFT_BEHAVIOUR_BLOCK const *)block->SBdataptr;
		SHAPEHEADER const *const shape = GetShapeData(block->shapeIndex);

		if (!shape)
		{
			continue;
		}

		Lift lift;
		int const width = shape->shapemaxx - shape->shapeminx, depth = shape->shapemaxz - shape->shapeminz;

		lift.car = block;
		lift.middleX = block->DynPtr->Position.vx + (shape->shapeminx + shape->shapemaxx) / 2;
		lift.middleZ = block->DynPtr->Position.vz + (shape->shapeminz + shape->shapemaxz) / 2;
		lift.halfWidth = (width < depth ? width : depth) / 2;
		lift.halfX = width / 2;
		lift.halfZ = depth / 2;
		lift.ends[0].carY = data->upHeight;
		lift.ends[1].carY = data->downHeight;

		std::string said = "found: the car's middle at " + std::to_string(lift.middleX) + " " + std::to_string(lift.middleZ) + ", " + std::to_string(lift.halfWidth * 2) + " mm across at its narrowest";

		for (int end = 0; end < 2; end++)
		{
			LiftEnd &at = lift.ends[end];

			at.standY = at.carY + shape->shapeminy;
			at.hasLanding = FindTheLanding(lift, at.standY, &at.landing);
			said += std::string(", the ") + EndWord(end) + " stop with its top at " + std::to_string(at.standY) + (at.hasLanding ? " and a landing at " + std::to_string(at.landing.x) + " " + std::to_string(at.landing.y) + " " + std::to_string(at.landing.z) : std::string(" and no landing beside it"));
		}

		lifts.push_back(lift);
		SayAboutTheLift(lift, said + (data->Enabled ? "" : ", and it's switched off for now"));
	}
}

static PLATFORMLIFT_BEHAVIOUR_BLOCK const *CarOf(Lift const &lift)
{
	return (PLATFORMLIFT_BEHAVIOUR_BLOCK const *)lift.car->SBdataptr;
}

static bool CarIsStoppedAt(Lift const &lift, int end)
{
	PLATFORMLIFT_BEHAVIOUR_BLOCK const *const car = CarOf(lift);
	int const off = lift.car->DynPtr->Position.vy - lift.ends[end].carY;

	return (car->state == PLBS_AtRest || car->state == PLBS_Activating) && off <= kTheCarIsLevelWithin && off >= -kTheCarIsLevelWithin;
}

static bool TouchingTheCar(Lift const &lift)
{
	for (COLLISIONREPORT const *report = Player->ObStrategyBlock->DynPtr->CollisionReportPtr; report; report = report->NextCollisionReportPtr)
	{
		if (report->ObstacleSBPtr == lift.car)
		{
			return true;
		}
	}

	return false;
}

// Is a place over a lift's car, seen from above, with this much to spare round its edge? The car is a box, so this is a square. Its turn, if it has one, is left out.
static bool OverTheCar(Lift const &lift, VECTORCH const &place, int toSpare)
{
	int const acrossX = place.vx - lift.middleX, acrossZ = place.vz - lift.middleZ;

	return acrossX <= lift.halfX + toSpare && acrossX >= -lift.halfX - toSpare && acrossZ <= lift.halfZ + toSpare && acrossZ >= -lift.halfZ - toSpare;
}

// The lift whose car he's standing on now, or -1
static int TheLiftHesOn()
{
	for (size_t n = 0; n < lifts.size(); n++)
	{
		if (TouchingTheCar(lifts[n]) && OverTheCar(lifts[n], Player->ObStrategyBlock->DynPtr->Position))
		{
			return (int)n;
		}
	}

	return -1;
}

// Is a place over a car that isn't stopped level with this height? Walking there would be walking into the shaft.
static bool OverACarThatIsntLevelWith(VECTORCH const &place, int feetY)
{
	for (Lift const &lift : lifts)
	{
		if (!OverTheCar(lift, place, kHudsonGridMarineHalfWidth))
		{
			continue;
		}

		for (int end = 0; end < 2; end++)
		{
			if (CarIsStoppedAt(lift, end) && lift.ends[end].standY - feetY <= kSameLevelWithin && feetY - lift.ends[end].standY <= kSameLevelWithin)
			{
				return false;
			}
		}

		return true;
	}

	return false;
}

static std::vector<uint8_t> const &ReachedFromTheLanding(int liftNumber, int end)
{
	int const key = liftNumber * 2 + end;
	const auto known = reachedFromALanding.find(key);

	if (known != reachedFromALanding.end())
	{
		return known->second;
	}

	std::vector<uint8_t> &marks = reachedFromALanding[key];
	HudsonGridPoint const &landing = lifts[(size_t)liftNumber].ends[end].landing;

	HudsonNav_TheGrid().MarkWhatCanBeReachedFrom(landing.x, landing.y, landing.z, &marks);
	return marks;
}

// Which lift to take first, to get from here to there, and at which end to get on. It looks one lift further at a time, up to kMostLiftsInARow, so the plan with the fewest lifts wins. False if no
// lift joins the two, and then what's said says why.
static bool PlanARide(VECTORCH const &here, VECTORCH const &there, std::string *said)
{
	FindTheLifts();

	HudsonGrid const &grid = HudsonNav_TheGrid();
	std::vector<uint8_t> fromHere;

	if (lifts.empty())
	{
		*said = "there are no lifts in this level";
		return false;
	}

	if (!grid.MarkWhatCanBeReachedFrom(here.vx, here.vy, here.vz, &fromHere))
	{
		*said = "there's no floor where he is";
		return false;
	}

	if (grid.IsMarked(fromHere, there.vx, there.vy, there.vz))
	{
		*said = "he can walk there without a lift, though the way didn't get there";
		return false;
	}

	struct Reached
	{
		std::vector<uint8_t> const *marks;
		int firstLift, firstEnd;
		std::string how;
	};

	std::vector<Reached> reached(1);
	std::vector<bool> used(lifts.size(), false);

	reached[0].marks = &fromHere;
	reached[0].firstLift = -1;
	reached[0].firstEnd = 0;

	for (int inARow = 0; inARow < kMostLiftsInARow && !reached.empty(); inARow++)
	{
		std::vector<Reached> further;

		for (Reached const &from : reached)
		{
			for (size_t n = 0; n < lifts.size(); n++)
			{
				Lift const &lift = lifts[n];

				if (used[n] || !CarOf(lift)->Enabled || !lift.ends[0].hasLanding || !lift.ends[1].hasLanding)
				{
					continue;
				}

				for (int on = 0; on < 2; on++)
				{
					HudsonGridPoint const &landing = lift.ends[on].landing;

					if (!grid.IsMarked(*from.marks, landing.x, landing.y, landing.z))
					{
						continue;
					}

					Reached next;

					used[n] = true;
					next.marks = &ReachedFromTheLanding((int)n, 1 - on);
					next.firstLift = from.firstLift < 0 ? (int)n : from.firstLift;
					next.firstEnd = from.firstLift < 0 ? on : from.firstEnd;
					next.how = from.how + (from.how.empty() ? "" : ", then ") + HudsonStrategies_Title(lift.car) + " from its " + EndWord(on) + " to its " + EndWord(1 - on);

					if (grid.IsMarked(*next.marks, there.vx, there.vy, there.vz))
					{
						rideLift = next.firstLift;
						rideFrom = next.firstEnd;
						*said = "the grid knows no way, but a lift joins them: " + next.how;
						return true;
					}

					further.push_back(next);
					break;
				}
			}
		}

		reached.swap(further);
	}

	*said = "the grid knows no way, and no lift that's switched on joins where he is to where he's going";
	return false;
}

static void SetRideStage(RideStage stage, std::string const &what, DWORD now)
{
	rideStage = stage;
	rideStageAt = now;
	SayAboutTheLift(lifts[(size_t)rideLift], what);
}

// Starts a ride that's been planned: first, the way to the landing
static void StartTheRide(VECTORCH const &here, DWORD now)
{
	ridesThisStep++;
	farBehindSince = 0;
	SetRideStage(kWalkingToTheLift, std::string("walks to its landing at the ") + EndWord(rideFrom), now);
	StartFindingTheWay(here);
}

// The ride didn't work. What he does instead is what he'd have done without a lift: the first walks as near as the grid gets and is put at the step, and the second is moved up in time.
static void GiveUpTheRide(std::string const &why, DWORD now)
{
	SayAboutTheLift(lifts[(size_t)rideLift], "gives up and hops, because " + why + ". The hop is the cheat he'd have had without the lift");
	rideStage = kNotRiding;
	noLiftHelps = true;
	plannedARideAt = now;
	StartFindingTheWay(Player->ObStrategyBlock->DynPtr->Position);
}

// One frame of riding. True if that's what he did this frame.
static bool RidingALift(PLAYER_STATUS *status, DWORD now)
{
	if (rideStage == kNotRiding || rideLift < 0 || rideLift >= (int)lifts.size())
	{
		return false;
	}

	Lift const &lift = lifts[(size_t)rideLift];
	DYNAMICSBLOCK const *const dyn = Player->ObStrategyBlock->DynPtr;
	int const rideTo = 1 - rideFrom;
	DWORD const inThisStage = now - rideStageAt;

	// Waiting at a landing or standing on the car isn't being stuck
	if (rideStage != kWalkingToTheLift)
	{
		placeLastChangedAt = now;
	}

	switch (rideStage)
	{
		case kWalkingToTheLift:
			if (WalkOn(status, now))
			{
				return true;
			}

			if (!wayGoesAllTheWay)
			{
				GiveUpTheRide("the grid's way to its landing didn't get there", now);
				return false;
			}

			SetRideStage(kWaitingForTheCar, std::string("waits for the car at the ") + EndWord(rideFrom), now);
			return true;

		case kWaitingForTheCar:
			TurnTowards(status, dyn, lift.middleX - dyn->Position.vx, lift.middleZ - dyn->Position.vz);

			if (inThisStage >= kWaitForTheCarAtMost)
			{
				GiveUpTheRide("the car didn't come in " + std::to_string((int)(kWaitForTheCarAtMost / 1000)) + " seconds", now);
				return false;
			}

			// Standing at the landing calls it, the same as for anyone (bh_platform_lift.cpp). Until it's stopped level with him, he stays where he is.
			if (!CarIsStoppedAt(lift, rideFrom))
			{
				return true;
			}

			// The first takes the second with him, so he waits until the second is beside him, as he does before a switch
			if (WaitingForTheOther(status, now, true))
			{
				return true;
			}

			// He rides on the far side of the middle from his landing, along whichever way the car is longer, so that the near side is left for the second
			{
				HudsonGridPoint const &landing = lift.ends[rideFrom].landing;
				int const roomX = lift.halfX - kHudsonGridMarineHalfWidth - 150, roomZ = lift.halfZ - kHudsonGridMarineHalfWidth - 150;

				rideSpot.vx = lift.middleX;
				rideSpot.vy = dyn->Position.vy;
				rideSpot.vz = lift.middleZ;

				if (roomX > 0 && roomX >= roomZ)
				{
					rideSpot.vx += landing.x < lift.middleX ? roomX : -roomX;
				}
				else if (roomZ > 0)
				{
					rideSpot.vz += landing.z < lift.middleZ ? roomZ : -roomZ;
				}
			}

			SetRideStage(kGettingOn, std::string("steps on at the ") + EndWord(rideFrom) + ", the car stopped at height " + std::to_string(lift.car->DynPtr->Position.vy) + ", his feet at " + std::to_string(dyn->Position.vy) + ", for the far side of the car at " + std::to_string(rideSpot.vx) + " " + std::to_string(rideSpot.vz), now);
			return true;

		case kGettingOn:
		{
			// At the far side, where his whole square is on the car
			int const onWithin = 300;

			if (!CarIsStoppedAt(lift, rideFrom) && !TouchingTheCar(lift))
			{
				// It went before he was on it. He stops at once, which leaves him on the landing, and waits for it to come back.
				SetRideStage(kWaitingForTheCar, "the car went before he was on it, so he stops, and waits for the car again", now);
				return true;
			}

			if (AcrossTo(rideSpot.vx, rideSpot.vz) <= onWithin)
			{
				SetRideStage(kRiding, "rides, standing still on the car", now);
				return true;
			}

			if (inThisStage >= kGetOnAtMost)
			{
				SetRideStage(kWaitingForTheCar, "couldn't get onto the car in " + std::to_string((int)(kGetOnAtMost / 1000)) + " seconds, so he waits for the car again", now);
				return true;
			}

			// At full speed now, because the second has to get on behind him before it goes. It only goes 1.5 seconds after he first touches it, so he's on long before.
			TurnTowards(status, dyn, rideSpot.vx - dyn->Position.vx, rideSpot.vz - dyn->Position.vz);
			WalkForward(status);
			return true;
		}

		case kRiding:
			// No keys at all: he stands where he is on top of it
			if (CarIsStoppedAt(lift, rideTo) && inThisStage > 500)
			{
				SetRideStage(kGettingOff, std::string("steps off at the ") + EndWord(rideTo), now);
				return true;
			}

			// It never went, and he isn't touching it, so he isn't on it after all
			if (CarIsStoppedAt(lift, rideFrom) && inThisStage > 4000 && !TouchingTheCar(lift))
			{
				SetRideStage(kWaitingForTheCar, "isn't on the car after all, so he waits for the car again", now);
				return true;
			}

			if (inThisStage >= kRideAtMost)
			{
				GiveUpTheRide("the ride took longer than " + std::to_string((int)(kRideAtMost / 1000)) + " seconds", now);
				return false;
			}

			return true;

		case kGettingOff:
		{
			HudsonGridPoint const &landing = lift.ends[rideTo].landing;

			if (!OverTheCar(lift, dyn->Position, kHudsonGridMarineHalfWidth) && (!TouchingTheCar(lift) || AcrossTo(landing.x, landing.z) < 400))
			{
				ridesDone++;
				rideStage = kNotRiding;
				farBehindSince = 0;
				legWaitIsDue = true;
				SayAboutTheLift(lift, std::string("is off, having ridden it from its ") + EndWord(rideFrom) + " to its " + EndWord(rideTo) + ", at " + std::to_string(dyn->Position.vx) + " " + std::to_string(dyn->Position.vy) + " " + std::to_string(dyn->Position.vz));
				StartFindingTheWay(dyn->Position);
				return true;
			}

			// It went back with him still on it, so he rides it back, and gets off at the end he got on at
			if (!CarIsStoppedAt(lift, rideTo))
			{
				rideFrom = rideTo;
				SetRideStage(kRiding, "the car went back with him still on it, so he rides it back", now);
				return true;
			}

			if (inThisStage >= kGetOffAtMost)
			{
				GiveUpTheRide("he couldn't get off in " + std::to_string((int)(kGetOffAtMost / 1000)) + " seconds", now);
				return false;
			}

			TurnTowards(status, dyn, landing.x - dyn->Position.vx, landing.z - dyn->Position.vz);
			WalkForward(status);
			return true;
		}

		default:
			return false;
	}
}

// When the grid's search has found no way: perhaps a lift joins the two. True if he's set off for one.
static bool TryALift(VECTORCH const &here, VECTORCH const &there, DWORD now)
{
	if (rideStage != kNotRiding || ridesThisStep >= kMostRidesForAStep)
	{
		return false;
	}

	if (following ? now - plannedARideAt < kTheSecondPlansNoMoreOftenThan : noLiftHelps)
	{
		return false;
	}

	std::string said;

	plannedARideAt = now;

	if (!PlanARide(here, there, &said))
	{
		noLiftHelps = true;
		SayAboutTheStep(said);
		return false;
	}

	SayAboutTheStep(said);
	StartTheRide(here, now);
	return true;
}

// The grid knows no way to the step, and no lift joins where he is to it. If a lift goes to it from some other floor, he's put on that floor instead of at the step, well back from the lift,
// and then rides it: the cheat covers only the part of the way the grid doesn't know, and isn't at the lift. False if no lift goes there, and then he's put at the step as before.
static bool PutHimWhereALiftGoesFrom(VECTORCH const &there, DWORD now)
{
	FindTheLifts();

	HudsonGrid const &grid = HudsonNav_TheGrid();

	for (size_t n = 0; n < lifts.size(); n++)
	{
		Lift const &lift = lifts[n];

		if (!CarOf(lift)->Enabled || !lift.ends[0].hasLanding || !lift.ends[1].hasLanding)
		{
			continue;
		}

		for (int on = 0; on < 2; on++)
		{
			if (!grid.IsMarked(ReachedFromTheLanding((int)n, 1 - on), there.vx, there.vy, there.vz))
			{
				continue;
			}

			// The nearest place to the lift that's still well back from it, on the floor the landing joins, and not over any car
			std::vector<uint8_t> const &byTheLanding = ReachedFromTheLanding((int)n, on);
			HudsonGridCell cell, best;
			bool found = false;
			double bestAcross = 0.0, furthest = 0.0;

			for (size_t p = 0; p < byTheLanding.size(); p++)
			{
				if (!byTheLanding[p] || !grid.CellAt((int)p, &cell))
				{
					continue;
				}

				double const acrossX = (double)cell.x - lift.middleX, acrossZ = (double)cell.z - lift.middleZ;
				double const across = sqrt(acrossX * acrossX + acrossZ * acrossZ);
				VECTORCH const place = { cell.x, cell.y, cell.z };
				bool overACar = false;

				furthest = across > furthest ? across : furthest;

				if (across < kPutBackFromALift || across > kPutNoFurtherFromALift || (found && across >= bestAcross))
				{
					continue;
				}

				for (Lift const &other : lifts)
				{
					overACar = overACar || OverTheCar(other, place);
				}

				if (!overACar)
				{
					found = true;
					bestAcross = across;
					best = cell;
				}
			}

			SayAboutTheLift(lift, std::string("goes from its ") + EndWord(on) + " to " + StepTitle() + ", and the floor by that landing reaches as far as " + std::to_string((int)furthest) + " mm from it");

			if (!found)
			{
				continue;
			}

			HudsonGridPoint const point = { best.x, best.y, best.z };

			movedToThisStep = true;
			noLiftHelps = false;
			plannedARideAt = 0;
			MoveHimToTheFloorAt(point, "the grid knows no way to " + StepTitle() + ", but " + HudsonStrategies_Title(lift.car) + " goes there from its " + EndWord(on) + ", so he's put " + std::to_string((int)bestAcross) + " mm back from it, to ride it", now);
			way.clear();
			wayAt = 0;
			wayGoesAllTheWay = false;
			lookAgainNextFrame = true;
			return true;
		}
	}

	return false;
}

// ---- What each machine says about the lifts, for the lift test and for comparing the two machines. Both lines use the Windows clock, which both games on one machine share.

// liftlog: the line agreed with the lift's own network code, so that one reader reads both: the car's place and state on every change, and once a second while it moves. name is the car's 8-byte
// name in the level, in hex, because it isn't text. sent is how many lift messages this machine has sent or had for it, and -1 means this build doesn't count them.
static void SayEachLiftsState(DWORD now)
{
	static std::map<STRATEGYBLOCK const *, int> stateSaid;
	static std::map<STRATEGYBLOCK const *, DWORD> saidAt;

	for (int i = 0; i < NumActiveStBlocks; i++)
	{
		STRATEGYBLOCK const *const block = ActiveStBlockList[i];

		if (!block || block->I_SBtype != I_BehaviourPlatform || !block->SBdataptr || !block->DynPtr)
		{
			continue;
		}

		PLATFORMLIFT_BEHAVIOUR_BLOCK const *const car = (PLATFORMLIFT_BEHAVIOUR_BLOCK const *)block->SBdataptr;
		const auto known = stateSaid.find(block);
		bool const moving = car->state == PLBS_GoingUp || car->state == PLBS_GoingDown;
		bool const changed = known == stateSaid.end() || known->second != (int)car->state;

		if (!changed && !(moving && now - saidAt[block] >= 1000))
		{
			continue;
		}

		// The end it's going to while it moves, and the end it's at while it isn't
		bool const up = car->state == PLBS_GoingUp || (car->state != PLBS_GoingDown && block->DynPtr->Position.vy - car->upHeight < car->downHeight - block->DynPtr->Position.vy);
		char name[17];
		char line[200];

		for (int b = 0; b < 8; b++)
		{
			_snprintf(name + b * 2, 3, "%02x", (unsigned char)block->SBname[b]);
		}

		name[16] = 0;
		_snprintf(line, sizeof(line) - 1, "liftlog: t=%lu name=%s y=%d state=%d end=%s sent=-1", (unsigned long)timeGetTime(), name, block->DynPtr->Position.vy, (int)car->state, up ? "up" : "down");
		line[sizeof(line) - 1] = 0;

		// The first time, which car this is, in words
		if (known == stateSaid.end())
		{
			LogString(std::string("liftlog: name=") + name + " is " + HudsonStrategies_Title(block));
		}

		// With ghostlog=yes the lift sync's own writer (Hudson_GhostLogEachFrame, hudson.cpp) writes this line, with the real count
		// of lift messages, so this one steps aside and the reader isn't given each line twice
		extern bool Hudson_GhostLogIsOn();

		if (!Hudson_GhostLogIsOn())
		{
			LogString(line);
		}
		stateSaid[block] = (int)car->state;
		saidAt[block] = now;
	}
}

// lift: where each body is, every 100 ms while it's near a lift, as this machine sees it: this machine's own player, and the other player's ghost
static void SayOneNearALift(STRATEGYBLOCK const *body, char const *who, bool mine)
{
	if (!body || !body->DynPtr)
	{
		return;
	}

	VECTORCH const place = body->DynPtr->Position;

	for (Lift const &lift : lifts)
	{
		double const acrossX = (double)place.vx - lift.middleX, acrossZ = (double)place.vz - lift.middleZ;
		double const reach = (double)lift.halfWidth + kNearALiftAcross;

		if (acrossX * acrossX + acrossZ * acrossZ > reach * reach || place.vy < lift.ends[0].standY - kNearALiftUpOrDown || place.vy > lift.ends[1].carY + kNearALiftUpOrDown)
		{
			continue;
		}

		// On the car: for his own body, touching it while over it. For the other's, which can't be felt here, over it and within 300 mm of its top.
		int const carTop = lift.car->DynPtr->Position.vy + (lift.ends[0].standY - lift.ends[0].carY);
		bool const onTheCar = mine ? TouchingTheCar(lift) && OverTheCar(lift, place) : OverTheCar(lift, place) && place.vy - carTop <= 300 && carTop - place.vy <= 300;
		char line[300];

		_snprintf(line, sizeof(line) - 1, "lift: t=%lu %s near %s: at %d %d %d, on the floor %d, on the car %d, health %d; the car at height %d, %s", (unsigned long)timeGetTime(), who, HudsonStrategies_Title(lift.car).c_str(), place.vx, place.vy, place.vz,
			body->DynPtr->IsInContactWithFloor ? 1 : 0, onTheCar ? 1 : 0, body->SBDamageBlock.Health >> ONE_FIXED_SHIFT, lift.car->DynPtr->Position.vy, LiftStateWords(CarOf(lift)->state));
		line[sizeof(line) - 1] = 0;
		LogString(line);
		return;
	}
}

static void SayWhereEachIsNearALift(DWORD now)
{
	static DWORD saidAt = 0;

	if (now - saidAt < kSayWhereEachIsNearALiftEvery)
	{
		return;
	}

	saidAt = now;
	FindTheLifts();
	SayOneNearALift(Player->ObStrategyBlock, "me", true);
	SayOneNearALift(otherPlayer, "other", false);
}

// A fall, said when he lands: from the highest he was since he left the floor, to where he landed, and whether he landed on a lift's car
static void WatchForAFall()
{
	static bool wasOnTheFloor = true;
	static int highestSinceTheFloor = 0;   // the game's y goes down, so this is the smallest y
	DYNAMICSBLOCK const *const dyn = Player->ObStrategyBlock->DynPtr;
	bool const onTheFloor = dyn->IsInContactWithFloor != 0;

	if (!onTheFloor)
	{
		highestSinceTheFloor = wasOnTheFloor || dyn->Position.vy < highestSinceTheFloor ? dyn->Position.vy : highestSinceTheFloor;
	}
	else if (!wasOnTheFloor && dyn->Position.vy - highestSinceTheFloor > kSayAFallOfMoreThan)
	{
		int const lift = TheLiftHesOn();

		SayAboutTheStep("he fell " + std::to_string(dyn->Position.vy - highestSinceTheFloor) + " mm, t=" + std::to_string((unsigned long)timeGetTime()) + (lift >= 0 ? ", onto the car of " + HudsonStrategies_Title(lifts[(size_t)lift].car) : std::string("")));
	}

	wasOnTheFloor = onTheFloor;
}

// Where the first Hudson is, looked at every frame, whatever this one is doing: a fight or a search can last a while, and looked at only then, his ordinary running would look like a jump.
// In one run that moved the second up twice for nothing.
static void WatchWhereTheFirstIs(DWORD now)
{
	static VECTORCH whereHeWas = { 0, 0, 0 };
	static DWORD whenHeGotThere = 0;
	static bool seenHim = false;

	if (!following || !otherPlayer || !otherPlayer->DynPtr)
	{
		seenHim = false;
		return;
	}

	VECTORCH const there = otherPlayer->DynPtr->Position;
	int const jump = DistanceTo(there, whereHeWas);

	if (!seenHim)
	{
		seenHim = true;
		whereHeWas = there;
		whenHeGotThere = now;
		return;
	}

	// Nothing new about him this frame
	if (jump == 0)
	{
		return;
	}

	firstLastMovedAt = now;

	DWORD const took = now - whenHeGotThere;

	if (jump > kHeWasMovedFurtherThan)
	{
		heWasJustMoved = true;
		SayAboutTheStep("the first Hudson went " + std::to_string(jump) + " mm since " + std::to_string((int)took) + " ms ago, further than any late message makes him jump, so he was moved");
	}

	whereHeWas = there;
	whenHeGotThere = now;
}

// playgun=: the gun he's given, once, and asked for until it's in his hands. Every change of gun is said, so the film can be matched to who's firing what. It's the run's choice of gun,
// not something to get him through the level, so it isn't counted as a cheat.
static void SeeToHisGun(PLAYER_STATUS *status)
{
	if (gunToHold > 0 && gunToHold <= MAX_NO_OF_WEAPON_SLOTS && !gunGiven)
	{
		PLAYER_WEAPON_DATA *const weapon = &status->WeaponSlot[gunToHold - 1];

		gunGiven = true;

		if (weapon->WeaponIDNumber == NULL_WEAPON || weapon->Possessed == -1)
		{
			LogString("play: gun: slot " + std::to_string(gunToHold) + " has no gun he may have, so he keeps what he has");
			gunToHold = 0;
		}
		else
		{
			weapon->Possessed = 1;
			weapon->PrimaryMagazinesRemaining = weapon->PrimaryMagazinesRemaining < 50 ? 50 : weapon->PrimaryMagazinesRemaining;
			LogString("play: gun: he's given the gun in slot " + std::to_string(gunToHold) + ", weapon " + std::to_string((int)weapon->WeaponIDNumber) + ", with 50 spare magazines, as playgun= asks");
		}
	}

	if (gunToHold > 0 && !gunInHand)
	{
		if ((int)status->SelectedWeaponSlot == gunToHold - 1)
		{
			gunInHand = true;
		}
		else
		{
			status->Mvt_InputRequests.Flags.Rqst_WeaponNo = gunToHold;
		}
	}

	int const slotNow = (int)status->SelectedWeaponSlot;

	if (slotNow != gunSlotSaid && slotNow >= 0 && slotNow < MAX_NO_OF_WEAPON_SLOTS)
	{
		gunSlotSaid = slotNow;
		LogString("play: gun: t=" + std::to_string((unsigned long)timeGetTime()) + ", slot " + std::to_string(slotNow + 1) + " is in his hands, weapon " + std::to_string((int)status->WeaponSlot[slotNow].WeaponIDNumber));
	}
}

// ---- Following, for the second of two Hudsons. He has no list to do, because a switch pressed by both goes back to how it was. He keeps up with the first and fights what he sees.

// The floor nearest a place that can be walked to from another: the grid's way from the one towards the other ends there, even when it can't get all the way. False if there's no floor at all.
static bool FindTheFloorTowards(VECTORCH const &from, VECTORCH const &towards, HudsonGridPoint *floor)
{
	HudsonGrid &grid = HudsonNav_TheGridToSearch();
	HudsonGridSearch search = kHudsonGridSearching;

	grid.StartSearch(from.vx, from.vy, from.vz, towards.vx, towards.vy, towards.vz);

	for (int tries = 0; tries < 100 && search == kHudsonGridSearching; tries++)
	{
		search = grid.Work(kPlacesToSearchEachFrame);
	}

	if (search == kHudsonGridSearching || grid.Way().empty())
	{
		return false;
	}

	*floor = grid.Way().back();
	return true;
}

// Moves the second up to the first: to the spot behind him, or failing that, to his other side, ahead of him, or level with him on either side, each 2 m away. It's floor the grid has, that can
// be walked to from where the first stands, so it's on his side of any wall, and it's never within a metre of him or over a lift's car. It's a cheat, and counted as one.
static void MoveHimUpBehindHim(VECTORCH const &there, std::string const &why, DWORD now)
{
	DYNAMICSBLOCK const *const his = otherPlayer->DynPtr;
	// Behind and to one side, behind and to the other, beside him either way, and ahead, in that order
	static const int kTries[5][2] = { { -kBehindHimBy, kToOneSideBy }, { -kBehindHimBy, -kToOneSideBy }, { 0, kBehindHimBy }, { 0, -kBehindHimBy }, { kBehindHimBy, 0 } };

	for (int i = 0; i < 5; i++)
	{
		VECTORCH spot = there;
		HudsonGridPoint floor;

		spot.vx += MUL_FIXED(his->OrientMat.mat31, kTries[i][0]) + MUL_FIXED(his->OrientMat.mat11, kTries[i][1]);
		spot.vz += MUL_FIXED(his->OrientMat.mat33, kTries[i][0]) + MUL_FIXED(his->OrientMat.mat13, kTries[i][1]);

		if (!FindTheFloorTowards(there, spot, &floor))
		{
			continue;
		}

		VECTORCH const landing = { floor.x, floor.y, floor.z };
		bool overACar = false;

		for (Lift const &lift : lifts)
		{
			overACar = overACar || OverTheCar(lift, landing, kHudsonGridMarineHalfWidth);
		}

		if (AcrossBetween(landing, there) < kNeverNearerThan + kHudsonGridMarineHalfWidth || overACar)
		{
			continue;
		}

		MoveHimToTheFloorAt(floor, why, now);
		way.clear();
		wayAt = 0;
		return;
	}

	SayAboutTheStep("he'd have been moved up to the other Hudson, because " + why + ", but the grid has no floor 2 m from him that isn't on a lift");
}

// He's nearer the first than he should ever be, or the first has backed into him: he faces him and steps back, unless that would take him over the edge of a shaft
static void StepBackFrom(PLAYER_STATUS *status, DYNAMICSBLOCK const *dyn, VECTORCH const &there, DWORD now)
{
	static DWORD saidAt = 0;
	VECTORCH behindMe = dyn->Position;

	behindMe.vx -= MUL_FIXED(dyn->OrientMat.mat31, kNeverNearerThan);
	behindMe.vz -= MUL_FIXED(dyn->OrientMat.mat33, kNeverNearerThan);
	placeLastChangedAt = now;
	TurnTowards(status, dyn, there.vx - dyn->Position.vx, there.vz - dyn->Position.vz);

	if (now - saidAt >= 2000)
	{
		saidAt = now;
		SayAboutTheStep("he's within " + std::to_string(kNeverNearerThan) + " mm of the other Hudson, so he steps back");
	}

	if (OverACarThatIsntLevelWith(behindMe, dyn->Position.vy))
	{
		return;
	}

	status->Mvt_InputRequests.Flags.Rqst_Backward = 1;
	status->Mvt_MotionIncrement = -ONE_FIXED;
}

// The lift whose car a place is on: over its square, and within 300 mm of its top. -1 if none.
static int TheCarAPlaceIsOn(VECTORCH const &place)
{
	for (size_t n = 0; n < lifts.size(); n++)
	{
		Lift const &lift = lifts[n];
		int const carTop = lift.car->DynPtr->Position.vy + (lift.ends[0].standY - lift.ends[0].carY);

		if (OverTheCar(lift, place) && place.vy - carTop <= 300 && carTop - place.vy <= 300)
		{
			return (int)n;
		}
	}

	return -1;
}

// Where the second stands on a car the first is already on: on the other side of the middle from him, along whichever way the car is longer, with a metre between their squares. False if
// the car is too small for that.
static bool ASpotOnTheCarAwayFrom(Lift const &lift, VECTORCH const &him, VECTORCH *spot)
{
	int const roomX = lift.halfX - kHudsonGridMarineHalfWidth - 150, roomZ = lift.halfZ - kHudsonGridMarineHalfWidth - 150;

	if (roomX < 0 || roomZ < 0)
	{
		return false;
	}

	spot->vx = lift.middleX;
	spot->vy = him.vy;
	spot->vz = lift.middleZ;

	if (roomX >= roomZ)
	{
		spot->vx += him.vx > lift.middleX ? -roomX : roomX;
	}
	else
	{
		spot->vz += him.vz > lift.middleZ ? -roomZ : roomZ;
	}

	return AcrossBetween(*spot, him) >= kGapOnACar + 2 * kHudsonGridMarineHalfWidth;
}

// Is a car stopped at an end that's level with these feet?
static bool CarIsStoppedLevelWith(Lift const &lift, int feetY)
{
	for (int end = 0; end < 2; end++)
	{
		if (CarIsStoppedAt(lift, end) && lift.ends[end].standY - feetY <= kSameLevelWithin && feetY - lift.ends[end].standY <= kSameLevelWithin)
		{
			return true;
		}
	}

	return false;
}

static void FollowHim(PLAYER_STATUS *status, DWORD now)
{
	if (!otherPlayer)
	{
		return;
	}

	DYNAMICSBLOCK const *const dyn = Player->ObStrategyBlock->DynPtr;
	VECTORCH const there = otherPlayer->DynPtr->Position;
	int const distance = DistanceTo(dyn->Position, there);
	int const upOrDown = there.vy - dyn->Position.vy;
	bool const heWasMoved = heWasJustMoved;

	heWasJustMoved = false;

	if (distance <= kTooFarBehind)
	{
		farBehindSince = 0;
	}
	else if (farBehindSince == 0)
	{
		farBehindSince = now;
	}

	static std::string saidAboutTheCar;
	std::string sayAboutTheCar;
	int const carHesOn = TheLiftHesOn();
	int const carTheFirstIsOn = TheCarAPlaceIsOn(there);
	VECTORCH const spot = TheSpotBehindHim();

	static bool carMovedWithHimOn = false;

	if (carHesOn < 0)
	{
		carMovedWithHimOn = false;
	}

	// On a car that's moving, he stands still, whatever else is going on
	if (carHesOn >= 0 && !CarIsStoppedAt(lifts[(size_t)carHesOn], 0) && !CarIsStoppedAt(lifts[(size_t)carHesOn], 1))
	{
		carMovedWithHimOn = true;
		placeLastChangedAt = now;
		return;
	}

	// Never nearer than a metre: if he is, or the first has backed into him, he steps back
	if (distance < kNeverNearerThan)
	{
		StepBackFrom(status, dyn, there, now);
		return;
	}

	// The first was moved by a cheat, so this one comes too, and it's counted as his own cheat
	if (heWasMoved && distance > kCloseEnoughToHim)
	{
		farBehindSince = 0;
		MoveHimUpBehindHim(there, "the first Hudson was moved, so this one comes too, " + std::to_string(distance) + " mm", now);
		return;
	}

	// He's on a car that has stopped, and the first isn't on it: he gets off at once, towards the spot behind him, before it takes him away
	if (carHesOn >= 0 && carTheFirstIsOn != carHesOn && upOrDown <= kSameLevelWithin && upOrDown >= -kSameLevelWithin)
	{
		placeLastChangedAt = now;
		TurnTowards(status, dyn, spot.vx - dyn->Position.vx, spot.vz - dyn->Position.vz);
		WalkForward(status);
		return;
	}

	// Both on the same car: he stays where he is while it waits to go and while it moves. When it has taken them to the other end, the one nearer the landing there gets off first, so that
	// neither walks into the other. If that's him, he walks off past the landing.
	if (carHesOn >= 0 && carTheFirstIsOn == carHesOn)
	{
		Lift const &lift = lifts[(size_t)carHesOn];
		HudsonGridPoint const &landing = lift.ends[CarIsStoppedAt(lift, 0) ? 0 : 1].landing;
		VECTORCH const landingPlace = { landing.x, landing.y, landing.z };

		placeLastChangedAt = now;

		if (carMovedWithHimOn && AcrossBetween(dyn->Position, landingPlace) < AcrossBetween(there, landingPlace))
		{
			TurnTowards(status, dyn, landing.x - dyn->Position.vx, landing.z - dyn->Position.vz);
			WalkForward(status);
			return;
		}

		TurnTowards(status, dyn, there.vx - dyn->Position.vx, there.vz - dyn->Position.vz);
		return;
	}

	// The first is on a car, and this one isn't. He gets on after him if the car is stopped level with him, the first is wholly on it, there's room for a metre between them, and he's near
	// enough to be on before it goes. Otherwise he waits at the landing for the next trip.
	if (carTheFirstIsOn >= 0)
	{
		Lift const &lift = lifts[(size_t)carTheFirstIsOn];
		VECTORCH onTheCar;
		bool const room = ASpotOnTheCarAwayFrom(lift, there, &onTheCar);
		bool const level = CarIsStoppedLevelWith(lift, dyn->Position.vy);
		bool const firstIsWhollyOn = OverTheCar(lift, there, -kHudsonGridMarineHalfWidth);
		bool const nearEnough = OverTheCar(lift, dyn->Position, kJoinTheRideFromWithin);
		bool const alreadyGettingOn = OverTheCar(lift, dyn->Position, kHudsonGridMarineHalfWidth);

		if (room && (level || alreadyGettingOn) && firstIsWhollyOn && (nearEnough || alreadyGettingOn))
		{
			sayAboutTheCar = "gets on after the first, on the other side of the car from him";

			if (sayAboutTheCar != saidAboutTheCar)
			{
				saidAboutTheCar = sayAboutTheCar;
				SayAboutTheLift(lift, sayAboutTheCar);
			}

			placeLastChangedAt = now;
			TurnTowards(status, dyn, onTheCar.vx - dyn->Position.vx, onTheCar.vz - dyn->Position.vz);

			if (AcrossBetween(dyn->Position, onTheCar) > 300)
			{
				WalkForward(status);
			}

			return;
		}

		sayAboutTheCar = std::string("waits at the landing for the next trip, because ") + (!room ? "the car has no room for both with a metre between them" : !level ? "the car isn't level with him" : !firstIsWhollyOn ? "the first isn't wholly on it yet" : "he's too far from it to be on before it goes");

		if (sayAboutTheCar != saidAboutTheCar && (level || !room))
		{
			saidAboutTheCar = sayAboutTheCar;
			SayAboutTheLift(lift, sayAboutTheCar);
		}

		placeLastChangedAt = now;
		TurnTowards(status, dyn, there.vx - dyn->Position.vx, there.vz - dyn->Position.vz);

		// Waiting at the landing, he still looks for a way every so often, so that once the first has gone up, he plans a ride of his own
		if (now - lookedForHimAt >= kLookForHimEvery * 3)
		{
			StartFindingTheWay(dyn->Position);
		}

		return;
	}

	saidAboutTheCar.clear();

	// At the spot behind him, or as near the first as he ever stops: this is where he wants to be, so standing still here isn't being stuck
	if (distance <= kStopsWithin || AcrossBetween(dyn->Position, spot) <= kAtTheSpotWithin)
	{
		placeLastChangedAt = now;
		timesStuckHere = 0;
		TurnTowards(status, dyn, there.vx - dyn->Position.vx, there.vz - dyn->Position.vz);
		return;
	}

	// Too far behind for longer than the first waits for him. Why he's behind comes first: a door shut on this machine here is a sync failure, and the table looks for these lines.
	if (farBehindSince != 0 && now - farBehindSince >= kCatchUpAfter)
	{
		farBehindSince = 0;
		SayAboutTheStep("fell behind: " + TheDoorBetween(dyn->Position, there));
		SayWhatsHoldingHim();
		MoveHimUpBehindHim(there, "he's been more than " + std::to_string(kTooFarBehind) + " mm behind for " + std::to_string((int)(kCatchUpAfter / 1000)) + " seconds, which is longer than the first waits for him, and is " + std::to_string(distance) + " mm behind now", now);
		return;
	}

	if (now - lookedForHimAt >= kLookForHimEvery)
	{
		StartFindingTheWay(dyn->Position);
		return;
	}

	// The grid knows no way to him, but he's near and on much the same level, so straight at him, the way a player would go. It isn't a cheat: if something's in the way, it stops him. Not onto
	// a lift's car that isn't stopped level with him, though, which would be walking into the shaft: then he stands and waits.
	bool const straightAtHim = !wayGoesAllTheWay && distance <= kWalkStraightAtHimWithin && upOrDown <= kSameLevelWithin && upOrDown >= -kSameLevelWithin;

	if (straightAtHim && OverACarThatIsntLevelWith(there, dyn->Position.vy))
	{
		placeLastChangedAt = now;
		TurnTowards(status, dyn, there.vx - dyn->Position.vx, there.vz - dyn->Position.vz);
		return;
	}

	if (straightAtHim != walkingStraightAtHim)
	{
		walkingStraightAtHim = straightAtHim;
		SayAboutTheStep(straightAtHim ? "the grid knows no way to the other Hudson, " + std::to_string(distance) + " mm away, so he walks straight at him" : "he goes back to the grid's way");
	}

	if (straightAtHim)
	{
		TurnTowards(status, dyn, spot.vx - dyn->Position.vx, spot.vz - dyn->Position.vz);
		WalkForward(status);
		return;
	}

	WalkOn(status, now);
}

// ---- Waiting, for the first of two Hudsons

// True while he's standing, waiting for the second. He waits when the second is far behind, and, when he's asked to, until the second is beside him: before each new leg, before a switch, and
// before he gets on a lift. The log says why and for how long. The wait ends when the second is beside him, or when it has lasted as long as it may.
static bool WaitingForTheOther(PLAYER_STATUS *status, DWORD now, bool wantsHimBeside)
{
	if (following || !otherPlayer || !otherPlayer->DynPtr)
	{
		return false;
	}

	DYNAMICSBLOCK const *const dyn = Player->ObStrategyBlock->DynPtr;
	VECTORCH const there = otherPlayer->DynPtr->Position;
	int const distance = DistanceTo(dyn->Position, there);
	bool const besideHim = distance <= kBesideHim;

	// After a wait that ran out, he waits again once the second has been beside him, or after a while anyway: in the first lift pair the second was stuck 3.5 m away, and was then left
	// 36 m behind for good
	if (besideHim || (!mayWaitAgain && now - waitRanOutAt >= kWaitAgainAfter))
	{
		mayWaitAgain = true;
	}

	if (waitingSince == 0)
	{
		bool const farBehind = distance > kWaitWhenFurtherThan;
		bool const notBesideHim = wantsHimBeside && !besideHim;

		if (!mayWaitAgain || (!farBehind && !notBesideHim))
		{
			return false;
		}

		waitingSince = now;
		SayAboutTheStep("he waits for the other Hudson, who is " + std::to_string(distance) + " mm away, " + (farBehind ? std::string("too far behind") : "so that both go on to " + StepTitle() + " together"));
	}

	DWORD const waited = now - waitingSince;

	if (besideHim || waited >= kWaitAtMost)
	{
		char seconds[20];

		_snprintf(seconds, sizeof(seconds) - 1, "%.1f", waited / 1000.0);
		seconds[sizeof(seconds) - 1] = 0;
		SayAboutTheStep(besideHim ? std::string("the other Hudson is beside him after a wait of ") + seconds + " seconds, so he goes on"
			: std::string("he waited ") + seconds + " seconds and the other Hudson is still " + std::to_string(distance) + " mm away, so he goes on without him");
		waitingSince = 0;
		waitsDone++;
		waitedMilliseconds += waited;
		mayWaitAgain = besideHim;
		waitRanOutAt = now;

		// Waiting is neither being stuck nor taking too long over the step
		stepStartedAt += waited;
		placeLastChangedAt = now;
		return false;
	}

	placeLastChangedAt = now;
	TurnTowards(status, dyn, there.vx - dyn->Position.vx, there.vz - dyn->Position.vz);
	return true;
}

// A lift's car goes to its other end whenever someone stands on it, again and again, in a game of one too (bh_platform_lift.cpp). So with nothing left to do, he doesn't stand on one: once it
// has stopped, he walks off it, towards where his last step was.
static void StepOffALift(PLAYER_STATUS *status)
{
	DYNAMICSBLOCK const *const dyn = Player->ObStrategyBlock->DynPtr;
	STRATEGYBLOCK const *lift = 0;

	for (COLLISIONREPORT const *report = dyn->CollisionReportPtr; report; report = report->NextCollisionReportPtr)
	{
		if (report->ObstacleSBPtr && report->ObstacleSBPtr->I_SBtype == I_BehaviourPlatform && report->ObstacleSBPtr->SBdataptr)
		{
			lift = report->ObstacleSBPtr;
		}
	}

	if (!lift || steps.empty())
	{
		saidHesOnALift = false;
		return;
	}

	PLATFORMLIFT_BEHAVIOUR_BLOCK const *const car = (PLATFORMLIFT_BEHAVIOUR_BLOCK const *)lift->SBdataptr;

	if (car->state == PLBS_GoingUp || car->state == PLBS_GoingDown)
	{
		return;
	}

	if (!saidHesOnALift)
	{
		saidHesOnALift = true;
		SayAboutTheStep("he's standing on " + HudsonStrategies_Title(lift) + " with nothing left to do, so he walks off it");
	}

	HudsonPlayStep const &last = steps.back();

	TurnTowards(status, dyn, last.x - dyn->Position.vx, last.z - dyn->Position.vz);
	WalkForward(status);
}

// ---- What each machine sees: the step's switches and the lifts, said when they change, with the Windows clock, which is the same for both games on one machine. The two logs line up on it.

// Says what something is now, if that isn't what it was last time. "more" goes on the end of the line, but isn't part of what's compared: a moving car's height would be new every time.
static void SayWhenOneChanges(std::string const &what, std::string const &state, std::string const &more)
{
	const auto known = watchedStates.find(what);

	if (known != watchedStates.end() && known->second == state)
	{
		return;
	}

	char time[20];

	_snprintf(time, sizeof(time) - 1, "%lu", (unsigned long)timeGetTime());
	time[sizeof(time) - 1] = 0;
	LogString(std::string("play: watch: t=") + time + " " + what + (known == watchedStates.end() ? " is " : " was " + known->second + ", and is now ") + state + more);
	watchedStates[what] = state;
}

static void LookAtWhatsWatched(DWORD now)
{
	if (now - lookedAtWhatsWatchedAt < kLookAtWhatsWatchedEvery)
	{
		return;
	}

	lookedAtWhatsWatchedAt = now;

	for (HudsonPlayStep const &step : watchedSteps)
	{
		STRATEGYBLOCK const *const target = HudsonStrategies_Find(step.name.c_str());

		SayWhenOneChanges(step.name, target ? HudsonStrategies_State(target) : std::string("not here"), "");
	}
}

// ---- Doing the step, once he's there

static void DoThePress(PLAYER_STATUS *status, DWORD now)
{
	HudsonPlayStep const &step = steps[stepAt];
	STRATEGYBLOCK *const target = HudsonStrategies_Find(step.name.c_str());
	DYNAMICSBLOCK const *const dyn = Player->ObStrategyBlock->DynPtr;

	if (!target)
	{
		SayAboutTheStep(step.name + " has gone");
		NextStep(false);
		return;
	}

	std::string const state = HudsonStrategies_State(target);

	if (pressesTried > 0 && state != stateBefore)
	{
		SayAboutTheStep("the press worked: " + step.name + " was " + stateBefore + ", and is now " + state);
		NextStep(true);
		return;
	}

	// Before the first press, the other Hudson comes to his side, so that both go on together
	if (pressesTried == 0 && switchWaitIsDue)
	{
		if (WaitingForTheOther(status, now, true))
		{
			return;
		}

		switchWaitIsDue = false;
	}

	if (pressesTried >= kMostPresses && now - lastPressAt >= kTimeBetweenPresses)
	{
		// All of that from too far away says nothing about the switch, so he gets one go at it from close up
		if (!movedToThisStep && DistanceTo(Global_VDB_Ptr->VDB_World, WhereItIs(target, StepPlace())) > kUseKeyReachesAhead)
		{
			MoveHimToTheStep("the use key doesn't reach from here", now);
			pressesTried = 0;
			lastPressAt = 0;
			return;
		}

		SayAboutTheStep("he pressed " + step.name + " " + std::to_string(kMostPresses) + " times and it stayed " + state + ", so he goes on");
		NextStep(false);
		return;
	}

	VECTORCH const place = WhereItIs(target, StepPlace());
	int const toX = place.vx - dyn->Position.vx, toZ = place.vz - dyn->Position.vz;

	TurnTowards(status, dyn, toX, toZ);
	LookUpOrDownAt(status, place);

	bool const facing = HudsonPlay_AimedWell(HudsonPlay_AngleToTurn(dyn->OrientMat.mat31, dyn->OrientMat.mat33, toX, toZ));

	if (facing && pressesTried < kMostPresses && (lastPressAt == 0 || now - lastPressAt >= kTimeBetweenPresses))
	{
		int const distance = DistanceTo(Global_VDB_Ptr->VDB_World, place);

		stateBefore = state;
		lastPressAt = now;
		pressesTried++;
		status->Mvt_InputRequests.Flags.Rqst_Operate = 1;
		SayAboutTheStep("he presses the use key at " + step.name + ", which is " + state + ", from " + std::to_string(distance) + " mm" + (distance > kUseKeyReachesAhead ? ", which is further than it reaches" : ""));
	}
}

static void DoTheShooting(PLAYER_STATUS *status, DWORD now, DWORD patience)
{
	HudsonPlayStep const &step = steps[stepAt];
	STRATEGYBLOCK *const target = HudsonStrategies_Find(step.name.c_str());

	if (!target || target->SBflags.please_destroy_me || target->SBDamageBlock.Health <= 0)
	{
		SayAboutTheStep(step.name + " is gone");
		NextStep(true);
		return;
	}

	if (shootingStartedAt == 0)
	{
		shootingStartedAt = now;
	}

	if (now - shootingStartedAt >= patience)
	{
		KillItForHim(target, step.name, "he's been at it for " + std::to_string((int)(patience / 1000)) + " seconds");
		NextStep(false);
		return;
	}

	// He only fires at what he can see. If it's gone somewhere else, he goes after it, the way a player would.
	if (!ItsInSightAndRange(target))
	{
		StartFindingTheWay(Player->ObStrategyBlock->DynPtr->Position);
		return;
	}

	VECTORCH place;

	GetTargetingPointOfObject_Far(target, &place);
	FireAt(status, place);
}

static void DoThePickUp(PLAYER_STATUS *status, DWORD now)
{
	HudsonPlayStep const &step = steps[stepAt];
	STRATEGYBLOCK const *const target = HudsonStrategies_Find(step.name.c_str());

	// Walking onto it is what picks it up, so he keeps walking at it until it goes
	if (!target || target->SBflags.please_destroy_me)
	{
		SayAboutTheStep(step.name + " has gone from where it was");
		NextStep(true);
		return;
	}

	if (now - phaseStartedAt >= kWaitToPickUp)
	{
		if (!movedToThisStep)
		{
			MoveHimToTheStep(step.name + " is still there and he couldn't walk onto it", now);
			phaseStartedAt = now;
			return;
		}

		SayAboutTheStep(step.name + " is still there, so he goes on");
		NextStep(false);
		return;
	}

	DYNAMICSBLOCK const *const dyn = Player->ObStrategyBlock->DynPtr;
	VECTORCH const place = WhereItIs(target, StepPlace());
	int const toX = place.vx - dyn->Position.vx, toZ = place.vz - dyn->Position.vz;

	// Only measured across, because it may be lying on the floor and his middle is well above that
	if (!HudsonPlay_ReachedPoint(toX, toZ, 0, kPickedUpFrom, 1))
	{
		TurnTowards(status, dyn, toX, toZ);
		WalkForward(status);
	}
}

static void DoTheStep(PLAYER_STATUS *status, DWORD now)
{
	HudsonPlayStep const &step = steps[stepAt];

	switch (step.kind)
	{
		case kHudsonPlayPress:
			DoThePress(status, now);
			break;

		case kHudsonPlayWalkInto:
			if (!TheWalkInSwitchWentOff() && now - phaseStartedAt >= kStandOnAPlaceFor)
			{
				SayAboutTheStep("he got to " + step.name + " and it stayed " + stateBefore + ", so he goes on");
				NextStep(false);
			}
			break;

		case kHudsonPlayPickUp:
			DoThePickUp(status, now);
			break;

		case kHudsonPlayDestroy:
			DoTheShooting(status, now, kGiveUpOnShootingSomething);
			break;

		default:
			DoTheShooting(status, now, kGiveUpOnAKill);
			break;
	}
}

// ---- What's kept for watching him. None of this changes what he does.

// Where he is goes on the trail each time he's gone far enough. A very long run loses its oldest part, rather than going on growing.
static void AddToTheTrail()
{
	VECTORCH const placeNow = Player->ObStrategyBlock->DynPtr->Position;
	HudsonGridPoint const point = { placeNow.vx, placeNow.vy, placeNow.vz };

	if (!trail.empty())
	{
		double const x = (double)point.x - trail.back().x, z = (double)point.z - trail.back().z;

		if (x * x + z * z < (double)kTrailPointEvery * kTrailPointEvery)
		{
			return;
		}
	}

	if (trail.size() >= kTrailMostPoints)
	{
		trail.erase(trail.begin(), trail.begin() + kTrailMostPoints / 8);
	}

	trail.push_back(point);
}

// How far he is from his way this frame, and how long he's been pushing without getting anywhere, for the run's result. Only while he's walking the way: a fight takes him off it on purpose.
static void MeasureHowWellHesFollowing(DWORD now)
{
	DWORD const sinceLastFrame = lastFrameAt ? now - lastFrameAt : 0;

	lastFrameAt = now;

	// Only what was worked out while he was walking on the way last frame
	if (phase != kWalking || !walkedLastFrame || !onTheWayNow)
	{
		return;
	}

	strayAdded += offTheWayNow;
	strayFrames++;
	strayMost = offTheWayNow > strayMost ? offTheWayNow : strayMost;

	// Past half of what counts as stuck, near enough the same place, and trying to walk
	if (walkedLastFrame && now - placeLastChangedAt > kHudsonPlayStuckAfter / 2)
	{
		stuckMilliseconds += sinceLastFrame;
	}
}

// One line a second, for reading a stall afterwards: where he is, where he's making for, how fast the game is asked to move him and how far he really went, and what he's touching.
static void SayThisSecond(PLAYER_STATUS const *status, DWORD now)
{
	if (now - saidThisSecondAt < 1000)
	{
		return;
	}

	saidThisSecondAt = now;
	secondsSaid++;

	DYNAMICSBLOCK const *const dyn = Player->ObStrategyBlock->DynPtr;
	VECTORCH const placeNow = dyn->Position;
	double const movedX = (double)placeNow.vx - placeASecondAgo.vx, movedZ = (double)placeNow.vz - placeASecondAgo.vz;
	int const moved = (int)sqrt(movedX * movedX + movedZ * movedZ);
	double const speedX = dyn->LinVelocity.vx, speedZ = dyn->LinVelocity.vz;
	char text[300];

	placeASecondAgo = placeNow;

	if (wayAt < way.size())
	{
		HudsonGridPoint const &point = way[wayAt];
		int const toX = point.x - placeNow.vx, toZ = point.z - placeNow.vz;

		_snprintf(text, sizeof(text) - 1, "second %d: at %d %d %d, point %d of %d at %d %d %d, %d mm across, %d up or down, %d off the way, turn off by %d, walk asked %d, speed asked %d, moved %d, on the floor %d, health %d", secondsSaid, placeNow.vx, placeNow.vy, placeNow.vz, (int)wayAt + 1, (int)way.size(), point.x, point.y, point.z, (int)sqrt((double)toX * toX + (double)toZ * toZ), point.y - placeNow.vy, offTheWayNow,
			HudsonPlay_AngleToTurn(dyn->OrientMat.mat31, dyn->OrientMat.mat33, toX, toZ), walkedLastFrame ? 1 : 0, (int)sqrt(speedX * speedX + speedZ * speedZ), moved, dyn->IsInContactWithFloor ? 1 : 0, Player->ObStrategyBlock->SBDamageBlock.Health >> ONE_FIXED_SHIFT);
	}
	else
	{
		_snprintf(text, sizeof(text) - 1, "second %d: at %d %d %d, no point to walk to, walk asked %d, speed asked %d, moved %d, on the floor %d, health %d", secondsSaid, placeNow.vx, placeNow.vy, placeNow.vz, walkedLastFrame ? 1 : 0, (int)sqrt(speedX * speedX + speedZ * speedZ), moved, dyn->IsInContactWithFloor ? 1 : 0, Player->ObStrategyBlock->SBDamageBlock.Health >> ONE_FIXED_SHIFT);
	}

	text[sizeof(text) - 1] = 0;

	std::string line = std::string("play: ") + text + ", in room " + RoomOf(Player->ObStrategyBlock);
	int shown = 0;

	for (COLLISIONREPORT const *report = dyn->CollisionReportPtr; report && shown < 3; report = report->NextCollisionReportPtr, shown++)
	{
		char facing[60];

		_snprintf(facing, sizeof(facing) - 1, " facing %d %d %d", report->ObstacleNormal.vx, report->ObstacleNormal.vy, report->ObstacleNormal.vz);
		facing[sizeof(facing) - 1] = 0;
		line += std::string(", touching ") + (report->ObstacleSBPtr ? HudsonStrategies_Title(report->ObstacleSBPtr) : std::string("the level")) + facing;
	}

	LogString(line);
}

// ---- Each frame

bool HudsonPlay_RibbonIsOn()
{
	return HudsonRunIsOn && ribbonIsOn;
}

void HudsonPlay_RibbonCorners(std::vector<HudsonPlayRibbonCorner> &corners)
{
	if (!HudsonRunIsOn)
	{
		corners.clear();
		return;
	}

	corners.clear();

	// The route goes in first, because that's what's being watched. It starts from the point he last passed.
	HudsonPlayRibbon_AddBand(way, wayAt > 0 ? wayAt - 1 : 0, kRouteOffTheFloor, kRouteColour, corners);

	for (size_t i = 0; i < steps.size(); i++)
	{
		HudsonPlayRibbon_AddBlock(steps[i].x, steps[i].y, steps[i].z, kStepBlockHalfWidth, kStepBlockHeight, i < stepAt ? kStepDoneColour : kStepToDoColour, corners);
	}

	for (HudsonGridPoint const &place : cheatPlaces)
	{
		HudsonPlayRibbon_AddBlock(place.x, place.y, place.z, kCheatPostHalfWidth, kCheatPostHeight, kCheatColour, corners);
	}

	HudsonPlayRibbon_AddBand(trail, 0, kTrailOffTheFloor, kTrailColour, corners);
}

// Reads the level's list of steps from HudsonChains. False if it isn't there or was refused, and the log says why.
static bool ReadTheList(const char *levelName, std::vector<HudsonPlayStep> *into)
{
	std::string const path = std::string("HudsonChains\\") + levelName + ".play";
	FILE *const file = fopen(path.c_str(), "rb");

	if (!file)
	{
		LogString("play: there's no list of steps for this level: " + path);
		return false;
	}

	// One byte more than the limit, so a file that's too big is seen to be, and refused for it
	std::string text(kHudsonPlayMostBytes + 1, '\0');

	text.resize(fread(&text[0], 1, text.size(), file));
	fclose(file);

	std::string whyNot;

	if (!HudsonPlay_ReadSteps(text, into, &whyNot))
	{
		LogString("play: the list of steps was refused: " + whyNot);
		return false;
	}

	return true;
}

bool HudsonPlay_Start(const char *levelName)
{
	if (!ReadTheList(levelName, &steps))
	{
		return false;
	}

	LogString("play: this level has " + std::to_string((int)steps.size()) + " steps");
	watchedSteps = steps;

	stepAt = 0;
	stepsThatWorked = 0;
	cheatsUsed = 0;
	ribbonIsOn = true;
	phase = kWaitingForTheGrid;
	HudsonNav_StartTheGrid();

	return true;
}

void HudsonPlay_StartFollowing(const char *levelName)
{
	LogString("play: this is the second Hudson, so he follows the first and fights what he sees");

	// He doesn't do the steps, but this machine says what becomes of each one's switch, so that the two logs can be compared. Without the list, there's only the lifts.
	if (!ReadTheList(levelName, &watchedSteps))
	{
		watchedSteps.clear();
	}

	following = true;
	ribbonIsOn = true;
	cheatsUsed = 0;
	phase = kWaitingForTheGrid;
	HudsonNav_StartTheGrid();
}

void HudsonPlay_Input(void *playerStatus, const void *theOtherPlayer)
{
	PLAYER_STATUS *const status = (PLAYER_STATUS *)playerStatus;
	DWORD const now = GetTickCount();

	otherPlayer = (const STRATEGYBLOCK *)theOtherPlayer;

	if ((steps.empty() && !following) || phase == kStopped || !status->IsAlive)
	{
		return;
	}

	KeepHimAlive(status, now);
	AddToTheTrail();
	SayThisSecond(status, now);
	SeeToHisGun(status);
	WatchWhereTheFirstIs(now);
	MeasureHowWellHesFollowing(now);
	walkedLastFrame = false;

	if (now - saidWhereAt >= kSayWhereHeIsEvery && phase != kWaitingForTheGrid && (following || stepAt < steps.size()))
	{
		saidWhereAt = now;
		SayAboutTheStep(std::string("he's on his way to ") + StepTitle() + ", with health " + std::to_string(Player->ObStrategyBlock->SBDamageBlock.Health >> ONE_FIXED_SHIFT));
	}

	// This comes before anything else, so he isn't left standing there while the grid's being built or the level's ending
	bool const fighting = Fighting(status, now);

	if (phase == kWaitingForTheGrid)
	{
		if (HudsonNav_TheGridIsStillBuilding())
		{
			return;
		}

		if (!HudsonNav_TheGrid().Ready())
		{
			LogString("play: there's no grid, so he can't find his way");
			Stop(kHudsonExitPlayStuck, "there was no grid to find his way on");
			return;
		}

		phase = following ? kWalking : kStartingAStep;
		farBehindSince = 0;
	}

	if (phase == kFinishedTheList)
	{
		StepOffALift(status);

		// Nothing's left to do, and the game hasn't ended the level, so it either takes a while or it won't
		if (now - phaseStartedAt >= kWaitForTheLevelToEnd)
		{
			LogString("play: he's been down the whole list, " + std::to_string(stepsThatWorked) + " of " + std::to_string((int)steps.size()) + " steps worked, with " + std::to_string(cheatsUsed) + " cheats, and the level didn't end");
			Stop(kHudsonExitLevelDidntEnd, "he's been down the whole list, and the level didn't end");
		}

		return;
	}

	if (fighting)
	{
		return;
	}

	VECTORCH const here = Player->ObStrategyBlock->DynPtr->Position;

	switch (phase)
	{
		case kStartingAStep:
		{
			HudsonPlayStep const &step = steps[stepAt];
			STRATEGYBLOCK const *const target = HudsonStrategies_Find(step.name.c_str());

			phaseStartedAt = now;
			shootingStartedAt = 0;
			placeLastSeenAt = here;
			placeLastChangedAt = now;
			timesStuckHere = 0;
			rescueUntil = 0;
			stepStartedAt = now;
			movedToThisStep = false;
			legWaitIsDue = true;
			switchWaitIsDue = true;
			noLiftHelps = false;
			ridesThisStep = 0;
			SayAboutTheStep(StepTitle());

			if (!target)
			{
				SayAboutTheStep("there's nothing called " + step.name + " here, so he goes on");
				NextStep(false);
				break;
			}

			stateBefore = HudsonStrategies_State(target);
			StartFindingTheWay(here);
			break;
		}

		case kFindingTheWay:
		{
			HudsonGrid &grid = HudsonNav_TheGridToSearch();
			HudsonGridSearch const search = grid.Work(kPlacesToSearchEachFrame);

			if (search == kHudsonGridSearching)
			{
				break;
			}

			way = grid.Way();
			wayAt = 0;
			wayGoesAllTheWay = search == kHudsonGridFound;
			phase = kWalking;

			// No way on the grid: perhaps a lift joins the two. Not while he's on his way to one already, nor for the second while the first is near and on his level, when he walks straight at him.
			if (!wayGoesAllTheWay && rideStage == kNotRiding && !(following && otherPlayer && DistanceTo(here, otherPlayer->DynPtr->Position) <= kWalkStraightAtHimWithin
				&& otherPlayer->DynPtr->Position.vy - here.vy <= kSameLevelWithin && here.vy - otherPlayer->DynPtr->Position.vy <= kSameLevelWithin))
			{
				HudsonPlayStep const *const step = following ? 0 : &steps[stepAt];
				bool const staysPut = step && (step->kind == kHudsonPlayPress || step->kind == kHudsonPlayWalkInto);
				VECTORCH const there = following ? otherPlayer->DynPtr->Position : staysPut ? StepPlace() : WhereItIs(HudsonStrategies_Find(step->name.c_str()), StepPlace());

				if (TryALift(here, there, now))
				{
					break;
				}
			}

			// Someone following looks again every few seconds, which mustn't count as having got somewhere, or he'd never be found to be stuck
			if (!following)
			{
				placeLastChangedAt = now;
				SayAboutTheStep("the grid's way has " + std::to_string((int)way.size()) + " points, and " + (wayGoesAllTheWay ? "goes all the way" : "only goes as near as it could"));
			}

			break;
		}

		case kWalking:
			if (RidingALift(status, now))
			{
				break;
			}

			if (following)
			{
				FollowHim(status, now);
			}
			else
			{
				WalkAlongTheWay(status, now);
			}

			break;

		case kDoingTheStep:
			DoTheStep(status, now);
			break;

		default:
			break;
	}
}

int HudsonPlay_EndOfFrame(const char **howItEnded)
{
	PLAYER_STATUS const *const status = Player && Player->ObStrategyBlock ? (PLAYER_STATUS const *)Player->ObStrategyBlock->SBdataptr : NULL;

	// In a game of two, the game has already dealt with the level's end by now: it clears the flag and calls for the next level. This run is one level, so that call is put off
	// here, or the game would go on and load it with nothing left watching.
	bool const nextLevelWasCalledFor = Coop_LevelChangeIsDue();

	if (nextLevelWasCalledFor)
	{
		Coop_LevelChangeHasStarted();
	}

	if (AvP.LevelCompleted || nextLevelWasCalledFor)
	{
		LogString("play: the game ended the level by itself, with " + std::to_string(stepsThatWorked) + " of " + std::to_string((int)steps.size()) + " steps worked and " + std::to_string(cheatsUsed) + " cheats");

		if (cheatsUsed > 0)
		{
			*howItEnded = "he got to the level's end with cheats, so this doesn't show a player can";
			return kHudsonExitPlayFinishedWithCheats;
		}

		*howItEnded = "he played the level to its end";
		return kHudsonExitPlayFinished;
	}

	// A move asked for during the frame. Nothing is moved outside the level's rooms, because the game can't cope with a player who isn't in one.
	if (moveIsWanted)
	{
		moveIsWanted = false;

		if (ModuleFromPosition(&moveHimTo, (MODULE *)0))
		{
			DYNAMICSBLOCK *const dyn = Player->ObStrategyBlock->DynPtr;

			Hudson_MoveHimOnFoot(moveHimTo);
			dyn->LinVelocity.vx = dyn->LinVelocity.vy = dyn->LinVelocity.vz = 0;
		}
		else
		{
			LogString("play: that place isn't in any room, so he stays where he is");
		}
	}

	if (status && !status->IsAlive && phase != kStopped)
	{
		SayAboutTheStep("he died");
		Stop(kHudsonExitPlayerDied, "he died");
	}

	if (phase == kStopped)
	{
		*howItEnded = endWords;
		return endCode;
	}

	return 0;
}

void HudsonPlay_WatchTheLevel(const void *theOtherPlayer)
{
	DWORD const now = GetTickCount();

	if (!Player || !Player->ObStrategyBlock || !Player->ObStrategyBlock->DynPtr)
	{
		return;
	}

	otherPlayer = (const STRATEGYBLOCK *)theOtherPlayer;
	LookAtWhatsWatched(now);

	// The user's rule: never on top of each other. Every frame the two bodies touch is said, with the time and whether it's near a lift, and the reader counts them.
	if (otherPlayer)
	{
		for (COLLISIONREPORT const *report = Player->ObStrategyBlock->DynPtr->CollisionReportPtr; report; report = report->NextCollisionReportPtr)
		{
			if (report->ObstacleSBPtr == otherPlayer)
			{
				VECTORCH const here = Player->ObStrategyBlock->DynPtr->Position;
				bool nearALift = false;

				for (Lift const &lift : lifts)
				{
					nearALift = nearALift || OverTheCar(lift, here, kNearALiftAcross);
				}

				LogString("touch: t=" + std::to_string((unsigned long)timeGetTime()) + " the two touch, at " + std::to_string(here.vx) + " " + std::to_string(here.vy) + " " + std::to_string(here.vz) + (nearALift ? ", near a lift" : ", not near a lift"));
				break;
			}
		}
	}
	SayEachLiftsState(now);
	WatchForAFall();

	if (HudsonNav_TheGrid().Ready())
	{
		SayWhereEachIsNearALift(now);
	}
}

void HudsonPlay_KeepHimAliveWhileHeWaits(void *playerStatus)
{
	PLAYER_STATUS *const status = (PLAYER_STATUS *)playerStatus;

	if (status && status->IsAlive && Player && Player->ObStrategyBlock)
	{
		KeepHimAlive(status, GetTickCount());
	}
}

void HudsonPlay_SetStepSeconds(int seconds)
{
	// The plan reader only takes 10 to 600; anything else here keeps the usual limit, so a mistake can't make a step endless or instant
	aStepHasTakenTooLong = seconds >= 10 && seconds <= 600 ? (DWORD)seconds * 1000 : kAStepHasTakenTooLong;
}

void HudsonPlay_HoldGun(int slot)
{
	gunToHold = slot;
}

void HudsonPlay_ForTheResult(char *line, int size)
{
	_snprintf(line, size - 1, "play_steps=%d play_steps_worked=%d play_cheats=%d play_stray_most=%d play_stray_average=%d play_big_turns=%d play_stuck_times=%d play_stuck_seconds=%.1f play_waits=%d play_waited_seconds=%.1f play_rides=%d", (int)steps.size(), stepsThatWorked, cheatsUsed, strayMost, strayFrames ? (int)(strayAdded / strayFrames) : 0, bigTurns, stuckTimes, stuckMilliseconds / 1000.0, waitsDone, waitedMilliseconds / 1000.0, ridesDone);
	line[size - 1] = 0;
}
