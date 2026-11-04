// Could a shadow land on anything the camera can see? Written 2026-11-04 by Melanikus for the AvP port.
//
// WHY THIS IS HERE
//
// A wall that's out of view can still throw a shadow onto something in view, so walls out of view have to cast. But most of a level's walls are
// nowhere near what's on screen, and every one that casts is more work for the graphics card, once for each light. This says which ones can be
// left out. A wrong yes only costs a little time. A wrong no would make a shadow vanish. So each step here can only make the answer yes.
//
// HOW IT WORKS
//
// Hold a ball in front of a lamp. Its shadow is a cone, widening away from the lamp, and it stops mattering at the end of the lamp's range, because
// no light gets that far to be blocked. That shape is awkward to test, so it's swapped for a simpler one it fits inside: imagine stretching a skin
// tight round two balls, the real one and a second at the end of the range, as wide as the cone is there.
//
// Next, what can be seen. All of it lies on the inside of the planes at the sides of the screen, and inside the box of something that was drawn. If
// the stretched shape is wholly outside any one plane of the screen, no part of the shadow can touch anything seen. If it's wholly outside one
// side of a box, it can't touch that box, and if that's true of every box, it can't touch anything drawn. And a stretched skin is only wholly
// outside a plane if both of its balls are, so those two balls are all that's asked about.
//
// The boxes are what leave out the rooms behind the walls. A room like that is in line with the screen, but nothing in it was drawn. There has to be
// a box for each thing: one box round everything drawn would also hold every room lying between the things in it.

#include "shadow_reach.h"

#include <math.h>

// How far the planes through the camera stand outside the real edge of the screen: here, each side is about a tenth further out. Nothing past the
// real edge can be seen, so this is only so that a small rounding or mismatch can't cut off a shadow that's just on screen.
static const float kScreenEdgeMovedOut = 0.9f;

// Numbers this big are rounded when they're stored, so a ball is made this much bigger to be sure that it holds its whole box.
static const float kBallRoundedUp = 1.0f;

static void AddPlane(ShadowReachView *view, float x, float y, float z, float distance)
{
	if (view->count == ShadowReachView::kMostPlanes)
	{
		return;
	}

	float *plane = view->planes[view->count++];

	plane[0] = x;
	plane[1] = y;
	plane[2] = z;
	plane[3] = distance;
}

void ShadowReach_StartView(ShadowReachView *view)
{
	view->count = 0;
}

void ShadowReach_AddCameraView(ShadowReachView *view, float scaleAcross, float scaleUp)
{
	// nothing behind the camera can be seen
	AddPlane(view, 0.0f, 0.0f, 1.0f, 0.0f);

	// The projection puts a point at the screen's edge when its x times scaleAcross is as big as its z. So x times scale, plus z, is how far it's
	// inside that edge, and the sum is divided by the length of those two numbers, which makes it a true distance.
	float const across = scaleAcross * kScreenEdgeMovedOut;
	float const up = scaleUp * kScreenEdgeMovedOut;
	float const acrossLength = sqrtf(across * across + 1.0f);
	float const upLength = sqrtf(up * up + 1.0f);

	AddPlane(view,  across / acrossLength, 0.0f, 1.0f / acrossLength, 0.0f);
	AddPlane(view, -across / acrossLength, 0.0f, 1.0f / acrossLength, 0.0f);
	AddPlane(view, 0.0f,  up / upLength, 1.0f / upLength, 0.0f);
	AddPlane(view, 0.0f, -up / upLength, 1.0f / upLength, 0.0f);
}

void ShadowReach_BallRoundBox(const float *smallest, const float *largest, float *ball)
{
	float const halfX = (largest[0] - smallest[0]) * 0.5f;
	float const halfY = (largest[1] - smallest[1]) * 0.5f;
	float const halfZ = (largest[2] - smallest[2]) * 0.5f;

	ball[0] = smallest[0] + halfX;
	ball[1] = smallest[1] + halfY;
	ball[2] = smallest[2] + halfZ;
	ball[3] = sqrtf(halfX * halfX + halfY * halfY + halfZ * halfZ) + kBallRoundedUp;
}

// Is the stretched shape wholly outside one side of this box? near and far are the two balls' middles.
static bool ShapeMissesBox(const ShadowReachBox &box, const float *near, float nearRadius, const float *far, float farRadius)
{
	for (int axis = 0; axis < 3; axis++)
	{
		if (near[axis] + nearRadius < box.smallest[axis] && far[axis] + farRadius < box.smallest[axis])
		{
			return true;
		}

		if (near[axis] - nearRadius > box.largest[axis] && far[axis] - farRadius > box.largest[axis])
		{
			return true;
		}
	}

	return false;
}

bool ShadowReach_CanReach(const ShadowReachView &view, const ShadowReachBox *boxes, int numBoxes, const ShadowReachBall &ball, const float *light)
{
	float const range = light[3];
	float const toBallX = ball.x - light[0];
	float const toBallY = ball.y - light[1];
	float const toBallZ = ball.z - light[2];
	float const distanceSquared = toBallX * toBallX + toBallY * toBallY + toBallZ * toBallZ;
	float const radiusSquared = ball.radius * ball.radius;

	// a light inside the ball throws shadows in every direction, so there's no cone to ask about
	if (distanceSquared <= radiusSquared)
	{
		return true;
	}

	float const distance = sqrtf(distanceSquared);

	// the light stops short of the ball
	if (distance - ball.radius >= range)
	{
		return false;
	}

	// How much wider the cone gets with each unit it goes along its middle. The line from the light that just touches the ball, the line to the
	// ball's middle, and the ball's radius make a right-angled triangle, and this is the far side of that triangle over the near side.
	float const widening = ball.radius / sqrtf(distanceSquared - radiusSquared);

	// The second ball, one light range along the cone, and as wide as the cone is there. It's measured straight across the cone, so it's a little
	// more than what the range itself would reach: the cone's edge reaches the range sooner than its middle does. That's the side to be wrong on.
	float const near[3] = { ball.x, ball.y, ball.z };
	float const far[3] =
	{
		light[0] + toBallX / distance * range,
		light[1] + toBallY / distance * range,
		light[2] + toBallZ / distance * range
	};
	float const farRadius = range * widening;

	for (int i = 0; i < view.count; i++)
	{
		float const *plane = view.planes[i];
		float const ballInside = plane[0] * near[0] + plane[1] * near[1] + plane[2] * near[2] + plane[3] + ball.radius;
		float const farInside = plane[0] * far[0] + plane[1] * far[1] + plane[2] * far[2] + plane[3] + farRadius;

		if (ballInside < 0.0f && farInside < 0.0f)
		{
			return false;
		}
	}

	if (numBoxes == 0)
	{
		return true;
	}

	for (int i = 0; i < numBoxes; i++)
	{
		if (!ShapeMissesBox(boxes[i], near, ball.radius, far, farRadius))
		{
			return true;
		}
	}

	return false;
}
