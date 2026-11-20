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
// LANES=WATCH (added 2026-11-20 by Melanikus (Frost), Bishop's order after the owner's Steam play: the aliens ran at the walls and never reached a ceiling):
// lanes=watch standat=x,y,z (run-hudson.ps1 -LanesWatch -StandAt) is the level as he plays it: nothing is kept still and no alien is made; the player is put at
// standat= and can't die, as above. It ends after three minutes of game time, or at frames=.
//
// EVERY ALIEN'S LINE (both kinds of probe, 2026-11-20): the near alien's own code tells the probe what it read at the moment it read it, through the three calls
// below, because Global_Route is one for every NPC (at the end of a frame it is the route of whichever NPC searched last). hudson-lanes.txt then has an A line a
// frame for each near alien (its state, gravity, target, route, links, and why its gravity is as it is by Rebellion's lines), an F line when a far one's AI module
// changes, and at the end an ALIEN line for each (did it ever reach a ceiling, and if not the reason that kept gravity on most) and BY STATE totals.
//
// WHAT IT WRITES: hudson-lanes.txt in the save folder, a line a frame (f=, t=, near=, mod=, ai=, wps=, state=, dist=, pos=, ceil=, on=,
// std=, down=, nocontact=, crouch=, enc=, in=, corners=, tgt=, route=, link=, pmod=), a line for each event (CLIMBS, ON THE CEILING,
// DOORWAY, DROP, FAR, NEAR, IN THE PLAYER'S MODULE, ATTACK), and at the end the VERDICT: STAYED UP, DROPPED (the frame, the module, the
// box and the reason, by its row in AvP-docs\engine\inteng\reassess\alien-drop-audit.md), or NEVER WENT UP.

#pragma once

struct strategyblock;
struct vectorch;

// Before the level: what the plan asked for. Only stores it.
// onCeiling (alienon=ceiling): the alien is made hanging from the ceiling at alienat=, its own gravity up, for a run of the lane alone
// watch (lanes=watch): the level as he plays it, no alien made
void HudsonLanes_Begin(int alienAtX, int alienAtY, int alienAtZ, int standAtX, int standAtY, int standAtZ, int mostFrames, bool onCeiling, bool watch);

// lanes=watch with play=yes (probe v3, added 2026-11-20 by Melanikus (Frost)): he plays the level from its own start (hudson_play.h), so he is neither put anywhere nor
// kept anywhere; the watch ends when his list is done, or when he makes no headway (1 m) for 15 s
void HudsonLanes_HePlays();

// supply= (added 2026-11-20 by Melanikus (Frost), Bishop: a fixed alien supply, so two plots compare fairly in one run each): with lanes=watch play=yes, the probe makes
// count aliens at the place, one every everySeconds of game time from the watch's start, the console's own way; the level's own NPCs, generators and hive are kept still
void HudsonLanes_Supply(int count, int atX, int atY, int atZ, int everySeconds);

// Whether this is the probe's alien, the one thing Hudson_KeepsStill lets move
bool HudsonLanes_IsTheAlien(const struct strategyblock *block);

// Each frame, from Hudson_EndOfFrame. True when the probe is over: *howItEnded and *exitCode are what to end the level with.
bool HudsonLanes_EndOfFrame(long frame, const char **howItEnded, int *exitCode);

// From the alien's own code, only while a lane probe runs (they return at once otherwise), and they only read: before and after a near alien's state runs
// (bh_near.cpp NearAlienBehaviour), and after each NPC's waypoint search (bh_ais.cpp NPCGetMovementDirection: the point it headed for, the direction it found, and
// whether it steered)
void HudsonLanes_AlienBefore(const struct strategyblock *alien);
void HudsonLanes_AlienAfter(const struct strategyblock *alien);
void HudsonLanes_Searched(const struct strategyblock *npc, const struct vectorch *target, const struct vectorch *heading, int found);
