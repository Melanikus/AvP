// Copyright (C) 2010 Barry Duncan. All Rights Reserved.
// The original author of this code can be contacted at: bduncan22@hotmail.com

// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
// INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
// FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// DEVELOPERS AND CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Modified 2026-11-05 by Melanikus for the AvP port:
//   a square picture with all its smaller copies
// Modified 2026-11-03 by Melanikus for the AvP port:
//   removed the line that pulled in the Xbox header, because the Xbox port's folder was deleted

#ifndef _renderer_h_
#define _renderer_h_

#include <stdint.h>
#include <string>
#include <vector>
#include "renderStates.h"

enum R_USAGE
{
	USAGE_DYNAMIC,
	USAGE_STATIC
};

typedef size_t texID_t;
typedef int32_t effectID_t;
typedef int32_t shaderID_t;

// forward declarations
class VertexBuffer;
class IndexBuffer;
class VertexDeclaration;
struct r_VertexShader;
struct r_PixelShader;
struct Texture;
struct AVPTEXTURE;

bool R_BeginScene();
bool R_EndScene();

// texture functions
bool R_SetTexture            (uint32_t stage, texID_t textureID);
void R_UnsetTexture          (texID_t textureID);
void R_ForgetTexture         (uint32_t stage);
bool R_LockTexture           (Texture &texture, uint8_t **data, uint32_t *pitch, enum TextureLock lockType);
bool R_UnlockTexture         (Texture &texture);
bool R_CreateTextureWithCopies (uint32_t side, uint32_t levels, Texture &texture);
bool R_LockTextureCopy       (Texture &texture, uint32_t level, uint8_t **data, uint32_t *pitch);
bool R_UnlockTextureCopy     (Texture &texture, uint32_t level);
bool R_CreateTexture         (uint32_t width, uint32_t height, uint32_t bitsPerPixel, enum TextureUsage usageType, Texture &texture);
void R_ReleaseTexture        (Texture &texture);
bool R_CreateTextureFromFile (const std::string &fileName, Texture &texture);
bool R_CreateTallFontTexture       (AVPTEXTURE &tex, enum TextureUsage usageType, Texture &texture);
bool R_CreateTextureFromAvPTexture (AVPTEXTURE &AvPTexture, enum TextureUsage usageType, Texture &texture);

// vertex buffer functions
bool R_CreateVertexBuffer  (class VertexBuffer &vertexBuffer);
bool R_ReleaseVertexBuffer (class VertexBuffer &vertexBuffer);
bool R_LockVertexBuffer    (class VertexBuffer &vertexBuffer, uint32_t offsetToLock, uint32_t sizeToLock, void **data, enum R_USAGE usage);
bool R_UnlockVertexBuffer  (class VertexBuffer &vertexBuffer);
bool R_SetVertexBuffer     (class VertexBuffer &vertexBuffer);
bool R_DrawPrimitive       (uint32_t numPrimitives);
//bool R_DrawIndexedPrimitive(uint32_t numVerts, uint32_t startIndex, uint32_t numPrimitives);
bool R_DrawIndexedPrimitive(uint32_t baseVertexIndex, uint32_t minIndex, uint32_t numVerts, uint32_t startIndex, uint32_t numPrimitives);

// index buffer functions
bool R_CreateIndexBuffer   (class IndexBuffer &indexBuffer);
bool R_ReleaseIndexBuffer  (class IndexBuffer &indexBuffer);
bool R_LockIndexBuffer     (class IndexBuffer &indexBuffer, uint32_t offsetToLock, uint32_t sizeToLock, uint16_t **data, enum R_USAGE usage);
bool R_UnlockIndexBuffer   (class IndexBuffer &indexBuffer);
bool R_SetIndexBuffer      (class IndexBuffer &indexBuffer);

// vertex declaration
bool R_CreateVertexDeclaration  (class VertexDeclaration *vertexDeclaration);
bool R_SetVertexDeclaration     (class VertexDeclaration &declaration);
bool R_ReleaseVertexDeclaration (class VertexDeclaration &declaration);

// vertex shader functions
bool R_CreateVertexShader      (const std::string &fileName, r_VertexShader &vertexShader, VertexDeclaration *vertexDeclaration);
bool R_SetVertexShader         (r_VertexShader &vertexShader);
void R_ReleaseVertexShader     (r_VertexShader &vertexShader);
bool R_SetVertexShaderConstant (r_VertexShader &vertexShader, uint32_t registerIndex, enum SHADER_CONSTANT type, const void *constantData);
bool R_UnsetVertexShader();

// pixel shader functions
bool R_CreatePixelShader  (const std::string &fileName, r_PixelShader &pixelShader);
bool R_SetPixelShader     (r_PixelShader &pixelShader);
void R_ReleasePixelShader (r_PixelShader &pixelShader);
bool R_SetPixelShaderConstant (r_PixelShader &pixelShader, uint32_t registerIndex, enum SHADER_CONSTANT type, const void *constantData);
bool R_UnsetPixelShader();

void R_NextVideoMode();
void R_PreviousVideoMode();
std::string& R_GetVideoModeDescription();
void R_SetCurrentVideoMode();

void ChangeTranslucencyMode   (enum TRANSLUCENCY_TYPE translucencyRequired);
void ChangeTextureAddressMode (uint32_t samplerIndex, enum TEXTURE_ADDRESS_MODE textureAddressMode);
void ChangeFilteringMode      (uint32_t samplerIndex, enum FILTERING_MODE_ID filteringRequired);
void ChangeZWriteEnable       (enum ZWRITE_ENABLE zWriteEnable);

void R_ClearZBuffer();

bool InitialiseDirect3D();
bool R_ChangeResolution		(uint32_t width, uint32_t height);
void DrawAlphaMenuQuad		(uint32_t topX, uint32_t topY, texID_t textureID, uint32_t alpha);
void DrawTallFontCharacter	(uint32_t topX, uint32_t topY, texID_t textureID, uint32_t texU, uint32_t texV, uint32_t charWidth, uint32_t charHeight, uint32_t alpha);
void DrawCloudTable			(uint32_t topX, uint32_t topY, uint32_t wordLength, uint32_t alpha);
void DrawFadeQuad			(uint32_t topX, uint32_t topY, uint32_t alpha);
void DrawSmallMenuCharacter (uint32_t topX, uint32_t topY, uint32_t texU, uint32_t texV, uint32_t red, uint32_t green, uint32_t blue, uint32_t alpha);
void DrawQuad				(uint32_t x, uint32_t y, uint32_t width, uint32_t height, texID_t textureID, uint32_t colour, enum TRANSLUCENCY_TYPE translucencyType);
void DrawFmvFrame			(uint32_t frameWidth, uint32_t frameHeight, const std::vector<texID_t> &textureIDs);
void DrawFontQuad			(uint32_t x, uint32_t y, uint32_t charWidth, uint32_t charHeight, texID_t textureID, float *uvArray, uint32_t colour, enum TRANSLUCENCY_TYPE translucencyType);

// A screen of text and menu pictures drawn into another swap chain, between the game's own frames: the chain's back buffer cleared to
// background (an ARGB colour), then everything fill(context) draws with Font_DrawText and the other screen-list calls, at width by height
// pixels, then the chain presented. The game never calls it; AVP_OpenInteng's panels do (R-DRAW-CHAIN-1, d3d_render.cpp says the rules).
// Gives back true when the chain was presented. Added 2026-11-14 by Melanikus (Dietrich) for the AvP port.
struct IDirect3DSwapChain9;
bool R_DrawScreenToChain	(IDirect3DSwapChain9 *chain, uint32_t width, uint32_t height, uint32_t background, void (*fill)(void *context), void *context);
// The next R_DrawScreenToChain also saves its chain's picture as drawn, before it's shown, as a PNG at path, for a proof run, which has
// nothing on the screen to capture. A path of 260 letters or more, or none, asks for nothing, and so cancels one asked for. Afterwards
// R_ChainPictureSaved says whether the last picture asked for was saved. Added 2026-11-14 by Melanikus (Dietrich) for the AvP port
// (R-DRAW-CHAIN-1 D3).
void R_SaveNextChainPicture	(const char *path);
bool R_ChainPictureSaved		();
// While the level editor adds its panels to the game's own frame (after the level, before the frame is shown), its text and menu calls ask for
// places in pixels, as they are while a level runs, and not on the menus' 640 by 480 grid, which is what they read outside a running level.
// Off again when it's done. R_DrawScreenToChain sets the same switch for its own list. Added 2026-11-15 by Melanikus (Dietrich) for the AvP port
// (R-DRAW-CHAIN-1 D4).
void R_SetScreenPlacesInPixels	(bool inPixels);
// The object the level editor has selected, as the game's display block, or 0 for none: it is drawn again in green over the level, a pass of
// its own (a see-through green over the solid modes, green lines in the wire modes), and nothing else changes. Only the editor sets it. Added
// 2026-11-15 by Melanikus (Dietrich) for the AvP port (R-DRAW-CHAIN-1 D5).
// The level editor's switch for the draw mode's fill: each drawing of the world lists sets the card's fill (lines or solid) from WireFrameMode itself, and
// the screen lists after it are solid. Without it the last drawing of a frame took the fill the HUD's adds had left, so the nearest polygons were solid
// in a wire mode. Only the editor sets it; the game never does. (d3d_render.cpp)
void R_SetEditorFillFollowsWire	(bool on);

// THE LEVEL EDITOR'S THREE ORTHOGRAPHIC VIEWS (inteng's track and point screens: top left the x/y view, top right the z/y view, bottom left the x/z view;
// bottom right is left to the 3D view). A drawing of its own over the level, in inteng's 640 by 480 screen laid over the box of the window the editor gives:
// every polygon edge of every module and object in play, as lines, with a black ground behind each quarter, a crosshair at the 3D cursor, and the
// selected objects in green. Only the editor turns it on; the game never does. Added 2026-11-15 by Melanikus (Dietrich) for the AvP port.
// A view's numbers are inteng's own view record (FUN_004f3e30): a place on the 640 by 480 screen (origin), world units a pixel stands for (scale), and the
// world place that origin shows (world); a world place maps to the screen as (world - worldOffset) / scale + origin, x across for the x/y and x/z views, z
// across and FLIPPED for the z/y view, y down, and z down FLIPPED for the x/z view.
struct EditorOrthoView
{
	int originX, originY;
	int scaleX, scaleY;
	int worldX, worldY, worldZ;
};
struct EditorOrtho
{
	bool on;
	bool showLevel;                      // true: the level's own polygons are drawn in the views too (a stand-in; inteng's screen shows only the track)
	EditorOrthoView views[3];            // 0: x across, y down; 1: z across (flipped), y down; 2: x across, z down (flipped)
	int boxLeft, boxTop, boxWidth, boxHeight;   // the window's pixels the 640 by 480 screen is laid over
	bool crossShown[3];                  // a crosshair in each view, at the screen place given (FUN_00553ab0's own drawing is not read: a small cross, ours)
	int crossX[3], crossY[3];
};
void R_SetEditorOrtho			(const EditorOrtho *settings);
// Lines drawn in the three views in the world places given (the track's own space on the track screens), each in its own ARGB colour: up to 4096; count 0 none
struct EditorOrthoLine
{
	int x0, y0, z0, x1, y1, z1;
	unsigned int colour;
};
void R_SetEditorOrthoLines		(const EditorOrthoLine *lines, int count);
// The level editor's 3D view in a box of the window, not the whole of it (the bottom right quarter of the three-view layout): the world's drawings go into
// that box of the window (x, y, width, height in pixels), with the projection's aspect made the box's own; the screen lists (HUD, panels) still go over the whole
// window. Off (on false) puts the game's own projection back. Only the editor sets it. Added 2026-11-15 by Melanikus (Dietrich) for the AvP port.
void R_SetEditorMainViewport	(bool on, int x, int y, int width, int height);
// The level editor's Untextured: every polygon of the level is drawn with one plain texel of the colour given (ARGB), so it is as the level's light makes it,
// whatever the draw mode. Off (false) draws the textures as the game does. Only the editor sets it. Added 2026-11-15 by Melanikus (Dietrich) for the AvP port.
void R_SetEditorUntextured		(bool on, unsigned int colour);
// The extent in the world of everything the three views draw, from the polygons of the display blocks in play: false when there is none yet
bool R_EditorOrthoBounds		(int mins[3], int maxs[3]);
struct displayblock;
void R_SetEditorSelectedBlocks	(const struct displayblock *const *blocks, int count);
// The same with a colour for each (ARGB; null colours: green): the editor's link colours of Module Linking, a module drawn in one, its linked modules in another
void R_SetEditorTintedBlocks	(const struct displayblock *const *blocks, const unsigned int *colours, int count);   // up to 256 objects, all drawn in green
int R_EditorSelectedItemsDrawn	();   // how many list items the last green pass drew, for the editor's report
void CreateScreenShotImage  ();
void DeRedTexture           (Texture &texture);
void SetTransforms();
uint32_t R_GetNumVideoModes();
char*    R_GetDeviceName();

#ifdef USE_D3D9
	#include <../win32/d3_func.h>
#endif

#ifdef USE_OPENGL
	#include "ogl_func.h" // doesn't exist yet
#endif

#ifdef USE_GLIDE
	#include "glide_func.h" // hehe
#endif

#endif // include guard