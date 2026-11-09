
// Modified 2026-11-04 by Melanikus for the AvP port:
//   the game can be closed from the taskbar while it isn't the active window
#ifdef _WIN32 // windows only
/****

Windows functionality that is definitely
not project specific.

****/

#include "3dc.h"

// Externs

extern bool bActive;
extern bool bRunning;
extern int16_t MouseWheelStatus;
extern bool Net_GameGoesOnWhileInactive(void);   // networking.cpp

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

		// JH 13/2/98 - if the app is not active we should not return from the message lopp
		// until the app is re-activated
		// Modified 2026-11-09 by Melanikus for the AvP port: except in a network game. There the others go on playing, and everything
		// that keeps the game in it runs on this thread: ENet ends a connection it stops hearing from, and the relay ends a host's room
		// it isn't told about every 15 s. A host whose window wasn't in front lost its room that way. So a network game goes on behind
		// another window; the keyboard and the mouse's buttons only reach the window in front, and the rest of the mouse and the gamepad
		// are left out while this one isn't (win_proj.cpp, di_func.cpp).
		if (!bActive && !Net_GameGoesOnWhileInactive())
		{
			ResetFrameCounter();
			Sleep(0);
		}
	}
	while (!bActive && bRunning && !Net_GameGoesOnWhileInactive());   // a player can close the game from the taskbar while it isn't active
}

#endif // ifdef _WIN32
