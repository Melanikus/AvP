// Which way a polygon's texture runs across it. Written 2026-11-04 by Melanikus for the AvP port.
//
// A relief picture says how each pixel of a texture leans: a little left or right in the picture, a little up or down. Before that can light a
// wall, something has to say which way "right in the picture" points on that wall, and that depends on how the texture was laid on the polygon.
// A polygon's three corners answer it, because each has a place in the world and a place in the texture. Going along a side of the polygon moves
// so far in the world and so far in the texture. Two sides give two of those facts, which is enough to work out how far the world moves for one
// step along the texture's U, and for one step along its V.
//
// Only U's direction is stored. V's is nearly always square to it, so it's rebuilt from the normal, and all that's kept is which side it's on.
// That can be either, because a texture can be laid on backwards.

#include "surface_tangent.h"

#include <math.h>
#include <string.h>

void SurfaceTangent_Work(const float *corners, const float *coordinates, SurfaceTangent *tangent)
{
	memset(tangent, 0, sizeof(*tangent));

	double side1[3], side2[3];

	for (int axis = 0; axis < 3; axis++)
	{
		side1[axis] = (double)corners[3 + axis] - corners[axis];
		side2[axis] = (double)corners[6 + axis] - corners[axis];
	}

	double const u1 = (double)coordinates[2] - coordinates[0], v1 = (double)coordinates[3] - coordinates[1];
	double const u2 = (double)coordinates[4] - coordinates[0], v2 = (double)coordinates[5] - coordinates[1];

	// If the corners are in a line in the texture, the two facts are really one, and nothing can be worked out. This is the sum that says so.
	double const area = u1 * v2 - u2 * v1;

	if (area == 0.0)
	{
		return;
	}

	double alongU[3], alongV[3];

	for (int axis = 0; axis < 3; axis++)
	{
		alongU[axis] = (side1[axis] * v2 - side2[axis] * v1) / area;
		alongV[axis] = (side2[axis] * u1 - side1[axis] * u2) / area;
	}

	double const length = sqrt(alongU[0] * alongU[0] + alongU[1] * alongU[1] + alongU[2] * alongU[2]);

	// A very small length means the world hardly moves across the whole texture, which is a polygon with no real size.
	if (length < 0.000001)
	{
		return;
	}

	// The same arrow the polygon's normal uses, from the same two sides, so the shader's cross product comes out the same way round as this one.
	double const faceX = side1[1] * side2[2] - side1[2] * side2[1];
	double const faceY = side1[2] * side2[0] - side1[0] * side2[2];
	double const faceZ = side1[0] * side2[1] - side1[1] * side2[0];

	double const crossX = faceY * alongU[2] - faceZ * alongU[1];
	double const crossY = faceZ * alongU[0] - faceX * alongU[2];
	double const crossZ = faceX * alongU[1] - faceY * alongU[0];

	tangent->x = (float)(alongU[0] / length);
	tangent->y = (float)(alongU[1] / length);
	tangent->z = (float)(alongU[2] / length);
	tangent->flip = (crossX * alongV[0] + crossY * alongV[1] + crossZ * alongV[2] < 0.0) ? -1.0f : 1.0f;
}
