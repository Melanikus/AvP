// How a lamp's light looks per pixel: see pixel_look.h. Written 2026-11-06 by Melanikus for the AvP port.
#include "pixel_look.h"

#include "shoulder_lamp.h"

static double const kAsBuilt[kPixelLookNumbers] = { kPixelGlowAsBuilt, kPixelCurveAsBuilt, kPixelLevelAsBuilt };
static double const kFewest[kPixelLookNumbers] = { kPixelGlowFewest, kPixelCurveFewest, kPixelLevelFewest };
static double const kMost[kPixelLookNumbers] = { kPixelGlowMost, kPixelCurveMost, kPixelLevelMost };

static double now[kPixelLookNumbers] = { kPixelGlowAsBuilt, kPixelCurveAsBuilt, kPixelLevelAsBuilt };

double PixelLook_Now(PixelLookNumber which)
{
	return now[which];
}

bool PixelLook_FromWords(PixelLookNumber which, const char *words, unsigned int length)
{
	double number = 0.0;

	if (!ReadPlainNumber(words, length, &number) || number < kFewest[which] || number > kMost[which])
	{
		return false;
	}

	now[which] = number;
	return true;
}

bool PixelLook_PutBack()
{
	bool wasChanged = false;

	for (int i = 0; i < kPixelLookNumbers; i++)
	{
		wasChanged = wasChanged || now[i] != kAsBuilt[i];
		now[i] = kAsBuilt[i];
	}

	return wasChanged;
}

void PixelLook_Numbers(float *four)
{
	for (int i = 0; i < kPixelLookNumbers; i++)
	{
		four[i] = (float)now[i];
	}

	four[3] = 0.0f;
}
