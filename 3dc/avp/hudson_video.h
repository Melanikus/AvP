#pragma once

// A film of what a Hudson run drew, as an mp4 in the run's folder. hudson_plan.h says which runs have one. Written 2026-11-05 by Melanikus for the AvP port.
//
// There are three parts. This file and hudson_video.cpp need no graphics card and no video writer, so a test can try all of it: which moment a picture belongs to, the queue of pictures
// waiting to be written, and when to stop. hudson_video_writer.cpp does the writing, using the video writer that comes with Windows. hudson.cpp takes the pictures.
//
// WHAT IT PROMISES:
// - It only ever happens in a Hudson run, which needs -dev and -hudson. A normal game never loads the video writer.
// - A failure that Windows reports can't take the game down: recording stops, the log says why, and the run carries on. That isn't a promise about a crash inside an encoder, which runs in the game's own process, and that includes one from
//   the graphics card's maker, which Windows may choose. A crash like that ends the run as any crash would, though what was written so far can still be played.
// - It uses memory, in a 32-bit game. The queue alone is about 50 MB at 1920x1080 and 226 MB at the largest size, and the encoder needs its own. If there isn't the memory for the queue, there's no film, and the run goes on.
// - The game never waits for it. If the writer falls behind and the queue is full, that picture is dropped, and the log and the result say how many were, so a film with gaps can't look clean.
// - It does slow a frame, because reading a finished frame back from the card takes time. So a run that measures time can't have it, and the cost goes in the log.

#include <stdint.h>

enum
{
	kHudsonVideoFramesPerSecond = 30,
	kHudsonVideoRingFrames = 6,      // about a fifth of a second of pictures waiting. It's kept small because each one is a whole screen, in a 32-bit game
	kHudsonVideoMostWidth = 4096,    // about the most an H.264 encoder takes
	kHudsonVideoMostHeight = 2304,
	kHudsonVideoBreakAtPicture = 60, // break=video makes the writer fail here, about two seconds in, to prove that a run survives it
	kHudsonVideoFinishWaitMilliseconds = 20000 // how long the end of a run waits for the film to be finished, so that a stuck writer can't hold it up
};

long long const kHudsonVideoMostBytes = 2000000000LL;    // one file's limit, about 2 GB. At 1920x1080 that's about 20 minutes
long long const kHudsonVideoDiskToLeave = 1000000000LL;  // it stops rather than let the disk go below this, because other things need the disk too

// Which picture of the film a moment belongs to, counting seconds from when recording started. A picture goes in the slot its moment belongs to, so the film plays at real speed whatever the game's frame rate was.
int HudsonVideo_SlotAt(double seconds);

// When a slot starts in the film, and how long one lasts, in the video writer's units of 100 nanoseconds
long long HudsonVideo_TimeOfSlot(int slot);
long long HudsonVideo_SlotLength();

// The queue of pictures waiting to be written, in a fixed ring of kHudsonVideoRingFrames places. It only keeps count: the pictures themselves are in hudson_video_writer.cpp. Nothing here locks anything, so whoever uses it from two threads locks it.
struct HudsonVideoQueue
{
	int first;     // the place holding the oldest picture
	int waiting;   // how many there are
};

int HudsonVideoQueue_PlaceToAdd(const HudsonVideoQueue &q);     // where the next picture goes, or -1 if the queue is full
void HudsonVideoQueue_Added(HudsonVideoQueue *q);
int HudsonVideoQueue_PlaceToWrite(const HudsonVideoQueue &q);   // the oldest picture, or -1 if there are none
void HudsonVideoQueue_Written(HudsonVideoQueue *q);

enum HudsonVideoStop
{
	kHudsonVideoGoOn,
	kHudsonVideoFileIsFull,
	kHudsonVideoDiskIsFull,
	kHudsonVideoDiskWontSay    // the disk wouldn't say how much room is left: a film never goes on not knowing it can't fill the disk
};

// Should a film being written stop here? bytesFree is -1 if the disk wouldn't say, and then it stops too, because a film that can't tell how
// much room is left could fill the disk (Primary's read, 2026-11-09: a film went on not knowing, so only its own size limit held it).
HudsonVideoStop HudsonVideo_ShouldStop(long long bytesSoFar, long long mostBytes, long long bytesFree);

// Is there room to start? There has to be room for the largest film it might be, as well as what's left alone. bytesFree is -1 if the disk
// wouldn't say, and then there's no film: not knowing isn't room.
bool HudsonVideo_DiskHasRoom(long long bytesFree, long long mostBytes);

// Why a picture of this size can't be recorded, in words, or nothing if it can
const char *HudsonVideo_WhyNotThisSize(int width, int height);

// How many bits a second to ask the encoder for, which goes up with the size of the picture: about 5.5 million at 1280x720, and 12.4 million at 1920x1080
int HudsonVideo_BitsPerSecond(int width, int height);

// Copies a picture of 4 bytes a pixel out of the card's rows, which may be longer than the picture is wide, into rows with no gaps between them. It gives back false, and copies nothing, if the rows are said to be too short to hold the picture.
bool HudsonVideo_CopyPicture(uint8_t *to, int width, int height, const uint8_t *from, int pitch);
