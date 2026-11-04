// Could a shadow land on anything the camera can see? Written 2026-11-04 by Melanikus for the AvP port.
// See shadow_reach.cpp for how it works. This file knows nothing about the game, so a test can check it on its own.

#ifndef SHADOW_REACH_H
#define SHADOW_REACH_H

// A ball: where its middle is, and how big it is.
struct ShadowReachBall
{
	float x, y, z, radius;
};

// Flat planes with everything the camera can see on their inside. Each is four numbers: an arrow one unit long that points inwards, and then the
// number that makes (arrow times place) plus that number come to how far a place is inside the plane. Below zero means it's outside.
struct ShadowReachView
{
	enum { kMostPlanes = 11 };

	float planes[kMostPlanes][4];
	int count;
};

// Starts a view with no planes in it, which nothing is outside of.
void ShadowReach_StartView(ShadowReachView *view);

// Adds the camera to the view, with the camera at 0 0 0 and looking along z. scaleAcross and scaleUp are the projection matrix's _11 and _22.
void ShadowReach_AddCameraView(ShadowReachView *view, float scaleAcross, float scaleUp);

// Adds a box with sides along x, y and z. smallest and largest are its two opposite corners, x y z each.
void ShadowReach_AddBox(ShadowReachView *view, const float *smallest, const float *largest);

// Works out a ball that holds the whole of a box. ball gets four numbers: x y z and the radius.
void ShadowReach_BallRoundBox(const float *smallest, const float *largest, float *ball);

// Could something inside the ball shadow anything inside the view, from this light? light is the first four numbers of a light on the list: x y z
// and its range. Everything has to be in the same numbers as the view. A yes can be wrong, which only costs a little time. A no never is.
bool ShadowReach_CanReach(const ShadowReachView &view, const ShadowReachBall &ball, const float *light);

#endif
