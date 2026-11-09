// The console's key: the one under Esc, found by its place on the keyboard, whatever the keyboard's layout. Written 2026-11-09 by Melanikus
// for the AvP port.
//
// WHY. In 1999 the game read the keyboard with DirectInput, which names each key by where it is (a scan code), and the console opened on
// DIK_GRAVE: the key under Esc, left of 1, on every PC keyboard. The port reads keys from Windows' messages instead, which name a key by
// what the layout says it is (a virtual key), and it opened the console on VK_OEM_8 alone. That's the key under Esc only on a UK keyboard.
// A US keyboard calls the same key VK_OEM_3, and the owner's Brazilian ABNT2 does too, so on most players' keyboards the console never
// opened (GIVEALLWEAPONS, and #180's NEWDYNAMICCOLLISIONS switch). This puts back 1999's rule.
//
// HOW. Each key message (WM_KEYDOWN, WM_KEYUP and their WM_SYS... forms) carries the key's scan code in bits 16 to 23 of its lParam, and
// bit 24 says whether it's an "extended" key, one of the keys that share a scan code with another, such as the arrows with the number
// pad. The key under Esc is scan code 0x29, not extended, on every PC keyboard: the same number as DIK_GRAVE. win_proj.cpp asks this of
// each key message, and gives such a key to the console alone, never also as whatever the layout calls it. So the key that's ' or " on
// one layout still works as KEY_APOSTROPHE wherever it is on that layout.
//
// It's game-free, so console_key_test can try it with the messages a real keyboard sends.

#ifndef CONSOLE_KEY_H
#define CONSOLE_KEY_H

#include <stdint.h>

// The key under Esc: DirectInput's DIK_GRAVE, which 1999 read
uint32_t const kConsoleKeyScanCode = 0x29;

// Whether a key message is about the key under Esc. keyData is the message's lParam (its low 32 bits, which is all of it on a 32-bit game).
inline bool ConsoleKey_IsTheKeyUnderEsc(uint32_t keyData)
{
	uint32_t const scanCode = (keyData >> 16) & 0xFF;
	bool const extended = ((keyData >> 24) & 1) != 0;
	return scanCode == kConsoleKeyScanCode && !extended;
}

#endif
