// AvP port by Barry Duncan.
// Modified 2026-11-04 by Melanikus for the AvP port:
//   a run doesn't take the focus, keeps going without it, and has nobody to show a message to
// Modified 2026-11-05 by Melanikus for the AvP port:
//   a run's window can't be activated, and sizing it doesn't take the focus
//   nothing a person does with the keyboard or the mouse reaches a run
//   holding a run's window by its title bar, or a menu on it, can't stop its frames
// Modified 2026-11-06 by Melanikus for the AvP port:
//   a run's window goes on the second screen by the game's own doing, with a part of it for each game
// Modified 2026-11-07 by Melanikus for the AvP port:
//   error messages name this file by hand, from the top of the repository, instead of with __FILE__, which put the whole folder
//   it was built in, with the name of whoever built it, into the game
// Modified 2026-11-09 by Melanikus for the AvP port:
//   with -dev -windowed, a test copy's window goes in its own half of the screen (window_slot.h), not the top left corner

#ifdef _WIN32
/****

Project specific (or potentially project specific) windows functionality

****/

// To link code to main C functions

#include "console.h"
#include "iofocus.h"
#include "logString.h"

extern unsigned char KeyboardInput[256];

#include "3dc.h"
#include "hudson.h"
#include "inline.h"
#include "psndplat.h"
#include "onscreenKeyboard.h"
#include "window_slot.h"

#include "reentrant_queue.h"
	// Added 21/11/97 by DHM: support for a queue of Windows
	// messages to avoid problems with re-entrancy due to WinProc()

#include "dxlog.h"
#include <zmouse.h>
#include "vdb.h"
#include <assert.h>

// This file's name in the error log, written by hand. __FILE__ would give the whole folder it was built in (C:\Users\<name>\...),
// and that would go into the game for anyone to read. tools/check-package-imports.ps1 refuses a package that names any such folder.
static const char kThisFile[] = "3dc/avp/win95/win_proj.cpp";

void MakeToAsciiTable(void);

// mousewheel msg id
UINT const RWM_MOUSEWHEEL = RegisterWindowMessage(MSH_MOUSEWHEEL);
int16_t MouseWheelStatus;

unsigned char ksarray[256];
unsigned char ToAsciiTable[256][256];

// Dubious
#define grabmousecapture FALSE

/*
	Name of project window etc for windows interface
	Project specific (fairly obviously...).
	Determines the default menu in which the application
	appears (altho' other code will undoubtedly be needed
	as well...), so that a NULL here should ensure no menu.
*/

const char *className = "AvP";
// const char *windowTitle = "AvP";

//	Necessary globals

HWND hWndMain;
HINSTANCE hInst; // temp

bool bActive  = true;   // is application active?
bool bRunning = true;

// Parameters for main (assumed full screen) window
int WinLeftX, WinRightX, WinTopY, WinBotY;

// Externs
extern int VideoMode;
extern int WindowMode;
extern WINSCALEXY TopLeftSubWindow;
extern WINSCALEXY ExtentXYSubWindow;

// Window procedure (to run continuously while WinMain is active).
// Only necessary functions are handling keyboard input (for the moment
// at least - cf. DirectInput) and dealing with important system 
// messages, e.g. WM_PAINT.

// Remember to support all the keys you need for your project
// for both KEYUP and KEYDOWN messages!!!

// IMPORTANT!!! The WindowProc is project specific
// by default, since various nifty hacks can always
// be implemented directly via the windows procedure

extern void KeyboardEntryQueue_Add(char c);
extern void IngameKeyboardInput_KeyDown(unsigned char key);
extern void IngameKeyboardInput_KeyUp(unsigned char key);
extern void IngameKeyboardInput_ClearBuffer(void);
extern void Mouse_ButtonUp(unsigned char button);
extern void Mouse_ButtonDown(unsigned char button);

long xPosRelative = 0;
long yPosRelative = 0;
bool mouseMoved = false;

int inpcalls = 0;

// What someone does with the keyboard, the mouse's buttons or its wheel, or the mouse passing over the window, which the game would hide the pointer for.
static bool IsSomeonesHand(UINT message)
{
	switch (message)
	{
		case WM_MOUSEWHEEL:
		case WM_CHAR:
		case WM_KEYDOWN:
		case WM_KEYUP:
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_XBUTTONDOWN:
		case WM_XBUTTONUP:
		case WM_SETCURSOR:
			return true;

		default:
			return false;
	}
}

// What someone does with the keyboard, the mouse's buttons or its wheel, or the mouse passing over the window, which the game would hide the pointer for.
LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC			hdc;
	RECT		NewWindCoord;

	if (message == RWM_MOUSEWHEEL) {
		message = WM_MOUSEWHEEL;
	}

	// Nobody's playing a run. What it shows must not depend on someone's hand, even if the window is under the mouse
	if (HudsonRunIsOn && IsSomeonesHand(message)) {
		return ::DefWindowProc(hWnd, message, wParam, lParam);
	}

	switch (message)
	{
		case WM_MOUSEWHEEL:
		{
			MouseWheelStatus = (int16_t)HIWORD(wParam);
			return 0;
		}

		case WM_INPUT:
		{
			UINT dwSize = 0;
			::GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));
			assert(dwSize == 40);

			static BYTE lpb[40];

			// Modified 2026-11-09 by Melanikus for the AvP port: the read is only done when what Windows has fits the room for it
			if (dwSize > sizeof(lpb) || ::GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) == (UINT)-1)
			{
				return 0;
			}

			RAWINPUT *raw = (RAWINPUT*)lpb;

			// Modified 2026-11-09 by Melanikus for the AvP port: the mouse is read even while another window is in front (RIDEV_INPUTSINK,
			// below), and a network game goes on then (win_func.cpp). A mouse moved in that other window must not turn the player here.
			if (raw->header.dwType == RIM_TYPEMOUSE && bActive)
			{
				mouseMoved = true;
				xPosRelative += raw->data.mouse.lLastX;
				yPosRelative += raw->data.mouse.lLastY;
			}

			return 0;
			break;
		}

		// 21/11/97 DHM: Added processing of WM_CHAR messages:
		case WM_CHAR:
		{
			if (IOFOCUS_Get() & IOFOCUS_NEWCONSOLE)
			{
				Con_AddTypedChar((char)wParam);
				break;
			}

			RE_ENTRANT_QUEUE_WinProc_AddMessage_WM_CHAR((char)wParam);
			KeyboardEntryQueue_Add((char)wParam);
			break;
		}
		case WM_KEYDOWN:
		{
			RE_ENTRANT_QUEUE_WinProc_AddMessage_WM_KEYDOWN(wParam);
		}
		// it's intentional for this case to fall through to WM_SYSKEYDOWN
		case WM_SYSKEYDOWN:
		{
			// handle left/right alt keys
			if (wParam == VK_MENU)
			{
				if (lParam&(1<<24)) {
					wParam = VK_RMENU;
				}
				else {
					wParam = VK_LMENU;
				}
			}

			// handle left/right control keys
			if (wParam == VK_CONTROL)
			{
				if (lParam&(1<<24)) {
					wParam = VK_RCONTROL;
				}
				else {
					wParam = VK_LCONTROL;
				}
			}

			// handle left/right shift keys
			if (wParam == VK_SHIFT)
			{
				if ((::GetKeyState(VK_RSHIFT) & 0x8000) && (KeyboardInput[KEY_RIGHTSHIFT] == FALSE))
				{
					wParam = VK_RSHIFT;
				}
				else if ((::GetKeyState(VK_LSHIFT) & 0x8000) && (KeyboardInput[KEY_LEFTSHIFT] == FALSE))
				{
					wParam = VK_LSHIFT;
				}
			}

			IngameKeyboardInput_KeyDown((char)wParam);
#if 0
			int scancode = (lParam>>16)&255;
			unsigned char vkcode = (wParam&255);

			// ignore the status of caps lock
			//ksarray[VK_CAPITAL] = 0;
			//ksarray[VK_CAPITAL] = GetKeyState(VK_CAPITAL);
			if (vkcode!=VK_CAPITAL && vkcode!=VK_SCROLL)
			{
				#if 0
				WORD output;
				if (ToAscii(vkcode,scancode,&ksarray[0],&output,0))
				{
					IngameKeyboardInput_KeyDown((unsigned char)(output));
				}
				#else
				if (ToAsciiTable[vkcode][scancode])
				{
					char buf[100];
					sprintf(buf, "vkcode: %d scancode: %d\n", vkcode ,scancode);
					OutputDebugString(buf);
//					IngameKeyboardInput_KeyDown(ToAsciiTable[vkcode][scancode]);
				}
				#endif
			}
#endif
			// reset caps lock status
			//ksarray[VK_CAPITAL] = GetKeyState(VK_CAPITAL);
			//ToAscii(wParam&255,scancode,&ksarray[0],&output,0);
			//return 0;
			break;
		}

		case WM_SYSKEYUP:
		case WM_KEYUP:
		{
			// handle left/right alt keys
			if (wParam == VK_MENU)
			{
				if (lParam&(1<<24)) {
					wParam = VK_RMENU;
				}
				else {
					wParam = VK_LMENU;
				}
			}

			// handle left/right control keys
			if (wParam == VK_CONTROL)
			{
				if (lParam&(1<<24)) {
					wParam = VK_RCONTROL;
				}
				else {
					wParam = VK_LCONTROL;
				}
			}

			// handle left/right shift keys
			if (wParam == VK_SHIFT)
			{
				if ((!(::GetKeyState(VK_RSHIFT) & 0x8000)) && (KeyboardInput[KEY_RIGHTSHIFT] == TRUE))
				{
					wParam = VK_RSHIFT;
				}
				else if ((!(::GetKeyState(VK_LSHIFT) & 0x8000)) && (KeyboardInput[KEY_LEFTSHIFT] == TRUE))
				{
					wParam = VK_LSHIFT;
				}
			}

			IngameKeyboardInput_KeyUp((char)wParam);
#if 0
			int scancode = (lParam>>16)&255;
			unsigned char vkcode = (wParam&255);

			// ignore the status of caps lock
			//ksarray[VK_CAPITAL] = 0;
//MakeToAsciiTable();
			//ksarray[VK_CAPITAL] = GetKeyState(VK_CAPITAL);
			if (vkcode!=VK_CAPITAL && vkcode!=VK_SCROLL)
			{
				#if 0
				WORD output;
				unsigned char z = ToAscii(vkcode,scancode,&ksarray[0],&output,0);
				unsigned char a = (unsigned char)output;
				unsigned char b = ToAsciiTable[vkcode][scancode];
				#endif
				#if 0
				WORD output;
				if (ToAscii(vkcode,scancode,&ksarray[0],&output,0))
				{
					IngameKeyboardInput_KeyUp((unsigned char)(output));
				}
				#else
				if (ToAsciiTable[vkcode][scancode])
				{
					IngameKeyboardInput_KeyUp(ToAsciiTable[vkcode][scancode]);
				}
				#endif
			}
			// reset caps lock status
			//ksarray[VK_CAPITAL] = GetKeyState(VK_CAPITAL);
			//ToAscii(wParam&255,scancode,&ksarray[0],&output,0);
#endif
			//return 0;
			break;
		}
		case WM_LBUTTONDOWN:
		{
			Mouse_ButtonDown(LeftMouse);
			break;
		}
		case WM_LBUTTONUP:
		{
			Mouse_ButtonUp(LeftMouse);
			break;
		}
		case WM_RBUTTONDOWN:
		{
			Mouse_ButtonDown(RightMouse);
			break;
		}
		case WM_RBUTTONUP:
		{
			Mouse_ButtonUp(RightMouse);
			break;
		}
		case WM_MBUTTONDOWN:
		{
			Mouse_ButtonDown(MiddleMouse);
			break;
		}
		case WM_MBUTTONUP:
		{
			Mouse_ButtonUp(MiddleMouse);
			break;
		}
		case WM_XBUTTONDOWN:
		{
			Mouse_ButtonDown(GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ? ExtraMouse1 : ExtraMouse2);
			break;
		}
		case WM_XBUTTONUP:
		{
			Mouse_ButtonUp(GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ? ExtraMouse1 : ExtraMouse2);
			break;
		}

	 // This, in combination with code in win_func,
	 // will hopefully disable Alt-Tabbing...
		case WM_ACTIVATEAPP:
		{
			if (wParam) {
				bActive = true;
			}
			else {
				// the game stops while it hasn't got the focus, and a run mostly won't have it
				bActive = HudsonRunIsOn;
			}

			IngameKeyboardInput_ClearBuffer();
			return 0;
		}

     // Three below are for safety, to turn off
	 // as much as possible of the more annoying 
	 // functionality of the default Windows 
	 // procedure handler

		case WM_ACTIVATE:
			return 0;

		// Holding a window by its title bar, or a menu on it, keeps the game's thread inside Windows, and no frame can start
		// until it's let go. Nobody's playing a run, and it may be running on someone's desk, so a run's window can't do either.
		case WM_SYSCOMMAND:
			if (HudsonRunIsOn)
			{
				switch (wParam & 0xFFF0)
				{
					case SC_MOVE:
					case SC_SIZE:
					case SC_KEYMENU:
					case SC_MOUSEMENU:
						return 0;

					default:
						break;
				}
			}
			break;

		case WM_CONTEXTMENU:
			if (HudsonRunIsOn)
			{
				return 0;
			}
			break;

		case WM_CREATE:
			break;

		case WM_MOVE:
		{
			// Necessary to stop it crashing in 640x480
			// FullScreen modes on window initialisation
			if (WindowMode == WindowModeSubWindow)
			{
				::GetWindowRect(hWndMain, &NewWindCoord);
				WinLeftX  = NewWindCoord.left;
				WinTopY   = NewWindCoord.top;
				WinRightX = NewWindCoord.right;
				WinBotY   = NewWindCoord.bottom;
			}
			break;
		}

		case WM_SIZE:
		{
			// Necessary to stop it crashing in 640x480
			// FullScreen modes on window initialisation
			if (WindowMode == WindowModeSubWindow)
			{
				::GetWindowRect(hWndMain, &NewWindCoord);
				WinLeftX  = NewWindCoord.left;
				WinTopY   = NewWindCoord.top;
				WinRightX = NewWindCoord.right;
				WinBotY   = NewWindCoord.bottom;
			}
			break;
		}
		case WM_SETCURSOR:
			SetCursor(NULL);
			return TRUE;

		case WM_ERASEBKGND:
			return TRUE;

		case WM_PAINT:
			hdc = ::BeginPaint(hWnd, &ps);
			::EndPaint(hWnd, &ps);
			return TRUE;

#if 0
		/* Patrick 11/6/97: this to detects the end of a cdda track */
		case MM_MCINOTIFY:
			PlatCDDAManagementCallBack(wParam, (LONG)lParam);
			break;
#endif
		case WM_CLOSE:
		{
			bRunning = false;
			return 0; // TODO - check this
		}

		case WM_DESTROY:
		{
			::PostQuitMessage(0);
			break;
		}
	}
	return ::DefWindowProc(hWnd, message, wParam, lParam);
}

bool InitialiseWindowsSystem(HINSTANCE hInstance, int nCmdShow, int WinInitMode)
{
	WNDCLASSEX	wcex;
	memset(&wcex, 0, sizeof(WNDCLASSEX));
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style  = CS_DBLCLKS;
	wcex.lpfnWndProc = WindowProc;
	wcex.cbClsExtra = 0; 
	wcex.cbWndExtra = 0;
	wcex.hInstance  = hInstance;
	wcex.hIcon   = ::LoadIcon(NULL, IDI_APPLICATION);
	wcex.hCursor = ::LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = NULL;
	wcex.lpszMenuName  = NULL;
	wcex.lpszClassName = className;

	hInst = hInstance;

	if (!RegisterClassEx(&wcex))
	{
		Hudson_StopIfTheGameWantsToAsk("Could not register Window");
		::MessageBox(NULL, "Could not register Window", "Error!", MB_ICONEXCLAMATION | MB_OK);
		return false;
	}

	// if "-w" passed to command line
	if (WindowMode == WindowModeSubWindow)
	{
		RECT clientRect;
		clientRect.left = 0;
		clientRect.top  = 0;
		clientRect.right  = CW_USEDEFAULT;
		clientRect.bottom = CW_USEDEFAULT;

		::AdjustWindowRect(&clientRect, ::GetWindowLongPtr(hWndMain, GWL_STYLE), FALSE);

		// A run's window starts where it's going to stay, so that it never covers the main screen, even while the game loads. In a normal game these are left as they are.
		int startLeft = clientRect.left;
		int startTop = clientRect.top;
		int startWidth = clientRect.right - clientRect.left;
		int startHeight = clientRect.bottom - clientRect.top;

		Hudson_PlaceTheWindow(&startLeft, &startTop, &startWidth, &startHeight);

		// A run's window can't be activated, so nobody loses their keyboard to it, and it stays off the taskbar. In a normal game this is the plain 0 that CreateWindow itself passes.
		hWndMain = ::CreateWindowEx(
			Hudson_StaysOutOfTheWay() ? WS_EX_NOACTIVATE : 0,
			className,
			GetWindowTitle(),
			0,
			startLeft,
			startTop,
			startWidth,
			startHeight,
			0,
			NULL,
			hInstance,
			0
		);
	}
	else if (WindowMode == WindowModeFullScreen)
	{
		RECT clientRect;
		clientRect.left = 0;
		clientRect.top  = 0;
		clientRect.right  = ScreenDescriptorBlock.SDB_Width;
		clientRect.bottom = ScreenDescriptorBlock.SDB_Height;

		hWndMain = ::CreateWindowEx(
			WS_EX_TOPMOST,
			className,   // Name of class (registered by RegisterClass call above) 
			GetWindowTitle(), // Name of window 
			WS_POPUP,
			0,
			0,
			clientRect.right,
			clientRect.bottom,
			NULL,
			NULL,
			hInstance,
			0
		);
	}

	if (!hWndMain)
	{
		::UnregisterClass(wcex.lpszClassName, wcex.hInstance);
		Hudson_StopIfTheGameWantsToAsk("Could not create Window");
		::MessageBox(NULL, "Could not create Window", "Error!", MB_ICONEXCLAMATION | MB_OK);
		return false;
	}

	// Set the window up to be displayed 
	// a run doesn't take the focus from whoever's using the machine
	::ShowWindow(hWndMain, Hudson_StaysOutOfTheWay() ? SW_SHOWNOACTIVATE : (HudsonRunIsOn ? SW_SHOW : nCmdShow));
	// Update once (i.e. send WM_PAINT message to the window procedure)
	::UpdateWindow(hWndMain);

	Hudson_BringTheWindowForward();

// Grab ALL mouse messages for our window.
// Note this will only work if the window is
// foreground (as it is... ).  This ensures that
// we will still get MOUSEMOVE etc messages even
// if the mouse is out of the defined window area.

	#if grabmousecapture
	SetCapture(hWndMain);
// Load null cursor shape
	SetCursor(NULL);
	#endif

	return true;
}

void ChangeWindowsSize(uint32_t width, uint32_t height)
{
	RECT testRect;
	RECT newWindowSize;
	HRESULT LastError;

	newWindowSize.top  = 0;
	newWindowSize.left = 0;
	newWindowSize.right  = width;
	newWindowSize.bottom = height;

	if (WindowMode == WindowModeSubWindow)
	{
		if (::AdjustWindowRect(&newWindowSize, ::GetWindowLongPtr(hWndMain, GWL_STYLE), FALSE) == 0)
		{
			LastError = ::HRESULT_FROM_WIN32(::GetLastError());
			LogDxError(LastError, __LINE__, kThisFile);
			return;
		}
	}

	// Sizing a window also makes it the active one and brings it to the front, unless it's told not to. That took the focus on a run's first launch. A run leaves whatever's in use alone.
	UINT const howToSize = SWP_SHOWWINDOW | (Hudson_StaysOutOfTheWay() ? (SWP_NOACTIVATE | SWP_NOZORDER) : 0);

	int left = 0;
	int top = 0;
	int wide = newWindowSize.right - newWindowSize.left;
	int tall = newWindowSize.bottom - newWindowSize.top;

	// A Hudson run places its own window (windowslot=, Hudson_PlaceTheWindow). Otherwise -dev -windowed puts a test copy's window in its own
	// half of the screen, so two copies side by side never overlap (window_slot.h).
	WindowSlot slot;

	if (WindowMode == WindowModeSubWindow && !Hudson_PlaceTheWindow(&left, &top, &wide, &tall) && WindowSlot_Last(&slot))
	{
		left = slot.left;
		top = slot.top;
	}

	if (::SetWindowPos(hWndMain, 0, left, top, wide, tall, howToSize) == 0)
	{
		LastError = ::HRESULT_FROM_WIN32(::GetLastError());
		LogDxError(LastError, __LINE__, kThisFile);
		return;
	}

	if (::GetClientRect(hWndMain, &testRect) == 0)
	{
		LastError = ::HRESULT_FROM_WIN32(::GetLastError());
		LogDxError(LastError, __LINE__, kThisFile);
	}
}

void InitialiseRawInput()
{
#if 1
	#ifndef HID_USAGE_PAGE_GENERIC
		#define HID_USAGE_PAGE_GENERIC			((USHORT) 0x01)
	#endif
	#ifndef HID_USAGE_GENERIC_MOUSE
		#define HID_USAGE_GENERIC_MOUSE			((USHORT) 0x02)
	#endif

	RAWINPUTDEVICE Rid[1];
	Rid[0].usUsagePage = HID_USAGE_PAGE_GENERIC; 
	Rid[0].usUsage = HID_USAGE_GENERIC_MOUSE; 
	Rid[0].dwFlags = RIDEV_INPUTSINK;   
	Rid[0].hwndTarget = hWndMain;

	// This is what reads the mouse even when the window hasn't got the focus. A run doesn't listen, because the mouse belongs to whoever is at the machine
	if (!HudsonRunIsOn)
	{
		::RegisterRawInputDevices(Rid, 1, sizeof(Rid[0]));
	}
#endif
}

// Project specific to go with the initialiser

BOOL ExitWindowsSystem(void)
{
	// Release dedicated mouse capture
	#if grabmousecapture
	ReleaseCapture();
	#endif

	return ::DestroyWindow(hWndMain);
}

#endif //ifdef _WIN32
