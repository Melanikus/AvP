// Relief mapping: how a flat polygon comes to look carved, with the raised parts really hiding what's behind them.
//
// THE IDEA. A depth map says how far down each spot of a picture lies below the polygon. Your eye looks along a straight line through the polygon, into that imagined
// surface below it. So this walks along that line, a step at a time, until the line has gone deeper than the surface at that spot. That's the spot you really see, and its
// colour and its normal are read from there, not from where the line first touched the polygon. A rib in front of a groove hides the groove, because the walk stops on the rib.
//
// It's the method of Fabio Policarpo, Manuel M. Oliveira and Joao L. D. Comba, "Real-Time Relief Mapping on Arbitrary Polygonal Surfaces", I3D 2005, pages 155-162, which builds on
// Oliveira's relief texture mapping: a fixed search in even steps, to find the first step past the surface, and then a fixed search by halves, to find the spot between two steps.
//
// THE POLYGON IS THE TOP. Everything sinks in from it and nothing rises above it, so a polygon's outline stays exactly where it is, and nothing here can show past its edge.
//
// NOTHING HERE COMES FROM A FILE'S SAY-SO. The number of steps is fixed below, and the depth is clamped before it gets here, so a map can't make this take longer or reach further.
//
// THE PICTURE'S SIZE IN THE WORLD. The depth is a fraction of one picture's width. So a picture spread over a bigger wall gets deeper grooves, in the same proportion, and it keeps its look.
// In the stock levels one picture covers from 3 to 14 metres of wall, and often nearly twice as far one way as the other. A picture that's stretched counts as its average width, and the walk
// goes by how the picture really lies on the wall, so a groove is the same depth from whichever side you look at it.

sampler2D ReliefHeight : register(s2);    // the depth map, or the flat picture, whose alpha of 0 means "there's no depth here"
float4 WorldAxes[3]    : register(c0);    // the world's x, y and z directions, as the camera sees them
float4 ReliefSettings  : register(c3);    // x: the depth, as a fraction of a picture's width, and 0 means no walk at all. y: +1 white is the polygon's level, -1 black is. z: +1 or -1, which way the normal map's tilt goes

static const int   kReliefEvenSteps   = 16;       // the first search
static const int   kReliefHalvings    = 6;        // the second search
static const float kReliefLeastSlant  = 0.2f;     // seen this close to edge-on or closer, the line is treated as if it were at this slant, or a step could leap across most of a picture
static const float kReliefMostDeeper  = 400.0f;   // the most a pixel's place can move in the world, in the game's units, which are millimetres. A polygon with a nonsense texture stops here
static const float kReliefMostShift   = 1.0f;     // the furthest the walk can go along the picture, which is one whole picture. On any sane polygon the depth and the slant already stop it sooner

// What the walk found.
struct ReliefHit
{
    float2 place;           // where on the picture to read the colour and the normal
    float3 viewPosition;    // where that spot is, as the camera sees it
    float2 dx;              // how much the picture moves across one pixel of the screen, for reading it: see below
    float2 dy;
};

// The three directions of the picture on this polygon, in the world. Gives back false if it hasn't got them: a surface with no direction of its own, or a polygon with no tangent. The flat
// normal is only a start: on a curved polygon the normal changes from pixel to pixel, and it may not stand square to the tangent, so what stands out along it is taken off.
bool PictureDirections(float3 normal, float4 tangent, out float3 straightOut, out float3 alongU, out float3 alongV)
{
    straightOut = float3(0.0f, 0.0f, 0.0f);
    alongU      = float3(0.0f, 0.0f, 0.0f);
    alongV      = float3(0.0f, 0.0f, 0.0f);

    float normalSize = dot(normal, normal);

    if (normalSize <= 0.0f || abs(tangent.w) < 0.5f)
    {
        return false;
    }

    straightOut = normal / sqrt(normalSize);
    alongU      = tangent.xyz - straightOut * dot(straightOut, tangent.xyz);

    float uSize = dot(alongU, alongU);

    if (uSize <= 0.000001f)
    {
        return false;
    }

    alongU = alongU / sqrt(uSize);
    alongV = tangent.w * cross(straightOut, alongU);

    return true;
}

// Turns a direction in the world into the same direction as the camera sees it.
float3 ReliefToCamera(float3 world)
{
    return world.x * WorldAxes[0].xyz + world.y * WorldAxes[1].xyz + world.z * WorldAxes[2].xyz;
}

// How far below the polygon the surface lies at this spot, from 0 at the polygon to 1 at the deepest. dx and dy say which of the map's smaller copies to read. The walk's spot jumps about from
// pixel to pixel, so the card couldn't work that out for itself, and it would read the wrong copy and shimmer. Alpha is 0 only on the flat picture, so a wall with no depth map never sinks,
// whichever way the height is flipped.
float ReliefSink(float2 place, float2 dx, float2 dy)
{
    float4 height = tex2Dgrad(ReliefHeight, place, dx, dy);
    float  sink   = (ReliefSettings.y > 0.0f) ? 1.0f - height.r : height.r;

    return sink * height.a;
}

// The walk. Call it outside any "if", because it asks how things change across the screen, and a shader can't ask that inside one.
// place is where the line of sight touches the polygon on the picture, viewPosition is where it touches it as the camera sees it, with the camera at 0 0 0, and normal and tangent are in the world.
// To leave a polygon out, give it a zero tangent: it gets back the same place and position.
ReliefHit ReliefMarch(float2 place, float3 viewPosition, float3 normal, float4 tangent)
{
    ReliefHit hit;

    hit.place        = place;
    hit.viewPosition = viewPosition;
    hit.dx           = ddx(place);
    hit.dy           = ddy(place);

    // Asked here, because these can't be asked for inside an "if": how far one pixel of the screen moves in the world.
    float3 worldAcross   = ddx(viewPosition);
    float3 worldDown     = ddy(viewPosition);
    float  viewSize      = dot(viewPosition, viewPosition);

    float3 straightOut, alongU, alongV;
    bool   hasDirections = PictureDirections(normal, tangent, straightOut, alongU, alongV);

    if (ReliefSettings.x <= 0.0f || !hasDirections || viewSize <= 0.0f)
    {
        return hit;
    }

    float3 view   = viewPosition / sqrt(viewSize);    // the line of sight, going away from the eye
    float3 uSeen  = ReliefToCamera(alongU);
    float3 vSeen  = ReliefToCamera(alongV);
    float  inward = -dot(view, ReliefToCamera(straightOut));

    // Looking at the back of a polygon, or exactly along it
    if (inward <= 0.0f)
    {
        return hit;
    }

    // HOW THE PICTURE LIES ON THE WALL. One pixel across the screen and one pixel down the screen each move some way along the wall, and some way along the picture. Two such moves are
    // enough to work out how far the picture moves for one unit of wall in each of the wall's two directions, however the picture is stretched or turned. It's two equations in two unknowns, and
    // wallArea is the number they both get divided by. It's zero only if those two pixels moved along one line, which a polygon seen from its front can't do, so that's left alone too.
    float2 wallAcross = float2(dot(worldAcross, uSeen), dot(worldAcross, vSeen));
    float2 wallDown   = float2(dot(worldDown, uSeen), dot(worldDown, vSeen));
    float  wallArea   = wallAcross.x * wallDown.y - wallAcross.y * wallDown.x;

    if (abs(wallArea) <= 0.000001f)
    {
        return hit;
    }

    float2 picturePerU = (hit.dx * wallDown.y - hit.dy * wallAcross.y) / wallArea;
    float2 picturePerV = (hit.dy * wallAcross.x - hit.dx * wallDown.x) / wallArea;

    // The same two moves say how much of the picture one unit of wall takes up. Its upside down is how much wall one whole picture covers, as an area, and the square root of that is its average width.
    float pictureArea = abs(picturePerU.x * picturePerV.y - picturePerU.y * picturePerV.x);

    if (pictureArea <= 0.0f)
    {
        return hit;
    }

    float worldDepth = ReliefSettings.x / sqrt(pictureArea);    // how deep the very deepest is, in the world
    float slant      = max(inward, kReliefLeastSlant);

    // How far along the picture the line of sight goes on its way down to the very deepest
    float2 wholeWay = (picturePerU * dot(view, uSeen) + picturePerV * dot(view, vSeen)) * worldDepth / slant;
    float  wayLength = length(wholeWay);

    if (wayLength > kReliefMostShift)
    {
        wholeWay = wholeWay * kReliefMostShift / wayLength;
    }

    float above = 0.0f;    // the last place on the line still above the surface: 0 is at the polygon, and 1 is at the very deepest
    float below = 0.0f;    // the first found to be under it

    // The surface can be right at the polygon here, and then there's nothing to find. It's what stops a wall with no depth map costing more than this one read.
    if (ReliefSink(place, hit.dx, hit.dy) > 0.0f)
    {
        // The line reaches 1 on the last step, and nothing can be deeper than 1, so this always finds it.
        [loop]
        for (int step = 0; step < kReliefEvenSteps; step++)
        {
            below += 1.0f / kReliefEvenSteps;

            if (below >= ReliefSink(place + wholeWay * below, hit.dx, hit.dy))
            {
                break;
            }

            above = below;
        }

        // The spot is somewhere between those two. Each halving looks in the middle, and keeps the half it's in.
        [loop]
        for (int halving = 0; halving < kReliefHalvings; halving++)
        {
            float middle = (above + below) * 0.5f;

            if (middle >= ReliefSink(place + wholeWay * middle, hit.dx, hit.dy))
            {
                below = middle;
            }
            else
            {
                above = middle;
            }
        }
    }

    hit.place = place + wholeWay * below;

    // The same spot in the world. Going down that far takes this much distance along the line of sight.
    float deeper = min(below * worldDepth / slant, kReliefMostDeeper);

    hit.viewPosition = viewPosition + view * deeper;

    return hit;
}
