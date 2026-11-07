// surface.vsh, with the way each polygon's texture runs across it passed along too. Used when relief pictures are on: see texture_relief.cpp.
struct VS_INPUT
{
    float4 Position   : POSITION;
    float4 Diffuse    : COLOR0;
    float4 Specular   : COLOR1;
    float2 Texture    : TEXCOORD0;
    float4 Normal     : NORMAL;     // x y z: which way this point faces, in the world. w: 1 if its polygon goes in the pictures
    float4 Tangent    : TANGENT;    // x y z: the way the texture's U grows, in the world. w: +1 or -1 for which way V runs, 0 if there's no tangent
};

// Vertex shader output structure
struct VS_OUTPUT
{
    float4 Position     : POSITION;
    float3 ViewPosition : TEXCOORD0;
    float3 Normal       : TEXCOORD1;
    float2 Texture      : TEXCOORD2;
    float3 Keep         : TEXCOORD3;    // x: 1 if this polygon goes in the pictures. y and z: the alphas the game's own alpha test goes by
    float4 Tangent      : TEXCOORD4;
};

// Global variables
// These two go straight into these registers: see SurfacePictures_Draw.
float4x4 WorldViewProj : register(c0);
float4   Route         : register(c4);    // x: 1 if the polygons in the level's fixed buffer have been drawn from it, so they're left out here


// Name: Surface vertex shader with relief
// Type: Vertex shader
// Desc: Puts a polygon of the game's main list exactly where the game drew it, and hands the pixel shader what the pictures need

VS_OUTPUT vs_main(in VS_INPUT In)
{
    VS_OUTPUT Out;

    // A polygon that isn't drawn here is put off the screen, so no pixel of it costs anything. Its fourth number is 0 if it's not in the pictures, and 2 if
    // it's in the fixed buffer: see surface_normals.cpp.
    bool leftOut = In.Normal.w < 0.5f || (In.Normal.w > 1.5f && Route.x > 0.5f);

    Out.Position     = leftOut ? float4(0.0f, 0.0f, -1.0f, 1.0f) : mul(In.Position, WorldViewProj);
    Out.ViewPosition = In.Position.xyz;
    Out.Normal       = In.Normal.xyz;
    Out.Texture      = In.Texture;
    Out.Keep         = float3(In.Normal.w, In.Diffuse.a, In.Specular.a);
    Out.Tangent      = In.Tangent;

    return Out;
}
