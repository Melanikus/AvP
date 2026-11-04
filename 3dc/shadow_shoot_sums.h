// The sums behind the shoot tool, -shadowshoot. Written 2026-11-04 by Melanikus for the AvP port. This is debug code, and it comes out before merge, with shadow_shoot.cpp.
//
// They're in a file of their own, touching nothing of the game's or the graphics card's, so that a test can run them with neither: tests/shadow_shoot_test.cpp.
//
// THE NUMBERS: everything here is in "the list's numbers", the ones the game's polygons are drawn in. The camera is at 0 0 0, x goes right, y goes up, and z goes straight
// ahead. A ray from the camera through one pixel goes along (across, up, 1): for each unit straight ahead, it goes that far right and that far up.
//
// HOW A SHADOW IS COUNTED: every triangle of every shadow prism is drawn over the screen, writing nothing but the stencil, a counter each pixel has. Only the parts hidden behind
// what the game has already drawn count. A triangle facing away from the camera adds one, and a triangle facing the camera takes one away. A place inside a prism has one
// face behind it that faces away and nothing else, so its count ends up above zero. That place is in shadow. These sums work out by hand what the card should have done at one
// pixel, so the two can be held side by side.

#ifndef SHADOW_SHOOT_SUMS_H
#define SHADOW_SHOOT_SUMS_H

#include <math.h>

struct ShootRay
{
	double across;
	double up;
};

// A ray that lands exactly on the edge two triangles share has to count as hitting just one of them, or the count comes out wrong. So each edge that the ray lies on belongs to
// the triangle it runs upwards through, or rightwards if it's level. Going round a triangle one way, a shared edge runs one way in one triangle and the other way in its neighbour.
inline bool Shoot_RayIsInsideEdge(double weight, double edgeAcross, double edgeUp)
{
	return weight > 0.0 || (weight == 0.0 && (edgeUp > 0.0 || (edgeUp == 0.0 && edgeAcross > 0.0)));
}

// Does the ray go through this triangle, and how far ahead? Each corner is three numbers. addsOne says which way the stencil counts it: true for a triangle facing away from the
// camera. depth can come out behind the camera, so the caller has to look at it. It slides the picture sideways first, a little more the further away a thing is, which makes the
// ray lie straight along z and lets it be treated as flat. That sliding keeps everything's depth and doesn't change which way a triangle faces.
inline bool Shoot_RayCrosses(const float *a, const float *b, const float *c, const ShootRay &ray, double *depth, bool *addsOne)
{
	double const ax = (double)a[0] - ray.across * (double)a[2], ay = (double)a[1] - ray.up * (double)a[2];
	double const bx = (double)b[0] - ray.across * (double)b[2], by = (double)b[1] - ray.up * (double)b[2];
	double const cx = (double)c[0] - ray.across * (double)c[2], cy = (double)c[1] - ray.up * (double)c[2];

	// Twice its area on that flat picture. The sign says which way round its corners go: above zero is anticlockwise, and that's the side the stencil adds one for. No area means
	// it's edge-on to the ray, and the card draws no pixels for it.
	double const area = (bx - ax) * (cy - ay) - (cx - ax) * (by - ay);

	if (area == 0.0)
	{
		return false;
	}

	double const sign = (area > 0.0) ? 1.0 : -1.0;

	// How much each corner counts for at the place the ray goes through. If any is below zero, that place is outside the triangle.
	double const weightA = (bx * cy - by * cx) * sign;
	double const weightB = (cx * ay - cy * ax) * sign;
	double const weightC = (ax * by - ay * bx) * sign;

	if (!Shoot_RayIsInsideEdge(weightA, (cx - bx) * sign, (cy - by) * sign)
		|| !Shoot_RayIsInsideEdge(weightB, (ax - cx) * sign, (ay - cy) * sign)
		|| !Shoot_RayIsInsideEdge(weightC, (bx - ax) * sign, (by - ay) * sign))
	{
		return false;
	}

	*depth = (weightA * (double)a[2] + weightB * (double)b[2] + weightC * (double)c[2]) / (area * sign);
	*addsOne = area > 0.0;

	return true;
}

// How far the ray is from the nearest edge of a triangle, in pixels on the screen. The card can't tell apart places closer than about a sixteenth of a pixel, so a ray this close to an
// edge can go either way. Gives back -1 if a corner is closer than nearestDepth, and so can't be put on the screen. pixelsAcross is how many pixels one unit of "across" is.
inline double Shoot_PixelsFromEdge(const float *a, const float *b, const float *c, const ShootRay &ray, double pixelsAcross, double pixelsUp, double nearestDepth)
{
	float const *corners[3] = { a, b, c };
	double x[3], y[3];

	for (int i = 0; i < 3; i++)
	{
		if ((double)corners[i][2] < nearestDepth)
		{
			return -1.0;
		}

		x[i] = ((double)corners[i][0] / (double)corners[i][2] - ray.across) * pixelsAcross;
		y[i] = ((double)corners[i][1] / (double)corners[i][2] - ray.up) * pixelsUp;
	}

	double nearest = 1.0e30;

	for (int i = 0; i < 3; i++)
	{
		int const next = (i + 1) % 3;
		double const dx = x[next] - x[i], dy = y[next] - y[i];
		double const lengthSquared = dx * dx + dy * dy;

		// how far along this edge is the place nearest the ray? It has to stay between the two ends
		double along = (lengthSquared > 0.0) ? -(x[i] * dx + y[i] * dy) / lengthSquared : 0.0;
		along = (along < 0.0) ? 0.0 : (along > 1.0) ? 1.0 : along;

		double const px = x[i] + dx * along, py = y[i] + dy * along;
		double const distance = sqrt(px * px + py * py);

		nearest = (distance < nearest) ? distance : nearest;
	}

	return nearest;
}

// Where a depth lands in a 24-bit depth buffer, counted in the buffer's own steps. It isn't rounded. projection33 and projection43 are those two numbers of the game's projection.
inline double Shoot_DepthLevel(double depth, double projection33, double projection43)
{
	return (projection33 + projection43 / depth) * 16777216.0;
}

// The other way: how far away one of those levels is.
inline double Shoot_DepthOfLevel(double level, double projection33, double projection43)
{
	return projection43 / (level / 16777216.0 - projection33);
}

// A 16-bit float as the card stores it: a sign, 5 bits of size and 10 of detail.
inline float Shoot_HalfFloat(unsigned short half)
{
	int const exponent = (half >> 10) & 31;
	float const fraction = (float)(half & 1023) / 1024.0f;
	float const value = (exponent == 0) ? fraction * 0.00006103515625f : (1.0f + fraction) * (float)ldexp(1.0, exponent - 15);

	return (half & 0x8000) ? -value : value;
}

// ---- which part of a prism a triangle is ----

// ToyAddPrism writes a prism in this order: for each triangle of the polygon, one for the near cap and then one for the far cap. Then two triangles for each side.
enum ShootPrismPart
{
	kShootNearCap,
	kShootFarCap,
	kShootSide
};

inline int Shoot_CornersOfPrism(int polygonCorners)
{
	return ((polygonCorners - 2) * 2 + polygonCorners * 2) * 3;
}

// side gets which side, counting from the polygon's first corner, for a triangle that is one. A polygon facing away from the light is read backwards, so the count starts from the other end then.
inline ShootPrismPart Shoot_PrismPartOf(int triangle, int polygonCorners, int *side)
{
	int const capTriangles = (polygonCorners - 2) * 2;

	*side = 0;

	if (triangle < capTriangles)
	{
		return (triangle % 2 == 0) ? kShootNearCap : kShootFarCap;
	}

	*side = (triangle - capTriangles) / 2;
	return kShootSide;
}

// ---- where a prism's corner goes, when it's kept in the world's numbers ----

// A place in the world, in the list's numbers. view is the twelve numbers of the game's ViewMatrix. y gets turned over because the game's view has it pointing down.
inline void Shoot_WorldToList(const float *view, const float *world, float *onList)
{
	onList[0] =  (world[0] * view[0] + world[1] * view[1] + world[2] * view[2] + view[3]);
	onList[1] = -(world[0] * view[4] + world[1] * view[5] + world[2] * view[6] + view[7]);
	onList[2] =  (world[0] * view[8] + world[1] * view[9] + world[2] * view[10] + view[11]);
}

inline void Shoot_RayFromLight(const float *place, const float *light, float *ray)
{
	float const x = place[0] - light[0], y = place[1] - light[1], z = place[2] - light[2];
	float const squared = x * x + y * y + z * z;
	float const length = sqrtf((squared > 0.0001f) ? squared : 0.0001f);

	ray[0] = x / length;
	ray[1] = y / length;
	ray[2] = z / length;
}

// What shaders/volumeCut.vsh does with one corner, step by step, in numbers of the same size, so that this lands where the card put it. If that shader changes, this has to change
// with it. corner is x y z in the world, and end is its KeptCorner end. light is where the prism is cast from, in the list's numbers. farExact is -keptsnapfar.
inline void Shoot_FinishCutCorner(const float *view, const float *light, bool farExact, const float *corner, float end, float farMark, float *onList)
{
	float exact[3];
	Shoot_WorldToList(view, corner, exact);

	// the same cut the game makes, towards zero. y is turned over after the cut in the shader and before it here, which comes to the same, because the cut goes towards zero both ways
	float const wall[3] = { truncf(exact[0]), truncf(exact[1]), truncf(exact[2]) };
	float ray[3];

	Shoot_RayFromLight(wall, light, ray);

	if (end < -farMark)
	{
		float farRay[3] = { ray[0], ray[1], ray[2] };
		float const reach = -end - farMark;

		if (farExact)
		{
			Shoot_RayFromLight(exact, light, farRay);
		}

		onList[0] = light[0] + farRay[0] * reach;
		onList[1] = light[1] + farRay[1] * reach;
		onList[2] = light[2] + farRay[2] * reach;
		return;
	}

	onList[0] = wall[0] + ray[0] * end;
	onList[1] = wall[1] + ray[1] * end;
	onList[2] = wall[2] + ray[2] * end;
}

// ---- adding up a pixel ----

// How close to what's in the depth buffer a triangle can be, in the buffer's steps, before nobody can say which side of it the card puts it. Rounding the corners and working out
// each pixel's depth can each move it by about a step, and which way it goes changes from frame to frame. This is the flicker the tool is there to find.
static const double kShootUnsureSteps = 1.5;

struct ShootTally
{
	int sure;      // what the triangles clearly behind what's there add up to
	int mayAdd;    // how many too close to call would add one
	int mayTake;   // how many too close to call would take one away
};

// stepsBehind is how far behind what's in the depth buffer this triangle is, in steps. Only a triangle hidden behind it counts at all.
inline void Shoot_CountCrossing(ShootTally &tally, bool addsOne, double stepsBehind)
{
	if (stepsBehind > kShootUnsureSteps)
	{
		tally.sure += addsOne ? 1 : -1;
	}
	else if (stepsBehind >= -kShootUnsureSteps)
	{
		if (addsOne)
		{
			tally.mayAdd++;
		}
		else
		{
			tally.mayTake++;
		}
	}
}

// Could the crossings add up to what the card's counter says? The counter has 8 bits and goes round from 0 to 255 when it takes one away, so 255 means minus one.
inline bool Shoot_TallyAgrees(const ShootTally &tally, int stencil)
{
	int const count = (stencil > 127) ? stencil - 256 : stencil;

	return count >= tally.sure - tally.mayTake && count <= tally.sure + tally.mayAdd;
}

// ---- polygons stopped from casting ----

// A short list, because each one is checked for every polygon of every pass. A new one that won't fit is turned down, and nothing else is touched. A polygon is known by its shape and
// its number in that shape, so stopping one on an object stops every copy of that object. The shape is only ever compared, never read through.
static const int kShootMostStops = 16;

struct ShootStopList
{
	const void *shapes[kShootMostStops];
	int polygons[kShootMostStops];
	int count;

	bool Has(const void *shape, int polygon) const
	{
		for (int i = 0; i < count; i++)
		{
			if (shapes[i] == shape && polygons[i] == polygon)
			{
				return true;
			}
		}

		return false;
	}

	bool HasShape(const void *shape) const
	{
		for (int i = 0; i < count; i++)
		{
			if (shapes[i] == shape)
			{
				return true;
			}
		}

		return false;
	}

	// gives back the place it went in, or -1 if there's no room or it's already there
	int Add(const void *shape, int polygon)
	{
		if (count == kShootMostStops || Has(shape, polygon))
		{
			return -1;
		}

		shapes[count] = shape;
		polygons[count] = polygon;

		return count++;
	}

	// takes off the newest, and says false if there was none
	bool TakeOffNewest()
	{
		if (count == 0)
		{
			return false;
		}

		count--;
		return true;
	}
};

#endif
