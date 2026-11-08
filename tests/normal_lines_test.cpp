// Tests the sums behind the NORMAL LINES view, in 3dc/normal_lines.h, and that the game is wired to them. Written 2026-11-06 by Melanikus for the AvP port.
//
// Nothing here needs a graphics card or the game's data. What the lines look like in the game, and whether they say anything about the dark patches, needs a person looking at it.

// Modified 2026-11-08 by Melanikus for the AvP port:
//   its Between is the shared one in source_slice.h, so a marker that isn't there is a failed check of its own (SOURCE-SLICE-1, #128)
#include "normal_lines.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include "source_slice.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static bool Near(float a, float b, float within = 0.01f)
{
	return fabsf(a - b) <= within;
}

static std::string ReadSourceFile(char const *name)
{
	// /FC in this test's settings makes __FILE__ the whole path of this file, so the repository's top is found from it
	std::string path = __FILE__;
	std::string::size_type const tests = path.rfind("tests");

	if (tests == std::string::npos)
	{
		return "";
	}

	std::ifstream file((path.substr(0, tests) + name).c_str(), std::ios::binary);
	std::stringstream all;
	all << file.rdbuf();

	std::string text = all.str();
	text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
	return text;
}

// The text from one marker up to the next (source_slice.h). A marker that isn't there is a failed check of its own, named by the marker.
static std::string Between(std::string const &text, char const *from, char const *to)
{
	return SourceSlice(text, from, to, kSliceEndFromOneOn, [](const std::string &why) { Check(false, why.c_str()); });
}

static bool Has(std::string const &text, char const *words)
{
	return text.find(words) != std::string::npos;
}

// A floor-like polygon's facing is straight along z here, so a normal tilted by some degrees from z is easy to make.
static void TiltedFromZ(float degrees, float *normal)
{
	float const radians = degrees * 3.14159265f / 180.0f;

	normal[0] = sinf(radians);
	normal[1] = 0.0f;
	normal[2] = cosf(radians);
}

static void CheckTheBands()
{
	double const facing[3] = { 0.0, 0.0, 5000.0 };
	float normal[3];

	TiltedFromZ(0.0f, normal);
	Check(NormalLines_KindOf(normal, facing) == kNormalAgrees, "straight out of the polygon agrees");
	TiltedFromZ(2.0f, normal);
	Check(NormalLines_KindOf(normal, facing) == kNormalAgrees, "2 degrees agrees");
	TiltedFromZ(4.9f, normal);
	Check(NormalLines_KindOf(normal, facing) == kNormalAgrees, "4.9 degrees agrees");
	TiltedFromZ(5.1f, normal);
	Check(NormalLines_KindOf(normal, facing) == kNormalLeans, "5.1 degrees leans");
	TiltedFromZ(44.9f, normal);
	Check(NormalLines_KindOf(normal, facing) == kNormalLeans, "44.9 degrees leans");
	TiltedFromZ(45.1f, normal);
	Check(NormalLines_KindOf(normal, facing) == kNormalLeansBadly, "45.1 degrees leans badly");
	TiltedFromZ(135.0f, normal);
	Check(NormalLines_KindOf(normal, facing) == kNormalLeansBadly, "past 90 is the same band");

	float const shortOne[3] = { 0.0f, 0.0f, 0.01f };
	Check(NormalLines_KindOf(shortOne, facing) == kNormalAgrees, "how long it is doesn't change its band");

	float const zero[3] = { 0.0f, 0.0f, 0.0f };
	Check(NormalLines_KindOf(zero, facing) == kNormalIsZero, "zero has its own band");

	float const notANumber[3] = { sqrtf(-1.0f), 0.0f, 0.0f };
	Check(NormalLines_KindOf(notANumber, facing) == kNormalIsZero, "a number that isn't one says nothing about a direction");

	float const huge[3] = { 1e30f, 0.0f, 0.0f };
	Check(NormalLines_KindOf(huge, facing) == kNormalIsZero, "nor does one too big to work with");
}

// A quad the camera sees from its front, in the list's numbers: x to the right, y up, and 1000 away. A polygon's corners run anticlockwise on screen seen from the front: surface_static.cpp says so, and it's what
// the card goes by. So the arrow worked out from them points away from the camera, which is the mirror image the top of surface_normals.cpp talks about.
static float const quadPlaces[12] = { -100.0f, -100.0f, 1000.0f, 100.0f, -100.0f, 1000.0f, 100.0f, 100.0f, 1000.0f, -100.0f, 100.0f, 1000.0f };

// Reading a line that isn't there gives somewhere no line ever is, so the check that asked fails, and the rest of the checks still run.
static NormalLineEnd const &EndAt(NormalLinesList const &list, int kind, size_t place)
{
	static NormalLineEnd const nowhere = { -1e9f, -1e9f, -1e9f, 0, 0, 0 };

	return (place < list.ends[kind].size()) ? list.ends[kind][place] : nowhere;
}

static float const noTurning[9] = { 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f };

static void MakeNormals(float *normals, float x, float y, float z, float fourth)
{
	for (int i = 0; i < 4; i++)
	{
		normals[i * 4 + 0] = x;
		normals[i * 4 + 1] = y;
		normals[i * 4 + 2] = z;
		normals[i * 4 + 3] = fourth;
	}
}

static void CheckTheListRoute()
{
	static NormalLinesList list;
	float normals[16];

	double facing[3];
	NormalLines_Facing(quadPlaces, 4, facing);
	Check(facing[2] != 0.0 && Near((float)facing[0], 0.0f) && Near((float)facing[1], 0.0f), "a polygon's corners give an arrow straight through it");
	Check(Near((float)fabs(facing[2]), 80000.0f, 1.0f), "and it's as long as twice its area");

	// The camera is at nought and looks along +z, so the side it sees faces -z.
	NormalLines_Clear(list);
	MakeNormals(normals, 0.0f, 0.0f, -1.0f, 1.0f);
	NormalLines_AddListPolygon(list, quadPlaces, normals, 4, noTurning, false);
	Check(list.found[kNormalAgrees] == 4, "a normal towards the camera agrees with a polygon seen from the front");
	Check(list.ends[kNormalAgrees].size() == 8, "each corner gets one line");
	Check(Near(EndAt(list, kNormalAgrees, 0).x, -100.0f) && Near(EndAt(list, kNormalAgrees, 0).z, 1000.0f), "a line starts at its corner");
	Check(Near(EndAt(list, kNormalAgrees, 1).z, 1000.0f - kNormalLineLength), "and ends the length of the line out of it");
	Check(list.oddLength == 0, "a normal 1 long isn't odd");
	Check(list.polygonsFromList == 1, "the polygon is counted");

	NormalLines_Clear(list);
	MakeNormals(normals, 0.0f, 0.0f, 1.0f, 1.0f);
	NormalLines_AddListPolygon(list, quadPlaces, normals, 4, noTurning, false);
	Check(list.found[kNormalLeansBadly] == 4 && list.pastNinety == 4, "a normal into the wall is past 90");
	Check(list.ends[kNormalLeansBadly].size() == 16, "and gets two lines at each corner");
	Check(Near(EndAt(list, kNormalLeansBadly, 1).z, 1000.0f + kNormalLineLength), "its real one goes into the wall");
	Check(Near(EndAt(list, kNormalLeansBadly, 3).z, 1000.0f - kNormalLineLength), "and the other comes out of it, where it can be seen");

	float leaning[3];
	TiltedFromZ(60.0f, leaning);
	NormalLines_Clear(list);
	MakeNormals(normals, leaning[0], leaning[1], -leaning[2], 1.0f);
	NormalLines_AddListPolygon(list, quadPlaces, normals, 4, noTurning, false);
	Check(list.found[kNormalLeansBadly] == 4 && list.pastNinety == 0, "60 degrees leans badly, but isn't past 90");
	Check(list.ends[kNormalLeansBadly].size() == 8, "so it's already out of the wall, and needs no second line");

	NormalLines_Clear(list);
	MakeNormals(normals, 0.0f, 0.0f, 0.0f, 1.0f);
	NormalLines_AddListPolygon(list, quadPlaces, normals, 4, noTurning, false);
	Check(list.found[kNormalIsZero] == 4, "a zero normal that's really there is counted as zero");
	Check(list.ends[kNormalIsZero].size() == 24, "and gets a cross of three lines");

	NormalLines_Clear(list);
	MakeNormals(normals, 0.0f, 0.0f, 0.0f, 0.0f);
	NormalLines_AddListPolygon(list, quadPlaces, normals, 4, noTurning, false);
	Check(list.found[kNormalOutOfPictures] == 1 && list.found[kNormalIsZero] == 0, "a fourth number of 0 is out of the pictures, and isn't a zero normal");
	Check(list.ends[kNormalOutOfPictures].size() == 10, "it gets its four edges and a line across");

	NormalLines_Clear(list);
	MakeNormals(normals, 0.0f, 0.0f, -1.0f, 2.0f);
	NormalLines_AddListPolygon(list, quadPlaces, normals, 4, noTurning, true);
	Check(list.leftToBuffer == 1 && list.found[kNormalAgrees] == 0, "a polygon the fixed buffer drew isn't read from the list as well");

	NormalLines_Clear(list);
	NormalLines_AddListPolygon(list, quadPlaces, normals, 4, noTurning, false);
	Check(list.leftToBuffer == 0 && list.found[kNormalAgrees] == 4, "on a frame the buffer didn't draw, the list has it");

	// A normal in the world's numbers is turned the way surface.psh does it: here the world's x is the list's z, and the other way about.
	float const swapXAndZ[9] = { 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f };
	NormalLines_Clear(list);
	MakeNormals(normals, -1.0f, 0.0f, 0.0f, 1.0f);
	NormalLines_AddListPolygon(list, quadPlaces, normals, 4, swapXAndZ, false);
	Check(list.found[kNormalAgrees] == 4, "a normal is turned into the list's numbers before it's compared");

	float const sliver[9] = { 0.0f, 0.0f, 1000.0f, 10.0f, 10.0f, 1000.0f, 20.0f, 20.0f, 1000.0f };
	NormalLines_Clear(list);
	MakeNormals(normals, 0.0f, 0.0f, -1.0f, 1.0f);
	NormalLines_AddListPolygon(list, sliver, normals, 3, noTurning, false);
	Check(list.noArea == 1 && list.found[kNormalAgrees] == 0, "a polygon with no area covers no pixels, so it's left out and counted");

	NormalLines_Clear(list);
	NormalLines_AddListPolygon(list, quadPlaces, normals, 2, noTurning, false);
	Check(list.polygonsFromList == 0, "two corners aren't a polygon");
}

static void CheckTheLengths()
{
	static NormalLinesList list;
	float const place[3] = { 0.0f, 0.0f, 1000.0f };
	double const facing[3] = { 0.0, 0.0, -1000.0 };

	float const longOne[3] = { 0.0f, 0.0f, -100.0f };
	NormalLines_Clear(list);
	NormalLines_AddCorner(list, place, longOne, facing);
	Check(Near(EndAt(list, kNormalAgrees, 1).z, 1000.0f - kNormalLineLength * kNormalLineLongest), "a very long normal stops at the longest line");
	Check(list.oddLength == 1, "and is counted as an odd length");

	float const shortOne[3] = { 0.0f, 0.0f, -0.001f };
	NormalLines_Clear(list);
	NormalLines_AddCorner(list, place, shortOne, facing);
	Check(Near(EndAt(list, kNormalAgrees, 1).z, 1000.0f - kNormalLineLength * kNormalLineShortest), "a very short one is still long enough to see");
	Check(list.oddLength == 1, "and is counted too");

	float const half[3] = { 0.0f, 0.0f, -0.5f };
	NormalLines_Clear(list);
	NormalLines_AddCorner(list, place, half, facing);
	Check(Near(EndAt(list, kNormalAgrees, 1).z, 1000.0f - kNormalLineLength * 0.5f), "in between, the line is as long as the normal says");

	NormalLines_Clear(list);
	float const behind[3] = { 0.0f, 0.0f, -10.0f };
	NormalLines_AddCorner(list, behind, half, facing);
	Check(list.behindCamera == 1 && list.found[kNormalAgrees] == 0, "a corner behind the camera is left out and counted");

	NormalLines_Clear(list);
	float const onTheCamera[3] = { 0.0f, 0.0f, 0.0f };
	NormalLines_AddCorner(list, onTheCamera, half, facing);
	Check(list.behindCamera == 1, "and so is one right on it");

	NormalLines_Clear(list);
	float const farAway[3] = { 0.0f, 0.0f, kNormalLineFurthest + 1.0f };
	NormalLines_AddCorner(list, farAway, half, facing);
	Check(list.tooFar == 1 && list.found[kNormalAgrees] == 0, "a corner too far away is left out and counted");

	NormalLines_Clear(list);
	float const justNearEnough[3] = { 0.0f, 0.0f, kNormalLineFurthest - 1.0f };
	NormalLines_AddCorner(list, justNearEnough, half, facing);
	Check(list.tooFar == 0 && list.found[kNormalAgrees] == 1, "one just inside isn't");

	// An outline only needs one corner near. The far ones are still its corners.
	float const reachingOut[9] = { 0.0f, 0.0f, 1000.0f, 0.0f, 90000.0f, 1000.0f, 90000.0f, 0.0f, 1000.0f };
	NormalLines_Clear(list);
	NormalLines_AddOutline(list, reachingOut, 3);
	Check(list.found[kNormalOutOfPictures] == 1 && list.ends[kNormalOutOfPictures].size() == 6, "a big polygon with one corner near keeps its outline");

	float const allFar[9] = { 90000.0f, 0.0f, 1000.0f, 90000.0f, 100.0f, 1000.0f, 90100.0f, 0.0f, 1000.0f };
	NormalLines_Clear(list);
	NormalLines_AddOutline(list, allFar, 3);
	Check(list.tooFar == 1 && list.found[kNormalOutOfPictures] == 0, "and one with none near doesn't");
}

// The two routes have to agree, or the same wall would change colour with the route. So this makes a polygon in the world and sends it both ways, through a camera that isn't lined up with anything.
static void CheckTheRoutesAgree()
{
	// The same layout as ToyShadows_ViewMatrix: a line of four numbers for each of x, y and z.
	float const yaw = 0.6f, pitch = 0.3f;
	float const view[12] =
	{
		cosf(yaw), 0.0f, -sinf(yaw), 11.0f,
		sinf(yaw) * sinf(pitch), cosf(pitch), cosf(yaw) * sinf(pitch), -22.0f,
		sinf(yaw) * cosf(pitch), -sinf(pitch), cosf(yaw) * cosf(pitch), 1500.0f,
	};

	// The same layout as ToyShadows_WorldAxes
	float axes[9];

	for (int axis = 0; axis < 3; axis++)
	{
		axes[axis * 3 + 0] = view[0 + axis];
		axes[axis * 3 + 1] = -view[4 + axis];
		axes[axis * 3 + 2] = view[8 + axis];
	}

	float const world[9] = { 0.0f, 0.0f, 0.0f, 400.0f, 0.0f, 0.0f, 0.0f, 400.0f, 0.0f };
	double worldFacing[3];
	NormalLines_Facing(world, 3, worldFacing);

	float const length = (float)sqrt(worldFacing[0] * worldFacing[0] + worldFacing[1] * worldFacing[1] + worldFacing[2] * worldFacing[2]);
	float const out[3] = { (float)worldFacing[0] / length, (float)worldFacing[1] / length, (float)worldFacing[2] / length };

	float places[9];

	for (int i = 0; i < 3; i++)
	{
		NormalLines_WorldToList(view, world + i * 3, places + i * 3);
	}

	float normals[12];

	for (int i = 0; i < 3; i++)
	{
		normals[i * 4 + 0] = out[0];
		normals[i * 4 + 1] = out[1];
		normals[i * 4 + 2] = out[2];
		normals[i * 4 + 3] = 1.0f;
	}

	static NormalLinesList list;
	NormalLines_Clear(list);
	NormalLines_AddListPolygon(list, places, normals, 3, axes, false);
	Check(list.found[kNormalAgrees] == 3, "on the list, a normal out of the way its corners face agrees, through a mirror and a turned camera");

	float const worldNormals[9] = { out[0], out[1], out[2], out[0], out[1], out[2], out[0], out[1], out[2] };
	bool drawn[3] = { false, false, false };

	NormalLines_Clear(list);
	NormalLines_AddBufferTriangle(list, view, axes, world, worldNormals, true, drawn);
	Check(list.found[kNormalAgrees] == 3, "and from the fixed buffer it agrees too");
	Check(drawn[0] && drawn[1] && drawn[2], "and its corners are marked done");

	float const backwards[9] = { -out[0], -out[1], -out[2], -out[0], -out[1], -out[2], -out[0], -out[1], -out[2] };
	bool drawnAgain[3] = { false, false, false };
	NormalLines_Clear(list);
	NormalLines_AddBufferTriangle(list, view, axes, world, backwards, true, drawnAgain);
	Check(list.found[kNormalLeansBadly] == 3 && list.pastNinety == 3, "a normal the other way is past 90 on that route");

	for (int i = 0; i < 3; i++)
	{
		normals[i * 4 + 0] = -out[0];
		normals[i * 4 + 1] = -out[1];
		normals[i * 4 + 2] = -out[2];
	}

	NormalLines_Clear(list);
	NormalLines_AddListPolygon(list, places, normals, 3, axes, false);
	Check(list.found[kNormalLeansBadly] == 3 && list.pastNinety == 3, "and on the other");

	// That camera sees the polygon from one side or the other. Going round its corners the other way swaps which.
	float const turnedRound[9] = { world[0], world[1], world[2], world[6], world[7], world[8], world[3], world[4], world[5] };
	bool one[3] = { false, false, false };
	bool two[3] = { false, false, false };

	NormalLines_Clear(list);
	NormalLines_AddBufferTriangle(list, view, axes, world, worldNormals, false, one);
	int const backsOneWay = list.backs;

	NormalLines_Clear(list);
	NormalLines_AddBufferTriangle(list, view, axes, turnedRound, worldNormals, false, two);
	int const backsTheOtherWay = list.backs;

	Check(backsOneWay + backsTheOtherWay == 1, "a triangle with its back to the camera is dropped, in one direction only");
	Check((backsOneWay ? !one[0] : one[0]) && (backsTheOtherWay ? !two[0] : two[0]), "and a dropped one's corners aren't marked done");

	bool partly[3] = { true, false, true };
	NormalLines_Clear(list);
	NormalLines_AddBufferTriangle(list, view, axes, world, worldNormals, true, partly);
	Check(list.found[kNormalAgrees] == 1 && partly[1], "a corner another triangle did isn't done twice");

	float const inLine[9] = { 0.0f, 0.0f, 0.0f, 10.0f, 10.0f, 10.0f, 20.0f, 20.0f, 20.0f };
	bool notDrawn[3] = { false, false, false };
	NormalLines_Clear(list);
	NormalLines_AddBufferTriangle(list, view, axes, inLine, worldNormals, true, notDrawn);
	Check(list.noArea == 1 && !notDrawn[0], "a triangle with no area is left out and counted");

	// The way surfaceStatic.vsh puts a corner: cut towards zero, and y turned over
	float const straight[12] = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f };
	float const corner[3] = { 10.9f, 20.9f, -30.9f };
	float place[3];
	NormalLines_WorldToList(straight, corner, place);
	Check(Near(place[0], 10.0f, 0.0f) && Near(place[1], -20.0f, 0.0f) && Near(place[2], -30.0f, 0.0f), "a corner lands where the shader puts it");
}

static void CheckTheLimits()
{
	static NormalLinesList list;
	float const from[3] = { 0.0f, 0.0f, 1.0f };
	float const to[3] = { 0.0f, 0.0f, 2.0f };

	NormalLines_Clear(list);

	for (int i = 0; i < kNormalLinePlentyOfAgreeing; i++)
	{
		NormalLines_AddLine(list, kNormalAgrees, from, to);
	}

	Check(list.leftOut[kNormalAgrees] == 0, "a list that only just fills has left nothing out");
	Check(!NormalLines_AddLine(list, kNormalAgrees, from, to), "the next one is refused");
	Check((int)list.ends[kNormalAgrees].size() == kNormalLinePlentyOfAgreeing * 2, "and the list didn't grow");
	Check(list.leftOut[kNormalAgrees] == 1, "and it's counted");
	Check(NormalLines_AddLine(list, kNormalLeans, from, to), "a full list of agreeing lines doesn't push out the ones that matter");

	for (int i = 0; i <= kNormalLinePlentyOfTheRest; i++)
	{
		NormalLines_AddLine(list, kNormalLeansBadly, from, to);
	}

	Check((int)list.ends[kNormalLeansBadly].size() == kNormalLinePlentyOfTheRest * 2, "the others have a limit of their own");
	Check(NormalLines_StoppedShort(list, 0), "and stopping short is noticed");

	char line[600];
	NormalLines_Describe(list, 7, true, false, false, 3, line, sizeof(line));
	Check(Has(line, "STOPPED SHORT: 2 lines past the limit, 3 polygons not noted"), std::string("the log says so. It said: ") + line);

	NormalLines_Clear(list);
	Check(!NormalLines_StoppedShort(list, 0), "with nothing left out it hasn't stopped short");
	Check(NormalLines_StoppedShort(list, 1), "and polygons that didn't fit count as stopping short too");
	Check(list.ends[kNormalAgrees].empty() && list.leftOut[kNormalAgrees] == 0, "starting again forgets the lot");

	list.found[kNormalAgrees] = 11;
	list.found[kNormalLeans] = 22;
	list.found[kNormalLeansBadly] = 33;
	list.pastNinety = 4;
	list.found[kNormalIsZero] = 55;
	NormalLines_Describe(list, 7, true, false, false, 0, line, sizeof(line));
	Check(Has(line, "11 agree (under 5 degrees), 22 lean (5 to 45), 33 lean badly (past 45, and 4 of those past 90), 55 zero"), std::string("the log has the four counts. It said: ") + line);
	Check(!Has(line, "STOPPED SHORT"), "and doesn't cry wolf");
	Check(Has(line, "surface pass used the list alone"), "and says which route it read");

	NormalLines_Describe(list, 7, true, true, false, 0, line, sizeof(line));
	Check(Has(line, "surface pass used both routes"), "both routes are said");
	NormalLines_Describe(list, 7, false, false, false, 0, line, sizeof(line));
	Check(Has(line, "no surface pass this frame"), "and no pass at all");
	NormalLines_Describe(list, 7, true, false, true, 0, line, sizeof(line));
	Check(Has(line, "only the last part of the frame"), "and lists drawn early, because the pass only gets what was left");

	// A line too small for the words stays inside it. The rest of the buffer proves nothing was written past the size it was given.
	char small[32];
	char guarded[64];
	memset(guarded, 'x', sizeof(guarded));
	NormalLines_Describe(list, 7, true, false, false, 9, guarded, sizeof(small));
	Check(guarded[sizeof(small)] == 'x', "the log's line never runs past the room it was given");
}

static char const *NameForTest(int shapeNumber)
{
	return (shapeNumber == 7) ? "Body\nPart" : (shapeNumber == 8) ? "AVeryLongNameThatGoesOnAndOnAndOnAndOnAndOnAndOn" : 0;
}

static void CheckTheShapes()
{
	static NormalLinesShapes shapes;
	NormalLines_ForgetShapes(shapes);

	NormalLines_NoteShape(shapes, 1, -1, true, 0, 0, 0);
	Check(shapes.count == 0, "a polygon with nothing leaning isn't noted");

	NormalLines_NoteShape(shapes, 1, -1, true, 0, 1, 2);
	NormalLines_NoteShape(shapes, 7, 12, false, 3, 3, 0);
	NormalLines_NoteShape(shapes, 1, -1, true, 0, 1, 0);
	Check(shapes.count == 2, "each shape is noted once");
	Check(shapes.list[0].badly == 2 && shapes.list[0].leaning == 2, "and its polygons add up");

	NormalLines_NoteShape(shapes, 1, 5, true, 0, 0, 1);
	Check(shapes.count == 3, "the same shape on another kind of object is noted apart");

	NormalLines_NoteShape(shapes, 8, -1, true, 0, 9, 0);
	NormalLines_PutWorstFirst(shapes);
	Check(shapes.list[0].shapeNumber == 7, "the most past 90 goes first");
	Check(shapes.list[1].shapeNumber == 8, "then the most past 45");
	Check(shapes.list[2].shapeNumber == 1 && shapes.list[2].behaviour == -1, "then the most leaning");
	Check(shapes.list[3].shapeNumber == 1 && shapes.list[3].behaviour == 5, "and so on");

	char line[1000];
	NormalLines_DescribeShapes(shapes, NameForTest, 2, line, sizeof(line));
	Check(Has(line, "shape 7 \"Body?Part\", behaviour 12, NO POLYGON LIST: 3 past 90, 3 past 45, 0 lean;"), std::string("the log names the worst shape, and a line break in a name can't start a new line. It said: ") + line);
	Check(Has(line, "has a polygon list: 0 past 90, 9 past 45"), "it says which have the list");
	Check(!Has(line, "AVeryLongNameThatGoesOnAndOnAndOnAndOn"), "and a long name is cut short");
	Check(Has(line, "and 2 more shapes, 0 of them not kept"), "and it says how many it didn't name");

	NormalLines_DescribeShapes(shapes, NameForTest, 4, line, sizeof(line));
	Check(Has(line, "shape 1 \"\", behaviour -1"), "a shape with no name still has its number");
	Check(!Has(line, "more shapes"), "and with all of them named it doesn't say there are more");

	NormalLinesShapes nothing;
	NormalLines_ForgetShapes(nothing);
	NormalLines_DescribeShapes(nothing, NameForTest, 4, line, sizeof(line));
	Check(Has(line, "no shape on the game's list has a leaning corner"), "with none leaning it says that");

	char guarded[64];
	memset(guarded, 'x', sizeof(guarded));
	NormalLines_DescribeShapes(shapes, NameForTest, 4, guarded, 40);
	Check(guarded[40] == 'x', "the line never runs past the room it was given");

	NormalLines_ForgetShapes(shapes);

	for (int i = 0; i < kNormalLinesShapesKept + 3; i++)
	{
		NormalLines_NoteShape(shapes, i, -1, true, 1, 1, 0);
	}

	Check(shapes.count == kNormalLinesShapesKept, "the list of shapes has a limit");
	Check(shapes.notKept == 3, "and what doesn't fit is counted");
	NormalLines_DescribeShapes(shapes, NameForTest, 1, line, sizeof(line));
	Check(Has(line, "and 66 more shapes, 3 of them not kept"), "and said");

	std::string const render = ReadSourceFile("3dc/d3d_render.cpp");
	std::string const kshape = ReadSourceFile("3dc/kshape.cpp");
	std::string const chunks = ReadSourceFile("3dc/win95/chunk_load.cpp");

	Check(Has(render, "ToyShadows_WhatIsBeingDrawn(&polygon.shapeNumber, &polygon.behaviour, &polygon.hasPolygonList);"), "each polygon notes what it was drawn as");
	Check(Has(kshape, "*hasPolygonList = Global_ShapeHeaderPtr && Global_ShapeHeaderPtr->sh_surfacepolygons;"), "from the shape being drawn");
	Check(Has(render, "NormalLines_DescribeShapes(shapes, GetLoadedShapeName, kShapesInTheLog, shapesLine, sizeof(shapesLine));"), "the log names them when it says the counts");
	Check(Has(chunks, "ShapeInMSL const *const sim = (position >= 0) ? ShapeInMSL::GetByMSL(position) : 0;"), "and asking for a shape that can't exist gets no name");
}

static void CheckJ()
{
	enum { kOff, kFinal, kEdges, kNormals };
	int view = kNormals;
	int lines = 0;

	Check(View_StepNormalLines(&view, kOff, kFinal, kNormals, &lines) && lines == 1 && view == kFinal, "after NORMALS, J gets the lines on the real picture");
	Check(View_StepNormalLines(&view, kOff, kFinal, kNormals, &lines) && lines == 0 && view == kOff, "and after the lines, it comes round to OFF");
	Check(!View_StepNormalLines(&view, kOff, kFinal, kNormals, &lines) && lines == 0 && view == kOff, "OFF goes on as it always has");

	view = kFinal;
	Check(!View_StepNormalLines(&view, kOff, kFinal, kNormals, &lines) && lines == 0 && view == kFinal, "FINAL with no lines goes on as it always has");

	view = kEdges;
	Check(!View_StepNormalLines(&view, kOff, kFinal, kNormals, &lines) && lines == 0 && view == kEdges, "and so does any other");

	// LIGHTING and SHADOWS can take the view to OFF while the lines are on
	view = kOff;
	lines = 1;
	Check(View_StepNormalLines(&view, kOff, kFinal, kNormals, &lines) && lines == 0 && view == kOff, "J from there turns them off");
}

static void CheckTheGame()
{
	std::string const render = ReadSourceFile("3dc/d3d_render.cpp");
	std::string const normals = ReadSourceFile("3dc/surface_normals.cpp");
	std::string const pictures = ReadSourceFile("3dc/surface_pictures.cpp");
	std::string const card = ReadSourceFile("3dc/static_level_card.cpp");

	Check(!render.empty() && !normals.empty() && !pictures.empty() && !card.empty(), "the game's source can be read");

	std::string const draw = Between(render, "static void DrawNormalLines()\n{", "// The developer tools show what walls hide.");
	std::string const note = Between(render, "static void NoteCornersForNormalLines(", "// Writes down a polygon's normals");
	std::string const write = Between(normals, "void SurfaceNormals_WritePolygon(", "void SurfaceNormals_UseList()");
	std::string const shoot = Between(render, "static void KeepDeveloperToolsOutOfNetworkGames()", "static int viewToComeBackTo");

	Check(!draw.empty() && !note.empty() && !write.empty() && !shoot.empty(), "the pieces being looked at are there");

	// It shows the real numbers
	Check(Has(write, "*copyPlace++ = normal[0];") && Has(write, "*copyPlace++ = normal[1];") && Has(write, "*copyPlace++ = normal[2];") && Has(write, "*copyPlace++ = fourthNumber;"), "the copy holds the very numbers written to the card's list");
	Check(Has(write, "copyOfList + firstVertex * kNumbersPerVertex"), "and in the same places");
	Check(Has(normals, "memset(copyOfList, 0, verticesInList * kNumbersPerVertex * sizeof(float));"), "and it starts each list empty, like the card's");
	Check(Has(draw, "SurfaceNormals_TheCopy(&verticesInCopy)"), "the lines read that copy");
	Check(Has(draw, "if (polygon.firstVertex <= verticesInCopy && polygon.corners <= verticesInCopy - polygon.firstVertex)"), "and only inside it");
	Check(Has(draw, "if (!surfaceNormalsWereWrittenThisFrame)\n\t{\n\t\treturn;\n\t}\n\n\tuint32_t verticesInCopy = 0;"), "and never on a frame that wrote none, when it would still hold an older frame's numbers");
	Check(Has(render, "SurfaceNormals_KeepACopy(NormalLinesOn != 0);"), "the copy costs nothing unless the view's on");

	// It only reads what the surface pass reads
	Check(Has(note, "if (!NormalLinesOn || gunLayer || translucency != TRANSLUCENCY_OFF)"), "the weapon and see-through polygons aren't noted, because the pass doesn't draw them");
	Check(Has(note, "normalLinesPolygons.size() >= kNormalLinesMostPolygons || normalLinesCorners.size() / 3 + count > kNormalLinesMostCorners"), "the note has a limit");
	Check(Has(note, "normalLinesNotNoted++;"), "and what doesn't fit is counted");
	Check(Has(render, "normalLinesCorners.clear();\n\t\tnormalLinesPolygons.clear();"), "a list that's opened again starts its notes again, as the pass only gets what's in it");
	Check(Has(pictures, "lastDrawWasFromBuffer = fromBuffer;"), "the route the pass took is remembered");
	Check(Has(draw, "surfacePicturesAreFromThisFrame && SurfacePictures_LastDrawWasFromBuffer()"), "and the lines go by it");

	std::string const addBuffer = Between(render, "static void AddTheFixedBufferToNormalLines(", "static void DrawNormalLines()\n{");

	Check(Has(addBuffer, "SurfaceStatic_FrameRanges(&rangeCount)"), "the fixed buffer's lines come from the ranges the pass drew");
	Check(Has(addBuffer, "if (first + 3 > indexCount)") && Has(addBuffer, "if (place >= vertexCount)"), "and every number that says where to look is checked");
	Check(Has(card, "if (!DevMode)\n\t{\n\t\tstd::vector<StaticVertex>().swap(theLevel.vertices);"), "what the card was sent is only kept with -dev");

	// Each place that puts a solid polygon on the list notes it
	Check(Has(Between(render, "void D3D_ZBufferedGouraudTexturedPolygon_Output(", "void D3D_ZBufferedGouraudPolygon_Output("), "NoteCornersForNormalLines(renderVerticesPtr, RenderPolygon.NumberOfVertices, RenderPolygon.TranslucencyMode);"), "level polygons are noted");
	Check(Has(Between(render, "void D3D_ZBufferedGouraudPolygon_Output(", "void D3D_PredatorThermalVisionPolygon_Output("), "NoteCornersForNormalLines(renderVerticesPtr, RenderPolygon.NumberOfVertices, RenderPolygon.TranslucencyMode);"), "and ones with no texture");
	Check(Has(Between(render, "void D3D_PredatorThermalVisionPolygon_Output(", "void D3D_ZBufferedCloakedPolygon_Output("), "NoteCornersForNormalLines(renderVerticesPtr, RenderPolygon.NumberOfVertices, TRANSLUCENCY_OFF);"), "and thermal vision's");
	Check(Has(Between(render, "void D3D_SkyPolygon_Output(", "void D3D_DrawMoltenMetalMesh_Unclipped("), "NoteCornersForNormalLines(renderVerticesPtr, RenderPolygon.NumberOfVertices, RenderPolygon.TranslucencyMode);"), "and the sky's");

	// They stand in the level
	Check(Has(draw, "device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);"), "walls hide the lines");
	Check(Has(draw, "device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);"), "by the usual test");
	Check(Has(draw, "device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);"), "and the lines don't change what's in the depth buffer");
	Check(Has(draw, "D3DPT_LINELIST"), "they're lines");
	Check(Has(draw, "asItWas->Apply();") && Has(draw, "if (FAILED(device->CreateStateBlock(D3DSBT_ALL, &asItWas)))"), "what they change on the card is put back, and nothing is changed if it couldn't be saved");

	std::string const execute = Between(render, "static bool ExecuteBuffer()", "// draw player weapon");

	Check(Has(execute, "if (toyFinalFlush && NormalLinesOn)\n\t{\n\t\tDrawNormalLines();"), "they're drawn only in a frame's last drawing, and ahead of the weapon's clearing the depth buffer");

	// Developers only
	Check(Has(render, "if (DevKeysCanAct() && ViewSettings_KeyPressed(kViewKeyDevViews))"), "J still needs -dev");
	Check(Has(render, "DevMode && AvP.Network == I_No_Network && PlayerKeysCanAct()"), "and that still means -dev, and no network game");
	Check(Has(render, "View_StepNormalLines(&ToyShadowsMode, kToyViewOff, kToyViewFinal, kViewNormals, &NormalLinesOn)"), "the lines come after NORMALS");
	Check(Has(render, "int NormalLinesOn = 0;"), "and start off");
	Check(Has(shoot, "if (NormalLinesOn)\n\t{\n\t\tNormalLinesOn = 0;"), "a network game turns them off");
	Check(Has(draw, "if (!NormalLinesOn || ToyShadowsMode != kToyViewFinal || AvP.Network != I_No_Network)"), "and the drawing itself refuses one, because the card with no stencil never gets as far as turning them off");
	Check(Has(render, "\"VIEW: %s\", NormalLinesOn ? kNormalLinesName : kToyViewNames[ToyShadowsMode]"), "the screen says which view it is");

	// The log, on the first frame and when it stops short
	Check(Has(draw, "firstFrameOfTheView || toyFrame % kFramesBetweenReports == 0 || (stoppedShort && !saidItStoppedShort)"), "the view's first frame says its counts, and so does stopping short");
}

int main()
{
	// Nothing is held back, so that if this ever crashes, what it had got to is on the screen
	setvbuf(stdout, NULL, _IONBF, 0);

	CheckTheBands();
	CheckTheListRoute();
	CheckTheLengths();
	CheckTheRoutesAgree();
	CheckTheLimits();
	CheckTheShapes();
	CheckJ();
	CheckTheGame();

	if (checksFailed)
	{
		printf("FAILED: %d checks\n", checksFailed);
		return 1;
	}

	puts("all normal lines checks passed");
	return 0;
}
