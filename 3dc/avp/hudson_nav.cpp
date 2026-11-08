// Hudson's navigation: how he walks through a level by himself. Written 2026-11-05 by Melanikus for the AvP port.
//
// WHAT THIS IS. With nav= on the command line, Hudson starts in the middle of the room, standing on its floor, and walks out of it, one module after another, until he's reached as many new ones as he
// was asked to. This only ever runs in a Hudson run, which is a test copy of the game with nobody at the keyboard, and it refuses a network game.
//
// A ROUTE. With route= he doesn't choose where to go. He follows a file that says the way through the level: walk to this room, press this switch, walk to this spot. It's made from what the level's own
// logic needs, so a door a route goes through is one that the route has opened by then. hudson_nav_plan.h says what the file looks like.
//
// PRESSING SOMETHING. He walks up to it, waits to be on the floor, and turns to face it. If it's in his reach he presses it. His reach is what the player's use key allows, OperateObjectInLineOfSight in triggers.cpp: 3
// metres in front of him, 1 metre to the side or up or down, and in sight. Then it's the game's own RequestState, as it is for the player. The key itself can't be used, because it works from a ray to the player, and
// he's standing where the player is. The log says what the key would have made of it, for comparing. If it's out of his reach, it's asked anyway, because a run shouldn't stop there. That's counted apart from a real
// press, so it can never read as one.
//
// WAITING AT A DOOR. A door takes time, so being stuck next to one doesn't count, up to a limit, in case it never opens.
//
// HOW HE PLANS BY HIMSELF. A level is cut into modules, which are rooms and bits of corridor, and the level file says which module links to which, and where you come in when you go from one to the next. Those
// spots are called entry points. He looks for the nearest module he hasn't been to, counting how many modules it is away, and walks to the entry point of the next one on the way. When he's in a new
// room he walks to the middle of it and has a good look around before going on. Those are the same links and spots the marines the level sends at you use.
//
// HOW HE WALKS. Like any marine in the game, because that's what he is. A marine on guard who finds himself away from his spot walks back to it by himself: at a marine's speed, with a marine's walking
// animation, facing where he's going, going round things that block him, and going from room to room through the entry points. All of that is the game's own marine code, in Execute_MNS_SentryMode in
// bh_marine.cpp. So all this file does is keep moving his spot, with Marine_GuardThisSpot. Nothing here sets his speed. The game's own dynamics move him, so walls stop him, gravity holds him on the floor and
// steps lift him up. Two things about a guard shape how his spot is moved. He stops when he's within about a metre and a half of it, so it's always kept further ahead of him than that, along the way he's going,
// and he's there when he passes it. And if it's in another module he goes by the game's own way between modules, which can turn him back through the room he just left. So it's always in the module he's in.
//
// HOW FAST. A walk is held at 60 frames a second, for two reasons. Someone's watching it. And the game moves him by a whole number of millimetres each frame, so at a couple of hundred frames a second he goes well
// under the speed the marine's data gives him: 0.75 to 0.95 metres a second where the data says about 1.5. The log gives the speed of each leg as it really was. With pace=run he goes at the marine's combat speed
// instead, about 4 metres a second with the run animation. There's nothing between those in the game's data.
//
// WHAT THE MARINE CODE CAN'T DO, AND THE MAP. It walks straight at his spot, and it can get round something in the way, but not round a corner of the room itself. So a straight line that goes through a wall
// leaves him pressed against that wall. So before setting off, he checks the line with rays at his knees and at his head. If it's clear, he goes straight, which is most of the time. If it isn't, or he gets stuck
// anyway, he stands still for a moment and works out a way round, with the map in hudson_nav_graph.h. Then his spot keeps being moved along that way, a little ahead of him, always to somewhere he can see, so he
// still walks by the marine code. A few rays a frame is all the map gets, so that working it out can't hold a frame up.
//
// WHEN HE'S STUCK. If he's stayed within a metre of one spot for 5 seconds, and the map has already had its turn at that leg, the log says where, and he's picked up and put down at the spot he was going to,
// so a run keeps showing the next part of the level. A run has to be honest about that, so being put down is counted apart from walking: a module he reached by being put there never counts as walked. If he didn't walk into a single module, the
// run ends with kHudsonExitMarineStuck, whatever else happened. He's never picked up on the way to the middle of a room, because he's already in the room he was going to: he just carries on.
//
// THE LIDAR. Every so often he looks around with rays, using the game's own line of sight: eight ways round on the level, eight tilted down, eight tilted up, one straight up and one straight down, and
// one straight down from a step ahead of him, which is how a drop shows. Each ray's hit is sorted by which way the polygon it hit faces: flat floor, a slope he can walk on, too steep, wall or ceiling.
// The game's own collisions say what counts as walkable. The counts for each room go in the log when he leaves it. These only report. It's the map's own rays that find the way, by the same rules. All rays can
// only hit what's loaded at the time, which is the modules around him, so he can only see where he is and what's near.
//
// FINDING THE MIDDLE OF A ROOM. That uses the game's own test for whether a marine can stand somewhere, FarLoc_IsAPlaceToStand in pfarlocs.cpp, which reads the room's polygons. It's not done with the
// lidar's rays, because they only hit what's loaded, and whoever asks may be about to put him in a room that isn't. If the very middle has no floor, or that spot's in another room, it tries further out,
// a stride at a time. Those lists of places are also what the tour stands him at, so there's one way of doing this.
//
// HOW MUCH THIS CAN TAKE. The lists are as big as the level, up to the limits below. Working out where to go happens only when he's reached somewhere, and takes as long as the level is big. Each look
// around is a fixed number of rays, and the log has a limit on those lines, so a long run can't fill the disk.

#include "3dc.h"
#include "inline.h"
#include "module.h"
#include "strategy_def.h"
#include "gamedef.h"
#include "logString.h"
#include "bh_types.h"
#include "dynblock.h"
#include "bh_ais.h"
#include "bh_marine.h"
#include "pfarlocs.h"
#include "pvisible.h"
#include "los.h"
#include "targeting.h"
#include "hudson_plan.h"
#include "hudson_strategies.h"
#include "hudson_nav.h"
#include "hudson_nav_plan.h"
#include "hudson_nav_graph.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <windows.h>
#include <mmsystem.h>

extern int AIModuleArraySize;
extern int NormalFrameTime;
extern DISPLAYBLOCK *Player;
extern DISPLAYBLOCK *OnScreenBlockList[];
extern int NumOnScreenBlocks;

static int const kMostModules = 8192;            // a level with more than this has something wrong with it
static int const kMostLinksOfAModule = 64;       // the same goes for a module with more than this many neighbours

static int const kJustAboveTheFloor = 100;       // how far above a floor to put him. His own place is at his feet, so he barely drops
static int const kEyeAboveHisFeet = 1500;        // where he looks from
static int const kLookFarthest = 20000;          // past this, in millimetres, a ray counts as finding nothing
static int const kStepAhead = 1000;              // how far in front of him the drop is looked for
static int const kLowestStep = 500;              // floor lower than this under that spot is a drop, and not a step down
static long const kFramesBetweenLooks = 60;
static int const kMostSecondsToFace = 2 * kHudsonNavOneWhole;      // turning to face what he's pressing: long enough for a half turn, in the game's time
static int const kSecondsToWatchAPress = 2 * kHudsonNavOneWhole;   // how long he looks at it afterwards, so that someone can see it, and it has time to change
static int const kCloseEnoughToPress = 2200;                       // how close he walks to a thing to press it. It's in reach from 3000, and he doesn't stop dead
static int const kPressReachesFront = 3000;                        // what the player's use key allows: ACTIVATION_Z_RANGE, ACTIVATION_X_RANGE and ACTIVATION_Y_RANGE in triggers.cpp
static int const kPressReachesAcross = 1000;
static int const kPressReachesUpOrDown = 1000;
static int const kThicknessOfAWall = 300;                          // something on a wall can be a little way into it, so a ray that stops this close to it has got there
static int const kNearEnoughToARouteSpot = 3000;                   // a step of a route is done when he's in its room and this near its spot, because one room can have more than one level
static int const kMostDoorWaits = 4;                               // how many times the stuck rule gives way to a door on one leg: 5 seconds each, which is longer than a door takes
static int const kDoorIsWithin = 3000;                             // a door this close, on his way, is what he's waiting for
static int const kRaysForTheMapEachFrame = 64;                     // enough for about two spots a frame, so that it can't hold a frame up. The game's log says how long they really take
static long const kFramesBetweenSteers = 10;
static double const kSecondsForAFrame = 1.0 / 60.0;
static int const kMostRoomChangesOnAStep = 8;                      // a route's step that goes from room to room more than this is going round in circles
static int const kMostSecondsOnAStep = 120 * kHudsonNavOneWhole;   // and one that takes longer than this isn't getting there
static int const kMostPointsAhead = 12;                            // how far along the way he looks for somewhere he can see: 12 strides
static int const kLeadOfHisSpot = 2200;                            // how far ahead of him his spot is kept, because a guard stops about 1500 from it
static char const kRouteFolder[] = "hudson-routes";                // beside the game, and not in the run's own folder, because that's emptied before each run
static int const kMostLookLines = 2000;

static std::vector<std::vector<int> > links;     // for each module, the numbers of the modules he can walk to from it
static std::vector<char> beenThere;
static int modulesWanted = 0;
static int walkedInto = 0;
static int putInto = 0;
static int movedOn = 0;
static int lookLines = 0;
static int moduleHere = -1;                      // the module he was in at the end of the last frame
static int goingTo = -1;                         // the module he's walking to, or -1 if he isn't walking to one
static bool goingToTheMiddle = false;            // he's walking to the middle of the room he's in, and not to the next one
static bool justPutDown = false;                 // he was put down last frame, so the module he's in now isn't one he walked into
static VECTORCH target = { 0, 0, 0 };
static VECTORCH middleOfTheRoom = { 0, 0, 0 };
static HudsonNavProgress progress;
static int pressed = 0;
static int pressedByAsking = 0;
static int pressesThatDidSomething = 0;

// The map, and what he's doing about the leg he's on
enum WayState
{
	kWayLeftAlone,    // his spot stays where it was put: on his way to the middle of a room, or turning to face something
	kWayStraight,     // his spot is being kept ahead of him, on the line to where he's going
	kWaySearching,    // he's standing still, and the map's working out a way
	kWayFollowing     // his spot is being kept ahead of him, along the way the map found
};

static HudsonNavGraph theMap;
static WayState wayState = kWayLeftAlone;
static bool mapHasHadItsTurn = false;     // on this leg. The stuck rule comes after that
static AIMODULE *legModule = 0;
static int legsByMap = 0;
static int legsMapCouldntDo = 0;
static double mapMilliseconds = 0.0;
static double mapLongestFrame = 0.0;

// How fast he really went, on his own feet. Being put down, standing still to think and turning to face something don't count.
static VECTORCH placeLastFrame = { 0, 0, 0 };
static double legMetres = 0.0;
static double legSeconds = 0.0;
static double allMetres = 0.0;
static double allSeconds = 0.0;
static double lastFrameEnded = 0.0;
static int roomChangesOnThisStep = 0;
static int loops = 0;
static int timeOnThisStep = 0;

// Following a route
enum RouteStage
{
	kRouteStartTheStep,
	kRouteWalking,        // on his way to this step's place
	kRouteFacing,         // there, and turning to face what he's pressing
	kRouteWatching        // pressed it, and looking at what it does
};

static std::vector<HudsonNavStep> route;
static size_t routeStep = 0;
static RouteStage routeStage = kRouteStartTheStep;
static int timeInThisStage = 0;
static STRATEGYBLOCK *thingToPress = 0;
static VECTORCH thingIsAt = { 0, 0, 0 };
static std::string stateBeforePress;
static int doorWaits = 0;
static int raysInThisRoom[kHudsonNavRayKinds];

static const char *NameOfModule(const MODULE *module)
{
	return (module && module->name) ? module->name : "(no name)";
}

static const char *NameOfAIModule(int number)
{
	if (number < 0 || number >= AIModuleArraySize || !AIModuleArray[number].m_module_ptrs || !*AIModuleArray[number].m_module_ptrs)
	{
		return "(no name)";
	}

	return NameOfModule(*AIModuleArray[number].m_module_ptrs);
}

bool HudsonNav_MiddleOfRoom(int room, VECTORCH *place, int *away)
{
	if (room < 0 || room >= AIModuleArraySize)
	{
		return false;
	}

	// A room can be made of several parts, so it's the middle of the biggest.
	MODULE *biggest = 0;
	double biggestArea = -1.0;

	for (MODULE **part = AIModuleArray[room].m_module_ptrs; part && *part; part++)
	{
		double const area = (double)((*part)->m_maxx - (*part)->m_minx) * (double)((*part)->m_maxz - (*part)->m_minz);

		if ((*part)->m_mapptr && area > biggestArea)
		{
			biggest = *part;
			biggestArea = area;
		}
	}

	if (!biggest)
	{
		return false;
	}

	int const middleX = biggest->m_world.vx + (biggest->m_minx + biggest->m_maxx) / 2;
	int const middleZ = biggest->m_world.vz + (biggest->m_minz + biggest->m_maxz) / 2;

	for (int number = 0; number < kHudsonNavSpiralPlaces; number++)
	{
		int acrossX = 0, acrossZ = 0;

		HudsonNav_SpiralPlace(number, &acrossX, &acrossZ);

		VECTORCH here = { middleX + acrossX, biggest->m_world.vy, middleZ + acrossZ };

		if (FarLoc_IsAPlaceToStand(biggest, &here))
		{
			here.vy -= kJustAboveTheFloor;

			MODULE const *const found = ModuleFromPosition(&here, (MODULE *)0);

			// A spot this far out from the middle may be past the room's edge, in another room
			if (found && found->m_aimodule == &AIModuleArray[room])
			{
				*place = here;
				*away = (int)sqrt((double)acrossX * acrossX + (double)acrossZ * acrossZ);
				return true;
			}
		}
	}

	// Nowhere near the middle has a floor. So it's the nearest place on the list the game works out for its own marines and aliens to appear at, which were all tested the same way. They're written down
	// relative to the room, at the floor's height.
	if (!FALLP_AuxLocs)
	{
		return false;
	}

	FARLOCATIONSHEADER const &places = FALLP_AuxLocs[room];
	bool found = false;
	double nearest = 0.0;

	for (int i = 0; i < places.numLocations; i++)
	{
		VECTORCH here = places.locationsList[i];

		here.vx += AIModuleArray[room].m_world.vx;
		here.vy += AIModuleArray[room].m_world.vy - kJustAboveTheFloor;
		here.vz += AIModuleArray[room].m_world.vz;

		double const acrossX = (double)here.vx - middleX;
		double const acrossZ = (double)here.vz - middleZ;
		double const distance = sqrt(acrossX * acrossX + acrossZ * acrossZ);

		if ((!found || distance < nearest) && ModuleFromPosition(&here, (MODULE *)0))
		{
			*place = here;
			nearest = distance;
			found = true;
		}
	}

	*away = (int)nearest;
	return found;
}

// The list of which module he can walk to from which. An air duct is for aliens, and so is an entry point marked that way. A marine can't use either, so neither goes on the list.
static void MakeTheLinks()
{
	int const count = (AIModuleArraySize < kMostModules) ? AIModuleArraySize : kMostModules;

	links.assign(count, std::vector<int>());
	beenThere.assign(count, 0);

	for (int number = 0; number < count; number++)
	{
		AIMODULE *const module = &AIModuleArray[number];
		AIMODULE **next = module->m_link_ptrs;

		if (!next)
		{
			continue;
		}

		for (int i = 0; *next && i < kMostLinksOfAModule; i++, next++)
		{
			AIMODULE *const neighbour = *next;

			if (neighbour->m_index < 0 || neighbour->m_index >= count)
			{
				continue;
			}

			if (!neighbour->m_module_ptrs || !*neighbour->m_module_ptrs || ((*neighbour->m_module_ptrs)->m_flags & MODULEFLAG_AIRDUCT))
			{
				continue;
			}

			FARENTRYPOINT const *const way = GetAIModuleEP(neighbour, module);

			if (way && !way->alien_only)
			{
				links[number].push_back(neighbour->m_index);
			}
		}
	}
}

// Which module he's in, or -1 if he's in none the list knows.
static int ModuleHeIsIn(const STRATEGYBLOCK *marine)
{
	if (!marine->containingModule || !marine->containingModule->m_aimodule)
	{
		return -1;
	}

	int const number = marine->containingModule->m_aimodule->m_index;

	return (number >= 0 && number < (int)links.size()) ? number : -1;
}

// What the map asks: the game's own line of sight, leaving out him and the player.
class RaysOfTheGame : public HudsonNavRays
{
public:
	explicit RaysOfTheGame(const STRATEGYBLOCK *who) : marine(who) {}

	HudsonNavRayAnswer Cast(int x, int y, int z, int directionX, int directionY, int directionZ, int longest)
	{
		VECTORCH place = { x, y, z };
		VECTORCH way = { directionX, directionY, directionZ };

		FindPolygonInLineOfSight_TwoIgnores(&way, &place, 0, marine->SBdptr, Player);

		HudsonNavRayAnswer answer = { false, false, longest, 0 };

		if (LOS_ObjectHitPtr != 0 && LOS_Lambda <= longest)
		{
			answer.hit = true;
			answer.distance = LOS_Lambda;
			answer.normalY = LOS_ObjectNormal.vy;

			if (LOS_ObjectHitPtr->ObStrategyBlock)
			{
				AVP_BEHAVIOUR_TYPE const type = LOS_ObjectHitPtr->ObStrategyBlock->I_SBtype;

				answer.door = type == I_BehaviourProximityDoor || type == I_BehaviourLiftDoor || type == I_BehaviourSwitchDoor;
			}
		}

		return answer;
	}

private:
	const STRATEGYBLOCK *marine;
};

// Stops him where he stands, and has the map work out a way round to where he's going.
static void LookForAWay(STRATEGYBLOCK *marine, long frame)
{
	VECTORCH const place = marine->DynPtr->Position;
	AIMODULE *const hisModule = marine->containingModule ? marine->containingModule->m_aimodule : legModule;

	mapHasHadItsTurn = true;
	wayState = kWaySearching;

	if (hisModule)
	{
		Marine_GuardThisSpot(marine, &place, hisModule);
	}

	theMap.StartSearch(place.vx, place.vy, place.vz, target.vx, target.vy, target.vz);

	char line[160] = { 0 };

	_snprintf(line, sizeof(line) - 1, "nav: frame %ld, the straight line from %d %d %d to %d %d %d won't do, so he's working out a way round", frame, place.vx, place.vy, place.vz, target.vx, target.vy, target.vz);
	LogString(line);
}

static void SteerAt(STRATEGYBLOCK *marine, int x, int z);

// Tells him where to go, and starts measuring whether he's getting there. If the line isn't clear, the map has its turn straight away. stopsShort is how far before the spot the line has to be clear: something on a
// wall is at the end of a line that always hits that wall.
static void SendHimTo(STRATEGYBLOCK *marine, AIMODULE *module, long frame, int stopsShort = 0)
{
	VECTORCH const place = marine->DynPtr->Position;
	RaysOfTheGame rays(marine);

	legModule = module;
	mapHasHadItsTurn = false;
	doorWaits = 0;
	HudsonNav_StartProgress(&progress, place.vx, place.vy, place.vz);

	double const acrossX = (double)target.vx - place.vx;
	double const acrossZ = (double)target.vz - place.vz;
	double const length = sqrt(acrossX * acrossX + acrossZ * acrossZ);
	double const clearTo = (length > stopsShort) ? (length - stopsShort) / length : 0.0;
	int const clearX = place.vx + (int)(acrossX * clearTo);
	int const clearZ = place.vz + (int)(acrossZ * clearTo);

	// On another floor, a straight line doesn't mean anything
	if (abs(target.vy - place.vy) <= kHudsonNavStepUp && theMap.LineIsClear(rays, place.vx, place.vy, place.vz, clearX, clearZ, false))
	{
		wayState = kWayStraight;
		SteerAt(marine, target.vx, target.vz);
		return;
	}

	LookForAWay(marine, frame);
}

// How long a piece of work took, for the log, so that nobody has to guess what the map costs.
static double SecondsNow()
{
	LARGE_INTEGER counts, perSecond;

	QueryPerformanceCounter(&counts);
	QueryPerformanceFrequency(&perSecond);

	return (double)counts.QuadPart / (double)perSecond.QuadPart;
}

// Puts his spot ahead of him, on the line towards this point: past it, if it's near, because a guard stops short of his spot. It's in his own module, so that he goes straight at it, and not by the game's way
// between modules, which can turn him back.
static void SteerAt(STRATEGYBLOCK *marine, int x, int z)
{
	VECTORCH const place = marine->DynPtr->Position;
	AIMODULE *const hisModule = marine->containingModule ? marine->containingModule->m_aimodule : legModule;
	double const acrossX = (double)x - place.vx;
	double const acrossZ = (double)z - place.vz;
	double const length = sqrt(acrossX * acrossX + acrossZ * acrossZ);

	if (length <= 0.0 || !hisModule)
	{
		return;
	}

	double const lead = (length > kLeadOfHisSpot) ? length : kLeadOfHisSpot;
	VECTORCH const spot = { place.vx + (int)(acrossX * lead / length), place.vy, place.vz + (int)(acrossZ * lead / length) };

	Marine_GuardThisSpot(marine, &spot, hisModule);
}

// Steers him along the map's way, towards the furthest point of it he can see.
static void SteerAlongTheWay(STRATEGYBLOCK *marine)
{
	VECTORCH const place = marine->DynPtr->Position;
	RaysOfTheGame rays(marine);
	int point = theMap.FurthestInSight(rays, place.vx, place.vy, place.vz, kMostPointsAhead);

	// It can't see any of it from here, such as on stairs: the next point was shown to be a step away, so it goes there
	if (point < 0)
	{
		point = theMap.NextAlong();
	}

	if (point >= 0)
	{
		SteerAt(marine, theMap.Way()[point].x, theMap.Way()[point].z);
	}
}

// Does the map's part of each frame.
static void KeepHimOnHisWay(STRATEGYBLOCK *marine, long frame)
{
	if (wayState == kWayLeftAlone)
	{
		return;
	}

	if (wayState == kWayStraight)
	{
		if (frame % kFramesBetweenSteers == 0)
		{
			SteerAt(marine, target.vx, target.vz);
		}

		return;
	}

	double const started = SecondsNow();

	if (wayState == kWaySearching)
	{
		RaysOfTheGame rays(marine);
		HudsonNavSearch const search = theMap.Work(rays, kRaysForTheMapEachFrame);

		// Standing still to think isn't being stuck
		HudsonNav_StartProgress(&progress, marine->DynPtr->Position.vx, marine->DynPtr->Position.vy, marine->DynPtr->Position.vz);

		if (search != kHudsonNavSearching)
		{
			char line[220] = { 0 };

			_snprintf(line, sizeof(line) - 1, "nav: frame %ld, %s: %d points, after looking at %d spots. The map holds %d spots: %d flat and %d sloping", frame, search == kHudsonNavFound ? "found a way round" : "NO WAY ROUND that it could find, so he goes straight",
				(int)theMap.Way().size(), theMap.LastSearchLooks(), (int)theMap.Spots(), (int)theMap.FlatSpots(), (int)theMap.SlopeSpots());
			LogString(line);

			if (search == kHudsonNavFound)
			{
				legsByMap++;
				wayState = kWayFollowing;
				SteerAlongTheWay(marine);
			}
			else
			{
				legsMapCouldntDo++;
				wayState = kWayStraight;
				SteerAt(marine, target.vx, target.vz);
			}
		}
	}
	else if (frame % kFramesBetweenSteers == 0)
	{
		SteerAlongTheWay(marine);
	}

	double const milliseconds = (SecondsNow() - started) * 1000.0;

	mapMilliseconds += milliseconds;
	mapLongestFrame = (milliseconds > mapLongestFrame) ? milliseconds : mapLongestFrame;
}

// Chooses where he goes next. False if there's nowhere.
static bool ChooseWhereNext(STRATEGYBLOCK *marine, long frame)
{
	int const here = ModuleHeIsIn(marine);

	goingToTheMiddle = false;
	goingTo = (here >= 0) ? HudsonNav_NextModule(links, here, beenThere) : -1;

	if (goingTo < 0)
	{
		return false;
	}

	AIMODULE *const module = &AIModuleArray[goingTo];
	FARENTRYPOINT const *const way = GetAIModuleEP(module, &AIModuleArray[here]);

	// the list was made from these very entry points, so this is only for a level that changes what it said
	if (!way)
	{
		goingTo = -1;
		return false;
	}

	target = way->position;
	target.vx += module->m_world.vx;
	target.vy += module->m_world.vy;
	target.vz += module->m_world.vz;

	char line[200] = { 0 };

	_snprintf(line, sizeof(line) - 1, "nav: frame %ld, in %s, going to %s, by the spot at %d %d %d", frame, NameOfAIModule(here), NameOfAIModule(goingTo), target.vx, target.vy, target.vz);
	LogString(line);

	SendHimTo(marine, module, frame);
	return true;
}

static HudsonNavRayKind OneRay(const STRATEGYBLOCK *marine, const VECTORCH &from, const VECTORCH &direction, int *distance);

// Sends him to the middle of the room he's in. False if it hasn't got one he can stand in, or he can't see it from here, or he's already there.
static bool GoToTheMiddle(STRATEGYBLOCK *marine, int here, long frame)
{
	VECTORCH middle;
	int away = 0;

	if (!HudsonNav_MiddleOfRoom(here, &middle, &away))
	{
		return false;
	}

	// From his eyes, on the level. There's no use setting off for a middle he can't see, because the marine code walks in a straight line and can't get round the corner of a room.
	VECTORCH eye = marine->DynPtr->Position;
	eye.vy -= kEyeAboveHisFeet;

	VECTORCH way = { middle.vx - eye.vx, 0, middle.vz - eye.vz };
	double const across = sqrt((double)way.vx * way.vx + (double)way.vz * way.vz);

	if (across < kHudsonNavMiddleIsWithin)
	{
		return false;
	}

	Normalise(&way);

	int distance = 0;

	if (OneRay(marine, eye, way, &distance) != kHudsonNavRayOpen && distance < (int)across)
	{
		char line[200] = { 0 };

		_snprintf(line, sizeof(line) - 1, "nav: frame %ld, in %s, can't see its middle at %d %d %d: there's something %d away, on the way there", frame, NameOfAIModule(here), middle.vx, middle.vy, middle.vz, distance);
		LogString(line);
		return false;
	}

	middleOfTheRoom = middle;
	goingToTheMiddle = true;

	// A guard stops short of his spot, so this one is further on, by that much.
	target = middle;
	target.vx += MUL_FIXED(way.vx, kHudsonNavGuardStopsWithin);
	target.vz += MUL_FIXED(way.vz, kHudsonNavGuardStopsWithin);

	char line[160] = { 0 };

	_snprintf(line, sizeof(line) - 1, "nav: frame %ld, in %s, going to its middle, at %d %d %d", frame, NameOfAIModule(here), middle.vx, middle.vy, middle.vz);
	LogString(line);

	// Only ever in a straight line. The middle is only for a good look around, so it's not worth working out a way to it.
	wayState = kWayLeftAlone;
	mapHasHadItsTurn = true;
	Marine_GuardThisSpot(marine, &target, &AIModuleArray[here]);
	HudsonNav_StartProgress(&progress, marine->DynPtr->Position.vx, marine->DynPtr->Position.vy, marine->DynPtr->Position.vz);
	return true;
}

// One ray. distance is how far it went, in millimetres. It's counted for the room he's in.
static HudsonNavRayKind OneRay(const STRATEGYBLOCK *marine, const VECTORCH &from, const VECTORCH &direction, int *distance)
{
	VECTORCH place = from;
	VECTORCH way = direction;

	FindPolygonInLineOfSight_TwoIgnores(&way, &place, 0, marine->SBdptr, Player);

	bool const hit = LOS_ObjectHitPtr != 0 && LOS_Lambda <= kLookFarthest;
	bool door = false;

	if (hit && LOS_ObjectHitPtr->ObStrategyBlock)
	{
		AVP_BEHAVIOUR_TYPE const type = LOS_ObjectHitPtr->ObStrategyBlock->I_SBtype;

		door = type == I_BehaviourProximityDoor || type == I_BehaviourLiftDoor || type == I_BehaviourSwitchDoor;
	}

	*distance = hit ? LOS_Lambda : kLookFarthest;

	HudsonNavRayKind const kind = HudsonNav_ReadRay(hit, door, hit ? LOS_ObjectNormal.vy : 0);

	raysInThisRoom[kind]++;

	return kind;
}

// Adds what one ray found to a line, without ever going past its end.
static void Add(char *line, size_t size, const char *word, HudsonNavRayKind kind, int distance)
{
	size_t const used = strlen(line);

	if (used + 1 < size)
	{
		if (kind == kHudsonNavRayOpen)
		{
			_snprintf(line + used, size - used - 1, " %s=open", word);
		}
		else
		{
			_snprintf(line + used, size - used - 1, " %s=%s:%d", word, HudsonNav_RayWord(kind), distance);
		}
	}
}

// Says what all the rays found in the room he's leaving, or finishing in, and starts counting again.
static void SayWhatTheRoomIsMadeOf(int room)
{
	char line[240] = { 0 };

	_snprintf(line, sizeof(line) - 1, "nav: room %s, what the rays hit: flat floor %d, slope %d, too steep %d, wall %d, ceiling %d, door %d, open %d", NameOfAIModule(room), raysInThisRoom[kHudsonNavRayFlatFloor], raysInThisRoom[kHudsonNavRaySlope],
		raysInThisRoom[kHudsonNavRayTooSteep], raysInThisRoom[kHudsonNavRayWall], raysInThisRoom[kHudsonNavRayCeiling], raysInThisRoom[kHudsonNavRayDoor], raysInThisRoom[kHudsonNavRayOpen]);
	LogString(line);

	memset(raysInThisRoom, 0, sizeof(raysInThisRoom));
}

// Looks around, and says what's there.
static void LookAround(const STRATEGYBLOCK *marine, long frame)
{
	if (lookLines >= kMostLookLines)
	{
		return;
	}

	lookLines++;

	static const char *const wayWords[8] = { "ahead", "ahead_right", "right", "behind_right", "behind", "behind_left", "left", "ahead_left" };

	VECTORCH const feet = marine->DynPtr->Position;
	VECTORCH eye = feet;
	eye.vy -= kEyeAboveHisFeet;    // y goes down in this game

	// The way he's facing, on the flat
	VECTORCH facing = { marine->DynPtr->OrientMat.mat31, 0, marine->DynPtr->OrientMat.mat33 };

	if (facing.vx == 0 && facing.vz == 0)
	{
		facing.vz = kHudsonNavOneWhole;
	}

	Normalise(&facing);

	char line[500] = { 0 };

	_snprintf(line, sizeof(line) - 1, "nav: lidar frame %ld at %d %d %d in %s:", frame, feet.vx, feet.vy, feet.vz, NameOfModule(marine->containingModule));

	int distance = 0;
	int aheadDistance = 0;

	for (int eighths = 0; eighths < 8; eighths++)
	{
		VECTORCH way = { 0, 0, 0 };

		HudsonNav_TurnEighths(facing.vx, facing.vz, eighths, &way.vx, &way.vz);

		HudsonNavRayKind const kind = OneRay(marine, eye, way, &distance);

		Add(line, sizeof(line), wayWords[eighths], kind, distance);

		if (eighths == 0)
		{
			aheadDistance = distance;
		}

		// The same way, tilted 45 degrees down and then up. These only go in the room's counts, or the line would be too long to read.
		VECTORCH tilted = { MUL_FIXED(way.vx, 46341), 46341, MUL_FIXED(way.vz, 46341) };
		int ignored = 0;

		OneRay(marine, eye, tilted, &ignored);
		tilted.vy = -46341;
		OneRay(marine, eye, tilted, &ignored);
	}

	VECTORCH const up = { 0, -kHudsonNavOneWhole, 0 };
	VECTORCH const down = { 0, kHudsonNavOneWhole, 0 };

	HudsonNavRayKind kind = OneRay(marine, eye, up, &distance);
	Add(line, sizeof(line), "up", kind, distance);

	kind = OneRay(marine, eye, down, &distance);
	Add(line, sizeof(line), "down", kind, distance);

	// One step ahead, if there's room: how far down the floor is there, counted from his feet
	if (aheadDistance > kStepAhead)
	{
		VECTORCH ahead = eye;

		ahead.vx += MUL_FIXED(facing.vx, kStepAhead);
		ahead.vz += MUL_FIXED(facing.vz, kStepAhead);

		kind = OneRay(marine, ahead, down, &distance);

		int const below = distance - kEyeAboveHisFeet;
		size_t const used = strlen(line);

		if (kind != kHudsonNavRayFlatFloor && kind != kHudsonNavRaySlope)
		{
			_snprintf(line + used, sizeof(line) - used - 1, " step_ahead=no_floor");
		}
		else if (below > kLowestStep)
		{
			_snprintf(line + used, sizeof(line) - used - 1, " step_ahead=drop:%d", below);
		}
		else
		{
			_snprintf(line + used, sizeof(line) - used - 1, " step_ahead=%s:%d", HudsonNav_RayWord(kind), below);
		}
	}

	LogString(line);
}

// Reads the route of this name. False if it can't be used, and the log says why. A route's name comes from the command line, which only lets through letters, digits, - and _, so it can't lead anywhere else.
static bool ReadTheRoute(const char *name)
{
	char exePath[MAX_PATH] = { 0 };
	DWORD const pathLength = GetModuleFileNameA(NULL, exePath, sizeof(exePath));
	char *const lastSlash = strrchr(exePath, '\\');

	if (pathLength == 0 || pathLength >= sizeof(exePath) || !lastSlash)
	{
		LogString("nav: can't tell what folder the game is in, so there's no route");
		return false;
	}

	*lastSlash = 0;

	std::string const path = std::string(exePath) + "\\" + kRouteFolder + "\\" + name + ".txt";
	FILE *const file = fopen(path.c_str(), "rb");

	if (!file)
	{
		LogString("nav: there's no route at " + path);
		return false;
	}

	// One byte more than a route can be, so that a longer file shows, and that's all that's ever read
	std::vector<char> text(kHudsonNavMostRouteBytes + 1);
	size_t const size = fread(&text[0], 1, text.size(), file);

	fclose(file);

	const char *const problem = HudsonNav_ReadRoute(&text[0], size, &route);

	if (problem)
	{
		route.clear();
		LogString("nav: the route at " + path + " isn't used, because " + problem);
		return false;
	}

	char line[120] = { 0 };

	_snprintf(line, sizeof(line) - 1, "nav: following a route of %d steps, from %s", (int)route.size(), name);
	LogString(line);
	return true;
}

// Waits until a sixtieth of a second has gone since the last frame ended. Sleeping gets close without using up the processor, and the last bit is waited out by asking the clock.
void HudsonNav_HoldTheFrameRate()
{
	double now = SecondsNow();

	while (now - lastFrameEnded < kSecondsForAFrame)
	{
		if (kSecondsForAFrame - (now - lastFrameEnded) > 0.002)
		{
			Sleep(1);
		}

		now = SecondsNow();
	}

	lastFrameEnded = now;
}

bool HudsonNav_Start(STRATEGYBLOCK *marine, int wanted, const char *routeName, bool run)
{
	if (AvP.Network != I_No_Network)
	{
		LogString("nav: this doesn't run in a network game");
		return false;
	}

	MakeTheLinks();

	modulesWanted = wanted;
	walkedInto = 0;
	putInto = 0;
	movedOn = 0;
	lookLines = 0;
	pressed = 0;
	pressedByAsking = 0;
	pressesThatDidSomething = 0;
	theMap.Clear();
	wayState = kWayLeftAlone;
	mapHasHadItsTurn = false;
	placeLastFrame = marine->DynPtr->Position;
	legMetres = legSeconds = allMetres = allSeconds = 0.0;
	lastFrameEnded = SecondsNow();
	roomChangesOnThisStep = 0;
	timeOnThisStep = 0;
	loops = 0;

	// So that Sleep(1) means about a millisecond, and not the ordinary sixteenth of a second. Only for this run, and it ends with the game.
	timeBeginPeriod(1);

	if (run)
	{
		Marine_Hurry(marine);
		LogString("nav: he runs, at the marine's combat speed");
	}

	// Each marine is given his own speed, from 7/8 to 9/8 of what the data says, so this is what to compare with the speed he really goes at.
	{
		char line[120] = { 0 };

		_snprintf(line, sizeof(line) - 1, "nav: this marine's own speeds: he walks at %.2f and runs at %.2f metres a second, by the data", 1.5 * ((MARINE_STATUS_BLOCK *)marine->SBdataptr)->speedConstant / kHudsonNavOneWhole, 4.0 * ((MARINE_STATUS_BLOCK *)marine->SBdataptr)->speedConstant / kHudsonNavOneWhole);
		LogString(line);
	}

	legModule = 0;
	legsByMap = 0;
	legsMapCouldntDo = 0;
	mapMilliseconds = 0.0;
	mapLongestFrame = 0.0;
	justPutDown = false;
	goingToTheMiddle = false;
	moduleHere = ModuleHeIsIn(marine);
	memset(raysInThisRoom, 0, sizeof(raysInThisRoom));
	route.clear();
	routeStep = 0;
	routeStage = kRouteStartTheStep;

	if (routeName && routeName[0] && !ReadTheRoute(routeName))
	{
		return false;
	}

	if (moduleHere < 0)
	{
		LogString("nav: he isn't in a module that has links");
		return false;
	}

	beenThere[moduleHere] = 1;

	char line[160] = { 0 };

	_snprintf(line, sizeof(line) - 1, "nav: starts in %s, with %d modules in the level, asked to reach %d new ones", NameOfAIModule(moduleHere), (int)links.size(), wanted);
	LogString(line);

	// He's already in the middle of this room, so the first look is from there
	LookAround(marine, 0);

	return !route.empty() || ChooseWhereNext(marine, 0);
}

// Says what's stopping him, so that a stuck line can be read: what the game's own collisions say he's touching, and what rays at his feet, his knees and his head hit on the way to where he's going.
static void SayWhatsStoppingHim(const STRATEGYBLOCK *marine)
{
	DYNAMICSBLOCK const *const dyn = marine->DynPtr;
	char line[320] = { 0 };

	_snprintf(line, sizeof(line) - 1, "nav: stuck: he's facing %d %d, trying to go at %d %d %d, on the floor: %s", dyn->OrientMat.mat31, dyn->OrientMat.mat33, dyn->LinVelocity.vx, dyn->LinVelocity.vy, dyn->LinVelocity.vz, dyn->IsInContactWithFloor ? "yes" : "no");
	LogString(line);

	int shown = 0;

	for (COLLISIONREPORT const *report = dyn->CollisionReportPtr; report && shown < 4; report = report->NextCollisionReportPtr, shown++)
	{
		std::string const what = report->ObstacleSBPtr ? HudsonStrategies_Title(report->ObstacleSBPtr) : std::string("the room itself");

		_snprintf(line, sizeof(line) - 1, ", facing %d %d %d, at %d %d %d", report->ObstacleNormal.vx, report->ObstacleNormal.vy, report->ObstacleNormal.vz, report->ObstaclePoint.vx, report->ObstaclePoint.vy, report->ObstaclePoint.vz);
		LogString("nav: stuck: touching " + what + line);
	}

	double const acrossX = (double)target.vx - dyn->Position.vx;
	double const acrossZ = (double)target.vz - dyn->Position.vz;
	double const length = sqrt(acrossX * acrossX + acrossZ * acrossZ);

	if (length <= 0.0)
	{
		return;
	}

	VECTORCH const way = { (int)(acrossX * kHudsonNavOneWhole / length), 0, (int)(acrossZ * kHudsonNavOneWhole / length) };
	int const heights[4] = { 100, kHudsonNavStepUp + 50, kHudsonNavKneeHeight + 400, kHudsonNavHeadHeight };

	_snprintf(line, sizeof(line) - 1, "nav: stuck: towards it, %d away:", (int)length);

	for (int i = 0; i < 4; i++)
	{
		VECTORCH from = dyn->Position;
		int distance = 0;

		from.vy -= heights[i];

		HudsonNavRayKind const kind = OneRay(marine, from, way, &distance);
		size_t const used = strlen(line);

		_snprintf(line + used, sizeof(line) - used - 1, " at %d high, %s:%d", heights[i], HudsonNav_RayWord(kind), distance);
	}

	LogString(line);
}

// Whether a door's close, on his way. Being stuck there is only waiting for it.
static bool DoorIsOnHisWay(const STRATEGYBLOCK *marine)
{
	double const acrossX = (double)target.vx - marine->DynPtr->Position.vx;
	double const acrossZ = (double)target.vz - marine->DynPtr->Position.vz;
	double const length = sqrt(acrossX * acrossX + acrossZ * acrossZ);

	if (length <= 0.0)
	{
		return false;
	}

	RaysOfTheGame rays(marine);
	int const directionX = (int)(acrossX * kHudsonNavOneWhole / length);
	int const directionZ = (int)(acrossZ * kHudsonNavOneWhole / length);
	HudsonNavRayAnswer const answer = rays.Cast(marine->DynPtr->Position.vx, marine->DynPtr->Position.vy - kHudsonNavKneeHeight, marine->DynPtr->Position.vz, directionX, 0, directionZ, kDoorIsWithin);

	return answer.hit && answer.door;
}

// Picks him up and puts him down at the spot he was going to, because he isn't getting there. False if there's no room at that spot.
static bool PutHimDown(STRATEGYBLOCK *marine, long frame)
{
	VECTORCH const from = marine->DynPtr->Position;
	MODULE *const room = ModuleFromPosition(&target, (MODULE *)0);

	SayWhatsStoppingHim(marine);

	char line[300] = { 0 };

	_snprintf(line, sizeof(line) - 1, "nav: frame %ld, STUCK at %d %d %d in %s, going to %s by the spot at %d %d %d, %s", frame, from.vx, from.vy, from.vz, NameOfModule(marine->containingModule), NameOfAIModule(goingTo),
		target.vx, target.vy, target.vz, room ? "so he's put down there" : "and that spot is in no room, so he can't be put there");
	LogString(line);

	if (!room)
	{
		return false;
	}

	marine->DynPtr->Position = target;
	marine->DynPtr->PrevPosition = target;
	marine->DynPtr->LinVelocity.vx = 0;
	marine->DynPtr->LinVelocity.vy = 0;
	marine->DynPtr->LinVelocity.vz = 0;
	marine->containingModule = room;
	movedOn++;
	justPutDown = true;
	wayState = kWayLeftAlone;

	return true;
}

// Takes a way that can't be used off the list.
static void CrossOff(int from, int to)
{
	if (from < 0 || from >= (int)links.size())
	{
		return;
	}

	std::vector<int> &list = links[from];

	for (size_t i = 0; i < list.size(); i++)
	{
		if (list[i] == to)
		{
			list.erase(list.begin() + i);
			return;
		}
	}
}

// Which module has a part with this name. -1 if none has.
static int RoomNamed(const std::string &name)
{
	for (int number = 0; number < (int)links.size(); number++)
	{
		for (MODULE **part = AIModuleArray[number].m_module_ptrs; part && *part; part++)
		{
			if ((*part)->name && _stricmp((*part)->name, name.c_str()) == 0)
			{
				return number;
			}
		}
	}

	return -1;
}

// The module this spot's in. He's told to go to a spot and a module together, so this is his own if the spot's in none.
static AIMODULE *ModuleOfSpot(const VECTORCH &spot, const STRATEGYBLOCK *marine)
{
	VECTORCH place = spot;
	MODULE const *const room = ModuleFromPosition(&place, (MODULE *)0);

	if (room && room->m_aimodule)
	{
		return room->m_aimodule;
	}

	return marine->containingModule ? marine->containingModule->m_aimodule : 0;
}

static void NextStep()
{
	routeStep++;
	routeStage = kRouteStartTheStep;
	timeInThisStage = 0;
	goingTo = -1;
	wayState = kWayLeftAlone;
	roomChangesOnThisStep = 0;
	timeOnThisStep = 0;
}

// Starts the step the route has got to. False if the route is over, for whatever the log says.
static bool StartTheStep(STRATEGYBLOCK *marine, const HudsonNavStep &step, int here, long frame)
{
	char line[260] = { 0 };

	timeInThisStage = 0;

	if (step.kind == kHudsonNavStepOther)
	{
		_snprintf(line, sizeof(line) - 1, "nav: frame %ld, step %d of the route is one he can't do yet: %s", frame, (int)routeStep + 1, step.name.c_str());
		LogString(line);
		return false;
	}

	VECTORCH const spot = { step.x, step.y, step.z };

	if (step.kind == kHudsonNavStepWalk)
	{
		int const room = RoomNamed(step.name);

		if (room < 0)
		{
			_snprintf(line, sizeof(line) - 1, "nav: frame %ld, step %d of the route is a room this level hasn't, and he stops: %s", frame, (int)routeStep + 1, step.name.c_str());
			LogString(line);
			return false;
		}

		// The route's next room can be the one he's in already. But one room can have more than one level, and his way to the next level goes by this spot, so only if he's near it.
		if (room == here && HudsonNav_IsAtSpot(marine->DynPtr->Position.vx - spot.vx, marine->DynPtr->Position.vy - spot.vy, marine->DynPtr->Position.vz - spot.vz, kNearEnoughToARouteSpot))
		{
			NextStep();
			return true;
		}

		goingTo = room;
		target = spot;

		_snprintf(line, sizeof(line) - 1, "nav: frame %ld, step %d of the route: walking to %s, by the spot at %d %d %d", frame, (int)routeStep + 1, step.name.c_str(), spot.vx, spot.vy, spot.vz);
		LogString(line);

		routeStage = kRouteWalking;
		SendHimTo(marine, &AIModuleArray[room], frame);
		return true;
	}

	target = spot;
	goingTo = -1;
	thingToPress = 0;
	thingIsAt = spot;

	if (step.kind == kHudsonNavStepPress)
	{
		thingToPress = HudsonStrategies_Find(step.name.c_str());

		if (!thingToPress)
		{
			_snprintf(line, sizeof(line) - 1, "nav: frame %ld, step %d of the route is to press something this level hasn't, and he stops: %s", frame, (int)routeStep + 1, step.name.c_str());
			LogString(line);
			return false;
		}

		// He walks at the thing itself, on his own floor, and stops short of it, as anyone would. It's on a wall, so nowhere there is for standing on.
		target.vy = marine->DynPtr->Position.vy;
	}

	AIMODULE *const module = ModuleOfSpot(target, marine);

	_snprintf(line, sizeof(line) - 1, "nav: frame %ld, step %d of the route: going to %s, standing at %d %d %d", frame, (int)routeStep + 1, step.name.c_str(), target.vx, target.vy, target.vz);
	LogString(line);

	if (!module)
	{
		LogString("nav: he's in no module and neither is the spot, so he stops");
		return false;
	}

	goingTo = module->m_index;
	routeStage = kRouteWalking;

	SendHimTo(marine, module, frame, step.kind == kHudsonNavStepPress ? kCloseEnoughToPress : 0);
	return true;
}

// Whether the use key would look at something of this kind: the list in OperateObjectInLineOfSight.
static bool UseKeyWorksOn(AVP_BEHAVIOUR_TYPE type)
{
	return type == I_BehaviourBinarySwitch || type == I_BehaviourLinkSwitch || type == I_BehaviourAutoGun || type == I_BehaviourDatabase;
}

// Whether a thing is in his reach: what the player's use key allows, from where he stands and the way he's facing. Puts in words what he found.
static bool ThingIsInReach(const STRATEGYBLOCK *marine, const STRATEGYBLOCK *thing, const VECTORCH &place, char *words, size_t wordsSize)
{
	VECTORCH eye = marine->DynPtr->Position;

	eye.vy -= kEyeAboveHisFeet;

	double const facingX = marine->DynPtr->OrientMat.mat31;
	double const facingZ = marine->DynPtr->OrientMat.mat33;
	double const length = sqrt(facingX * facingX + facingZ * facingZ);
	double const x = (double)place.vx - eye.vx;
	double const y = (double)place.vy - eye.vy;
	double const z = (double)place.vz - eye.vz;
	double const front = (length > 0.0) ? (x * facingX + z * facingZ) / length : 0.0;
	double const across = (length > 0.0) ? (x * facingZ - z * facingX) / length : 0.0;
	double const distance = sqrt(x * x + y * y + z * z);
	bool inSight = false;

	if (distance > 0.0)
	{
		VECTORCH from = eye;
		VECTORCH way = { (int)(x * kHudsonNavOneWhole / distance), (int)(y * kHudsonNavOneWhole / distance), (int)(z * kHudsonNavOneWhole / distance) };

		FindPolygonInLineOfSight_TwoIgnores(&way, &from, 0, marine->SBdptr, Player);

		inSight = !LOS_ObjectHitPtr || LOS_ObjectHitPtr == thing->SBdptr || (double)LOS_Lambda >= distance - kThicknessOfAWall;
	}

	bool const inRange = front > 0.0 && front < kPressReachesFront && fabs(across) < kPressReachesAcross && fabs(y) < kPressReachesUpOrDown;

	_snprintf(words, wordsSize - 1, "%d in front, %d across, %d up or down, and in sight: %s", (int)front, (int)across, (int)y, inSight ? "yes" : "no");

	return inRange && inSight;
}

// Says what the player's use key would make of this thing, test by test, as OperateObjectInLineOfSight makes them, without pressing it: its kind, being on the screen, its place in front of the camera, and a line of
// sight from it to the player. It says that last one twice, the second time leaving out his own body, because he's standing where the player is, and would be in the way of a ray to the player. It also says what else
// is in reach, because the key presses the nearest to the middle of the screen, which needn't be this.
static void SayWhatTheUseKeyWouldDo(const STRATEGYBLOCK *marine, const STRATEGYBLOCK *thing)
{
	DISPLAYBLOCK *const picture = thing->SBdptr;
	bool onScreen = false;

	for (int i = 0; i < NumOnScreenBlocks; i++)
	{
		onScreen = onScreen || OnScreenBlockList[i] == picture;
	}

	char line[340] = { 0 };

	if (!picture)
	{
		_snprintf(line, sizeof(line) - 1, "nav: use key: its kind is %d, which the key %s, and it has no picture in the world at all, so the key can't see it", (int)thing->I_SBtype, UseKeyWorksOn(thing->I_SBtype) ? "works on" : "doesn't work on");
		LogString(line);
	}
	else
	{
		bool const sees = IsThisObjectVisibleFromThisPosition_WithIgnore(Player, picture, &picture->ObWorld, 10000) != 0;
		bool seesWithoutHim = false;

		{
			VECTORCH from = picture->ObWorld;
			VECTORCH way;

			GetTargetingPointOfObject(Player, &way);
			way.vx -= from.vx;
			way.vy -= from.vy;
			way.vz -= from.vz;
			Normalise(&way);

			FindPolygonInLineOfSight_TwoIgnores(&way, &from, 0, picture, marine->SBdptr);
			seesWithoutHim = LOS_ObjectHitPtr == Player;
		}

		_snprintf(line, sizeof(line) - 1, "nav: use key: its kind is %d, which the key %s. It's at %d %d %d. On the screen: %s. %d in front, %d across, %d up or down, where the key wants up to 3000, 1000 and 1000. It can see the player: %s, and with his own body left out: %s", (int)thing->I_SBtype,
			UseKeyWorksOn(thing->I_SBtype) ? "works on" : "doesn't work on", picture->ObWorld.vx, picture->ObWorld.vy, picture->ObWorld.vz, onScreen ? "yes" : "no", picture->ObView.vz, picture->ObView.vx, picture->ObView.vy, sees ? "yes" : "no", seesWithoutHim ? "yes" : "no");
		LogString(line);
	}

	for (int i = 0; i < NumOnScreenBlocks; i++)
	{
		DISPLAYBLOCK const *const other = OnScreenBlockList[i];

		if (other && other != picture && other->ObStrategyBlock && UseKeyWorksOn(other->ObStrategyBlock->I_SBtype) && other->ObView.vz > 0 && other->ObView.vz < 3000 && abs(other->ObView.vx) < 1000 && abs(other->ObView.vy) < 1000)
		{
			LogString("nav: use key: also in its reach: " + HudsonStrategies_Title(other->ObStrategyBlock));
		}
	}
}

// Follows the route for one frame. False when it's over. wasPutDown says he was picked up and put down at this step's spot last frame.
static bool FollowTheRoute(STRATEGYBLOCK *marine, int here, bool stuck, bool wasPutDown, long frame)
{
	if (routeStep >= route.size())
	{
		return false;
	}

	HudsonNavStep const &step = route[routeStep];
	VECTORCH const place = marine->DynPtr->Position;
	char line[260] = { 0 };

	if (routeStage == kRouteStartTheStep)
	{
		return StartTheStep(marine, step, here, frame);
	}

	if (routeStage == kRouteWalking)
	{
		bool arrived = false;

		if (step.kind == kHudsonNavStepWalk)
		{
			arrived = here == goingTo && HudsonNav_IsAtSpot(place.vx - target.vx, place.vy - target.vy, place.vz - target.vz, kNearEnoughToARouteSpot);
		}
		else if (step.kind == kHudsonNavStepPress)
		{
			// and on the floor: he can go past something on his way down a shaft
			arrived = marine->DynPtr->IsInContactWithFloor && HudsonNav_IsAtSpot(place.vx - thingIsAt.vx, 0, place.vz - thingIsAt.vz, kCloseEnoughToPress);
		}
		else
		{
			arrived = here == goingTo && HudsonNav_IsAtSpot(place.vx - target.vx, place.vy - target.vy, place.vz - target.vz, kHudsonNavGuardStopsWithin);
		}

		// Near enough to reach something he's pressing is near enough. It's no use moving him for the sake of a step or two, and that would count against him.
		if (!arrived && stuck && step.kind == kHudsonNavStepPress && HudsonNav_IsAtSpot(place.vx - thingIsAt.vx, 0, place.vz - thingIsAt.vz, kPressReachesFront))
		{
			_snprintf(line, sizeof(line) - 1, "nav: frame %ld, can't get any closer to %s, and it may be in reach from %d %d %d", frame, step.name.c_str(), place.vx, place.vy, place.vz);
			LogString(line);
			arrived = true;
		}

		// Put at the very spot, and it still isn't that step done: then being put there again won't do it. The route goes on from where he is.
		if (!arrived && wasPutDown)
		{
			_snprintf(line, sizeof(line) - 1, "nav: frame %ld, step %d of the route is left out, because being put at its spot, %d %d %d, didn't finish it", frame, (int)routeStep + 1, target.vx, target.vy, target.vz);
			LogString(line);
			NextStep();
			return true;
		}

		if (!arrived && (stuck || here < 0))
		{
			if (PutHimDown(marine, frame))
			{
				HudsonNav_StartProgress(&progress, target.vx, target.vy, target.vz);
				return true;
			}

			// Nowhere to put him. The stuck rule is only for moving a run on, so the route goes on from where he is.
			_snprintf(line, sizeof(line) - 1, "nav: frame %ld, step %d of the route is left out, because he can't get there or be put there", frame, (int)routeStep + 1);
			LogString(line);
			NextStep();
			return true;
		}

		if (!arrived)
		{
			return true;
		}

		if (step.kind != kHudsonNavStepPress)
		{
			_snprintf(line, sizeof(line) - 1, "nav: frame %ld, step %d of the route is done: at %s, at %d %d %d", frame, (int)routeStep + 1, step.name.c_str(), place.vx, place.vy, place.vz);
			LogString(line);
			NextStep();
			return true;
		}

		// Turns to face it, as a guard does
		VECTORCH const facePoint = { thingIsAt.vx, place.vy, thingIsAt.vz };

		wayState = kWayLeftAlone;
		Marine_GuardAndFace(marine, &facePoint);
		routeStage = kRouteFacing;
		timeInThisStage = 0;
		return true;
	}

	timeInThisStage += NormalFrameTime;

	if (routeStage == kRouteFacing)
	{
		bool const facing = HudsonPlan_IsFacing(marine->DynPtr->OrientMat.mat31, marine->DynPtr->OrientMat.mat33, thingIsAt.vx - place.vx, thingIsAt.vz - place.vz);

		if (!facing && timeInThisStage < kMostSecondsToFace)
		{
			return true;
		}

		// The list can have changed since he set off, so it's looked up by name again: a pointer from then could be to something that's gone.
		thingToPress = HudsonStrategies_Find(step.name.c_str());

		if (!thingToPress)
		{
			_snprintf(line, sizeof(line) - 1, "nav: frame %ld, %s has gone from the level, so it isn't pressed", frame, step.name.c_str());
			LogString(line);
			NextStep();
			return true;
		}

		stateBeforePress = HudsonStrategies_State(thingToPress);

		char reach[160] = { 0 };
		VECTORCH const thingPlace = thingToPress->SBdptr ? thingToPress->SBdptr->ObWorld : thingIsAt;
		bool const inReach = ThingIsInReach(marine, thingToPress, thingPlace, reach, sizeof(reach));

		SayWhatTheUseKeyWouldDo(marine, thingToPress);

		_snprintf(line, sizeof(line) - 1, "nav: frame %ld, %s %s from %d %d %d: %s. Facing it: %s. Before: ", frame, inReach ? "PRESSES" : "CAN'T REACH", step.name.c_str(), place.vx, place.vy, place.vz, reach, facing ? "yes" : "no, he ran out of time turning");
		LogString(line + stateBeforePress);

		// The game's own way of asking, as it is for the player. If he can't reach it, a run still asks, so that it shows the rest of the level. That's counted apart, so that it can't read as a press.
		RequestState(thingToPress, 1, 0);

		if (inReach)
		{
			pressed++;
		}
		else
		{
			pressedByAsking++;
		}

		routeStage = kRouteWatching;
		timeInThisStage = 0;
		return true;
	}

	if (timeInThisStage < kSecondsToWatchAPress)
	{
		return true;
	}

	thingToPress = HudsonStrategies_Find(step.name.c_str());

	std::string const stateNow = thingToPress ? HudsonStrategies_State(thingToPress) : std::string("gone");
	bool const changed = stateNow != stateBeforePress;

	if (changed)
	{
		pressesThatDidSomething++;
	}

	LogString("nav: after pressing " + step.name + ": " + stateNow + (changed ? "" : ". NOTHING CHANGED"));

	NextStep();
	return true;
}

// Says it's over, with what he did. False, so that the caller can hand it straight back.
static bool Finish(long frame, const char *why)
{
	SayWhatTheRoomIsMadeOf(moduleHere);

	char line[200] = { 0 };

	_snprintf(line, sizeof(line) - 1, "nav: frame %ld, %s. Walked into %d new modules, put into %d, moved on %d times, asked for %d. Pressed %d with the use key, asked %d directly, and %d of those changed", frame, why, walkedInto, putInto, movedOn, modulesWanted, pressed, pressedByAsking, pressesThatDidSomething);
	LogString(line);

	char speedLine[120] = { 0 };

	_snprintf(speedLine, sizeof(speedLine) - 1, "nav: on his own feet he walked %.0f metres in %.0f seconds: %.2f metres a second. Loops on a route: %d", allMetres, allSeconds, allSeconds > 0.0 ? allMetres / allSeconds : 0.0, loops);
	LogString(speedLine);

	char mapLine[240] = { 0 };

	_snprintf(mapLine, sizeof(mapLine) - 1, "nav: the map found %d ways round, and couldn't for %d. It holds %d spots: %d flat and %d sloping. It used %llu rays and %.1f ms in all, %.2f ms at most in a frame", legsByMap, legsMapCouldntDo, (int)theMap.Spots(), (int)theMap.FlatSpots(), (int)theMap.SlopeSpots(),
		(unsigned long long)theMap.RaysUsed(), mapMilliseconds, mapLongestFrame);
	LogString(mapLine);

	return false;
}

// Adds this frame to how far he's really walked. Only while he's meant to be walking, and being put down isn't walking.
static void MeasureHim(const VECTORCH &place)
{
	bool const walking = wayState == kWayStraight || wayState == kWayFollowing || goingToTheMiddle;

	if (walking && !justPutDown)
	{
		double const x = (double)place.vx - placeLastFrame.vx;
		double const y = (double)place.vy - placeLastFrame.vy;
		double const z = (double)place.vz - placeLastFrame.vz;
		double const metres = sqrt(x * x + y * y + z * z) / 1000.0;
		double const seconds = (double)NormalFrameTime / kHudsonNavOneWhole;

		legMetres += metres;
		legSeconds += seconds;
		allMetres += metres;
		allSeconds += seconds;
	}

	placeLastFrame = place;
}

bool HudsonNav_EndOfFrame(STRATEGYBLOCK *marine, long frame)
{
	if (frame % kFramesBetweenLooks == 0)
	{
		LookAround(marine, frame);
	}

	VECTORCH const place = marine->DynPtr->Position;

	MeasureHim(place);

	int const here = ModuleHeIsIn(marine);
	bool const newPlace = here >= 0 && here != moduleHere;
	bool mustChoose = false;
	bool const wasPutDown = justPutDown;

	if (newPlace)
	{
		bool const isNew = !beenThere[here];

		beenThere[here] = 1;

		if (isNew)
		{
			if (justPutDown)
			{
				putInto++;
			}
			else
			{
				walkedInto++;
			}
		}

		SayWhatTheRoomIsMadeOf(moduleHere);

		char line[300] = { 0 };

		_snprintf(line, sizeof(line) - 1, "nav: frame %ld, %s %s (%s), at %d %d %d. Walked into %d new, put into %d new, moved on %d times. That leg: %.1f metres in %.1f seconds, %.2f metres a second", frame, justPutDown ? "PUT INTO" : "walked into", NameOfAIModule(here), isNew ? "new" : "been there",
			place.vx, place.vy, place.vz, walkedInto, putInto, movedOn, legMetres, legSeconds, legSeconds > 0.0 ? legMetres / legSeconds : 0.0);
		LogString(line);

		legMetres = 0.0;
		legSeconds = 0.0;
		roomChangesOnThisStep++;

		moduleHere = here;

		// A new room gets a good look from its middle. One he's going back through doesn't: he just carries on through it. Nor does one on a route, which knows where it's going.
		if (route.empty() && (!isNew || !GoToTheMiddle(marine, here, frame)))
		{
			mustChoose = true;
		}
	}
	else if (justPutDown && route.empty())
	{
		// Put down at the spot, and still in the same module, means that way doesn't lead anywhere, however good it looks on the list. Without this he'd be put back there for ever.
		char line[160] = { 0 };

		_snprintf(line, sizeof(line) - 1, "nav: frame %ld, being put at the spot for %s left him in %s, so that way is crossed off", frame, NameOfAIModule(goingTo), NameOfAIModule(moduleHere));
		LogString(line);

		CrossOff(moduleHere, goingTo);
		mustChoose = true;
	}

	justPutDown = false;

	KeepHimOnHisWay(marine, frame);

	bool stuck = HudsonNav_IsStuck(&progress, place.vx, place.vy, place.vz, NormalFrameTime);

	// Falling isn't being stuck, and landing starts his leg again from wherever that was
	if (!marine->DynPtr->IsInContactWithFloor)
	{
		HudsonNav_StartProgress(&progress, place.vx, place.vy, place.vz);
		stuck = false;
	}

	// Next to a door, he's only waiting for it, up to a point
	if (stuck && doorWaits < kMostDoorWaits && DoorIsOnHisWay(marine))
	{
		char line[120] = { 0 };

		_snprintf(line, sizeof(line) - 1, "nav: frame %ld, waiting for a door, on his way at %d %d %d", frame, place.vx, place.vy, place.vz);
		LogString(line);

		doorWaits++;
		HudsonNav_StartProgress(&progress, place.vx, place.vy, place.vz);
		stuck = false;
	}

	// Stuck going straight is what the map's for, if it hasn't had its turn on this leg. Being picked up is only for after that.
	if (stuck && wayState == kWayStraight && !mapHasHadItsTurn && !goingToTheMiddle && (route.empty() || routeStage == kRouteWalking))
	{
		LookForAWay(marine, frame);
		HudsonNav_StartProgress(&progress, place.vx, place.vy, place.vz);
		stuck = false;
	}

	if (!route.empty())
	{
		// Facing something or watching it, he's meant to be standing still
		bool const walking = routeStage == kRouteWalking || routeStage == kRouteStartTheStep;

		// Going round in circles on one step, or taking far too long about it: the log says so, and the stuck rule moves him on
		if (walking)
		{
			timeOnThisStep += NormalFrameTime;

			if (roomChangesOnThisStep > kMostRoomChangesOnAStep || timeOnThisStep > kMostSecondsOnAStep)
			{
				char line[220] = { 0 };

				_snprintf(line, sizeof(line) - 1, "nav: frame %ld, LOOP: step %d of the route, %s, has changed rooms %d times in %d seconds and isn't done. He's in %s", frame, (int)routeStep + 1, routeStep < route.size() ? route[routeStep].name.c_str() : "(none)",
					roomChangesOnThisStep, timeOnThisStep / kHudsonNavOneWhole, NameOfAIModule(here));
				LogString(line);

				loops++;
				roomChangesOnThisStep = 0;
				timeOnThisStep = 0;
				stuck = true;
			}
		}

		if (walkedInto + putInto >= modulesWanted)
		{
			return Finish(frame, "that's all he was asked for");
		}

		if (!FollowTheRoute(marine, here, walking && stuck, wasPutDown, frame))
		{
			return Finish(frame, routeStep >= route.size() ? "he's come to the end of the route" : "the route stops here");
		}

		return true;
	}

	if (goingToTheMiddle)
	{
		bool const arrived = HudsonNav_IsAtSpot(place.vx - middleOfTheRoom.vx, place.vy - middleOfTheRoom.vy, place.vz - middleOfTheRoom.vz, kHudsonNavMiddleIsWithin);

		if (arrived || stuck)
		{
			char line[160] = { 0 };

			_snprintf(line, sizeof(line) - 1, "nav: frame %ld, %s the middle of %s, at %d %d %d", frame, arrived ? "stands in" : "can't reach", NameOfAIModule(here), place.vx, place.vy, place.vz);
			LogString(line);

			LookAround(marine, frame);
			goingToTheMiddle = false;
			mustChoose = true;
		}
	}
	else if ((stuck || here < 0) && goingTo >= 0)
	{
		// In no module at all is as stuck as he can be, so he's moved on straight away
		if (PutHimDown(marine, frame))
		{
			// where he goes next is worked out once the next frame says which module he's in
			HudsonNav_StartProgress(&progress, target.vx, target.vy, target.vz);
		}
		else
		{
			CrossOff(moduleHere, goingTo);
			mustChoose = true;
		}
	}

	if (mustChoose)
	{
		if (walkedInto + putInto >= modulesWanted)
		{
			return Finish(frame, "that's all he was asked for");
		}

		if (!ChooseWhereNext(marine, frame))
		{
			return Finish(frame, "there's nowhere new he can get to from here");
		}
	}

	return true;
}

int HudsonNav_ModulesWalkedInto()
{
	return walkedInto;
}

int HudsonNav_TimesMovedOn()
{
	return movedOn;
}

int HudsonNav_ThingsPressed()
{
	return pressed;
}

int HudsonNav_ThingsPressedByAsking()
{
	return pressedByAsking;
}

int HudsonNav_PressesThatDidSomething()
{
	return pressesThatDidSomething;
}

void HudsonNav_TidyHisTurn(STRATEGYBLOCK *marine)
{
	MATRIXCH &turn = marine->DynPtr->OrientMat;
	int clean[9];

	if (HudsonNav_UprightTurn(turn.mat31, turn.mat33, clean))
	{
		turn.mat11 = clean[0]; turn.mat12 = clean[1]; turn.mat13 = clean[2];
		turn.mat21 = clean[3]; turn.mat22 = clean[4]; turn.mat23 = clean[5];
		turn.mat31 = clean[6]; turn.mat32 = clean[7]; turn.mat33 = clean[8];
	}
}
