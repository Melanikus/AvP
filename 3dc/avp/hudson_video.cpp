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

// A letter that may be in a film's or a step's name: small letters, digits and '-' (see hudson_video.h)
static bool IsANameLetter(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
}

bool HudsonVideo_ClipName(const char *filmName, const char *step, char *out, size_t outSize)
{
	static const char kStart[] = "hudson-video-";
	static const char kEnd[] = ".mp4";
	static const char kPicture[] = ".png";
	size_t const startLength = sizeof(kStart) - 1;
	size_t const endLength = sizeof(kEnd) - 1;
	size_t const pictureLength = sizeof(kPicture) - 1;

	if (!filmName || !step || !out || outSize == 0)
	{
		return false;
	}

	// The film's name: "hudson-video-", then only the name's letters, then ".mp4"
	size_t const filmLength = strlen(filmName);

	if (filmLength <= startLength + endLength || strncmp(filmName, kStart, startLength) != 0 || strcmp(filmName + filmLength - endLength, kEnd) != 0)
	{
		return false;
	}

	size_t const baseLength = filmLength - endLength;

	for (size_t i = 0; i < baseLength; i++)
	{
		if (!IsANameLetter(filmName[i]))
		{
			return false;
		}
	}

	// The step: a picture's name loses its ".png"; then it's 1 to kHudsonVideoLongestStep of the name's letters
	size_t stepLength = strlen(step);

	if (stepLength > pictureLength && strcmp(step + stepLength - pictureLength, kPicture) == 0)
	{
		stepLength -= pictureLength;
	}

	if (stepLength == 0 || stepLength > kHudsonVideoLongestStep)
	{
		return false;
	}

	for (size_t i = 0; i < stepLength; i++)
	{
		if (!IsANameLetter(step[i]))
		{
			return false;
		}
	}

	// The film's name without ".mp4", '-', the step, ".mp4", and the 0 that ends it
	if (baseLength + 1 + stepLength + endLength + 1 > outSize)
	{
		return false;
	}

	memcpy(out, filmName, baseLength);
	out[baseLength] = '-';
	memcpy(out + baseLength + 1, step, stepLength);
	memcpy(out + baseLength + 1 + stepLength, kEnd, endLength + 1);
	return true;
}
