// The test for 3dc/shadow_volumes.cpp, the bookkeeping for shadow prisms kept between frames.
// Written 2026-11-04 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// A wrong answer here shows up as the wrong shadow, or as a draw past the end of the graphics card's list. So this checks that what's kept is only
// found by a key that matches in every part, that nothing is ever handed out past the end of the list, that a full store stays safe and empties
// only when it should, and that a list of runs never holds more than it can.

#include "shadow_volumes.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

static void Check(bool good, const char *what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what);

	if (!good)
	{
		failures++;
	}
}

static ShadowVolumeKey MakeKey(int number)
{
	ShadowVolumeKey key;
	memset(&key, 0, sizeof(key));
	key.castFrom[0] = 100.0f;
	key.lightPlace[0] = 100.0f;
	key.range = 5000.0f;
	key.lightKind = 1.0f;
	key.listNumber = (uint32_t)number;
	return key;
}

static bool IsFound(const ShadowVolumeKey &key)
{
	ShadowVolumeRun run;
	return ShadowVolumes_Find(key, &run);
}

// Keeps some corners the way the game does, and says where they went.
static int Keep(const ShadowVolumeKey &key, int corners)
{
	int const place = ShadowVolumes_PlaceFor(corners);

	if (place >= 0)
	{
		ShadowVolumes_Keep(key, corners);
	}

	return place;
}

static void CheckFindingThings()
{
	ShadowVolumes_Empty();

	ShadowVolumeKey const first = MakeKey(1), second = MakeKey(2);
	ShadowVolumeRun run = { 0, 0 };

	Check(!IsFound(first), "nothing is found in an empty store");
	Check(Keep(first, 36) == 0, "the first thing kept goes at the start");
	Check(Keep(second, 24) == 36, "the next goes straight after it");

	Check(ShadowVolumes_Find(first, &run) && run.first == 0 && run.corners == 36, "the first is found where it was put");
	Check(ShadowVolumes_Find(second, &run) && run.first == 36 && run.corners == 24, "the second is found where it was put");

	ShadowVolumeKey const nothing = MakeKey(3);
	Check(Keep(nothing, 0) == 60, "a light with no shadows here is kept too, so it isn't worked out again");
	run.corners = 99;
	Check(ShadowVolumes_Find(nothing, &run) && run.corners == 0, "and it's found, with no corners");
	Check(Keep(MakeKey(4), 3) == 60, "and it took up no room");
}

static void CheckEveryPartOfTheKeyCounts()
{
	ShadowVolumes_Empty();

	ShadowVolumeKey const key = MakeKey(1);
	Keep(key, 36);

	for (int i = 0; i < 3; i++)
	{
		ShadowVolumeKey changed = key;
		changed.castFrom[i] += 1.0f;
		Check(!IsFound(changed), "a different place to cast from is a different shadow");

		changed = key;
		changed.lightPlace[i] += 1.0f;
		Check(!IsFound(changed), "a light in a different place is a different shadow");

		changed = key;
		changed.place[i] += 1;
		Check(!IsFound(changed), "a shape in a different place is a different shadow");
	}

	ShadowVolumeKey changed = key;
	changed.range += 1.0f;
	Check(!IsFound(changed), "a different range is a different shadow");

	changed = key;
	changed.lightKind = 2.0f;
	Check(!IsFound(changed), "a different kind of light is a different shadow");

	changed = key;
	changed.listNumber = 2;
	Check(!IsFound(changed), "a different shape is a different shadow");

	Check(IsFound(key), "and the key itself is still found");
}

// Two keys usually land in different places in the table, and then they never get compared at all. With this many, plenty of them share a place, so
// a key that only differs in its very last number has to be told apart by the comparison. If it isn't, one is handed the other's corners.
static void CheckKeysThatShareAPlaceInTheTable()
{
	ShadowVolumes_Empty();

	const int kKeys = 20000;

	for (int i = 0; i < kKeys; i++)
	{
		ShadowVolumeKey key = MakeKey(1);
		key.place[2] = i;
		Keep(key, 3);
	}

	bool allRight = true;

	for (int i = 0; i < kKeys; i++)
	{
		ShadowVolumeKey key = MakeKey(1);
		key.place[2] = i;

		ShadowVolumeRun run = { -1, -1 };
		allRight = allRight && ShadowVolumes_Find(key, &run) && run.first == i * 3;
	}

	Check(allRight, "keys that differ only in their last number each find their own corners");
}

static void CheckTheListCantOverflow()
{
	ShadowVolumes_Empty();

	ShadowVolumeKey const key = MakeKey(1);

	Check(ShadowVolumes_PlaceFor(-3) == -1, "a negative count is refused");
	Check(ShadowVolumes_PlaceFor(kShadowVolumeMostCorners + 1) == -1, "more corners than the list holds are refused");

	ShadowVolumes_Empty();
	Check(Keep(key, kShadowVolumeMostCorners - 3) == 0, "nearly the whole list can be used");
	Check(ShadowVolumes_PlaceFor(6) == -1, "corners that would run past the end are refused");
	Check(Keep(MakeKey(2), 3) == kShadowVolumeMostCorners - 3, "corners that exactly reach the end fit");
	Check(ShadowVolumes_PlaceFor(3) == -1, "a full list refuses more");

	ShadowVolumes_Keep(MakeKey(3), 3);
	Check(!IsFound(MakeKey(3)), "keeping without room is ignored, even if something forgot to ask");

	ShadowVolumes_Keep(MakeKey(4), -3);
	Check(!IsFound(MakeKey(4)), "keeping a negative count is ignored");
}

static void CheckEmptyingWaits()
{
	ShadowVolumes_Empty();

	ShadowVolumeKey const key = MakeKey(1);
	ShadowVolumes_StartFrame(0, 0);
	Keep(key, kShadowVolumeMostCorners);

	for (int frame = 0; frame < kShadowVolumeFramesBetweenEmptying * 3; frame++)
	{
		ShadowVolumes_StartFrame(0, 0);
	}

	Check(IsFound(key), "a store that was never refused anything isn't emptied, however long it runs");
	Check(ShadowVolumes_PlaceFor(3) == -1, "this one is refused");

	ShadowVolumeRun const run = { 0, 3 };
	ShadowVolumes_AddRun(run);
	Check(IsFound(key) && ShadowVolumes_NumRuns() == 1, "what's kept stays usable for the rest of the frame, so no list of runs points at nothing");

	ShadowVolumes_StartFrame(0, 0);
	Check(!IsFound(key), "a full store that has waited long enough is emptied at the start of a frame");
	Check(ShadowVolumes_NumRuns() == 0, "and any list of runs goes with it");
	Check(ShadowVolumes_PlaceFor(3) == 0, "and starts from the beginning");

	Keep(key, kShadowVolumeMostCorners);
	Check(ShadowVolumes_PlaceFor(3) == -1, "filled again straight away");

	for (int frame = 0; frame < kShadowVolumeFramesBetweenEmptying - 1; frame++)
	{
		ShadowVolumes_StartFrame(0, 0);
	}

	Check(IsFound(key), "but a store that filled up again isn't emptied until it has waited");

	ShadowVolumes_StartFrame(0, 0);
	Check(IsFound(key), "right up to the last frame of the wait");

	ShadowVolumes_StartFrame(0, 0);
	Check(!IsFound(key), "and then it is");
}

static void CheckRunsDontOutliveTheirFrame()
{
	ShadowVolumes_Empty();

	ShadowVolumeRun const run = { 0, 3 };

	ShadowVolumes_AddRun(run);
	ShadowVolumes_StartFrame(0, 0);
	Check(ShadowVolumes_NumRuns() == 0, "a new frame starts with no runs, even when nothing was emptied");

	ShadowVolumes_AddRun(run);
	ShadowVolumes_Empty();
	Check(ShadowVolumes_NumRuns() == 0, "emptying the store leaves no runs, as when the card's list is lost");
}

static void CheckAFullTable()
{
	ShadowVolumes_Empty();

	int keys = 0;

	while (keys < 100000 && Keep(MakeKey(keys + 1), 3) >= 0)
	{
		keys++;
	}

	printf("  (the table took %d keys)\n", keys);
	Check(keys > 40000 && keys < 65536, "the table holds plenty, and stops before its last empty place");

	bool allThere = true;

	for (int i = 0; i < keys; i++)
	{
		ShadowVolumeRun run = { -1, -1 };
		allThere = allThere && ShadowVolumes_Find(MakeKey(i + 1), &run) && run.first == i * 3 && run.corners == 3;
	}

	Check(allThere, "every key in a full table is found, in its own place");
	Check(!IsFound(MakeKey(keys + 1)), "a key that isn't there is looked for, and the search ends");

	ShadowVolumes_Keep(MakeKey(keys + 1), 3);
	Check(!IsFound(MakeKey(keys + 1)), "a full table ignores more keys, even if something forgot to ask");
}

static void CheckRuns()
{
	ShadowVolumes_StartRuns();

	ShadowVolumeRun const a = { 0, 36 }, b = { 36, 24 }, c = { 100, 12 }, none = { 500, 0 };

	Check(ShadowVolumes_AddRun(a) && ShadowVolumes_NumRuns() == 1, "a first run");
	Check(ShadowVolumes_AddRun(b) && ShadowVolumes_NumRuns() == 1, "a run that starts where the last ended joins it");
	Check(ShadowVolumes_Run(0).first == 0 && ShadowVolumes_Run(0).corners == 60, "and the two are one run");
	Check(ShadowVolumes_AddRun(none) && ShadowVolumes_NumRuns() == 1, "a run with no corners adds nothing");
	Check(ShadowVolumes_AddRun(c) && ShadowVolumes_NumRuns() == 2, "a run with a gap before it stays separate");
	Check(ShadowVolumes_Run(1).first == 100 && ShadowVolumes_Run(1).corners == 12, "and is kept as it was");

	ShadowVolumes_StartRuns();
	Check(ShadowVolumes_NumRuns() == 0, "starting again leaves no runs");

	int added = 0;

	for (int i = 0; i < 100000; i++)
	{
		ShadowVolumeRun const apart = { i * 10, 3 };

		if (!ShadowVolumes_AddRun(apart))
		{
			break;
		}

		added++;
	}

	printf("  (the list took %d runs)\n", added);
	Check(added > 1000 && added < 100000 && ShadowVolumes_NumRuns() == added, "a full list refuses more runs, and says so");
	Check(ShadowVolumes_Run(added - 1).first == (added - 1) * 10, "and the last one that went on is intact");

	ShadowVolumeRun const joins = { (added - 1) * 10 + 3, 3 };
	Check(ShadowVolumes_AddRun(joins) && ShadowVolumes_NumRuns() == added, "a full list can still join a run to its last one");
	ShadowVolumes_StartRuns();
}

static void CheckStillLights()
{
	float const lamp[kShadowVolumeNumbersEach] = { 10.0f, 20.0f, 30.0f, 5000.0f, 0.0f };
	float moved[kShadowVolumeNumbersEach];
	memcpy(moved, lamp, sizeof(moved));
	moved[1] += 1.0f;

	ShadowVolumes_Empty();
	ShadowVolumes_StartFrame(lamp, 1);
	Check(!ShadowVolumes_LightIsStill(lamp), "a light that has only just appeared isn't still yet");

	ShadowVolumes_StartFrame(lamp, 1);
	Check(ShadowVolumes_LightIsStill(lamp), "a light in the same place as last frame is");

	ShadowVolumes_StartFrame(moved, 1);
	Check(ShadowVolumes_LightIsStill(lamp) && !ShadowVolumes_LightIsStill(moved), "a light that moved this frame isn't, and only last frame counts");

	ShadowVolumes_StartFrame(moved, 1);
	Check(!ShadowVolumes_LightIsStill(lamp), "a place a light has left doesn't count");

	for (int i = 0; i < kShadowVolumeNumbersEach; i++)
	{
		float changed[kShadowVolumeNumbersEach];
		memcpy(changed, moved, sizeof(changed));
		changed[i] += 1.0f;
		Check(!ShadowVolumes_LightIsStill(changed), "every one of a light's numbers counts");
	}

	// more lights than the store has room to remember
	static float many[(kShadowVolumeMostLights + 8) * kShadowVolumeNumbersEach];

	for (int i = 0; i < kShadowVolumeMostLights + 8; i++)
	{
		many[i * kShadowVolumeNumbersEach] = (float)i;
	}

	ShadowVolumes_StartFrame(many, kShadowVolumeMostLights + 8);
	ShadowVolumes_StartFrame(many, kShadowVolumeMostLights + 8);
	Check(ShadowVolumes_LightIsStill(&many[(kShadowVolumeMostLights - 1) * kShadowVolumeNumbersEach]), "the last light there's room for is remembered");
	Check(!ShadowVolumes_LightIsStill(&many[kShadowVolumeMostLights * kShadowVolumeNumbersEach]), "one past that isn't, which only means it isn't kept");

	ShadowVolumes_StartFrame(0, -1);
	ShadowVolumes_StartFrame(0, 0);
	Check(!ShadowVolumes_LightIsStill(lamp), "a frame with a nonsense count of lights has none");
}

static void CheckCounts()
{
	ShadowVolumeCounts counts;
	ShadowVolumes_Empty();
	ShadowVolumes_TakeCounts(&counts);

	ShadowVolumeKey const key = MakeKey(1);
	Keep(key, 12);
	IsFound(key);
	IsFound(key);
	ShadowVolumes_PlaceFor(kShadowVolumeMostCorners);

	ShadowVolumes_TakeCounts(&counts);
	Check(counts.kept == 1 && counts.found == 2 && counts.refused == 1, "the log's counts add up");
	Check(counts.cornersInUse == 12 && counts.entriesInUse == 1, "and so does what's in use");

	ShadowVolumes_TakeCounts(&counts);
	Check(counts.kept == 0 && counts.found == 0 && counts.refused == 0, "reading the counts starts them again");
	Check(counts.cornersInUse == 12 && counts.entriesInUse == 1, "but not what's in use");
}

int main()
{
	CheckFindingThings();
	CheckEveryPartOfTheKeyCounts();
	CheckKeysThatShareAPlaceInTheTable();
	CheckTheListCantOverflow();
	CheckEmptyingWaits();
	CheckRunsDontOutliveTheirFrame();
	CheckAFullTable();
	CheckRuns();
	CheckStillLights();
	CheckCounts();

	if (failures)
	{
		printf("FAILED: %d checks\n", failures);
		return 1;
	}

	printf("The shadow volumes test passed\n");
	return 0;
}
