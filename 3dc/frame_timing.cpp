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

#include "frame_timing.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "logString.h"

static const int kFramesBetweenReports = 300;

// A frame's fixed parts, plus three for each pass of up to 40 prisms. Past this, the last mark takes everything that's left.
static const int kMostMarks = 150;

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

static int64_t processorTotal[kNumProcessorStages];
static int64_t mainVerticesTotal = 0;
static int64_t drawsTotal = 0;
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
	processorTotal[stage] += ClockNow() - started;
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
		"card: %.1f ms: scene %.1f, surface pictures %.1f, light total %.1f, prisms sent %.1f for %d corners, prisms kept %.1f for %d corners, shading %.1f, softening %.1f, dimming %.1f, adding lights %.1f, after %.1f",
		allOfIt, stageTotal[kStageScene], stageTotal[kStageSurfacePictures], stageTotal[kStageLightTotal], stageTotal[kStagePrismsSent], cornersSent,
		stageTotal[kStagePrismsKept], cornersKept, stageTotal[kStageShading], stageTotal[kStageSoftening], stageTotal[kStageDimming],
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

static void ReportProcessorTimes()
{
	int64_t const now = ClockNow();

	if (lastReportClock)
	{
		double const frames = (double)framesCounted;
		char line[300];

		_snprintf(line, sizeof(line) - 1,
			"processor: ms a frame over %d frames: whole frame %.1f, shapes %.1f, locks %.2f, drawing %.1f of which shadow pass %.1f, waiting to present %.1f; %.0f vertices and %.0f draws a frame",
			framesCounted, ClockMilliseconds(now - lastReportClock) / frames, ClockMilliseconds(processorTotal[kProcessorShapes]) / frames,
			ClockMilliseconds(processorTotal[kProcessorLocks]) / frames, ClockMilliseconds(processorTotal[kProcessorDrawing]) / frames,
			ClockMilliseconds(processorTotal[kProcessorShadowPass]) / frames, ClockMilliseconds(processorTotal[kProcessorPresent]) / frames,
			(double)mainVerticesTotal / frames, (double)drawsTotal / frames);
		line[sizeof(line) - 1] = 0;
		LogString(line);
	}

	lastReportClock = now;
	memset(processorTotal, 0, sizeof(processorTotal));
	mainVerticesTotal = 0;
	drawsTotal = 0;
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

	if (++framesCounted >= kFramesBetweenReports)
	{
		ReportProcessorTimes();
	}
}
