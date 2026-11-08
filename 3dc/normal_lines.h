// The sums behind the NORMAL LINES view, with nothing of the game in them, so that tests/normal_lines_test.cpp can run them. Written 2026-11-06 by Melanikus for the AvP port.
//
// A "normal" is an arrow saying which way a point of a surface faces. Lights go by it, so a wrong one can paint a patch dark. This view stands each arrow up in the
// level as a real line, so that a person can walk up to a dark patch and see what the lights were told about it. Walls hide the lines, the same as anything else in the level.
//
// IT SHOWS WHAT THE CARD WAS GIVEN. Nothing here works out a normal, because then it could look right while the real numbers were wrong. They come from the copy
// SurfaceNormals_WritePolygon makes while it writes the card's list, or from the fixed buffer's own corners. The only thing worked out here is which way a polygon
// faces, from the order of its corners, as something to hold each normal up against.
//
// THE COLOURS, drawn in this order, so that the loud ones land on top where lines overlap:
//   green    the normal agrees with its polygon, to within 5 degrees. Whole numbers in the level's files can account for a degree or so
//   amber    it leans between 5 and 45 degrees from its polygon. On something flat, the lights would make part of the polygon darker than the rest
//   red      it leans past 45 degrees, or past 90, which is pointing away. Past 90 its real line goes into the wall, where the wall would hide it, so it's also drawn mirrored out of the wall
//   cyan     a small cross: the normal is zero, so it points nowhere, and the lights treat that point as turned side-on
//   magenta  the outline of a polygon the game draws but the surface pictures leave out: its fourth number is 0, surface.vsh calls that Keep.x
//
// WHAT IT DOESN'T KNOW: on the fixed buffer, a polygon whose stored normal disagrees with its corners has had its triangles turned round to suit the normal, so that polygon can
// show as agreeing there, and show red on the old route. The old route is the honest one for that question. See static_level.cpp.
//
// Every list here has a limit. What doesn't fit is counted and said in the log, and the agreeing lines have a limit of their own, so they can't push out the ones that matter.

#ifndef NORMAL_LINES_H
#define NORMAL_LINES_H

#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include <algorithm>
#include <vector>

static char const * const kNormalLinesName = "NORMAL LINES";

enum NormalLineKind
{
	kNormalAgrees,
	kNormalLeans,
	kNormalLeansBadly,
	kNormalIsZero,
	kNormalOutOfPictures,
	kNormalLineKinds
};

float const kNormalLineLength = 300.0f;         // millimetres, for a normal of length 1
float const kNormalLineShortest = 0.25f;        // a very short normal is drawn this many times that, or nobody could see it. The pixel shader stretches it to 1 anyway
float const kNormalLineLongest = 3.0f;          // and a very long one no more than this, so a damaged number can't draw a line across the level
float const kNormalLineCrossSize = 40.0f;       // how far each arm of a zero normal's cross reaches
float const kNormalLineFurthest = 20000.0f;     // past 20 metres a line is a couple of pixels, and it only hides what's near
double const kNormalLineAgreeCosine = 0.99619470;   // the cosine of 5 degrees
double const kNormalLineBadCosine = 0.70710678;     // the cosine of 45 degrees
double const kNormalLineNoArea = 0.000001;      // the same as WorkOutPolygon in surface_normals.cpp: 0.001 long, squared
int const kNormalLinePlentyOfAgreeing = 40000;
int const kNormalLinePlentyOfTheRest = 20000;

inline int NormalLines_MostOfKind(int kind)
{
	return (kind == kNormalAgrees) ? kNormalLinePlentyOfAgreeing : kNormalLinePlentyOfTheRest;
}

struct NormalLineEnd
{
	float x, y, z;
	uint8_t red, green, blue;
};

struct NormalLinesList
{
	std::vector<NormalLineEnd> ends[kNormalLineKinds];   // two for each line
	int found[kNormalLineKinds];       // corners of each kind, and whole polygons for kNormalOutOfPictures, whether or not their lines fitted
	int leftOut[kNormalLineKinds];     // lines that didn't fit under the limit
	int polygonsFromList;              // polygons read from the game's list
	int trianglesFromBuffer;           // triangles read from the fixed buffer
	int leftToBuffer;                  // polygons on the list whose fourth number says the fixed buffer draws them, on a frame it did
	int noArea;                        // polygons with no area, which cover no pixels
	int backs;                         // fixed buffer triangles with their backs to the camera, which the card drops
	int behindCamera;                  // corners behind the camera
	int tooFar;                        // corners past kNormalLineFurthest
	int oddLength;                     // normals that aren't about 1 long
	int pastNinety;                    // of the ones that lean badly, how many are past 90 degrees, so pointing away from their polygon
};

inline void NormalLines_Clear(NormalLinesList &list)
{
	for (int kind = 0; kind < kNormalLineKinds; kind++)
	{
		list.ends[kind].clear();
		list.found[kind] = 0;
		list.leftOut[kind] = 0;
	}

	list.polygonsFromList = 0;
	list.trianglesFromBuffer = 0;
	list.leftToBuffer = 0;
	list.noArea = 0;
	list.backs = 0;
	list.behindCamera = 0;
	list.tooFar = 0;
	list.oddLength = 0;
	list.pastNinety = 0;
}

inline void NormalLines_KindColour(int kind, uint8_t *red, uint8_t *green, uint8_t *blue)
{
	static uint8_t const colours[kNormalLineKinds][3] =
	{
		{ 64, 255, 64 },     // green
		{ 255, 176, 0 },     // amber
		{ 255, 0, 0 },       // red
		{ 0, 255, 255 },     // cyan
		{ 255, 0, 255 },     // magenta
	};

	*red = colours[kind][0];
	*green = colours[kind][1];
	*blue = colours[kind][2];
}

// Adds a line, if there's room for it. It starts at half brightness and ends at full, so that a line that's only a few pixels long still says which way it goes.
inline bool NormalLines_AddLine(NormalLinesList &list, int kind, const float *from, const float *to)
{
	if ((int)(list.ends[kind].size() / 2) >= NormalLines_MostOfKind(kind))
	{
		list.leftOut[kind]++;
		return false;
	}

	uint8_t red, green, blue;
	NormalLines_KindColour(kind, &red, &green, &blue);

	NormalLineEnd const start = { from[0], from[1], from[2], (uint8_t)(red / 2), (uint8_t)(green / 2), (uint8_t)(blue / 2) };
	NormalLineEnd const end = { to[0], to[1], to[2], red, green, blue };

	list.ends[kind].push_back(start);
	list.ends[kind].push_back(end);
	return true;
}

// Which way a polygon's corners say it faces: the same sum as PolygonFacing in polygon_facing.h, on three numbers for each corner. As long as twice the polygon's area.
inline void NormalLines_Facing(const float *corners, int count, double *facing)
{
	facing[0] = 0.0;
	facing[1] = 0.0;
	facing[2] = 0.0;

	for (int i = 1; i < count - 1; i++)
	{
		double const ax = (double)corners[i * 3 + 0] - corners[0], ay = (double)corners[i * 3 + 1] - corners[1], az = (double)corners[i * 3 + 2] - corners[2];
		double const bx = (double)corners[i * 3 + 3] - corners[0], by = (double)corners[i * 3 + 4] - corners[1], bz = (double)corners[i * 3 + 5] - corners[2];

		facing[0] += ay * bz - az * by;
		facing[1] += az * bx - ax * bz;
		facing[2] += ax * by - ay * bx;
	}
}

inline bool NormalLines_HasArea(const double *facing)
{
	return facing[0] * facing[0] + facing[1] * facing[1] + facing[2] * facing[2] >= kNormalLineNoArea;
}

// Turns a direction from the world's numbers into the ones the game's list uses, with axes from ToyShadows_WorldAxes. This is the sum surface.psh does.
inline void NormalLines_Turn(const float *axes, const float *direction, float *turned)
{
	for (int i = 0; i < 3; i++)
	{
		turned[i] = direction[0] * axes[i] + direction[1] * axes[3 + i] + direction[2] * axes[6 + i];
	}
}

// Puts a fixed buffer corner where surfaceStatic.vsh puts it, with view from ToyShadows_ViewMatrix: the game's sum, cut down to whole numbers, with y turned over.
inline void NormalLines_WorldToList(const float *view, const float *world, float *place)
{
	for (int i = 0; i < 3; i++)
	{
		float const sum = world[0] * view[i * 4 + 0] + world[1] * view[i * 4 + 1] + world[2] * view[i * 4 + 2] + view[i * 4 + 3];

		place[i] = (float)(int)sum;
	}

	place[1] = -place[1];
}

// Is there a direction here at all? Zero has none, and the pixel shader says the same. A number that isn't a number says nothing about any direction either.
inline bool NormalLines_HasDirection(const float *normal)
{
	double const size = (double)normal[0] * normal[0] + (double)normal[1] * normal[1] + (double)normal[2] * normal[2];

	return size > 0.0 && size < 1e12;
}

// How well a normal with a direction lines up with its polygon, which has to have area: 1 is straight out of it, 0 is along it, and below 0 is pointing away.
inline double NormalLines_Along(const float *normal, const double *facing)
{
	double const normalSize = (double)normal[0] * normal[0] + (double)normal[1] * normal[1] + (double)normal[2] * normal[2];
	double const facingSize = facing[0] * facing[0] + facing[1] * facing[1] + facing[2] * facing[2];

	return (normal[0] * facing[0] + normal[1] * facing[1] + normal[2] * facing[2]) / sqrt(normalSize * facingSize);
}

// Says which band of lean this normal is in.
inline NormalLineKind NormalLines_KindOf(const float *normal, const double *facing)
{
	if (!NormalLines_HasDirection(normal))
	{
		return kNormalIsZero;
	}

	double const along = NormalLines_Along(normal, facing);

	return (along >= kNormalLineAgreeCosine) ? kNormalAgrees : (along >= kNormalLineBadCosine) ? kNormalLeans : kNormalLeansBadly;
}

// One corner: place and normal are in the numbers of the game's list.
inline void NormalLines_AddCorner(NormalLinesList &list, const float *place, const float *normal, const double *facing)
{
	if (!(place[2] > 0.0f))
	{
		list.behindCamera++;
		return;
	}

	if (place[0] * place[0] + place[1] * place[1] + place[2] * place[2] > kNormalLineFurthest * kNormalLineFurthest)
	{
		list.tooFar++;
		return;
	}

	NormalLineKind const kind = NormalLines_KindOf(normal, facing);

	list.found[kind]++;

	if (kind == kNormalIsZero)
	{
		for (int axis = 0; axis < 3; axis++)
		{
			float from[3] = { place[0], place[1], place[2] };
			float to[3] = { place[0], place[1], place[2] };

			from[axis] -= kNormalLineCrossSize;
			to[axis] += kNormalLineCrossSize;
			NormalLines_AddLine(list, kind, from, to);
		}

		return;
	}

	double const length = sqrt((double)normal[0] * normal[0] + (double)normal[1] * normal[1] + (double)normal[2] * normal[2]);

	if (length < 0.95 || length > 1.05)
	{
		list.oddLength++;
	}

	double const drawnLength = kNormalLineLength * ((length < kNormalLineShortest) ? kNormalLineShortest : (length > kNormalLineLongest) ? kNormalLineLongest : length);
	double const across = drawnLength / length;
	float const tip[3] = { (float)(place[0] + normal[0] * across), (float)(place[1] + normal[1] * across), (float)(place[2] + normal[2] * across) };

	NormalLines_AddLine(list, kind, place, tip);

	// Past 90 degrees its real line is inside the wall, and seen from the front the wall hides it. This one has the same lean, turned out through the wall's surface. Exactly sideways counts
	// too, because a light gets nothing from that either.
	if (kind == kNormalLeansBadly && !(NormalLines_Along(normal, facing) > 0.0))
	{
		list.pastNinety++;

		double const facingLength = sqrt(facing[0] * facing[0] + facing[1] * facing[1] + facing[2] * facing[2]);
		double const fx = facing[0] / facingLength, fy = facing[1] / facingLength, fz = facing[2] / facingLength;
		double const into = 2.0 * (normal[0] * fx + normal[1] * fy + normal[2] * fz);
		float const mirrored[3] =
		{
			(float)(place[0] + (normal[0] - into * fx) * across),
			(float)(place[1] + (normal[1] - into * fy) * across),
			(float)(place[2] + (normal[2] - into * fz) * across),
		};

		NormalLines_AddLine(list, kind, place, mirrored);
	}
}

// The outline of a polygon that stays out of the pictures, with lines across it so that it can't be mistaken for an edge of the level. It only needs one corner to be near.
inline void NormalLines_AddOutline(NormalLinesList &list, const float *places, int corners)
{
	bool nearEnough = false;

	for (int i = 0; i < corners; i++)
	{
		const float *place = places + i * 3;

		nearEnough = nearEnough || (place[0] * place[0] + place[1] * place[1] + place[2] * place[2] <= kNormalLineFurthest * kNormalLineFurthest);
	}

	if (!nearEnough)
	{
		list.tooFar++;
		return;
	}

	list.found[kNormalOutOfPictures]++;

	for (int i = 0; i < corners; i++)
	{
		NormalLines_AddLine(list, kNormalOutOfPictures, places + i * 3, places + ((i + 1) % corners) * 3);
	}

	for (int i = 2; i < corners - 1; i++)
	{
		NormalLines_AddLine(list, kNormalOutOfPictures, places, places + i * 3);
	}
}

// One polygon from the game's list, as the surface pass gets it. places holds three numbers for each corner, in the list's numbers. normals holds four for each corner, as the
// card's list does: x y z in the world's directions, and the number surface.vsh goes by. bufferDrewThisFrame says the fixed buffer drew its polygons on this frame.
inline void NormalLines_AddListPolygon(NormalLinesList &list, const float *places, const float *normals, int corners, const float *axes, bool bufferDrewThisFrame)
{
	if (corners < 3)
	{
		return;
	}

	list.polygonsFromList++;

	// the same two tests as surface.vsh, and with the same numbers
	float const fourth = normals[3];

	if (fourth < 0.5f)
	{
		NormalLines_AddOutline(list, places, corners);
		return;
	}

	if (fourth > 1.5f && bufferDrewThisFrame)
	{
		list.leftToBuffer++;
		return;
	}

	// The list has y turned over, which makes it a mirror image, and a mirror image's corners go round the other way. So the arrow its corners give points out of the polygon's back.
	double facing[3];
	NormalLines_Facing(places, corners, facing);

	if (!NormalLines_HasArea(facing))
	{
		list.noArea++;
		return;
	}

	facing[0] = -facing[0];
	facing[1] = -facing[1];
	facing[2] = -facing[2];

	for (int i = 0; i < corners; i++)
	{
		float turned[3];
		NormalLines_Turn(axes, normals + i * 4, turned);
		NormalLines_AddCorner(list, places + i * 3, turned, facing);
	}
}

// One triangle from the fixed buffer. Corners there aren't shared between polygons, but a polygon's triangles share them, so drawn says which of the three are done already, and
// this marks the ones it does. worldCorners and worldNormals hold three numbers for each. A triangle with its back to the camera is left alone, as the card does, unless the game shows both sides of it.
inline void NormalLines_AddBufferTriangle(NormalLinesList &list, const float *view, const float *axes, const float *worldCorners, const float *worldNormals, bool twoSided, bool *drawn)
{
	list.trianglesFromBuffer++;

	double worldFacing[3];
	NormalLines_Facing(worldCorners, 3, worldFacing);

	if (!NormalLines_HasArea(worldFacing))
	{
		list.noArea++;
		return;
	}

	double facing[3];

	for (int i = 0; i < 3; i++)
	{
		facing[i] = worldFacing[0] * axes[i] + worldFacing[1] * axes[3 + i] + worldFacing[2] * axes[6 + i];
	}

	float places[9];

	for (int i = 0; i < 3; i++)
	{
		NormalLines_WorldToList(view, worldCorners + i * 3, places + i * 3);
	}

	// The camera is at nought in these numbers, so a surface whose arrow points the same way as the line from the camera to it has its back to the camera
	if (!twoSided && places[0] * facing[0] + places[1] * facing[1] + places[2] * facing[2] >= 0.0)
	{
		list.backs++;
		return;
	}

	for (int i = 0; i < 3; i++)
	{
		if (!drawn[i])
		{
			float turned[3];
			NormalLines_Turn(axes, worldNormals + i * 3, turned);
			NormalLines_AddCorner(list, places + i * 3, turned, facing);
			drawn[i] = true;
		}
	}
}

// What J does with this view, which comes after NORMALS on its list. It draws the real picture with the lines on top, so as far as the shadow pass goes the view is FINAL, and this is
// what says the lines are wanted. Gives back false if this isn't J's move, and then the view goes on to the next as it always has.
inline bool View_StepNormalLines(int *view, int viewOff, int viewFinal, int viewBeforeTheLines, int *linesOn)
{
	if (*linesOn)
	{
		*linesOn = 0;
		*view = viewOff;
		return true;
	}

	if (*view == viewBeforeTheLines)
	{
		*linesOn = 1;
		*view = viewFinal;
		return true;
	}

	return false;
}

// WHICH SHAPES: the log also names the shapes holding the leaning corners, so a person can tell a model from a wall. This only knows the game's list: the fixed buffer doesn't say which shape a corner came from.
struct NormalLinesShape
{
	int shapeNumber;       // its place in the game's list of shapes
	int behaviour;         // what the object is, from AVP_BEHAVIOUR_TYPE, or -1 for a piece of the level with no owner
	bool hasPolygonList;   // only a shape with a list of polygons gets the rule that flattens a corner past 30 degrees: see surface_normals.cpp
	int pastNinety;
	int badly;             // past 45 degrees, the ones past 90 included
	int leaning;           // 5 to 45 degrees
};

int const kNormalLinesShapesKept = 64;

struct NormalLinesShapes
{
	NormalLinesShape list[kNormalLinesShapesKept];
	int count;
	int notKept;   // shapes with leaning corners after the list was full
};

inline void NormalLines_ForgetShapes(NormalLinesShapes &shapes)
{
	shapes.count = 0;
	shapes.notKept = 0;
}

// Adds what one polygon's corners did. Corners that agree or are zero aren't what this is looking for.
inline void NormalLines_NoteShape(NormalLinesShapes &shapes, int shapeNumber, int behaviour, bool hasPolygonList, int pastNinety, int badly, int leaning)
{
	if (pastNinety <= 0 && badly <= 0 && leaning <= 0)
	{
		return;
	}

	for (int i = 0; i < shapes.count; i++)
	{
		NormalLinesShape &shape = shapes.list[i];

		if (shape.shapeNumber == shapeNumber && shape.behaviour == behaviour)
		{
			shape.pastNinety += pastNinety;
			shape.badly += badly;
			shape.leaning += leaning;
			return;
		}
	}

	if (shapes.count >= kNormalLinesShapesKept)
	{
		shapes.notKept++;
		return;
	}

	NormalLinesShape const shape = { shapeNumber, behaviour, hasPolygonList, pastNinety, badly, leaning };
	shapes.list[shapes.count++] = shape;
}

// The worst go first: most past 90, then most past 45, then most leaning.
inline void NormalLines_PutWorstFirst(NormalLinesShapes &shapes)
{
	std::sort(shapes.list, shapes.list + shapes.count, [](NormalLinesShape const &a, NormalLinesShape const &b)
	{
		if (a.pastNinety != b.pastNinety) { return a.pastNinety > b.pastNinety; }
		if (a.badly != b.badly) { return a.badly > b.badly; }
		return a.leaning > b.leaning;
	});
}

// Its name, as text for the log. A name comes from a level file, so anything that isn't a plain letter or number is shown as a question mark, and it can't make a line of its own in the log.
inline void NormalLines_CleanName(const char *name, char *clean, size_t size)
{
	size_t place = 0;

	for (; name && name[place] && place + 1 < size; place++)
	{
		unsigned char const c = (unsigned char)name[place];

		clean[place] = (c >= 32 && c < 127) ? (char)c : '?';
	}

	clean[place] = 0;
}

// The line for the log, naming the worst few. nameOf gives a shape's name from its number, or nothing. It stops before it runs out of room, and shapes must already be worst first.
inline void NormalLines_DescribeShapes(const NormalLinesShapes &shapes, const char *(*nameOf)(int), int most, char *line, size_t size)
{
	int written = snprintf(line, size, "normal lines: %s", shapes.count ? "worst shapes:" : "no shape on the game's list has a leaning corner");

	for (int i = 0; i < shapes.count && i < most; i++)
	{
		if (written < 0 || (size_t)written >= size)
		{
			return;
		}

		NormalLinesShape const &shape = shapes.list[i];
		char name[32];
		NormalLines_CleanName(nameOf(shape.shapeNumber), name, sizeof(name));

		int const added = snprintf(line + written, size - written, " shape %d \"%s\", behaviour %d, %s: %d past 90, %d past 45, %d lean;", shape.shapeNumber, name, shape.behaviour,
			shape.hasPolygonList ? "has a polygon list" : "NO POLYGON LIST", shape.pastNinety, shape.badly, shape.leaning);

		written = (added < 0) ? -1 : written + added;
	}

	if (written >= 0 && (size_t)written < size && (shapes.count > most || shapes.notKept > 0))
	{
		snprintf(line + written, size - written, " and %d more shapes, %d of them not kept", ((shapes.count > most) ? shapes.count - most : 0) + shapes.notKept, shapes.notKept);
	}
}

inline bool NormalLines_StoppedShort(const NormalLinesList &list, int notNoted)
{
	if (notNoted > 0)
	{
		return true;
	}

	for (int kind = 0; kind < kNormalLineKinds; kind++)
	{
		if (list.leftOut[kind] > 0)
		{
			return true;
		}
	}

	return false;
}

// The line for the log. notNoted is how many polygons didn't fit on the list of what was drawn this frame.
inline void NormalLines_Describe(const NormalLinesList &list, int frame, bool passRan, bool bufferDrew, bool earlyFlush, int notNoted, char *line, size_t size)
{
	int leftOutInAll = 0;

	for (int kind = 0; kind < kNormalLineKinds; kind++)
	{
		leftOutInAll += list.leftOut[kind];
	}

	int const written = snprintf(line, size,
		"normal lines: frame %d, %s%s, %d polygons from the list, %d triangles from the fixed buffer, %d left to the buffer: corners %d agree (under 5 degrees), %d lean (5 to 45), %d lean badly (past 45, and %d of those past 90), %d zero, %d odd length; %d polygons out of the pictures; %d no area, %d backs, %d behind the camera, %d too far%s",
		frame, !passRan ? "no surface pass this frame" : bufferDrew ? "surface pass used both routes" : "surface pass used the list alone", earlyFlush ? ", and the lists were drawn early, so this is only the last part of the frame" : "",
		list.polygonsFromList, list.trianglesFromBuffer, list.leftToBuffer,
		list.found[kNormalAgrees], list.found[kNormalLeans], list.found[kNormalLeansBadly], list.pastNinety, list.found[kNormalIsZero], list.oddLength, list.found[kNormalOutOfPictures],
		list.noArea, list.backs, list.behindCamera, list.tooFar, NormalLines_StoppedShort(list, notNoted) ? "; STOPPED SHORT:" : "");

	if (NormalLines_StoppedShort(list, notNoted) && written > 0 && (size_t)written < size)
	{
		snprintf(line + written, size - written, " %d lines past the limit, %d polygons not noted", leftOutInAll, notNoted);
	}
}

#endif
