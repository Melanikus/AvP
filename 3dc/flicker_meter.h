// Counts how much of a picture flickers from frame to frame. Written 2026-11-04 by Melanikus for the AvP port.
// See flicker_meter.cpp for how it works. This file knows nothing about the game, so a test can check it on its own.

#ifndef FLICKER_METER_H
#define FLICKER_METER_H

#include <vector>

// How far ahead of the camera a place is: under 5 metres, 5 to 20, 20 to 50 and further, and a band of its own for places whose distance isn't known. A small
// flickering patch far away is lost in a number for the whole screen, and depth is much coarser far away, so that's where to look. It's measured straight ahead,
// not in a line to the place, because straight ahead is what sets how coarse depth is.
enum { kFlickerBands = 5 };

// Which band a distance is in. The game's units are millimetres. The distance picture starts each frame filled with 1, and nothing can be drawn nearer than 64.
// So 1 or less means the picture holds nothing for that place: there's no surface there, or there is one and the picture left it out. It could be near, so it
// can't be counted with the far places, where it would look like far flicker.
inline int FlickerBandFor(float distance)
{
	return (distance <= 1.0f) ? 4 : (distance >= 50000.0f) ? 3 : (distance >= 20000.0f) ? 2 : (distance >= 5000.0f) ? 1 : 0;
}

struct FlickerCounts
{
	int samples;     // how many places in the picture were compared
	int changed;     // how many differ from the last frame
	int cameBack;    // how many of those are back to what they were two frames ago
	int cameBackInBand[kFlickerBands];   // the same, by distance. All zero if no distances were given
};

class FlickerMeter
{
public:
	// howMuchCounts is the smallest difference that counts as a change. Anything less is only rounding.
	explicit FlickerMeter(float howMuchCounts);

	// Takes this frame's picture, and says how it compares with the last two. bands says which band each place is in, and if it isn't the same length
	// as the picture, nothing is counted by distance.
	FlickerCounts Add(const std::vector<float> &samples, const std::vector<unsigned char> &bands);

	// Forgets the last two pictures. The next two frames have too little to compare with.
	void Forget();

private:
	float howMuchCounts;
	std::vector<float> lastFrame;
	std::vector<float> frameBefore;
};

#endif
