// The shoot tool, -shadowshoot. Written 2026-11-04 by Melanikus for the AvP port. See shadow_shoot.cpp. This is debug code, and it comes out before merge.

#ifndef SHADOW_SHOOT_H
#define SHADOW_SHOOT_H

#include <d3d9.h>
#include <stddef.h>

extern int ShadowShootOn;          // -shadowshoot. With this at 0, nothing here runs
extern bool ShadowShootFrame;      // true while a shot's frame is being drawn

enum { kShootMostCorners = 16 };

// What the ray hit. It's copied out of the game, so it can't go stale.
struct ShootTarget
{
	bool found;
	const void *shape;     // only ever compared, never read through
	int shapeNumber;
	int polygon;
	int moduleNumber;      // -1 if it isn't part of the level
	int behaviour;         // what the object is, from AVP_BEHAVIOUR_TYPE, or -1
	int flags;             // the polygon's PolyFlags
	bool isPartOfAModel;
	bool hasSimplerVersions;   // a part of a model that's swapped for a simpler shape at a distance, whose polygons are numbered differently
	bool partWasReplaced;      // a part of a model given another shape, so what's drawn isn't the shape the ray tested
	bool isMorphing;           // like a door: the ray tested it in between two shapes, and these corners are its first shape
	int distance;          // how far along the ray, as the game worked it out
	int point[3];          // where in the world, as the game worked it out
	float arrow[3];        // straight out of the side the ray hit
	bool hasPlane;         // its shape stores a plane for it: an arrow out of its back, and a distance
	float plane[4];
	int numCorners;
	float corners[kShootMostCorners * 3]; // in the world
};

// What's known about the frame, for the shot's log. view and cameraInWorld are read at once and aren't kept.
struct ShootFrameFacts
{
	int frame;
	const char *commit;
	const char *room;
	const float *view;
	const float *cameraInWorld;
	float projection11, projection22, projection33, projection43;
	int crosshairX, crosshairY;
	bool farCornersExact;   // -keptsnapfar
	bool cornersCut;        // kept and walked corners are finished by volumeCut.vsh
	bool testBoxesOn;       // -shadowtest, whose boxes are drawn but aren't anyone's polygon
};

// These three are in kshape.cpp, which knows the game's shapes. FindTarget sends a ray from the camera, along direction, a line 1 long in the world. ShapeNumber gives a shape's number
// in the game's list, or -1. CrosshairPixel gives where the sight is drawn, or -1 and -1 if this player has none.
void ShadowShoot_FindTarget(const float *direction, ShootTarget *target);
int ShadowShoot_ShapeNumber(const void *shape);
void ShadowShoot_CrosshairPixel(int *x, int *y);

// The keys. These two give back words for the screen. keptPrismsOn says whether a stop also moves its shape off the kept route.
void ShadowShoot_AskForShot();
const char *ShadowShoot_StopCasting(bool keptPrismsOn);
const char *ShadowShoot_UndoStop();

// This frame can't take a shot that was asked for. That press is dropped, so it can't go off later somewhere else. why is said in the log. Gives back words for the screen, or nothing if no shot was asked for.
const char *ShadowShoot_DropAskedShot(int frame, const char *why);

// Every frame that gets as far as its prisms, at the end. It says whether a new stop stopped anything. Gives back words for the screen, or nothing.
const char *ShadowShoot_EndOfEveryFrame();

bool ShadowShoot_IsStopped(const void *shape, int polygon);
bool ShadowShoot_ShapeHasStops(const void *shape);
int ShadowShoot_NumStopped();

// The level is ending: a polygon's number means nothing in the next one, so what's been shot and stopped goes.
void ShadowShoot_LevelEnded();

// The device is about to be reset, which Direct3D won't do while anything of the card's is still held.
void ShadowShoot_ReleaseCardThings();

// The frame. If a shot was asked for, this starts it: ShadowShootFrame goes true until ShadowShoot_EndFrame.
void ShadowShoot_StartFrame(LPDIRECT3DDEVICE9 device, const ShootFrameFacts &facts);

// One pass of one light. light is where it's cast from, and centre is where the light really is: both as on the light list.
void ShadowShoot_StartPass(int lightNumber, const float *light, const float *centre, bool cloakedOnly);

// Which way a prism gets to the card.
enum ShootRoute
{
	kShootRouteNone,
	kShootRouteSent,     // made this frame, on the view's numbers
	kShootRouteWalked,   // made this frame, on the world's numbers
	kShootRouteKept      // on the card already, and worked out again here because the card's copy can't be read
};

// One polygon's prism. corners is x y z for each corner, numCorners of them. These look for where the ray goes through it, and keep that for the log.
void ShadowShoot_CrossOnList(const float *corners, int numCorners, int polygonCorners, const void *shape, int polygon);

// The same for a prism in the world's numbers. ends holds each corner's KeptCorner end.
void ShadowShoot_CrossInWorld(const float *corners, const float *ends, int numCorners, int polygonCorners, const void *shape, int polygon, ShootRoute route, bool cut);

// Prisms that turned out not to be drawn take their crossings back with them.
size_t ShadowShoot_CrossingMark();
void ShadowShoot_ForgetCrossings(size_t mark);

// A kept prism was worked out again: does it have as many corners as the card's copy?
void ShadowShoot_NoteKeptGroup(int cornersOnCard, int cornersWorkedOut, bool whole);

// After a pass marks the stencil, and after it puts its share in the mask. These only read, and everything the card was set to is put back.
void ShadowShoot_AfterMarking(LPDIRECT3DDEVICE9 device);
void ShadowShoot_AfterShading(LPDIRECT3DDEVICE9 device, IDirect3DSurface9 *mask);

// Either side of a light being added per pixel, so the log can say what it added.
void ShadowShoot_BeforeAddingLight(LPDIRECT3DDEVICE9 device, int lightNumber);
void ShadowShoot_AfterAddingLight(LPDIRECT3DDEVICE9 device, int lightNumber);

// The end of the frame: writes the shot's block to the log. It writes on the mask, so it has to come after everything that reads the mask. The caller puts the card's settings back after this.
void ShadowShoot_EndFrame(LPDIRECT3DDEVICE9 device, const float *lights, int numLights, bool picturesDrawn, IDirect3DSurface9 *mask, IDirect3DSurface9 *depth, IDirect3DTexture9 *depthTexture);

// What to tint, every frame: the last shot, and each polygon that's stopped. number counts from 0 up, and false means there are no more. corners gets x y z in the world, with room for kShootMostCorners.
bool ShadowShoot_TintNumber(int number, float *corners, int *numCorners, bool *stopped);

#endif
