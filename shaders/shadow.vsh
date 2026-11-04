// Vertex shader input structure
struct VS_INPUT
{
    float4 Position   : POSITION;
    float4 Diffuse    : COLOR0;
    float4 Specular   : COLOR1;
    float2 Texture    : TEXCOORD0;
};

// Vertex shader output structure
struct VS_OUTPUT
{
    float4 Position     : POSITION;
    float3 ViewPosition : TEXCOORD0;
    float3 Normal       : TEXCOORD1;
};

// Global variables
float4x4 WorldViewProj;


// Name: Shadow vertex shader
// Type: Vertex shader
// Desc: Puts a level polygon exactly where the game drew it, and hands the pixel shader where each pixel is and which way the
// polygon faces. The way it faces comes in as a colour, because a colour is what the game's vertex format has room for.

VS_OUTPUT vs_main(in VS_INPUT In)
{
    VS_OUTPUT Out;

    Out.Position     = mul(In.Position, WorldViewProj);
    Out.ViewPosition = In.Position.xyz;
    Out.Normal       = In.Diffuse.xyz * 2.0f - 1.0f;

    return Out;
}
