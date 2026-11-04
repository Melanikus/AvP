// Modified 2026-11-04 by Melanikus for the AvP port:
// added the kinds of polygon the shadow toy keeps, and how many lights it handles.
#ifndef _included_d3d_render_h_ /* Is this your first time? */
#define _included_d3d_render_h_ 1

#include "kshape.h"
#include "d3_func.h"

// The shadow toy keeps a copy of each polygon it may need at the end of the frame, and each copy has one of these kinds,
// which says what that polygon may do. The collector in kshape.cpp chooses the kind. The shadow pass in d3d_render.cpp obeys it.
enum ToyCasterKind
{
	kToyCasterLevel = 0,        // part of the level, such as a wall or a door: casts a shadow, and shadows fall on it
	kToyCasterObject = 1,       // a placed object, such as a crate or a machine: the same
	kToyCasterModel = 2,        // a body part of a model, such as an alien: the same
	kToyCasterCloaked = 3,      // a cloaked Predator: casts only a faint shadow
	kToyCasterCastOnly = 4,     // a wall that wasn't drawn this frame: casts a shadow, and is never drawn by the shadow pass
	kToyCasterKinds = 5
};

// How many lights the shadow toy handles in one frame. Steady lights are lamps and flares. Brief lights are muzzle flashes, fires
// and explosions, which get places of their own, so that one can't push out a steady light and make that light's shadows jump.
// shadow.psh and shadowFlare.psh are sized for kToyMostLights, so they have to change along with it.
enum
{
	kToyMostSteadyLights = 8,
	kToyMostBriefLights = 4,
	kToyMostLights = kToyMostSteadyLights + kToyMostBriefLights,

	// each light is 9 numbers: x y z, range, red green blue, its kind (0 baked into the walls, 1 live, in the game's lighting,
	// 2 live, added per pixel), then 1 if it's lit the way the game lights it, or 0 for the look chosen for flares
	kToyLightFloats = 9,

	// The furthest a light counts as reaching, in the game's units. A shadow has to end well inside the far plane, or the
	// stencil can't count it. A level can ask for more: 1,024,900 is in the stock levels.
	kToyMostRange = 100000
};

extern void D3D_DrawBackdrop();
extern void PostLandscapeRendering();
extern void D3D_DrawWaterTest(MODULE *testModulePtr);

extern void D3D_ZBufferedCloakedPolygon_Output         (POLYHEADER *inputPolyPtr, RENDERVERTEX *renderVerticesPtr);
extern void D3D_ZBufferedGouraudTexturedPolygon_Output (POLYHEADER *inputPolyPtr, RENDERVERTEX *renderVerticesPtr, bool isShapePolygon = false);
extern void D3D_ZBufferedGouraudPolygon_Output         (POLYHEADER *inputPolyPtr, RENDERVERTEX *renderVerticesPtr);
extern void D3D_ZBufferedTexturedPolygon_Output        (POLYHEADER *inputPolyPtr, RENDERVERTEX *renderVerticesPtr);
extern void D3D_PredatorThermalVisionPolygon_Output    (POLYHEADER *inputPolyPtr, RENDERVERTEX *renderVerticesPtr);
extern void D3D_BackdropPolygon_Output                 (POLYHEADER *inputPolyPtr, RENDERVERTEX *renderVerticesPtr);
extern void D3D_SkyPolygon_Output                      (POLYHEADER *inputPolyPtr, RENDERVERTEX *renderVerticesPtr);
extern void D3D_Decal_Output                           (DECAL *decalPtr,   RENDERVERTEX *renderVerticesPtr);
static void D3D_Particle_Output                        (PARTICLE *particlePtr, PARTICLEVERTEX *renderVerticesPtr);
extern void D3D_DrawParticle_Rain                      (PARTICLE *particlePtr, VECTORCH *prevPositionPtr);

void D3D_Rectangle(int x0, int y0, int x1, int y1, int r, int g, int b, int a);
extern void DrawMenuTextGlow(uint32_t topLeftX, uint32_t topLeftY, uint32_t size, uint32_t alpha);
extern void D3D_RenderHUDString(char *stringPtr, int x, int y, int colour);

extern void D3D_DecalSystem_Setup();
extern void D3D_DecalSystem_End();

extern void D3D_FadeDownScreen(int brightness, int colour);
extern void D3D_PlayerOnFireOverlay();

void D3D_DrawWaterPatch(int xOrigin, int yOrigin, int zOrigin);
void D3D_DrawWaterFall(int xOrigin, int yOrigin, int zOrigin);
void D3D_DrawMoltenMetalMesh_Unclipped();
static void D3D_OutputTriangles();

void ThisFramesRenderingHasBegun();
void ThisFramesRenderingHasFinished();
extern void D3D_DrawSliderBar(int x, int y, int alpha);
extern void D3D_DrawSlider(int x, int y, int alpha);
extern void D3D_FadeDownScreen(int brightness, int colour);

void D3D_Rectangle(int x0, int y0, int x1, int y1, int r, int g, int b, int a);

/*extern*/ void CheckWireFrameMode(int shouldBeOn);

extern void InitForceField();

#endif
