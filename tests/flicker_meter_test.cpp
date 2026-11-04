// The test for 3dc/flicker_meter.cpp, whose number decides which fix for flickering shadows is the right one.
// Written 2026-11-04 by Melanikus for the AvP port.
//
// tests/build.ps1 compiles this with the real flicker_meter.cpp, and the program gives back a non-zero result if any check failed. It uses no files,
// no graphics card and none of the game.

#include <stdio.h>
#include <vector>

#include "flicker_meter.h"

static int checksFailed = 0;

static void Check(bool good, const char *what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what);

	if (!good)
	{
		checksFailed++;
	}
}

static std::vector<float> Picture(float a, float b, float c)
{
	std::vector<float> samples;
	samples.push_back(a);
	samples.push_back(b);
	samples.push_back(c);
	return samples;
}

static const std::vector<unsigned char> noBands;

int main()
{
	{
		FlickerMeter meter(0.05f);
		FlickerCounts const first = meter.Add(Picture(0.0f, 0.0f, 0.0f), noBands);
		Check(first.samples == 3 && first.changed == 0 && first.cameBack == 0, "the first picture has nothing to compare with");

		FlickerCounts const same = meter.Add(Picture(0.0f, 0.0f, 0.0f), noBands);
		Check(same.changed == 0 && same.cameBack == 0, "the same picture again: nothing changed");

		FlickerCounts const small = meter.Add(Picture(0.04f, 0.0f, 0.0f), noBands);
		Check(small.changed == 0, "a change under the limit is only rounding");
	}

	{
		FlickerMeter meter(0.05f);
		meter.Add(Picture(0.0f, 0.0f, 0.0f), noBands);
		meter.Add(Picture(0.0f, 0.0f, 0.0f), noBands);

		FlickerCounts const edge = meter.Add(Picture(1.0f, 0.0f, 0.0f), noBands);
		Check(edge.changed == 1 && edge.cameBack == 0, "a shadow's edge passing: one place changes, and it hasn't come back");

		FlickerCounts const stays = meter.Add(Picture(1.0f, 0.0f, 0.0f), noBands);
		Check(stays.changed == 0 && stays.cameBack == 0, "and it stays changed: nothing more to count");
	}

	{
		FlickerMeter meter(0.05f);
		meter.Add(Picture(0.0f, 0.0f, 0.0f), noBands);
		meter.Add(Picture(1.0f, 1.0f, 0.0f), noBands);

		FlickerCounts const back = meter.Add(Picture(0.0f, 1.0f, 0.0f), noBands);
		Check(back.changed == 1 && back.cameBack == 1, "dark and straight back: that's flicker, and only that place counts");
	}

	{
		FlickerMeter meter(0.05f);
		meter.Add(Picture(0.0f, 0.0f, 0.0f), noBands);
		meter.Add(Picture(0.5f, 0.0f, 0.0f), noBands);

		FlickerCounts const onwards = meter.Add(Picture(1.0f, 0.0f, 0.0f), noBands);
		Check(onwards.changed == 1 && onwards.cameBack == 0, "two steps the same way is movement, not flicker");
	}

	{
		FlickerMeter meter(0.05f);
		meter.Add(Picture(0.0f, 0.0f, 0.0f), noBands);
		meter.Add(Picture(1.0f, 0.0f, 0.0f), noBands);

		FlickerCounts const nearly = meter.Add(Picture(0.04f, 0.0f, 0.0f), noBands);
		Check(nearly.changed == 1 && nearly.cameBack == 0, "not quite back, by more than half the limit, doesn't count");
	}

	{
		FlickerMeter meter(0.05f);
		meter.Add(Picture(0.0f, 0.0f, 0.0f), noBands);
		meter.Add(Picture(1.0f, 0.0f, 0.0f), noBands);

		std::vector<float> larger(5, 0.0f);
		FlickerCounts const resized = meter.Add(larger, noBands);
		Check(resized.samples == 5 && resized.changed == 0 && resized.cameBack == 0, "a picture of another size can't be compared, and doesn't read past either list");

		FlickerCounts const next = meter.Add(larger, noBands);
		Check(next.changed == 0 && next.cameBack == 0, "the next one compares again");
	}

	{
		FlickerMeter meter(0.05f);
		meter.Add(Picture(0.0f, 0.0f, 0.0f), noBands);
		meter.Add(Picture(1.0f, 0.0f, 0.0f), noBands);
		meter.Forget();

		FlickerCounts const fresh = meter.Add(Picture(0.0f, 0.0f, 0.0f), noBands);
		Check(fresh.changed == 0 && fresh.cameBack == 0, "after forgetting, there's nothing to compare with");
	}

	{
		FlickerMeter meter(0.05f);
		std::vector<float> nothing;
		FlickerCounts const empty = meter.Add(nothing, noBands);
		Check(empty.samples == 0 && empty.changed == 0, "an empty picture is fine");
	}

	{
		std::vector<unsigned char> bands;
		bands.push_back(0);
		bands.push_back(3);
		bands.push_back(3);

		FlickerMeter meter(0.05f);
		meter.Add(Picture(0.0f, 0.0f, 0.0f), bands);
		meter.Add(Picture(1.0f, 1.0f, 1.0f), bands);

		FlickerCounts const back = meter.Add(Picture(0.0f, 0.0f, 1.0f), bands);
		Check(back.cameBack == 2 && back.cameBackInBand[0] == 1 && back.cameBackInBand[3] == 1 && back.cameBackInBand[1] == 0, "flicker is counted in the band of the place it's in");

		std::vector<unsigned char> tooShort(1, 0);
		FlickerMeter other(0.05f);
		other.Add(Picture(0.0f, 0.0f, 0.0f), tooShort);
		other.Add(Picture(1.0f, 0.0f, 0.0f), tooShort);

		FlickerCounts const unknown = other.Add(Picture(0.0f, 0.0f, 0.0f), tooShort);
		Check(unknown.cameBack == 1 && unknown.cameBackInBand[0] == 0, "bands of the wrong length are ignored, and aren't read past their end");

		std::vector<unsigned char> wild(3, 200);
		FlickerMeter third(0.05f);
		third.Add(Picture(0.0f, 0.0f, 0.0f), wild);
		third.Add(Picture(1.0f, 0.0f, 0.0f), wild);

		FlickerCounts const outside = third.Add(Picture(0.0f, 0.0f, 0.0f), wild);
		Check(outside.cameBack == 1 && outside.cameBackInBand[3] == 0, "a band that doesn't exist counts nowhere");

		Check(FlickerBandFor(4999.0f) == 0 && FlickerBandFor(5000.0f) == 1 && FlickerBandFor(20000.0f) == 2 && FlickerBandFor(50000.0f) == 3, "the bands start where they say");
		Check(FlickerBandFor(1.0f) == 3 && FlickerBandFor(0.0f) == 3, "no surface counts as the furthest");
	}

	if (checksFailed)
	{
		printf("FAILED: %d checks\n", checksFailed);
		return 1;
	}

	printf("all flicker meter checks passed\n");
	return 0;
}
