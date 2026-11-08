// The beam of another marine's lamp, as a shape: a faint cone of light from the lamp to the first thing each part of it meets. Written 2026-11-07
// by Melanikus for the AvP port.
//
// HOW IT'S MADE. Rays go out from the lamp, spread evenly over the beam: one down the middle, and rings of them round it, out to the beam's edge.
// Each stops at the first wall it meets. Joining the ends of neighbouring rays makes the pool of light on the wall, and the rays round the edge
// make the cone's sides, which are the beam you see in the air. Together they're one closed shape. This is the Gouraud view's beam: it needs no
// shader and no stencil, only colours at the corners. It isn't a shadow volume, but it's made from the same walls the shadows are.
//
// HOW IT'S LIT, which the user asked to be soft and faint. The beam rises from nothing just in front of the lamp, then fades along its length. Its
// sides fade with how squarely they face the camera, so the beam's edges fade to nothing: seen square on you look through the whole beam, and
// edge on through none of it. The pool fades out at its rim the way the real light's spot does. Close to the camera it all fades to nothing, so
// walking into a beam doesn't fill the screen. Where beams cross, the light they add together stops at kLampVolumeMostTogether.
//
// Everything is in the view's numbers, with the camera at 0 0 0, and in millimetres. Nothing here knows about the game, so tests/lamp_volume_test.cpp
// tries all of it.

#ifndef LAMP_VOLUME_H
#define LAMP_VOLUME_H

#include <math.h>

// The beam's edge, as the cosine of the angle from its middle: the same as kLampNothingOutside in shaders/lightShapes.fxh, so the beam drawn here
// is as wide as the light it stands for.
static const float kLampVolumeConeCos = 0.93f;

// How far a beam reaches at most, in millimetres
static const float kLampVolumeReach = 12000.0f;

// How far each end is pulled back from the wall it meets, in millimetres, so the pool lies just in front of the wall and isn't lost in it
static const float kLampVolumePullBack = 30.0f;

// A ray meeting something closer than this to the lamp, in millimetres, is taken to have met the marine carrying it, and goes on past it
static const float kLampVolumeNearestHit = 10.0f;

// How strong the beam is where it's strongest, as a share of the lamp's colour: a little way out from the lamp, in the middle of the beam, seen
// square on. It's faint: a beam you can see in the air, not a solid shape. The first film had 0.25, and the user asked for it softer and fainter.
static const float kLampVolumeHaze = 0.12f;

// Inside this, as the cosine of the angle from the beam's middle, the pool on the wall has all its light, and it fades out to nothing at the edge
// (kLampVolumeConeCos). The same as kLampFullInside in shaders/lightShapes.fxh, so the pool fades the way the real light's spot does.
static const float kLampVolumeFullInside = 0.99f;

// The beam rises from nothing at the lamp to all its light this far out, in millimetres, so it doesn't start as a hard bright point
static const float kLampVolumeFadeInTo = 250.0f;

// The most light all the beams together add to any one pixel, as a share of the lamp's colour. A beam seen side on is two layers of haze, its near
// side and its far side, so this is a little more than one beam's middle: where two beams cross it's hardly brighter than one. DrawLampBeams in
// d3d_render.cpp sees to it, when the card can make the picture that needs.
static const float kLampVolumeMostTogether = 0.3f;

// Closer to the camera than the first of these, a corner has no light at all, and from the second on, all of it, in millimetres
static const float kLampVolumeCameraFadeFrom = 300.0f;
static const float kLampVolumeCameraFadeTo = 1500.0f;

// The rays: one in the middle, then rings of these many, from the middle out to the edge. The pool is made from all their ends. The sides have
// corners of their own along each ray of the last ring, so that their light can fade along the beam in a curve, and not in a straight line.
enum
{
	kLampVolumeRingCount = 4,
	kLampVolumeRays = 1 + 8 + 16 + 24 + 32,
	kLampVolumeEdgeRays = 32,                                                  // the last ring: the beam's edge
	kLampVolumeSteps = 4,                                                      // corners along each side: three on the way, then where its ray ends
	kLampVolumeFirstSideCorner = 1 + kLampVolumeRays,                          // the lamp, then the end of each ray, then the sides' own corners
	kLampVolumeVertices = kLampVolumeFirstSideCorner + kLampVolumeEdgeRays * kLampVolumeSteps,
	kLampVolumeSideTriangles = kLampVolumeEdgeRays * (1 + 2 * (kLampVolumeSteps - 1)),   // the sides come first in the list, then the pool, so either can be drawn alone
	kLampVolumeTriangles = kLampVolumeSideTriangles + 8 + (8 + 16) + (16 + 24) + (24 + 32)   // the pool: the middle to the first ring, and between each pair of rings
};

static const int kLampVolumeRing[kLampVolumeRingCount] = { 8, 16, 24, 32 };

// How far from the lamp the sides' corners are, in millimetres, before the last one, which is where its ray ends. Closer together near the lamp,
// where the light changes fastest. The first is where the beam has risen to all its light, kLampVolumeFadeInTo. A ray that ends sooner has the
// rest of them at its end.
static const float kLampVolumeStepAt[kLampVolumeSteps - 1] = { 250.0f, 1500.0f, 4000.0f };

struct LampVolumeTriangle
{
	float a[3], b[3], c[3];
};

struct LampVolumeVertex
{
	float place[3];
	float strength;   // from 0 to 1, the share of the beam's colour at this corner
};

static inline float LampVolume_Dot(const float *a, const float *b)
{
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

// Two directions at right angles to aim, and to each other, for laying the rings out round it
static inline void LampVolume_Square(const float aim[3], float across[3], float up[3])
{
	// Whichever of the three axes aim is least along gives the steadiest square
	float const ax = fabsf(aim[0]), ay = fabsf(aim[1]), az = fabsf(aim[2]);
	float const helper[3] = { (ax <= ay && ax <= az) ? 1.0f : 0.0f, (ay < ax && ay <= az) ? 1.0f : 0.0f, (az < ax && az < ay) ? 1.0f : 0.0f };

	across[0] = aim[1] * helper[2] - aim[2] * helper[1];
	across[1] = aim[2] * helper[0] - aim[0] * helper[2];
	across[2] = aim[0] * helper[1] - aim[1] * helper[0];

	float const length = sqrtf(LampVolume_Dot(across, across));

	across[0] /= length;
	across[1] /= length;
	across[2] /= length;

	up[0] = aim[1] * across[2] - aim[2] * across[1];
	up[1] = aim[2] * across[0] - aim[0] * across[2];
	up[2] = aim[0] * across[1] - aim[1] * across[0];
}

// Where each ray points, 1 long, round aim, which must be 1 long too. The middle one first, then each ring, starting from the same side.
// The rings are evenly spread in angle out to the beam's edge, so the last ring is the edge.
static inline void LampVolume_Rays(const float aim[3], float out[kLampVolumeRays][3])
{
	float across[3], up[3];
	float const edge = acosf(kLampVolumeConeCos);
	int ray = 0;

	LampVolume_Square(aim, across, up);

	out[ray][0] = aim[0];
	out[ray][1] = aim[1];
	out[ray][2] = aim[2];
	ray++;

	for (int ring = 0; ring < kLampVolumeRingCount; ring++)
	{
		float const outward = edge * (float)(ring + 1) / (float)kLampVolumeRingCount;
		float const along = cosf(outward), off = sinf(outward);

		for (int i = 0; i < kLampVolumeRing[ring]; i++)
		{
			float const round = 6.2831853f * (float)i / (float)kLampVolumeRing[ring];
			float const c = cosf(round) * off, s = sinf(round) * off;

			out[ray][0] = aim[0] * along + across[0] * c + up[0] * s;
			out[ray][1] = aim[1] * along + across[1] * c + up[1] * s;
			out[ray][2] = aim[2] * along + across[2] * c + up[2] * s;
			ray++;
		}
	}
}

// How far along a ray, from `from` in `direction` (1 long), it first meets one of the triangles, either side of it. reach if it meets none
// nearer. A meeting closer than kLampVolumeNearestHit doesn't count.
static inline float LampVolume_FirstHit(const float from[3], const float direction[3], float reach, const LampVolumeTriangle *triangles, int count)
{
	float nearest = reach;

	for (int t = 0; triangles && t < count; t++)
	{
		LampVolumeTriangle const &tri = triangles[t];
		float const e1[3] = { tri.b[0] - tri.a[0], tri.b[1] - tri.a[1], tri.b[2] - tri.a[2] };
		float const e2[3] = { tri.c[0] - tri.a[0], tri.c[1] - tri.a[1], tri.c[2] - tri.a[2] };
		float const p[3] = { direction[1] * e2[2] - direction[2] * e2[1], direction[2] * e2[0] - direction[0] * e2[2], direction[0] * e2[1] - direction[1] * e2[0] };
		float const det = LampVolume_Dot(e1, p);

		// The ray runs along the triangle's flat, or it has no size
		if (fabsf(det) < 1e-6f)
		{
			continue;
		}

		float const inverse = 1.0f / det;
		float const s[3] = { from[0] - tri.a[0], from[1] - tri.a[1], from[2] - tri.a[2] };
		float const u = LampVolume_Dot(s, p) * inverse;

		if (u < 0.0f || u > 1.0f)
		{
			continue;
		}

		float const q[3] = { s[1] * e1[2] - s[2] * e1[1], s[2] * e1[0] - s[0] * e1[2], s[0] * e1[1] - s[1] * e1[0] };
		float const v = LampVolume_Dot(direction, q) * inverse;

		if (v < 0.0f || u + v > 1.0f)
		{
			continue;
		}

		float const along = LampVolume_Dot(e2, q) * inverse;

		if (along >= kLampVolumeNearestHit && along < nearest)
		{
			nearest = along;
		}
	}

	return nearest;
}

// Whether a triangle could be met by any ray of the beam: not all of it behind the lamp, not all beyond its reach, and not wholly outside the
// cone. Only a quick check, which keeps any triangle it isn't sure of, so that the rays only test the few near the beam.
static inline bool LampVolume_MayBeHit(const LampVolumeTriangle &tri, const float from[3], const float aim[3], float reach)
{
	float const centre[3] = { (tri.a[0] + tri.b[0] + tri.c[0]) / 3.0f - from[0], (tri.a[1] + tri.b[1] + tri.c[1]) / 3.0f - from[1], (tri.a[2] + tri.b[2] + tri.c[2]) / 3.0f - from[2] };
	float radius = 0.0f;
	const float *corners[3] = { tri.a, tri.b, tri.c };

	for (int i = 0; i < 3; i++)
	{
		float const d[3] = { corners[i][0] - from[0] - centre[0], corners[i][1] - from[1] - centre[1], corners[i][2] - from[2] - centre[2] };
		float const r = sqrtf(LampVolume_Dot(d, d));

		radius = r > radius ? r : radius;
	}

	float const away = sqrtf(LampVolume_Dot(centre, centre));

	// Wholly beyond the reach, or wholly behind the lamp. The second is only a shortcut: the test of the cone below would leave those out as well, but
	// that's two slow sums, and about half the walls are behind a lamp. That's only true of a cone narrower than 90 degrees each way. kLampVolumeConeCos
	// is fixed here in the code, with no file, setting or key to change it, and lamp_volume_test fails if it's ever made that wide.
	if (away - radius > reach || LampVolume_Dot(centre, aim) < -radius)
	{
		return false;
	}

	// Close enough to the lamp to touch it: it might be met by any ray
	if (away <= radius)
	{
		return true;
	}

	// Outside the cone by more than its own size: the angle to its middle, less the angle its size covers, is past the edge
	float const toCentre = acosf(fmaxf(-1.0f, fminf(1.0f, LampVolume_Dot(centre, aim) / away)));
	float const itsSize = asinf(fminf(1.0f, radius / away));

	return toCentre - itsSize <= acosf(kLampVolumeConeCos);
}

// Copies the walls one beam could meet from a list kept for all of them, and gives back how many, at most `most`. A wall is only kept if any beam
// might meet it, so most of the list is often no use to this one, and each of a beam's rays would otherwise test every wall. walls and out can't be
// the same list.
static inline int LampVolume_KeepNear(const LampVolumeTriangle *walls, int count, const float from[3], const float aim[3], float reach, LampVolumeTriangle *out, int most)
{
	int kept = 0;

	for (int t = 0; walls && out && t < count && kept < most; t++)
	{
		if (LampVolume_MayBeHit(walls[t], from, aim, reach))
		{
			out[kept++] = walls[t];
		}
	}

	return kept;
}

// How much of the beam's light a corner this far from the camera gets: none up close, all of it from kLampVolumeCameraFadeTo on, and smoothly between
static inline float LampVolume_CameraFade(float distance)
{
	float const x = (distance - kLampVolumeCameraFadeFrom) / (kLampVolumeCameraFadeTo - kLampVolumeCameraFadeFrom);
	float const t = x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x;

	return t * t * (3.0f - 2.0f * t);
}

// How much of the beam's light there is this far from the lamp along it, in millimetres: none at the lamp itself, rising to all of it at
// kLampVolumeFadeInTo, then less and less, faster at first, to none at kLampVolumeReach. Something that isn't a number gets none.
static inline float LampVolume_AlongFade(float along)
{
	if (!(along > 0.0f) || !(along < kLampVolumeReach))
	{
		return 0.0f;
	}

	float const in = along >= kLampVolumeFadeInTo ? 1.0f : along / kLampVolumeFadeInTo;
	float const left = 1.0f - along / kLampVolumeReach;

	return in * left * left;
}

// How much of the pool's light there is at this angle from the beam's middle, given as its cosine: all of it inside kLampVolumeFullInside, none
// at the edge, and smoothly between, the same smooth step the shader uses for the real light
static inline float LampVolume_RimFade(float cosFromMiddle)
{
	float const x = (cosFromMiddle - kLampVolumeConeCos) / (kLampVolumeFullInside - kLampVolumeConeCos);
	float const t = x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x;

	return t * t * (3.0f - 2.0f * t);
}

// How squarely a part of the beam's side faces the camera, from 0 edge on to 1 square on. side is the way that side faces out, 1 long, and place
// is where that part is, with the camera at 0 0 0. Seen square on, the camera looks through the whole width of the beam there, and edge on
// through none of it.
static inline float LampVolume_Facing(const float side[3], const float place[3])
{
	float const distance = sqrtf(LampVolume_Dot(place, place));

	if (!(distance > 1.0f))
	{
		return 0.0f;
	}

	float const square = fabsf(LampVolume_Dot(side, place)) / distance;

	return square > 1.0f ? 1.0f : square;
}

// Joins two rings of ray ends, each starting from the same side, into a band of triangles: as many as the two rings have ends together.
// firstA and firstB are the vertex numbers of each ring's first end.
static inline int LampVolume_Band(int firstA, int countA, int firstB, int countB, unsigned short *indices)
{
	int i = 0, j = 0, made = 0;

	while (i < countA || j < countB)
	{
		// Step along whichever ring's next end comes round first
		bool const stepA = j >= countB || (i < countA && (float)(i + 1) / (float)countA <= (float)(j + 1) / (float)countB);

		indices[made * 3 + 0] = (unsigned short)(firstA + i % countA);
		indices[made * 3 + 1] = (unsigned short)(stepA ? firstA + (i + 1) % countA : firstB + (j + 1) % countB);
		indices[made * 3 + 2] = (unsigned short)(firstB + j % countB);
		made++;

		if (stepA)
		{
			i++;
		}
		else
		{
			j++;
		}
	}

	return made;
}

// Makes the beam's shape: kLampVolumeVertices corners and kLampVolumeTriangles triangles, three vertex numbers each. from is the lamp, aim the way
// it points (1 long), lampStrength how far on it is, from 0 to 1. Gives back how many triangles it made: kLampVolumeTriangles, or 0 if aim isn't
// a direction.
static inline int LampVolume_Build(const float from[3], const float aim[3], float lampStrength, const LampVolumeTriangle *triangles, int count, LampVolumeVertex *vertices, unsigned short *indices)
{
	float const aimLength = sqrtf(LampVolume_Dot(aim, aim));

	if (!vertices || !indices || !(aimLength > 0.5f && aimLength < 1.5f))
	{
		return 0;
	}

	float const strength = lampStrength < 0.0f ? 0.0f : lampStrength > 1.0f ? 1.0f : lampStrength;
	float rays[kLampVolumeRays][3];
	float lengths[kLampVolumeRays];

	LampVolume_Rays(aim, rays);

	// The lamp itself, where the beam starts from nothing: see LampVolume_AlongFade
	vertices[0].place[0] = from[0];
	vertices[0].place[1] = from[1];
	vertices[0].place[2] = from[2];
	vertices[0].strength = 0.0f;

	// The end of each ray, pulled back from what it meets: the pool of light on the wall. It fades with how far it is from the lamp, and out to
	// nothing at its rim.
	for (int r = 0; r < kLampVolumeRays; r++)
	{
		float const reach = LampVolume_FirstHit(from, rays[r], kLampVolumeReach, triangles, count);
		LampVolumeVertex &end = vertices[1 + r];

		lengths[r] = reach > kLampVolumePullBack ? reach - kLampVolumePullBack : reach;
		end.place[0] = from[0] + rays[r][0] * lengths[r];
		end.place[1] = from[1] + rays[r][1] * lengths[r];
		end.place[2] = from[2] + rays[r][2] * lengths[r];
		end.strength = kLampVolumeHaze * strength * LampVolume_AlongFade(lengths[r]) * LampVolume_RimFade(LampVolume_Dot(rays[r], aim))
			* LampVolume_CameraFade(sqrtf(LampVolume_Dot(end.place, end.place)));
	}

	// The sides' corners, along each ray round the edge. Their light fades along the beam, and with how squarely they face the camera, squared,
	// so that it fades well before the very edge, as a beam brightest in its middle does. The last corner of each is exactly where its ray ends,
	// so the sides meet the pool with no gap.
	for (int i = 0; i < kLampVolumeEdgeRays; i++)
	{
		int const ray = kLampVolumeRays - kLampVolumeEdgeRays + i;
		float const along = LampVolume_Dot(rays[ray], aim);

		// Which way the side faces out there: square to the ray, away from the beam's middle
		float side[3] = { rays[ray][0] * along - aim[0], rays[ray][1] * along - aim[1], rays[ray][2] * along - aim[2] };
		float const sideLength = sqrtf(LampVolume_Dot(side, side));

		for (int k = 0; k < 3; k++)
		{
			side[k] = sideLength > 0.000001f ? side[k] / sideLength : 0.0f;
		}

		for (int step = 0; step < kLampVolumeSteps; step++)
		{
			float const out = (step + 1 < kLampVolumeSteps && kLampVolumeStepAt[step] < lengths[ray]) ? kLampVolumeStepAt[step] : lengths[ray];
			LampVolumeVertex &corner = vertices[kLampVolumeFirstSideCorner + i * kLampVolumeSteps + step];

			corner.place[0] = from[0] + rays[ray][0] * out;
			corner.place[1] = from[1] + rays[ray][1] * out;
			corner.place[2] = from[2] + rays[ray][2] * out;

			float const facing = LampVolume_Facing(side, corner.place);

			corner.strength = kLampVolumeHaze * strength * LampVolume_AlongFade(out) * facing * facing * LampVolume_CameraFade(sqrtf(LampVolume_Dot(corner.place, corner.place)));
		}
	}

	int made = 0;
	int const firstOfRing[kLampVolumeRingCount] = { 2, 2 + 8, 2 + 8 + 16, 2 + 8 + 16 + 24 };

	// The sides: from the lamp to the first corner along each pair of neighbouring edge rays, then a band at a time out to where those rays end
	for (int i = 0; i < kLampVolumeEdgeRays; i++)
	{
		int const here = kLampVolumeFirstSideCorner + i * kLampVolumeSteps;
		int const next = kLampVolumeFirstSideCorner + ((i + 1) % kLampVolumeEdgeRays) * kLampVolumeSteps;

		indices[made * 3 + 0] = 0;
		indices[made * 3 + 1] = (unsigned short)here;
		indices[made * 3 + 2] = (unsigned short)next;
		made++;

		for (int step = 0; step + 1 < kLampVolumeSteps; step++)
		{
			indices[made * 3 + 0] = (unsigned short)(here + step);
			indices[made * 3 + 1] = (unsigned short)(here + step + 1);
			indices[made * 3 + 2] = (unsigned short)(next + step + 1);
			made++;

			indices[made * 3 + 0] = (unsigned short)(here + step);
			indices[made * 3 + 1] = (unsigned short)(next + step + 1);
			indices[made * 3 + 2] = (unsigned short)(next + step);
			made++;
		}
	}

	// The pool: the middle ray's end to the first ring, then a band between each ring and the next
	for (int i = 0; i < kLampVolumeRing[0]; i++)
	{
		indices[made * 3 + 0] = 1;
		indices[made * 3 + 1] = (unsigned short)(firstOfRing[0] + (i + 1) % kLampVolumeRing[0]);
		indices[made * 3 + 2] = (unsigned short)(firstOfRing[0] + i);
		made++;
	}

	for (int ring = 0; ring + 1 < kLampVolumeRingCount; ring++)
	{
		made += LampVolume_Band(firstOfRing[ring], kLampVolumeRing[ring], firstOfRing[ring + 1], kLampVolumeRing[ring + 1], &indices[made * 3]);
	}

	return made;
}

// Which of a made beam's triangles to draw: its sides, which are the beam in the air, its pool on the wall, or both. made is what LampVolume_Build
// gave back. Says in first where in its list they start, and gives back how many there are: none if the beam wasn't made.
static inline int LampVolume_Parts(int made, bool air, bool pool, int *first)
{
	*first = air ? 0 : kLampVolumeSideTriangles;

	if (made != kLampVolumeTriangles)
	{
		return 0;
	}

	return (air ? kLampVolumeSideTriangles : 0) + (pool ? kLampVolumeTriangles - kLampVolumeSideTriangles : 0);
}

#endif
