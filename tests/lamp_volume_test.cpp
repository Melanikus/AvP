// The test for 3dc/lamp_volume.h: the shape of another marine's lamp beam, as Gouraud draws it. Written 2026-11-07 by Melanikus for the AvP port.
//
// The beam is made from rays that stop at the first wall each meets, joined into one closed shape. These checks try the rays, the meeting of a
// ray with a wall, the quick check that leaves most walls out, and the finished shape: that it's closed, that every corner number is one it has,
// and that its light fades the way the comments say.

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <utility>

#include "lamp_volume.h"

static int checksFailed = 0;

static void Check(bool good, const char *what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what);

	if (!good)
	{
		checksFailed++;
	}
}

// A square wall across the way ahead, z millimetres away, as two triangles, big enough to catch every ray
static void WallAhead(float z, LampVolumeTriangle *two)
{
	float const big = 100000.0f;
	LampVolumeTriangle const first = { { -big, -big, z }, { big, -big, z }, { big, big, z } };
	LampVolumeTriangle const second = { { -big, -big, z }, { big, big, z }, { -big, big, z } };

	two[0] = first;
	two[1] = second;
}

static void TestRays()
{
	float const aim[3] = { 0.0f, 0.0f, 1.0f };
	float rays[kLampVolumeRays][3];

	LampVolume_Rays(aim, rays);

	bool allLong = true, allInside = true, edgeOnEdge = true;

	for (int r = 0; r < kLampVolumeRays; r++)
	{
		float const length = sqrtf(LampVolume_Dot(rays[r], rays[r]));

		allLong = allLong && fabsf(length - 1.0f) < 0.001f;
		allInside = allInside && LampVolume_Dot(rays[r], aim) >= kLampVolumeConeCos - 0.0005f;
	}

	for (int r = kLampVolumeRays - kLampVolumeRing[kLampVolumeRingCount - 1]; r < kLampVolumeRays; r++)
	{
		edgeOnEdge = edgeOnEdge && fabsf(LampVolume_Dot(rays[r], aim) - kLampVolumeConeCos) < 0.0005f;
	}

	Check(kLampVolumeRays == 81, "81 rays: one in the middle and rings of 8, 16, 24 and 32");
	Check(LampVolume_Dot(rays[0], aim) > 0.9999f, "the first goes straight down the middle");
	Check(allLong, "every ray is 1 long");
	Check(allInside, "and none goes outside the beam");
	Check(edgeOnEdge, "and the last ring is the beam's edge");

	float const slanted[3] = { 0.6f, 0.0f, 0.8f };

	LampVolume_Rays(slanted, rays);
	Check(LampVolume_Dot(rays[0], slanted) > 0.9999f && LampVolume_Dot(rays[kLampVolumeRays - 1], slanted) > kLampVolumeConeCos - 0.0005f, "a lamp pointing another way has its rays round that way");
}

static void TestMeeting()
{
	float const from[3] = { 0.0f, 0.0f, 0.0f };
	float const ahead[3] = { 0.0f, 0.0f, 1.0f };
	LampVolumeTriangle wall[2];

	WallAhead(5000.0f, wall);
	Check(fabsf(LampVolume_FirstHit(from, ahead, kLampVolumeReach, wall, 2) - 5000.0f) < 0.5f, "a ray meets a wall 5 metres ahead 5 metres along");

	WallAhead(-5000.0f, wall);
	Check(LampVolume_FirstHit(from, ahead, kLampVolumeReach, wall, 2) == kLampVolumeReach, "a wall behind the lamp isn't met");

	WallAhead(20000.0f, wall);
	Check(LampVolume_FirstHit(from, ahead, kLampVolumeReach, wall, 2) == kLampVolumeReach, "nor one beyond its reach");

	WallAhead(5.0f, wall);
	Check(LampVolume_FirstHit(from, ahead, kLampVolumeReach, wall, 2) == kLampVolumeReach, "nor one closer than the nearest a meeting counts");

	LampVolumeTriangle aside = { { 1000.0f, 1000.0f, 3000.0f }, { 2000.0f, 1000.0f, 3000.0f }, { 1000.0f, 2000.0f, 3000.0f } };
	Check(LampVolume_FirstHit(from, ahead, kLampVolumeReach, &aside, 1) == kLampVolumeReach, "a triangle off to one side isn't met");

	// Two walls: the nearer counts, whichever comes first in the list
	LampVolumeTriangle both[4];

	WallAhead(8000.0f, both);
	WallAhead(3000.0f, both + 2);
	Check(fabsf(LampVolume_FirstHit(from, ahead, kLampVolumeReach, both, 4) - 3000.0f) < 0.5f, "of two walls, the nearer is met");

	// The same wall, wound the other way, is met all the same
	WallAhead(4000.0f, wall);
	LampVolumeTriangle flipped[2] = { { { wall[0].a[0], wall[0].a[1], wall[0].a[2] }, { wall[0].c[0], wall[0].c[1], wall[0].c[2] }, { wall[0].b[0], wall[0].b[1], wall[0].b[2] } },
		{ { wall[1].a[0], wall[1].a[1], wall[1].a[2] }, { wall[1].c[0], wall[1].c[1], wall[1].c[2] }, { wall[1].b[0], wall[1].b[1], wall[1].b[2] } } };
	Check(fabsf(LampVolume_FirstHit(from, ahead, kLampVolumeReach, flipped, 2) - 4000.0f) < 0.5f, "a wall facing away is met too");

	LampVolumeTriangle flat = { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
	Check(LampVolume_FirstHit(from, ahead, kLampVolumeReach, &flat, 1) == kLampVolumeReach, "a triangle with no size is never met");
	Check(LampVolume_FirstHit(from, ahead, kLampVolumeReach, 0, 5) == kLampVolumeReach, "and with no list, nothing is");
}

static void TestTheQuickCheck()
{
	float const from[3] = { 0.0f, 0.0f, 0.0f };
	float const ahead[3] = { 0.0f, 0.0f, 1.0f };
	LampVolumeTriangle wall[2];

	WallAhead(5000.0f, wall);
	Check(LampVolume_MayBeHit(wall[0], from, ahead, kLampVolumeReach), "a wall ahead may be met");
	Check(kLampVolumeConeCos > 0.0f, "the beam is narrower than 90 degrees each way, which the shortcut for walls behind the lamp needs");

	LampVolumeTriangle const behind = { { -100.0f, 0.0f, -3000.0f }, { 100.0f, 0.0f, -3000.0f }, { 0.0f, 100.0f, -3000.0f } };
	LampVolumeTriangle const far = { { -100.0f, 0.0f, 30000.0f }, { 100.0f, 0.0f, 30000.0f }, { 0.0f, 100.0f, 30000.0f } };
	LampVolumeTriangle const outside = { { 3000.0f, 0.0f, 3000.0f }, { 3100.0f, 0.0f, 3000.0f }, { 3000.0f, 100.0f, 3000.0f } };
	LampVolumeTriangle const touching = { { -50.0f, -50.0f, -10.0f }, { 50.0f, -50.0f, -10.0f }, { 0.0f, 50.0f, 10.0f } };

	Check(!LampVolume_MayBeHit(behind, from, ahead, kLampVolumeReach), "a small one behind the lamp is left out");
	Check(!LampVolume_MayBeHit(far, from, ahead, kLampVolumeReach), "and one beyond its reach");
	Check(!LampVolume_MayBeHit(outside, from, ahead, kLampVolumeReach), "and one well outside the beam");
	Check(LampVolume_MayBeHit(touching, from, ahead, kLampVolumeReach), "but one round the lamp itself is kept");

	// Whatever the quick check leaves out, no ray meets: tried over a field of small triangles in every direction
	bool neverWrong = true;

	for (int x = -10; x <= 10; x++)
	{
		for (int y = -10; y <= 10; y++)
		{
			for (int z = -2; z <= 12; z++)
			{
				float const cx = x * 700.0f, cy = y * 700.0f, cz = z * 1000.0f;
				LampVolumeTriangle const small = { { cx - 200.0f, cy - 200.0f, cz }, { cx + 200.0f, cy - 200.0f, cz }, { cx, cy + 200.0f, cz + 100.0f } };

				if (!LampVolume_MayBeHit(small, from, ahead, kLampVolumeReach))
				{
					float rays[kLampVolumeRays][3];

					LampVolume_Rays(ahead, rays);

					for (int r = 0; r < kLampVolumeRays; r++)
					{
						neverWrong = neverWrong && LampVolume_FirstHit(from, rays[r], kLampVolumeReach, &small, 1) == kLampVolumeReach;
					}
				}
			}
		}
	}

	Check(neverWrong, "nothing the quick check leaves out is met by any ray");
}

// A real lamp's beam is drawn as just its sides, and with the beams in the air off just its pool, so the sides have to come first in the list,
// and be made only of the lamp and their own corners
static void TestTheSidesComeFirst()
{
	float const from[3] = { 0.0f, 0.0f, 0.0f };
	float const aim[3] = { 0.0f, 0.0f, 1.0f };
	LampVolumeVertex vertices[kLampVolumeVertices];
	unsigned short indices[kLampVolumeTriangles * 3];
	bool sidesUseTheirOwn = true, poolUsesItsOwn = true;
	int const made = LampVolume_Build(from, aim, 1.0f, 0, 0, vertices, indices);

	for (int t = 0; t < made; t++)
	{
		for (int k = 0; k < 3; k++)
		{
			bool const sideCorner = indices[t * 3 + k] == 0 || indices[t * 3 + k] >= kLampVolumeFirstSideCorner;

			sidesUseTheirOwn = sidesUseTheirOwn && (t >= kLampVolumeSideTriangles || sideCorner);
			poolUsesItsOwn = poolUsesItsOwn && (t < kLampVolumeSideTriangles || !sideCorner);
		}
	}

	Check(made == kLampVolumeTriangles && sidesUseTheirOwn, "the sides come first, made of the lamp and their own corners");
	Check(poolUsesItsOwn, "and the pool, after them, of the rays' ends");
}

// The fades, each on its own
static void TestTheFades()
{
	Check(LampVolume_AlongFade(0.0f) == 0.0f, "the beam has no light at the lamp itself");
	Check(LampVolume_AlongFade(50.0f) < LampVolume_AlongFade(150.0f) && LampVolume_AlongFade(150.0f) < LampVolume_AlongFade(kLampVolumeFadeInTo), "and rises from there");
	Check(fabsf(LampVolume_AlongFade(kLampVolumeFadeInTo) - (1.0f - kLampVolumeFadeInTo / kLampVolumeReach) * (1.0f - kLampVolumeFadeInTo / kLampVolumeReach)) < 0.0001f, "to all of it, less what it has already lost on the way");
	Check(LampVolume_AlongFade(kLampVolumeFadeInTo) > LampVolume_AlongFade(1500.0f) && LampVolume_AlongFade(1500.0f) > LampVolume_AlongFade(4000.0f)
		&& LampVolume_AlongFade(4000.0f) > LampVolume_AlongFade(11000.0f) && LampVolume_AlongFade(11000.0f) > 0.0f, "then fades all along it");
	Check(LampVolume_AlongFade(1500.0f) - LampVolume_AlongFade(4000.0f) > LampVolume_AlongFade(8000.0f) - LampVolume_AlongFade(10500.0f), "faster near the lamp than far from it");
	Check(LampVolume_AlongFade(kLampVolumeReach) == 0.0f && LampVolume_AlongFade(kLampVolumeReach * 2.0f) == 0.0f, "to none at its reach and past it");
	Check(LampVolume_AlongFade(-100.0f) == 0.0f && LampVolume_AlongFade(nanf("")) == 0.0f, "and behind the lamp, or for something that isn't a number, there's none");

	Check(kLampVolumeStepAt[0] == kLampVolumeFadeInTo, "the sides' first corner is where the beam has risen to all its light");
	Check(kLampVolumeStepAt[0] < kLampVolumeStepAt[1] && kLampVolumeStepAt[1] < kLampVolumeStepAt[2] && kLampVolumeStepAt[2] < kLampVolumeReach, "and they go further out, one after another, inside its reach");

	Check(kLampVolumeConeCos < kLampVolumeFullInside && kLampVolumeFullInside < 1.0f, "the pool has all its light inside a narrower cone than the beam's edge");
	Check(LampVolume_RimFade(1.0f) == 1.0f && LampVolume_RimFade(kLampVolumeFullInside) == 1.0f, "in its middle the pool has all of it");
	Check(LampVolume_RimFade(kLampVolumeConeCos) < 0.000001f && LampVolume_RimFade(0.5f) == 0.0f, "at the edge, and outside it, none");
	Check(LampVolume_RimFade(0.96f) > 0.3f && LampVolume_RimFade(0.96f) < 0.7f, "and about half halfway between");

	float const across[3] = { 1.0f, 0.0f, 0.0f };
	float const towards[3] = { 0.0f, 0.0f, -1.0f };
	float const ahead[3] = { 0.0f, 0.0f, 5000.0f };
	float const atTheCamera[3] = { 0.0f, 0.0f, 0.0f };

	Check(LampVolume_Facing(towards, ahead) == 1.0f, "a side facing the camera faces it square on");
	Check(LampVolume_Facing(across, ahead) == 0.0f, "one seen edge on doesn't face it at all");
	Check(LampVolume_Facing(towards, atTheCamera) == 0.0f, "and one at the camera itself counts as not facing it");

	Check(kLampVolumeMostTogether >= 2.0f * kLampVolumeHaze && kLampVolumeMostTogether < 4.0f * kLampVolumeHaze,
		"the most beams add together is more than one beam's two layers, and less than two beams' four");
}

// A beam crossing in front of the camera, from left to right: its sides fade towards its edges, as the camera sees them
static void TestTheSideView()
{
	float const from[3] = { -3000.0f, 0.0f, 5000.0f };
	float const across[3] = { 1.0f, 0.0f, 0.0f };
	LampVolumeVertex vertices[kLampVolumeVertices];
	unsigned short indices[kLampVolumeTriangles * 3];

	LampVolume_Build(from, across, 1.0f, 0, 0, vertices, indices);

	// The corners 4 metres out along each edge ray: the third of each side's corners
	int brightest = 0, nearest = 0;
	float most = -1.0f, least = 2.0f;

	for (int i = 0; i < kLampVolumeEdgeRays; i++)
	{
		LampVolumeVertex const &corner = vertices[kLampVolumeFirstSideCorner + i * kLampVolumeSteps + 2];
		LampVolumeVertex const &nearestSoFar = vertices[kLampVolumeFirstSideCorner + nearest * kLampVolumeSteps + 2];

		if (corner.strength > most)
		{
			most = corner.strength;
			brightest = i;
		}

		least = corner.strength < least ? corner.strength : least;
		nearest = corner.place[2] < nearestSoFar.place[2] ? i : nearest;
	}

	Check(brightest == nearest, "the side nearest the camera is the brightest");
	Check(most > 0.8f * kLampVolumeHaze * LampVolume_AlongFade(4000.0f), "with nearly all the light there is that far along");
	Check(least < 0.05f * most, "and the beam's edges, as the camera sees them, have almost none");
}

// Which parts of a beam are drawn
static void TestTheParts()
{
	int first = -1;

	Check(LampVolume_Parts(kLampVolumeTriangles, true, true, &first) == kLampVolumeTriangles && first == 0, "a lamp that isn't a real light has its whole beam drawn");
	Check(LampVolume_Parts(kLampVolumeTriangles, true, false, &first) == kLampVolumeSideTriangles && first == 0, "a real lamp only its sides, because it lights the wall itself");
	Check(LampVolume_Parts(kLampVolumeTriangles, false, true, &first) == kLampVolumeTriangles - kLampVolumeSideTriangles && first == kLampVolumeSideTriangles,
		"with the beams in the air off, a lamp that isn't real keeps its pool, which is all the light it has");
	Check(LampVolume_Parts(kLampVolumeTriangles, false, false, &first) == 0, "and a real lamp then has nothing drawn");
	Check(LampVolume_Parts(0, true, true, &first) == 0, "a beam that wasn't made has nothing drawn");
}

static void TestTheShape()
{
	float const from[3] = { 0.0f, 0.0f, 3000.0f };
	float const ahead[3] = { 0.0f, 0.0f, 1.0f };
	LampVolumeTriangle wall[2];
	LampVolumeVertex vertices[kLampVolumeVertices];
	unsigned short indices[kLampVolumeTriangles * 3];

	WallAhead(9000.0f, wall);
	memset(indices, 0xff, sizeof(indices));

	int const made = LampVolume_Build(from, ahead, 1.0f, wall, 2, vertices, indices);

	Check(made == kLampVolumeTriangles, "the shape has all its triangles");

	bool inRange = true;

	for (int i = 0; i < made * 3; i++)
	{
		inRange = inRange && indices[i] < kLampVolumeVertices;
	}

	Check(inRange, "and every corner number is one of its corners");

	// Closed: every edge belongs to exactly two triangles. The sides have their own corners where they meet the pool, in exactly the same places,
	// so corners in the same place count as one. Only tried when every corner number is one of its corners, because it looks them up.
	int samePlaceAs[kLampVolumeVertices];

	for (int i = 0; i < kLampVolumeVertices; i++)
	{
		samePlaceAs[i] = i;

		for (int j = 0; j < i; j++)
		{
			if (memcmp(vertices[i].place, vertices[j].place, sizeof(vertices[i].place)) == 0)
			{
				samePlaceAs[i] = samePlaceAs[j];
				break;
			}
		}
	}

	std::map<std::pair<int, int>, int> edges;

	for (int t = 0; inRange && t < made; t++)
	{
		for (int k = 0; k < 3; k++)
		{
			int const a = samePlaceAs[indices[t * 3 + k]], b = samePlaceAs[indices[t * 3 + (k + 1) % 3]];

			edges[std::make_pair(a < b ? a : b, a < b ? b : a)]++;
		}
	}

	bool closed = inRange;

	for (auto const &edge : edges)
	{
		closed = closed && edge.second == 2;
	}

	Check(closed, "and it's closed: every edge is shared by exactly two triangles");

	bool sidesMeetThePool = true;

	for (int i = 0; i < kLampVolumeEdgeRays; i++)
	{
		sidesMeetThePool = sidesMeetThePool && samePlaceAs[kLampVolumeFirstSideCorner + i * kLampVolumeSteps + kLampVolumeSteps - 1] == 1 + kLampVolumeRays - kLampVolumeEdgeRays + i;
	}

	Check(sidesMeetThePool, "the sides end exactly where the pool's rim is, with no gap");

	bool noneFlat = true;

	for (int t = 0; t < made; t++)
	{
		noneFlat = noneFlat && indices[t * 3] != indices[t * 3 + 1] && indices[t * 3 + 1] != indices[t * 3 + 2] && indices[t * 3] != indices[t * 3 + 2];
	}

	Check(noneFlat, "and no triangle uses a corner twice");

	// The middle ray's end lies the pull-back short of the wall
	Check(fabsf(vertices[1].place[2] - (9000.0f - kLampVolumePullBack)) < 1.0f, "each end stops just short of the wall it meets");
	Check(vertices[0].place[2] == 3000.0f, "and it starts at the lamp");

	// The light: none at the lamp, and the pool's middle has the haze's strength, faded for how far the wall is, the lamp being fully on and well
	// away from the camera. Its rim has none, as the real light's spot has none at its edge.
	Check(vertices[0].strength == 0.0f, "the beam starts from nothing at the lamp");
	Check(fabsf(vertices[1].strength - kLampVolumeHaze * LampVolume_AlongFade(9000.0f - 3000.0f - kLampVolumePullBack)) < 0.0001f, "the pool's middle has the haze's light, faded for how far the wall is");

	bool rimHasNone = true;

	for (int i = 0; i < kLampVolumeEdgeRays; i++)
	{
		rimHasNone = rimHasNone && vertices[1 + kLampVolumeRays - kLampVolumeEdgeRays + i].strength < 0.000001f;
	}

	Check(rimHasNone, "and the pool fades out to nothing at its rim");

	LampVolumeVertex nearer[kLampVolumeVertices];

	WallAhead(5000.0f, wall);
	LampVolume_Build(from, ahead, 1.0f, wall, 2, nearer, indices);
	Check(nearer[1].strength > vertices[1].strength, "a pool on a nearer wall is brighter");

	// Up close to the camera it fades to nothing: a lamp just behind the camera, pointing the way it looks, has the first corners of its sides right by it
	float const atTheCamera[3] = { 0.0f, 0.0f, -300.0f };
	LampVolumeVertex close[kLampVolumeVertices];
	bool closeHasNone = true;
	int closeCorners = 0;

	LampVolume_Build(atTheCamera, ahead, 1.0f, wall, 2, close, indices);

	for (int i = 0; i < kLampVolumeVertices; i++)
	{
		if (sqrtf(LampVolume_Dot(close[i].place, close[i].place)) < kLampVolumeCameraFadeFrom)
		{
			closeHasNone = closeHasNone && close[i].strength == 0.0f;
			closeCorners++;
		}
	}

	Check(closeCorners > 1 && closeHasNone, "a beam right by the camera adds nothing there, so a beam you stand in doesn't fill the screen");

	// Half on, half as bright; off, nothing
	LampVolumeVertex half[kLampVolumeVertices];

	LampVolume_Build(from, ahead, 0.5f, wall, 2, half, indices);
	Check(fabsf(half[1].strength - nearer[1].strength * 0.5f) < 0.0001f, "a lamp half faded in is half as bright");
	LampVolume_Build(from, ahead, -3.0f, wall, 2, half, indices);

	bool offHasNone = true;

	for (int i = 0; i < kLampVolumeVertices; i++)
	{
		offHasNone = offHasNone && half[i].strength == 0.0f;
	}

	Check(offHasNone, "and a strength below nothing counts as off");

	// With nothing in the way, the beam goes its whole reach, and the far end has no light left
	LampVolume_Build(from, ahead, 1.0f, 0, 0, vertices, indices);
	Check(fabsf(vertices[1].place[2] - (3000.0f + kLampVolumeReach - kLampVolumePullBack)) < 1.0f && vertices[1].strength < 0.0001f, "with nothing in the way it reaches its full length, and fades out there");

	// An aim that isn't a direction makes nothing
	float const nowhere[3] = { 0.0f, 0.0f, 0.0f };
	float const notANumber[3] = { nanf(""), 0.0f, 1.0f };

	Check(LampVolume_Build(from, nowhere, 1.0f, wall, 2, vertices, indices) == 0, "an aim of no length makes no shape");
	Check(LampVolume_Build(from, notANumber, 1.0f, wall, 2, vertices, indices) == 0, "nor does one that isn't a number");
}

// What only the game can run, read from its source. /FC makes __FILE__ the whole path, which is how the files are found.
static std::string ReadSourceFile(const char *name)
{
	std::string path = __FILE__;
	std::string::size_type const tests = path.rfind("tests");
	std::string const root = tests == std::string::npos ? std::string() : path.substr(0, tests);
	std::ifstream file(root + name, std::ios::binary);
	std::stringstream text;

	text << file.rdbuf();

	std::string out = text.str();
	std::string::size_type at;

	while ((at = out.find("\r\n")) != std::string::npos)
	{
		out.erase(at, 1);
	}

	return out;
}

static bool Has(const std::string &text, const char *words)
{
	return text.find(words) != std::string::npos;
}

// The text from one piece of words up to the next, or nothing if either is missing
static std::string Between(const std::string &text, const char *from, const char *to)
{
	std::string::size_type const start = text.find(from);
	std::string::size_type const end = (start == std::string::npos) ? std::string::npos : text.find(to, start);

	return (end == std::string::npos) ? std::string() : text.substr(start, end - start);
}

// A wall a beam could meet, as one big triangle across its way, this far along it
static LampVolumeTriangle WallAcrossTheBeam(float z)
{
	LampVolumeTriangle const wall = { { -8000.0f, -8000.0f, z }, { 8000.0f, -8000.0f, z }, { 0.0f, 8000.0f, z } };

	return wall;
}

static void TestTheWallsNearABeam()
{
	puts("The walls near one beam, out of those kept for all of them:");

	float const from[3] = { 0.0f, 0.0f, 0.0f };
	float const aim[3] = { 0.0f, 0.0f, 1.0f };
	LampVolumeTriangle const walls[5] =
	{
		{ { 5000.0f, 0.0f, 100.0f }, { 5000.0f, 100.0f, 100.0f }, { 5000.0f, 0.0f, 200.0f } },         // beside the lamp, well outside the cone
		WallAcrossTheBeam(3000.0f),
		{ { -500.0f, -500.0f, -3000.0f }, { 500.0f, -500.0f, -3000.0f }, { 0.0f, 500.0f, -3000.0f } },     // behind the lamp, and small: the quick check keeps a big one it can't be sure of
		{ { 30000.0f, 0.0f, 0.0f }, { 30000.0f, 1000.0f, 0.0f }, { 30000.0f, 0.0f, 1000.0f } },         // further than a beam reaches
		WallAcrossTheBeam(4000.0f)
	};
	LampVolumeTriangle nearby[5];
	int const kept = LampVolume_KeepNear(walls, 5, from, aim, kLampVolumeReach, nearby, 5);

	Check(kept == 2, "only the two in its way are kept");
	Check(kept == 2 && nearby[0].a[2] == 3000.0f && nearby[1].a[2] == 4000.0f, "and in the order they came");

	LampVolumeVertex fromAll[kLampVolumeVertices], fromNearby[kLampVolumeVertices];
	unsigned short indices[kLampVolumeTriangles * 3];

	LampVolume_Build(from, aim, 1.0f, walls, 5, fromAll, indices);
	LampVolume_Build(from, aim, 1.0f, nearby, kept, fromNearby, indices);

	Check(memcmp(fromAll, fromNearby, sizeof(fromAll)) == 0, "a beam made from those is exactly the shape it is from all of them");
	Check(LampVolume_KeepNear(walls, 5, from, aim, kLampVolumeReach, nearby, 1) == 1, "no more are kept than there's room for");
	Check(LampVolume_KeepNear(walls, 5, from, aim, kLampVolumeReach, nearby, 0) == 0, "and none with no room");
	Check(LampVolume_KeepNear(0, 5, from, aim, kLampVolumeReach, nearby, 5) == 0, "no list means none");
	Check(LampVolume_KeepNear(walls, 5, from, aim, kLampVolumeReach, 0, 5) == 0, "and so does nowhere to put them");
}

static void TestTheGamesSide()
{
	std::string const shapes = ReadSourceFile("3dc/kshape.cpp");
	std::string const render = ReadSourceFile("3dc/d3d_render.cpp");

	// The user's rule for the view that draws as the original did: with no lamp shown, nothing changes
	Check(Has(shapes, "if (numLampBeams == 0 || !dptr->ObMyModule || gunLayer || DrawingAReflection || ToyShadowsInMirror"), "SOURCE: with no beam this frame, no walls are kept, and only the level's own walls ever are");
	Check(Has(render, "if (LampBeams_Count() == 0)\n\t{\n\t\treturn;\n\t}"), "SOURCE: and nothing is drawn, so the frame is drawn as before");
	Check(Has(shapes, "// The beams' walls first, in any view, before the shadows' own tests leave\n\tLampBeams_TakeWalls(dptr, shapePtr);"), "SOURCE: the walls are kept in any view, Gouraud included");
	Check(Has(shapes, "OtherLamps_Gather((float)NormalFrameTime / 65536.0f);\n\tLampBeams_ForThisFrame();"), "SOURCE: and the beams are this frame's lamps, worked out before the level is drawn");
	Check(Has(shapes, "if (pointNumber < 0 || pointNumber >= shapePtr->numpoints || pointNumber >= maxrotpts)\n\t\t\t{\n\t\t\t\tpointsAreSafe = false;"), "SOURCE: a corner number from the level's file is checked before it's used");
	Check(Has(shapes, "if (numLampBeamWalls >= kLampBeamMostWalls)"), "SOURCE: and no more walls are kept than there's room for");
	// Read in the beams' own drawing only: the file sets these same two things in other places, which would pass the check for it
	std::string const drawsBeams = Between(render, "static void DrawLampBeams()\n{", "// The mask is added to many times");

	Check(Has(drawsBeams, "device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);") && Has(drawsBeams, "device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);"), "SOURCE: a beam is added onto the picture and writes no depth");
	Check(Has(render, "\tsavedCardSettings->Apply();\n}\n\n// The mask is added to many times"), "SOURCE: and every card setting it changed is put back");
	Check(Has(render, "if (toyFinalFlush)\n\t{\n\t\tDrawLampBeams();\n\t}\n\n\t// draw player weapon"), "SOURCE: beams are drawn once a frame, before the gun");
	Check(Has(render, "LampBeams_Forget();\n\n\t// The game goes on whether or not this frame could be drawn"), "SOURCE: and they end with their frame, so the menus after a level don't draw them");
	Check(Has(shapes, "static void LampBeams_ForThisFrame()\n{\n\tLampBeams_Forget();"), "SOURCE: and each frame starts with none");
	Check(Has(shapes, "int const wallsNear = LampVolume_KeepNear(lampBeamWalls, numLampBeamWalls, beam.from, beam.aim, kLampVolumeReach, lampBeamWallsNearOne, kLampBeamMostWalls);\n\tint const triangles = LampVolume_Build(beam.from, beam.aim, beam.strength, lampBeamWallsNearOne, wallsNear, vertices, indices);"), "SOURCE: a beam's rays test only the walls near it");
	Check(Has(render, "FrameTiming_CountLampBeams(LampBeams_Count(), LampBeams_WallsKept(), LampBeams_WallsLeftOut());"), "SOURCE: and what the beams cost goes in the game's own timing report");
	Check(Has(render, "if (triangles <= 0 || first < 0 || first + triangles > kLampVolumeTriangles)"), "SOURCE: only triangles inside a beam's list are drawn");

	// Where beams cross: added up in a picture of their own, which stops at the most they may add together
	std::string const releases = Between(render, "static void ReleaseShadowPictures()", "void ToyShadows_ReleaseTargets()");
	std::string const makesIt = Between(render, "static bool LampBeams_MakePicture(", "// The beams' picture, added onto the whole screen once");

	Check(Has(releases, "ToyRelease(lampBeamSurface);") && Has(releases, "ToyRelease(lampBeamTexture);") && Has(releases, "lampBeamPictureFailed = false;"), "SOURCE: the beams' picture goes before the device resets, and is tried again after");
	Check(Has(makesIt, "depthSize.Width >= screenSize.Width && depthSize.Height >= screenSize.Height"), "SOURCE: it's only used with a depth buffer as big as it");
	Check(Has(makesIt, "depthSize.MultiSampleType == D3DMULTISAMPLE_NONE && screenSize.MultiSampleType == D3DMULTISAMPLE_NONE"), "SOURCE: and neither anti-aliased, which a card can't share a depth buffer with");
	Check(Has(makesIt, "pictureSize.Width != screenSize.Width || pictureSize.Height != screenSize.Height"), "SOURCE: and it's made again if the screen changes size");
	Check(Has(makesIt, "lampBeamTexture = texture;\n\t\t\tlampBeamSurface = surface;\n\t\t\treturn true;"), "SOURCE: and only kept once it's all there");
	Check(Has(render, "float const share = usingThePicture ? shape[i].strength / kLampVolumeMostTogether : shape[i].strength;"), "SOURCE: in the picture each corner is a share of the most beams add together");
	Check(Has(render, "int const most[3] = { (int)(red * kLampVolumeMostTogether + 0.5f), (int)(green * kLampVolumeMostTogether + 0.5f), (int)(blue * kLampVolumeMostTogether + 0.5f) };"), "SOURCE: and the picture is added on in the lamp's colour, at that most");
	Check(Has(render, "\tif (usingThePicture)\n\t{\n\t\tdevice->SetRenderTarget(0, screen);\n\t\tdevice->SetViewport(&view);\n\t\tLampBeams_AddPictureToScreen(device, red, green, blue);\n\t}\n\n\tToyRelease(screen);\n\tsavedCardSettings->Apply();"),
		"SOURCE: the screen is drawn on again before the gun, and let go of");
	Check(Has(render, "device->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);\n\t\t\tdevice->SetViewport(&view);"), "SOURCE: the picture starts empty, with the screen's own view");

	// Other marines' lamps as real lights
	std::string const realLamps = Between(shapes, "static void ConsiderTheOtherLamps", "// Chooses this frame's lights.");

	Check(Has(shapes, "ConsiderTheShoulderLamp(steady, &numSteady, kLightChoiceMostCandidates);\n\tConsiderTheOtherLamps(steady, &numSteady, kLightChoiceMostCandidates);"), "SOURCE: the nearest lamps are put forward as real lights");
	Check(Has(realLamps, "if (!ToyShadowsLightsByPixel || !ToyShadows_VolumesInThisView() || !ShoulderLamp_RealLightIsWanted())"), "SOURCE: only with lights per pixel, shadows on, and the player's say-so: with no shadows a lamp would show what's behind a wall");
	Check(Has(realLamps, "numOtherLampLights < kLampMostReal"), "SOURCE: and only as many as are allowed");
	Check(!Has(realLamps, "AddLightBlock"), "SOURCE: never in the game's own lights, which a marine's hit roll and the player's stealth read");
	Check(Has(shapes, "|| OtherLampLightNumber(light) >= 0;"), "SOURCE: they come ahead of the level's lamps");
	Check(Has(shapes, "out[kToyLightAim + 3] = 1.0f;"), "SOURCE: and each is aimed its own way");
	Check(Has(shapes, "bool const pool = !otherLampIsALight[beam.lamp];"), "SOURCE: a lamp that's a real light doesn't light the wall twice");
	Check(Has(shapes, "bool const air = !ToyShadowsLightsByPixel || ShoulderLamp_BeamsAreWanted();"), "SOURCE: the beams in the air go only with lights per pixel, and only if the player turns them off");
	Check(Has(shapes, "return LampVolume_Parts(triangles, air, pool, first);"), "SOURCE: and those are the parts drawn");

	// The shader's spot, which the pool's rim fades like
	std::string const shader = ReadSourceFile("shaders/lightShapes.fxh");

	Check(Has(shader, "static const float kLampFullInside = 0.99f;") && kLampVolumeFullInside == 0.99f, "SOURCE: the pool has all its light inside the same cone as the real light");
	Check(Has(shader, "static const float kLampNothingOutside = 0.93f;") && kLampVolumeConeCos == 0.93f, "SOURCE: and none outside the same edge");
}

int main()
{
	printf("The shape of another marine's lamp beam:\n");

	TestRays();
	TestMeeting();
	TestTheQuickCheck();
	TestTheShape();
	TestTheSidesComeFirst();
	TestTheFades();
	TestTheSideView();
	TestTheParts();
	TestTheWallsNearABeam();
	TestTheGamesSide();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");

	return checksFailed ? 1 : 0;
}
