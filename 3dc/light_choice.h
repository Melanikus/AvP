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

#endif
