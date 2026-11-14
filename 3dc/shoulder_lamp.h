#pragma once

// The marine's shoulder lamp, like the small spotlights the marines wear on the shoulder in the film Aliens. Written 2026-11-05 by Melanikus for the AvP port.
//
// WHAT IT IS, WITH LIGHTS ADDED PER PIXEL: a real light, on the list of the frame's chosen lights, beside his eyes at his right shoulder and aimed at the middle of the screen. It lights each wall's real colour in a cone, so a pitch-dark room lights up, and it throws real
// shadows, through the same shadow volumes as any other light. ConsiderTheShoulderLamp in kshape.cpp puts it on the list, and its shape is at the top of shaders/lightShapes.fxh. It only shows where it lands, so there's no beam to see in the air.
//
// WHAT IT IS OTHERWISE: a picture effect, in two parts. shaders/shoulderLamp.psh lights a soft round spot in the middle of the finished picture. It brightens what's there, and adds each wall's own colour from the surface pictures, so that it lights a dark room. And a long
// soft cone leads into the spot from the right of the screen, which is only a faint haze and lights nothing, or looking through it would be night vision. In the three per-pixel modes the game has drawn the surface pictures anyway. In Gouraud nothing else does, so while the lamp is on they're drawn just
// for it, which costs one more drawing of the solid polygons, off the screen, and nothing with the lamp off. That shader says why the cone looks the way it does.
//
// Either way nothing else in the game knows about it, and for now other players can't see it. It flickers a little, by itself, and it works in network games too, even though it can help you see in the dark.
//
// WHEN IT'S ON: only when it's switched on, and only for a marine who's alive, on a frame of the game itself. MaintainHUD asks for it each frame, and menus and films never come through there, so they can't be lit by it.
//
// This file knows nothing about the game or the graphics card, so that a test can try all of it.

// Turns it on if it's off, and off if it's on. Everything that switches the lamp comes through here.
void ShoulderLamp_Toggle();
bool ShoulderLamp_IsOn();
void ShoulderLamp_TurnOff();

// Asked once for each frame of the game, with whether this player is someone who can have a lamp at all. The next one below then says whether to draw it.
void ShoulderLamp_AskForThisFrame(bool playerCanHaveALamp);

// Is it asked for on this frame? This only looks, and takes nothing away from the answer below.
bool ShoulderLamp_IsAskedForThisFrame();

// Could it be drawn on the coming frame? That frame hasn't been asked about yet, so this goes by what the last frame was told about who the player is.
bool ShoulderLamp_CouldBeDrawn();

// Should it be drawn on this frame? It says yes once for each time it was asked, so a frame nobody asked for can't draw it.
bool ShoulderLamp_TakeThisFramesAnswer();

// How many frames have had it drawn so far
long ShoulderLamp_FramesDrawn();

// Could it be a real light on the coming frame? It's chosen before the frame is lit, and asked for after, so this goes by what the last frame was told about who the player is.
bool ShoulderLamp_CanBeALight();

// Whether lamps are real lights, wherever they can be: the player's choice on the rendering page. See view_settings.cpp.
void ShoulderLamp_SetRealLightWanted(bool wanted);
bool ShoulderLamp_RealLightIsWanted();

// Whether other marines' lamps show their beam in the air when lights are drawn per pixel: the player's choice on the rendering page, on unless they say
// otherwise. With Gouraud lights the beam is a lamp's only light, so it always shows. See LampBeams_Make in kshape.cpp.
void ShoulderLamp_SetBeamsWanted(bool wanted);
bool ShoulderLamp_BeamsAreWanted();

// Where the real lamp is from his eyes, in millimetres: across to his right, up, and ahead. See kShoulderLampAcross.
const float *ShoulderLamp_PlaceFromEyes();
void ShoulderLamp_SetPlaceFromEyes(float across, float up, float ahead);

// THE ROBOT ARM. The lamp is on a little robot arm, like the ones in the game's universe, and it centres on what he's looking at, a moment behind him. So it has an aim of its own, which stays where it was in the world when he turns, and then catches up on a smooth curve, starting
// and stopping gently. This is told every frame, whether the lamp is on or not, so that it's never behind when it's switched on. Everything else about the lamp's aim comes from here, so the painted lamp and the real one follow the same way.
//
// How far behind it can get is limited, so a fast spin can't throw it off the screen: it stays at that limit, trailing him, until he stops, and then catches up in the same time. If he's been put somewhere new there's no chase, and it's simply there: on the first frame, after a long
// frame such as a level loading, when the camera moves further in one frame than anyone can walk, or when the view turns further in one frame than anyone can turn. seconds is how long the frame took, so it takes the same time at any frame rate.
void ShoulderLamp_FollowTheView(const double *look, const double *right, const double *down, double moved, double seconds);

// How long the arm takes to catch up, for this run of the game: nothing keeps it for the next one. 0 means it's stuck to the view, and never behind.
double ShoulderLamp_FollowSeconds();
void ShoulderLamp_SetFollowSeconds(double seconds);

// Reads the number typed after LAMPFOLLOW. It's only digits, with one dot at most, and it has to be from 0 up to kShoulderLampMostFollowSeconds. It's read by hand because the library's reader also takes things
// like "1e9", "inf" and "nan", and what it makes of a dot depends on the computer's language. Anything else gives back false and leaves seconds alone.
bool ShoulderLamp_FollowSecondsFromWords(const char *words, unsigned int length, double *seconds);

// THE SPOT. How big and how strong the painted lamp's spot is, for this run of the game: nothing keeps them for the next one. The LAMPSPOTSIZE and LAMPINTENSITY commands say.
// A bigger or brighter lamp than the other players have would be cheating, so a network game keeps both as built. Both have limits that keep it a lamp, even playing alone. Setting one outside
// its limits brings it to the nearest limit. These don't reach the lamp as a real light: its own look is THE LAMP'S LOOK, below. (Modified 2026-11-14 by Melanikus (Dietrich).)
double ShoulderLamp_SpotSize();
void ShoulderLamp_SetSpotSize(double size);
double ShoulderLamp_Intensity();
void ShoulderLamp_SetIntensity(double intensity);

// Reads the number typed after each command, the same way as LAMPFOLLOW's. Anything outside the limits gives back false, and leaves the number alone.
bool ShoulderLamp_SpotSizeFromWords(const char *words, unsigned int length, double *size);
bool ShoulderLamp_IntensityFromWords(const char *words, unsigned int length, double *intensity);

// LAMPLOOK. Added 2026-11-11 by Melanikus (Third) for the AvP port, at the owner's ask: since d43e17ec (other marines' lamps, #108), with
// lights added per pixel and "Lamps cast shadows" on, the player's own lamp is a real light. DrawShoulderLamp then draws nothing of the
// painted lamp, and its flickering spot with the hot middle, and the cone's haze in the dark, were gone. He hadn't asked for that. How it
// looks when it could be a real light, for this run of the game (nothing keeps it for the next one):
// - kShoulderLampLookRealOnly (0): the real light alone, with its shadows, as d43e17ec made it.
// - kShoulderLampLookBoth (1): the real light with its shadows, and over it the painted spot's flicker and hot middle and the cone's
//   haze. DrawShoulderLamp says how they're blended so the light isn't doubled (kShoulderLampOverARealLight). It's the default
//   (kShoulderLampLookAsBuilt): the owner, 2026-11-11, "Restore it, show me", "With shadows".
// - kShoulderLampLookPainted (2): the painted lamp alone, as with "Lamps cast shadows" off: the lamp is never a real light.
// - kShoulderLampLookBothFull (3): the real light with its shadows, and over it the painted lamp at full, walls and wet glint included,
//   exactly as it was drawn on its own on 2026-11-05 (4143c5c4, the owner's video). In the spot it adds to what the real light gives, so it's
//   brighter there than either alone; 1 is the same look blended so it isn't.
// A network game keeps it as built, like the spot's size and strength: a brighter lamp than the other players have would be cheating.
int const kShoulderLampLookRealOnly = 0;
int const kShoulderLampLookBoth = 1;
int const kShoulderLampLookPainted = 2;
int const kShoulderLampLookBothFull = 3;
int const kShoulderLampLookAsBuilt = kShoulderLampLookBoth;   // the default, which a network game keeps

// Over the real light (kShoulderLampLookBoth), how strongly the painted spot brightens what's there, as a part of what it does alone
float const kShoulderLampOverARealLight = 0.25f;

// What LAMPLOOK says now. Setting a number that isn't one of the four leaves it as it was.
int ShoulderLamp_Look();
void ShoulderLamp_SetLook(int look);

// Reads the number typed after LAMPLOOK: only "0", "1", "2" or "3". Anything else gives back false, and leaves look alone.
bool ShoulderLamp_LookFromWords(const char *words, unsigned int length, int *look);

// THE LAMP'S LOOK, R-LAMP-1. Added 2026-11-14 by Melanikus (Dietrich) for the AvP port. The owner, 2026-11-11: "We cut too much from
// range so we no longer see the nice spot in low ligth", then "everything that changed between nov 7 and now is a variable yes? all
// of them", and "DEfaults from 7th, but all tunable, thats what I want". So every number of the lamp's look that the co-op work of
// 2026-11-07 (67ae4b50) changed is now one the game sends to the shaders, and a console command changes it, playing alone. The
// numbers as built are the ones from before it (ce293c81, 2026-11-05). For this run of the game only: nothing keeps them for the next.
// A network game keeps every one as built, like the spot's size: a stronger or wider lamp than the other players have would be
// cheating. Each command refuses a number outside its limits, and says the limits. Each setter below brings a number outside them to
// the nearest limit, so whatever comes in, the shaders only ever get numbers inside them.
//
// The real light (shaders/lightShapes.fxh, the lamp's cone). These are shared by every lamp drawn as a real light, so playing alone
// they change the NPC marines' and friends' lamps too:
// - LAMPSTRENGTH: how strong it is. At 1 a wall right in front of it shows its own colour.
// - LAMPHALFDISTANCE: how far away, in millimetres, its light has fallen to half.
// - LAMPCONE: how wide the cone is, as two cosines of the angle from its middle: inside the first it's strongest, outside the second
//   there's nothing. Smaller numbers are wider: 0.90 is 26 degrees, 0.86 is 31.
// - LAMPSPILL: how strong the faint wide spill around the beam is, where the beam is 1. 0 is no spill. How far the spill reaches is
//   fixed: kShoulderLampSpillEdge.
// The painted lamp (shaders/shoulderLamp.psh):
// - LAMPSHOULDER: where its cone starts on the screen, across and down, in parts of the screen: (0, 0) is the top left and (1, 1) the
//   bottom right. Built, it's (1.00, -0.10), just above the top right corner.
// - LAMPSPOTHALFDISTANCE: how far away, in millimetres, the spot's light on a wall has fallen to half. 0, as built, means it doesn't
//   fall away at all: on 2026-11-05 it didn't, and that fall was added on 2026-11-06 (a318951a). Otherwise from 1000 to 20000.
double ShoulderLamp_Strength();
void ShoulderLamp_SetStrength(double strength);
double ShoulderLamp_HalfDistance();
void ShoulderLamp_SetHalfDistance(double millimetres);
double ShoulderLamp_ConeInside();
double ShoulderLamp_ConeOutside();
void ShoulderLamp_SetCone(double inside, double outside);
double ShoulderLamp_Spill();
void ShoulderLamp_SetSpill(double spill);
double ShoulderLamp_ShoulderAcross();
double ShoulderLamp_ShoulderDown();
void ShoulderLamp_SetShoulder(double across, double down);
double ShoulderLamp_SpotHalfDistance();
void ShoulderLamp_SetSpotHalfDistance(double millimetres);

// Read the words typed after each command, the same way as LAMPFOLLOW's. LAMPCONE and LAMPSHOULDER take two numbers with spaces
// between them, and LAMPSHOULDER's can start with a minus. Anything outside the limits, or anything else at all, gives back false and
// leaves the numbers alone.
bool ShoulderLamp_StrengthFromWords(const char *words, unsigned int length, double *strength);
bool ShoulderLamp_HalfDistanceFromWords(const char *words, unsigned int length, double *millimetres);
bool ShoulderLamp_ConeFromWords(const char *words, unsigned int length, double *inside, double *outside);
bool ShoulderLamp_SpillFromWords(const char *words, unsigned int length, double *spill);
bool ShoulderLamp_ShoulderFromWords(const char *words, unsigned int length, double *across, double *down);
bool ShoulderLamp_SpotHalfDistanceFromWords(const char *words, unsigned int length, double *millimetres);

// Puts the spot's size, its strength, LAMPLOOK and every number of the lamp's look above back as built. It says whether any had been
// changed, so that the log can say so once and not every frame. (Modified 2026-11-11 by Melanikus (Third): LAMPLOOK too. Modified
// 2026-11-14 by Melanikus (Dietrich): the lamp's look, R-LAMP-1.)
bool ShoulderLamp_PutSpotBack();

// What shaders/shoulderLamp.psh reads as Spot: the size, the size as built over the size, the strength, and one over the spot's
// half-distance, which is 0 when it doesn't fall away (LAMPSPOTHALFDISTANCE 0). (Modified 2026-11-14 by Melanikus (Dietrich): .w was
// a spare 0.)
void ShoulderLamp_SpotNumbers(float *four);

// What shaders/shoulderLamp.psh reads as Beam: the painted cone's line, from where it starts (LAMPSHOULDER) to where it's aimed, and one over that
// line's length squared. aimAcross and aimDown are how far the aim is from the middle of the screen, as the shader is sent them in Swing.xy, and wide
// is how wide the screen is for its height. The shader has no room left for these sums (it's at 62 of shader model 2's 64), so they're done here,
// once a frame. The length is never taken as shorter than kShoulderLampShortestBeam allows, so a shoulder put where the aim is can't make the
// shader divide by nothing. Added 2026-11-14 by Melanikus (Dietrich) for the AvP port (R-LAMP-1).
void ShoulderLamp_PaintedBeam(float aimAcross, float aimDown, float wide, float *four);

// What the light shaders read in their two registers for the lamp's look (shaders/lightShapes.fxh: LampCone at c26, LampFall at c27):
// the cone's inside and outside, the spill's strength and the lamp's strength; then one over its half-distance squared, and three
// spare 0s. Added 2026-11-14 by Melanikus (Dietrich) for the AvP port.
void ShoulderLamp_LightNumbers(float *eight);

// A plain number, typed by a person: only digits, with one dot at most. pixel_look.cpp uses it too.
bool ReadPlainNumber(const char *words, unsigned int length, double *out);

// The same, with a minus allowed in front, as in "-0.1". Only LAMPSHOULDER takes one. Added 2026-11-14 by Melanikus (Dietrich).
bool ReadSignedNumber(const char *words, unsigned int length, double *out);

// How far its aim is from where he's looking, across to the right and down, for each unit ahead. Both are 0 when he's been still for a moment.
float ShoulderLamp_AimAcross();
float ShoulderLamp_AimDown();

// How many frames it has been a real light, rather than the painted version
void ShoulderLamp_NoteARealLightFrame();
long ShoulderLamp_FramesAsARealLight();

// HOW THE ROBOT ARM MOVES. The user asked for a smooth movement that takes no more than a second. How long it takes can be changed while playing, with the LAMPFOLLOW command. The others need a rebuild. How much of the following shows on the screen is a number in shaders/shoulderLamp.psh, which doesn't.
double const kShoulderLampFollowSeconds = 1.0;      // it's within 1% of where he's looking by this long after he stops turning, until LAMPFOLLOW says otherwise
double const kShoulderLampMostFollowSeconds = 5.0;  // the most LAMPFOLLOW accepts. Any longer and the lamp hardly seems to be aimed at anything
double const kShoulderLampMostBehind = 0.45;        // how far behind his view it can get, as the straight line between the two directions if each is 1 long. 0.45 is about 26 degrees
double const kShoulderLampLongFrame = 0.5;          // a frame longer than this many seconds means something else was going on, like a level loading
double const kShoulderLampSnapTurn = -0.17;         // the view turning this far in one frame, as the cosine of the angle, is being put somewhere new, not turning. -0.17 is about 100 degrees

// THE SPOT'S LIMITS. These are parts of the screen's height, like the sizes in the shader, and the size as built has to be the same number as kSizeAtFarEnd there.
// At 0.35 it's 70% of the screen's height across, which is about as big as it gets and still reads as a lamp and not a floodlight. At 2 it's twice as strong as built, and the same reason applies.
double const kShoulderLampSpotSize = 0.21;
double const kShoulderLampFewestSpotSize = 0.05;
double const kShoulderLampMostSpotSize = 0.35;
double const kShoulderLampIntensity = 1.0;
double const kShoulderLampFewestIntensity = 0.25;
double const kShoulderLampMostIntensity = 2.0;

// THE LAMP'S LOOK: AS BUILT, AND ITS LIMITS (R-LAMP-1, above). Added 2026-11-14 by Melanikus (Dietrich) for the AvP port. As built is
// ce293c81's, the owner's "DEfaults from 7th". The limits keep it a lamp, and they keep every sum in the shaders safe:
// - The cone's edges are worked out by dividing by inside minus outside, and by inside minus the spill's edge. So inside has to be
//   kShoulderLampConeGap or more above outside, and its least is well above the spill's edge. Neither difference can be 0.
// - A half-distance goes in as one over itself, so it's never 0. LAMPSPOTHALFDISTANCE's 0 means no fall at all, and goes in as 0.
// - The shoulder can be up to half a screen off any edge. Anywhere nearer the middle the cone would come from inside the picture.
double const kShoulderLampStrength = 2.2;
double const kShoulderLampFewestStrength = 0.1;
double const kShoulderLampMostStrength = 4.0;
double const kShoulderLampHalfDistance = 9000.0;
double const kShoulderLampFewestHalfDistance = 1000.0;
double const kShoulderLampMostHalfDistance = 20000.0;           // as far as the lamp reaches: kShoulderLampRange
double const kShoulderLampConeInside = 0.90;
double const kShoulderLampConeOutside = 0.86;
double const kShoulderLampFewestConeInside = 0.80;
double const kShoulderLampMostConeInside = 0.995;
double const kShoulderLampFewestConeOutside = 0.70;
double const kShoulderLampMostConeOutside = 0.99;
double const kShoulderLampConeGap = 0.01;                       // the least inside can be above outside
double const kShoulderLampSpillEdge = 0.77;                     // how far the spill reaches: it has to be kLampSpillOutside in shaders/lightShapes.fxh
double const kShoulderLampSpill = 0.0;
double const kShoulderLampMostSpill = 0.5;
double const kShoulderLampShoulderAcross = 1.00;
double const kShoulderLampShoulderDown = -0.10;
double const kShoulderLampFewestShoulder = -0.5;
double const kShoulderLampMostShoulder = 1.5;
double const kShoulderLampSpotHalfDistance = 0.0;               // 0: the spot's light doesn't fall away
double const kShoulderLampFewestSpotHalfDistance = 1000.0;
double const kShoulderLampMostSpotHalfDistance = 20000.0;

// THE PAINTED CONE'S LINE (ShoulderLamp_PaintedBeam). The first two have to be kAim and kFollowing in shaders/shoulderLamp.psh, which works out where
// the spot is from the same two: if they differed, the cone wouldn't end at the spot.
double const kShoulderLampPaintedAim = 0.5;                     // where it's aimed, across and down: the middle of the screen
double const kShoulderLampPaintedFollowing = 1.0;               // how much of the robot arm's following shows
double const kShoulderLampShortestBeam = 0.00000001;            // the least the line's length squared is taken as: a ten-thousandth of the screen, squared

// WHERE THE REAL LAMP SITS. A light right at the eye throws every shadow straight away from the eye, where it can't be seen, so the further out this is, the more of its shadows show. A real shoulder is about this far from the eye.
// How strong it is, and how wide, are THE LAMP'S LOOK, above: the game sends them to shaders/lightShapes.fxh. (Modified 2026-11-14 by Melanikus (Dietrich).)
enum
{
	kShoulderLampAcross = 300,
	kShoulderLampUp = -100,
	kShoulderLampAhead = 0,
	kShoulderLampMostStep = 1000,     // moving further than this in one frame, in millimetres, isn't walking: he's been put somewhere
	kShoulderLampRange = 20000,       // in millimetres, beyond which it lights nothing and throws no shadow. A longer reach means more shadow to work out each frame
	kShoulderLampRed = 65536,         // its colour: 65536 is all of that colour
	kShoulderLampGreen = 61000,
	kShoulderLampBlue = 53000
};

// What a saved key really is. A profile saved before the lamp holds neverSet in the lamp's byte, because nothing ever wrote to it. The menu refuses to give that key to anything, so it can only mean this profile has never heard of the lamp, and
// then it gets the default key, unless the player already uses that for something else, and then no key, so that nothing changes under them. Any other value is what the player chose, including noKey if they cleared it on purpose. Every value is safe
// to use, because the arrays it picks from have a place for every value a byte can hold.
unsigned char ShoulderLamp_KeyFromProfile(unsigned char stored, unsigned char neverSet, unsigned char defaultKey, bool defaultIsTaken, unsigned char noKey);

// How strong the lamp is at this moment, from about 0.65 to 1. It wanders by itself, with now and then a brief dip, like a small lamp with a tired battery. It always moves smoothly, so it can't strobe.
float ShoulderLamp_Flicker(double seconds);
