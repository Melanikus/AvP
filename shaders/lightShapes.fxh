// What the three light shaders share: lightTotal.psh, lightShare.psh and lightOne.psh. Each light has to be worked out in exactly the
// same way everywhere, because lightShare and lightOne divide one light by the total that lightTotal added up. So the sums live here, once.

sampler2D Depth   : register(s0);    // how far away each pixel is
sampler2D Normals : register(s1);    // which way each pixel faces, as colours from 0 to 1
sampler2D Colours : register(s2);    // each pixel's texture colour. An alpha of 1 means a surface is here, and 0 means nothing is

// x y: one pixel, as a distance on a picture. z w: how far sideways and upwards the view widens with each unit of distance
float4 Screen : register(c0);

// The shapes a light's brightness can take on its way out from the light.
static const float kShapeBaked = 0.0f;    // the game's bake: a cosine fade, and only on surfaces facing the light
static const float kShapeGame  = 1.0f;    // the game's live lights (kshape.cpp, VertexIntensity_Standard_Opt): a straight fade
static const float kShapeFlare = 2.0f;    // the look chosen for flares: strong for most of its reach, then falling away


// Where on a picture this pixel is. Half a pixel on, because a picture's pixels are read at their middles.
float4 PlaceOnPicture(float2 screenPlace)
{
    return float4((screenPlace + 0.5f) * Screen.xy, 0.0f, 0.0f);
}

// Where this pixel is, in the same space as the light list. The picture only says how far away it is, but where it falls on the screen gives
// the rest, because the view widens evenly with distance. No half pixel here: Direct3D 9 puts a pixel's middle exactly on its number.
float3 PositionHere(float2 screenPlace, float distance)
{
    float across = (screenPlace.x * Screen.x * 2.0f - 1.0f) * Screen.z;
    float up     = (1.0f - screenPlace.y * Screen.y * 2.0f) * Screen.w;

    return float3(across * distance, up * distance, distance);
}

// Which way this pixel faces. A shape can really have a zero normal, which the picture holds as a grey close to 0.5, not exactly. Anything
// that short was a zero, so it's made one. A zero then lights as the game lights it: as a surface turned side-on.
float3 NormalHere(float4 place)
{
    float3 normal = tex2Dlod(Normals, place).xyz * 2.0f - 1.0f;

    return (dot(normal, normal) < 0.25f) ? float3(0.0f, 0.0f, 0.0f) : normal;
}

// How much light one light gives this spot, before its brightness. light is x y z and one over its range.
float LightShape(float4 light, float shape, float3 position, float3 normal)
{
    float3 toLight = light.xyz - position;
    float  howFar  = length(toLight);
    float  reach   = saturate(howFar * light.w);
    float  turned  = dot(normal, toLight / max(howFar, 1.0f));
    float  slant   = (turned + 0.25f) * 0.5f;

    // PAST THE EDGE. A polygon's stored normals can lean, because a corner's normal is shared with the polygons that meet there. With a lamp hanging just under a flat
    // ceiling, the true angle is so small that the lean outweighs it, and part of the ceiling looks turned away when it isn't. So "turned" can't say whether a surface
    // really faces a light. The surface itself can: its true flat direction comes from how the distance changes from this pixel to the next. A lamp on the same side of
    // that flat surface as the eye can reach it, and past the edge the surface gets an eighth, which is what the game gives (VertexIntensity_Standard_Opt in kshape.cpp),
    // so there's no step where "turned" crosses zero. A lamp on the far side gets nothing, as before: that's the back of a wall, and its shadow doesn't cover it. The change
    // from one to the other takes a quarter of a degree each way, so a lamp lying in the surface's own plane doesn't flip with the smallest rounding.
    float3 flat        = cross(ddx(position), ddy(position));
    float  eyesSide    = (dot(flat, position) > 0.0f) ? -1.0f : 1.0f;
    float  lampsSide   = eyesSide * dot(flat, toLight) / max(length(flat) * howFar, 0.0001f);
    float  pastTheEdge = 0.125f * saturate(0.5f + lampsSide * 114.0f);

    if (shape < 0.5f)
    {
        return cos(1.5707963f * reach) * saturate(turned);
    }

    if (shape < 1.5f)
    {
        // The game gives a point turned away from a live light an eighth of that light anyway. Here that's for a surface with no direction of its own, which NormalHere
        // makes exactly zero, and for one whose true flat surface faces the light. See PAST THE EDGE above.
        return (1.0f - reach) * ((turned > 0.0f) ? slant : (turned == 0.0f) ? 0.125f : pastTheEdge);
    }

    return (1.0f - reach * reach) * ((turned > 0.0f) ? slant : pastTheEdge);
}
