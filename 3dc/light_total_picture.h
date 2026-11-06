// Which picture the light total can be kept in. Written 2026-11-06 by Melanikus for the AvP port.
//
// The total holds two numbers a pixel, and each light adds itself to what's there, so the picture has to hold numbers over 1 and the card has to
// be able to add on it. Not every card can. A card that can't on either kind gets no total, and pixel_lights.cpp says so, and the game goes on
// without the surface pictures. It's on its own so that a test can try every answer a card might give, with no card.
#ifndef LIGHT_TOTAL_PICTURE_H
#define LIGHT_TOTAL_PICTURE_H

enum LightTotalPicture
{
	kLightTotalTwoNumbers,    // the two numbers the total needs, and no more
	kLightTotalFourNumbers,   // as good, at twice the memory, with two numbers nothing reads
	kLightTotalNoPicture
};

inline LightTotalPicture LightTotal_BestPicture(bool canAddToTwoNumbers, bool canAddToFourNumbers)
{
	if (canAddToTwoNumbers)
	{
		return kLightTotalTwoNumbers;
	}

	return canAddToFourNumbers ? kLightTotalFourNumbers : kLightTotalNoPicture;
}

#endif
