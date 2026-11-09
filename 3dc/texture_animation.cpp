// Where an animated texture's corners are on its picture, between two of its frames. Written 2026-11-08 by Melanikus for the AvP port
// (issue #97, reported by Bobblen). See texture_animation.h.
#include "texture_animation.h"

bool TextureAnimation_PlacesFit(const int *texels, long long count)
{
	if (!texels || count < 0)
	{
		return false;
	}

	for (long long i = 0; i < count; i++)
	{
		if (texels[i] < -kTextureAnimationFurthestTexel || texels[i] > kTextureAnimationFurthestTexel)
		{
			return false;
		}
	}

	return true;
}

void TextureAnimation_Corners(const int *firstFrame, const int *secondFrame, int corners, int alpha, bool interpolate, int *out)
{
	// A texel is 65536 in 16.16. A place between two frames is the first frame's, times how much of the way is still to go, plus the
	// second's, times how far it has come: in texels times 16.16, that's 16.16 already, so nothing is divided, and nothing is lost. A
	// place both frames share comes out as that place exactly, at every moment. The two weights add up to 65536 and the places are at
	// most 16383 either way (TextureAnimation_PlacesFit), so the sum is too, and fits a whole number. alpha is kept to 0 to 65536 first,
	// so that holds whatever it's given.
	if (alpha < 0)
	{
		alpha = 0;
	}

	if (alpha > 65536)
	{
		alpha = 65536;
	}

	long long const stillToGo = 65536 - alpha;

	for (int i = 0; i < corners * 2; i++)
	{
		long long place = (long long)firstFrame[i] * 65536;

		if (interpolate)
		{
			place = (long long)firstFrame[i] * stillToGo + (long long)secondFrame[i] * alpha;
		}

		out[i] = (int)place;
	}
}
