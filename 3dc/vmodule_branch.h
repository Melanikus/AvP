// Where a door's branch in a module's visibility list may go. Written 2026-11-14 by Melanikus for the AvP port.
//
// A module's visibility list (VMODULE, module.h) is walked from its first item to its end marker. An item that "branches if the door is
// closed" makes the walk jump to a later item in the same list. That's the only place the game's own setup ever points one: it finds the
// item by its place in the same list (ConvertVModuleNameToPointer in module.cpp, and projload.cpp), and a level file's branches may only
// go forward (VModule_Array_Chunk, chunk_object.cpp).
//
// So the walk takes a branch only if it lands on an item after this one, and no further than the end marker. Anything else is not taken,
// and the walk goes on with the next item, as it does for an open door. A place left wrong can then never send the walk into memory that
// isn't the list, or round the list for ever.
//
// This is plain sums, with nothing of the game's in it, so vmodule_branch_test checks it on its own.

#ifndef VMODULE_BRANCH_H
#define VMODULE_BRANCH_H

#include <stdint.h>

// The item the walk goes to next, from item, when the door is closed: target if it's on an item after item and no further than end (the
// list's end marker), or else the item after item. The places are compared as plain numbers, so a target from anywhere at all, or none,
// can be asked about safely.
template <typename Item>
inline Item *VModuleBranchTarget(Item *item, Item *target, Item *end)
{
	uintptr_t const here = (uintptr_t)item;
	uintptr_t const there = (uintptr_t)target;
	uintptr_t const last = (uintptr_t)end;

	bool const forward = there > here && there <= last;
	bool const onAnItem = forward && (there - here) % sizeof(Item) == 0;

	return onAnItem ? target : item + 1;
}

#endif
