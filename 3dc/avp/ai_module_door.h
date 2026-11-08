// Which of an AI module's modules makes it a door, and whether the aliens' scent goes through it. Written 2026-11-08 by Melanikus for the
// AvP port.
//
// WHY: aliens find their way by a scent (the pheromones) that spreads from one AI module to the next, unless a door between them is shut.
// An AI module is a list of the level's own modules, made from the level file (projload.cpp), and it counts as a door if any module on
// the list is one: AIModuleIsADoor in pfarlocs.cpp looks through the whole list. But whether that door was shut was then asked of the
// list's first module, whatever that was, and the first module's block was read as a door's without a look at what it was, or whether
// it had one. Now the door is found once, by AIModuleDoor_Find, and only the module that is the door is asked about it.
//
// The kinds, and how a module answers, come from the game (pfarlocs.cpp and pheromone.cpp), so this knows nothing of it, and
// tests/ai_module_door_test.cpp can try it with stand-ins for the game's modules.

#ifndef AI_MODULE_DOOR_H
#define AI_MODULE_DOOR_H

// The kinds of door, in the order of the game's MODULEDOORTYPE (pfarlocs.h). pfarlocs.cpp checks the two agree.
enum AIModuleDoorKind
{
	kAIModuleNotADoor,
	kAIModuleProxDoor,      // opens when something comes near, and can be locked
	kAIModuleLiftDoor,
	kAIModuleSecurityDoor   // a switch door
};

// The first module on the list that is a door, or nothing, and in kind what kind of door it is. kindOf(module) gives a module's kind.
// The list ends with an empty entry, as the level loader makes it.
template <typename Module, typename KindOf>
static inline Module *AIModuleDoor_Find(Module *const *list, KindOf kindOf, int *kind)
{
	*kind = kAIModuleNotADoor;

	if (!list)
	{
		return 0;
	}

	for (Module *const *at = list; *at; at++)
	{
		int const found = kindOf(*at);

		if (found != kAIModuleNotADoor)
		{
			*kind = found;
			return *at;
		}
	}

	return 0;
}

// Whether the scent goes through an AI module. Not a door: it does. A proximity door: unless it's locked. A lift door or a security door:
// only while it's open. Each is asked of the door that AIModuleDoor_Find finds, and of no other module.
// lockedOf(door) gives 1 for locked, 0 for not, and -1 for a door it can't read: then the door is taken as unlocked, as if it weren't
// there. openOf(door) gives whether a door is open.
template <typename Module, typename KindOf, typename LockedOf, typename OpenOf>
static inline bool AIModuleDoor_LetsScentThrough(Module *const *list, KindOf kindOf, LockedOf lockedOf, OpenOf openOf)
{
	int kind = kAIModuleNotADoor;
	Module *const door = AIModuleDoor_Find(list, kindOf, &kind);

	switch (kind)
	{
		case kAIModuleProxDoor:
			return lockedOf(door) != 1;

		case kAIModuleLiftDoor:
		case kAIModuleSecurityDoor:
			return openOf(door);

		default:
			return true;
	}
}

#endif
