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
    float4 Position   : POSITION;
    float2 Texture    : TEXCOORD0;
};


// Name: Shadow softening vertex shader
// Type: Vertex shader
// Desc: For one rectangle over the whole screen. Its corners are already at the edges of the screen, so nothing moves them.

VS_OUTPUT vs_main(in VS_INPUT In)
{
    VS_OUTPUT Out;

    Out.Position = float4(In.Position.xy, 0.0f, 1.0f);
    Out.Texture  = In.Texture;

    return Out;
}
