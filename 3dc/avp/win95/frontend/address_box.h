// The join page's address box, as it's drawn on the screen. Written 2026-11-07 by Melanikus for the AvP port.
//
// A player joining a game by its address types the host's address into this box, or picks one they saved. Players stream and share
// pictures of their screens, and an address there tells anyone watching where the host is (the user's ruling: no IPs). So the box
// shows a dot for each character, unless the player holds down the Tab key, which shows the address for as long as it's held, so a
// typing mistake can still be seen. The page's help line says so. Only what's drawn changes: the address itself, which is what the game
// joins, is just as it was typed. It's on for everyone, with nothing to turn it off, because a streamer shouldn't have to remember.
//
// tests/address_box_test.cpp tries it. menus.cpp draws the box and its help line with these.
#ifndef ADDRESS_BOX_H
#define ADDRESS_BOX_H

#include <stddef.h>

// The help line's own words, after the page's usual help for the box
static const char *const kAddressBoxHelp = "The address shows as dots, so it stays off streams and pictures: hold Tab to see it.";

// What the box shows: the address itself while Tab is held, and otherwise a dot for each character. shown has room for size
// characters, and always ends with a 0. No address shows as nothing.
inline void AddressBox_TextToDraw(const char *address, bool tabIsHeld, char *shown, size_t size)
{
	if (size == 0)
	{
		return;
	}

	size_t i = 0;

	while (address && address[i] && i + 1 < size)
	{
		shown[i] = tabIsHeld ? address[i] : '.';
		i++;
	}

	shown[i] = '\0';
}

#endif
