// THE NORMALS VIEW, a debug view for Hudson's runs (Drake 2026-11-20; the owner: "when testing with hudson, can we turn on some wireframe or debug mode that allows me to
// see the aliens in the dark?", "Showing normals would be enough/look cool"). Off unless the run asks (normals=yes, 3dc/debug_view.h). It draws what the main shaders
// (vertex.vsh, pixel.psh) draw, the level's polygons, the aliens and the player's weapon, but each pixel coloured by the way its polygon faces and at full brightness,
// so nothing is lost in the dark. It only changes how a pixel looks: nothing the game decides reads it.
//
// This half: the same place on the screen as vertex.vsh gives (its WorldViewProj, set by the game in the same register), and the polygon's corner as the game gave it
// (in the view's space: the game has turned it already) handed on to debugNormals.psh, which works out which way the polygon faces from how that changes across it.
struct VS_INPUT
{
    float4 Position : POSITION;
};

struct VS_OUTPUT
{
    float4 Position : POSITION;
    float3 Place    : TEXCOORD0;   // the corner in the view's space, blended across the polygon
};

float4x4 WorldViewProj;

VS_OUTPUT vs_main(in VS_INPUT In)
{
    VS_OUTPUT Out;

    Out.Position = mul(In.Position, WorldViewProj);
    Out.Place    = In.Position.xyz;

    return Out;
}
