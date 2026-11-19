// hudson_lanes.h: the lane probe, a Hudson run that watches one alien come for a player who stands still, frame by frame, to see whether
// it keeps to the ceiling across modules. Written 2026-11-19 by Melanikus (Frost) for the AvP port (the owner's ceiling lanes: "we need to
// test the concept of having an alien finishing a module on the ceiling and the next module has a ceiling box connecting to it so it
// never chooses to drop down").
//
// ASKED FOR with -dev -hudson level=<name> frames=<n> lanes=yes alienat=x,y,z standat=x,y,z (run-hudson.ps1 -Lanes -AlienAt -StandAt).
// The player is put at standat= and can't die; he has no hands on the keys, so he never moves or fires. One standard alien is made at
// alienat= (CreateAlienBot, as the console's own cheat makes one: its waypoints on, as a generator's are). Everything else the level has
// that moves (its NPCs, its generators, the hive) is kept still, so the alien is the only thing using the waypoint code.
//
// AS 1999's GAME. lanes=yes turns the alien ways off (floorislava=off) and NEWDYNAMICCOLLISIONS off, and changes nothing in the alien's
// code: everything here only READS what the game's own code left (the alien's state and gravity, the waypoint code's Global_Route and
// each box's contains_npc / contains_target flags from its last sweep, bh_waypoint.cpp). What AvPx still does differently from
// Rebellion's 1999 code with those two off (Frost's audit of 2026-11-19, against 3364ee94): pfarlocs.cpp's fourth-corner height fix (#161)
// moves the far aux locations a far alien lands on; pheromone.cpp's door test differs when the door is not its AI module's first module;
// near_wake.cpp wakes placed sleeping aliens near the player (not this run's alien). The header of hudson-lanes.txt says so too.
//
// WHAT IT WRITES: hudson-lanes.txt in the save folder, a line a frame (f=, t=, near=, mod=, ai=, wps=, state=, dist=, pos=, ceil=, on=,
// std=, down=, nocontact=, crouch=, enc=, in=, corners=, tgt=, route=, link=, pmod=), a line for each event (CLIMBS, ON THE CEILING,
// DOORWAY, DROP, FAR, NEAR, IN THE PLAYER'S MODULE, ATTACK), and at the end the VERDICT: STAYED UP, DROPPED (the frame, the module, the
// box and the reason, by its row in AvP-docs\engine\inteng\reassess\alien-drop-audit.md), or NEVER WENT UP.

#pragma once

struct strategyblock;

// Before the level: what the plan asked for. Only stores it.
void HudsonLanes_Begin(int alienAtX, int alienAtY, int alienAtZ, int standAtX, int standAtY, int standAtZ, int mostFrames);

// Whether this is the probe's alien, the one thing Hudson_KeepsStill lets move
bool HudsonLanes_IsTheAlien(const struct strategyblock *block);

// Each frame, from Hudson_EndOfFrame. True when the probe is over: *howItEnded and *exitCode are what to end the level with.
bool HudsonLanes_EndOfFrame(long frame, const char **howItEnded, int *exitCode);
