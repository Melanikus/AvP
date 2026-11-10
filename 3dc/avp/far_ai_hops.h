// The far AI's hops, in the log. Written 2026-11-10 by Melanikus (Third) for the AvP port (#109).
//
// WHY: an alien nobody can see is moved by the far AI, one module at a time, straight to a place in the next module: the entry point
// between the two, or one of the module's far locations (bh_far.cpp, LocateFarNPCInAIModule). In a film of Genshd1 an alien went 23 m in a
// second and came out under a floor, 0.57 m from an entry point that sits on that floor's top. Whether such a hop puts the body's middle on
// the floor, which would leave half of it under, depends on where an NPC's position is in its body, and nothing said so. These lines do.
//
// WHAT A LINE SAYS: which kind of NPC, the module it left and the one it went to, whether it was put on the entry point or a far location,
// whether the module it went to was in view, where its position was put (the game's millimetres, y going down), and how far its shape goes
// above and below that position: its shape's lowest and highest y. A position at the body's middle has those about equal and opposite; one
// at the feet has the top far above and the bottom near 0.
//
// HOW MANY: a hop can come often in a level full of aliens, so at most kFarAiHopsALevel lines are written a level, then one line saying the
// rest aren't. The caller keeps the count, and sets it to 0 as a level starts.
//
// Nothing here touches the game, so tests/far_ai_hops_test.cpp tries all of it. The module names come from the level file, which may be
// anyone's: each is cut to kFarAiHopsLongestName characters, and the whole line to the caller's buffer.

#ifndef FAR_AI_HOPS_H
#define FAR_AI_HOPS_H

#include <stddef.h>
#include <stdio.h>

enum
{
	kFarAiHopsALevel = 400,         // lines a level, before the one saying the rest aren't written
	kFarAiHopsLongestName = 32      // characters of a module's name in a line
};

// The game's side, in bh_far.cpp: a level's start counts the hops from 0 again (game.cpp calls it beside BuildFarModuleLocs)
void FarAiHops_LevelStarts(void);

struct FarAiHop
{
	const char *kind;      // "alien", "marine", "predator", or "npc" for any other kind
	const char *from;      // the module it left, or 0 when there's no name
	const char *to;        // the module it went to, or 0
	bool entryPoint;       // put on the entry point between the two, or else on one of the module's far locations
	bool inView;           // the module it went to was in view
	int x, y, z;           // where its position was put
	int shapeLowestY;      // its shape's lowest and highest y, from its position: y goes down, so the top of the body is the lowest
	int shapeHighestY;
};

// The line for one hop, or the one saying the level's limit is reached, or nothing after that. logged counts this level's lines, and goes
// up by one for each line given. Gives back whether out holds a line to write; out is always ended, even when there's none.
inline bool FarAiHops_Line(const FarAiHop &hop, int *logged, char *out, size_t size)
{
	if (!out || size == 0)
	{
		return false;
	}

	out[0] = 0;

	if (!logged || *logged > kFarAiHopsALevel)
	{
		return false;
	}

	if (*logged == kFarAiHopsALevel)
	{
		(*logged)++;
		snprintf(out, size, "far AI: %d hops are written this level, and the rest aren't", (int)kFarAiHopsALevel);
		return true;
	}

	(*logged)++;
	snprintf(out, size, "far AI: %s went from module %.*s to %.*s, %s, %s, put at %d %d %d; its shape goes from %d to %d about that point",
		hop.kind ? hop.kind : "npc",
		(int)kFarAiHopsLongestName, hop.from ? hop.from : "(no name)",
		(int)kFarAiHopsLongestName, hop.to ? hop.to : "(no name)",
		hop.entryPoint ? "onto the entry point" : "onto a far location",
		hop.inView ? "in view" : "out of view",
		hop.x, hop.y, hop.z, hop.shapeLowestY, hop.shapeHighestY);
	return true;
}

#endif
