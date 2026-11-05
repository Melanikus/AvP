// Puts a kept shadow prism on the screen. Its corners stay on the card in the world's numbers, so all that changes from frame to frame is this one
// matrix, which takes a place in the world straight to the screen. See 3dc/shadow_volumes.cpp. A prism only marks the stencil, so it has no colour.
float4x4 WorldToClip;

float4 vs_main(float4 Position : POSITION) : POSITION
{
    return mul(Position, WorldToClip);
}
