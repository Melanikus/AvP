// The picture a scope draws over the screen, shared by the sniper rifle and the revolver. Written 2026-11-06 by Melanikus for the AvP port.
// The game takes this from MelPyth M5 by Mupp. Only how it behaves was read from that mod, and this is written new.
//
// The sums for where the picture goes have no part of the game in them, so that tests/revolver_test.cpp can run them.

#pragma once

// Draws the scope's picture over the whole screen. Call it from a gun's DrawOverHud hook, only while its scope is up. If the picture's file isn't there, this draws
// nothing, and the player sees the zoomed view with no ring round it.
void WeaponScope_Draw(void);

// Where things go on a screen of this size. Everything is in the game's screen units.
struct WeaponScopeLayout
{
	int pictureX;
	int pictureY;
	int pictureWidth;
	int pictureHeight;  // the whole picture, including the part that hangs below the view: see below
	int viewWidth;      // the part of the screen the ring's view covers
	int viewHeight;
};

// The picture is square, but only its top three quarters are the view: 96 rows of 128, in the shape of an old 4:3 screen. The rows below are all black. So the picture is
// drawn a third taller than the view.
//
// The cross is one pixel wide, so it can't be exactly in the middle of a picture with an even size. It's in column 64 and row 47, counting from 0, and the middle of those is
// 64.5 across and 47.5 down: 129/256 of the way across the view, and 95/192 of the way down. The picture is placed by that, so that the cross lands on the middle of the
// screen, which is where the shot goes. Measured at 1280 by 720, placing it by its edges left the cross 4 pixels off.
//
// To keep the ring round on a screen of any shape, the view is as big as fits with those proportions, in the middle of the screen. What's left is black bars.
inline WeaponScopeLayout WeaponScope_LayoutFor(int screenWidth, int screenHeight)
{
	WeaponScopeLayout layout;

	if (screenWidth * 3 >= screenHeight * 4)
	{
		layout.viewHeight = screenHeight;
		layout.viewWidth = (screenHeight * 4) / 3;
	}
	else
	{
		layout.viewWidth = screenWidth;
		layout.viewHeight = (screenWidth * 3) / 4;
	}

	layout.pictureX = screenWidth / 2 - (layout.viewWidth * 129) / 256;
	layout.pictureY = screenHeight / 2 - (layout.viewHeight * 95) / 192;
	layout.pictureWidth = layout.viewWidth;
	layout.pictureHeight = (layout.viewHeight * 4) / 3;

	return layout;
}
