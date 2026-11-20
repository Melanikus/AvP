// hudson_lanes.cpp: the lane probe. hudson_lanes.h says what it is for and how it is asked for. Written 2026-11-19 by Melanikus (Frost)
// for the AvP port. Modified 2026-11-20 by Melanikus (Frost): lanes=watch, and every alien's line from what its own code read (Bishop's order, Vasquez's fields).
//
// Nothing here changes what the alien does: it reads the alien's blocks, the waypoint code's globals and each box's flags as the game's
// own code left them at the end of the frame. The one alien is the only thing using the waypoint code (everything else that moves is
// kept still), so Global_Route is its route. The rows (D1 ... D20, S3) are those of AvP-docs\engine\inteng\reassess\alien-drop-audit.md
// (Vasquez, 2026-11-19), whose line numbers are at 71638d0d.

#include "3dc.h"
#include "inline.h"
#include "module.h"
#include "strategy_def.h"
#include "gamedef.h"
#include "logString.h"
#include "bh_types.h"
#include "dynblock.h"
#include "bh_alien.h"
#include "pheromone.h"
#include "pvisible.h"
#include "utilities.h"
#include "new_dynamic_collisions.h"
#include "hudson.h"
#include "hudson_lanes.h"
#include "bh_ais.h"
#include "pfarlocs.h"
#include "alien_ways.h"
#include "stock_flags.h"     // the MODE line: every AvPx flag that touches aliens (Vasquez 11-20)
#include "debug_view.h"      // and the debug views, render only (Drake 11-20)
#include "hudson_play.h"     // lanes=watch with play=yes: his list done (probe v3)
#include "hudson_nav.h"      // supplyat=around: the middle of a room, where a marine can stand (HudsonNav_MiddleOfRoom)
#include "hudson_supply.h"   // supplyat=around: which room, and which ones keep their slot (on his level, by his route ahead)
#include "shoulder_lamp.h"   // the LAMP line

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

extern void CreateAlienBot(VECTORCH *Position, int type);   // bh_alien.cpp: the console's own way of making one
extern int NormalFrameTime;

// The waypoint code's own globals (bh_waypoint.cpp), only read here
extern WAYPOINT_ROUTE Global_Route;
extern int NPC_Waypoints;
extern int Num_Target_Waypoints;

// bh_near.cpp's own questions, only asked here (they read the alien's kind and wounds)
extern int AlienIsAbleToClimb(STRATEGYBLOCK *sbPtr);
extern int AlienIsAbleToStand(STRATEGYBLOCK *sbPtr);
extern int AlienHasNoArms(STRATEGYBLOCK *sbPtr);

namespace
{
	char const kLanesFileName[] = "hudson-lanes.txt";
	long const kFramesBeforeTheAlien = 60;          // the player is put down at frame 1 and has a second to land
	long const kFramesAfterTheFirstBlow = 120;      // the run goes on two seconds after the alien first attacks, for what it does then
	int const kAlienStartsAboveTheFloor = 300;      // as hudson.cpp's MakeTheAlien
	int const kPlayerStartsAboveTheFloor = 300;     // the player too, so that he lands in the second before the alien
	int const kCos45 = 46341;                        // 16.16: which surface its down points at
	int const kGravityKicksIn = 16384;               // TIME_BEFORE_GRAVITY_KICKS_IN (dynamics.h): TimeNotInContactWithFloor is set to this on every contact and counts
	                                                 // down without one; at 0 or below, a climber's down becomes straight down (dynamics.cpp ApplyGravity)
	int const kCurveToPlayerDistance = 8000;         // ALIEN_CURVETOPLAYERDIST (bh_near.cpp): Approach's gravity rule changes here

	bool asked = false;
	bool alienOnCeiling = false;                    // alienon=ceiling: the alien is made hanging from the ceiling at alienat=, for a run of the lane alone
	VECTORCH alienAt, standAt;
	long mostFrames = 0;
	MODULE const *placedIn = nullptr;               // the room standat= is in, where he must still be when the alien is made

	FILE *file = nullptr;
	bool alienMade = false;
	char alienName[SB_NAME_LENGTH] = { 0 };
	long madeAtFrame = 0;
	long long gameTime = 0;                          // 16.16 seconds since the alien was made, by the game's own frame times

	// the run's end on an event (Bishop 11-20, see where they are used)
	int const kRunMoved = 1000;                      // mm: moved at all, and headway toward him
	long long const kRunMovedWithin = 5LL * 65536;   // a void run: no move in this long
	long long const kRunNoHeadway = 15LL * 65536;    // no headway in this long
	long long const kRunMost = 60LL * 65536;         // at most
	VECTORCH madeSpot = { 0, 0, 0 };
	bool movedYet = false;
	int closest = -1;
	long long closestAt = 0;

	// what was seen
	bool climbing = false;                           // its gravity its own (UseStandardGravity 0) and its down not the floor's
	bool everClimbed = false, everOnTheCeiling = false;
	long climbedAt = -1, ceilingAt = -1;
	char climbedWhere[160] = { 0 }, ceilingWhere[160] = { 0 };   // plain buffers: nothing of Hudson's may have code that runs before WinMain (tools/check-hudson-asleep.ps1 rule 7)
	bool wasNear = false;
	int lastAiModule = -1;
	int doorwaysUp = 0;
	bool dropped = false;
	long droppedAt = -1;
	char dropWhere[160] = { 0 }, dropReason[400] = { 0 };
	bool inPlayersModule = false;
	long inPlayersModuleAt = -1;
	bool upWhenItCameIn = false;
	bool attacked = false;
	long attackedAt = -1;
	char downInPlayersModule[560] = { 0 };            // how it came down there, once it was there

	// THE STUTTER COUNT (Bishop 11-19 ~23:30, the owner's "the game is not dealing well with discreet squares": to compare v2d with the consolidated plot). While the alien is
	// near, in an AI module with waypoints and with a route this frame, a frame whose route starts from another box than the last frame's is a box switch, and one whose target is
	// held by another box (the lowest box with contains_target) a target switch; a switch back to the box it left the switch before is a return (flicker). A new AI module starts
	// the count afresh (crossing a doorway is not a switch). Over the run: the totals, per second of that time, and the busiest second.
	long stutterBoxSwitches = 0, stutterBoxReturns = 0, stutterTargetSwitches = 0, stutterFrames = 0;
	long long stutterTime = 0;                        // 16.16 seconds counted
	int stutterAi = -1, stutterStart = -1, stutterStartBefore = -1, stutterTarget = -1;
	long long stutterSecondFrom = 0;
	long stutterThisSecond = 0, stutterWorstSecond = 0;

	// EVERY ALIEN'S LINE (Bishop 11-20: in the owner's Steam play of the whole-level auto-plot file, ae03c42d, the aliens ran at the walls, climbed only now and then
	// when crawling, and he never saw one on a ceiling; Vasquez 11-20 named the fields). Each alien whose near state runs gets a line a frame, in both probes: lanes=yes
	// (its one alien) and lanes=watch (the level as he plays it: nothing kept still, no alien made). The alien's own code tells the probe what it read at the moment it
	// read it (HudsonLanes_AlienBefore and _AlienAfter around its near state, bh_near.cpp; HudsonLanes_Searched after each waypoint search, bh_ais.cpp), because
	// Global_Route is one for every NPC: at the end of a frame it is the route of whichever NPC searched last. Why its gravity is as it is, is read from what its state
	// saw, by the lines of Rebellion's code (AvP-assembla branches/reb 3dc/avp/Bh_near.c), and checked against the gravity the alien has: a reading that disagrees says so.
	bool watching = false;                           // lanes=watch
	long long const kWatchMost = 180LL * 65536;      // lanes=watch ends after 3 minutes of game time (Bishop: "for a few minutes"), or at frames=
	bool watchStarted = false;
	// lanes=watch with play=yes (probe v3, Bishop 11-20, the owner: "get hudson to walk it"): he plays from the level's start; the watch ends when his list is done,
	// when he makes no headway (kPlayHeadway) for kPlayNoHeadway, or at kPlayMost
	bool hePlays = false;
	int const kPlayHeadway = 1000;                   // mm
	long long const kPlayNoHeadway = 15LL * 65536;
	long long const kPlayMost = 180LL * 65536;         // laps of a stretch for at most 3 minutes (Bishop 11-20)
	VECTORCH headwayFrom = { 0, 0, 0 };              // where he was when he last made headway
	long long headwayAt = 0;
	// THE SUPPLY (supply=, Bishop 11-20: "the probe makes a FIXED supply per run ... so two plots compare fairly in one run each"): supplyCount aliens made at supplyAt,
	// one every supplyEvery from the watch's start, with the console's own CreateAlienBot (awake, as the console makes one); the level's own NPCs, generators and hive
	// are kept still (HudsonLanesAreOn, Hudson_KeepsStill), so these are the only aliens that move
	int supplyCount = 0;
	VECTORCH supplyAt = { 0, 0, 0 };
	long long supplyEvery = 10LL * 65536;
	int supplyMade = 0;
	std::vector<std::string> supplyNames;            // their strategy names (SB_NAME_LENGTH bytes each)
	// AROUND HIM (supplyat=around, Bishop 11-20, the owner's second stretch: "while you guys spawn aliens around him"): one at a time, every supplyEvery while fewer than
	// supplyCount of them hold a slot, at the middle of a room by hudson_supply.h's rule: out of his view (ModuleCurrVisArray, the game's own list of the rooms he can see),
	// 15 to 25 m from him, on his level, by the places he walks to next, the one nearest 20 m, so the same walk makes the same aliens; a far-away (40 m) or dead one
	// frees its slot. A room's middle is HudsonNav_MiddleOfRoom's (FarLoc_IsAPlaceToStand)
	bool supplyAround = false;
	long long supplyNextAt = 0;
	bool middlesMade = false;
	std::vector<VECTORCH> roomMiddles;
	// HITS ON HUDSON (Bishop 11-20, the owner: "more ceiling crawling aliens him faster", "hudson's getting more and more hurt"): his health and armour last frame
	bool healthSeen = false;
	int lastHealth = 0, lastArmour = 0;
	int const kHitBy = 3000;                         // mm: the alien nearest him within this is named as the one that hit him
	long thisFrame = 0;                              // the frame the alien code is in: the frame the next EndOfFrame is called with
	int lastSearchBy = 0;                            // who searched last: an alien's number, -1 another NPC, 0 nobody yet (a number, never a pointer: the NPC can be gone)

	enum Why
	{
		WhyNone, WhyWander, WhyAttack, WhyRetreat, WhyHuntStanding, WhyHuntCant, WhyHuntClimbs, WhyNearClimbs, WhyNearCant, WhyRippedOff, WhyNoTarget, WhyFarNoSearch,
		WhyFarNoBoxIt, WhyFarNoBoxTarget, WhyFarNoRoute, WhyFarNoLink, WhyFarStale, WhyFarPlain, WhyFarLane, WhyFarLaneCant, WhyWaitClimbs, WhyWaitCant, WhyAvoidance,
		WhyPounceJump, WhyDormant, WhyDying, WhyTaunting, WhyUnexplained, WhyCount
	};

	struct WhyWords
	{
		char const *code;
		int gravity;           // what the reading says UseStandardGravity is: 1 on, 0 off, -1 either (a state that leaves it as it was)
		char const *words;
	};

	WhyWords const kWhy[WhyCount] =
	{
		{ "-", -1, "no near state ran" },
		{ "WANDER", 1, "Wander turns gravity on every frame, 'don't climb on walls' (reb Bh_near.c 1340)" },
		{ "ATTACK", 1, "Attack turns gravity on (reb 1206)" },
		{ "RETREAT", 1, "Retreat turns gravity on (reb 1617)" },
		{ "HUNT-STANDING", 1, "Hunt turns gravity off only while crouched (reb 1781-1789), and it stands: AlienShouldBeCrawling said no (crawl=); its encouraged test (reb 2051) reads the route the last NPC to search left (lb=, by=)" },
		{ "HUNT-CROUCHED-CANT", 1, "Hunt, crouched, but AlienIsAbleToClimb says no (reb 1786: a praetorian, or a hand lost)" },
		{ "HUNT-CLIMBS", 0, "Hunt, crouched and able: gravity off (reb 1784)" },
		{ "APPROACH-NEAR", 0, "Approach under 8 m of its target: gravity off (reb 841)" },
		{ "APPROACH-NEAR-CANT", 1, "Approach under 8 m, but AlienIsAbleToClimb says no (reb 843)" },
		{ "RIPPED-OFF", 1, "Approach in the player's MODULE (pmod=1), heading nearly straight away from its surface (dot under -60000): it jumps off it, gravity on (reb 951 curving, 993 not)" },
		{ "APPROACH-NO-TARGET", -1, "Approach with no target goes to Hunt and leaves gravity as it was" },
		{ "APPROACH-FAR-NOSEARCH", 1, "Approach 8 m or more away: gravity on (reb 851), off again only if the route's first link is aliens-only (reb 974-979); it searched no route this frame (no waypoints in its module, D6), so that test read the route the last NPC to search left" },
		{ "APPROACH-FAR-D4-ALIEN", 1, "Approach 8 m or more away: gravity on (reb 851); no box of its module holds it (D4, npcboxes=0), so no route and no first link" },
		{ "APPROACH-FAR-D4-TARGET", 1, "Approach 8 m or more away: gravity on (reb 851); no box of its module holds the point it heads for (D4, tgtboxes=0)" },
		{ "APPROACH-FAR-D5", 1, "Approach 8 m or more away: gravity on (reb 851); no continuous route between its boxes and the target's (D5)" },
		{ "APPROACH-FAR-NOLINK", 1, "Approach 8 m or more away: gravity on (reb 851); the route has no first link (route= shows it: start==end is D3)" },
		{ "APPROACH-FAR-STALE", 1, "Approach 8 m or more away: gravity on (reb 851); the first link it read is in no box of its module" },
		{ "APPROACH-FAR-D2", 1, "Approach 8 m or more away: gravity on (reb 851); the route's first link is not aliens-only (D2)" },
		{ "APPROACH-FAR-LANE", 0, "Approach 8 m or more away, the route's first link aliens-only: gravity off (reb 976)" },
		{ "APPROACH-FAR-LANE-CANT", 1, "Approach 8 m or more away, the first link aliens-only, but AlienIsAbleToClimb says no (reb 978)" },
		{ "WAIT", 0, "Wait: gravity off, able to climb (reb 1472)" },
		{ "WAIT-CANT", 1, "Wait, not able to climb (reb 1474)" },
		{ "AVOIDANCE", -1, "Avoidance leaves gravity as it was" },
		{ "POUNCE-JUMP", -1, "Pounce (reb 2485) or Jump (reb 2646, 2679-2681)" },
		{ "DORMANT", 1, "Dormant (reb 2709) or Awakening (reb 2777): gravity on" },
		{ "DYING", -1, "Dying" },
		{ "TAUNTING", -1, "Taunting leaves gravity as it was" },
		{ "UNEXPLAINED", -1, "the reading above disagrees with the gravity the alien has (why= shows both)" },
	};

	int const kStates = 16;          // ALIEN_BHSTATE's values (ABS_Wait .. ABS_Taunting) are below this

	struct Watched
	{
		char name[SB_NAME_LENGTH];
		int number;                  // a1, a2, ... in the order the probe met them
		long firstFrame;
		char firstModule[64];
		// this frame: HudsonLanes_AlienBefore, then any HudsonLanes_Searched, then HudsonLanes_AlienAfter
		long beforeFrame;
		int ran;                     // the near state that ran
		int targetKind;              // its Target: 0 none, 1 the player, 2 another
		int targetDistance;          // to its Target, as Approach's 8 m test measures it (reb 834); -1 with none
		int encBefore;               // AlienIsEncouragedToCrawl() before its state ran: what Hunt's crouching test reads (reb 1753, 2051)
		int routeBy;                 // who searched last before its state ran (as lastSearchBy)
		char linkBefore[48];         // Global_Route.first_link then
		char crawl[16];              // which test in AlienShouldBeCrawling answers, asked as Hunt asks it, before its own search
		bool searched;
		int found, npcBoxes, targetBoxes;
		char dropout[16];
		VECTORCH searchedFor;
		VECTORCH heading;            // NPCGetWaypointDirection's direction, when it steered (found)
		int start, second, last, routeLength;
		char link[48];               // its own first link, after its search
		int encSearch;               // AlienIsEncouragedToCrawl() just after its own search: what Approach's test reads (reb 974)
		char in[48], tgt[48];        // the boxes its sweep found holding it, and holding the point it heads for
		char headsFor[12];           // second (reb bh_waypt.c 162) or start (169-176): the box its waypoint manager heads for
		VECTORCH targetPoint;        // its manager's current_target_point
		// the run
		long nearFrames, climbFrames, ceilingFrames, firstCeiling, firstClimb;
		char ceilingWhere[64];
		bool wasClimbing;
		long whyFrames[WhyCount];
		long stateFrames[kStates];
		int farAi;
		bool wasFar;
	};

	int const kMostWatched = 96;
	Watched watched[kMostWatched];   // plain data, all zero at the start: nothing of Hudson's may have code that runs before WinMain
	int watchedCount = 0;
	bool watchedFull = false;
	long stateTotals[kStates][4];    // by the near state that ran: alien-frames, gravity on, climbing (gravity its own, not on the floor), on the ceiling

	void Keep(char *to, size_t size, std::string const &text)
	{
		_snprintf(to, size - 1, "%s", text.c_str());
		to[size - 1] = 0;
	}

	char const *StateName(ALIEN_BHSTATE state)
	{
		switch (state)
		{
			case ABS_Wait: return "Wait";
			case ABS_Approach: return "Approach";
			case ABS_Hunt: return "Hunt";
			case ABS_Wander: return "Wander";
			case ABS_Retreat: return "Retreat";
			case ABS_Attack: return "Attack";
			case ABS_Avoidance: return "Avoidance";
			case ABS_Dying: return "Dying";
			case ABS_Pounce: return "Pounce";
			case ABS_Jump: return "Jump";
			case ABS_Dormant: return "Dormant";
			case ABS_Awakening: return "Awakening";
			case ABS_Taunting: return "Taunting";
			default: return "unknown";
		}
	}

	// A name from the level file: letters, digits and _ only, so that the line stays one line
	std::string Clean(char const *text, size_t most)
	{
		std::string out;

		for (size_t i = 0; text && i < most && text[i]; i++)
		{
			char const c = text[i];

			out.push_back((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ? c : '?');
		}

		return out.empty() ? std::string("(none)") : out;
	}

	std::string ModuleWords(MODULE const *module)
	{
		if (!module)
		{
			return "(none)";
		}

		return Clean(module->name, 64) + "#" + std::to_string(module->m_index);
	}

	void Write(std::string const &line)
	{
		if (file)
		{
			fputs(line.c_str(), file);
			fputs("\n", file);
		}
	}

	void Event(long frame, std::string const &words)
	{
		std::string const line = "EVENT f=" + std::to_string(frame) + " " + words;

		Write(line);
		LogString(("lanes: " + line).c_str());
	}

	STRATEGYBLOCK *TheAlien()
	{
		if (!alienMade)
		{
			return nullptr;
		}

		for (int i = 0; i < NumActiveStBlocks; i++)
		{
			STRATEGYBLOCK *const block = ActiveStBlockList[i];

			if (block && block->I_SBtype == I_BehaviourAlien && memcmp(block->SBname, alienName, SB_NAME_LENGTH) == 0)
			{
				return block;
			}
		}

		return nullptr;
	}

	STRATEGYBLOCK *TheAlienMadeAt(VECTORCH const &spot)
	{
		for (int i = 0; i < NumActiveStBlocks; i++)
		{
			STRATEGYBLOCK *const block = ActiveStBlockList[i];

			if (block && block->I_SBtype == I_BehaviourAlien && block->DynPtr && block->DynPtr->Position.vx == spot.vx && block->DynPtr->Position.vy == spot.vy &&
				block->DynPtr->Position.vz == spot.vz)
			{
				return block;
			}
		}

		return nullptr;
	}

	// A box's number in its AI module's list, or -1 if the pointer isn't one of them (another module's, or stale)
	int BoxNumber(WAYPOINT_HEADER const *boxes, WAYPOINT_VOLUME const *box)
	{
		if (!boxes || !box || !boxes->first_waypoint || boxes->num_waypoints <= 0)
		{
			return -1;
		}

		if (box < boxes->first_waypoint || box >= boxes->first_waypoint + boxes->num_waypoints)
		{
			return -1;
		}

		return (int)(box - boxes->first_waypoint);
	}

	// Where a link is: the box whose list holds it (that box's own number), or false if it's in no box of this AI module
	bool LinkPlace(WAYPOINT_HEADER const *boxes, WAYPOINT_LINK const *link, int *owner)
	{
		for (int b = 0; boxes && link && b < boxes->num_waypoints; b++)
		{
			WAYPOINT_VOLUME const *const box = boxes->first_waypoint + b;

			if (box->first_link && box->num_links > 0 && link >= box->first_link && link < box->first_link + box->num_links)
			{
				*owner = b;
				return true;
			}
		}

		return false;
	}

	// bh_waypoint.cpp's WaypointContainsPoint, copied (it is static there): a point in module space inside a box, its faces included
	bool BoxHolds(WAYPOINT_VOLUME const *box, VECTORCH const &point)
	{
		return point.vx >= box->centre.vx + box->min_extents.vx && point.vx <= box->centre.vx + box->max_extents.vx &&
			point.vy >= box->centre.vy + box->min_extents.vy && point.vy <= box->centre.vy + box->max_extents.vy &&
			point.vz >= box->centre.vz + box->min_extents.vz && point.vz <= box->centre.vz + box->max_extents.vz;
	}

	// How many of the 8 corners WaypointContainsNPC tests are in that box (the alien's drawn box, in module space)
	int CornersIn(STRATEGYBLOCK const *alien, WAYPOINT_VOLUME const *box, VECTORCH const &offset)
	{
		DISPLAYBLOCK const *const shown = alien->SBdptr;
		int count = 0;

		for (int c = 0; c < 8; c++)
		{
			VECTORCH corner;

			corner.vx = alien->DynPtr->Position.vx + ((c & 1) ? shown->ObMinX : shown->ObMaxX) - offset.vx;
			corner.vy = alien->DynPtr->Position.vy + ((c & 2) ? shown->ObMinY : shown->ObMaxY) - offset.vy;
			corner.vz = alien->DynPtr->Position.vz + ((c & 4) ? shown->ObMinZ : shown->ObMaxZ) - offset.vz;
			count += BoxHolds(box, corner) ? 1 : 0;
		}

		return count;
	}

	// EVERY ALIEN'S LINE: its record (see where watched[] is kept)
	Watched *Record(STRATEGYBLOCK const *alien, bool add)
	{
		for (int i = 0; i < watchedCount; i++)
		{
			if (memcmp(watched[i].name, alien->SBname, SB_NAME_LENGTH) == 0)
			{
				return &watched[i];
			}
		}

		if (!add)
		{
			return nullptr;
		}

		if (watchedCount >= kMostWatched)
		{
			watchedFull = true;
			return nullptr;
		}

		Watched *const w = &watched[watchedCount++];

		memset(w, 0, sizeof(*w));
		memcpy(w->name, alien->SBname, SB_NAME_LENGTH);
		w->number = watchedCount;
		w->firstFrame = thisFrame;
		w->firstCeiling = -1;
		w->firstClimb = -1;
		w->beforeFrame = -1;
		w->farAi = -2;
		Keep(w->firstModule, sizeof(w->firstModule), ModuleWords(alien->containingModule));
		return w;
	}

	int Apart(VECTORCH const &a, VECTORCH const &b)
	{
		double const dx = (double)a.vx - b.vx, dy = (double)a.vy - b.vy, dz = (double)a.vz - b.vz;

		return (int)sqrt(dx * dx + dy * dy + dz * dz);
	}

	std::string PlaceWords(VECTORCH const &v)
	{
		return std::to_string(v.vx) + "," + std::to_string(v.vy) + "," + std::to_string(v.vz);
	}

	// its surface by its down's y (down is +y): Vasquez's bands (11-20), FLOOR over 46000, WALL within 20000 of level, CEILING under -46000, a slope between
	char const *SurfaceOf(DYNAMICSBLOCK const *d)
	{
		int const vy = d->GravityDirection.vy;

		return vy > 46000 ? "floor" : vy < -46000 ? "ceiling" : (vy < 20000 && vy > -20000) ? "wall" : "slope";
	}

	// a link as "owner box.slot->target box flags", "none", or "elsewhere" (in no box of this AI module: another NPC's, or an earlier frame's)
	std::string LinkWords(WAYPOINT_HEADER const *boxes, WAYPOINT_LINK const *link)
	{
		if (!link)
		{
			return "none";
		}

		int owner = -1;

		if (!LinkPlace(boxes, link, &owner))
		{
			return "elsewhere";
		}

		char flags[16];

		_snprintf(flags, sizeof(flags) - 1, "0x%x", (unsigned)link->link_flags);
		flags[sizeof(flags) - 1] = 0;
		return std::to_string(owner) + "." + std::to_string((int)(link - boxes->first_waypoint[owner].first_link)) + "->" + std::to_string(link->link_target_index) + " " + flags;
	}

	// a direction's 16.16 dot with world up (up is -y), the direction made one long first; 0 for none
	int UpDot(VECTORCH const &v)
	{
		double const length = sqrt((double)v.vx * v.vx + (double)v.vy * v.vy + (double)v.vz * v.vz);

		return length > 0.0 ? (int)(-v.vy * 65536.0 / length) : 0;
	}

	// a 16.16 direction's dot with another direction of any length, made one long; 0 for none
	int DotWith(VECTORCH const &unit, VECTORCH const &v)
	{
		double const length = sqrt((double)v.vx * v.vx + (double)v.vy * v.vy + (double)v.vz * v.vz);

		return length > 0.0 ? (int)(((double)unit.vx * v.vx + (double)unit.vy * v.vy + (double)unit.vz * v.vz) / length) : 0;
	}

	// what a point it heads for is: an entry point into a module next to its own (which one), the player (within 2 m of him), or another point
	std::string PointWords(STRATEGYBLOCK const *alien, VECTORCH const &point)
	{
		AIMODULE *const mine = alien->containingModule ? alien->containingModule->m_aimodule : nullptr;

		for (int i = 0; mine && mine->m_link_ptrs && mine->m_link_ptrs[i]; i++)
		{
			AIMODULE *const next = mine->m_link_ptrs[i];
			FARENTRYPOINT const *const ep = GetAIModuleEP(next, mine);

			if (ep && abs(ep->position.vx + next->m_world.vx - point.vx) <= 2 && abs(ep->position.vy + next->m_world.vy - point.vy) <= 2 &&
				abs(ep->position.vz + next->m_world.vz - point.vz) <= 2)
			{
				return "EP>" + ModuleWords(next->m_module_ptrs ? next->m_module_ptrs[0] : nullptr) + "(ai " + std::to_string(next->m_index) + (ep->alien_only ? ", aliens-only)" : ")");
			}
		}

		if (Player && Player->ObStrategyBlock && Player->ObStrategyBlock->DynPtr && Apart(Player->ObStrategyBlock->DynPtr->Position, point) < 2000)
		{
			return "player";
		}

		return "point";
	}

	// which test in AlienShouldBeCrawling (bh_near.cpp; reb Bh_near.c 2025-2058) answers, in its order, asked of the alien as it is: its tests only read the alien and Global_Route
	char const *CrawlTest(STRATEGYBLOCK *alien, ALIEN_STATUS_BLOCK const *s)
	{
		if (alien->DynPtr->OrientMat.mat22 < 63000)
		{
			return "tilted";
		}

		if (alien->containingModule && (alien->containingModule->m_flags & MODULEFLAG_AIRDUCT))
		{
			return "duct";
		}

		if (AlienIsAbleToStand(alien) == 0)
		{
			return "legs";
		}

		if (s->BehaviourState == ABS_Attack)
		{
			return "no:attack";
		}

		if (AlienHasNoArms(alien))
		{
			return "no:arms";
		}

		if (AlienWays_Following(alien))
		{
			return "ways";
		}

		if (AlienIsEncouragedToCrawl() && s->EnableWaypoints)
		{
			return "encouraged";
		}

		return s->PreferToCrouch ? "prefer" : "no";
	}

	// why its gravity is as it is after its near state ran, read from what the state saw (the reasons and their lines: kWhy)
	Why GravityWhy(Watched const *w, ALIEN_STATUS_BLOCK const *s, DYNAMICSBLOCK const *d, bool able, bool inPlayersModule)
	{
		switch (w->ran)
		{
			case ABS_Wander: return WhyWander;
			case ABS_Attack: return WhyAttack;
			case ABS_Retreat: return WhyRetreat;
			case ABS_Hunt: return !s->IAmCrouched ? WhyHuntStanding : able ? WhyHuntClimbs : WhyHuntCant;
			case ABS_Wait: return able ? WhyWaitClimbs : WhyWaitCant;
			case ABS_Avoidance: return WhyAvoidance;
			case ABS_Pounce: case ABS_Jump: return WhyPounceJump;
			case ABS_Dormant: case ABS_Awakening: return WhyDormant;
			case ABS_Dying: return WhyDying;
			case ABS_Taunting: return WhyTaunting;
			case ABS_Approach: break;
			default: return WhyUnexplained;
		}

		if (w->targetKind == 0)
		{
			return WhyNoTarget;
		}

		// the rip-off sets the contact counter to 0 and gravity on (reb 951, 993)
		if (d->UseStandardGravity && d->TimeNotInContactWithFloor == 0 && inPlayersModule)
		{
			return WhyRippedOff;
		}

		// under 8 m: climbs when able (reb 841, 843); in a module with waypoints the encouraged test after it sets the same
		if (w->targetDistance < kCurveToPlayerDistance)
		{
			return able ? WhyNearClimbs : WhyNearCant;
		}

		// 8 m or more: gravity on (reb 851), and off again only when AlienIsEncouragedToCrawl, asked just after the search (reb 974), finds the first link aliens-only.
		// With no search of its own, it read what was there before.
		bool const encouraged = (w->searched ? w->encSearch : w->encBefore) != 0;

		if (encouraged && s->EnableWaypoints)
		{
			return able ? WhyFarLane : WhyFarLaneCant;
		}

		if (!w->searched)
		{
			return WhyFarNoSearch;
		}

		if (strcmp(w->dropout, "no-box") == 0)
		{
			return w->npcBoxes == 0 ? WhyFarNoBoxIt : WhyFarNoBoxTarget;
		}

		if (strcmp(w->dropout, "no-route") == 0)
		{
			return WhyFarNoRoute;
		}

		if (strcmp(w->link, "none") == 0)
		{
			return WhyFarNoLink;
		}

		return strcmp(w->link, "elsewhere") == 0 || strcmp(w->dropout, "no-modules") == 0 ? WhyFarStale : WhyFarPlain;
	}

	// lanes=watch: an alien whose module is out of the player's sight (no near state runs): a line when its AI module changes
	void FarScan()
	{
		for (int i = 0; i < NumActiveStBlocks; i++)
		{
			STRATEGYBLOCK *const block = ActiveStBlockList[i];

			if (!block || block->I_SBtype != I_BehaviourAlien || !block->DynPtr)
			{
				continue;
			}

			Watched *const w = Record(block, true);

			if (!w)
			{
				continue;
			}

			if (block->SBdptr)
			{
				w->wasFar = false;
				continue;
			}

			MODULE const *const module = block->containingModule;
			int const ai = module && module->m_aimodule ? module->m_aimodule->m_index : -1;

			if (!w->wasFar || ai != w->farAi)
			{
				ALIEN_STATUS_BLOCK const *const s = (ALIEN_STATUS_BLOCK const *)block->SBdataptr;

				Write("F a" + std::to_string(w->number) + " f=" + std::to_string(thisFrame) + " far mod=" + ModuleWords(module) + " ai=" + std::to_string(ai) + " pos=" + PlaceWords(block->DynPtr->Position) +
					" state=" + (s ? StateName(s->BehaviourState) : "?"));
				w->farAi = ai;
			}

			w->wasFar = true;
		}
	}

	// at the end: each alien, and the totals by state
	void WriteAliens()
	{
		Write("ALIENS " + std::to_string(watchedCount) + " met" + (watchedFull ? std::string(" (the table was full: later ones not counted)") : std::string()));

		for (int i = 0; i < watchedCount; i++)
		{
			Watched const *const w = &watched[i];
			std::string line = "ALIEN a" + std::to_string(w->number) + " met at f=" + std::to_string(w->firstFrame) + " in " + w->firstModule + ": near " + std::to_string(w->nearFrames) + " frames (";
			bool first = true;

			for (int st = 0; st < kStates; st++)
			{
				if (w->stateFrames[st])
				{
					line += (first ? "" : ", ") + std::string(StateName((ALIEN_BHSTATE)st)) + " " + std::to_string(w->stateFrames[st]);
					first = false;
				}
			}

			line += "); climbing " + std::to_string(w->climbFrames) + (w->firstClimb >= 0 ? " from f=" + std::to_string(w->firstClimb) : std::string()) + ", on the ceiling " + std::to_string(w->ceilingFrames);

			if (w->firstCeiling >= 0)
			{
				line += ": CEILING YES, first at f=" + std::to_string(w->firstCeiling) + " in " + w->ceilingWhere;
			}
			else
			{
				int top = -1;

				for (int why = 0; why < WhyCount; why++)
				{
					if (w->whyFrames[why] && kWhy[why].gravity == 1 && (top < 0 || w->whyFrames[why] > w->whyFrames[top]))
					{
						top = why;
					}
				}

				line += w->nearFrames == 0 ? std::string(": CEILING NO, never near") : top < 0 ? std::string(": CEILING NO") :
					": CEILING NO, gravity kept on mostly by " + std::string(kWhy[top].code) + " (" + std::to_string(w->whyFrames[top]) + " frames): " + kWhy[top].words;
			}

			line += "; every reason:";

			for (int why = 0; why < WhyCount; why++)
			{
				if (w->whyFrames[why])
				{
					line += " " + std::string(kWhy[why].code) + " " + std::to_string(w->whyFrames[why]);
				}
			}

			Write(line);
		}

		for (int st = 0; st < kStates; st++)
		{
			if (stateTotals[st][0])
			{
				Write("BY STATE " + std::string(StateName((ALIEN_BHSTATE)st)) + ": " + std::to_string(stateTotals[st][0]) + " near alien-frames, gravity on " + std::to_string(stateTotals[st][1]) +
					", climbing " + std::to_string(stateTotals[st][2]) + ", on the ceiling " + std::to_string(stateTotals[st][3]));
			}
		}
	}

	struct Seen
	{
		bool isNear;
		ALIEN_BHSTATE state;
		int distance;
		bool sameAiModuleAsThePlayer;
		bool moduleHasBoxes;
		int boxesHoldingIt;
		int boxesHoldingTheTarget;
		bool route;
		bool routeIsOneBox;
		bool linkFound;
		bool linkAlienOnly;
		bool linkNone;
		bool encouraged;
		bool standardGravity;
		int noContact;
		bool crouched;
		std::string where;   // module and box
	};

	// Why it came down, by the audit's rows, from what the frame shows
	std::string WhyItCameDown(Seen const &s)
	{
		if (!s.isNear)
		{
			return "S3: it went far (its module out of the player's sight): the far AI moves it module to module and puts it down upright (bh_far.cpp LocateFarNPCInAIModule)";
		}

		// the counter is kGravityKicksIn while it touches something and counts down without: at 0 its down is made straight down (until 11-19 ~21:30 this
		// read >= kGravityKicksIn, which is the TOUCHING value: no drop of the first runs was put down to it, both had standard gravity on)
		if (!s.standardGravity && s.noContact <= 0)
		{
			return "D19: it lost contact for a quarter second while climbing, so its down became straight down (dynamics.cpp, TIME_BEFORE_GRAVITY_KICKS_IN)";
		}

		switch (s.state)
		{
			case ABS_Attack: return "D13: Attack, within 2 m of its target (the wanted end of a lane)";
			case ABS_Wander: return "D12: Wander turns gravity on every frame";
			case ABS_Hunt: return s.crouched ? "D11: Hunt turns gravity on and climbs only when crouched and able (crouched, but not able)" : "D11: Hunt turns gravity on, and it isn't crouched";
			case ABS_Avoidance: return "D18: Avoidance, after something stopped it head-on or it couldn't reach its target";
			case ABS_Retreat: return "D14: Retreat";
			case ABS_Pounce: case ABS_Jump: return "D15: Pounce or Jump";
			case ABS_Dormant: case ABS_Awakening: return "D16: Dormant or Awakening";
			case ABS_Wait: return "D17: Wait";
			case ABS_Dying: return "D20: Dying";
			case ABS_Taunting: return "Taunting (not in the audit's table)";
			case ABS_Approach: break;
			default: return "a state the audit doesn't name";
		}

		if (s.distance < kCurveToPlayerDistance)
		{
			return s.sameAiModuleAsThePlayer ? "D9: in the player's module, under 8 m, moving nearly straight away from its surface: ripped off it with a jump (bh_near.cpp)"
				: "D10: Approach under 8 m should keep climbing outside the player's module: not explained by the audit";
		}

		if (s.encouraged)
		{
			return "D1 with an encouraged route: not explained by the audit";
		}

		std::string const d1 = "D1: Approach 8 m or more away turns gravity on unless the route's first link is alien-only; ";

		if (!s.moduleHasBoxes)
		{
			return d1 + "D6: its module has no waypoints, so the waypoint code doesn't run and the first link it reads is stale";
		}

		if (s.boxesHoldingIt == 0)
		{
			return d1 + "D4: no box of its module holds the alien";
		}

		if (s.boxesHoldingTheTarget == 0)
		{
			return d1 + "D4: no box of its module holds its target (the entry point into the next module, or the player)";
		}

		if (!s.route)
		{
			return d1 + "D5: no continuous route from its boxes to the target's (unlinked boxes, or a one-way link against it)";
		}

		if (s.routeIsOneBox || s.linkNone)
		{
			return d1 + "D3: a box it touches also holds its target (start = end), so there is no first link";
		}

		if (s.linkFound && !s.linkAlienOnly)
		{
			return d1 + "D2: the route's first link (stored on the second box, pointing back to the first) is not alien-only";
		}

		return d1 + "the first link is not one of this module's (stale)";
	}

	void Finish(char const *why)
	{
		if (watching)
		{
			WriteAliens();
			Write(std::string("END ") + why);
			LogString((std::string("lanes: the watch ended: ") + why).c_str());

			if (file)
			{
				fclose(file);
				file = nullptr;
			}

			return;
		}

		std::string verdict;

		if (!everClimbed)
		{
			verdict = "VERDICT NEVER WENT UP: it never climbed";
		}
		else if (dropped)
		{
			verdict = "VERDICT DROPPED at f=" + std::to_string(droppedAt) + " in " + std::string(dropWhere) + ": " + dropReason;
		}
		else
		{
			verdict = "VERDICT STAYED UP: it climbed at f=" + std::to_string(climbedAt) + " in " + std::string(climbedWhere) + (everOnTheCeiling ? ", on the ceiling at f=" + std::to_string(ceilingAt) + " in " +
				std::string(ceilingWhere) : std::string(", never on the ceiling itself")) + ", and crossed " + std::to_string(doorwaysUp) + " doorways up there" +
				(inPlayersModule ? std::string(", into the player's module ") + (upWhenItCameIn ? "still up" : "on the floor") + " at f=" + std::to_string(inPlayersModuleAt) : std::string(", never reaching the player's module"));
		}

		std::string const case2 = inPlayersModule ? "CASE 2 (the player's module): " + (downInPlayersModule[0] == 0 ? std::string("it never came down there") : std::string(downInPlayersModule)) +
			(attacked ? ", first attack at f=" + std::to_string(attackedAt) : std::string(", no attack")) : std::string("CASE 2: it never reached the player's module");

		double const stutterSeconds = (double)stutterTime / 65536.0;
		char stutter[400];

		_snprintf(stutter, sizeof(stutter) - 1, "STUTTER near in AI modules with waypoints for %.1f s (%ld frames): its start box switched %ld times (%.2f a second; %ld of them back to the box "
			"before), its target box %ld times (%.2f a second); the busiest second %ld switches", stutterSeconds, stutterFrames, stutterBoxSwitches,
			stutterSeconds > 0.0 ? stutterBoxSwitches / stutterSeconds : 0.0, stutterBoxReturns, stutterTargetSwitches, stutterSeconds > 0.0 ? stutterTargetSwitches / stutterSeconds : 0.0,
			stutterWorstSecond > stutterThisSecond ? stutterWorstSecond : stutterThisSecond);
		stutter[sizeof(stutter) - 1] = 0;

		Write(verdict);
		Write(case2);
		Write(stutter);
		LogString((std::string("lanes: ") + stutter).c_str());
		WriteAliens();
		Write(std::string("END ") + why);
		LogString(("lanes: " + verdict).c_str());
		LogString(("lanes: " + case2).c_str());

		if (file)
		{
			fclose(file);
			file = nullptr;
		}
	}

	// lanes=watch, each frame from the second on: the far aliens, the player's landing, and the end
	bool WatchFrame(long frame, STRATEGYBLOCK *me, const char **howItEnded, int *exitCode)
	{
		FarScan();

		if (!watchStarted)
		{
			if (frame < kFramesBeforeTheAlien)
			{
				if (file)
				{
					fflush(file);
				}

				return false;
			}

			// he has had his second to land: in the room he was put in, or the run ends here (when he plays, wherever the level's start put him)
			VECTORCH const landed = me->DynPtr->Position;

			if (!hePlays && me->containingModule != placedIn)
			{
				Write("PLAYER NOT KEPT at standat=: at " + PlaceWords(landed) + " in " + ModuleWords(me->containingModule) + ", not " + ModuleWords(placedIn));
				*howItEnded = "the player didn't stay where standat= put him";
				*exitCode = kHudsonExitMarineGone;
				Finish(*howItEnded);
				return true;
			}

			Write("PLAYER LANDED at " + PlaceWords(landed) + " in " + ModuleWords(me->containingModule));
			Write(std::string("LAMP ") + (ShoulderLamp_IsOn() ? "on" : "off") + (ShoulderLamp_RealLightIsWanted() ? ", a real light" : ", painted"));
			Event(frame, hePlays ? "WATCH STARTS: he plays his list from here" : "WATCH STARTS: three minutes of game time from here");
			watchStarted = true;
			headwayFrom = landed;
			headwayAt = 0;
		}

		gameTime += NormalFrameTime;

		// every frame, so that a run stopped from outside loses one line at most
		if (file)
		{
			fflush(file);
		}

		// HITS AND REFILLS: each frame his health or armour went down is a HIT, with the alien nearest him within kHitBy (its number, state and distance) as the one that
		// hit him; each time his health went up again (play fills it when it runs out) a REFILL. Health and armour as the game keeps them, a whole one = 65536
		{
			int const health = me->SBDamageBlock.Health, armour = me->SBDamageBlock.Armour;

			if (healthSeen && (health < lastHealth || armour < lastArmour))
			{
				STRATEGYBLOCK *nearest = nullptr;
				double nearestD = kHitBy;

				for (int i = 0; i < NumActiveStBlocks; i++)
				{
					STRATEGYBLOCK *const b = ActiveStBlockList[i];

					if (b && b->I_SBtype == I_BehaviourAlien && b->DynPtr && b->SBdataptr)
					{
						double const dx = (double)b->DynPtr->Position.vx - me->DynPtr->Position.vx, dy = (double)b->DynPtr->Position.vy - me->DynPtr->Position.vy,
							dz = (double)b->DynPtr->Position.vz - me->DynPtr->Position.vz;
						double const d = sqrt(dx * dx + dy * dy + dz * dz);

						if (d < nearestD)
						{
							nearest = b;
							nearestD = d;
						}
					}
				}

				Watched const *const w = nearest ? Record(nearest, true) : nullptr;
				char words[200];

				_snprintf(words, sizeof(words) - 1, "health %.1f -> %.1f, armour %.1f -> %.1f", lastHealth / 65536.0, health / 65536.0, lastArmour / 65536.0, armour / 65536.0);
				words[sizeof(words) - 1] = 0;
				Write("HIT f=" + std::to_string(frame) + " " + words + " at " + PlaceWords(me->DynPtr->Position) + " in " + ModuleWords(me->containingModule) + ", by " +
					(w ? "a" + std::to_string(w->number) + " (" + StateName(((ALIEN_STATUS_BLOCK *)nearest->SBdataptr)->BehaviourState) + ", " + std::to_string((int)nearestD) + " mm)" :
					std::string("no alien within 3 m")));
			}

			if (healthSeen && health > lastHealth)
			{
				char words[120];

				_snprintf(words, sizeof(words) - 1, "health %.1f -> %.1f", lastHealth / 65536.0, health / 65536.0);
				words[sizeof(words) - 1] = 0;
				Write("REFILL f=" + std::to_string(frame) + " " + words);
			}

			lastHealth = health;
			lastArmour = armour;
			healthSeen = true;
		}

		// AROUND HIM: while fewer than supplyCount of its aliens are alive, one every supplyEvery, at the middle of a room he can't see 15 to 25 m from him
		if (supplyAround && supplyCount > 0 && gameTime >= supplyNextAt)
		{
			int alive = 0;

			for (int i = 0; i < NumActiveStBlocks; i++)
			{
				STRATEGYBLOCK *const b = ActiveStBlockList[i];

				// one of the supply's keeps its slot while alive and near enough (hudson_supply.h): a far-away one frees it, so the supply keeps coming
				if (b && b->I_SBtype == I_BehaviourAlien && b->DynPtr && HudsonLanes_IsTheAlien(b))
				{
					double const ox = (double)b->DynPtr->Position.vx - me->DynPtr->Position.vx, oy = (double)b->DynPtr->Position.vy - me->DynPtr->Position.vy,
						oz = (double)b->DynPtr->Position.vz - me->DynPtr->Position.vz;

					alive += HudsonSupply_HoldsASlot(!NPC_IsDead(b), sqrt(ox * ox + oy * oy + oz * oz)) ? 1 : 0;
				}
			}

			if (!middlesMade)
			{
				middlesMade = true;

				for (int room = 0; room < AIModuleArraySize; room++)
				{
					VECTORCH place;
					int away = 0;

					if (HudsonNav_MiddleOfRoom(room, &place, &away))
					{
						roomMiddles.push_back(place);
					}
				}
			}

			if (alive < supplyCount && supplyMade < HudsonSupply_MostMade(supplyCount))
			{
				// THE ROOM (hudson_supply.h): out of his view, 15 to 25 m from him, on his level, by the places he walks to next, the one nearest 20 m
				VECTORCH const here = me->DynPtr->Position;
				std::vector<HudsonSupplyRoom> rooms;
				std::vector<HudsonSupplyPlace> ahead;

				for (VECTORCH const &p : roomMiddles)
				{
					VECTORCH at = p;
					MODULE const *const room = ModuleFromPosition(&at, (MODULE *)0);
					HudsonSupplyRoom r;

					r.x = p.vx;
					r.y = p.vy;
					r.z = p.vz;
					r.inHisView = !room || !ModuleCurrVisArray || ModuleCurrVisArray[room->m_index];
					rooms.push_back(r);
				}

				for (int k = 0; k < 6; k++)
				{
					HudsonSupplyPlace p;

					if (HudsonPlay_PlaceAhead(k, &p.x, &p.y, &p.z))
					{
						ahead.push_back(p);
					}
				}

				int const pick = HudsonSupply_Pick(rooms, here.vx, here.vy, here.vz, ahead);
				VECTORCH const *const best = pick >= 0 ? &roomMiddles[(size_t)pick] : nullptr;
				double bestD = 0;

				if (best)
				{
					double const dx = (double)best->vx - here.vx, dy = (double)best->vy - here.vy, dz = (double)best->vz - here.vz;

					bestD = sqrt(dx * dx + dy * dy + dz * dz);
				}

				if (best)
				{
					VECTORCH spot = *best;

					spot.vy -= kAlienStartsAboveTheFloor;
					CreateAlienBot(&spot, 0);

					STRATEGYBLOCK *const made = TheAlienMadeAt(spot);

					supplyMade++;

					if (made)
					{
						supplyNames.push_back(std::string(made->SBname, SB_NAME_LENGTH));
					}

					char howFar[40];   // not far: a Windows macro

					_snprintf(howFar, sizeof(howFar) - 1, "%.1f m from him", bestD / 1000.0);
					howFar[sizeof(howFar) - 1] = 0;
					Event(frame, std::string("SUPPLY ") + std::to_string(supplyMade) + " of " + std::to_string(supplyCount) + (made ? " MADE at " : " NOT MADE (the game made none) at ") +
						PlaceWords(spot) + " in " + ModuleWords(ModuleFromPosition(&spot, (MODULE *)0)) + " (around him: " + howFar + ", out of his view; " + std::to_string(alive + (made ? 1 : 0)) +
						" alive)");
					supplyNextAt = gameTime + supplyEvery;
				}
				else
				{
					supplyNextAt = gameTime + 65536;   // no room fits now: look again in a second
				}
			}
			else
			{
				supplyNextAt = gameTime + 65536;
			}
		}

		// the supply: the next alien when its time has come (the first at the watch's start)
		while (!supplyAround && supplyMade < supplyCount && gameTime >= supplyEvery * supplyMade)
		{
			VECTORCH spot = supplyAt;

			spot.vy -= kAlienStartsAboveTheFloor;   // up is smaller, as alienat=
			CreateAlienBot(&spot, 0);

			STRATEGYBLOCK *const made = TheAlienMadeAt(spot);

			supplyMade++;

			if (made)
			{
				supplyNames.push_back(std::string(made->SBname, SB_NAME_LENGTH));
			}

			Event(frame, std::string("SUPPLY ") + std::to_string(supplyMade) + " of " + std::to_string(supplyCount) + (made ? " MADE at " : " NOT MADE (the game made none) at ") + PlaceWords(spot) +
				" in " + ModuleWords(ModuleFromPosition(&spot, (MODULE *)0)));
		}

		// he plays: the watch ends when his list is done, or when he has made no headway for a while (probe v3)
		if (hePlays)
		{
			VECTORCH const here = me->DynPtr->Position;
			double const dx = (double)here.vx - headwayFrom.vx, dy = (double)here.vy - headwayFrom.vy, dz = (double)here.vz - headwayFrom.vz;

			if (dx * dx + dy * dy + dz * dz >= (double)kPlayHeadway * kPlayHeadway)
			{
				headwayFrom = here;
				headwayAt = gameTime;
			}

			char const *const end = HudsonPlay_FinishedTheList() ? "route done: he has been down his whole list" : gameTime - headwayAt >= kPlayNoHeadway ?
				"no headway (1 m) for 15 s" : gameTime >= kPlayMost ? "3 minutes of game time" : frame >= mostFrames ? "frames= ran out" : nullptr;

			if (end)
			{
				Write("PLAYER ENDS at " + PlaceWords(here) + " in " + ModuleWords(me->containingModule));
				*howItEnded = end;
				*exitCode = HudsonPlay_FinishedTheList() ? kHudsonExitFinished : kHudsonExitMarineStuck;
				Finish(*howItEnded);
				return true;
			}

			return false;
		}

		if (gameTime >= kWatchMost || frame >= mostFrames)
		{
			*howItEnded = gameTime >= kWatchMost ? "three minutes of game time watched" : "frames= ran out";
			*exitCode = kHudsonExitFinished;
			Finish(*howItEnded);
			return true;
		}

		return false;
	}
}

void HudsonLanes_Begin(int alienAtX, int alienAtY, int alienAtZ, int standAtX, int standAtY, int standAtZ, int framesMost, bool onCeiling, bool watch)
{
	if (!HudsonRunIsOn)
	{
		return;
	}

	asked = true;
	alienAt.vx = alienAtX;
	alienAt.vy = alienAtY;
	alienAt.vz = alienAtZ;
	standAt.vx = standAtX;
	standAt.vy = standAtY;
	standAt.vz = standAtZ;
	mostFrames = framesMost;
	alienOnCeiling = onCeiling;
	watching = watch;
}

void HudsonLanes_HePlays()
{
	hePlays = true;
}

void HudsonLanes_Supply(int count, int atX, int atY, int atZ, int everySeconds, bool around)
{
	supplyCount = count > 0 ? count : 0;
	supplyAround = around;
	supplyAt.vx = atX;
	supplyAt.vy = atY;
	supplyAt.vz = atZ;
	supplyEvery = (long long)(everySeconds > 0 ? everySeconds : 10) * 65536;
}

bool HudsonLanes_IsTheAlien(const STRATEGYBLOCK *block)
{
	if (!HudsonRunIsOn || !asked || !block || block->I_SBtype != I_BehaviourAlien)
	{
		return false;
	}

	// a supply alien (supply=) moves as the probe's one alien does
	for (std::string const &name : supplyNames)
	{
		if (memcmp(block->SBname, name.data(), SB_NAME_LENGTH) == 0)
		{
			return true;
		}
	}

	return alienMade && memcmp(block->SBname, alienName, SB_NAME_LENGTH) == 0;
}

// What the alien's own code read, at the moment it read it (hudson_lanes.h, "EVERY ALIEN'S LINE" above). They only read and keep: nothing here changes the alien.
void HudsonLanes_AlienBefore(const STRATEGYBLOCK *block)
{
	if (!HudsonRunIsOn || !asked || !block || !block->SBdataptr || !block->DynPtr)
	{
		return;
	}

	STRATEGYBLOCK *const alien = (STRATEGYBLOCK *)block;   // bh_near.cpp's questions take a block that isn't const; they only read it
	Watched *const w = Record(alien, true);

	if (!w)
	{
		return;
	}

	ALIEN_STATUS_BLOCK const *const s = (ALIEN_STATUS_BLOCK const *)alien->SBdataptr;
	MODULE const *const module = alien->containingModule;
	WAYPOINT_HEADER const *const boxes = module && module->m_aimodule ? module->m_aimodule->m_waypoints : nullptr;
	VECTORCH const none = { 0, 0, 0 };

	w->beforeFrame = thisFrame;
	w->ran = s->BehaviourState;
	w->targetKind = 0;
	w->targetDistance = -1;

	if (s->Target && s->Target->DynPtr)
	{
		w->targetKind = Player && s->Target == Player->ObStrategyBlock ? 1 : 2;
		w->targetDistance = Apart(alien->DynPtr->Position, s->Target->DynPtr->Position);
	}

	w->encBefore = AlienIsEncouragedToCrawl();
	w->routeBy = lastSearchBy;
	Keep(w->linkBefore, sizeof(w->linkBefore), LinkWords(boxes, Global_Route.first_link));
	Keep(w->crawl, sizeof(w->crawl), CrawlTest(alien, s));

	// nothing searched yet this frame
	w->searched = false;
	w->found = 0;
	w->npcBoxes = 0;
	w->targetBoxes = 0;
	Keep(w->dropout, sizeof(w->dropout), "-");
	w->searchedFor = none;
	w->heading = none;
	w->start = w->second = w->last = -1;
	w->routeLength = -1;
	Keep(w->link, sizeof(w->link), "-");
	w->encSearch = 0;
	Keep(w->in, sizeof(w->in), "-");
	Keep(w->tgt, sizeof(w->tgt), "-");
	Keep(w->headsFor, sizeof(w->headsFor), "-");
	w->targetPoint = none;
}

void HudsonLanes_Searched(const STRATEGYBLOCK *npc, const VECTORCH *target, const VECTORCH *heading, int found)
{
	if (!HudsonRunIsOn || !asked || !npc)
	{
		return;
	}

	if (npc->I_SBtype != I_BehaviourAlien || !npc->SBdataptr)
	{
		lastSearchBy = -1;
		return;
	}

	Watched *const w = Record(npc, true);

	lastSearchBy = w ? w->number : -1;

	if (!w || w->beforeFrame != thisFrame)
	{
		return;   // a search outside its near state: not one of the lines
	}

	MODULE const *const module = npc->containingModule;
	WAYPOINT_HEADER const *const boxes = module && module->m_aimodule ? module->m_aimodule->m_waypoints : nullptr;
	ALIEN_STATUS_BLOCK const *const s = (ALIEN_STATUS_BLOCK const *)npc->SBdataptr;
	WAYPOINT_MANAGER const &manager = s->waypointManager;
	VECTORCH const none = { 0, 0, 0 };

	w->searched = true;
	w->found = found;
	w->npcBoxes = NPC_Waypoints;
	w->targetBoxes = Num_Target_Waypoints;
	w->searchedFor = target ? *target : none;
	w->heading = found && heading ? *heading : none;
	// the dropouts of NPCGetWaypointDirection (reb bh_waypt.c 100, 122, 136, 151), in its order
	Keep(w->dropout, sizeof(w->dropout), !playerPherModule || !module ? "no-modules" : NPC_Waypoints == 0 || Num_Target_Waypoints == 0 ? "no-box" : !found ? "no-route" :
		Global_Route.start == Global_Route.last ? "start=end" : "none");

	std::string in, tgt;

	for (int b = 0; boxes && b < boxes->num_waypoints; b++)
	{
		WAYPOINT_VOLUME const *const box = boxes->first_waypoint + b;

		if (box->contains_npc)
		{
			in += (in.empty() ? "" : ",") + std::to_string(b);
		}

		if (box->contains_target)
		{
			tgt += (tgt.empty() ? "" : ",") + std::to_string(b);
		}
	}

	Keep(w->in, sizeof(w->in), in.empty() ? std::string("-") : in);
	Keep(w->tgt, sizeof(w->tgt), tgt.empty() ? std::string("-") : tgt);
	w->start = BoxNumber(boxes, Global_Route.start);
	w->second = BoxNumber(boxes, Global_Route.second);
	w->last = BoxNumber(boxes, Global_Route.last);
	w->routeLength = Global_Route.num_waypoints == ONE_FIXED ? -1 : Global_Route.num_waypoints;
	Keep(w->link, sizeof(w->link), LinkWords(boxes, Global_Route.first_link));
	w->encSearch = AlienIsEncouragedToCrawl();
	Keep(w->headsFor, sizeof(w->headsFor), !found || !manager.current_target ? "-" : manager.current_target == Global_Route.second && Global_Route.second != Global_Route.start ? "second" :
		manager.current_target == Global_Route.start ? "start" : "other");
	w->targetPoint = manager.current_target_point;
}

void HudsonLanes_AlienAfter(const STRATEGYBLOCK *block)
{
	if (!HudsonRunIsOn || !asked || !block || !block->SBdataptr || !block->DynPtr)
	{
		return;
	}

	STRATEGYBLOCK *const alien = (STRATEGYBLOCK *)block;
	Watched *const w = Record(alien, false);

	if (!w || w->beforeFrame != thisFrame)
	{
		return;
	}

	ALIEN_STATUS_BLOCK const *const s = (ALIEN_STATUS_BLOCK const *)alien->SBdataptr;
	DYNAMICSBLOCK const *const d = alien->DynPtr;
	MODULE const *const module = alien->containingModule;
	AIMODULE const *const ai = module ? module->m_aimodule : nullptr;
	WAYPOINT_HEADER const *const boxes = ai ? ai->m_waypoints : nullptr;
	bool const able = AlienIsAbleToClimb(alien) != 0;
	bool const inPlayersModule = playerPherModule && module == playerPherModule;
	bool const inPlayersAi = playerPherModule && ai && ai == playerPherModule->m_aimodule;
	Why const read = GravityWhy(w, s, d, able, inPlayersModule);
	int const gravityOn = d->UseStandardGravity ? 1 : 0;
	Why const why = kWhy[read].gravity < 0 || kWhy[read].gravity == gravityOn ? read : WhyUnexplained;
	char const *const surface = SurfaceOf(d);
	bool const climbing = !gravityOn && strcmp(surface, "floor") != 0;
	bool const onCeiling = !gravityOn && strcmp(surface, "ceiling") == 0;
	int const st = w->ran >= 0 && w->ran < kStates ? w->ran : 0;
	std::string const where = ModuleWords(module);

	w->nearFrames++;
	w->climbFrames += climbing ? 1 : 0;
	w->ceilingFrames += onCeiling ? 1 : 0;
	w->whyFrames[why]++;
	w->stateFrames[st]++;
	stateTotals[st][0]++;
	stateTotals[st][1] += gravityOn;
	stateTotals[st][2] += climbing ? 1 : 0;
	stateTotals[st][3] += onCeiling ? 1 : 0;

	// its contacts, from its last movement
	int contacts = 0, withNpc = 0;
	std::string touched;   // cn=: each report's normal and point (the first 6), for Vasquez's contact set (11-20: which faces the game counts)

	for (COLLISIONREPORT const *r = d->CollisionReportPtr; r && contacts < 256; r = r->NextCollisionReportPtr)
	{
		contacts++;
		withNpc += r->ObstacleSBPtr ? 1 : 0;

		if (contacts <= 6)
		{
			touched += (touched.empty() ? "" : ";") + PlaceWords(r->ObstacleNormal) + "@" + PlaceWords(r->ObstaclePoint) + (r->ObstacleSBPtr ? "npc" : "");
		}
	}

	bool const steered = w->searched && w->found;
	std::string const target = w->targetKind == 0 ? std::string("none") : std::string(w->targetKind == 1 ? "player:" : "other:") + std::to_string(w->targetDistance) +
		(w->targetDistance < kCurveToPlayerDistance ? "<8m" : "");
	std::string const ep = w->ran == ABS_Wander || w->ran == ABS_Hunt ? std::string(w->ran == ABS_Hunt ? "hunt:" : "wander:") + PointWords(alien, s->wanderData.worldPosition) + ":" +
		std::to_string(Apart(d->Position, s->wanderData.worldPosition)) : std::string("-");
	std::string const go = w->searched ? PointWords(alien, w->searchedFor) + ":" + std::to_string(Apart(d->Position, w->searchedFor)) : std::string("-");
	std::string const route = w->start >= 0 ? std::to_string(w->start) + ">" + (w->second >= 0 ? std::to_string(w->second) : std::string("-")) + ".." +
		(w->last >= 0 ? std::to_string(w->last) : std::string("-")) + " len=" + std::to_string(w->routeLength) : std::string("-");
	std::string const by = w->routeBy > 0 ? "a" + std::to_string(w->routeBy) : w->routeBy < 0 ? std::string("npc") : std::string("-");
	char t[32];

	_snprintf(t, sizeof(t) - 1, " t=%.3f", (double)gameTime / 65536.0);
	t[sizeof(t) - 1] = 0;

	std::string const line = "A a" + std::to_string(w->number) + " f=" + std::to_string(thisFrame) + t + " ran=" + StateName((ALIEN_BHSTATE)w->ran) + " now=" + StateName(s->BehaviourState) +
		" mod=" + where + " ai=" + std::to_string(ai ? ai->m_index : -1) + " wps=" + std::to_string(boxes ? boxes->num_waypoints : 0) + " pmod=" + (inPlayersModule ? "1" : "0") +
		" pai=" + (inPlayersAi ? "1" : "0") + " pos=" + PlaceWords(d->Position) + " on=" + surface + " down=" + PlaceWords(d->GravityDirection) + " mat=" + std::to_string(d->OrientMat.mat21) + "," +
		std::to_string(d->OrientMat.mat22) + "," + std::to_string(d->OrientMat.mat23) + " std=" + std::to_string(gravityOn) + " nocontact=" + std::to_string(d->TimeNotInContactWithFloor) +
		" floor=" + (d->IsInContactWithFloor ? "1" : "0") + " contacts=" + std::to_string(contacts) + "/" + std::to_string(withNpc) + " cn=" + (touched.empty() ? std::string("-") : touched) + " crouch=" + (s->IAmCrouched ? "1" : "0") +
		" crawl=" + w->crawl + " able=" + (able ? "1" : "0") + " tgt=" + target + " ep=" + ep + " go=" + go + " search=" + w->dropout + " npcboxes=" + std::to_string(w->npcBoxes) +
		" tgtboxes=" + std::to_string(w->targetBoxes) + " in=" + w->in + " tgtin=" + w->tgt + " route=" + route + " heads=" + w->headsFor +
		" point=" + (steered ? PlaceWords(w->targetPoint) : std::string("-")) + " link=" + w->link + " lb=" + w->linkBefore + " by=" + by + " enc=" + std::to_string(w->encSearch) +
		" encb=" + std::to_string(w->encBefore) + " heading=" + (steered ? PlaceWords(w->heading) : std::string("-")) + " hup=" + (steered ? std::to_string(UpDot(w->heading)) : std::string("-")) +
		" vel=" + PlaceWords(d->LinVelocity) + " vup=" + std::to_string(UpDot(d->LinVelocity)) + " rdot=" + (steered ? std::to_string(DotWith(d->GravityDirection, w->heading)) : std::string("-")) +
		" why=" + kWhy[why].code + (why == WhyUnexplained ? std::string("(read ") + kWhy[read].code + ")" : std::string());

	Write(line);

	// its own events (written only, not in the game's log: a level of aliens makes many)
	std::string const who = "EVENT f=" + std::to_string(thisFrame) + " a" + std::to_string(w->number) + " ";

	if (climbing && !w->wasClimbing)
	{
		w->firstClimb = w->firstClimb < 0 ? thisFrame : w->firstClimb;
		Write(who + "CLIMBS onto the " + surface + " in " + where + " (" + StateName((ALIEN_BHSTATE)w->ran) + ", " + kWhy[why].code + ")");
	}

	if (onCeiling && w->firstCeiling < 0)
	{
		w->firstCeiling = thisFrame;
		Keep(w->ceilingWhere, sizeof(w->ceilingWhere), where);
		Write(who + "ON THE CEILING in " + where);
	}

	if (!climbing && w->wasClimbing)
	{
		std::string const cause = gravityOn ? std::string(kWhy[why].code) + ": " + kWhy[why].words : d->TimeNotInContactWithFloor <= 0 ?
			std::string("LOST-CONTACT: no contact for a quarter second, its down went straight down (D19)") : std::string("its down is the floor's again, its gravity still its own");

		Write(who + "DOWN in " + where + " (" + StateName((ALIEN_BHSTATE)w->ran) + "): " + cause);
	}

	w->wasClimbing = climbing;
}

bool HudsonLanes_EndOfFrame(long frame, const char **howItEnded, int *exitCode)
{
	if (!HudsonRunIsOn || !asked)
	{
		return false;
	}

	// the alien code's calls until the next call here are of the next frame
	thisFrame = frame + 1;

	STRATEGYBLOCK *const me = Player ? Player->ObStrategyBlock : nullptr;

	if (!me || !me->DynPtr || !me->SBdataptr)
	{
		return false;
	}

	// He can't die (acid, a blow), and he has no hands on the keys
	((PLAYER_STATUS *)me->SBdataptr)->IsImmortal = 1;

	if (frame == 1)
	{
		// the alien ways are off already (floorislava=off, before the level); this is the other of AvPx's two switches
		NewDynamicCollisionsOn = 0;

		char const *const folder = GetSaveFolderPath();
		std::string const path = std::string(folder ? folder : "") + kLanesFileName;

		file = fopen(path.c_str(), "w");

		// THE MODE FIRST (probe v3, Bishop 11-20: "the run's first log line states the mode, and a run that isn't in stock mode stops itself"): every flag of
		// stock_flags.h as the game has it now, and the level's own alien-ways switch (alien_ways.cpp, set at the level's start)
		std::string const flags = StockFlags_Words();
		bool const stock = flags.size() >= 7 && flags.compare(flags.size() - 7, 7, "(stock)") == 0 && !AlienWaysSwitchedOn;

		Write("MODE " + StockFlags_Words() + "; " + DebugView_Words() + "; the level's alien ways " + (AlienWaysSwitchedOn ? "on" : "off") + (stock ? "; STOCK" : "; NOT STOCK: the run stops itself"));

		if (!stock)
		{
			*howItEnded = "not stock: a lane probe runs only with every AvPx change that touches aliens off";
			*exitCode = kHudsonExitBadArguments;
			Finish(*howItEnded);
			return true;
		}

		// THE SUPPLY's line, second: how many aliens, where, how often, and that the level's own are kept still; a place in no room stops the run
		if (supplyCount > 0 && supplyAround)
		{
			Write("SUPPLY up to " + std::to_string(supplyCount) + " aliens alive made by the probe around him, one every " + std::to_string((int)(supplyEvery / 65536)) +
				" s of game time while fewer hold a slot (alive and within 40 m), each at the middle of a room he can't see, 15 to 25 m from him, on his level (2 m), by the places he" +
				" walks to next (6 m), the one nearest 20 m (hudson_supply.h; HudsonNav_MiddleOfRoom, ModuleCurrVisArray; CreateAlienBot, the console's own), at most " +
				std::to_string(HudsonSupply_MostMade(supplyCount)) + " made; the level's own NPCs, generators and hive kept still");
		}
		else if (supplyCount > 0)
		{
			MODULE *const supplyRoom = ModuleFromPosition(&supplyAt, (MODULE *)0);

			Write("SUPPLY " + std::to_string(supplyCount) + " aliens made by the probe at " + PlaceWords(supplyAt) + " in " + ModuleWords(supplyRoom) + ", one every " +
				std::to_string((int)(supplyEvery / 65536)) + " s of game time from the watch's start (CreateAlienBot, the console's own); the level's own NPCs, generators and hive kept still");

			if (!supplyRoom)
			{
				*howItEnded = "supplyat= isn't in any of this level's rooms";
				*exitCode = kHudsonExitBadArguments;
				Finish(*howItEnded);
				return true;
			}
		}
		else
		{
			Write("SUPPLY the level's own (no supply=): its NPCs, generators and hive as they are, so two runs can meet different numbers of aliens");
		}

		// he plays (probe v3): from wherever the level's own start put him, so he is neither put anywhere nor kept there
		if (hePlays)
		{
			standAt = me->DynPtr->Position;
		}

		MODULE *const room = hePlays ? me->containingModule : ModuleFromPosition(&standAt, (MODULE *)0);

		if (!room)
		{
			*howItEnded = "standat= isn't in any of this level's rooms";
			*exitCode = kHudsonExitBadArguments;
			Finish(*howItEnded);
			return true;
		}

		// moved as a fight run moves him (hudson.cpp's MoveThePlayerTo: his world place, his rooms and the view's place as well); the place alone left him to be
		// put back at the level's start (the first run, 11-19 20:39: pmod=start#156 on every frame)
		if (!hePlays)
		{
			VECTORCH place = standAt;

			place.vy -= kPlayerStartsAboveTheFloor;   // up is smaller
			Hudson_MoveThePlayerTo(place);
		}

		placedIn = room;

		Write(hePlays ? "LANE PROBE v3, lanes=watch play=yes (hudson_lanes.cpp, Frost 2026-11-20): Hudson plays the level from its own start, on foot, by his grid's ways to each place of his list (hudson_play.cpp, the HudsonChains folder's <level>.play), holding his fire and unable to die; the level live, its own NPCs, generators and hive; every near alien a line a frame (A lines), a far one a line when its AI module changes (F lines); the watch ends when his list is done, or when he makes no headway (1 m) for 15 s"
			: watching ? "LANE PROBE, lanes=watch (hudson_lanes.cpp, Frost 2026-11-20): the level as he plays it, its own NPCs, generators and hive live, no alien made; the player put at standat=, standing still and unable to die; every near alien a line a frame (A lines), a far one a line when its AI module changes (F lines), for three minutes of game time"
			: "LANE PROBE (hudson_lanes.cpp, Frost 2026-11-19): one standard alien made at alienat=, the player put at standat=, standing still and unable to die; the level's own NPCs, generators and hive kept still");
		Write("AS 1999: stock=yes, every AvPx change that touches aliens off before the level (stock_flags.h, the MODE line: FLOORISLAVA, NEWDYNAMICCOLLISIONS, pfarlocs.cpp's fourth-corner height #161, pheromone.cpp's door test, near_wake.cpp); the alien's code unchanged");
		Write("ROWS: AvP-docs\\engine\\inteng\\reassess\\alien-drop-audit.md. FIELDS: f frame, t game seconds since the alien was made, near (its module drawn), mod its module, ai its AI module, wps boxes in its AI module, state, dist to the player, pos, ceil its height below its module's top, on its surface (by its down), std UseStandardGravity, down its GravityDirection, nocontact TimeNotInContactWithFloor (16384 while it touches something, counting down without; at 0 its down becomes straight down), crouch IAmCrouched, enc AlienIsEncouragedToCrawl, in the boxes whose contains_npc the last sweep set, corners of its 8 in the route's start box, tgt the boxes holding its target, route start>second..last, link the first link (owner box.slot->target flags), pmod the player's module");
		Write("ALIEN LINES (Frost 2026-11-20, Bishop's order, Vasquez's fields), A a<n>: a line a frame for each alien whose near state ran, from its own code at the moment it read it. f frame, t game seconds; ran the state that ran, now its state after; mod its module, ai its AI module, wps boxes there; pmod 1 when its MODULE is the player's (the rip-off's test), pai its AI module; pos; on its surface by its down's y (floor over 46000, wall within 20000, ceiling under -46000, slope between); down GravityDirection; mat OrientMat mat21-23 (tilted under 63000); std UseStandardGravity; nocontact TimeNotInContactWithFloor; floor IsInContactWithFloor; contacts the collision reports/of them with an NPC; crouch IAmCrouched; crawl the AlienShouldBeCrawling test that answers, asked before its search; able AlienIsAbleToClimb; tgt its Target and the distance (<8m under 8 m); ep Wander's or Hunt's entry point and the distance; go the point its waypoint search headed for and the distance; search the dropout (none, no-modules, no-box, no-route, start=end), npcboxes NPC_Waypoints, tgtboxes Num_Target_Waypoints; in the boxes holding it, tgtin those holding the point; route start>second..last and len; heads the box its manager heads for (second or start), point its current_target_point; link its own first link (owner box.slot->target flags); lb Global_Route.first_link before its state ran and by who left it (a<n>, npc, -); enc encouraged after its search, encb before; heading the search's direction, hup its dot with up; vel LinVelocity after its state, vup; rdot its down against the heading (the rip-off fires under -60000); why the reading of its gravity (WHY lines). F a<n>: a far alien, when its AI module changes. EVENT lines a<n>: CLIMBS, ON THE CEILING, DOWN");

		for (int why = 0; why < WhyCount; why++)
		{
			Write(std::string("WHY ") + kWhy[why].code + " (gravity " + (kWhy[why].gravity == 1 ? "on" : kWhy[why].gravity == 0 ? "off" : "as it was") + "): " + kWhy[why].words);
		}

		Write(std::string(hePlays ? "PLAYER STARTS (the level's own start; he plays his list) at " : "PLAYER at ") + std::to_string(standAt.vx) + " " + std::to_string(standAt.vy) + " " + std::to_string(standAt.vz) + " in " + ModuleWords(room) + (watching ? std::string("; no alien made") : "; the alien at " + std::to_string(alienAt.vx) +
			" " + std::to_string(alienAt.vy) + " " + std::to_string(alienAt.vz)));
		return false;
	}

	// lanes=watch: the level as he plays it (its frame: WatchFrame)
	if (watching)
	{
		return WatchFrame(frame, me, howItEnded, exitCode);
	}

	if (!alienMade)
	{
		if (frame < kFramesBeforeTheAlien)
		{
			return false;
		}

		// he has had his second to land: in the room he was put in, or the run ends here, because every line after would measure him from somewhere else
		VECTORCH const landed = me->DynPtr->Position;

		if (me->containingModule != placedIn)
		{
			Write("PLAYER NOT KEPT at standat=: at " + std::to_string(landed.vx) + " " + std::to_string(landed.vy) + " " + std::to_string(landed.vz) + " in " + ModuleWords(me->containingModule) +
				", not " + ModuleWords(placedIn));
			*howItEnded = "the player didn't stay where standat= put him";
			*exitCode = kHudsonExitMarineGone;
			Finish(*howItEnded);
			return true;
		}

		Write("PLAYER LANDED at " + std::to_string(landed.vx) + " " + std::to_string(landed.vy) + " " + std::to_string(landed.vz) + " in " + ModuleWords(me->containingModule));

		VECTORCH spot = alienAt;

		if (!alienOnCeiling)
		{
			spot.vy -= kAlienStartsAboveTheFloor;   // up is smaller; on the ceiling, alienat= is where it hangs, just under it
		}

		if (!ModuleFromPosition(&spot, (MODULE *)0))
		{
			*howItEnded = "alienat= isn't in any of this level's rooms";
			*exitCode = kHudsonExitBadArguments;
			Finish(*howItEnded);
			return true;
		}

		CreateAlienBot(&spot, 0);

		STRATEGYBLOCK *const made = TheAlienMadeAt(spot);

		if (!made)
		{
			*howItEnded = "the alien couldn't be made";
			*exitCode = kHudsonExitNoEnemy;
			Finish(*howItEnded);
			return true;
		}

		// alienon=ceiling: a start no stock level gives, for a run of the lane alone (Drake, 11-19 ~21:25: the climb is tested apart). It hangs as one that has
		// climbed does: its own gravity (UseStandardGravity 0) pointing up, its contact counter as a contact sets it (dynamics.cpp ApplyGravity), so it has its
		// quarter second to meet the ceiling, and turned half a turn, upside down. Everything after this frame is the game's own.
		if (alienOnCeiling && made->DynPtr)
		{
			DYNAMICSBLOCK *const d = made->DynPtr;

			d->UseStandardGravity = 0;
			d->GravityDirection.vx = 0;
			d->GravityDirection.vy = -ONE_FIXED;
			d->GravityDirection.vz = 0;
			d->TimeNotInContactWithFloor = kGravityKicksIn;
			d->OrientEuler.EulerX = 0;
			d->OrientEuler.EulerY = 0;
			d->OrientEuler.EulerZ = 2048;   // 4096 a turn
			CreateEulerMatrix(&d->OrientEuler, &d->OrientMat);
			TransposeMatrixCH(&d->OrientMat);
			d->PrevOrientEuler = d->OrientEuler;
			d->PrevOrientMat = d->OrientMat;
		}

		memcpy(alienName, made->SBname, SB_NAME_LENGTH);
		alienMade = true;
		madeAtFrame = frame;
		madeSpot = spot;   // where it was made: a void run is one where it never gets 1 m from here
		Event(frame, std::string(alienOnCeiling ? "MADE ON THE CEILING at " : "MADE at ") + std::to_string(spot.vx) + " " + std::to_string(spot.vy) + " " + std::to_string(spot.vz) + " in " + ModuleWords(made->containingModule));
		return false;
	}

	STRATEGYBLOCK *const alien = TheAlien();

	if (!alien || !alien->DynPtr || !alien->SBdataptr)
	{
		*howItEnded = "the alien is gone";
		*exitCode = kHudsonExitFinished;
		Finish(*howItEnded);
		return true;
	}

	gameTime += NormalFrameTime;

	ALIEN_STATUS_BLOCK const *const status = (ALIEN_STATUS_BLOCK const *)alien->SBdataptr;
	DYNAMICSBLOCK const *const dynamics = alien->DynPtr;
	MODULE const *const module = alien->containingModule;
	AIMODULE const *const aiModule = module ? module->m_aimodule : nullptr;
	WAYPOINT_HEADER const *const boxes = aiModule ? aiModule->m_waypoints : nullptr;
	AIMODULE const *const playersAiModule = playerPherModule ? playerPherModule->m_aimodule : nullptr;
	bool const isNear = alien->SBdptr != nullptr;   // not "near": windows.h has a macro of that name
	VECTORCH const at = dynamics->Position;
	VECTORCH const him = me->DynPtr->Position;
	double const dx = (double)at.vx - him.vx, dy = (double)at.vy - him.vy, dz = (double)at.vz - him.vz;
	int const distance = (int)sqrt(dx * dx + dy * dy + dz * dz);
	int const surface = dynamics->UseStandardGravity ? 0 : dynamics->GravityDirection.vy > kCos45 ? 0 : dynamics->GravityDirection.vy < -kCos45 ? 2 : 1;
	static char const *const kSurfaces[3] = { "floor", "wall", "ceiling" };
	bool const nowClimbing = !dynamics->UseStandardGravity && surface != 0;

	// what the waypoint code left, for its AI module
	std::string in, target, route = "-", link = "none", corners = "-";
	int holdingIt = 0, holdingTarget = 0, firstTarget = -1;

	for (int b = 0; boxes && b < boxes->num_waypoints; b++)
	{
		WAYPOINT_VOLUME const *const box = boxes->first_waypoint + b;

		if (box->contains_npc)
		{
			in += (holdingIt++ ? "," : "") + std::to_string(b);
		}

		if (box->contains_target)
		{
			target += (holdingTarget++ ? "," : "") + std::to_string(b);
			firstTarget = firstTarget < 0 ? b : firstTarget;
		}
	}

	int const start = BoxNumber(boxes, Global_Route.start), second = BoxNumber(boxes, Global_Route.second), last = BoxNumber(boxes, Global_Route.last);
	bool const routeNow = start >= 0;
	int linkOwner = -1;
	bool const linkFound = Global_Route.first_link && LinkPlace(boxes, Global_Route.first_link, &linkOwner);
	bool const linkAlienOnly = linkFound && (Global_Route.first_link->link_flags & linkflag_alienonly) != 0;
	bool const encouraged = AlienIsEncouragedToCrawl() != 0;   // the game's own question, as Approach asks it

	if (routeNow)
	{
		route = std::to_string(start) + ">" + (second >= 0 ? std::to_string(second) : std::string("-")) + ".." + (last >= 0 ? std::to_string(last) : std::string("-"));

		if (isNear)
		{
			corners = std::to_string(CornersIn(alien, Global_Route.start, aiModule->m_world));
		}
	}

	if (Global_Route.first_link)
	{
		if (linkFound)
		{
			WAYPOINT_LINK const *const l = Global_Route.first_link;
			char flags[16];

			_snprintf(flags, sizeof(flags) - 1, "0x%x", (unsigned)l->link_flags);
			flags[sizeof(flags) - 1] = 0;
			link = std::to_string(linkOwner) + "." + std::to_string((int)(l - boxes->first_waypoint[linkOwner].first_link)) + "->" + std::to_string(l->link_target_index) + " " + flags;
		}
		else
		{
			link = "elsewhere";   // left from another module, or an earlier frame (D6)
		}
	}

	int const ceiling = module ? at.vy - (module->m_world.vy + module->m_miny) : 0;
	char line[1024];

	_snprintf(line, sizeof(line) - 1, "f=%ld t=%.3f near=%d mod=%s ai=%d wps=%d state=%s dist=%d pos=%d,%d,%d ceil=%d on=%s std=%d down=%d,%d,%d nocontact=%d crouch=%d enc=%d in=%s corners=%s tgt=%s route=%s link=%s pmod=%s",
		frame, (double)gameTime / 65536.0, isNear ? 1 : 0, ModuleWords(module).c_str(), aiModule ? aiModule->m_index : -1, boxes ? boxes->num_waypoints : 0, StateName(status->BehaviourState), distance,
		at.vx, at.vy, at.vz, ceiling, kSurfaces[surface], dynamics->UseStandardGravity ? 1 : 0, dynamics->GravityDirection.vx, dynamics->GravityDirection.vy, dynamics->GravityDirection.vz,
		dynamics->TimeNotInContactWithFloor, status->IAmCrouched ? 1 : 0, encouraged ? 1 : 0, in.empty() ? "-" : in.c_str(), corners.c_str(), target.empty() ? "-" : target.c_str(), route.c_str(),
		link.c_str(), ModuleWords(playerPherModule).c_str());
	line[sizeof(line) - 1] = 0;
	Write(line);

	std::string const where = ModuleWords(module) + " box " + (start >= 0 ? std::to_string(start) : in.empty() ? std::string("none") : in);
	bool const sameAiModule = aiModule && aiModule == playersAiModule;

	// events
	if (isNear != wasNear && frame > madeAtFrame + 1)
	{
		Event(frame, isNear ? "NEAR in " + ModuleWords(module) : "FAR in " + ModuleWords(module));
	}

	if (nowClimbing && !everClimbed)
	{
		everClimbed = true;
		climbedAt = frame;
		Keep(climbedWhere, sizeof(climbedWhere), where);
		Event(frame, "CLIMBS in " + where + " (" + StateName(status->BehaviourState) + ", " + kSurfaces[surface] + ")");
	}

	if (nowClimbing && surface == 2 && !everOnTheCeiling)
	{
		everOnTheCeiling = true;
		ceilingAt = frame;
		Keep(ceilingWhere, sizeof(ceilingWhere), where);
		Event(frame, "ON THE CEILING in " + where);
	}

	int const aiNumber = aiModule ? aiModule->m_index : -1;

	if (aiNumber != lastAiModule && lastAiModule >= 0)
	{
		if (climbing && nowClimbing)
		{
			doorwaysUp++;
		}

		Event(frame, "DOORWAY into " + ModuleWords(module) + " (AI module " + std::to_string(aiNumber) + ") " + (climbing && nowClimbing ? std::string("UP on the ") + kSurfaces[surface] : std::string("on the ") +
			kSurfaces[surface]));
	}

	if (sameAiModule && !inPlayersModule)
	{
		inPlayersModule = true;
		inPlayersModuleAt = frame;
		upWhenItCameIn = nowClimbing;
		Event(frame, std::string("IN THE PLAYER'S MODULE ") + (nowClimbing ? "up" : "on the floor") + ", " + std::to_string(distance) + " mm from him");
	}

	// it was climbing and now isn't (gravity back on, or its down the floor's), or it went far while climbing
	if ((climbing && !nowClimbing) || (climbing && wasNear && !isNear))
	{
		Seen s;

		s.isNear = isNear;
		s.state = status->BehaviourState;
		s.distance = distance;
		s.sameAiModuleAsThePlayer = sameAiModule;
		s.moduleHasBoxes = boxes && boxes->num_waypoints > 0;
		s.boxesHoldingIt = holdingIt;
		s.boxesHoldingTheTarget = holdingTarget;
		s.route = routeNow;
		s.routeIsOneBox = routeNow && start == last;
		s.linkFound = linkFound;
		s.linkAlienOnly = linkAlienOnly;
		s.linkNone = Global_Route.first_link == nullptr;
		s.encouraged = encouraged;
		s.standardGravity = dynamics->UseStandardGravity != 0;
		s.noContact = dynamics->TimeNotInContactWithFloor;
		s.crouched = status->IAmCrouched != 0;
		s.where = where;

		std::string const why = WhyItCameDown(s);

		Event(frame, "DROP in " + where + ": " + why);

		if (inPlayersModule)
		{
			if (downInPlayersModule[0] == 0)
			{
				Keep(downInPlayersModule, sizeof(downInPlayersModule), "came down at f=" + std::to_string(frame) + " " + std::to_string(distance) + " mm from him: " + why);
			}
		}
		else if (!dropped)
		{
			dropped = true;
			droppedAt = frame;
			Keep(dropWhere, sizeof(dropWhere), where);
			Keep(dropReason, sizeof(dropReason), why);
		}
	}

	if (status->BehaviourState == ABS_Attack && !attacked)
	{
		attacked = true;
		attackedAt = frame;
		Event(frame, "ATTACK " + std::to_string(distance) + " mm from him, " + (nowClimbing ? std::string("still up") : std::string("on the floor")));
	}

	// the stutter count (see where it is kept)
	if (isNear && boxes && boxes->num_waypoints > 0 && routeNow)
	{
		if (aiNumber == stutterAi)
		{
			if (start != stutterStart)
			{
				stutterBoxSwitches++;
				stutterBoxReturns += start == stutterStartBefore ? 1 : 0;
				stutterThisSecond++;
				stutterStartBefore = stutterStart;
			}

			if (firstTarget != stutterTarget)
			{
				stutterTargetSwitches++;
				stutterThisSecond++;
			}
		}
		else
		{
			stutterStartBefore = -1;
		}

		stutterAi = aiNumber;
		stutterStart = start;
		stutterTarget = firstTarget;
		stutterFrames++;
		stutterTime += NormalFrameTime;

		if (stutterTime - stutterSecondFrom >= 65536)
		{
			stutterWorstSecond = stutterThisSecond > stutterWorstSecond ? stutterThisSecond : stutterWorstSecond;
			stutterThisSecond = 0;
			stutterSecondFrom = stutterTime;
		}
	}
	else
	{
		stutterAi = -1;
	}

	climbing = nowClimbing;
	wasNear = isNear;
	lastAiModule = aiNumber;

	// every frame, so that a run stopped from outside loses one line at most (the first run, 11-19 20:39, ended at frame 6439 with no word and 60 lines unwritten)
	if (file)
	{
		fflush(file);
	}

	// THE RUN ENDS ON AN EVENT (Bishop 11-20, the owner watching: "most of them is marine sitting for 3 minutes doing nothing"): a void run (the alien hasn't moved 1 m in
	// its first 5 s) costs his screen 5 s; no headway toward him (its distance not 1 m shorter) for 15 s ends it; 60 s at most; and the attack, 2 s after the first blow
	if (!movedYet && Apart(at, madeSpot) >= kRunMoved)
	{
		movedYet = true;
	}

	if (closest < 0 || distance < closest - kRunMoved)
	{
		closest = distance;
		closestAt = gameTime;
	}

	char const *const stop = !movedYet && gameTime >= kRunMovedWithin ? "VOID: the alien didn't move 1 m in its first 5 s" :
		gameTime - closestAt >= kRunNoHeadway ? "no headway toward him (1 m) for 15 s" : gameTime >= kRunMost ? "60 s of game time" : nullptr;

	if (stop)
	{
		*howItEnded = stop;
		*exitCode = kHudsonExitFinished;
		Finish(*howItEnded);
		return true;
	}

	if ((attacked && frame >= attackedAt + kFramesAfterTheFirstBlow) || frame >= mostFrames)
	{
		*howItEnded = attacked ? "the alien reached him and attacked" : "frames= ran out";
		*exitCode = kHudsonExitFinished;
		Finish(*howItEnded);
		return true;
	}

	return false;
}
