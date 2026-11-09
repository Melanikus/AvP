// See hudson_video.h. Written 2026-11-05 by Melanikus for the AvP port. This file knows nothing about the game, the graphics card or the video writer.
#include "hudson_video.h"

#include <string.h>

int HudsonVideo_SlotAt(double seconds)
{
	if (seconds <= 0.0)
	{
		return 0;
	}

	// The limit is only there so that a number too big for an int can't turn into nonsense. It's about two years of film.
	double const slot = seconds * kHudsonVideoFramesPerSecond;

	return slot >= 2000000000.0 ? 2000000000 : (int)slot;
}

long long HudsonVideo_TimeOfSlot(int slot)
{
	return (long long)slot * 10000000LL / kHudsonVideoFramesPerSecond;
}

long long HudsonVideo_SlotLength()
{
	return 10000000LL / kHudsonVideoFramesPerSecond;
}

int HudsonVideoQueue_PlaceToAdd(const HudsonVideoQueue &q)
{
	return q.waiting < kHudsonVideoRingFrames ? (q.first + q.waiting) % kHudsonVideoRingFrames : -1;
}

void HudsonVideoQueue_Added(HudsonVideoQueue *q)
{
	q->waiting++;
}

int HudsonVideoQueue_PlaceToWrite(const HudsonVideoQueue &q)
{
	return q.waiting > 0 ? q.first : -1;
}

void HudsonVideoQueue_Written(HudsonVideoQueue *q)
{
	q->first = (q->first + 1) % kHudsonVideoRingFrames;
	q->waiting--;
}

HudsonVideoStop HudsonVideo_ShouldStop(long long bytesSoFar, long long mostBytes, long long bytesFree)
{
	if (bytesSoFar >= mostBytes)
	{
		return kHudsonVideoFileIsFull;
	}

	if (bytesFree < 0)
	{
		return kHudsonVideoDiskWontSay;
	}

	if (bytesFree < kHudsonVideoDiskToLeave)
	{
		return kHudsonVideoDiskIsFull;
	}

	return kHudsonVideoGoOn;
}

bool HudsonVideo_DiskHasRoom(long long bytesFree, long long mostBytes)
{
	return bytesFree >= 0 && bytesFree - kHudsonVideoDiskToLeave >= mostBytes;
}

const char *HudsonVideo_WhyNotThisSize(int width, int height)
{
	if (width <= 0 || height <= 0)
	{
		return "the picture has no size";
	}

	if (width % 2 != 0 || height % 2 != 0)
	{
		return "H.264 can only take a picture whose width and height are even";
	}

	if (width > kHudsonVideoMostWidth || height > kHudsonVideoMostHeight)
	{
		return "the picture is bigger than this will record";
	}

	return 0;
}

int HudsonVideo_BitsPerSecond(int width, int height)
{
	return width * height * 6;
}

bool HudsonVideo_CopyPicture(uint8_t *to, int width, int height, const uint8_t *from, int pitch)
{
	if (pitch < width * 4)
	{
		return false;
	}

	for (int y = 0; y < height; y++)
	{
		memcpy(to + y * width * 4, from + y * pitch, width * 4);
	}

	return true;
}
