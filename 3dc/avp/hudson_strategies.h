// Modified 2026-11-06 by Melanikus for the AvP port:
//   HudsonStrategies_SayTheCounts
// What Hudson can find out about a level's strategies. Written 2026-11-05 by Melanikus for the AvP port.
//
// A strategy is the game's word for a thing in a level that does something: a door, a switch, a lift, a mission step.
// hudson.cpp decides when these run. They only look at the level, and only run in a Hudson run.

#ifndef HUDSON_STRATEGIES_H
#define HUDSON_STRATEGIES_H

#include <stddef.h>
#include <string>
#include <vector>

struct vectorch;
struct strategyblock;
typedef struct vectorch VECTORCH;
typedef struct strategyblock STRATEGYBLOCK;

// Writes every strategy in the level to the log: its kind, name, place, state and what it aims at.
void HudsonStrategies_ListAll();

// Writes one line to the log: how many of each kind of strategy the level holds right now. On a machine in a network game that doesn't run the NPCs, they're ghosts, and only
// the ones near somebody exist yet, so those are counted as ghosts of that kind.
void HudsonStrategies_SayTheCounts();

// Every strategy in the game comes from one pool with a fixed size: NPCs, ghosts, corpses, debris. So does everything drawn. Called every frame, this says
// every ten seconds how close the run came to the end of each.
void HudsonStrategies_NoteTheCrowd();

// The strategy with this name, or nothing. The name comes from the command line and is only compared with the level's own.
STRATEGYBLOCK *HudsonStrategies_Find(const char *word);

// Where to stand to see this strategy, and the point to look at. False if there's nowhere to stand. For a door, that's on
// the floor a few steps back from it, in a room next to it, straight through the doorway.
bool HudsonStrategies_WhereToStand(const STRATEGYBLOCK *target, VECTORCH *place, VECTORCH *looksAt);

// What state it's in, in words, so that two moments can be compared and a person can read the log. A name that can't be
// read says so, and then nothing can be said about whether it changed.
std::string HudsonStrategies_State(const STRATEGYBLOCK *target);

// Its kind and name, safe to put in the log.
std::string HudsonStrategies_Title(const STRATEGYBLOCK *target);

// Could a player go through this right now, on this machine? Only a door or a platform lift can say no.
bool HudsonStrategies_CanGoThrough(const STRATEGYBLOCK *target);

// The door-checking walk: 3dc/avp/coop_reach.h says what it is, and what it doesn't prove. Load once per level, with the
// path of the level's room list. It says in the log if there isn't one or it's refused, and then each answer is "no room list".
bool HudsonStrategies_LoadRoomList(const char *path);

// Looks at this machine's doors and players, and adds to the rooms reached so far. Call it often enough to catch a switch
// door, which is only open for about 4 seconds: once every 2 seconds is enough. It costs about one lookup per strategy.
void HudsonStrategies_SpreadReach();

// Is there a route from where the players have been to any of these rooms, through doors open when it was last spread? The
// answer comes in the words the log and the report should use, and routeExists says which way it went. "where" is the rooms
// a step's thing is in, and "nearRooms" the rooms it's only within 2 m of. Both come from the step's lines in the chain file.
std::string HudsonStrategies_AskReach(std::vector<std::string> const &where, std::vector<std::string> const &nearRooms, bool *routeExists);

#endif
