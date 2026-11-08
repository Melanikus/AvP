#pragma once

// How a lamp's light looks per pixel, for this run of the game. Written 2026-11-06 by Melanikus for the AvP port.
//
// A room lit per pixel can look paler than the same room in Gouraud. These three numbers change how a lamp's light shows on the surface it reaches, in shaders/lightOne.psh. They don't change how much
// light reaches that surface, so they don't move a shadow. A flare and the marine's shoulder lamp have looks of their own, and these leave them alone.
//
// The console words PIXELGLOW, PIXELCURVE and PIXELLEVEL change them while the game runs, and the on-screen tag shows them, so a picture of the screen says what they were. Nothing keeps them for the
// next run. They start at the look the game has always had, so nothing changes until someone types a word. A brighter room than the other players have would be cheating, so the words need -dev, they
// refuse in a network game, and a network game puts the numbers back.

enum PixelLookNumber
{
	kPixelLookGlow,    // how much of a lamp's light goes on top with no texture, as a flat tint on everything it reaches. Lower makes dark surfaces less misty, and 0 is none
	kPixelLookCurve,   // how deep the dim tones go. Above 1 dim light gets darker, while the brightest spots stay where they are
	kPixelLookLevel,   // how strong all of it is. Below 1 everything a lamp adds gets darker, its brightest spots too
	kPixelLookNumbers
};

double const kPixelGlowAsBuilt = 0.25;
double const kPixelGlowFewest = 0.0;
double const kPixelGlowMost = 1.0;

double const kPixelCurveAsBuilt = 1.0;
double const kPixelCurveFewest = 0.25;
double const kPixelCurveMost = 4.0;

double const kPixelLevelAsBuilt = 1.0;
double const kPixelLevelFewest = 0.0;
double const kPixelLevelMost = 2.0;

double PixelLook_Now(PixelLookNumber which);

// A number typed by a person, for this one. It gives back false and changes nothing if it isn't a plain number inside this one's limits.
bool PixelLook_FromWords(PixelLookNumber which, const char *words, unsigned int length);

// All three back as they were built. It gives back true if that changed any of them.
bool PixelLook_PutBack();

// What the shader gets: the three in order, and a spare.
void PixelLook_Numbers(float *four);
