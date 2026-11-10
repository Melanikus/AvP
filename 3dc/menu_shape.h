// How the menus are drawn on a screen wider than 4:3. Written 2026-11-10 by Melanikus for the AvP port, at the owner's word on Hudson's
// pictures of three looks: "True shape, starfield full width", then "Yes, make it the default".
//
// The menus are drawn on a grid 640 wide and 480 high, which is 4:3. Until 2026-11-10 that grid was stretched to fill the screen, so on a
// 16:9 screen every letter came out a third wider than it's tall. Now, on a screen wider than 4:3, the grid keeps one scale both ways, in
// the 4:3 part in the middle of the screen, so letters and pictures keep their true shape; and the starfield behind the menus alone is
// still drawn across the whole width, as it always was. A 4:3 screen, or a narrower one, is drawn exactly as before.
//
// d3d_render.cpp's WPos2DC turns a place on the grid into a place on the screen with this, and menus.cpp's DrawMainMenusBackdrop says when
// it's the starfield being drawn. It's here, with no game code, so tests/menu_shape_test.cpp can try it with plain numbers.

#ifndef MENU_SHAPE_H
#define MENU_SHAPE_H

// The share of the screen's width the menus' grid takes: 1 for all of it, or less for the middle part only. wide and high are the screen's
// size in pixels. backdrop is true while the starfield is drawn, which always takes the whole width. The sizes are a screen's, so their
// products can't come near an int's limit; anything not a size at all gets the whole width, as before.
inline float MenuShape_WidthShare(int wide, int high, bool backdrop)
{
	if (backdrop || wide <= 0 || high <= 0 || wide > 65535 || high > 65535 || wide * 3 <= high * 4)
	{
		return 1.0f;
	}

	return (float)(high * 4) / (float)(wide * 3);
}

#endif
