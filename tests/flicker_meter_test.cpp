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

int main()
{
	{
		FlickerMeter meter(0.05f);
		FlickerCounts const first = meter.Add(Picture(0.0f, 0.0f, 0.0f));
		Check(first.samples == 3 && first.changed == 0 && first.cameBack == 0, "the first picture has nothing to compare with");

		FlickerCounts const same = meter.Add(Picture(0.0f, 0.0f, 0.0f));
		Check(same.changed == 0 && same.cameBack == 0, "the same picture again: nothing changed");

		FlickerCounts const small = meter.Add(Picture(0.04f, 0.0f, 0.0f));
		Check(small.changed == 0, "a change under the limit is only rounding");
	}

	{
		FlickerMeter meter(0.05f);
		meter.Add(Picture(0.0f, 0.0f, 0.0f));
		meter.Add(Picture(0.0f, 0.0f, 0.0f));

		FlickerCounts const edge = meter.Add(Picture(1.0f, 0.0f, 0.0f));
		Check(edge.changed == 1 && edge.cameBack == 0, "a shadow's edge passing: one place changes, and it hasn't come back");

		FlickerCounts const stays = meter.Add(Picture(1.0f, 0.0f, 0.0f));
		Check(stays.changed == 0 && stays.cameBack == 0, "and it stays changed: nothing more to count");
	}

	{
		FlickerMeter meter(0.05f);
		meter.Add(Picture(0.0f, 0.0f, 0.0f));
		meter.Add(Picture(1.0f, 1.0f, 0.0f));

		FlickerCounts const back = meter.Add(Picture(0.0f, 1.0f, 0.0f));
		Check(back.changed == 1 && back.cameBack == 1, "dark and straight back: that's flicker, and only that place counts");
	}

	{
		FlickerMeter meter(0.05f);
		meter.Add(Picture(0.0f, 0.0f, 0.0f));
		meter.Add(Picture(0.5f, 0.0f, 0.0f));

		FlickerCounts const onwards = meter.Add(Picture(1.0f, 0.0f, 0.0f));
		Check(onwards.changed == 1 && onwards.cameBack == 0, "two steps the same way is movement, not flicker");
	}

	{
		FlickerMeter meter(0.05f);
		meter.Add(Picture(0.0f, 0.0f, 0.0f));
		meter.Add(Picture(1.0f, 0.0f, 0.0f));

		FlickerCounts const nearly = meter.Add(Picture(0.04f, 0.0f, 0.0f));
		Check(nearly.changed == 1 && nearly.cameBack == 0, "not quite back, by more than half the limit, doesn't count");
	}

	{
		FlickerMeter meter(0.05f);
		meter.Add(Picture(0.0f, 0.0f, 0.0f));
		meter.Add(Picture(1.0f, 0.0f, 0.0f));

		std::vector<float> larger(5, 0.0f);
		FlickerCounts const resized = meter.Add(larger);
		Check(resized.samples == 5 && resized.changed == 0 && resized.cameBack == 0, "a picture of another size can't be compared, and doesn't read past either list");

		FlickerCounts const next = meter.Add(larger);
		Check(next.changed == 0 && next.cameBack == 0, "the next one compares again");
	}

	{
		FlickerMeter meter(0.05f);
		meter.Add(Picture(0.0f, 0.0f, 0.0f));
		meter.Add(Picture(1.0f, 0.0f, 0.0f));
		meter.Forget();

		FlickerCounts const fresh = meter.Add(Picture(0.0f, 0.0f, 0.0f));
		Check(fresh.changed == 0 && fresh.cameBack == 0, "after forgetting, there's nothing to compare with");
	}

	{
		FlickerMeter meter(0.05f);
		std::vector<float> nothing;
		FlickerCounts const empty = meter.Add(nothing);
		Check(empty.samples == 0 && empty.changed == 0, "an empty picture is fine");
	}

	if (checksFailed)
	{
		printf("FAILED: %d checks\n", checksFailed);
		return 1;
	}

	printf("all flicker meter checks passed\n");
	return 0;
}
