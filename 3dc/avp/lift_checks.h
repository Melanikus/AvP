// The checks on what a network message says about a platform lift. Written 2026-11-07 by Melanikus for the AvP port.
//
// A message from another machine is hostile input. These checks hold nothing but plain numbers and need no other part of the game,
// so tests/lift_checks_test.cpp can try them with good and bad values. That test also reads pldnet.cpp, to check it still asks.

#ifndef LIFT_CHECKS_H
#define LIFT_CHECKS_H

// Is this one of a platform lift's states? They're numbered from 0, and howMany is how many there are: PLBS_GoingDown + 1, the last
// in PLATFORMLIFT_STATES in bh_platform_lift.h. The message carries the state as a plain byte, and a car given any other number
// would be in a state its own code doesn't know, so it would never move again.
inline bool Lift_StateIsKnown(int state, int howMany)
{
	return state >= 0 && state < howMany;
}

#endif
