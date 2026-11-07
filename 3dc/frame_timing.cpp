// How long each part of a frame takes, on the graphics card and on the processor. Written 2026-11-04 by Melanikus for the AvP port.
//
// WHY THIS IS HERE
//
// To speed up a frame, you have to know where it goes, and guessing keeps being wrong. Asking the card how long something took used to mean
// waiting for it to finish, which itself made the game stutter. Nothing here ever waits.
//
// THE CARD
//
// The card works through its list of jobs some time after the game hands them over, so a clock on the processor can't time them. Instead the game
// puts a "note the time" job between other jobs: that's a mark. Once every few seconds a frame is given a mark between each of its parts. The times
// come back a frame or two later, and each new frame asks whether they've arrived, without stopping if they haven't. Taking one time from the next
// says how long the part between them took.
//
// THE PROCESSOR
//
// Every frame adds up its time in a few places, using the processor's own clock, which takes almost no time to read. Every few seconds the averages
// go to the log.
//
// THE STALL LIST
//
// An average can't show a frame that stuck. So every frame is timed from end to end, and one that took too long goes in a short list in memory: what
// each part took, and what else was going on, such as textures or effects used for the first time. The list goes to the log later, so writing it
// can't cause what it's measuring. A frame's time runs from when the last frame was shown to when this one is, so a frame includes its own wait.

#include "frame_timing.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "logString.h"

static const int kFramesBetweenReports = 300;

// A frame's fixed parts, plus five for each pass of up to about 60. Past this, the last mark takes everything that's left.
static const int kMostMarks = 300;

// How long to go on asking before giving up on an answer. It would only come to this if the device had a problem.
static const int kMostFramesToWait = 60;

static IDirect3DQuery9 *marks[kMostMarks];
static FrameStage markStage[kMostMarks];
static UINT64 markTime[kMostMarks];
static IDirect3DQuery9 *clockChanged = 0;      // says whether the card's clock changed speed, which would spoil the times
static IDirect3DQuery9 *clockSpeed = 0;
static int marksUsed = 0;
static int marksRead = 0;
static bool timingThisFrame = false;
static bool waitingForAnswers = false;
static int framesWaited = 0;
static int framesUntilTimed = kFramesBetweenReports / 2;
static int cornersSent = 0;
static int cornersKept = 0;

// A frame slower than this is about two frames of 60 a second, so it goes in the stall list.
static const float kStallMilliseconds = 33.0f;

static const int kMostStalls = 64;
static const int kFirstUseTextLength = 48;

struct StallEntry
{
	int frame;
	float milliseconds;
	float stage[kNumProcessorStages];
	char room[16];
	int placesChanged;
	int keptGroupsMade;
	int polygonsCollected;
	int texturesNew;
	uint32_t textureBytesNew;
	char firstNewTexture[kFirstUseTextLength];
	char effectsNew[kFirstUseTextLength];
	bool deviceReset;
};

static StallEntry stalls[kMostStalls];
static int stallsHeld = 0;
static int stallsLost = 0;

static StallEntry thisFrame;
static int64_t processorThisFrame[kNumProcessorStages];
static int framesShown = 0;
static int64_t lastFrameShown = 0;

// one bit for each texture
static const uint32_t kTexturesKept = 65536;
static uint8_t textureSeen[kTexturesKept / 8];

static const uint32_t kEffectsKept = 256;
static bool effectSeen[kEffectsKept];

static int64_t processorTotal[kNumProcessorStages];
static int64_t mainVerticesTotal = 0;
static int64_t drawsTotal = 0;
static int texturesFirstUsedTotal = 0;
static int64_t textureBytesFirstUsedTotal = 0;
static int effectsFirstUsedTotal = 0;
static int framesCounted = 0;
static int64_t lastReportClock = 0;

static int64_t ClockNow()
{
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);
	return now.QuadPart;
}

static double ClockMilliseconds(int64_t ticks)
{
	LARGE_INTEGER frequency;
	QueryPerformanceFrequency(&frequency);
	return (double)ticks * 1000.0 / (double)frequency.QuadPart;
}

ProcessorStageClock::ProcessorStageClock(ProcessorStage stageToCount)
{
	stage = stageToCount;
	started = ClockNow();
}

ProcessorStageClock::~ProcessorStageClock()
{
	int64_t const taken = ClockNow() - started;

	processorTotal[stage] += taken;
	processorThisFrame[stage] += taken;
}

void FrameTiming_SetRoom(const char *room)
{
	_snprintf(thisFrame.room, sizeof(thisFrame.room) - 1, "%s", room);
	thisFrame.room[sizeof(thisFrame.room) - 1] = 0;
}

void FrameTiming_SetPolygonsCollected(int polygons)
{
	thisFrame.polygonsCollected = polygons;
}

void FrameTiming_SetFrameNumber(int frame)
{
	thisFrame.frame = frame;
}

void FrameTiming_CountPlacesChanged(int places)
{
	thisFrame.placesChanged += places;
}

void FrameTiming_CountKeptGroupsMade()
{
	thisFrame.keptGroupsMade++;
}

bool FrameTiming_TextureIsNew(uint32_t texture)
{
	if (texture >= kTexturesKept)
	{
		return false;
	}

	uint8_t const bit = (uint8_t)(1 << (texture & 7));

	if (textureSeen[texture >> 3] & bit)
	{
		return false;
	}

	textureSeen[texture >> 3] |= bit;
	return true;
}

void FrameTiming_NoteNewTexture(const char *name, uint32_t bytes)
{
	if (thisFrame.texturesNew == 0)
	{
		_snprintf(thisFrame.firstNewTexture, kFirstUseTextLength - 1, "%s", name);
		thisFrame.firstNewTexture[kFirstUseTextLength - 1] = 0;
	}

	thisFrame.texturesNew++;
	thisFrame.textureBytesNew += bytes;
	texturesFirstUsedTotal++;
	textureBytesFirstUsedTotal += bytes;
}

void FrameTiming_NoteEffectUsed(uint32_t effect, const char *name)
{
	if (effect >= kEffectsKept || effectSeen[effect])
	{
		return;
	}

	effectSeen[effect] = true;
	effectsFirstUsedTotal++;

	size_t const used = strlen(thisFrame.effectsNew);

	if (used + 2 < (size_t)kFirstUseTextLength)
	{
		_snprintf(thisFrame.effectsNew + used, kFirstUseTextLength - 1 - used, "%s ", name);
		thisFrame.effectsNew[kFirstUseTextLength - 1] = 0;
	}
}

void FrameTiming_Release()
{
	for (int i = 0; i < kMostMarks; i++)
	{
		if (marks[i])
		{
			marks[i]->Release();
			marks[i] = 0;
		}
	}

	if (clockChanged)
	{
		clockChanged->Release();
		clockChanged = 0;
	}

	if (clockSpeed)
	{
		clockSpeed->Release();
		clockSpeed = 0;
	}

	timingThisFrame = false;
	waitingForAnswers = false;
	thisFrame.deviceReset = true;
}

// These are made the first time they're needed, and again after a device reset. If the card can't do it, nothing is timed.
static bool MakeQueries(LPDIRECT3DDEVICE9 device)
{
	if (clockChanged)
	{
		return true;
	}

	if (FAILED(device->CreateQuery(D3DQUERYTYPE_TIMESTAMPDISJOINT, &clockChanged)) || FAILED(device->CreateQuery(D3DQUERYTYPE_TIMESTAMPFREQ, &clockSpeed)))
	{
		FrameTiming_Release();
		return false;
	}

	for (int i = 0; i < kMostMarks; i++)
	{
		if (FAILED(device->CreateQuery(D3DQUERYTYPE_TIMESTAMP, &marks[i])))
		{
			FrameTiming_Release();
			return false;
		}
	}

	return true;
}

static void ReportCardTimes(UINT64 ticksPerSecond)
{
	double stageTotal[kNumFrameStages] = { 0.0 };
	double allOfIt = 0.0;

	for (int i = 1; i < marksUsed; i++)
	{
		double const milliseconds = (double)(markTime[i] - markTime[i - 1]) * 1000.0 / (double)ticksPerSecond;

		stageTotal[markStage[i]] += milliseconds;
		allOfIt += milliseconds;
	}

	char line[400];
	_snprintf(line, sizeof(line) - 1,
		"card: %.1f ms: scene %.1f, surface pictures %.1f, light total %.1f, prisms sent %.1f for %d corners, prisms kept %.1f for %d corners, clearing the stencil %.1f, shading %.1f, softening %.1f, dimming %.1f, adding lights %.1f, after %.1f",
		allOfIt, stageTotal[kStageScene], stageTotal[kStageSurfacePictures], stageTotal[kStageLightTotal], stageTotal[kStagePrismsSent], cornersSent,
		stageTotal[kStagePrismsKept], cornersKept, stageTotal[kStageClearing], stageTotal[kStageShading], stageTotal[kStageSoftening], stageTotal[kStageDimming],
		stageTotal[kStageAddingLights], stageTotal[kStageAfter]);
	line[sizeof(line) - 1] = 0;
	LogString(line);
}

// Reads whatever has arrived, and comes back to the rest on the next frame. Passing no flag to GetData is what stops it making the card hurry.
static void CheckForAnswers()
{
	while (marksRead < marksUsed)
	{
		HRESULT const answer = marks[marksRead]->GetData(&markTime[marksRead], sizeof(UINT64), 0);

		if (answer != S_OK)
		{
			if (FAILED(answer) || ++framesWaited > kMostFramesToWait)
			{
				waitingForAnswers = false;
			}

			return;
		}

		marksRead++;
	}

	BOOL changed = TRUE;
	UINT64 ticksPerSecond = 0;

	if (clockChanged->GetData(&changed, sizeof(changed), 0) != S_OK || clockSpeed->GetData(&ticksPerSecond, sizeof(ticksPerSecond), 0) != S_OK)
	{
		if (++framesWaited > kMostFramesToWait)
		{
			waitingForAnswers = false;
		}

		return;
	}

	waitingForAnswers = false;

	if (changed || !ticksPerSecond)
	{
		LogString("card: the card's clock changed speed during this frame, so its times can't be trusted");
		return;
	}

	ReportCardTimes(ticksPerSecond);
}

void FrameTiming_StartFrame(LPDIRECT3DDEVICE9 device)
{
	if (waitingForAnswers)
	{
		CheckForAnswers();
	}

	if (--framesUntilTimed > 0 || waitingForAnswers || timingThisFrame)
	{
		return;
	}

	framesUntilTimed = kFramesBetweenReports;

	if (!MakeQueries(device))
	{
		return;
	}

	marksUsed = 0;
	cornersSent = 0;
	cornersKept = 0;
	timingThisFrame = true;

	clockChanged->Issue(D3DISSUE_BEGIN);
	marks[marksUsed++]->Issue(D3DISSUE_END);
}

void FrameTiming_Mark(FrameStage stage)
{
	if (!timingThisFrame)
	{
		return;
	}

	// one place is always left for the end of the frame
	if (marksUsed >= kMostMarks - 1 && stage != kStageAfter)
	{
		return;
	}

	markStage[marksUsed] = stage;
	marks[marksUsed++]->Issue(D3DISSUE_END);
}

void FrameTiming_CountPrismCorners(int sent, int kept)
{
	if (timingThisFrame)
	{
		cornersSent += sent;
		cornersKept += kept;
	}
}

void FrameTiming_CountMainVertices(int vertices)
{
	mainVerticesTotal += vertices;
}

void FrameTiming_CountDraw()
{
	drawsTotal++;
}

// What the clocks that don't overlap add up to. Anything left of a frame is in none of them.
static double ClockedPartsOf(double const *stage)
{
	return stage[kProcessorMessages] + stage[kProcessorGame] + stage[kProcessorViews] + stage[kProcessorHud] + stage[kProcessorMenus] + stage[kProcessorLocks]
		+ stage[kProcessorDrawing] + stage[kProcessorBeginScene] + stage[kProcessorEndScene] + stage[kProcessorPresent];
}

void FrameTiming_WriteStalls()
{
	for (int i = 0; i < stallsHeld; i++)
	{
		StallEntry const &stall = stalls[i];
		char line[800];

		double stages[kNumProcessorStages];

		for (int stage = 0; stage < kNumProcessorStages; stage++)
		{
			stages[stage] = stall.stage[stage];
		}

		double const left = stall.milliseconds - ClockedPartsOf(stages);

		_snprintf(line, sizeof(line) - 1,
			"stall: frame %d took %.1f ms%s: messages %.1f, game %.1f, views %.1f of which shapes %.1f, display %.1f, menus %.1f, locks %.1f, drawing %.1f of which shadow pass %.1f, finishing %.1f, present %.1f, starting the scene %.1f, log %.1f, in no clock %.1f; room %s; %d places changed, %d kept groups made, %d polygons collected, %d textures first used (%u KB, first %s), first use of effects: %s",
			stall.frame, stall.milliseconds, stall.deviceReset ? " with a device reset" : "", stall.stage[kProcessorMessages], stall.stage[kProcessorGame], stall.stage[kProcessorViews], stall.stage[kProcessorShapes],
			stall.stage[kProcessorHud], stall.stage[kProcessorMenus], stall.stage[kProcessorLocks], stall.stage[kProcessorDrawing], stall.stage[kProcessorShadowPass], stall.stage[kProcessorEndScene],
			stall.stage[kProcessorPresent], stall.stage[kProcessorBeginScene], stall.stage[kProcessorLog], (left > 0.0) ? left : 0.0, stall.room,
			stall.placesChanged, stall.keptGroupsMade, stall.polygonsCollected, stall.texturesNew, stall.textureBytesNew / 1024, stall.firstNewTexture, stall.effectsNew);
		line[sizeof(line) - 1] = 0;
		LogString(line);
	}

	if (stallsLost)
	{
		char line[100];

		_snprintf(line, sizeof(line) - 1, "stall: and %d more that the list had no room for", stallsLost);
		line[sizeof(line) - 1] = 0;
		LogString(line);
	}

	stallsHeld = 0;
	stallsLost = 0;
}

static void ReportProcessorTimes()
{
	int64_t const now = ClockNow();

	if (lastReportClock)
	{
		double const frames = (double)framesCounted;
		double stages[kNumProcessorStages];

		for (int stage = 0; stage < kNumProcessorStages; stage++)
		{
			stages[stage] = ClockMilliseconds(processorTotal[stage]) / frames;
		}

		double const whole = ClockMilliseconds(now - lastReportClock) / frames;
		double const left = whole - ClockedPartsOf(stages);
		char line[800];

		_snprintf(line, sizeof(line) - 1,
			"processor: ms a frame over %d frames: whole frame %.1f, messages %.1f, game %.1f, views %.1f of which shapes %.1f, display %.1f, menus %.1f, locks %.2f, drawing %.1f of which shadow pass %.1f and saving the card's settings %.2f, finishing %.1f, present %.1f, starting the scene %.1f, in no clock %.1f; %.0f vertices a frame, and %.0f indexed draws of the lists, counting the scene and the surface pictures, with no shadow draws in that; %d textures first used (%d KB), %d effects first used",
			framesCounted, whole, stages[kProcessorMessages], stages[kProcessorGame], stages[kProcessorViews], stages[kProcessorShapes], stages[kProcessorHud], stages[kProcessorMenus], stages[kProcessorLocks],
			stages[kProcessorDrawing], stages[kProcessorShadowPass], stages[kProcessorStateBlock], stages[kProcessorEndScene], stages[kProcessorPresent], stages[kProcessorBeginScene], (left > 0.0) ? left : 0.0,
			(double)mainVerticesTotal / frames, (double)drawsTotal / frames, texturesFirstUsedTotal, (int)(textureBytesFirstUsedTotal / 1024), effectsFirstUsedTotal);
		line[sizeof(line) - 1] = 0;
		LogString(line);

		int writes = 0, linesLost = 0;
		float slowest = 0.0f;
		LogTakeWriteTimes(&writes, &slowest, &linesLost);

		_snprintf(line, sizeof(line) - 1, "log: %d writes to the disk, the slowest %.1f ms, and only an error is written on the game's thread; %d lines lost", writes, slowest, linesLost);
		line[sizeof(line) - 1] = 0;
		LogString(line);
	}

	lastReportClock = now;
	memset(processorTotal, 0, sizeof(processorTotal));
	mainVerticesTotal = 0;
	drawsTotal = 0;
	texturesFirstUsedTotal = 0;
	textureBytesFirstUsedTotal = 0;
	effectsFirstUsedTotal = 0;
	framesCounted = 0;
}

void FrameTiming_EndFrame()
{
	if (timingThisFrame)
	{
		clockSpeed->Issue(D3DISSUE_END);
		clockChanged->Issue(D3DISSUE_END);
		timingThisFrame = false;
		waitingForAnswers = true;
		marksRead = 0;
		framesWaited = 0;
	}
}

void FrameTiming_FrameShown()
{
	int64_t const now = ClockNow();

	framesShown++;

	if (lastFrameShown)
	{
		float const milliseconds = (float)ClockMilliseconds(now - lastFrameShown);

		if (milliseconds >= kStallMilliseconds)
		{
			if (stallsHeld == kMostStalls)
			{
				stallsLost++;
			}
			else
			{
				// a frame with no shadow pass to number it, such as a film
				if (!thisFrame.frame)
				{
					thisFrame.frame = framesShown;
				}

				thisFrame.milliseconds = milliseconds;

				for (int i = 0; i < kNumProcessorStages; i++)
				{
					thisFrame.stage[i] = (float)ClockMilliseconds(processorThisFrame[i]);
				}

				stalls[stallsHeld++] = thisFrame;
			}
		}
	}

	memset(&thisFrame, 0, sizeof(thisFrame));
	memset(processorThisFrame, 0, sizeof(processorThisFrame));

	if (++framesCounted >= kFramesBetweenReports)
	{
		ReportProcessorTimes();
		FrameTiming_WriteStalls();
	}

	// taken after the report, so that the next frame's time starts from here
	lastFrameShown = ClockNow();
}
