// Finishes a kept shadow prism's corner each frame, the same way the prisms built each frame are made. See 3dc/shadow_volumes_card.cpp for why.
//
// The game cuts every corner of the walls it draws down to a whole number, towards zero, in the view's numbers. So a wall on screen isn't quite where
// the world says it is, by up to a unit along each direction, and by a different amount as the camera moves. A prism's near end has to stay in step with
// the wall it belongs to, or the thin gap between them changes width from frame to frame, and that shows close to a wall. So this makes the same cut as the
// game, and then does what ToyAddPrism does with a corner: a ray from the light through the corner, with the near end a little past the corner along it, and
// the far end much further along it from the light.
//
// A corner holds the wall's corner in the world, and one number saying which end it is and how far along. See KeptCorner.

// These go straight into these registers. The first two are the same as surfaceStatic.vsh's: see SurfaceStatic_SetShaderNumbers.
float4 WorldToView[3]    : register(c0);    // the game's ViewMatrix, a line for each of x, y and z
float4 ProjectionDown[4] : register(c3);    // the game's projection, read down its columns
float4 LightOnList       : register(c7);    // x y z: where this prism is cast from, in the numbers polygons are drawn in. w: 1 to leave far ends uncut

static const float kFarMark = 1000.0f;      // the same as kKeptCornerFarMark

// a ray one unit long. A corner that lands right on the light has no direction, and this gives it a harmless short ray and not a number that isn't real.
float3 RayTo(float3 place)
{
    float3 fromLight = place - LightOnList.xyz;
    return fromLight / sqrt(max(dot(fromLight, fromLight), 0.0001f));
}

// Name: Kept shadow prism, with its corners cut
// Type: Vertex shader
// Desc: Puts a kept prism's corner where a prism made this frame would have it. A prism only marks the stencil, so it has no colour.

float4 vs_main(float4 Corner : POSITION) : POSITION
{
    float4 world = float4(Corner.xyz, 1.0f);
    float3 view  = float3(dot(world, WorldToView[0]), dot(world, WorldToView[1]), dot(world, WorldToView[2]));
    float3 cut   = trunc(view);

    // y is turned over, as D3D_ZBufferedGouraudTexturedPolygon_Output does
    float3 wall  = float3(cut.x, -cut.y, cut.z);
    float3 ray   = RayTo(wall);

    // A far end is marked by a number below minus kFarMark. Anything else is how far past the wall's corner the near end starts.
    float  isFar = (Corner.w < -kFarMark) ? 1.0f : 0.0f;

    float3 nearEnd = wall + ray * Corner.w;
    float3 farRay  = lerp(ray, RayTo(float3(view.x, -view.y, view.z)), LightOnList.w);
    float3 farEnd  = LightOnList.xyz + farRay * (-Corner.w - kFarMark);

    float4 onList = float4(lerp(nearEnd, farEnd, isFar), 1.0f);

    return float4(dot(onList, ProjectionDown[0]), dot(onList, ProjectionDown[1]), dot(onList, ProjectionDown[2]), dot(onList, ProjectionDown[3]));
}
