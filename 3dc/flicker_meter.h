// Counts how much of a picture flickers from frame to frame. Written 2026-11-04 by Melanikus for the AvP port.
// See flicker_meter.cpp for how it works. This file knows nothing about the game, so a test can check it on its own.

#ifndef FLICKER_METER_H
#define FLICKER_METER_H

#include <vector>

struct FlickerCounts
{
	int samples;     // how many places in the picture were compared
	int changed;     // how many differ from the last frame
	int cameBack;    // how many of those are back to what they were two frames ago
};

class FlickerMeter
{
public:
	// howMuchCounts is the smallest difference that counts as a change. Anything less is only rounding.
	explicit FlickerMeter(float howMuchCounts);

	// Takes this frame's picture, and says how it compares with the last two.
	FlickerCounts Add(const std::vector<float> &samples);

	// Forgets the last two pictures. The next two frames have too little to compare with.
	void Forget();

private:
	float howMuchCounts;
	std::vector<float> lastFrame;
	std::vector<float> frameBefore;
};

#endif
