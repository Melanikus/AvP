// A polygon of the level's fixed buffer, drawn into the scene. See 3dc/static_scene.cpp.
//
// Its corners go where surfaceStatic.vsh puts them, by the game's own sum and the game's own cut to whole numbers, so that the scene and the surface pictures
// are the same surface to the last pixel, and anything still drawn the old way, like a decal, sits where it always did. surfaceStatic.vsh explains the cut.
struct VS_INPUT
{
    float3 Position : POSITION;     // in the world
    float2 Texture  : TEXCOORD0;
    float4 Baked    : COLOR0;       // the light baked into this corner, through the gamma table, as the game gives it
};

struct VS_OUTPUT
{
    float4 Position     : POSITION;
    float4 Diffuse      : COLOR0;
    float4 Specular     : COLOR1;
    float2 Texture      : TEXCOORD0;
    float3 ViewPosition : TEXCOORD1;    // only the comparison reads this
};

// These go straight into these registers: see SurfaceStatic_SetShaderNumbers.
float4 WorldToView[3]    : register(c0);    // the game's ViewMatrix, a line for each of x, y and z
float4 ProjectionDown[4] : register(c3);    // the game's projection, read down its columns


// Name: Scene vertex shader for the level's fixed buffer
// Type: Vertex shader
// Desc: Puts a corner where the game's own sums put it, with the light baked into it

VS_OUTPUT vs_main(in VS_INPUT In)
{
    VS_OUTPUT Out;

    float4 world = float4(In.Position, 1.0f);
    float3 view  = trunc(float3(dot(world, WorldToView[0]), dot(world, WorldToView[1]), dot(world, WorldToView[2])));

    float4 onList = float4(view.x, -view.y, view.z, 1.0f);

    Out.Position     = float4(dot(onList, ProjectionDown[0]), dot(onList, ProjectionDown[1]), dot(onList, ProjectionDown[2]), dot(onList, ProjectionDown[3]));
    Out.Diffuse      = In.Baked;
    Out.Specular     = float4(0.0f, 0.0f, 0.0f, 0.0f);
    Out.Texture      = In.Texture;
    Out.ViewPosition = onList.xyz;

    return Out;
}
