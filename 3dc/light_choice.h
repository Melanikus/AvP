// Which lights may throw a shadow. Written 2026-11-06 by Melanikus for the AvP port.
//
// A map has two kinds of light of its own. A level light is a bare point the level's makers placed to paint the walls with colour, and a map is pitch black without them. Nothing
// can be seen there, so a shadow from one comes from nowhere the player can point to. A placed light is a lamp you can see: an object with a shape, and when one of that shape's
// polygons has the corona mark, the game draws a glow on it (bh_light.cpp, particle.cpp). The rule is that only a lamp you can see casts a visible shadow.
//
// So with SHADOWAMBIENT off, which is how the game starts, the map's lights without a corona take no place on the shadow list and throw no shadow. They still light the map exactly
// as before: a level light's colour is baked into the walls, and any other light left off the list simply stays in the game's own lighting. SHADOWAMBIENT ON puts them back to
// compete for a place, to compare the two looks.
//
// Everything that isn't one of the map's own lights is not touched by this, corona or not: flares, muzzle flashes, explosions, rockets, bolts, things on fire and sparks.
//
// This goes by whether a lamp has a corona at all, and not by whether the glow is on screen just now. The glow depends on a detail setting and on the camera seeing the spot, and
// going by those would make shadows pop on and off as the player turned. A lamp that has been shot out is dark, and is treated as any other dark lamp: see ToyConsiderLight in kshape.cpp.
//
// This file holds only the decision, with nothing of the game in it, so that tests/view_switches_test.cpp can run it.

#ifndef LIGHT_CHOICE_H
#define LIGHT_CHOICE_H

enum LightKindForShadows
{
	kLightFromTheLevel,      // a bare point in the level file
	kLampWithACorona,        // a placed light whose shape has the corona mark
	kLampWithNoCorona,       // a placed light with no such polygon
	kLightOfAnyOtherKind     // anything else: not one of the map's own lights
};

// Can this light have a place on the shadow list? ambientCasts is what SHADOWAMBIENT says.
inline bool LightChoice_MayCast(LightKindForShadows kind, bool ambientCasts)
{
	if (kind == kLightFromTheLevel || kind == kLampWithNoCorona)
	{
		return ambientCasts;
	}

	return true;
}

// THE SCORE: among the lights that may cast, the ones to give a place are the ones that put the most light on what's on screen, and not simply the nearest. A strong lamp lighting a whole wall beats
// a dim one that happens to be closer, and a lamp in the next room with no surface in its reach takes no place at all, however close it is.
//
// While a frame is drawn, each polygon on the way to the screen is noted here. Only the largest few hundred are kept, because they're most of what can be seen. The next frame, before any light is
// used, each light's score is worked out over them: how much of the screen each stands for, times the light it gets, by the same sum the shaders use for a light's shape (lightShapes.fxh). So range
// alone earns nothing: a far reach only helps if there's a surface in it. Everything here is in the space the camera sees, as it was for the frame just drawn, so the caller has to give a light's place
// in that same space.
//
// WHAT THIS DOESN'T KNOW: walls. A lamp behind a wall scores as if the wall weren't there, the same way the light total counts it, and a polygon hidden behind another counts as on screen. So does the
// back of a polygon, because the game hands over the whole of a shape. It also only knows about polygons that came this way: with -staticscene the walls don't, so the caller goes by nearest there. Any
// of these gives a light more than it earned, which costs a place and not a shadow. One thing can give it less: a big polygon past the edge of the screen is held to the edge, so its weight and the spot
// it's judged by are only near enough, and that's why each sample also says how far it spreads.
//
// -shadownearest goes by distance instead, as the game used to. It needs -dev, and a network game puts it back. A lamp this calls "lighting nothing" then gets a place, so the same view drawn both ways says
// whether that lamp really showed nothing: take a picture of the view each way, and count the pixels that differ.
//
// None of this uses the game's random numbers or changes anything in the game: it only decides what this player's own screen draws, so it can't affect a network game.
//
// THE COST, on any frame: at most kLightChoiceMostCandidates steady and the same again of brief lights, each summed over kLightChoiceSamplesKept surfaces, which is 32,768 sums, and noting at most the
// 60,000 polygons of kToyMaxCasters. Measured on one PC in a small program, 2026-11-06: 0.16 ms for the sums and 1.0 ms to note that many polygons. Nothing here takes more with more lights or polygons.

int const kLightChoiceSamplesKept = 256;
int const kLightChoiceSamplesHeld = kLightChoiceSamplesKept * 2;
int const kLightChoiceMostCorners = 16;         // the most corners a polygon can have here. ToyShadows_AddCaster keeps to the same
int const kLightChoiceMostCandidates = 64;      // the nearest this many are scored. The rest go without

float const kLightChoiceMostRange = 100000.0f;  // the furthest a light counts as reaching: kToyMostRange in d3d_render.h
float const kLightChoiceStayScore = 1.3333f;    // a light with a place only has to come within a quarter of a better one, so that lights don't keep swapping as the camera moves
float const kLightChoiceShowsNothing = 0.0002f; // a light giving less than this, as a share of full brightness over the whole screen, can't be seen

// One polygon on screen, in the space the camera sees.
struct LightChoiceSample
{
	float x, y, z;     // the spot to judge it by, in its part on screen
	float nx, ny, nz;  // the way it faces, towards the camera
	float weight;      // how much of the screen it covers, from 0 to 1
	float spread;      // how far it goes from that spot
};

struct LightChoiceSamples
{
	LightChoiceSample list[kLightChoiceSamplesHeld];
	int count;
	float leastWorthKeeping;  // anything smaller than this isn't kept
	float totalWeight;        // of everything noted, whether kept or not
};

// One light up for a place
struct LightChoiceCandidate
{
	float score;         // from LightChoice_Score
	bool comesFirst;     // a flare or one of the player's own lights, which come ahead of any score, as they always have
	bool hasAPlace;      // it had a place last frame
	bool keepsItsPlace;  // a lamp that's only dark for the moment, which must not lose its place for it. See ToyConsiderLight in kshape.cpp
};

void LightChoice_Forget(LightChoiceSamples *samples);

// Notes one polygon: count corners, x y z each, in the space the camera sees with y going up. across and up are how much the view magnifies things.
void LightChoice_AddPolygon(LightChoiceSamples *samples, const float *pts, int count, float across, float up);

// Throws away all but the largest. Call this first: a score looks at everything left, which is what the cost above counts on.
void LightChoice_KeepTheLargest(LightChoiceSamples *samples);

// No samples means nothing to go by, as on the first frame after a load. The caller then goes by nearest, as the game used to.
bool LightChoice_HasSamples(const LightChoiceSamples &samples);

// The light one surface gets from a light, before its brightness. baked is the shape of a light baked into the walls, and otherwise it's a live one's. See LightShape in lightShapes.fxh.
float LightChoice_LightOnOneSurface(const LightChoiceSample &sample, float x, float y, float z, float range, bool baked);

// How much of a light reaches what's on screen. A light's place, in the space the camera saw last frame.
float LightChoice_Score(const LightChoiceSamples &samples, float x, float y, float z, float range, float brightness, bool baked);

// Which of the candidates get a place, at most most of them, best first. picked gets their numbers in the list, and needs room for most. showNothing gets how many were left out because they
// light nothing on screen, as opposed to finding no place left. It gives back how many were picked.
int LightChoice_Pick(const LightChoiceCandidate *candidates, int howMany, int most, int *picked, int *showNothing);

#endif
