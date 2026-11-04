// Counts how much of a picture flickers from frame to frame. Written 2026-11-04 by Melanikus for the AvP port.
//
// Flicker is hard to describe and hard to catch by eye, so this gives it a number, and a fix can then be proven without anyone having to watch.
//
// Counting the places that changed isn't enough. While the camera moves, everything on the screen changes, and that isn't flicker. But think of one
// place on the screen, with the edge of a shadow going past it. It goes from light to dark once, and it stays dark. Flicker is different: it goes to
// dark and then straight back. So this keeps the last two frames, and counts the places that changed and are now back to what they were two frames
// ago. Ordinary movement hardly ever does that, so this is the number to compare.

#include "flicker_meter.h"

#include <math.h>

FlickerMeter::FlickerMeter(float howMuch) : howMuchCounts(howMuch)
{
}

void FlickerMeter::Forget()
{
	lastFrame.clear();
	frameBefore.clear();
}

FlickerCounts FlickerMeter::Add(const std::vector<float> &samples)
{
	FlickerCounts counts = { (int)samples.size(), 0, 0 };

	// a picture of another size, after the screen's size changed, can't be compared with anything
	bool const canCompare = lastFrame.size() == samples.size();
	bool const canLookBack = canCompare && frameBefore.size() == samples.size();

	if (canCompare)
	{
		for (size_t i = 0; i < samples.size(); i++)
		{
			if (fabsf(samples[i] - lastFrame[i]) > howMuchCounts)
			{
				counts.changed++;

				// back within half the limit, so that a place only just changing twice in the same direction can't pass for it
				if (canLookBack && fabsf(samples[i] - frameBefore[i]) <= howMuchCounts * 0.5f)
				{
					counts.cameBack++;
				}
			}
		}
	}

	frameBefore.swap(lastFrame);
	lastFrame = samples;

	return counts;
}
