// How much of a light reaches what's on screen. Written 2026-11-06 by Melanikus for the AvP port. See light_choice.h for what this is for.

#include "light_choice.h"

#include <math.h>
#include <algorithm>

namespace
{
	// How close to the camera a corner can be and still be worked out as a place on the screen. Anything nearer, or behind, just counts as being off to whichever side it's on.
	float const kNearest = 1.0f;

	// A ray along a polygon this close to its edge-on line says nothing useful about where on the polygon it lands.
	float const kEdgeOn = 0.0001f;

	// One corner's place on the screen, from -1 to 1 each way, held to the edges of the screen. A polygon that goes past the edge ends up with its part on screen, near enough. It isn't exact, and
	// only a weight depends on it. Something that isn't a number stays that way, and the area worked out from it then refuses the whole polygon.
	void PlaceOnScreen(float x, float y, float z, float across, float up, float *screenX, float *screenY)
	{
		float sx, sy;

		if (z > kNearest)
		{
			sx = x * across / z;
			sy = y * up / z;
		}
		else
		{
			sx = (x > 0.0f) ? 1.0f : (x < 0.0f) ? -1.0f : 0.0f;
			sy = (y > 0.0f) ? 1.0f : (y < 0.0f) ? -1.0f : 0.0f;
		}

		*screenX = (sx > 1.0f) ? 1.0f : (sx < -1.0f) ? -1.0f : sx;
		*screenY = (sy > 1.0f) ? 1.0f : (sy < -1.0f) ? -1.0f : sy;
	}

	bool MoreOfTheScreen(const LightChoiceSample &a, const LightChoiceSample &b)
	{
		return a.weight > b.weight;
	}

	float Clamped01(float value)
	{
		return (value < 0.0f) ? 0.0f : (value > 1.0f) ? 1.0f : value;
	}

	// A light with a place only has to come close to one without
	float ScoreToGoBy(const LightChoiceCandidate &candidate)
	{
		return candidate.hasAPlace ? candidate.score * kLightChoiceStayScore : candidate.score;
	}

	bool ShowsNothing(const LightChoiceCandidate &candidate)
	{
		return ScoreToGoBy(candidate) < kLightChoiceShowsNothing && !candidate.keepsItsPlace;
	}
}

void LightChoice_Forget(LightChoiceSamples *samples)
{
	samples->count = 0;
	samples->leastWorthKeeping = 0.0f;
	samples->totalWeight = 0.0f;
}

void LightChoice_KeepTheLargest(LightChoiceSamples *samples)
{
	if (samples->count <= kLightChoiceSamplesKept)
	{
		return;
	}

	std::nth_element(samples->list, samples->list + kLightChoiceSamplesKept - 1, samples->list + samples->count, MoreOfTheScreen);
	samples->leastWorthKeeping = samples->list[kLightChoiceSamplesKept - 1].weight;
	samples->count = kLightChoiceSamplesKept;
}

void LightChoice_AddPolygon(LightChoiceSamples *samples, const float *pts, int count, float across, float up)
{
	if (count < 3 || count > kLightChoiceMostCorners)
	{
		return;
	}

	float screenX[kLightChoiceMostCorners], screenY[kLightChoiceMostCorners];
	float meanX = 0.0f, meanY = 0.0f;
	bool anyInFront = false;

	for (int i = 0; i < count; i++)
	{
		PlaceOnScreen(pts[i * 3 + 0], pts[i * 3 + 1], pts[i * 3 + 2], across, up, &screenX[i], &screenY[i]);
		meanX += screenX[i];
		meanY += screenY[i];
		anyInFront = anyInFront || pts[i * 3 + 2] > kNearest;
	}

	// All of it behind the camera can't be on screen, and its corners would otherwise spread to the screen's corners and count as all of it
	if (!anyInFront)
	{
		return;
	}

	meanX /= (float)count;
	meanY /= (float)count;

	// Its area on the screen, as a share of the whole screen, whose area is 4. Each triangle is counted with its sign, so that this comes out right whichever way its corners go round.
	float twiceArea = 0.0f;

	for (int i = 0; i < count; i++)
	{
		int const next = (i + 1 == count) ? 0 : i + 1;
		twiceArea += screenX[i] * screenY[next] - screenX[next] * screenY[i];
	}

	float const weight = fabsf(twiceArea) * 0.125f;

	if (!(weight > 0.0f))
	{
		return;  // edge on, off the screen, or not numbers
	}

	samples->totalWeight += weight;

	if (weight < samples->leastWorthKeeping)
	{
		return;
	}

	// The middle of the polygon and how far it spreads from there, and the way it faces. The way round is worked out from all its triangles, as ToyWorkOutSurface does in d3d_render.cpp.
	float middleX = 0.0f, middleY = 0.0f, middleZ = 0.0f;
	float nx = 0.0f, ny = 0.0f, nz = 0.0f;

	for (int i = 0; i < count; i++)
	{
		middleX += pts[i * 3 + 0];
		middleY += pts[i * 3 + 1];
		middleZ += pts[i * 3 + 2];
	}

	middleX /= (float)count;
	middleY /= (float)count;
	middleZ /= (float)count;

	for (int i = 1; i + 1 < count; i++)
	{
		float const ux = pts[i * 3 + 0] - pts[0], uy = pts[i * 3 + 1] - pts[1], uz = pts[i * 3 + 2] - pts[2];
		float const vx = pts[i * 3 + 3] - pts[0], vy = pts[i * 3 + 4] - pts[1], vz = pts[i * 3 + 5] - pts[2];

		nx += uy * vz - uz * vy;
		ny += uz * vx - ux * vz;
		nz += ux * vy - uy * vx;
	}

	float const length = sqrtf(nx * nx + ny * ny + nz * nz);

	if (!(length > 0.0f))
	{
		return;
	}

	nx /= length;
	ny /= length;
	nz /= length;

	// A polygon on screen is seen from the camera's side, so that's the side that counts as lit, whichever way its corners go round. This also counts the back of a polygon the game won't draw:
	// see light_choice.h.
	if (nx * middleX + ny * middleY + nz * middleZ > 0.0f)
	{
		nx = -nx;
		ny = -ny;
		nz = -nz;
	}

	// The spot to judge it by is the middle of its part on screen, which is the middle of the polygon if the camera can see all of it. A long wall's own middle may be nowhere near the screen.
	float placeX = middleX, placeY = middleY, placeZ = middleZ;

	if (across > 0.0f && up > 0.0f)
	{
		float const rayX = meanX / across, rayY = meanY / up;
		float const towards = nx * rayX + ny * rayY + nz;

		if (fabsf(towards) > kEdgeOn)
		{
			float const along = (nx * middleX + ny * middleY + nz * middleZ) / towards;

			if (along > kNearest)
			{
				placeX = rayX * along;
				placeY = rayY * along;
				placeZ = along;
			}
		}
	}

	float furthest = 0.0f;

	for (int i = 0; i < count; i++)
	{
		float const dx = pts[i * 3 + 0] - placeX, dy = pts[i * 3 + 1] - placeY, dz = pts[i * 3 + 2] - placeZ;
		float const here = dx * dx + dy * dy + dz * dz;
		furthest = (here > furthest) ? here : furthest;
	}

	// The list holds twice what's kept. When it's full the smaller half goes, and anything smaller than what's left never gets this far, so a frame of any size costs about the same.
	if (samples->count == kLightChoiceSamplesHeld)
	{
		LightChoice_KeepTheLargest(samples);
	}

	LightChoiceSample &sample = samples->list[samples->count++];
	sample.x = placeX;
	sample.y = placeY;
	sample.z = placeZ;
	sample.nx = nx;
	sample.ny = ny;
	sample.nz = nz;
	sample.weight = weight;
	sample.spread = sqrtf(furthest);
}

bool LightChoice_HasSamples(const LightChoiceSamples &samples)
{
	return samples.count > 0 && samples.totalWeight > 0.0f;
}

float LightChoice_LightOnOneSurface(const LightChoiceSample &sample, float x, float y, float z, float range, bool baked)
{
	float const toX = x - sample.x, toY = y - sample.y, toZ = z - sample.z;
	float const howFar = sqrtf(toX * toX + toY * toY + toZ * toZ);

	// The part of a big polygon nearest the light could be as far nearer as the polygon spreads. Too much light here only costs a place, and too little could lose a light that's really showing.
	float const nearestOfIt = (howFar > sample.spread) ? howFar - sample.spread : 0.0f;
	float const reach = Clamped01(nearestOfIt / range);
	float const turned = (sample.nx * toX + sample.ny * toY + sample.nz * toZ) / ((howFar > 1.0f) ? howFar : 1.0f);

	if (baked)
	{
		return (float)cos(1.5707963f * reach) * Clamped01(turned);
	}

	return (1.0f - reach) * ((turned > 0.0f) ? (turned + 0.25f) * 0.5f : 0.0f);
}

float LightChoice_Score(const LightChoiceSamples &samples, float x, float y, float z, float range, float brightness, bool baked)
{
	if (!LightChoice_HasSamples(samples) || !(range > 0.0f) || !(brightness > 0.0f))
	{
		return 0.0f;
	}

	float const most = (range < kLightChoiceMostRange) ? range : kLightChoiceMostRange;
	float sum = 0.0f;

	for (int i = 0; i < samples.count; i++)
	{
		sum += samples.list[i].weight * LightChoice_LightOnOneSurface(samples.list[i], x, y, z, most, baked);
	}

	float const score = sum * brightness / samples.totalWeight;

	return (score == score) ? score : 0.0f;
}

int LightChoice_Pick(const LightChoiceCandidate *candidates, int howMany, int most, int *picked, int *showNothing)
{
	int numPicked = 0;
	int numNothing = 0;
	bool taken[kLightChoiceMostCandidates];

	int const count = (howMany > kLightChoiceMostCandidates) ? kLightChoiceMostCandidates : howMany;

	for (int i = 0; i < count; i++)
	{
		taken[i] = false;
	}

	// Whatever has to come first does, in the order it came
	for (int i = 0; i < count && numPicked < most; i++)
	{
		if (candidates[i].comesFirst)
		{
			taken[i] = true;
			picked[numPicked++] = i;
		}
	}

	// Then the rest, the most light first. On a tie, the one that came first wins, which is the nearest.
	while (numPicked < most)
	{
		int best = -1;
		float bestScore = 0.0f;

		for (int i = 0; i < count; i++)
		{
			if (taken[i] || ShowsNothing(candidates[i]))
			{
				continue;
			}

			float const score = ScoreToGoBy(candidates[i]);

			if (best < 0 || score > bestScore)
			{
				best = i;
				bestScore = score;
			}
		}

		if (best < 0)
		{
			break;
		}

		taken[best] = true;
		picked[numPicked++] = best;
	}

	for (int i = 0; i < count; i++)
	{
		if (!taken[i] && ShowsNothing(candidates[i]))
		{
			numNothing++;
		}
	}

	if (showNothing)
	{
		*showNothing = numNothing;
	}

	return numPicked;
}
