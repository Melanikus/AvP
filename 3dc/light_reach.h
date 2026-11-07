// Which part of the screen a light can reach. Written 2026-11-06 by Melanikus for the AvP port.
//
// A light only goes as far as its range, so all it can light is inside a ball round it. This works out a rectangle on the screen that's sure to
// hold all of that ball. Each light's passes are only drawn inside its rectangle, so a rectangle that's too big only costs pixels, but one that's
// too small would cut off part of the light. The sum is a little bigger than it has to be, on purpose: it takes a box round the ball, not the ball.
// It's on its own so that a test can try it with no graphics card.
#ifndef LIGHT_REACH_H
#define LIGHT_REACH_H

// How far across the screen, from -1 to 1: left to right, and bottom to top.
struct LightReachBox
{
	float left, right, bottom, top;
};

inline float LightReach_Smaller(float a, float b)
{
	return (a < b) ? a : b;
}

inline float LightReach_Larger(float a, float b)
{
	return (a > b) ? a : b;
}

// x y z is where the light is, as the camera sees it, with z the distance straight ahead. nearest is how close the camera can see. across and up
// are how much the view magnifies things. Says whether the light can reach the screen at all, and if it can, box says where.
inline bool LightReach_Box(float x, float y, float z, float range, float nearest, float across, float up, LightReachBox *box)
{
	if (z + range < nearest)
	{
		return false;  // all of it is behind the camera
	}

	box->left = -1.0f;
	box->right = 1.0f;
	box->bottom = -1.0f;
	box->top = 1.0f;

	// With part of the ball level with the camera or behind it, the sum below would divide by nothing or by less, so the whole screen is the safe answer.
	if (z - range > nearest)
	{
		float const nearZ = z - range, farZ = z + range;

		// Further away, things are drawn closer to the middle of the screen. So an edge's furthest point can be at the near side of the box or the far side, whichever is further out.
		float const left   = across * LightReach_Smaller((x - range) / nearZ, (x - range) / farZ);
		float const right  = across * LightReach_Larger((x + range) / nearZ, (x + range) / farZ);
		float const bottom = up * LightReach_Smaller((y - range) / nearZ, (y - range) / farZ);
		float const top    = up * LightReach_Larger((y + range) / nearZ, (y + range) / farZ);

		if (left >= 1.0f || right <= -1.0f || bottom >= 1.0f || top <= -1.0f)
		{
			return false;  // all of it is off to one side
		}

		box->left   = LightReach_Larger(left, -1.0f);
		box->right  = LightReach_Smaller(right, 1.0f);
		box->bottom = LightReach_Larger(bottom, -1.0f);
		box->top    = LightReach_Smaller(top, 1.0f);
	}

	return true;
}

#endif
