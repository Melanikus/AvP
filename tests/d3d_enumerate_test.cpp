// The test of the game's list of video adapters, modes and formats (win32/d3_enumerate.cpp, D3D-ENUM-1). Written 2026-11-14 by
// Melanikus (Dietrich) for the AvP port.
//
// WHAT IT'S FOR: the list used to be filled with no bound. A PC with more than 5 video adapters wrote past the game's 5; a screen with
// more than 100 different modes, which is ordinary today, wrote past the 100; and the count of formats was shared by every adapter, so
// a second adapter wrote past its own 20. Here a made-up Direct3D reports more of each than the lists hold, and the lists sit in front
// of guard bytes, so a write past any of them shows as a FAILED line: a changed count, a changed name, or a changed guard byte.
//
// Nothing here uses a real Direct3D or a window.

#include "d3_enumerate.h"

#include <stdio.h>
#include <string.h>

static int checksFailed = 0;

static void Check(bool passed, const char *what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what);

	if (!passed)
	{
		checksFailed++;
	}
}

// A made-up Direct3D 9. It reports what the test sets, and remembers the highest adapter it was asked about.
class FakeD3D : public IDirect3D9
{
public:
	UINT adapters = 1;
	UINT modesPerFormat = 0;        // how many modes each format reports, on every adapter
	UINT onlyAdapterWithModes = ~0u; // when set, only this adapter has modes
	bool everyModeItsOwnFormat = false;
	UINT formatsPerAdapter = 0;     // when set, mode i's format is one of this many, different for each adapter
	UINT failIdentifierAt = ~0u;
	UINT failModeAt = ~0u;          // this mode of every format fails, leaving rubbish where the mode would be
	UINT highestAsked = 0;
	bool askedAtAll = false;

	void Asked(UINT adapter)
	{
		askedAtAll = true;
		highestAsked = adapter > highestAsked ? adapter : highestAsked;
	}

	STDMETHOD(QueryInterface)(REFIID, void **out) { *out = 0; return E_NOINTERFACE; }
	STDMETHOD_(ULONG, AddRef)() { return 1; }
	STDMETHOD_(ULONG, Release)() { return 1; }
	STDMETHOD(RegisterSoftwareDevice)(void *) { return E_NOTIMPL; }
	STDMETHOD_(UINT, GetAdapterCount)() { return adapters; }

	STDMETHOD(GetAdapterIdentifier)(UINT adapter, DWORD, D3DADAPTER_IDENTIFIER9 *identifier)
	{
		Asked(adapter);
		memset(identifier, 0, sizeof(*identifier));
		snprintf(identifier->Driver, sizeof(identifier->Driver), "fake driver %u", adapter);
		snprintf(identifier->Description, sizeof(identifier->Description), "fake adapter %u", adapter);
		return adapter == failIdentifierAt ? E_FAIL : S_OK;
	}

	STDMETHOD_(UINT, GetAdapterModeCount)(UINT adapter, D3DFORMAT)
	{
		Asked(adapter);
		return onlyAdapterWithModes == ~0u || onlyAdapterWithModes == adapter ? modesPerFormat : 0;
	}

	// Mode i of a format is (640 + i) by 480, at 60 Hz, so every mode is different, and in the format asked for unless the test says
	STDMETHOD(EnumAdapterModes)(UINT adapter, D3DFORMAT format, UINT mode, D3DDISPLAYMODE *out)
	{
		Asked(adapter);

		// A failed call, as a driver can make one: what's left in the mode is rubbish, and large enough to pass the size filter
		if (mode == failModeAt)
		{
			memset(out, 0xCD, sizeof(*out));
			return E_FAIL;
		}

		out->Width = 640 + mode;
		out->Height = 480;
		out->RefreshRate = 60;
		out->Format = format;

		if (everyModeItsOwnFormat)
		{
			out->Format = (D3DFORMAT)(1000 + mode);
		}
		else if (formatsPerAdapter > 0)
		{
			out->Format = (D3DFORMAT)(2000 + adapter * 100 + mode % formatsPerAdapter);
		}

		return S_OK;
	}

	STDMETHOD(GetAdapterDisplayMode)(UINT, D3DDISPLAYMODE *) { return E_NOTIMPL; }
	STDMETHOD(CheckDeviceType)(UINT, D3DDEVTYPE, D3DFORMAT, D3DFORMAT, BOOL) { return E_NOTIMPL; }
	STDMETHOD(CheckDeviceFormat)(UINT, D3DDEVTYPE, D3DFORMAT, DWORD, D3DRESOURCETYPE, D3DFORMAT) { return E_NOTIMPL; }
	STDMETHOD(CheckDeviceMultiSampleType)(UINT, D3DDEVTYPE, D3DFORMAT, BOOL, D3DMULTISAMPLE_TYPE, DWORD *) { return E_NOTIMPL; }
	STDMETHOD(CheckDepthStencilMatch)(UINT, D3DDEVTYPE, D3DFORMAT, D3DFORMAT, D3DFORMAT) { return E_NOTIMPL; }
	STDMETHOD(CheckDeviceFormatConversion)(UINT, D3DDEVTYPE, D3DFORMAT, D3DFORMAT) { return E_NOTIMPL; }
	STDMETHOD(GetDeviceCaps)(UINT, D3DDEVTYPE, D3DCAPS9 *) { return E_NOTIMPL; }
	STDMETHOD_(HMONITOR, GetAdapterMonitor)(UINT) { return 0; }
	STDMETHOD(CreateDevice)(UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS *, IDirect3DDevice9 **) { return E_NOTIMPL; }
};

// The game's list, as the game holds it, with guard bytes behind it: room for three more adapters' worth, so a write past the end
// lands here, where the test can see it
static struct
{
	D3DDRIVERINFO drivers[MAX_D3D_DRIVERS];
	unsigned char guard[3 * sizeof(D3DDRIVERINFO)];
} list;

static const unsigned char kGuard = 0xA5;

static void Clear()
{
	memset(list.drivers, 0, sizeof(list.drivers));
	memset(list.guard, kGuard, sizeof(list.guard));
}

static bool GuardIsWhole()
{
	for (size_t i = 0; i < sizeof(list.guard); i++)
	{
		if (list.guard[i] != kGuard)
		{
			return false;
		}
	}

	return true;
}

// Whether each adapter kept still has its own name: the Formats list is right in front of it, so a write past the formats changes it
static bool NamesAreWhole(uint32_t count)
{
	for (uint32_t i = 0; i < count; i++)
	{
		char driver[32];
		char description[32];

		snprintf(driver, sizeof(driver), "fake driver %u", i);
		snprintf(description, sizeof(description), "fake adapter %u", i);

		if (strcmp(list.drivers[i].AdapterInfo.Driver, driver) != 0 || strcmp(list.drivers[i].AdapterInfo.Description, description) != 0)
		{
			return false;
		}
	}

	return true;
}

static const D3DFORMAT kFormats[] = { D3DFMT_X8R8G8B8, D3DFMT_A8R8G8B8, D3DFMT_R8G8B8 };
static uint32_t const kFormatCount = sizeof(kFormats) / sizeof(kFormats[0]);

static bool Enumerate(FakeD3D &d3d9, uint32_t formats, uint32_t *numDrivers, D3DEnumeration *report)
{
	return D3D_EnumerateAdapters(&d3d9, kFormats, formats, list.drivers, numDrivers, report);
}

static void TestAsBefore()
{
	puts("A PC the lists hold, read as before:");

	FakeD3D d3d9;
	uint32_t numDrivers = 99;
	D3DEnumeration report;

	Clear();
	d3d9.adapters = 2;
	d3d9.modesPerFormat = 10;
	Check(Enumerate(d3d9, kFormatCount, &numDrivers, &report) && numDrivers == 2 && report.adaptersFound == 2 && report.failure == S_OK,
		"two adapters are read, and both kept");
	Check(list.drivers[0].NumModes == 30 && list.drivers[1].NumModes == 30 && list.drivers[0].NumFormats == 3 && list.drivers[1].NumFormats == 3,
		"each has its 10 modes in each of 3 formats, and the 3 formats once each");
	Check(list.drivers[0].DisplayMode[0].Width == 640 && list.drivers[0].DisplayMode[29].Width == 649 && list.drivers[0].DisplayMode[29].Format == D3DFMT_R8G8B8
		&& list.drivers[0].Formats[2] == D3DFMT_R8G8B8, "in the order Direct3D gives them");
	Check(report.modesLeftOut == 0 && report.formatsLeftOut == 0 && NamesAreWhole(2) && GuardIsWhole(), "with nothing left out, and nothing written past");

	numDrivers = 99;
	d3d9 = FakeD3D();
	d3d9.adapters = 0;
	Check(Enumerate(d3d9, kFormatCount, &numDrivers, &report) && numDrivers == 0 && report.adaptersFound == 0 && !d3d9.askedAtAll, "a PC with no adapter gives an empty list, asking about none");
}

// A made-up Direct3D whose modes come in a given order, to see what's left out
class ModesD3D : public FakeD3D
{
public:
	const D3DDISPLAYMODE *modes = 0;
	UINT count = 0;

	STDMETHOD_(UINT, GetAdapterModeCount)(UINT, D3DFORMAT) { return count; }

	STDMETHOD(EnumAdapterModes)(UINT, D3DFORMAT, UINT mode, D3DDISPLAYMODE *out)
	{
		*out = modes[mode];
		return S_OK;
	}
};

static void TestWhatsLeftOut()
{
	puts("Modes the list leaves out:");

	D3DDISPLAYMODE const modes[] =
	{
		{ 639, 480, 60, D3DFMT_X8R8G8B8 },    // too narrow
		{ 640, 479, 60, D3DFMT_X8R8G8B8 },    // too short
		{ 640, 480, 60, D3DFMT_X8R8G8B8 },
		{ 640, 480, 75, D3DFMT_X8R8G8B8 },    // the same at another refresh rate
		{ 640, 480, 60, D3DFMT_A8R8G8B8 },    // the same size in another format: a mode of its own
		{ 1920, 1080, 144, D3DFMT_X8R8G8B8 },
	};
	ModesD3D d3d9;
	uint32_t numDrivers = 0;
	D3DEnumeration report;

	Clear();
	d3d9.modes = modes;
	d3d9.count = sizeof(modes) / sizeof(modes[0]);
	Check(Enumerate(d3d9, 1, &numDrivers, &report) && list.drivers[0].NumModes == 3 && list.drivers[0].NumFormats == 2,
		"a mode below 640 by 480 is left out, and so is one already listed at another refresh rate; the same size in another format isn't");
	Check(list.drivers[0].DisplayMode[0].RefreshRate == 60 && list.drivers[0].DisplayMode[2].Width == 1920 && report.modesLeftOut == 0,
		"the first refresh rate listed is the one kept, and none of these counts as left out for room");
}

static void TestTooMany()
{
	puts("More than the lists hold:");

	FakeD3D d3d9;
	uint32_t numDrivers = 0;
	D3DEnumeration report;

	// One more mode than the list holds, so a write past it would be one mode, landing on the counts behind the list
	Clear();
	d3d9.modesPerFormat = MAX_D3D_DISPLAY_MODES + 1;
	Check(Enumerate(d3d9, 1, &numDrivers, &report) && list.drivers[0].NumModes == MAX_D3D_DISPLAY_MODES && report.modesLeftOut == 1,
		"101 different modes: 100 kept, 1 left out and counted");
	Check(list.drivers[0].DisplayMode[MAX_D3D_DISPLAY_MODES - 1].Width == 640 + MAX_D3D_DISPLAY_MODES - 1 && list.drivers[0].NumFormats == 1 && GuardIsWhole(),
		"the first 100 are the ones kept, the count of formats is whole, and nothing is written past");

	Clear();
	d3d9.modesPerFormat = 150;
	Check(Enumerate(d3d9, kFormatCount, &numDrivers, &report) && list.drivers[0].NumModes == MAX_D3D_DISPLAY_MODES && report.modesLeftOut == 3 * 150 - 100
		&& NamesAreWhole(1) && GuardIsWhole(), "150 modes in each of 3 formats, as a screen of today can have: 100 kept, the other 350 counted");

	// One more format than the list holds, so a write past it would land on the adapter's name, right behind the formats
	Clear();
	d3d9.modesPerFormat = MAX_D3D_FORMATS + 1;
	d3d9.everyModeItsOwnFormat = true;
	Check(Enumerate(d3d9, 1, &numDrivers, &report) && list.drivers[0].NumFormats == MAX_D3D_FORMATS && report.formatsLeftOut == 1 && NamesAreWhole(1),
		"21 different formats: 20 kept, 1 left out and counted, and the adapter's name behind them is whole");
	Check(list.drivers[0].NumModes == MAX_D3D_FORMATS + 1 && list.drivers[0].Formats[MAX_D3D_FORMATS - 1] == (D3DFORMAT)(1000 + MAX_D3D_FORMATS - 1),
		"all 21 modes are kept, as the mode list has room, and the first 20 formats are the ones listed");

	// Eight adapters, more than the 5 kept: anything written for the sixth would land in the guard bytes
	Clear();
	d3d9 = FakeD3D();
	d3d9.adapters = 8;
	d3d9.modesPerFormat = 4;
	Check(Enumerate(d3d9, 1, &numDrivers, &report) && numDrivers == MAX_D3D_DRIVERS && report.adaptersFound == 8 && d3d9.highestAsked == MAX_D3D_DRIVERS - 1,
		"8 adapters: 5 kept, 8 said, and the sixth on is never asked about");
	Check(NamesAreWhole(MAX_D3D_DRIVERS) && list.drivers[MAX_D3D_DRIVERS - 1].NumModes == 4 && GuardIsWhole(), "and nothing is written past the fifth");

	Clear();
	d3d9 = FakeD3D();
	d3d9.adapters = 0xFFFFFFFFu;
	Check(Enumerate(d3d9, 1, &numDrivers, &report) && numDrivers == MAX_D3D_DRIVERS && d3d9.highestAsked == MAX_D3D_DRIVERS - 1 && GuardIsWhole(),
		"and the largest count an adapter could say is kept to 5 too");
}

static void TestEachAdapterItsOwn()
{
	puts("Each adapter's own counts:");

	FakeD3D d3d9;
	uint32_t numDrivers = 0;
	D3DEnumeration report;

	// Two adapters of 15 formats each: a count shared by both reached 30 on the second, past its 20
	Clear();
	d3d9.adapters = 2;
	d3d9.modesPerFormat = 30;
	d3d9.formatsPerAdapter = 15;
	Check(Enumerate(d3d9, 1, &numDrivers, &report) && list.drivers[0].NumFormats == 15 && list.drivers[1].NumFormats == 15 && report.formatsLeftOut == 0,
		"two adapters of 15 formats each have 15 each: the count is each adapter's own");
	Check(list.drivers[1].Formats[0] == (D3DFORMAT)(2100) && list.drivers[1].Formats[14] == (D3DFORMAT)(2114) && NamesAreWhole(2) && GuardIsWhole(),
		"and the second's list starts at its own first format, with nothing written past");

	// Read again over the same lists, with fewer modes: each count starts again from 0
	d3d9.modesPerFormat = 5;
	d3d9.formatsPerAdapter = 5;
	Check(Enumerate(d3d9, 1, &numDrivers, &report) && list.drivers[0].NumModes == 5 && list.drivers[0].NumFormats == 5 && list.drivers[1].NumModes == 5
		&& list.drivers[1].NumFormats == 5, "read again over lists that were filled, each count starts again from 0");

	// Only the third adapter has modes
	Clear();
	d3d9 = FakeD3D();
	d3d9.adapters = 3;
	d3d9.modesPerFormat = 7;
	d3d9.onlyAdapterWithModes = 2;
	Check(Enumerate(d3d9, 1, &numDrivers, &report) && list.drivers[0].NumModes == 0 && list.drivers[1].NumModes == 0 && list.drivers[2].NumModes == 7,
		"each adapter's modes are its own");
}

static void TestFailure()
{
	puts("When Direct3D can't describe an adapter:");

	FakeD3D d3d9;
	uint32_t numDrivers = 0;
	D3DEnumeration report;

	Clear();
	d3d9.adapters = 4;
	d3d9.failIdentifierAt = 2;
	Check(!Enumerate(d3d9, 1, &numDrivers, &report) && report.failure == E_FAIL && report.adaptersFound == 4 && d3d9.highestAsked == 2,
		"the reading stops at the adapter that failed, and says why, so the game stops as it did before");

	// A mode Direct3D fails to give (D3D-ENUM-1 B4)
	Clear();
	d3d9 = FakeD3D();
	d3d9.modesPerFormat = 10;
	d3d9.failModeAt = 3;

	bool noRubbish = true;

	Check(Enumerate(d3d9, 1, &numDrivers, &report) && list.drivers[0].NumModes == 9, "a mode Direct3D fails to give is skipped, and the other 9 are kept");

	for (uint32_t i = 0; i < list.drivers[0].NumModes; i++)
	{
		noRubbish = noRubbish && list.drivers[0].DisplayMode[i].Width < 0xCDCDCDCDu && list.drivers[0].DisplayMode[i].Format == D3DFMT_X8R8G8B8;
	}

	Check(noRubbish && list.drivers[0].NumFormats == 1 && GuardIsWhole(), "and nothing of what the failed call left behind is on the list");
}

int main()
{
	// Each line is written as it's printed, so none is lost if a check after it brings the program down
	setvbuf(stdout, 0, _IONBF, 0);

	TestAsBefore();
	TestWhatsLeftOut();
	TestTooMany();
	TestEachAdapterItsOwn();
	TestFailure();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
