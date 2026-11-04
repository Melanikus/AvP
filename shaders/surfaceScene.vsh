// A polygon of the level's fixed buffer, drawn into the scene. See 3dc/static_scene.cpp.
//
// Its corners go where surfaceStatic.vsh puts them, by the game's own sum and the game's own cut to whole numbers, so that the scene and the surface pictures
// are the same surface to the last pixel, and anything still drawn the old way, like a decal, sits where it always did. surfaceStatic.vsh explains the cut.
//
// ITS LIGHT is what the game gives a corner in VertexIntensity_Standard_Opt (kshape.cpp), worked out here. The light baked into a wall comes with the corner. On
// top of that go the live lights that aren't drawn per pixel, from a table of all of them, so the processor never has to touch a corner. Then the game's gamma
// curve, last, as it does it.
struct VS_INPUT
{
    float3 Position : POSITION;     // in the world
    float2 Texture  : TEXCOORD0;
    float3 Normal   : NORMAL;       // which way this point faces, in the world
    float4 Baked    : COLOR0;       // red green blue: the light baked into this corner. alpha: 1 if live lights reach this polygon, 0 if it glows on its own
};

struct VS_OUTPUT
{
    float4 Position     : POSITION;
    float4 Diffuse      : COLOR0;
    float4 Specular     : COLOR1;
    float2 Texture      : TEXCOORD0;
    float3 ViewPosition : TEXCOORD1;    // only the comparison reads this
};

// These go straight into these registers: see SurfaceStatic_SetShaderNumbers and StaticScene's SetLightNumbers.
float4 WorldToView[3]    : register(c0);    // the game's ViewMatrix, a line for each of x, y and z
float4 ProjectionDown[4] : register(c3);    // the game's projection, read down its columns
float4 GammaSetting      : register(c7);    // x: the gamma slider, from 0 to 1. A half changes nothing
float4 Lights[128]       : register(c8);    // two lines for each light. First: x y z in the world, and its range. Second: red green blue, and 1 if it shines
int    LightCount        : register(i0);


// One step of the game's gamma curve, in UpdateGammaSettings.
float3 GammaStep(float3 number)
{
    return number * number + 2.0f * GammaSetting.x * number * (1.0f - number);
}

// The whole curve: two steps, and a very slight stretch, because the game divides by 255 going in and multiplies by 256 coming out.
float3 Gamma(float3 number)
{
    return saturate(GammaStep(GammaStep(saturate(number))) * (256.0f / 255.0f));
}


// Name: Scene vertex shader for the level's fixed buffer
// Type: Vertex shader
// Desc: Puts a corner where the game's own sums put it, and gives it the light the game would

VS_OUTPUT vs_main(in VS_INPUT In)
{
    VS_OUTPUT Out;

    float4 world = float4(In.Position, 1.0f);
    float3 view  = trunc(float3(dot(world, WorldToView[0]), dot(world, WorldToView[1]), dot(world, WorldToView[2])));

    float4 onList = float4(view.x, -view.y, view.z, 1.0f);

    Out.Position     = float4(dot(onList, ProjectionDown[0]), dot(onList, ProjectionDown[1]), dot(onList, ProjectionDown[2]), dot(onList, ProjectionDown[3]));
    Out.Texture      = In.Texture;
    Out.ViewPosition = onList.xyz;

    float3 added = float3(0.0f, 0.0f, 0.0f);
    float3 shine = float3(0.0f, 0.0f, 0.0f);

    if (In.Baked.a > 0.5f)
    {
        for (int i = 0; i < LightCount; i++)
        {
            float4 place  = Lights[i * 2];
            float4 colour = Lights[i * 2 + 1];

            float3 toLight = place.xyz - In.Position;
            float3 away    = abs(toLight);

            // the game's quick distance: the longest side, and a quarter of the other two
            float longest  = max(away.x, max(away.y, away.z));
            float distance = longest + (away.x + away.y + away.z - longest) * 0.25f;

            if (distance < place.w)
            {
                float strength = place.w - distance;

                if (distance > 0.0f)
                {
                    float turned = dot(In.Normal, toLight);

                    strength *= (turned > 0.0f) ? (turned / distance + 0.25f) * 0.5f : 0.125f;
                }

                added += strength * colour.rgb;
                shine += strength * colour.rgb * colour.a;
            }
        }
    }

    // A highlight is a quarter of what the same light adds to the colour: the game takes one down by ten bits, and the other by eight.
    Out.Diffuse  = float4(Gamma(In.Baked.rgb + added), 1.0f);
    Out.Specular = float4(Gamma(shine * 0.25f), 0.0f);

    return Out;
}
