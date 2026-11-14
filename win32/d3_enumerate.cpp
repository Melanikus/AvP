// See d3_enumerate.h. Added 2026-11-14 by Melanikus (Dietrich) for the AvP port (D3D-ENUM-1): taken out of InitialiseDirect3D
// (d3_func.cpp), with every list bounded at the point it's written.

#include "d3_enumerate.h"

bool D3D_EnumerateAdapters(IDirect3D9 *d3d9, const D3DFORMAT *displayFormats, uint32_t numDisplayFormats, D3DDRIVERINFO (&drivers)[MAX_D3D_DRIVERS],
	uint32_t *numDrivers, D3DEnumeration *report)
{
	report->adaptersFound = 0;
	report->modesLeftOut = 0;
	report->formatsLeftOut = 0;
	report->failure = S_OK;

	// The number of video cards in the system, and the most of them the list keeps
	uint32_t const found = d3d9->GetAdapterCount();
	uint32_t const kept = found < MAX_D3D_DRIVERS ? found : MAX_D3D_DRIVERS;

	report->adaptersFound = found;
	*numDrivers = kept;

	// Each one's name and description
	for (uint32_t driverIndex = 0; driverIndex < kept; driverIndex++)
	{
		HRESULT const result = d3d9->GetAdapterIdentifier(driverIndex, /*D3DENUM_WHQL_LEVEL*/0, &drivers[driverIndex].AdapterInfo);

		if (FAILED(result))
		{
			report->failure = result;
			return false;
		}
	}

	// Each one's display modes and their formats
	for (uint32_t thisDevice = 0; thisDevice < kept; thisDevice++)
	{
		D3DDRIVERINFO &driver = drivers[thisDevice];

		driver.NumModes = 0;
		driver.NumFormats = 0;

		for (uint32_t displayFormatIndex = 0; displayFormatIndex < numDisplayFormats; displayFormatIndex++)
		{
			// the number of display modes this adapter has in this format
			uint32_t const numAdapterModes = d3d9->GetAdapterModeCount(thisDevice, displayFormats[displayFormatIndex]);

			for (uint32_t modeIndex = 0; modeIndex < numAdapterModes; modeIndex++)
			{
				D3DDISPLAYMODE mode;

				// A mode Direct3D couldn't give is skipped: what's in mode then is whatever was there before, not a mode
				// (Modified 2026-11-14 by Melanikus (Dietrich), D3D-ENUM-1 B4: the answer used to be ignored.)
				if (FAILED(d3d9->EnumAdapterModes(thisDevice, displayFormats[displayFormatIndex], modeIndex, &mode)))
				{
					continue;
				}

				// Low-resolution modes are left out
				if (mode.Width < 640 || mode.Height < 480)
				{
					continue;
				}

				// So is a mode already listed: the same mode at another refresh rate
				uint32_t j = 0;

				for (; j < driver.NumModes; j++)
				{
					if (driver.DisplayMode[j].Width == mode.Width && driver.DisplayMode[j].Height == mode.Height && driver.DisplayMode[j].Format == mode.Format)
					{
						break;
					}
				}

				if (j < driver.NumModes)
				{
					continue;
				}

				// A new mode goes on the list, while there's room
				if (driver.NumModes == MAX_D3D_DISPLAY_MODES)
				{
					report->modesLeftOut++;
					continue;
				}

				driver.DisplayMode[driver.NumModes] = mode;
				driver.NumModes++;

				// and its format, once, on this adapter's own list of formats, while there's room
				uint32_t f = 0;

				for (; f < driver.NumFormats; f++)
				{
					if (mode.Format == driver.Formats[f])
					{
						break;
					}
				}

				if (f < driver.NumFormats)
				{
					continue;
				}

				if (driver.NumFormats == MAX_D3D_FORMATS)
				{
					report->formatsLeftOut++;
					continue;
				}

				driver.Formats[driver.NumFormats] = mode.Format;
				driver.NumFormats++;
			}
		}
	}

	return true;
}
