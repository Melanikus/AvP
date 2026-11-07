// How walking and crouching slow the player down. Written 2026-11-07 by Melanikus for the AvP port. Found through Bobblen's
// avp-forever (commit 2b70546); written fresh, not taken from his code.
//
// Walking (not holding the run key, or not able to run) and crouching move at half speed, forwards, backwards and to the
// sides. The TurnSpeedAdjust setting in [Gameplay] (game.cpp) only says whether turning slows down too: true, the default,
// halves it as well; false keeps turning at full speed. Before this, turning it off also stopped walking and crouching from
// slowing anyone down at all (pmove.cpp took the full-speed path whenever it was off), so nobody could walk.
//
// pmove.cpp's ExecuteFreeMovement calls this, and tests/walk_speed_test.cpp tries it with the setting both ways.

#pragma once

inline void WalkSpeeds_Halve(int &forwardSpeed, int &strafeSpeed, int &turnSpeed, bool turningSlowsToo)
{
	forwardSpeed = forwardSpeed / 2;
	strafeSpeed = strafeSpeed / 2;

	if (turningSlowsToo)
	{
		turnSpeed = turnSpeed / 2;
	}
}
