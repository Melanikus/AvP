// Tools for finding out what the shadows are doing. Written 2026-11-04 by Melanikus for the AvP port.
//
// Something wrong with a shadow can last a single frame, and it's very hard to describe. So there are two tools here.
//
// THE CAPTURE: press F8 when something looks wrong, and the next 16 frames are saved, as they appeared, in whichever view is showing. Each frame also
// saves its shadow mask, and numbers.txt says where the camera was and what each light did. So anyone can go through them one frame at a time, and
// nobody has to find words for what they saw. The frames are held in memory until the last one is taken, so that the disk can't change what's being
// recorded, and then the game stops for a few seconds to save them.
//
// THE FLICKER NUMBER: with -shadowflicker, every frame's shadow mask and picture are read back and compared with the last two, and every few seconds
// the log says how much flickered, kept apart by whether the camera was moving. That makes every frame slower, so it's only for measuring.

#include "shadow_debug.h"
#include "flicker_meter.h"

#include <windows.h>
#include <d3dx9.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <fstream>

#include "logString.h"
#include "utilities.h"

extern void NewOnScreenMessage(char *messagePtr);

static const int kFramesInACapture = 16;

struct CapturedFrame
{
	IDirect3DSurface9 *screen;
	IDirect3DSurface9 *mask;
	int frame;
};

static CapturedFrame captured[kFramesInACapture];
static int framesTaken = 0;
static bool capturing = false;
static IDirect3DSurface9 *maskThisFrame = 0;
static std::string captureNumbers;
static std::string captureFolder;

static void Show(const char *text)
{
	static char message[100];

	_snprintf(message, sizeof(message) - 1, "%s", text);
	message[sizeof(message) - 1] = 0;
	NewOnScreenMessage(message);
}

// A copy in the computer's memory. Gives back nothing if there's no room for one.
static IDirect3DSurface9 *CopyPicture(LPDIRECT3DDEVICE9 device, IDirect3DSurface9 *picture)
{
	D3DSURFACE_DESC size;
	IDirect3DSurface9 *copy = 0;

	if (FAILED(picture->GetDesc(&size))
		|| FAILED(device->CreateOffscreenPlainSurface(size.Width, size.Height, size.Format, D3DPOOL_SYSTEMMEM, &copy, NULL)))
	{
		return 0;
	}

	if (FAILED(device->GetRenderTargetData(picture, copy)))
	{
		copy->Release();
		return 0;
	}

	return copy;
}

static void GiveBack(IDirect3DSurface9 *&surface)
{
	if (surface)
	{
		surface->Release();
		surface = 0;
	}
}

static void ForgetCapture()
{
	for (int i = 0; i < framesTaken; i++)
	{
		GiveBack(captured[i].screen);
		GiveBack(captured[i].mask);
	}

	GiveBack(maskThisFrame);
	framesTaken = 0;
	capturing = false;
	captureNumbers.clear();
}

bool ShadowCapture_IsOn()
{
	return capturing;
}

void ShadowCapture_Start(int frame)
{
	if (capturing)
	{
		return;
	}

	SYSTEMTIME now;
	GetLocalTime(&now);

	char name[80];
	_snprintf(name, sizeof(name) - 1, "%04d%02d%02d-%02d%02d%02d-frame-%d", now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond, frame);
	name[sizeof(name) - 1] = 0;

	char const *saveFolder = GetSaveFolderPath();
	std::string const captures = std::string(saveFolder ? saveFolder : "") + "shadow-captures\\";

	CreateDirectoryA(captures.c_str(), NULL);
	captureFolder = captures + name + "\\";

	if (!CreateDirectoryA(captureFolder.c_str(), NULL))
	{
		LogString("shadow capture: couldn't make a folder for it, so nothing was captured");
		return;
	}

	capturing = true;
	framesTaken = 0;
	captureNumbers.clear();
	LogString(std::string("shadow capture: started, in shadow-captures\\") + name);
	Show("SHADOW CAPTURE: 16 FRAMES");
}

void ShadowCapture_AddNumbers(const std::string &numbers)
{
	if (capturing)
	{
		captureNumbers += numbers;
	}
}

void ShadowCapture_TakeMask(LPDIRECT3DDEVICE9 device, IDirect3DSurface9 *mask)
{
	if (capturing && !maskThisFrame)
	{
		maskThisFrame = CopyPicture(device, mask);
	}
}

static void SaveCapture()
{
	int saved = 0;

	for (int i = 0; i < framesTaken; i++)
	{
		char name[64];

		_snprintf(name, sizeof(name) - 1, "frame-%06d.png", captured[i].frame);
		name[sizeof(name) - 1] = 0;

		if (captured[i].screen && SUCCEEDED(D3DXSaveSurfaceToFileA((captureFolder + name).c_str(), D3DXIFF_PNG, captured[i].screen, NULL, NULL)))
		{
			saved++;
		}

		_snprintf(name, sizeof(name) - 1, "frame-%06d-mask.png", captured[i].frame);
		name[sizeof(name) - 1] = 0;

		if (captured[i].mask)
		{
			D3DXSaveSurfaceToFileA((captureFolder + name).c_str(), D3DXIFF_PNG, captured[i].mask, NULL, NULL);
		}
	}

	std::ofstream file((captureFolder + "numbers.txt").c_str(), std::ios::out);
	file << captureNumbers;
	file.close();

	char line[100];
	_snprintf(line, sizeof(line) - 1, "shadow capture: saved %d frames of %d", saved, framesTaken);
	line[sizeof(line) - 1] = 0;
	LogString(line);
	Show("SHADOW CAPTURE SAVED");
}

void ShadowCapture_EndFrame(LPDIRECT3DDEVICE9 device, int frame)
{
	if (!capturing)
	{
		return;
	}

	IDirect3DSurface9 *backBuffer = 0;
	IDirect3DSurface9 *screen = 0;

	if (SUCCEEDED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer)))
	{
		screen = CopyPicture(device, backBuffer);
		backBuffer->Release();
	}

	captured[framesTaken].screen = screen;
	captured[framesTaken].mask = maskThisFrame;
	captured[framesTaken].frame = frame;
	maskThisFrame = 0;
	framesTaken++;

	// out of memory saves what there is
	if (framesTaken == kFramesInACapture || !screen)
	{
		SaveCapture();
		ForgetCapture();
	}
}

void ShadowCapture_GiveUp()
{
	if (capturing)
	{
		LogString("shadow capture: the device is being reset, so this capture was dropped");
		ForgetCapture();
	}
}

// The flicker number. These are how much of a change counts: a twentieth of the way from no shadow to full shadow, and a few steps of 255 on screen.
static const float kMaskChangeCounts = 0.05f;
static const float kScreenChangeCounts = 6.0f;

static const int kFramesBetweenReports = 300;
static const int kWorstFramesKept = 3;

// less than this says the camera didn't move
static const float kStillPlace = 0.5f;
static const float kStillTurn = 0.0001f;

static FlickerMeter meters[kNumFlickerPictures] = { FlickerMeter(kMaskChangeCounts), FlickerMeter(kScreenChangeCounts) };
static FlickerCounts thisFrameCounts[kNumFlickerPictures];

struct FlickerGroup
{
	int frames;
	double changed[kNumFlickerPictures];
	double cameBack[kNumFlickerPictures];
};

struct WorstFlicker
{
	int frame;
	int score;
	FlickerCounts counts[kNumFlickerPictures];
	float moved;
	float turned;
};

static FlickerGroup movingFrames;
static FlickerGroup stillFrames;
static WorstFlicker worstFrames[kWorstFramesKept];
static float lastCameraPlace[3];
static float lastCameraAxes[9];
static bool haveLastCamera = false;
static int framesSinceReport = 0;
static int samplesInAPicture[kNumFlickerPictures];

void Flicker_AddPicture(FlickerPicture picture, const std::vector<float> &samples)
{
	thisFrameCounts[picture] = meters[picture].Add(samples);
	samplesInAPicture[picture] = (int)samples.size();
}

void Flicker_WriteReport()
{
	if (!framesSinceReport)
	{
		return;
	}

	char line[400];
	double const moving = movingFrames.frames ? (double)movingFrames.frames : 1.0;
	double const still = stillFrames.frames ? (double)stillFrames.frames : 1.0;

	_snprintf(line, sizeof(line) - 1,
		"flicker: %d frames moving: per frame, mask %.0f changed and %.0f came back, screen %.0f changed and %.0f came back; %d frames still: mask %.0f and %.0f, screen %.0f and %.0f; of %d mask places and %d screen places",
		movingFrames.frames, movingFrames.changed[kFlickerMask] / moving, movingFrames.cameBack[kFlickerMask] / moving, movingFrames.changed[kFlickerScreen] / moving, movingFrames.cameBack[kFlickerScreen] / moving,
		stillFrames.frames, stillFrames.changed[kFlickerMask] / still, stillFrames.cameBack[kFlickerMask] / still, stillFrames.changed[kFlickerScreen] / still, stillFrames.cameBack[kFlickerScreen] / still,
		samplesInAPicture[kFlickerMask], samplesInAPicture[kFlickerScreen]);
	line[sizeof(line) - 1] = 0;
	LogString(line);

	for (int i = 0; i < kWorstFramesKept; i++)
	{
		WorstFlicker const &worst = worstFrames[i];

		if (worst.score > 0)
		{
			_snprintf(line, sizeof(line) - 1, "flicker:   frame %d: mask %d changed and %d came back, screen %d changed and %d came back; the camera moved %.1f and turned %.5f",
				worst.frame, worst.counts[kFlickerMask].changed, worst.counts[kFlickerMask].cameBack, worst.counts[kFlickerScreen].changed, worst.counts[kFlickerScreen].cameBack, worst.moved, worst.turned);
			line[sizeof(line) - 1] = 0;
			LogString(line);
		}
	}

	memset(&movingFrames, 0, sizeof(movingFrames));
	memset(&stillFrames, 0, sizeof(stillFrames));
	memset(worstFrames, 0, sizeof(worstFrames));
	framesSinceReport = 0;
}

void Flicker_EndFrame(int frame, const float *cameraPlace, const float *cameraAxes)
{
	float moved = 0.0f;
	float turned = 0.0f;

	if (haveLastCamera)
	{
		float const dx = cameraPlace[0] - lastCameraPlace[0];
		float const dy = cameraPlace[1] - lastCameraPlace[1];
		float const dz = cameraPlace[2] - lastCameraPlace[2];

		moved = sqrtf(dx * dx + dy * dy + dz * dz);

		for (int i = 0; i < 9; i++)
		{
			float const change = fabsf(cameraAxes[i] - lastCameraAxes[i]);
			turned = (change > turned) ? change : turned;
		}
	}

	memcpy(lastCameraPlace, cameraPlace, sizeof(lastCameraPlace));
	memcpy(lastCameraAxes, cameraAxes, sizeof(lastCameraAxes));
	haveLastCamera = true;

	FlickerGroup &group = (moved < kStillPlace && turned < kStillTurn) ? stillFrames : movingFrames;

	group.frames++;

	for (int i = 0; i < kNumFlickerPictures; i++)
	{
		group.changed[i] += thisFrameCounts[i].changed;
		group.cameBack[i] += thisFrameCounts[i].cameBack;
	}

	int const score = thisFrameCounts[kFlickerMask].cameBack + thisFrameCounts[kFlickerScreen].cameBack;

	for (int i = 0; i < kWorstFramesKept; i++)
	{
		if (score > worstFrames[i].score)
		{
			for (int later = kWorstFramesKept - 1; later > i; later--)
			{
				worstFrames[later] = worstFrames[later - 1];
			}

			worstFrames[i].frame = frame;
			worstFrames[i].score = score;
			memcpy(worstFrames[i].counts, thisFrameCounts, sizeof(thisFrameCounts));
			worstFrames[i].moved = moved;
			worstFrames[i].turned = turned;
			break;
		}
	}

	// a frame that read no pictures, such as another view, has nothing to carry over
	memset(thisFrameCounts, 0, sizeof(thisFrameCounts));

	if (++framesSinceReport >= kFramesBetweenReports)
	{
		Flicker_WriteReport();
	}
}
