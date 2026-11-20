// hudson_lanes.cpp: the lane probe. hudson_lanes.h says what it is for and how it is asked for. Written 2026-11-19 by Melanikus (Frost)
// for the AvP port.
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

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <string>

extern void CreateAlienBot(VECTORCH *Position, int type);   // bh_alien.cpp: the console's own way of making one
extern int NormalFrameTime;

// The waypoint code's own globals (bh_waypoint.cpp), only read here
extern WAYPOINT_ROUTE Global_Route;
extern int NPC_Waypoints;
extern int Num_Target_Waypoints;

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

		Write(verdict);
		Write(case2);
		Write(std::string("END ") + why);
		LogString(("lanes: " + verdict).c_str());
		LogString(("lanes: " + case2).c_str());

		if (file)
		{
			fclose(file);
			file = nullptr;
		}
	}
}

void HudsonLanes_Begin(int alienAtX, int alienAtY, int alienAtZ, int standAtX, int standAtY, int standAtZ, int framesMost, bool onCeiling)
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
}

bool HudsonLanes_IsTheAlien(const STRATEGYBLOCK *block)
{
	if (!HudsonRunIsOn || !asked || !alienMade || !block)
	{
		return false;
	}

	return block->I_SBtype == I_BehaviourAlien && memcmp(block->SBname, alienName, SB_NAME_LENGTH) == 0;
}

bool HudsonLanes_EndOfFrame(long frame, const char **howItEnded, int *exitCode)
{
	if (!HudsonRunIsOn || !asked)
	{
		return false;
	}

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

		MODULE *const room = ModuleFromPosition(&standAt, (MODULE *)0);

		if (!room)
		{
			*howItEnded = "standat= isn't in any of this level's rooms";
			*exitCode = kHudsonExitBadArguments;
			Finish(*howItEnded);
			return true;
		}

		// moved as a fight run moves him (hudson.cpp's MoveThePlayerTo: his world place, his rooms and the view's place as well); the place alone left him to be
		// put back at the level's start (the first run, 11-19 20:39: pmod=start#156 on every frame)
		VECTORCH place = standAt;

		place.vy -= kPlayerStartsAboveTheFloor;   // up is smaller
		Hudson_MoveThePlayerTo(place);
		placedIn = room;

		Write("LANE PROBE (hudson_lanes.cpp, Frost 2026-11-19): one standard alien made at alienat=, the player put at standat=, standing still and unable to die; the level's own NPCs, generators and hive kept still");
		Write("AS 1999: FLOORISLAVA OFF (the alien ways off), NEWDYNAMICCOLLISIONS OFF; the alien's code unchanged. Still AvPx's, not 1999's: pfarlocs.cpp's fourth-corner height fix (#161, where a far alien lands), pheromone.cpp's door test (scent through a door that isn't its AI module's first module), near_wake.cpp (placed sleeping aliens, not this one)");
		Write("ROWS: AvP-docs\\engine\\inteng\\reassess\\alien-drop-audit.md. FIELDS: f frame, t game seconds since the alien was made, near (its module drawn), mod its module, ai its AI module, wps boxes in its AI module, state, dist to the player, pos, ceil its height below its module's top, on its surface (by its down), std UseStandardGravity, down its GravityDirection, nocontact TimeNotInContactWithFloor (16384 while it touches something, counting down without; at 0 its down becomes straight down), crouch IAmCrouched, enc AlienIsEncouragedToCrawl, in the boxes whose contains_npc the last sweep set, corners of its 8 in the route's start box, tgt the boxes holding its target, route start>second..last, link the first link (owner box.slot->target flags), pmod the player's module");
		Write("PLAYER at " + std::to_string(standAt.vx) + " " + std::to_string(standAt.vy) + " " + std::to_string(standAt.vz) + " in " + ModuleWords(room) + "; the alien at " + std::to_string(alienAt.vx) +
			" " + std::to_string(alienAt.vy) + " " + std::to_string(alienAt.vz));
		return false;
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
	int holdingIt = 0, holdingTarget = 0;

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

	climbing = nowClimbing;
	wasNear = isNear;
	lastAiModule = aiNumber;

	// every frame, so that a run stopped from outside loses one line at most (the first run, 11-19 20:39, ended at frame 6439 with no word and 60 lines unwritten)
	if (file)
	{
		fflush(file);
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
