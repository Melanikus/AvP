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
    float2 Texture      : TEXCOORD1;
};

// Global variables
float4x4 WorldViewProj;


// Name: Flare light vertex shader
// Type: Vertex shader
// Desc: Puts a polygon of the game's main list exactly where the game drew it, and hands the pixel shader where each pixel is.

VS_OUTPUT vs_main(in VS_INPUT In)
{
    VS_OUTPUT Out;

    Out.Position     = mul(In.Position, WorldViewProj);
    Out.ViewPosition = In.Position.xyz;
    Out.Texture      = In.Texture;

    return Out;
}
