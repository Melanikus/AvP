// How big the HUD is drawn: the health, ammo, motion tracker and the other readings on the screen during a level. Written 2026-11-08 by
// Melanikus for the AvP port (card HUD-SCALE-1). Reported by Bobblen; follows his avp-forever (commit a53f482), rewritten.
//
// WHAT CHANGED: in 1999 the HUD grew with the screen's width. Everything was drawn at width / 640 times its size on a 640 by 480 screen,
// so a wide screen made it huge: 3 times at 1920 by 1080, and over 5 times at 3440 by 1440, where the Predator's column of symbols ran
// off the bottom of the screen. Now it grows with the screen's height, so it takes the same share of the screen's height on every shape
// of screen, and the player can make it smaller or bigger. Each piece is still placed from its own corner or edge, as before.
//
// THE USUAL SIZE: the size the HUD has always had on a 1280 by 1024 screen, which the user chose to keep. There, width / 640 is 2, and
// height / 480 is 2.13, so the usual size is 2 / 2.13 = 0.9375 times height / 480. On a 1024 by 768 screen it's 1.5, where it was 1.6.
//
// THE PLAYER'S SIZE: the "HUD size" row on the Rendering page of the menus, from 50% to 150% of the usual size, in steps of 10. It's kept
// in AliensVsPredator.cfg as HUDSize, under [Video]. Anyone can edit that file, so what it says is read strictly (AvP Tao 3): see
// HUDSize_FromSavedWord.
//
// THE NUMBERS are the game's own fixed-point numbers, where 65536 means 1 (ONE_FIXED). A scale of 131072 draws everything twice its size
// on a 640 by 480 screen.
//
// This file needs nothing from the rest of the game, so that tests/hud_scale_test.cpp can try every case. view_settings.cpp joins the
// player's size to the menus and the settings file, and HUD_FollowTheScreen, in d3d_hud.cpp, gives the scale to everything that draws.

#ifndef HUD_SCALE_H
#define HUD_SCALE_H

// The player's HUD size, as a percentage of the usual size
static const int kHUDSizeSmallest = 50;
static const int kHUDSizeLargest = 150;
static const int kHUDSizeStep = 10;
static const int kHUDSizeUsual = 100;

// The menu's slider has a place for each size, counted from 0 for the smallest: 0 to 10, with the usual size at 5, in the middle
static const int kHUDSizeSliderPlaces = (kHUDSizeLargest - kHUDSizeSmallest) / kHUDSizeStep;
static const int kHUDSizeUsualPlace = (kHUDSizeUsual - kHUDSizeSmallest) / kHUDSizeStep;

// The usual size, 0.9375 times the screen's height / 480, in fixed point. It's 15/16 exactly, so the usual HUD on a 1280 by 1024 screen is
// exactly the old one: 1024 * 61440 / 480 = 131072, which is 2, the old 1280 / 640.
static const int kHUDUsualSize = 61440;

// The tallest screen the scale is worked out for. A taller one is drawn as if it were this tall. It keeps the sum in HUDScale_ForScreen far
// inside what a whole number can hold, and no screen is anywhere near it.
static const int kHUDTallestScreen = 16384;

// The Predator's column of health symbols, and its column of field charge: 6 symbols, each 51 rows tall at a scale of 1. hud.cpp draws
// them with these, and the test checks that a column fits on the screen at the largest HUD size.
static const int kPredatorSymbolRows = 51;
static const int kPredatorColumnSymbols = 6;

// The alien's health bar is a long strip at the bottom of the screen, its thickness at a scale of 1: 640 / 32, which is what it was on a
// 640 by 480 screen
static const int kAlienBarThickness = 20;

// How big the HUD is drawn on a screen this many rows tall, with the player's size (a percentage, kHUDSizeSmallest to kHUDSizeLargest).
// Anything outside those is brought inside first.
int HUDScale_ForScreen(int screenHeight, int size);

// The size at a place on the menu's slider, and the place for a size. A place outside the slider, or a size outside the sizes, is brought
// to the nearest one there is.
int HUDSize_AtSliderPlace(int place);
int HUDSize_SliderPlace(int size);

// Reads the settings file's word for the HUD size. A word of 1 to 3 digits and nothing else is a number. A number outside 50 to 150 is
// brought inside, and one between two steps goes to the nearest, a half going up. Anything else, an empty word included, is the usual
// size. *size is always the size to use. Gives back true only if the word was already exactly that size, written plainly, as the menu
// writes it. Otherwise the caller says so in the log, without repeating the word, and writes the size back.
bool HUDSize_FromSavedWord(const char *word, int *size);

// Where the alien's health bar goes on the screen. Its full length is the width of a 4:3 screen of this height, and never wider than the
// screen itself, centred. That's how it looked on the 4:3 screens it was drawn for. Its full thickness follows the scale, so the player's HUD
// size makes it thicker or thinner, but never longer. As health goes down, the bar gets shorter and thinner towards its own middle, as it
// always has. A health of 100 or more is the whole bar. On a 1280 by 1024 screen at the usual size, it's exactly the old bar at every
// health, to the pixel.
struct HUDBar
{
	int left;        // the bar's left edge, in pixels from the screen's left
	int top;         // its top edge, in pixels from the screen's top
	int length;      // how long it is, across the screen
	int thickness;   // how thick it is, down the screen
};

HUDBar HUDAlienBar_Place(int screenWidth, int screenHeight, int scale, unsigned int health);

#endif
