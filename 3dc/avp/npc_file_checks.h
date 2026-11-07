// The checks on numbers an NPC is given by a save file. Written 2026-11-07 by Melanikus for the AvP port.
//
// A save file is hostile input: anyone can hand someone a damaged one. These checks hold nothing but plain numbers, and need no
// other part of the game, so tests/npc_crashes_test.cpp can try them with good and bad numbers. That test also reads the
// loaders that use them (LoadStrategy_Marine, LoadStrategy_Predator) to check they still ask, before anything is changed.

#ifndef NPC_FILE_CHECKS_H
#define NPC_FILE_CHECKS_H

// Is this number one of a kind's weapons? Each kind's weapons are numbered from 0 up to just before an end mark, such as MNPCW_End
// for a marine. A number outside that isn't only missing from the list: C++ doesn't allow turning it into the weapons' own kind of
// number at all, so it has to be refused before it's looked up.
inline bool NpcFile_WeaponNumberIsOnList(int number, int endOfList)
{
	return number >= 0 && number < endOfList;
}

#endif
