// The shoot tool. Written 2026-11-04 by Melanikus for the AvP port. This is debug code, behind -shadowshoot, and it comes out before merge.
//
// WHAT IT'S FOR: when a shadow flickers, nobody can tell by looking which polygon and which light are doing it. So point the crosshair at the spot and press F9. A ray goes out through
// that pixel, and the log gets a block saying what it hit, each light on that spot, and every shadow prism the ray goes through, in order, with how far each is from the wall in
// the depth buffer's own steps. A prism's cap sitting within about a step of the wall is the fight that makes a shadow flicker.
//
// F10 stops the polygon last shot from casting from any light, and F5 undoes the newest stop. The polygon shot shows green and a stopped one shows red, so anyone can see it's the one
// they meant. Stops and tints last until the level is unloaded, because a polygon's number only means something in its own level. A restart, a death or a load of the same
// level doesn't unload it, so they carry on through those, and the tag still counts them. A tint stays where it was put, even if the thing it was on has moved since.
//
// THE TWO SIDES OF A BLOCK: the crossings are worked out here by hand, from the same corners the card was given. Then the card is asked what its stencil counter really says at that pixel,
// and what the mask really holds. The two should agree. If they don't, that's a finding too, and a pass that disagrees also says how many triangles only just missed the ray.
//
// READING THE CARD: a depth buffer and a stencil can't be read directly in Direct3D 9, but a card can be asked "how many pixels passed?". So with the scissor down to the one pixel, a
// flat quad at a chosen depth passes or it doesn't, and halving the range each time finds the buffer's value in 25 goes. The stencil counter is found the same way, one bit at a time.
// Pictures can be copied back and read. All of that waits for the card, so a shot's frame is slow. A frame with no shot does none of it.
//
// A SHOT MUSTN'T CHANGE WHAT IT MEASURES: those test quads write nothing, not to the screen, the depth buffer or the stencil. Every setting is saved first and put back after, and the
// settings changed are compared with what they were, and the log says if any differ. The one exception is at the very end of the frame, when each lamp's share is run on the mask at that
// one pixel, and by then nothing needs the mask until the next frame clears it.
//
// A SHOT MUSTN'T HANG THE GAME: if the card doesn't answer a question within half a second, the shot asks nothing more of it, and the log says so. The whole shot also has a limit of a few seconds,
// after which the same happens. Copying a picture back can't be timed out, because Direct3D 9 only offers a call that waits, so one already started has to finish, but no more are started. The block always says
// how long the shot took. Before reading anything, the card is asked two questions with known answers. If it gets those wrong, nothing it says can be believed, so nothing is read.
//
// KEPT PRISMS are on the card and can't be read back, so on a shot's frame they're worked out again, the same way they were first made. That gives the same numbers, as long as the
// light and the shape haven't changed, and they can't have, because those are what a kept prism is found by. Their corner count is compared with the card's copy as a check.

#include "3dc.h"
#include "inline.h"
#include "module.h"
#include "strategy_def.h"
#include "gamedef.h"
#include "kshape.h"
#include "d3_func.h"
#include "d3d_render.h"
#include "logString.h"
#include "pixel_lights.h"
#include "surface_pictures.h"
#include "shadow_shoot.h"
#include "shadow_shoot_sums.h"
#include "shadow_volumes_card.h"

#include <algorithm>
#include <string>
#include <vector>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

int ShadowShootOn = 0;
bool ShadowShootFrame = false;

// Where a light's numbers are: see kToyLightFloats.
enum
{
	kShootLightRange = 3,
	kShootLightKind = 7,
	kShootLightLook = 8
};

// A triangle this close to the ray, without being crossed, could have been drawn on that pixel by the card anyway.
static const double kShootNearMissPixels = 0.1;

// A pass could cross a great many prisms. All of them count, but only this many are written out.
static const int kMostCrossingsWritten = 300;

struct ShootCrossing
{
	double depth;
	bool addsOne;
	const void *shape;
	int polygon;
	ShootPrismPart part;
	int side;
	ShootRoute route;
	double pixelsFromEdge;   // -1 if a corner is too close to the camera to say
};

struct ShootPass
{
	int lightNumber;
	int position;            // which of this light's passes
	bool cloaked;
	float castFrom[3];       // in the world
	float castFromOnList[3];
	std::vector<ShootCrossing> crossings;
	int nearMisses;
	int unknownLayouts;      // prisms not shaped as ToyAddPrism writes them, which would mean the names here can't be trusted
	int keptGroups;
	int keptCornersOnCard;
	int keptCornersWorkedOut;
	int keptGroupsNotWhole;
	bool stencilRead;
	bool inRectangle;
	int stencil;
	bool maskRead;
	float maskAfter;
};

static bool shotAsked = false;
static int shotsTaken = 0;

static const double kShotMostSeconds = 5.0;
static const double kOneAnswerMostSeconds = 0.5;

static IDirect3DQuery9 *shotQuery = 0;
static LARGE_INTEGER shotStartedAt;
static bool cardStoppedAnswering = false;
static bool shotRanOutOfTime = false;
static bool cardAnswersMakeNoSense = false;
static bool depthReadAsNothing = false;   // the depth buffer read as its very first level, which nothing drawn can be at
static int settingsNotSaved = 0;       // how many reads of this shot were skipped, because the card's settings couldn't be saved first
static int settingsNotPutBack = 0;     // how many times a setting wasn't what it had been, after being put back

static int shotPixelX = 0, shotPixelY = 0;
static float shotWidth = 0.0f, shotHeight = 0.0f;
static ShootRay shotRay = { 0.0, 0.0 };
static double pixelsPerAcross = 0.0, pixelsPerUp = 0.0;
static double nearestDepth = 64.0, furthestDepth = 1000000.0;
static float viewNumbers[12];
static float cameraPlace[3];
static double projection33 = 1.0, projection43 = -64.0;
static bool farExact = false;
static bool cutCorners = false;
static bool testBoxesOn = false;
static int frameNumber = 0;
static const char *frameCommit = "";
static const char *frameRoom = "";

static ShootTarget target;
static bool haveTarget = false;
static bool depthRead = false;
static double bufferLevel = 0.0;
static bool wallFromCorners = false;
static double wallDepthFromCorners = 0.0;

static std::vector<ShootPass> passes;
static float screenBefore[kToyMostLights][4];
static float screenAfter[kToyMostLights][4];
static bool screenRead[kToyMostLights];
static int passesOfLight[kToyMostLights];

static ShootStopList stops = { { 0 }, { 0 }, 0 };
static float stopCorners[kShootMostStops][kShootMostCorners * 3];
static int stopNumCorners[kShootMostStops];
static int matchesOfNewStop = 0;       // how many prisms the newest stop has kept from being made, since it was pressed
static bool newStopToReportOn = false;

static char wordsForTheScreen[64];

static void Write(const char *format, ...)
{
	char line[640];
	va_list arguments;

	va_start(arguments, format);
	_vsnprintf(line, sizeof(line) - 1, format, arguments);
	va_end(arguments);

	line[sizeof(line) - 1] = 0;
	LogString(line);
}

// ---- the keys ----

void ShadowShoot_AskForShot()
{
	shotAsked = true;
}

const char *ShadowShoot_DropAskedShot(int frame, const char *why)
{
	if (!shotAsked)
	{
		return 0;
	}

	shotAsked = false;
	Write("shoot: frame %d: no shot, because %s. The press is dropped", frame, why);

	return "SHOOT: NO SHOT ON THIS FRAME, SEE THE LOG";
}

const char *ShadowShoot_StopCasting(bool keptPrismsOn)
{
	wordsForTheScreen[sizeof(wordsForTheScreen) - 1] = 0;

	if (!haveTarget || !target.found)
	{
		return "SHOOT: NOTHING SHOT YET";
	}

	// The ray tested the shape in this part's plan, so the number means nothing on the shape it has now.
	if (target.partWasReplaced)
	{
		LogString("shoot: nothing stopped: this part of a model has been given another shape, so the polygon the ray found isn't a polygon of the shape it casts");
		return "SHOOT: THAT PART WAS REPLACED, NOTHING STOPPED";
	}

	int const place = stops.Add(target.shape, target.polygon);

	if (place < 0)
	{
		_snprintf(wordsForTheScreen, sizeof(wordsForTheScreen) - 1, (stops.count == kShootMostStops) ? "SHOOT: %d STOPPED, THE MOST THERE CAN BE" : "SHOOT: %d STOPPED, AND THAT ONE ALREADY IS", stops.count);
		return wordsForTheScreen;
	}

	memcpy(stopCorners[place], target.corners, sizeof(stopCorners[place]));
	stopNumCorners[place] = target.numCorners;

	matchesOfNewStop = 0;
	newStopToReportOn = true;

	Write("shoot: shape %d polygon %d no longer casts from any light, on every copy of that shape", target.shapeNumber, target.polygon);

	if (keptPrismsOn)
	{
		LogString("shoot: with kept prisms on, every polygon of that shape now goes the ordinary way, not just this one. So a flicker going away now may be that change of route, and not this polygon");
	}

	_snprintf(wordsForTheScreen, sizeof(wordsForTheScreen) - 1, "SHOOT: SHAPE %d POLYGON %d STOPPED", target.shapeNumber, target.polygon);
	return wordsForTheScreen;
}

const char *ShadowShoot_UndoStop()
{
	wordsForTheScreen[sizeof(wordsForTheScreen) - 1] = 0;

	if (!stops.TakeOffNewest())
	{
		return "SHOOT: NOTHING IS STOPPED";
	}

	// a stop that's gone has nothing left to report on, and what's left would be reported as if it were new
	newStopToReportOn = false;

	Write("shoot: the newest stop was undone, and %d are left", stops.count);

	_snprintf(wordsForTheScreen, sizeof(wordsForTheScreen) - 1, "SHOOT: UNDONE, %d STILL STOPPED", stops.count);
	return wordsForTheScreen;
}

bool ShadowShoot_IsStopped(const void *shape, int polygon)
{
	if (!stops.Has(shape, polygon))
	{
		return false;
	}

	if (stops.count > 0 && stops.shapes[stops.count - 1] == shape && stops.polygons[stops.count - 1] == polygon)
	{
		matchesOfNewStop++;
	}

	return true;
}

const char *ShadowShoot_EndOfEveryFrame()
{
	if (!newStopToReportOn)
	{
		return 0;
	}

	newStopToReportOn = false;

	if (matchesOfNewStop > 0)
	{
		Write("shoot: that stop matched %d polygons in this frame's lists, before each light's reach is asked. So it shows the stop found them, not that they would have cast", matchesOfNewStop);
		return 0;
	}

	LogString("shoot: THAT STOP MATCHED NOTHING on this frame. Either no light reaches that polygon just now, or it isn't what's being cast: a distant model casts a simpler shape, and a part can be given another");

	return "SHOOT: THAT STOP MATCHED NOTHING THIS FRAME";
}

bool ShadowShoot_ShapeHasStops(const void *shape)
{
	return stops.HasShape(shape);
}

int ShadowShoot_NumStopped()
{
	return stops.count;
}

bool ShadowShoot_TintNumber(int number, float *corners, int *numCorners, bool *stopped)
{
	if (number < 0)
	{
		return false;
	}

	if (number < stops.count)
	{
		memcpy(corners, stopCorners[number], sizeof(stopCorners[number]));
		*numCorners = stopNumCorners[number];
		*stopped = true;
		return true;
	}

	if (number == stops.count && haveTarget && target.found && !stops.Has(target.shape, target.polygon))
	{
		memcpy(corners, target.corners, sizeof(target.corners));
		*numCorners = target.numCorners;
		*stopped = false;
		return true;
	}

	return false;
}

void ShadowShoot_LevelEnded()
{
	stops.count = 0;
	haveTarget = false;
	shotAsked = false;
	newStopToReportOn = false;
}

void ShadowShoot_ReleaseCardThings()
{
	if (shotQuery)
	{
		shotQuery->Release();
		shotQuery = 0;
	}

	ShadowShootFrame = false;
	shotAsked = false;
	passes.clear();
}

// ---- saving the card's settings around a read ----

// Every setting SetOnePixelState changes, which are the ones worth checking afterwards.
static const D3DRENDERSTATETYPE kSettingsChanged[] =
{
	D3DRS_SCISSORTESTENABLE, D3DRS_COLORWRITEENABLE, D3DRS_ALPHABLENDENABLE, D3DRS_ALPHATESTENABLE, D3DRS_FOGENABLE, D3DRS_LIGHTING, D3DRS_CULLMODE, D3DRS_ZWRITEENABLE, D3DRS_ZENABLE,
	D3DRS_ZFUNC, D3DRS_TWOSIDEDSTENCILMODE, D3DRS_STENCILENABLE, D3DRS_STENCILFUNC, D3DRS_STENCILREF, D3DRS_STENCILMASK, D3DRS_STENCILFAIL, D3DRS_STENCILZFAIL, D3DRS_STENCILPASS, D3DRS_STENCILWRITEMASK
};

static const int kNumSettingsChanged = sizeof(kSettingsChanged) / sizeof(kSettingsChanged[0]);

struct SavedSettings
{
	IDirect3DStateBlock9 *block;
	DWORD values[kNumSettingsChanged];
	RECT scissor;
	IDirect3DVertexShader9 *vertexShader;
	IDirect3DPixelShader9 *pixelShader;
};

static bool SaveSettings(LPDIRECT3DDEVICE9 device, SavedSettings *saved)
{
	saved->block = 0;
	saved->vertexShader = 0;
	saved->pixelShader = 0;

	if (FAILED(device->CreateStateBlock(D3DSBT_ALL, &saved->block)))
	{
		settingsNotSaved++;
		return false;
	}

	for (int i = 0; i < kNumSettingsChanged; i++)
	{
		saved->values[i] = 0;
		device->GetRenderState(kSettingsChanged[i], &saved->values[i]);
	}

	device->GetScissorRect(&saved->scissor);
	device->GetVertexShader(&saved->vertexShader);
	device->GetPixelShader(&saved->pixelShader);

	return true;
}

static void PutSettingsBack(LPDIRECT3DDEVICE9 device, SavedSettings *saved)
{
	if (FAILED(saved->block->Apply()))
	{
		settingsNotPutBack++;
	}

	saved->block->Release();
	saved->block = 0;

	for (int i = 0; i < kNumSettingsChanged; i++)
	{
		DWORD now = 0;

		device->GetRenderState(kSettingsChanged[i], &now);

		if (now != saved->values[i])
		{
			settingsNotPutBack++;
		}
	}

	RECT scissorNow;
	IDirect3DVertexShader9 *vertexShaderNow = 0;
	IDirect3DPixelShader9 *pixelShaderNow = 0;

	device->GetScissorRect(&scissorNow);
	device->GetVertexShader(&vertexShaderNow);
	device->GetPixelShader(&pixelShaderNow);

	if (memcmp(&scissorNow, &saved->scissor, sizeof(RECT)) != 0 || vertexShaderNow != saved->vertexShader || pixelShaderNow != saved->pixelShader)
	{
		settingsNotPutBack++;
	}

	// asking for a shader holds on to it, so each one is let go
	if (vertexShaderNow) { vertexShaderNow->Release(); }
	if (pixelShaderNow) { pixelShaderNow->Release(); }
	if (saved->vertexShader) { saved->vertexShader->Release(); }
	if (saved->pixelShader) { saved->pixelShader->Release(); }
}

// ---- asking the card about one pixel ----

// Everything off but the one pixel. The next draw writes nothing: all it can do is pass or fail.
static void SetOnePixelState(LPDIRECT3DDEVICE9 device)
{
	RECT const pixel = { shotPixelX, shotPixelY, shotPixelX + 1, shotPixelY + 1 };

	device->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
	device->SetScissorRect(&pixel);
	device->SetVertexShader(NULL);
	device->SetPixelShader(NULL);
	device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
	device->SetRenderState(D3DRS_COLORWRITEENABLE, 0);
	device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
	device->SetRenderState(D3DRS_FOGENABLE, FALSE);
	device->SetRenderState(D3DRS_LIGHTING, FALSE);
	device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
	device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
	device->SetRenderState(D3DRS_TWOSIDEDSTENCILMODE, FALSE);
	device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
	device->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
	device->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
	device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
	device->SetRenderState(D3DRS_STENCILWRITEMASK, 0);
}

static double SecondsSince(const LARGE_INTEGER &then)
{
	LARGE_INTEGER now, ticksPerSecond;

	QueryPerformanceCounter(&now);
	QueryPerformanceFrequency(&ticksPerSecond);

	return (double)(now.QuadPart - then.QuadPart) / (double)ticksPerSecond.QuadPart;
}

// Is anything more to be asked of the card? This is what stops a shot once it has gone wrong or gone on too long.
static bool ShotMayAskMore()
{
	if (cardStoppedAnswering || cardAnswersMakeNoSense || shotRanOutOfTime)
	{
		return false;
	}

	if (SecondsSince(shotStartedAt) > kShotMostSeconds)
	{
		shotRanOutOfTime = true;
		return false;
	}

	return true;
}

// The card stopped answering, or the device has gone. Nothing more is asked of it for this shot, so that this costs one wait and no more.
static void GiveUpAsking()
{
	if (!shotRanOutOfTime && !cardAnswersMakeNoSense)
	{
		cardStoppedAnswering = true;
	}

	if (shotQuery)
	{
		shotQuery->Release();
		shotQuery = 0;
	}
}

// A quad at this depth, over the whole screen, though the scissor only lets one pixel through. Did that pixel pass? If the card doesn't say in half a second, this gives up, and it says false.
static bool PixelPasses(LPDIRECT3DDEVICE9 device, float depth)
{
	struct ScreenCorner
	{
		float x, y, z, rhw;
		DWORD colour;
	};

	if (!shotQuery)
	{
		return false;
	}

	if (!ShotMayAskMore())
	{
		GiveUpAsking();
		return false;
	}

	ScreenCorner const quad[4] =
	{
		{ 0.0f,      0.0f,       depth, 1.0f, 0 },
		{ shotWidth, 0.0f,       depth, 1.0f, 0 },
		{ 0.0f,      shotHeight, depth, 1.0f, 0 },
		{ shotWidth, shotHeight, depth, 1.0f, 0 },
	};

	shotQuery->Issue(D3DISSUE_BEGIN);
	device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(ScreenCorner));
	shotQuery->Issue(D3DISSUE_END);

	LARGE_INTEGER started;
	QueryPerformanceCounter(&started);

	DWORD counted = 0;
	HRESULT result;

	do
	{
		result = shotQuery->GetData(&counted, sizeof(counted), D3DGETDATA_FLUSH);
	} while (result == S_FALSE && SecondsSince(started) < kOneAnswerMostSeconds);

	if (result != S_OK)
	{
		GiveUpAsking();
		return false;
	}

	return counted > 0;
}

// Two questions with known answers. A quad can't fail with a depth test that always passes, and it can't pass with one that never does. A card or driver that gets these wrong has nothing to say that can be believed.
static bool CardAnswersMakeSense(LPDIRECT3DDEVICE9 device)
{
	device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
	device->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);

	bool const passedWhenItMust = PixelPasses(device, 0.5f);

	device->SetRenderState(D3DRS_ZFUNC, D3DCMP_NEVER);

	bool const passedWhenItCant = PixelPasses(device, 0.5f);

	// not answering is a different fault, and it's already been noted
	if (!shotQuery)
	{
		return true;
	}

	return passedWhenItMust && !passedWhenItCant;
}

// What's in the depth buffer at the pixel, in the buffer's steps. A quad passes if it's no further than what's there, so this looks for the furthest quad that still passes.
static bool ReadDepthBuffer(LPDIRECT3DDEVICE9 device, double *level)
{
	int lowest = 0, highest = 16777216;

	device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
	device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);

	while (lowest < highest && shotQuery)
	{
		int const middle = lowest + (highest - lowest + 1) / 2;

		if (PixelPasses(device, (float)((double)middle / 16777216.0)))
		{
			lowest = middle;
		}
		else
		{
			highest = middle - 1;
		}
	}

	*level = (double)lowest;
	return shotQuery != 0;
}

// The stencil counter at the pixel, one bit at a time.
static bool ReadStencil(LPDIRECT3DDEVICE9 device, int *value)
{
	int found = 0;

	device->SetRenderState(D3DRS_STENCILENABLE, TRUE);
	device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_EQUAL);

	for (int bit = 0; bit < 8 && shotQuery; bit++)
	{
		device->SetRenderState(D3DRS_STENCILREF, 1 << bit);
		device->SetRenderState(D3DRS_STENCILMASK, 1 << bit);

		if (PixelPasses(device, 0.0f))
		{
			found |= 1 << bit;
		}
	}

	*value = found;
	return shotQuery != 0;
}

// One pixel of a picture on the card, as up to four numbers. Everything comes out as a shader would read it. It copies the whole picture back, because that's all Direct3D 9 offers.
static bool ReadPixel(LPDIRECT3DDEVICE9 device, IDirect3DSurface9 *picture, float *numbers)
{
	D3DSURFACE_DESC size;
	IDirect3DSurface9 *copy = 0;
	D3DLOCKED_RECT locked;
	bool read = false;

	numbers[0] = numbers[1] = numbers[2] = numbers[3] = 0.0f;

	if (!ShotMayAskMore())
	{
		return false;
	}

	if (!picture || FAILED(picture->GetDesc(&size)) || shotPixelX < 0 || shotPixelY < 0 || (UINT)shotPixelX >= size.Width || (UINT)shotPixelY >= size.Height
		|| FAILED(device->CreateOffscreenPlainSurface(size.Width, size.Height, size.Format, D3DPOOL_SYSTEMMEM, &copy, NULL)))
	{
		return false;
	}

	RECT const pixel = { shotPixelX, shotPixelY, shotPixelX + 1, shotPixelY + 1 };

	if (SUCCEEDED(device->GetRenderTargetData(picture, copy)) && SUCCEEDED(copy->LockRect(&locked, &pixel, D3DLOCK_READONLY)))
	{
		read = true;

		if (size.Format == D3DFMT_R32F)
		{
			numbers[0] = *(float const *)locked.pBits;
		}
		else if (size.Format == D3DFMT_R16F || size.Format == D3DFMT_G16R16F || size.Format == D3DFMT_A16B16G16R16F)
		{
			int const count = (size.Format == D3DFMT_R16F) ? 1 : (size.Format == D3DFMT_G16R16F) ? 2 : 4;
			unsigned short const *halves = (unsigned short const *)locked.pBits;

			for (int i = 0; i < count; i++)
			{
				numbers[i] = Shoot_HalfFloat(halves[i]);
			}
		}
		else if (size.Format == D3DFMT_A2R10G10B10)
		{
			DWORD const all = *(DWORD const *)locked.pBits;

			numbers[0] = (float)((all >> 20) & 1023) / 1023.0f;
			numbers[1] = (float)((all >> 10) & 1023) / 1023.0f;
			numbers[2] = (float)(all & 1023) / 1023.0f;
			numbers[3] = (float)(all >> 30) / 3.0f;
		}
		else if (size.Format == D3DFMT_A8R8G8B8 || size.Format == D3DFMT_X8R8G8B8)
		{
			unsigned char const *bytes = (unsigned char const *)locked.pBits;

			numbers[0] = (float)bytes[2] / 255.0f;
			numbers[1] = (float)bytes[1] / 255.0f;
			numbers[2] = (float)bytes[0] / 255.0f;
			numbers[3] = (float)bytes[3] / 255.0f;
		}
		else
		{
			read = false;
		}

		copy->UnlockRect();
	}

	copy->Release();
	return read;
}

// ---- the frame ----

void ShadowShoot_StartFrame(LPDIRECT3DDEVICE9 device, const ShootFrameFacts &facts)
{
	if (!shotAsked)
	{
		return;
	}

	shotAsked = false;

	D3DVIEWPORT9 viewport;

	if (FAILED(device->GetViewport(&viewport)) || facts.projection11 == 0.0f || facts.projection22 == 0.0f || facts.projection33 == 0.0f || facts.projection43 == 0.0f)
	{
		LogString("shoot: couldn't read the view, so there's no shot");
		return;
	}

	// the crosshair's pixel, or the middle if the game's numbers for it are off the screen. A number below zero turns into a huge one here, so that catches those as well.
	shotPixelX = ((DWORD)facts.crosshairX < viewport.Width) ? facts.crosshairX : (int)(viewport.Width / 2);
	shotPixelY = ((DWORD)facts.crosshairY < viewport.Height) ? facts.crosshairY : (int)(viewport.Height / 2);

	shotWidth = (float)viewport.Width;
	shotHeight = (float)viewport.Height;

	// A pixel's middle in Direct3D 9 is exactly on its number, so the middle pixel of a screen of even size looks exactly straight ahead: lightShapes.fxh says the same in PositionHere.
	shotRay.across = ((double)shotPixelX * 2.0 / (double)viewport.Width - 1.0) / (double)facts.projection11;
	shotRay.up = (1.0 - (double)shotPixelY * 2.0 / (double)viewport.Height) / (double)facts.projection22;
	pixelsPerAcross = (double)facts.projection11 * (double)viewport.Width * 0.5;
	pixelsPerUp = (double)facts.projection22 * (double)viewport.Height * 0.5;

	projection33 = (double)facts.projection33;
	projection43 = (double)facts.projection43;
	nearestDepth = -projection43 / projection33;
	furthestDepth = (projection33 != 1.0) ? projection43 / (1.0 - projection33) : 1.0e30;

	memcpy(viewNumbers, facts.view, sizeof(viewNumbers));
	memcpy(cameraPlace, facts.cameraInWorld, sizeof(cameraPlace));
	farExact = facts.farCornersExact;
	cutCorners = facts.cornersCut;
	testBoxesOn = facts.testBoxesOn;
	frameNumber = facts.frame;
	frameCommit = facts.commit;
	frameRoom = facts.room;

	// that same ray in the world. Each row of the view is the way one of the camera's directions points in the world, and the list's y is the view's turned over.
	double along[3], length = 0.0;

	for (int i = 0; i < 3; i++)
	{
		along[i] = (double)viewNumbers[8 + i] + shotRay.across * (double)viewNumbers[0 + i] - shotRay.up * (double)viewNumbers[4 + i];
		length += along[i] * along[i];
	}

	length = sqrt(length);

	float const direction[3] = { (float)(along[0] / length), (float)(along[1] / length), (float)(along[2] / length) };

	ShadowShoot_FindTarget(direction, &target);
	haveTarget = true;
	shotsTaken++;

	// The polygon as the game cut it for drawing, in the fan the game draws it as: how far does the ray go before it meets that?
	wallFromCorners = false;

	for (int i = 1; target.found && i + 1 < target.numCorners && !wallFromCorners; i++)
	{
		float a[3], b[3], c[3];
		bool addsOne = false;

		Shoot_WorldToList(viewNumbers, &target.corners[0], a);
		Shoot_WorldToList(viewNumbers, &target.corners[i * 3], b);
		Shoot_WorldToList(viewNumbers, &target.corners[(i + 1) * 3], c);

		for (int axis = 0; axis < 3; axis++)
		{
			a[axis] = truncf(a[axis]);
			b[axis] = truncf(b[axis]);
			c[axis] = truncf(c[axis]);
		}

		wallFromCorners = Shoot_RayCrosses(a, b, c, shotRay, &wallDepthFromCorners, &addsOne);
	}

	passes.clear();
	memset(screenRead, 0, sizeof(screenRead));
	memset(passesOfLight, 0, sizeof(passesOfLight));

	depthRead = false;
	bufferLevel = 0.0;
	depthReadAsNothing = false;
	cardStoppedAnswering = false;
	shotRanOutOfTime = false;
	cardAnswersMakeNoSense = false;
	settingsNotSaved = 0;
	settingsNotPutBack = 0;
	QueryPerformanceCounter(&shotStartedAt);

	if (FAILED(device->CreateQuery(D3DQUERYTYPE_OCCLUSION, &shotQuery)))
	{
		shotQuery = 0;
		LogString("shoot: this card can't be asked how many pixels passed, so the depth buffer and the stencil won't be read");
	}

	if (shotQuery)
	{
		SavedSettings settings;

		if (SaveSettings(device, &settings))
		{
			SetOnePixelState(device);

			if (CardAnswersMakeSense(device))
			{
				depthRead = ReadDepthBuffer(device, &bufferLevel);

				// Level 0 is the near plane itself, and nothing is drawn there. A card saying that is more likely to be saying "no" to everything.
				if (depthRead && bufferLevel == 0.0)
				{
					depthRead = false;
					depthReadAsNothing = true;
				}
			}
			else
			{
				cardAnswersMakeNoSense = true;
				GiveUpAsking();
			}

			PutSettingsBack(device, &settings);
		}
	}

	ShadowShootFrame = true;
}

void ShadowShoot_StartPass(int lightNumber, const float *light, const float *centre, bool cloakedOnly)
{
	(void)centre;

	ShootPass pass;

	pass.lightNumber = lightNumber;
	pass.position = (lightNumber >= 0 && lightNumber < kToyMostLights) ? passesOfLight[lightNumber]++ : 0;
	pass.cloaked = cloakedOnly;
	memcpy(pass.castFrom, light + kToyLightWorld, sizeof(pass.castFrom));
	memcpy(pass.castFromOnList, light, sizeof(pass.castFromOnList));
	pass.nearMisses = 0;
	pass.unknownLayouts = 0;
	pass.keptGroups = 0;
	pass.keptCornersOnCard = 0;
	pass.keptCornersWorkedOut = 0;
	pass.keptGroupsNotWhole = 0;
	pass.stencilRead = false;
	pass.inRectangle = true;
	pass.stencil = 0;
	pass.maskRead = false;
	pass.maskAfter = 0.0f;

	passes.push_back(pass);
}

// ---- crossing prisms ----

static void CrossOneTriangle(ShootPass &pass, const float *a, const float *b, const float *c, int triangle, int polygonCorners, const void *shape, int polygon, ShootRoute route)
{
	double depth = 0.0;
	bool addsOne = false;

	if (!Shoot_RayCrosses(a, b, c, shotRay, &depth, &addsOne))
	{
		double const fromEdge = Shoot_PixelsFromEdge(a, b, c, shotRay, pixelsPerAcross, pixelsPerUp, nearestDepth);

		if (fromEdge >= 0.0 && fromEdge < kShootNearMissPixels)
		{
			pass.nearMisses++;
		}

		return;
	}

	// the card clips off anything closer than the near plane or past the far one, and that part makes no pixels
	if (depth < nearestDepth || depth > furthestDepth)
	{
		return;
	}

	ShootCrossing crossing;

	crossing.depth = depth;
	crossing.addsOne = addsOne;
	crossing.shape = shape;
	crossing.polygon = polygon;
	crossing.part = Shoot_PrismPartOf(triangle, polygonCorners, &crossing.side);
	crossing.route = route;
	crossing.pixelsFromEdge = Shoot_PixelsFromEdge(a, b, c, shotRay, pixelsPerAcross, pixelsPerUp, nearestDepth);

	pass.crossings.push_back(crossing);
}

void ShadowShoot_CrossOnList(const float *corners, int numCorners, int polygonCorners, const void *shape, int polygon)
{
	if (passes.empty())
	{
		return;
	}

	ShootPass &pass = passes.back();

	if (numCorners != Shoot_CornersOfPrism(polygonCorners))
	{
		pass.unknownLayouts++;
	}

	for (int i = 0; i + 2 < numCorners; i += 3)
	{
		CrossOneTriangle(pass, &corners[i * 3], &corners[(i + 1) * 3], &corners[(i + 2) * 3], i / 3, polygonCorners, shape, polygon, kShootRouteSent);
	}
}

void ShadowShoot_CrossInWorld(const float *corners, const float *ends, int numCorners, int polygonCorners, const void *shape, int polygon, ShootRoute route, bool cut)
{
	if (passes.empty())
	{
		return;
	}

	ShootPass &pass = passes.back();

	if (numCorners != Shoot_CornersOfPrism(polygonCorners))
	{
		pass.unknownLayouts++;
	}

	for (int i = 0; i + 2 < numCorners; i += 3)
	{
		float onList[3][3];

		for (int corner = 0; corner < 3; corner++)
		{
			int const number = i + corner;

			if (cut)
			{
				Shoot_FinishCutCorner(viewNumbers, pass.castFromOnList, farExact, &corners[number * 3], ends[number], kKeptCornerFarMark, onList[corner]);
			}
			else
			{
				Shoot_WorldToList(viewNumbers, &corners[number * 3], onList[corner]);
			}
		}

		CrossOneTriangle(pass, onList[0], onList[1], onList[2], i / 3, polygonCorners, shape, polygon, route);
	}
}

size_t ShadowShoot_CrossingMark()
{
	return passes.empty() ? 0 : passes.back().crossings.size();
}

void ShadowShoot_ForgetCrossings(size_t mark)
{
	if (!passes.empty() && passes.back().crossings.size() > mark)
	{
		passes.back().crossings.resize(mark);
	}
}

void ShadowShoot_NoteKeptGroup(int cornersOnCard, int cornersWorkedOut, bool whole)
{
	if (passes.empty())
	{
		return;
	}

	ShootPass &pass = passes.back();

	pass.keptGroups++;
	pass.keptCornersOnCard += cornersOnCard;
	pass.keptCornersWorkedOut += cornersWorkedOut;
	pass.keptGroupsNotWhole += whole ? 0 : 1;
}

// ---- after each stage of a pass ----

void ShadowShoot_AfterMarking(LPDIRECT3DDEVICE9 device)
{
	if (passes.empty() || !shotQuery)
	{
		return;
	}

	ShootPass &pass = passes.back();
	DWORD scissorOn = FALSE;
	RECT scissor = { 0, 0, 0, 0 };

	device->GetRenderState(D3DRS_SCISSORTESTENABLE, &scissorOn);
	device->GetScissorRect(&scissor);

	// Outside this light's rectangle, the stencil wasn't cleared or marked by this pass. What's there is left over from another light.
	pass.inRectangle = !scissorOn || (shotPixelX >= scissor.left && shotPixelX < scissor.right && shotPixelY >= scissor.top && shotPixelY < scissor.bottom);

	SavedSettings settings;

	if (SaveSettings(device, &settings))
	{
		SetOnePixelState(device);
		pass.stencilRead = ReadStencil(device, &pass.stencil);
		PutSettingsBack(device, &settings);
	}
}

void ShadowShoot_AfterShading(LPDIRECT3DDEVICE9 device, IDirect3DSurface9 *mask)
{
	if (passes.empty())
	{
		return;
	}

	ShootPass &pass = passes.back();
	float numbers[4];

	if (ReadPixel(device, mask, numbers))
	{
		pass.maskRead = true;
		pass.maskAfter = numbers[0];
	}
}

void ShadowShoot_BeforeAddingLight(LPDIRECT3DDEVICE9 device, int lightNumber)
{
	IDirect3DSurface9 *screen = 0;

	if (lightNumber >= 0 && lightNumber < kToyMostLights && SUCCEEDED(device->GetRenderTarget(0, &screen)))
	{
		screenRead[lightNumber] = ReadPixel(device, screen, screenBefore[lightNumber]);
		screen->Release();
	}
}

void ShadowShoot_AfterAddingLight(LPDIRECT3DDEVICE9 device, int lightNumber)
{
	IDirect3DSurface9 *screen = 0;

	if (lightNumber >= 0 && lightNumber < kToyMostLights && SUCCEEDED(device->GetRenderTarget(0, &screen)))
	{
		screenRead[lightNumber] = screenRead[lightNumber] && ReadPixel(device, screen, screenAfter[lightNumber]);
		screen->Release();
	}
}

// ---- the log ----

static const char *RouteWord(ShootRoute route)
{
	return (route == kShootRouteKept) ? "kept" : (route == kShootRouteWalked) ? "walked" : "sent";
}

static const char *PartWord(ShootPrismPart part)
{
	return (part == kShootNearCap) ? "NEAR CAP" : (part == kShootFarCap) ? "FAR CAP" : "side";
}

static bool CrossingIsNearer(const ShootCrossing &a, const ShootCrossing &b)
{
	return a.depth < b.depth;
}

// A light's share, which is what it takes out of this pixel when it's blocked. It runs the game's own shader, with the pixel's stencil set so that it's marked, on the mask, which
// nothing needs any more this frame.
static bool ReadShare(LPDIRECT3DDEVICE9 device, const float *light, IDirect3DSurface9 *mask, IDirect3DTexture9 *depthTexture, const D3DVIEWPORT9 &viewport, float *share)
{
	RECT const pixel = { shotPixelX, shotPixelY, shotPixelX + 1, shotPixelY + 1 };
	D3DRECT const clearing = { shotPixelX, shotPixelY, shotPixelX + 1, shotPixelY + 1 };
	float numbers[4];

	if (!ShotMayAskMore())
	{
		return false;
	}

	device->SetRenderTarget(0, mask);
	device->SetViewport(&viewport);
	device->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
	device->SetScissorRect(&pixel);
	device->SetRenderState(D3DRS_STENCILWRITEMASK, 0xFFFFFFFF);
	device->SetRenderState(D3DRS_STENCILMASK, 0xFFFFFFFF);

	if (FAILED(device->Clear(1, &clearing, D3DCLEAR_TARGET | D3DCLEAR_STENCIL, 0, 1.0f, 1))
		|| FAILED(PixelLights_AddShareToMask(device, light, 1.0f, depthTexture))
		|| !ReadPixel(device, mask, numbers))
	{
		return false;
	}

	*share = numbers[0];
	return true;
}

// One pass's lines in the block. It adds to the shot's counts as it goes.
static void WritePass(ShootPass &pass, double wallLevel, double wallDepth, int *marked, int *passesRead, int *disagreements, int *fights, double *closestCap)
{
	std::sort(pass.crossings.begin(), pass.crossings.end(), CrossingIsNearer);

	ShootTally tally = { 0, 0, 0 };

	for (size_t c = 0; c < pass.crossings.size(); c++)
	{
		ShootCrossing const &crossing = pass.crossings[c];

		Shoot_CountCrossing(tally, crossing.addsOne, Shoot_DepthLevel(crossing.depth, projection33, projection43) - wallLevel);
	}

	char stencilWords[120] = "The stencil wasn't read";

	if (!pass.inRectangle)
	{
		_snprintf(stencilWords, sizeof(stencilWords) - 1, "This pixel is outside this light's rectangle, so nothing is marked here");
	}
	else if (pass.stencilRead)
	{
		bool const agrees = Shoot_TallyAgrees(tally, pass.stencil);

		*disagreements += agrees ? 0 : 1;
		*passesRead += 1;
		*marked += (pass.stencil != 0) ? 1 : 0;

		_snprintf(stencilWords, sizeof(stencilWords) - 1, "The stencil says %d, so it's %s. %s", pass.stencil, pass.stencil ? "MARKED" : "not marked", agrees ? "They agree" : "THEY DISAGREE");
	}

	char maskWords[64] = "";

	if (pass.maskRead)
	{
		_snprintf(maskWords, sizeof(maskWords) - 1, ". The mask after this pass is %.4f", pass.maskAfter);
	}

	Write("    pass %d%s, cast from %.1f %.1f %.1f: %u crossings add up to %d, with +%d and -%d too close to call. %s%s", pass.position, pass.cloaked ? " (cloaked things only)" : "", pass.castFrom[0], pass.castFrom[1], pass.castFrom[2],
		(unsigned int)pass.crossings.size(), tally.sure, tally.mayAdd, tally.mayTake, stencilWords, maskWords);

	if (pass.nearMisses > 0)
	{
		Write("      %d triangles missed the ray by under %.1f of a pixel, and the card could have drawn them on this pixel", pass.nearMisses, kShootNearMissPixels);
	}

	if (pass.unknownLayouts > 0)
	{
		Write("      %d prisms weren't laid out as ToyAddPrism writes them, so the names of their parts below can't be trusted", pass.unknownLayouts);
	}

	if (pass.keptGroups > 0)
	{
		Write("      kept: %d groups, %d corners on the card, %d worked out again here%s%s", pass.keptGroups, pass.keptCornersOnCard, pass.keptCornersWorkedOut,
			(pass.keptCornersOnCard != pass.keptCornersWorkedOut) ? ". THEY DIFFER" : "", (pass.keptGroupsNotWhole > 0) ? ". SOME WEREN'T WHOLE" : "");
	}

	for (size_t c = 0; c < pass.crossings.size(); c++)
	{
		ShootCrossing const &crossing = pass.crossings[c];
		double const stepsBehind = Shoot_DepthLevel(crossing.depth, projection33, projection43) - wallLevel;
		bool const close = fabs(stepsBehind) <= kShootUnsureSteps;
		bool const isCap = crossing.part != kShootSide;

		if (isCap && fabs(stepsBehind) < fabs(*closestCap))
		{
			*closestCap = stepsBehind;
		}

		*fights += (close && isCap) ? 1 : 0;

		if ((int)c >= kMostCrossingsWritten)
		{
			continue;
		}

		char sideWords[24] = "";

		if (crossing.part == kShootSide)
		{
			_snprintf(sideWords, sizeof(sideWords) - 1, " %d", crossing.side);
		}

		char edgeWords[40] = "";

		if (crossing.pixelsFromEdge >= 0.0 && crossing.pixelsFromEdge < 0.25)
		{
			_snprintf(edgeWords, sizeof(edgeWords) - 1, ", %.3f pixels from its edge", crossing.pixelsFromEdge);
		}

		Write("      %.3f along: %s%s, %s, %s, so %s, shape %d polygon %d. %+.3f units behind the wall, %+.2f steps%s%s", crossing.depth * sqrt(1.0 + shotRay.across * shotRay.across + shotRay.up * shotRay.up), PartWord(crossing.part), sideWords, RouteWord(crossing.route),
			crossing.addsOne ? "facing away" : "facing the camera", crossing.addsOne ? "adds one" : "takes one", ShadowShoot_ShapeNumber(crossing.shape), crossing.polygon, crossing.depth - wallDepth, stepsBehind, edgeWords, (close && isCap) ? "  <-- FIGHT" : "");
	}

	if ((int)pass.crossings.size() > kMostCrossingsWritten)
	{
		Write("      and %d more, which were counted but aren't listed", (int)pass.crossings.size() - kMostCrossingsWritten);
	}
}

void ShadowShoot_EndFrame(LPDIRECT3DDEVICE9 device, const float *lights, int numLights, bool picturesDrawn, IDirect3DSurface9 *mask, IDirect3DSurface9 *depth, IDirect3DTexture9 *depthTexture)
{
	if (!ShadowShootFrame)
	{
		return;
	}

	ShadowShootFrame = false;

	// everything from the pictures, before the mask is used for anything else
	float pictureDepth = 0.0f, pictureNormal[3] = { 0.0f, 0.0f, 0.0f };
	bool havePictureDepth = false, havePictureNormal = false;
	float numbers[4];

	// The depth picture starts each frame at 1, and nothing real is that close, because the near plane is at 64. So 1 or less means no surface was drawn here. It doesn't mean something far away.
	bool pictureHoldsNothing = false;

	if (picturesDrawn && ReadPixel(device, depth, numbers))
	{
		pictureDepth = numbers[0];
		havePictureDepth = pictureDepth > 1.0f;
		pictureHoldsNothing = !havePictureDepth;
	}

	if (picturesDrawn && SurfacePictures_Normal())
	{
		IDirect3DSurface9 *normals = 0;

		if (SUCCEEDED(SurfacePictures_Normal()->GetSurfaceLevel(0, &normals)))
		{
			if (ReadPixel(device, normals, numbers))
			{
				for (int axis = 0; axis < 3; axis++)
				{
					pictureNormal[axis] = numbers[axis] * 2.0f - 1.0f;
				}

				havePictureNormal = true;
			}

			normals->Release();
		}
	}

	IDirect3DSurface9 *screen = 0;
	float finalPixel[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	bool haveFinalPixel = false;
	D3DVIEWPORT9 viewport;
	bool const haveScreen = SUCCEEDED(device->GetRenderTarget(0, &screen)) && SUCCEEDED(device->GetViewport(&viewport));

	if (haveScreen)
	{
		haveFinalPixel = ReadPixel(device, screen, finalPixel);
	}

	float shares[kToyMostLights];
	bool shareRead[kToyMostLights];

	memset(shareRead, 0, sizeof(shareRead));

	if (haveScreen && picturesDrawn && mask)
	{
		for (int i = 0; i < numLights && i < kToyMostLights; i++)
		{
			float const *light = &lights[i * kToyLightFloats];

			// a light added per pixel isn't a share of anything: the mask just says how much of it is blocked
			if (light[kShootLightKind] != 2.0f && !LightOnListIsDark(light) && light[kShootLightRange] > 0.0f)
			{
				shareRead[i] = ReadShare(device, light, mask, depthTexture, viewport, &shares[i]);
			}
		}
	}

	bool targetPutBack = true;

	if (screen)
	{
		targetPutBack = SUCCEEDED(device->SetRenderTarget(0, screen)) && (!haveScreen || SUCCEEDED(device->SetViewport(&viewport)));
		screen->Release();
	}

	if (shotQuery)
	{
		shotQuery->Release();
		shotQuery = 0;
	}

	// How far away the wall is, for working out distances from it. The depth buffer's own value is the one that matters, because that's what the card compares against.
	// Without it, the next best is the picture, and then the polygon's corners. With none of them, everything stays far away, as an empty depth buffer is.
	double wallDepth = havePictureDepth ? (double)pictureDepth : wallFromCorners ? wallDepthFromCorners : furthestDepth;
	double wallLevel = Shoot_DepthLevel(wallDepth, projection33, projection43);

	if (depthRead)
	{
		wallLevel = bufferLevel;
		wallDepth = Shoot_DepthOfLevel(bufferLevel, projection33, projection43);
	}

	double const stepHere = wallDepth * wallDepth / (-projection43 * 16777216.0);

	Write("SHOT %d, frame %d, commit %s, room %s: pixel %d %d, ray across %.6f up %.6f", shotsTaken, frameNumber, frameCommit, frameRoom, shotPixelX, shotPixelY, shotRay.across, shotRay.up);
	Write("  camera at %.0f %.0f %.0f. Kept and walked corners are %s", cameraPlace[0], cameraPlace[1], cameraPlace[2], cutCorners ? (farExact ? "cut, far ends exact" : "cut") : "exact");

	Write("  this shot took %.0f milliseconds, up to the start of this block. Its limit is %.0f seconds", SecondsSince(shotStartedAt) * 1000.0, kShotMostSeconds);

	if (cardStoppedAnswering)
	{
		LogString("  THE CARD STOPPED ANSWERING during this shot, so nothing more was asked of it: anything below that wasn't read is why");
	}

	if (shotRanOutOfTime)
	{
		LogString("  THIS SHOT RAN OUT OF TIME, so nothing more was asked of the card: anything below that wasn't read is why");
	}

	if (cardAnswersMakeNoSense)
	{
		LogString("  THE CARD'S ANSWERS CAN'T BE BELIEVED: a test that had to pass didn't, or one that couldn't pass did. So the depth buffer and the stencil weren't read");
	}

	if (depthReadAsNothing)
	{
		LogString("  THE DEPTH BUFFER READ AS ITS VERY FIRST LEVEL, where nothing can be drawn, so that isn't believed. Distances below come from the next best there is");
	}

	if (testBoxesOn)
	{
		LogString("  -shadowtest is on: its boxes are drawn, but they're nobody's polygon, so they aren't in the crossings. A pass that disagrees may only be that");
	}

	if (settingsNotSaved > 0)
	{
		Write("  THE CARD'S SETTINGS COULDN'T BE SAVED %d times, so those reads were skipped", settingsNotSaved);
	}

	if (settingsNotPutBack > 0 || !targetPutBack)
	{
		Write("  SOMETHING WASN'T PUT BACK as it had been: %d settings, and the picture being drawn on %s, so this shot may have changed what it measured", settingsNotPutBack, targetPutBack ? "was fine" : "WAS NOT");
	}

	if (target.found)
	{
		Write("  hit: shape %d polygon %d, %s %d, behaviour %d%s, flags 0x%08X, %d corners", target.shapeNumber, target.polygon, (target.moduleNumber >= 0) ? "module" : "not a module", target.moduleNumber,
			target.behaviour, target.isPartOfAModel ? ", part of a model" : "", (unsigned int)target.flags, target.numCorners);
		Write("  point %d %d %d, %d from the camera. The side hit faces %.4f %.4f %.4f", target.point[0], target.point[1], target.point[2], target.distance, target.arrow[0], target.arrow[1], target.arrow[2]);

		if (target.isPartOfAModel)
		{
			LogString("  THIS IS PART OF A MODEL, not a wall");
		}

		if (target.hasSimplerVersions)
		{
			LogString("  THIS PART HAS SIMPLER VERSIONS for a distance, numbered differently, so what it casts may not be this polygon. A stop may match nothing");
		}

		if (target.partWasReplaced)
		{
			LogString("  THIS PART WAS GIVEN ANOTHER SHAPE. The ray tested the shape in its plan, so this polygon, its corners and the tint may not be what's on the screen");
		}

		if (target.isMorphing)
		{
			LogString("  THIS IS MORPHING, like a door. The ray tested it in between two shapes, and these corners are its first shape standing still, so the tint and the distance by its corners can be off");
		}

		if (target.hasPlane)
		{
			float const cameraSide = target.plane[0] * cameraPlace[0] + target.plane[1] * cameraPlace[1] + target.plane[2] * cameraPlace[2] - target.plane[3];

			Write("  the camera is %.1f from its flat surface, on its %s. A one-sided polygon seen from its back isn't drawn, so then the ray has found something other than what's on screen", fabsf(cameraSide), (cameraSide > 0.0f) ? "BACK" : "front");
		}

		for (int i = 0; i < target.numCorners; i++)
		{
			Write("    corner %d: %.0f %.0f %.0f", i, target.corners[i * 3 + 0], target.corners[i * 3 + 1], target.corners[i * 3 + 2]);
		}
	}
	else
	{
		LogString("  the ray hit nothing the game can shoot, so everything below is about the pixel, and there's no polygon to name");
	}

	char byBuffer[64] = "UNREAD", byPicture[64] = "UNREAD", byCorners[64] = "the ray missed them";

	// the depth buffer starts each frame at its last level
	if (depthRead && bufferLevel >= 16777215.0)
	{
		_snprintf(byBuffer, sizeof(byBuffer) - 1, "NOTHING was drawn at this pixel");
	}
	else if (depthRead)
	{
		_snprintf(byBuffer, sizeof(byBuffer) - 1, "%.2f (level %.0f)", wallDepth, wallLevel);
	}

	if (havePictureDepth)
	{
		_snprintf(byPicture, sizeof(byPicture) - 1, "%.2f", pictureDepth);
	}
	else if (pictureHoldsNothing)
	{
		_snprintf(byPicture, sizeof(byPicture) - 1, "NOTHING is held for this pixel");
	}

	if (wallFromCorners)
	{
		_snprintf(byCorners, sizeof(byCorners) - 1, "%.2f", wallDepthFromCorners);
	}

	Write("  how far ahead the wall is. By the depth buffer: %s. By the depth picture: %s. By the polygon's cut corners: %s", byBuffer, byPicture, byCorners);
	Write("  one depth step there is %.3f units. A triangle within %.1f steps of the wall is too close to call", stepHere, kShootUnsureSteps);

	if (havePictureNormal)
	{
		Write("  the normal picture says %.3f %.3f %.3f, in the view's numbers", pictureNormal[0], pictureNormal[1], pictureNormal[2]);
	}

	if (haveFinalPixel)
	{
		Write("  on the screen: %.3f %.3f %.3f, before glass and the HUD", finalPixel[0], finalPixel[1], finalPixel[2]);
	}

	float const hitPlace[3] = { (float)target.point[0], (float)target.point[1], (float)target.point[2] };
	int fights = 0, disagreements = 0, passesRead = 0;
	double closestCap = 1.0e30;

	for (int i = 0; i < numLights && i < kToyMostLights; i++)
	{
		float const *light = &lights[i * kToyLightFloats];
		float const *place = light + kToyLightWorld;

		if (LightOnListIsDark(light))
		{
			Write("  LIGHT %d: nothing in this place on the list, or a lamp that's dark for the moment", i);
			continue;
		}

		float const dx = place[0] - hitPlace[0], dy = place[1] - hitPlace[1], dz = place[2] - hitPlace[2];
		float const howFar = sqrtf(dx * dx + dy * dy + dz * dz);
		bool const isFlare = light[kShootLightKind] == 2.0f && light[kShootLightLook] == 0.0f;
		float const flatTurned = target.found ? (target.arrow[0] * dx + target.arrow[1] * dy + target.arrow[2] * dz) / ((howFar > 1.0f) ? howFar : 1.0f) : 0.0f;
		char pictureWords[80] = "no picture";
		char hitWords[64] = "there's no hit to measure from";
		char flatWords[32] = "no hit";

		if (target.found)
		{
			_snprintf(hitWords, sizeof(hitWords) - 1, "%.1f from the hit", howFar);
			_snprintf(flatWords, sizeof(flatWords) - 1, "%.3f flat", flatTurned);
		}

		if (havePictureNormal && havePictureDepth)
		{
			float const here[3] = { (float)shotRay.across * pictureDepth, (float)shotRay.up * pictureDepth, pictureDepth };
			float const tx = light[0] - here[0], ty = light[1] - here[1], tz = light[2] - here[2];
			float const toLight = sqrtf(tx * tx + ty * ty + tz * tz);

			_snprintf(pictureWords, sizeof(pictureWords) - 1, "%.3f by the picture", (pictureNormal[0] * tx + pictureNormal[1] * ty + pictureNormal[2] * tz) / ((toLight > 1.0f) ? toLight : 1.0f));
		}

		char planeWords[96] = "";

		if (target.found && target.hasPlane)
		{
			float const side = target.plane[0] * place[0] + target.plane[1] * place[1] + target.plane[2] * place[2] - target.plane[3];

			_snprintf(planeWords, sizeof(planeWords) - 1, ", %.1f from its flat surface on its %s", fabsf(side), (side > 0.0f) ? "back" : "front");
		}

		char shareWords[96] = "its share wasn't read";

		if (shareRead[i])
		{
			_snprintf(shareWords, sizeof(shareWords) - 1, "Its share is %.4f", shares[i]);
		}
		else if (screenRead[i])
		{
			_snprintf(shareWords, sizeof(shareWords) - 1, "It added %.3f %.3f %.3f to the screen", screenAfter[i][0] - screenBefore[i][0], screenAfter[i][1] - screenBefore[i][1], screenAfter[i][2] - screenBefore[i][2]);
		}

		Write("  LIGHT %d, a %s, kind %.0f: at %.0f %.0f %.0f, %s, range %.0f%s. N.L %s, %s. %s", i, isFlare ? "flare" : "lamp", light[kShootLightKind], place[0], place[1], place[2], hitWords, light[kShootLightRange], planeWords, flatWords, pictureWords, shareWords);

		int passesOfThisLight = 0, marked = 0;

		for (size_t p = 0; p < passes.size(); p++)
		{
			if (passes[p].lightNumber == i)
			{
				passesOfThisLight++;
				WritePass(passes[p], wallLevel, wallDepth, &marked, &passesRead, &disagreements, &fights, &closestCap);
			}
		}

		if (passesOfThisLight == 0)
		{
			LogString("    no passes: it can't reach the screen");
		}
		else
		{
			Write("    it marked this pixel in %d of its %d passes", marked, passesOfThisLight);
		}
	}

	Write("  SHOT %d ends: %d passes read, %d disagree, %d caps too close to call", shotsTaken, passesRead, disagreements, fights);

	if (closestCap < 1.0e29)
	{
		Write("  the closest cap to the wall is %+.2f steps behind it", closestCap);
	}

	passes.clear();
}
