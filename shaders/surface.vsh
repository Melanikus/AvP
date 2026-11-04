// Vertex shader input structure. Normal comes from a second list beside the game's vertices: see surface_normals.cpp.
struct VS_INPUT
{
    float4 Position   : POSITION;
    float4 Diffuse    : COLOR0;
    float4 Specular   : COLOR1;
    float2 Texture    : TEXCOORD0;
    float4 Normal     : NORMAL;     // x y z: which way this point faces, in the world. w: 1 if its polygon goes in the pictures
};

// Vertex shader output structure
struct VS_OUTPUT
{
    float4 Position     : POSITION;
    float3 ViewPosition : TEXCOORD0;
    float3 Normal       : TEXCOORD1;
    float2 Texture      : TEXCOORD2;
    float3 Keep         : TEXCOORD3;    // x: 1 if this polygon goes in the pictures. y and z: the alphas the game's own test uses
};

// Global variables
float4x4 WorldViewProj;
float4   WorldAxes[3] : register(c4);    // the world's x, y and z directions, as the camera sees them


// Name: Surface vertex shader
// Type: Vertex shader
// Desc: Puts a polygon of the game's main list exactly where the game drew it, and hands the pixel shader what the pictures need. Normals
// arrive in the world's directions. This is the one place each frame where they're turned to match the camera's.

VS_OUTPUT vs_main(in VS_INPUT In)
{
    VS_OUTPUT Out;

    Out.Position     = mul(In.Position, WorldViewProj);
    Out.ViewPosition = In.Position.xyz;
    Out.Normal       = In.Normal.x * WorldAxes[0].xyz + In.Normal.y * WorldAxes[1].xyz + In.Normal.z * WorldAxes[2].xyz;
    Out.Texture      = In.Texture;
    Out.Keep         = float3(In.Normal.w, In.Diffuse.a, In.Specular.a);

    return Out;
}
