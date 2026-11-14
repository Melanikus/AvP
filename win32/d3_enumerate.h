// The game's list of video adapters, with each one's display modes and formats, read from Direct3D 9 when the game starts. Added
// 2026-11-14 by Melanikus (Dietrich) for the AvP port (D3D-ENUM-1).
//
// It used to be part of InitialiseDirect3D (d3_func.cpp). It's here, on its own, so a test can hand it a made-up Direct3D that reports
// more adapters, modes and formats than the game's lists hold.
//
// Each list has a fixed size, and nothing is ever written past it:
// - at most MAX_D3D_DRIVERS adapters. The rest are left out; the game draws on the first one.
// - at most MAX_D3D_DISPLAY_MODES different modes for each adapter. A screen with more is ordinary today, and the rest are left out.
// - at most MAX_D3D_FORMATS different formats for each adapter, counted for each adapter on its own.
// What was left out is counted, so InitialiseDirect3D can say so in the log.

#pragma once

#include <d3d9.h>
#include <stdint.h>

// The most video adapters the game keeps
#define MAX_D3D_DRIVERS 5

// The most display modes and formats it keeps for each one
#define MAX_D3D_DISPLAY_MODES 100
#define MAX_D3D_FORMATS 20

// What the game knows about one video adapter
typedef struct D3DDriverInfo
{
	D3DFORMAT				Formats[MAX_D3D_FORMATS];
	D3DADAPTER_IDENTIFIER9	AdapterInfo;
	D3DDISPLAYMODE			DisplayMode[MAX_D3D_DISPLAY_MODES];
	uint32_t				NumModes;     // how many of DisplayMode are filled
	uint32_t				NumFormats;   // how many of Formats are filled
} D3DDRIVERINFO;

// What D3D_EnumerateAdapters found, and what it left out
struct D3DEnumeration
{
	uint32_t adaptersFound;    // how many adapters Direct3D says there are
	uint32_t modesLeftOut;     // modes left out because an adapter's list was full (a mode counts once for each refresh rate listed)
	uint32_t formatsLeftOut;   // formats left out because an adapter's list was full
	HRESULT failure;           // what stopped it, or S_OK
};

// Reads the adapters Direct3D reports into drivers, up to MAX_D3D_DRIVERS, and how many it kept into *numDrivers. For each, it reads
// the modes of every format in displayFormats (numDisplayFormats of them), leaves out those below 640 by 480, the same mode at
// another refresh rate, and any mode Direct3D fails to give, and lists each mode's format once. Gives back false when Direct3D couldn't describe an adapter, with
// report->failure saying why: the game can't go on then.
bool D3D_EnumerateAdapters(IDirect3D9 *d3d9, const D3DFORMAT *displayFormats, uint32_t numDisplayFormats, D3DDRIVERINFO (&drivers)[MAX_D3D_DRIVERS],
	uint32_t *numDrivers, D3DEnumeration *report);

// The place of the video mode the options menu shows, on an adapter's list of count modes (#252, added 2026-11-14 by Melanikus
// (Dietrich)). An adapter can have no mode at all of 640 by 480 or more, and then nothing may read its list: the place stays 0 and isn't
// on the list. With modes, Next and Previous go round from the end to the start, and a place that isn't on the list (below 0, or past
// the end) goes to the start or the end.
int32_t D3D_NextVideoMode(int32_t place, uint32_t count);
int32_t D3D_PreviousVideoMode(int32_t place, uint32_t count);

// Whether a place is one of the list's modes, from 0 to count - 1: the only places the list may be read at
bool D3D_VideoModeIsListed(int32_t place, uint32_t count);
