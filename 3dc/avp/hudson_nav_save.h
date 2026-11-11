// A run's map and paths, saved as plain text beside its log. Written 2026-11-05 by Melanikus for the AvP port.
//
// WHY. Each run works out a map and throws it away, and only the log says roughly where he went. So each run now keeps: the map's places and what kind each is, every way it planned, and the
// trail he really walked. It's the same plain list the 3D view of the map draws from, so that a saved run can be drawn later.
//
// NOTHING READS THESE BACK. Not into the game, anyway. A file the game read back would be something from outside that it had to distrust, and that would need its own decision. So they only go out.
//
// IT CAN'T HURT THE RUN. Each file has a limit on how big it gets, and stops at it, saying so on its last line. If the disk says no, the file stops, the log says so once, and the run carries on.
//
// NOTHING HERE KNOWS ABOUT THE GAME, so a test can try all of it.
#ifndef HUDSON_NAV_SAVE_H
#define HUDSON_NAV_SAVE_H

#include <stdio.h>

long const kHudsonNavSaveMostMapBytes = 64000000;    // about a million places: more than any stock level has
long const kHudsonNavSaveMostWaysBytes = 16000000;   // a long run's every plan
long const kHudsonNavSaveMostTrailBytes = 16000000;  // a line a quarter of a second is about 2 MB an hour

class HudsonNavSaveFile
{
public:
	HudsonNavSaveFile();
	~HudsonNavSaveFile();

	// Starts a new file, emptying any left by an earlier run in this folder. False if it can't. Asking to write to a file that isn't open does nothing, so the callers don't have to check.
	bool Open(const char *path, long mostBytes);

	// Adds a line. No more than kHudsonNavSaveMostLine characters of it are kept, and it stops being written at the limit, or at the first time the disk says no.
	void Line(const char *format, ...);

	// Puts what's been written on the disk, so that a run that dies part way still leaves it all up to here.
	void Flush();

	void Close();

	bool IsOpen() const { return file != 0; }

	// It stopped early. Why is one of "too big" or "the disk said no", or nothing if it hasn't.
	const char *WhyItStopped() const { return why; }

	long Bytes() const { return bytes; }

private:
	HudsonNavSaveFile(const HudsonNavSaveFile &);
	HudsonNavSaveFile &operator=(const HudsonNavSaveFile &);

	FILE *file;
	long bytes;
	long most;
	const char *why;
};

int const kHudsonNavSaveMostLine = 200;

#endif
