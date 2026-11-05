// Which way a polygon faces. Written 2026-11-04 by Melanikus for the AvP port.
//
// The first three corners of a polygon aren't enough to say. If they happen to lie almost in a line, the triangle they make is a sliver, and it can
// point anywhere: on 634 stock polygons it's more than 25 degrees out, and on 28 it faces the wrong way altogether, one of them a wall 3,000 units across.
// So this adds up all the polygon's triangles. A sliver only adds its tiny share, and the big triangle decides. The answer is as long as twice the
// polygon's area, which also says how big it is, and it follows the order of the corners, not the normal stored for the polygon.

#ifndef POLYGON_FACING_H
#define POLYGON_FACING_H

#include "3dc.h"

// corners holds count corner numbers, and all of them have to have been checked already. facing gets x y z.
static inline void PolygonFacing(const VECTORCH *points, const int *corners, int count, double *facing)
{
	facing[0] = 0.0;
	facing[1] = 0.0;
	facing[2] = 0.0;

	VECTORCH const &first = points[corners[0]];

	for (int i = 1; i < count - 1; i++)
	{
		double const ax = (double)points[corners[i]].vx - first.vx, ay = (double)points[corners[i]].vy - first.vy, az = (double)points[corners[i]].vz - first.vz;
		double const bx = (double)points[corners[i + 1]].vx - first.vx, by = (double)points[corners[i + 1]].vy - first.vy, bz = (double)points[corners[i + 1]].vz - first.vz;

		facing[0] += ay * bz - az * by;
		facing[1] += az * bx - ax * bz;
		facing[2] += ax * by - ay * bx;
	}
}

#endif
