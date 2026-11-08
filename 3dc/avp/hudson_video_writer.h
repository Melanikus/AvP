#pragma once

// Writes a run's film, using Media Foundation, the video writer that comes with Windows. See hudson_video.h for what it promises. Written 2026-11-05 by Melanikus for the AvP port.
//
// HOW IT WORKS: whoever has a picture copies it into a place in a small queue, and a thread of the writer's own takes pictures out and hands them to the encoder. So when the encoder is slow, only that thread waits. If the queue is
// full, the picture is dropped and counted.
//
// WHAT IT LOADS: Media Foundation's DLLs, only when a film starts, and only from Windows' own folder, so that a normal game never loads them, and a file with the same name planted beside the game can't be picked up instead. Nothing is
// downloaded, and nothing is added to what the game needs to start.
//
// THE FILE: it's written in fragments (MFTranscodeContainerType_FMPEG4), so if the game dies part way, what was written so far can still be played. Only the last moments are lost.

#include <stdint.h>

// What's happened so far
struct HudsonVideoFacts
{
	bool started;          // a thread is writing, or was
	bool goingOn;          // it still wants pictures
	bool finishedInTime;   // the film was finished properly, and Finish didn't run out of patience
	long written;          // pictures handed to the encoder
	long dropped;          // pictures that were due, and had no room in the queue
	long noPicture;        // moments of the film the game drew no frame for, because it was running at under 30 frames a second. The film just holds the picture before
	long long bytes;       // the file's size when last looked at
	int width, height;     // how big each picture of the film is, which is how big the game drew it
	char stopped[16];      // one word for the result, for why it stopped early. Empty if it hasn't
	char why[256];         // the same, in words, for the log
};

// Are Media Foundation's DLLs on this machine? Some versions of Windows come without them, and some Windows Server machines do.
bool HudsonVideoWriter_CanRecordHere();

// Starts a film, and waits a limited time for the writer to be ready. It gives back false if it didn't start, and the facts say why. breakAtPicture makes that picture fail in the encoder for real, to prove a run survives it: 0 means never. It can't be called while a film is being written.
bool HudsonVideoWriter_Start(const char *path, int width, int height, int breakAtPicture, long long mostBytes);

// Does the film want a picture for this moment? Reading a frame back from the card is the slow part, so this is asked first. It says no if this moment's picture has been taken already, if it has stopped, or if the queue is full, and then that picture counts as dropped.
bool HudsonVideoWriter_WantsPictureFor(double seconds);

// Hands over a picture, of the size the film was started with and 4 bytes a pixel, in the card's X8R8G8B8 order. Only one thread may call this and the one above.
void HudsonVideoWriter_Add(const uint8_t *picture, int pitch, double seconds);

// Something other than the writer has found it can't go on, like the picture no longer being readable. The first reason given is the one that stays.
void HudsonVideoWriter_StopBecause(const char *word, const char *why);

// Writes what's waiting and finishes the file, waiting at most this long. Any thread can call it, more than once, and whether a film was started or not.
void HudsonVideoWriter_Finish(unsigned long mostMilliseconds);

void HudsonVideoWriter_GetFacts(HudsonVideoFacts *facts);
