// The polygons of the game's own list that are also in the level's fixed buffer, and nothing else, for comparing the two. See CompareSceneRoutes in
// 3dc/d3d_render.cpp. Corners and colours are exactly as the game worked them out this frame.
struct VS_INPUT
{
    float4 Position : POSITION;
    float4 Diffuse  : COLOR0;
    float4 Specular : COLOR1;
    float2 Texture  : TEXCOORD0;
    float4 Normal   : NORMAL;       // w: 2 if this polygon is in the fixed buffer. See surface_normals.cpp
};

struct VS_OUTPUT
{
    float4 Position     : POSITION;
    float4 Diffuse      : COLOR0;
    float4 Specular     : COLOR1;
    float2 Texture      : TEXCOORD0;
    float3 ViewPosition : TEXCOORD1;
};

float4x4 WorldViewProj : register(c0);


// Name: Scene comparison vertex shader
// Type: Vertex shader
// Desc: Leaves out everything that isn't in the fixed buffer, by putting it off the screen

VS_OUTPUT vs_main(in VS_INPUT In)
{
    VS_OUTPUT Out;

    Out.Position     = (In.Normal.w > 1.5f) ? mul(In.Position, WorldViewProj) : float4(0.0f, 0.0f, -1.0f, 1.0f);
    Out.Diffuse      = In.Diffuse;
    Out.Specular     = In.Specular;
    Out.Texture      = In.Texture;
    Out.ViewPosition = In.Position.xyz;

    return Out;
}
