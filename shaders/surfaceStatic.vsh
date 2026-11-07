// The surface pass for a polygon that lives on the card, in the buffer made when the level loads. See 3dc/surface_static.cpp.
//
// It has to land on exactly the pixels the game drew it on, or a light would read a surface that isn't there. The game takes each corner from the world to
// the view with its own sum, and then cuts each number down to a whole number, towards zero: that's what its (int) does. So a corner moves by up to a whole
// unit, and always towards the middle of the view. This does the same sum and the same cut. Without the cut, these polygons would sit just behind the
// picture the game drew, and most of their pixels would fail the depth test.
struct VS_INPUT
{
    float3 Position : POSITION;     // in the world
    float2 Texture  : TEXCOORD0;
    float3 Normal   : NORMAL;       // which way this point faces, in the world
    float4 Tangent  : TANGENT;      // x y z: the way the texture's U grows, in the world. w: +1 or -1 for which way V runs, 0 if there's no tangent
};

// The same as surface.vsh and surfaceRelief.vsh give the pixel shaders
struct VS_OUTPUT
{
    float4 Position     : POSITION;
    float3 ViewPosition : TEXCOORD0;
    float3 Normal       : TEXCOORD1;
    float2 Texture      : TEXCOORD2;
    float3 Keep         : TEXCOORD3;
    float4 Tangent      : TEXCOORD4;
};

// These go straight into these registers: see SurfaceStatic_Draw.
float4 WorldToView[3]    : register(c0);    // the game's ViewMatrix, a line for each of x, y and z
float4 ProjectionDown[4] : register(c3);    // the game's projection, read down its columns


// Name: Surface vertex shader for the level's fixed buffer
// Type: Vertex shader
// Desc: Puts a corner where the game's own sums put it, and hands the pixel shader what the pictures need

VS_OUTPUT vs_main(in VS_INPUT In)
{
    VS_OUTPUT Out;

    float4 world = float4(In.Position, 1.0f);
    float3 view  = float3(dot(world, WorldToView[0]), dot(world, WorldToView[1]), dot(world, WorldToView[2]));

    // cut to whole numbers, then turn y over, as D3D_ZBufferedGouraudTexturedPolygon_Output does
    view = trunc(view);

    float4 onList = float4(view.x, -view.y, view.z, 1.0f);

    Out.Position     = float4(dot(onList, ProjectionDown[0]), dot(onList, ProjectionDown[1]), dot(onList, ProjectionDown[2]), dot(onList, ProjectionDown[3]));
    Out.ViewPosition = onList.xyz;
    Out.Normal       = In.Normal;
    Out.Texture      = In.Texture;

    // Always in the pictures. On a frame that isn't a cheat, the game gives a solid polygon an alpha of 1 and no alpha of its own in the specular colour,
    // so these two numbers leave the hole test in the pixel shader reading the texture's alpha alone.
    Out.Keep         = float3(1.0f, 1.0f, 0.0f);
    Out.Tangent      = In.Tangent;

    return Out;
}
