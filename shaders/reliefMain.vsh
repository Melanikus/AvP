// vertex.vsh, for Relief map only, with what the march needs passed along too. The normals and tangents come from two lists kept beside the game's own list of corners, which are only
// attached while this shader is drawing, so with Relief map off nothing here runs and nothing about the main draw changes. See DrawSolidsWithHeights in d3d_render.cpp.

// Vertex shader input structure
struct VS_INPUT
{
    float4 Position   : POSITION;
    float4 Diffuse    : COLOR0;
    float4 Specular   : COLOR1;
    float2 Texture    : TEXCOORD0;
    float4 Normal     : NORMAL;     // x y z: which way this point faces, in the world. w: 0 if nobody wrote this polygon down
    float4 Tangent    : TANGENT;    // x y z: the way the texture's U grows, in the world. w: +1 or -1 for which way V runs
};

// Vertex shader output structure
struct VS_OUTPUT
{
    float4 Position     : POSITION;
    float4 Diffuse      : COLOR0;
    float4 Specular     : COLOR1;
    float2 Texture      : TEXCOORD0;
    float3 ViewPosition : TEXCOORD1;
    float3 Normal       : TEXCOORD2;
    float4 Tangent      : TEXCOORD3;
};

// Global variables. This goes straight into these registers: see DrawSolidsWithHeights.
float4x4 WorldViewProj : register(c0);


// Name: Relief map main vertex shader
// Type: Vertex shader
// Desc: Puts a polygon exactly where vertex.vsh would. A polygon nobody wrote a normal for gets a tangent of zero, because its entry in that list can be left over from another
// frame, and the march takes a zero tangent to mean this polygon stays where it is.

VS_OUTPUT vs_main(in VS_INPUT In)
{
    VS_OUTPUT Out;

    Out.Position     = mul(In.Position, WorldViewProj);
    Out.Diffuse      = In.Diffuse;
    Out.Specular     = In.Specular;
    Out.Texture      = In.Texture;
    Out.ViewPosition = In.Position.xyz;
    Out.Normal       = In.Normal.xyz;
    Out.Tangent      = (In.Normal.w < 0.5f) ? float4(0.0f, 0.0f, 0.0f, 0.0f) : In.Tangent;

    return Out;
}
