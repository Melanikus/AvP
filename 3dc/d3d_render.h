#ifndef _included_d3d_render_h_ /* Is this your first time? */
#define _included_d3d_render_h_ 1

#include "kshape.h"
#include "d3_func.h"

extern void D3D_DrawBackdrop();
extern void PostLandscapeRendering();
extern void D3D_DrawWaterTest(MODULE *testModulePtr);

extern void D3D_ZBufferedCloakedPolygon_Output         (POLYHEADER *inputPolyPtr, RENDERVERTEX *renderVerticesPtr);
extern void D3D_ZBufferedGouraudTexturedPolygon_Output (POLYHEADER *inputPolyPtr, RENDERVERTEX *renderVerticesPtr);
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
