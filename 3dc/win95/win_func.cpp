
// Modified 2026-11-04 by Melanikus for the AvP port:
//   the game can be closed from the taskbar while it isn't the active window
// Modified 2026-11-09 by Melanikus for the AvP port:
//   a test copy (-dev -windowed) keeps running while it isn't the active window, so two copies side by side both go on
#ifdef _WIN32 // windows only
/****

Windows functionality that is definitely
not project specific.

****/

#include "3dc.h"
#include "window_slot.h"

// Externs

extern bool bActive;
extern bool bRunning;
extern int16_t MouseWheelStatus;

void CheckForWindowsMessages(void)
{
	MSG msg;
	
	MouseWheelStatus = 0;

	do
	{
		while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
		{
			if (!GetMessage(&msg, NULL, 0, 0)) {
				return;
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		
		// A test copy (-dev -windowed) never waits here, in front or behind: a host and a joiner on one PC must both keep running, or the
		// one behind the other's window stops answering the network (window_slot.h). Its messages have been handled above all the same.
		if (WindowSlot_IsOn())
		{
			return;
		}

		// JH 13/2/98 - if the app is not active we should not return from the message lopp
		// until the app is re-activated
		if (!bActive)
		{
			ResetFrameCounter();
			Sleep(0);
		}
	}
	while (!bActive && bRunning);    // a player can close the game from the taskbar while it isn't active
}

#endif // ifdef _WIN32
