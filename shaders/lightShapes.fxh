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
static const float kShapeCone  = 3.0f;    // the marine's shoulder lamp: a cone that aims at the middle of the screen

// ---- HOW THE WET GLINT LOOKS (a specular highlight). The game reads this file as it starts, so changing one of these needs no rebuild, only a restart. Nobody has judged these yet.

// How strong the glint is. At 1 the very middle of it adds as much as the light gives the surface it's on.
static const float kShineStrength = 0.8f;

// How tight it is. A higher number is a smaller, harder glint, like wet skin, and a lower one is a wide sheen.
static const float kShininess = 40.0f;

// ---- HOW THE MARINE'S SHOULDER LAMP LOOKS. The game reads this file as it starts, so changing one of these needs no rebuild, only a restart. Where the lamp sits and its colour are in 3dc/shoulder_lamp.h.

// How far ahead of his eyes the lamp is aimed, in millimetres. The lamp is off to the side, so it's angled in to meet the middle of the screen at this distance.
static const float kLampAimsAt = 6000.0f;

// How wide the cone is, as the cosine of the angle from its middle. It's strongest inside the first, nothing outside the second, and it falls away softly all the way between. Smaller numbers are wider. 0.99 is 8 degrees, 0.93 is 22, and 0.77 is 40.
static const float kLampFullInside = 0.99f;
static const float kLampNothingOutside = 0.93f;

// A real small lamp also throws a faint wide spill around its beam. How far that reaches, the same way, and how strong it is where the beam is 1.
static const float kLampSpillOutside = 0.77f;
static const float kLampSpillStrength = 0.12f;

// How strong it is. At 1 a wall right in front of it shows its own colour.
static const float kLampStrength = 1.2f;

// How far away, in millimetres, it has fallen to half. This is a big part of what makes it look like light in a room, rather than a shape on the screen: a near wall is bright and a far one dim.
static const float kLampHalfDistance = 7000.0f;

// .xy is how far the lamp's aim is from where he's looking, across and down, for each unit ahead. It's on a little robot arm that follows him: see ShoulderLamp_FollowTheView in 3dc/shoulder_lamp.h.
float4 LampAim : register(c25);


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

// The glint of one light on this spot, from 0 to 1: it's strongest where the surface is turned halfway between the light and the eye, so it slides over the surface as either moves. The bumps come from the normal,
// which the relief picture has already tilted in the surface pass. The eye is at 0 0 0, because that's where the space of the light list and of PositionHere starts. This isn't in LightShape, and it mustn't be:
// lightTotal adds that up, and the others divide by it, so a glint in there would change how much of a pixel's light each shadow takes.
float ShineHere(float3 toLight, float3 position, float3 normal)
{
    // A direction with no length would divide by 0, and that pixel could come out any colour. It happens with a light exactly on the spot, or straight behind a surface, so the sizes here are never taken as less than a small number.
    float3 toLightNow = toLight / max(length(toLight), 1.0f);

    if (dot(normal, toLightNow) <= 0.0f)
    {
        return 0.0f;
    }

    float3 halfway = toLightNow - position / max(length(position), 1.0f);

    // Not below a very small number, because some cards can't raise exactly 0 to a power
    return pow(max(dot(normal, halfway / max(length(halfway), 0.0001f)), 0.0001f), kShininess);
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

    if (shape > 2.5f)
    {
        // The middle of the cone goes from the lamp to the place it's aimed at, and this says how close to that this spot is. A surface with no direction of its own gets the same eighth as above.
        float along = dot(normalize(float3(LampAim.x * kLampAimsAt, -LampAim.y * kLampAimsAt, kLampAimsAt) - light.xyz), -toLight / max(howFar, 1.0f));
        float inCone = smoothstep(kLampSpillOutside, kLampFullInside, along) * kLampSpillStrength + smoothstep(kLampNothingOutside, kLampFullInside, along);
        float facing = (turned == 0.0f) ? 0.125f : saturate(turned);

        return kLampStrength * inCone * facing * (1.0f - reach * reach) / (1.0f + howFar * howFar / (kLampHalfDistance * kLampHalfDistance));
    }

    return (1.0f - reach * reach) * ((turned > 0.0f) ? slant : pastTheEdge);
}
