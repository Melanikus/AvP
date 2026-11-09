// Two more lines for the network's 10-second report: how this machine's frames went, and how often the other players' and NPCs' figures
// had to wait for news. Written 2026-11-08 by Melanikus for the AvP port, for the co-op lag measurements (issue #88).
//
// WHAT IT'S FOR: when a tester says "it lagged", or "they got stuck, then teleported", the log should say so with numbers. net_stats.h
// already says what the network was doing every 10 seconds. These say what that did to the game:
//
//     net: frames: 600 in the last 10 s, the slowest 41 ms, 3 slower than 33 ms
//     net: figures: 4 went more than 100 ms with nothing new in the last 10 s; the longest 740 ms (a player's), then a jump of 1.4 m;
//          the biggest jump after a wait 1.4 m
//
// A frame slower than 33 ms is one that took as long as two at 60 a second. A figure is another player, or an NPC the host runs, as this
// machine draws it. "Nothing new" means no message about it came in, so it stood still or went on guessing (see engine/coop-lag-plan.md).
// The jump is how far it moved in the frame its news came. These are counted, not written one by one, so a bad connection can't fill the
// log: it's two lines every 10 seconds, whatever happens.
//
// networking.cpp keeps the counts and writes the lines with the others. pldnet.cpp tells it each frame's time, and pldghost.cpp tells it
// each wait. tests/net_watch_test.cpp tries all of this.

#ifndef NET_WATCH_H
#define NET_WATCH_H

#include <stdio.h>
#include <stddef.h>

// A frame slower than this is counted: as long as two frames at 60 a second
static const unsigned int kNetSlowFrameMs = 33;

// A figure with nothing new for longer than this is counted: about six frames
static const unsigned int kNetFigureWaitMs = 100;

// ---- Frames

struct NetFrameWatch
{
	unsigned int frames;
	unsigned int slow;
	unsigned int slowestMs;
};

inline void NetFrameWatch_Note(NetFrameWatch *watch, unsigned int frameMs)
{
	watch->frames++;

	if (frameMs > kNetSlowFrameMs)
	{
		watch->slow++;
	}

	if (frameMs > watch->slowestMs)
	{
		watch->slowestMs = frameMs;
	}
}

// A frame's time in milliseconds, from the game's own frame time, which counts 65536 to a second. One below nothing counts as nothing.
inline unsigned int NetFrameWatch_Milliseconds(int frameTime)
{
	return frameTime <= 0 ? 0 : (unsigned int)(((unsigned long long)frameTime * 1000) >> 16);
}

inline void NetFrameWatch_Line(const NetFrameWatch &watch, unsigned int seconds, char *words, size_t size)
{
	if (size == 0)
	{
		return;
	}

	int const used = _snprintf(words, size - 1, "net: frames: %u in the last %u s, the slowest %u ms, %u slower than %u ms", watch.frames, seconds, watch.slowestMs, watch.slow, kNetSlowFrameMs);
	words[(used < 0 || (size_t)used >= size) ? size - 1 : (size_t)used] = '\0';
}

// ---- Figures waiting for news

// For one figure: when its last news came. It starts as all zeros, as a ghost's memory does when it's made.
struct NetFigureNews
{
	unsigned int lastMs;
	unsigned char seen;
};

// The frame brought news about this figure, at nowMs (a clock in milliseconds that may go round). Gives back true, with how long it waited
// in *waitedMs, when it waited longer than kNetFigureWaitMs since its last news. The first news ever has nothing to wait from.
inline bool NetFigureNews_Came(NetFigureNews *news, unsigned int nowMs, unsigned int *waitedMs)
{
	bool const hadNews = news->seen != 0;
	unsigned int const waited = nowMs - news->lastMs;   // right even when the clock has gone round

	news->lastMs = nowMs;
	news->seen = 1;

	if (!hadNews || waited <= kNetFigureWaitMs)
	{
		return false;
	}

	*waitedMs = waited;
	return true;
}

struct NetFigureWatch
{
	unsigned int waits;
	unsigned int longestMs;
	unsigned int jumpAfterLongestMm;
	unsigned int biggestJumpMm;
	unsigned char longestWasAPlayer;
};

// One wait longer than kNetFigureWaitMs, and how far the figure moved in millimetres in the frame its news came
inline void NetFigureWatch_Note(NetFigureWatch *watch, unsigned int waitedMs, unsigned int jumpMm, bool aPlayer)
{
	if (waitedMs <= kNetFigureWaitMs)
	{
		return;
	}

	watch->waits++;

	if (waitedMs > watch->longestMs)
	{
		watch->longestMs = waitedMs;
		watch->jumpAfterLongestMm = jumpMm;
		watch->longestWasAPlayer = aPlayer ? 1 : 0;
	}

	if (jumpMm > watch->biggestJumpMm)
	{
		watch->biggestJumpMm = jumpMm;
	}
}

inline void NetFigureWatch_Line(const NetFigureWatch &watch, unsigned int seconds, char *words, size_t size)
{
	if (size == 0)
	{
		return;
	}

	int used;

	if (watch.waits == 0)
	{
		used = _snprintf(words, size - 1, "net: figures: none went more than %u ms with nothing new in the last %u s", kNetFigureWaitMs, seconds);
	}
	else
	{
		// Millimetres as metres, to a tenth, rounded
		unsigned int const jump = (watch.jumpAfterLongestMm + 50) / 100;
		unsigned int const biggest = (watch.biggestJumpMm + 50) / 100;

		used = _snprintf(words, size - 1, "net: figures: %u went more than %u ms with nothing new in the last %u s; the longest %u ms (%s), then a jump of %u.%u m; the biggest jump after a wait %u.%u m",
			watch.waits, kNetFigureWaitMs, seconds, watch.longestMs, watch.longestWasAPlayer ? "a player's" : "an NPC's", jump / 10, jump % 10, biggest / 10, biggest % 10);
	}

	words[(used < 0 || (size_t)used >= size) ? size - 1 : (size_t)used] = '\0';
}

#endif
