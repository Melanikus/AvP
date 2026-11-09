// Does Hudson's counting find what's wrong in a picture made wrong on purpose? Written 2026-11-05 by Melanikus for the AvP port.
//
// hudson_vision.h says what this counts. Everything here is plain arrays, with no game and no graphics card. A run in a test copy, with
// a shader broken on purpose, is what proves the rest of the way from the card to the log.
#include "hudson_vision.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <vector>

static int checksFailed = 0;

static void Check(bool passed, const char *what)
{
	printf("  %s  %s\n", passed ? "ok" : "FAILED", what);

	if (!passed)
	{
		checksFailed++;
	}
}

static uint32_t const kWidth = 8, kHeight = 6;

// Three pictures of a good frame: a surface everywhere, facing somewhere, 5000 away. Pitches are wider than the rows on purpose, as a card's can be.
struct Pictures
{
	std::vector<uint8_t> distance, normal, colour;
	static uint32_t const kPitch = (kWidth + 3) * 4;

	Pictures() : distance(kHeight * kPitch), normal(kHeight * kPitch), colour(kHeight * kPitch)
	{
		for (uint32_t y = 0; y < kHeight; y++)
		{
			for (uint32_t x = 0; x < kWidth; x++)
			{
				SetDistance(x, y, 5000.0f);
				SetNormal(x, y, 128, 200, 128);
				colour[y * kPitch + x * 4 + 3] = 255;
			}
		}
	}

	void SetDistance(uint32_t x, uint32_t y, float value) { memcpy(&distance[y * kPitch + x * 4], &value, sizeof(value)); }
	void SetNormal(uint32_t x, uint32_t y, uint8_t blue, uint8_t green, uint8_t red)
	{
		uint8_t *const pixel = &normal[y * kPitch + x * 4];
		pixel[0] = blue;
		pixel[1] = green;
		pixel[2] = red;
	}
	void SetNoSurface(uint32_t x, uint32_t y) { colour[y * kPitch + x * 4 + 3] = 0; }

	SurfacePicturesInMemory InMemory() const
	{
		SurfacePicturesInMemory pictures = { &distance[0], &normal[0], &colour[0], kPitch, kPitch, kPitch };
		return pictures;
	}
};

static bool BoxIs(const HudsonVisionFinding &found, int pixels, int left, int top, int right, int bottom, int firstX, int firstY)
{
	return found.pixels == pixels && found.left == left && found.top == top && found.right == right && found.bottom == bottom && found.firstX == firstX && found.firstY == firstY;
}

static void TestAGoodFrame()
{
	Pictures pictures;
	HudsonVisionCounts counts;

	HudsonVision_Count(pictures.InMemory(), kWidth, kHeight, false, &counts);

	Check(counts.pixelsLookedAt == (int)(kWidth * kHeight), "every pixel is looked at");
	Check(counts.surfaces == (int)(kWidth * kHeight), "and a good frame is all surface");
	Check(counts.noNormal.pixels == 0 && counts.noDistance.pixels == 0 && counts.nothingDrawn.pixels == 0, "with nothing to report");
	Check(counts.samples == 0, "and no rays to send");
	Check(counts.nearest == 5000.0f && counts.furthest == 5000.0f, "and it says how far away the surfaces are");
}

static void TestNoNormal()
{
	Pictures pictures;
	HudsonVisionCounts counts;

	pictures.SetNormal(5, 1, 0, 0, 0);
	pictures.SetNormal(2, 4, 0, 0, 0);
	HudsonVision_Count(pictures.InMemory(), kWidth, kHeight, false, &counts);

	Check(BoxIs(counts.noNormal, 2, 2, 1, 5, 4, 5, 1), "a surface with no normal is found, with its box and the first of them");
	Check(counts.noDistance.pixels == 0 && counts.nothingDrawn.pixels == 0, "and isn't taken for anything else");
	Check(counts.surfaces == (int)(kWidth * kHeight), "and it still counts as a surface");

	Pictures oneZero;
	oneZero.SetNormal(3, 3, 0, 0, 1);
	HudsonVision_Count(oneZero.InMemory(), kWidth, kHeight, false, &counts);
	Check(counts.noNormal.pixels == 0, "one direction of nothing isn't no normal: red counts");

	Pictures greenOnly;
	greenOnly.SetNormal(3, 3, 0, 1, 0);
	HudsonVision_Count(greenOnly.InMemory(), kWidth, kHeight, false, &counts);
	Check(counts.noNormal.pixels == 0, "green counts");

	Pictures blueOnly;
	blueOnly.SetNormal(3, 3, 1, 0, 0);
	HudsonVision_Count(blueOnly.InMemory(), kWidth, kHeight, false, &counts);
	Check(counts.noNormal.pixels == 0, "and blue counts");

	Pictures unusedByte;
	unusedByte.SetNormal(3, 3, 0, 0, 0);
	unusedByte.normal[3 * Pictures::kPitch + 3 * 4 + 3] = 255;
	HudsonVision_Count(unusedByte.InMemory(), kWidth, kHeight, false, &counts);
	Check(counts.noNormal.pixels == 1, "the fourth byte means nothing in eight bits");

	// Ten bits: the top two bits are unused
	Pictures tenBit;
	uint32_t const onlyTheUnusedBits = 0xC0000000;
	memcpy(&tenBit.normal[Pictures::kPitch + 4], &onlyTheUnusedBits, sizeof(onlyTheUnusedBits));
	uint32_t const lowestBit = 0x00000001;
	memcpy(&tenBit.normal[Pictures::kPitch + 8], &lowestBit, sizeof(lowestBit));
	uint32_t const highestUsedBit = 0x20000000;
	memcpy(&tenBit.normal[Pictures::kPitch + 12], &highestUsedBit, sizeof(highestUsedBit));
	HudsonVision_Count(tenBit.InMemory(), kWidth, kHeight, true, &counts);
	Check(BoxIs(counts.noNormal, 1, 1, 1, 1, 1, 1, 1), "in ten bits the top two mean nothing, and a bit from any of the three directions counts");
}

static void TestNoDistance()
{
	Pictures pictures;
	HudsonVisionCounts counts;

	pictures.SetDistance(0, 0, kHudsonVisionClearedDistance);
	pictures.SetDistance(7, 5, kHudsonVisionClearedDistance);
	pictures.SetDistance(3, 2, 900.0f);
	pictures.SetDistance(4, 2, 7000.0f);
	HudsonVision_Count(pictures.InMemory(), kWidth, kHeight, false, &counts);

	Check(BoxIs(counts.noDistance, 2, 0, 0, 7, 5, 0, 0), "a surface whose distance is still as it was cleared is found, from corner to corner");
	Check(counts.nearest == 900.0f && counts.furthest == 7000.0f, "and doesn't count as how far away anything is");
	Check(counts.noNormal.pixels == 0, "and isn't taken for no normal");

	Pictures almost;
	almost.SetDistance(2, 2, 1.5f);
	HudsonVision_Count(almost.InMemory(), kWidth, kHeight, false, &counts);
	Check(counts.noDistance.pixels == 0 && counts.nearest == 1.5f, "a surface very close by is a real distance");
}

static void TestNothingDrawn()
{
	Pictures pictures;
	HudsonVisionCounts counts;

	pictures.SetNoSurface(1, 2);
	pictures.SetNoSurface(6, 3);
	pictures.SetNormal(1, 2, 0, 0, 0);
	pictures.SetDistance(6, 3, kHudsonVisionClearedDistance);
	HudsonVision_Count(pictures.InMemory(), kWidth, kHeight, false, &counts);

	Check(BoxIs(counts.nothingDrawn, 2, 1, 2, 6, 3, 1, 2), "pixels with no surface are found");
	Check(counts.surfaces == (int)(kWidth * kHeight) - 2, "and don't count as surfaces");
	Check(counts.noNormal.pixels == 0 && counts.noDistance.pixels == 0, "and having nothing there isn't missing a normal or a distance");
	Check(counts.samples == 2 && counts.sampleX[0] == 1 && counts.sampleY[0] == 2 && counts.sampleX[1] == 6 && counts.sampleY[1] == 3, "with few of them, each gets a ray");

	Pictures nearlySolid;
	nearlySolid.colour[3] = 254;
	HudsonVision_Count(nearlySolid.InMemory(), kWidth, kHeight, false, &counts);
	Check(counts.nothingDrawn.pixels == 1, "only a full alpha counts as a surface, as everywhere else that reads these");

	Pictures empty;

	for (uint32_t y = 0; y < kHeight; y++)
	{
		for (uint32_t x = 0; x < kWidth; x++)
		{
			empty.SetNoSurface(x, y);
		}
	}

	HudsonVision_Count(empty.InMemory(), kWidth, kHeight, false, &counts);
	Check(counts.nothingDrawn.pixels == (int)(kWidth * kHeight), "a frame of nothing is all found");
	Check(counts.samples == kHudsonVisionMostSamples, "and never more rays than the limit");
	Check(counts.sampleY[kHudsonVisionMostSamples - 1] > 3, "and they're spread out down the screen, not all at the top");
	Check(counts.nearest == 0.0f && counts.furthest == 0.0f, "and no surfaces means no distances");

	// One pixel fewer, so that the steps don't come out even, and without the limit there'd be more than there's room for
	Pictures nearlyEmpty = empty;

	nearlyEmpty.colour[3] = 255;
	HudsonVision_Count(nearlyEmpty.InMemory(), kWidth, kHeight, false, &counts);
	Check(counts.samples == kHudsonVisionMostSamples, "and steps that don't come out even still stop at the limit");
}

static bool Near(float a, float b)
{
	return fabs(a - b) < 0.0001f;
}

static void TestRays()
{
	// A camera looking straight along z, with across along x and up along y
	float const view[12] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 };
	float direction[3] = { 9, 9, 9 };

	Check(HudsonVision_RayThrough(640, 360, 1280, 720, 1.0f, 2.0f, view, direction), "a ray can go through the middle of the screen");
	Check(Near(direction[0], 0.0f) && Near(direction[1], 0.0f) && Near(direction[2], 1.0f), "and it goes straight ahead");

	Check(HudsonVision_RayThrough(1280 - 1, 360, 1280, 720, 1.0f, 2.0f, view, direction), "the right edge of the screen is on it");
	Check(direction[0] > 0.5f && Near(direction[1], 0.0f), "and looks to the right, by how wide the view is");

	Check(HudsonVision_RayThrough(0, 360, 1280, 720, 1.0f, 2.0f, view, direction), "the left edge is on it");
	Check(direction[0] < -0.5f, "and looks to the left");

	Check(HudsonVision_RayThrough(640, 0, 1280, 720, 1.0f, 2.0f, view, direction), "the top is on it");
	Check(direction[1] < -0.3f && direction[1] > -0.5f, "and looks along up with the game's sign, by the second number of the projection");

	Check(HudsonVision_RayThrough(640, 719, 1280, 720, 1.0f, 2.0f, view, direction), "the bottom is on it");
	Check(direction[1] > 0.3f, "and looks the other way");

	float const length = sqrt(direction[0] * direction[0] + direction[1] * direction[1] + direction[2] * direction[2]);
	Check(Near(length, 1.0f), "a ray's direction has a length of 1");

	Check(!HudsonVision_RayThrough(-1, 360, 1280, 720, 1.0f, 2.0f, view, direction), "a pixel off the left isn't on the screen");
	Check(!HudsonVision_RayThrough(1280, 360, 1280, 720, 1.0f, 2.0f, view, direction), "or off the right");
	Check(!HudsonVision_RayThrough(640, -1, 1280, 720, 1.0f, 2.0f, view, direction), "or off the top");
	Check(!HudsonVision_RayThrough(640, 720, 1280, 720, 1.0f, 2.0f, view, direction), "or off the bottom");
	Check(!HudsonVision_RayThrough(0, 0, 0, 720, 1.0f, 2.0f, view, direction), "a screen of no width can't be used");
	Check(!HudsonVision_RayThrough(0, 0, 1280, 0, 1.0f, 2.0f, view, direction), "or no height");
	Check(!HudsonVision_RayThrough(0, 0, 1280, 720, 0.0f, 2.0f, view, direction), "and a projection of nothing can't either");
	Check(!HudsonVision_RayThrough(0, 0, 1280, 720, 1.0f, 0.0f, view, direction), "either way");

	float const nothing[12] = { 0 };
	Check(!HudsonVision_RayThrough(640, 360, 1280, 720, 1.0f, 2.0f, nothing, direction), "and a view of nothing has no direction");
}

static void TestTheScreen()
{
	// Everything mid grey, as blue, green, red and one unused, with a pitch wider than the row
	int const width = 4, height = 3;
	uint32_t const pitch = (width + 2) * 4;
	std::vector<uint8_t> bits(height * pitch, 100);
	HudsonVisionScreenPixel pixel;

	Check(HudsonVision_ScreenAt(&bits[0], pitch, width, height, 1, 1, &pixel), "a pixel on the screen can be looked at");
	Check(pixel.x == 1 && pixel.y == 1 && pixel.colour[0] == 100 && pixel.around[0] == 100 && !pixel.differs, "and one like those round it doesn't differ");

	// A black crack
	bits[pitch + 4] = 0;
	bits[pitch + 5] = 0;
	bits[pitch + 6] = 0;
	HudsonVision_ScreenAt(&bits[0], pitch, width, height, 1, 1, &pixel);
	Check(pixel.colour[0] == 0 && pixel.colour[1] == 0 && pixel.colour[2] == 0 && pixel.around[0] == 100 && pixel.differs, "a black pixel among grey ones does");

	// Only one colour off, each in turn, to show which byte is which
	std::vector<uint8_t> red(height * pitch, 100);
	red[pitch + 6] = 200;
	HudsonVision_ScreenAt(&red[0], pitch, width, height, 1, 1, &pixel);
	Check(pixel.colour[0] == 200 && pixel.colour[1] == 100 && pixel.colour[2] == 100 && pixel.differs, "the third byte is red");

	std::vector<uint8_t> green(height * pitch, 100);
	green[pitch + 5] = 200;
	HudsonVision_ScreenAt(&green[0], pitch, width, height, 1, 1, &pixel);
	Check(pixel.colour[1] == 200 && pixel.differs, "the second is green");

	std::vector<uint8_t> blue(height * pitch, 100);
	blue[pitch + 4] = 200;
	HudsonVision_ScreenAt(&blue[0], pitch, width, height, 1, 1, &pixel);
	Check(pixel.colour[2] == 200 && pixel.differs, "the first is blue");

	std::vector<uint8_t> near(height * pitch, 100);
	near[pitch + 4] = 148;
	HudsonVision_ScreenAt(&near[0], pitch, width, height, 1, 1, &pixel);
	Check(!pixel.differs, "a difference up to the limit doesn't count");
	near[pitch + 4] = 149;
	HudsonVision_ScreenAt(&near[0], pitch, width, height, 1, 1, &pixel);
	Check(pixel.differs, "and one past it does");

	// The average leaves out the pixel itself, and copes with being in a corner
	std::vector<uint8_t> corner(height * pitch, 0);
	corner[0] = corner[1] = corner[2] = 255;
	HudsonVision_ScreenAt(&corner[0], pitch, width, height, 0, 0, &pixel);
	Check(pixel.colour[0] == 255 && pixel.around[0] == 0 && pixel.differs, "the average leaves out the pixel itself, even in a corner of the screen");

	std::vector<uint8_t> farCorner(height * pitch, 50);
	HudsonVision_ScreenAt(&farCorner[0], pitch, width, height, width - 1, height - 1, &pixel);
	Check(pixel.around[0] == 50 && !pixel.differs, "and the opposite corner doesn't read off the edge");

	// In the dark, what counts is how much darker it is
	std::vector<uint8_t> dark(height * pitch, 0);

	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			dark[y * pitch + x * 4 + 2] = 23;
		}
	}

	dark[pitch + 6] = 0;
	HudsonVision_ScreenAt(&dark[0], pitch, width, height, 1, 1, &pixel);
	Check(pixel.around[0] == 23 && pixel.differs && !pixel.tooDark, "a black pixel among dark red ones stands out, though it's far less than the limit away");

	dark[pitch + 6] = 5;
	HudsonVision_ScreenAt(&dark[0], pitch, width, height, 1, 1, &pixel);
	Check(pixel.differs, "a quarter as bright or less still does");

	dark[pitch + 6] = 6;
	HudsonVision_ScreenAt(&dark[0], pitch, width, height, 1, 1, &pixel);
	Check(!pixel.differs, "and more than that doesn't");

	std::vector<uint8_t> veryDark(height * pitch, 0);

	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			veryDark[y * pitch + x * 4 + 2] = 15;
		}
	}

	veryDark[pitch + 6] = 0;
	HudsonVision_ScreenAt(&veryDark[0], pitch, width, height, 1, 1, &pixel);
	Check(pixel.tooDark && !pixel.differs, "below the dark limit everywhere, nothing can be said");

	veryDark[pitch + 6] = 16;
	HudsonVision_ScreenAt(&veryDark[0], pitch, width, height, 1, 1, &pixel);
	Check(!pixel.tooDark, "the pixel itself being bright enough is enough to say");

	std::vector<uint8_t> brightAround(height * pitch, 16);
	brightAround[pitch + 4] = brightAround[pitch + 5] = brightAround[pitch + 6] = 0;
	HudsonVision_ScreenAt(&brightAround[0], pitch, width, height, 1, 1, &pixel);
	Check(!pixel.tooDark && pixel.differs, "and so is those round it being");

	Check(!HudsonVision_ScreenAt(&bits[0], pitch, width, height, -1, 0, &pixel), "a pixel off the left isn't on the screen");
	Check(!HudsonVision_ScreenAt(&bits[0], pitch, width, height, 0, -1, &pixel), "nor off the top");
	Check(!HudsonVision_ScreenAt(&bits[0], pitch, width, height, width, 0, &pixel), "nor off the right");
	Check(!HudsonVision_ScreenAt(&bits[0], pitch, width, height, 0, height, &pixel), "nor off the bottom");
	Check(!HudsonVision_ScreenAt(&bits[0], pitch, width, height, 0, -1, &pixel), "or off the top");
	Check(!HudsonVision_ScreenAt(&bits[0], pitch, width, height, width, 0, &pixel), "or off the right");
	Check(!HudsonVision_ScreenAt(&bits[0], pitch, width, height, 0, height, &pixel), "or off the bottom");
}

static void TestShadows()
{
	HudsonVisionShadows shadows = { true, 2, 100, 0 };

	Check(HudsonVision_ShadowsAreMissing(shadows), "shadows on, lights chosen, things to cast them, and no corners is a fault");

	shadows.corners = 1;
	Check(!HudsonVision_ShadowsAreMissing(shadows), "any corners at all isn't");
	shadows.corners = 0;

	shadows.volumesOn = false;
	Check(!HudsonVision_ShadowsAreMissing(shadows), "shadows being off isn't");
	shadows.volumesOn = true;

	shadows.lights = 0;
	Check(!HudsonVision_ShadowsAreMissing(shadows), "no lights isn't");
	shadows.lights = 2;

	shadows.casters = 0;
	Check(!HudsonVision_ShadowsAreMissing(shadows), "and nothing to cast them isn't");
}

static void TestRaysAgainstThePicture()
{
	float const view[12] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 };
	float const ahead[3] = { 0.0f, 0.0f, 1.0f };
	float const slanted[3] = { 0.6f, 0.0f, 0.8f };

	Check(HudsonVision_RayAgrees(5000.0f, 5000, ahead, view), "straight ahead, a ray and the picture read the same");
	Check(HudsonVision_RayAgrees(4000.0f, 5000, slanted, view), "at a slant, the ray is longer than the picture says, by how much it points away");
	Check(!HudsonVision_RayAgrees(5000.0f, 5000, slanted, view), "and the same length isn't right there");
	Check(!HudsonVision_RayAgrees(5000.0f, 2000, ahead, view), "a ray that hit something much nearer doesn't agree");
	Check(!HudsonVision_RayAgrees(5000.0f, 9000, ahead, view), "and nor does one much further away");
	Check(HudsonVision_RayAgrees(5000.0f, 5149, ahead, view), "just inside the 3% allowed agrees");
	Check(!HudsonVision_RayAgrees(5000.0f, 5151, ahead, view), "just outside it doesn't");
	Check(HudsonVision_RayAgrees(100.0f, 119, ahead, view), "close up, 20 units are allowed");
	Check(!HudsonVision_RayAgrees(100.0f, 121, ahead, view), "and no more");
	Check(fabs(HudsonVision_HowFarAhead(5000, slanted, view) - 4000.0) < 1.0, "how far straight ahead a slanted ray goes");

	// A camera looking some other way than straight along z, so that every part of the sum matters
	float const turned[12] = { 1, 0, 0, 0, 0, 1, 0, 0, 0.6f, 0.8f, 0, 0 };
	float const alongX[3] = { 1.0f, 0.0f, 0.0f };
	float const alongY[3] = { 0.0f, 1.0f, 0.0f };

	Check(fabs(HudsonVision_HowFarAhead(1000, alongX, turned) - 600.0) < 1.0, "a camera turned part way along x sees a ray along x as part way ahead");
	Check(fabs(HudsonVision_HowFarAhead(1000, alongY, turned) - 800.0) < 1.0, "and the same for y");

	float const arrowOutOfTheBack[4] = { 0.0f, 0.0f, 1.0f, 0.0f };
	float const backwards[3] = { 0.0f, 0.0f, -1.0f };
	float const sideOn[3] = { 1.0f, 0.0f, 0.0f };

	Check(!HudsonVision_SeenFromBehind(ahead, arrowOutOfTheBack), "a ray heading the way the arrow out of a polygon's back points meets its front");
	Check(HudsonVision_SeenFromBehind(backwards, arrowOutOfTheBack), "and one heading against it meets its back");
	Check(!HudsonVision_SeenFromBehind(sideOn, arrowOutOfTheBack), "and side-on isn't behind");
}

static void TestChecksArePicked()
{
	{
		Pictures whole;
		HudsonVisionCounts counts;

		HudsonVision_Count(whole.InMemory(), kWidth, kHeight, false, &counts);

		Check(counts.checks == kHudsonVisionMostChecks, "a few pixels with a surface are picked to check rays against");
		Check(counts.checkX[0] == 0 && counts.checkX[kHudsonVisionMostChecks - 1] == (int)kWidth - 1, "spread across the screen, not down its left edge");

		int lowest = 0;
		bool wentBackUp = false;

		for (int i = 0; i < counts.checks; i++)
		{
			lowest = counts.checkY[i] > lowest ? counts.checkY[i] : lowest;
			wentBackUp = wentBackUp || (i > 0 && counts.checkY[i] < counts.checkY[i - 1]);
		}

		Check(lowest > 3, "and spread down it");
		Check(wentBackUp, "and not along one line");
	}

	// These two are where the second and third of them fall on a screen this size
	Pictures pictures;
	HudsonVisionCounts counts;

	pictures.SetDistance(1, 4, kHudsonVisionClearedDistance);
	pictures.SetNoSurface(2, 1);
	HudsonVision_Count(pictures.InMemory(), kWidth, kHeight, false, &counts);

	Check(counts.checks == kHudsonVisionMostChecks - 2, "a pixel with no distance or no surface is one check fewer");

	bool anyBad = false;

	for (int i = 0; i < counts.checks; i++)
	{
		anyBad = anyBad || (counts.checkX[i] == 1 && counts.checkY[i] == 4) || (counts.checkX[i] == 2 && counts.checkY[i] == 1) || counts.checkDistance[i] != 5000.0f;
	}

	Check(!anyBad, "none with no distance or no surface, and each with its distance");

	HudsonVisionCounts nothing;
	HudsonVision_Count(pictures.InMemory(), 0, 0, false, &nothing);
	Check(nothing.checks == 0, "a picture of no size has nothing to check");
}

static void TestTheCheck()
{
	uint8_t const *const nothing = (uint8_t const *)"";

	// Published values for this sum, so that a tool outside the game can be held to the same answer
	Check(HudsonVision_Check(nothing, 0) == 0x811C9DC5u, "the check of nothing is the sum's starting number");
	Check(HudsonVision_Check((uint8_t const *)"a", 1) == 0xE40C292Cu, "the check of a letter is the published one");
	Check(HudsonVision_Check((uint8_t const *)"foobar", 6) == 0xBF9CF968u, "and of a word");
	Check(HudsonVision_Check((uint8_t const *)"ab", 2) != HudsonVision_Check((uint8_t const *)"ba", 2), "the order of the bytes matters");
}

int main()
{
	printf("What Hudson can tell from a frame's pictures:\n");

	TestAGoodFrame();
	TestNoNormal();
	TestNoDistance();
	TestNothingDrawn();
	TestRays();
	TestTheCheck();
	TestShadows();
	TestTheScreen();
	TestRaysAgainstThePicture();
	TestChecksArePicked();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");

	return checksFailed ? 1 : 0;
}
