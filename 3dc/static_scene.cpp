// The level's walls, drawn from the fixed buffer in place of working them out each frame. Written 2026-11-04 by Melanikus for the AvP port.
//
// Each frame the game takes every polygon of every wall in view, turns its corners to face the camera and adds up its light, one corner at a time, on the
// processor. But a wall that never moves is the same every frame, so static_level.cpp put them all on the graphics card when the level loaded. On a frame
// that uses this route, the game's shape drawing never sees those walls. All that's left is to say which runs of the buffer to draw.
//
// WHICH FRAMES: only ordinary ones. The vision modes, the mirror and the cheats all change a wall's colour or corners in ways the buffer knows nothing about,
// so those frames go the old way, and so does a frame that's being compared. Decided once, before anything is drawn.
//
// WHICH WALLS: a module goes this way only if every polygon it draws is in the buffer. See-through and animated polygons aren't, so a room with any of those
// stays wholly on the old route, which is about one room in twenty-five. So does a module that isn't marked as already lit: StaticLevel only holds the light
// that was baked into a wall, and such a module would have the level's own lamps added to it live.
//
// WHEN THEY'RE DRAWN: after the solid part of the game's list, because the sky is at the start of that and walls have to cover it, and before decals, which
// need them behind them. If the list fills up and goes early, they go with that first part, or glass drawn early would stop the wall behind it being drawn.
// They're known before any of that, so this is always possible.
//
// LIGHT: the light baked into a wall is in the buffer. Lights that are added per pixel go on as they do for everything. The rest of the live lights, like a
// muzzle flash or a lamp that wasn't chosen, go in a table of numbers, and surfaceScene.vsh adds them up for each corner with the game's own sum. So all
// the lights are at hand on the card at once, and nothing is worked out corner by corner on the processor. The gamma curve is done there too, last, because
// a sum of light has to go through it after it's added up, and not before.
//
// IF THE CARD REFUSES: the old route has already been skipped, so that frame has lost its walls. So after one failure, this route stays off for the rest of the
// level, and one bad frame stays one bad frame.

#include "3dc.h"
#include "module.h"
#include "gamedef.h"

#include "static_scene.h"
#include "static_frame.h"
#include "surface_static.h"
#include "d3_func.h"
#include "renderer.h"
#include "VertexDeclaration.h"
#include "logString.h"
#include "user_profile.h"
#include "kshape.h"

#include <vector>

extern int WireFrameMode;
extern int DrawFullBright;
extern bool MirroringActive;
extern int RequestedGammaSetting;

bool StaticScene_ViewAllows();      // d3d_render.cpp: is this a view of the real picture, on a frame that isn't being compared?
bool ShadowReportIsDue();           // d3d_render.cpp: is this frame one the shadow pass reports on?

// What this route took, for the log, so that a run can say whether it really ran. A switch that's on and a route that said no look the same on the screen.
static int modulesTakenThisFrame = 0;
static int modulesNotWholeThisFrame = 0;
static int modulesNoRoomThisFrame = 0;
static int framesAsked = 0;
static int framesWithWalls = 0;
static int framesAskedInAllLevel = 0;
static int framesWithWallsInAllLevel = 0;
int StaticScene_GatherLights(float *table, int mostLights, bool *passedLimit);   // view.cpp, where the game's own list of lights lives

int StaticSceneOn = 0;              // -staticscene
int StaticSceneNoLiveLights = 0;    // -staticnolivelights

static const int kMostRanges = 16384;
static const int kMostLightsOnWalls = 64;   // Lights in surfaceScene.vsh holds this many

static StaticRange ranges[kMostRanges];
static int numRanges = 0;

static std::vector<uint32_t> frameTaken;   // by module number: the last frame that took it
static uint32_t frameNumber = 0;

static bool choosing = false;
static bool wallsAreDue = false;
static bool refused = false;               // something failed on this level, so this route is off until the next one
static int modulesNotLit = 0;
static bool toldAboutNotLit = false;

static void RefuseForThisLevel(char const *why)
{
	if (!refused)
	{
		LogString(std::string("static scene: ") + why + ", so the rest of this level goes the old way");
		refused = true;
	}
}

void StaticScene_StartLevel()
{
	refused = false;
	toldAboutNotLit = false;
	framesAsked = 0;
	framesWithWalls = 0;
	framesAskedInAllLevel = 0;
	framesWithWallsInAllLevel = 0;
}

int StaticScene_FramesAskedThisLevel()
{
	return framesAskedInAllLevel;
}

int StaticScene_FramesWithWallsThisLevel()
{
	return framesWithWallsInAllLevel;
}

// As often as the shadow pass's own report, so that they can be read side by side. Only with the switch on.
static void ReportIfDue()
{
	if (!StaticSceneOn || !ShadowReportIsDue())
	{
		return;
	}

	char line[220] = { 0 };

	_snprintf(line, sizeof(line) - 1, "static scene: %d of the last %d frames drew walls from the fixed buffer. This one: %d modules from the buffer, on the old route %d not all in the buffer, %d not lit, %d with no room left, %d runs",
		framesWithWalls, framesAsked, modulesTakenThisFrame, modulesNotWholeThisFrame, modulesNotLit, modulesNoRoomThisFrame, numRanges);
	LogString(line);

	framesAsked = 0;
	framesWithWalls = 0;
}

static bool FrameCanUseThisRoute()
{
	if (!StaticSceneOn || refused || !StaticLevel_Ready() || !StaticLevel_ColourBuffer() || !d3d.surfaceSceneDecl || d3d.surfaceSceneEffect == kNullEffectID)
	{
		return false;
	}

	if (CurrentVisionMode != VISION_MODE_NORMAL || MirroringActive || DrawFullBright || WireFrameMode || !StaticScene_ViewAllows())
	{
		return false;
	}

	if (MIRROR_CHEATMODE || TRIPTASTIC_CHEATMODE || UNDERWATER_CHEATMODE || DISCOINFERNO_CHEATMODE || MOTIONBLUR_CHEATMODE)
	{
		return false;
	}

	return true;
}

void StaticScene_StartChoosing()
{
	numRanges = 0;
	wallsAreDue = false;
	modulesNotLit = 0;
	modulesTakenThisFrame = 0;
	modulesNotWholeThisFrame = 0;
	modulesNoRoomThisFrame = 0;
	choosing = FrameCanUseThisRoute();

	if (StaticSceneOn)
	{
		framesAsked++;
		framesAskedInAllLevel++;
	}

	frameNumber++;

	// after four billion frames the count comes back round, and an old frame could pass for this one
	if (frameNumber == 0)
	{
		frameTaken.assign(frameTaken.size(), 0);
		frameNumber = 1;
	}
}

void StaticScene_OfferModule(const DISPLAYBLOCK *block)
{
	if (!choosing || !block || !block->ObMyModule)
	{
		return;
	}

	int const moduleNumber = block->ObMyModule->m_index;

	if (moduleNumber < 0 || !StaticLevel_ModuleIsWhole(moduleNumber))
	{
		modulesNotWholeThisFrame++;
		return;
	}

	if (!(block->ObFlags3 & ObFlag3_PreLit))
	{
		modulesNotLit++;
		return;
	}

	int count = 0;
	StaticRange const *moduleRanges = StaticLevel_RangesOf(moduleNumber, &count);

	if (count <= 0)
	{
		return;
	}

	// a module that doesn't fit stays on the old route, and the rest of the frame carries on
	if (count > kMostRanges - numRanges)
	{
		modulesNoRoomThisFrame++;
		return;
	}

	if ((size_t)moduleNumber >= frameTaken.size())
	{
		frameTaken.resize(moduleNumber + 1, 0);
	}

	frameTaken[moduleNumber] = frameNumber;
	modulesTakenThisFrame++;

	for (int i = 0; i < count; i++)
	{
		ranges[numRanges++] = moduleRanges[i];
	}
}

void StaticScene_FinishChoosing()
{
	if (!choosing)
	{
		ReportIfDue();
		return;
	}

	choosing = false;

	if (modulesNotLit && !toldAboutNotLit)
	{
		LogString("static scene: " + std::to_string(modulesNotLit) + " modules in view aren't marked as lit, so they stay on the old route");
		toldAboutNotLit = true;
	}

	// the most a card will draw in one go, which never changes, so it is only asked once
	static uint32_t mostTriangles = 0;

	if (!mostTriangles)
	{
		D3DCAPS9 caps;
		mostTriangles = SUCCEEDED(d3d.lpD3DDevice->GetDeviceCaps(&caps)) ? caps.MaxPrimitiveCount : 65535;
	}

	numRanges = StaticFrame_SortAndJoin(ranges, numRanges, mostTriangles);
	wallsAreDue = numRanges > 0;

	if (wallsAreDue)
	{
		framesWithWalls++;
		framesWithWallsInAllLevel++;
	}

	ReportIfDue();
}

bool StaticScene_TookModule(int moduleNumber)
{
	return moduleNumber >= 0 && (size_t)moduleNumber < frameTaken.size() && frameTaken[moduleNumber] == frameNumber;
}

bool StaticScene_TookAny()
{
	return numRanges > 0;
}

bool StaticScene_WallsAreDue()
{
	return wallsAreDue;
}

// The gamma slider and the table of lights, straight into surfaceScene.vsh's registers.
static void SetLightNumbers(LPDIRECT3DDEVICE9 device)
{
	static float table[kMostLightsOnWalls * 8];
	static bool toldAboutLimit = false;

	bool passedLimit = false;
	int const count = StaticSceneNoLiveLights ? 0 : StaticScene_GatherLights(table, kMostLightsOnWalls, &passedLimit);

	if (passedLimit && !toldAboutLimit)
	{
		LogString("static scene: more than " + std::to_string(kMostLightsOnWalls) + " live lights at once, so the walls go without the rest");
		toldAboutLimit = true;
	}

	float const gamma[4] = { (float)RequestedGammaSetting / 256.0f, 0.0f, 0.0f, 0.0f };
	device->SetVertexShaderConstantF(7, gamma, 1);

	if (count)
	{
		device->SetVertexShaderConstantF(8, table, count * 2);
	}

	// for an int register: how many times round, where the count starts, and how far it steps
	int const loop[4] = { count, 0, 1, 0 };
	device->SetVertexShaderConstantI(0, loop, 1);
}

// inScene says these go through the game's own calls, which remember what's set. The comparison runs inside the shadow pass, where the card is set by hand and
// put back at the end, so those calls would think nothing needed changing.
static bool DrawRuns(LPDIRECT3DDEVICE9 device, const StaticRange *runs, int count, effectID_t effect, bool inScene)
{
	d3d.effectSystem->SetActive(effect);
	SurfaceStatic_SetShaderNumbers(device);
	SetLightNumbers(device);

	bool worked = d3d.surfaceSceneDecl->Set();
	worked = SUCCEEDED(device->SetStreamSource(0, StaticLevel_VertexBuffer(), 0, sizeof(StaticVertex))) && worked;
	worked = SUCCEEDED(device->SetStreamSource(1, StaticLevel_ColourBuffer(), 0, sizeof(DWORD))) && worked;
	worked = SUCCEEDED(device->SetIndices(StaticLevel_IndexBuffer())) && worked;

	if (inScene)
	{
		ChangeTranslucencyMode(TRANSLUCENCY_OFF);
		ChangeFilteringMode(0, FILTERING_BILINEAR_ON);
		ChangeTextureAddressMode(0, TEXTURE_WRAP);
		ChangeZWriteEnable(ZWRITE_ENABLED);
	}
	else
	{
		device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
		device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
		device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
		device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	}

	// the game drops the backs of its polygons one at a time, and these are dropped by the card: surface_static.cpp says why it's this way round
	device->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);

	bool twoSided = false;

	for (int i = 0; worked && i < count; i++)
	{
		StaticRange const &run = runs[i];

		if (inScene)
		{
			R_SetTexture(0, run.texture);
		}
		else
		{
			device->SetTexture(0, Tex_GetTexture(run.texture));
		}

		if (run.twoSided != twoSided)
		{
			device->SetRenderState(D3DRS_CULLMODE, run.twoSided ? D3DCULL_NONE : D3DCULL_CW);
			twoSided = run.twoSided;
		}

		worked = SUCCEEDED(device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, run.baseVertex, run.firstVertex, run.vertexCount, run.firstIndex, run.triangles));
	}

	device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	device->SetStreamSource(1, NULL, 0, 0);

	return worked;
}

bool StaticScene_DrawForComparing(LPDIRECT3DDEVICE9 device, const StaticRange *runs, int count)
{
	return DrawRuns(device, runs, count, d3d.surfaceSceneCompareEffect, false);
}

void StaticScene_DrawWalls(LPDIRECT3DDEVICE9 device)
{
	wallsAreDue = false;

	if (!DrawRuns(device, ranges, numRanges, d3d.surfaceSceneEffect, true))
	{
		RefuseForThisLevel("the card refused the fixed buffer");
	}
}
