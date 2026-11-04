// The test for 3dc/shadow_reach.cpp, which decides which walls out of view can be left out of a light's shadows.
// Written 2026-11-04 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// That code may say yes too often, but it may never say no to a shadow that really lands where the camera can see, because that shadow would
// vanish from the screen. So the heart of this test builds real shadows: it takes a light and a ball, and picks points that really are in the
// ball's shadow. If one of those points is in the view, and the code said no, that's a failure. It does this for a great many made-up scenes.
// A test like that alone would pass if the code always said yes, so it also checks that it says no often, and a few scenes are worked by hand.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this with the real shadow_reach.cpp, and the program gives back a non-zero result if any check failed. It uses no
// files and none of the game.

#include <math.h>
#include <stdio.h>

#include "shadow_reach.h"

static int checksFailed = 0;

static void Check(bool good, const char *what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what);

	if (!good)
	{
		checksFailed++;
	}
}

// A number between lowest and highest. It's the same numbers on every run, so that a failure can be seen again.
static unsigned int randomState = 12345;

static float RandomBetween(float lowest, float highest)
{
	randomState = randomState * 1103515245u + 12345u;

	float const fraction = (float)((randomState >> 8) & 0xFFFFFF) / 16777216.0f;

	return lowest + (highest - lowest) * fraction;
}

// A camera like the game's, and a box round what was drawn, 20,000 across in every direction from the camera.
static void MakeTheUsualView(ShadowReachView *view)
{
	float const smallest[3] = { -10000.0f, -3000.0f, -2000.0f };
	float const largest[3] = { 10000.0f, 3000.0f, 20000.0f };

	ShadowReach_StartView(view);
	ShadowReach_AddCameraView(view, 1.0f, 1.3f);
	ShadowReach_AddBox(view, smallest, largest);
}

static void CheckScenesWorkedByHand()
{
	ShadowReachView view;
	MakeTheUsualView(&view);

	Check(view.count == ShadowReachView::kMostPlanes, "a camera and a box fill the view's planes exactly, and none is lost");

	{
		ShadowReachBall const ball = { 0.0f, 0.0f, -100000.0f, 500.0f };
		float const light[4] = { 0.0f, 0.0f, -100100.0f, 90000.0f };
		Check(!ShadowReach_CanReach(view, ball, light), "far behind the camera, with the shadow going further away: no");
	}

	{
		ShadowReachBall const ball = { 0.0f, 0.0f, 5000.0f, 500.0f };
		float const light[4] = { 0.0f, 0.0f, 100.0f, 10000.0f };
		Check(ShadowReach_CanReach(view, ball, light), "straight ahead of the camera: yes");
	}

	{
		ShadowReachBall const ball = { 0.0f, 0.0f, 5000.0f, 500.0f };
		float const light[4] = { 0.0f, 0.0f, 100.0f, 4000.0f };
		Check(!ShadowReach_CanReach(view, ball, light), "a light that stops short of the ball: no");
	}

	{
		ShadowReachBall const ball = { 0.0f, 0.0f, -50000.0f, 1000.0f };
		float const light[4] = { 0.0f, 0.0f, -50000.0f, 100.0f };
		Check(ShadowReach_CanReach(view, ball, light), "a light inside the ball: yes, even a long way off and with a tiny range");
	}

	{
		ShadowReachBall const ball = { 0.0f, 0.0f, -30000.0f, 500.0f };
		float const light[4] = { 0.0f, 0.0f, -35000.0f, 40000.0f };
		Check(ShadowReach_CanReach(view, ball, light), "behind the camera, but with its shadow thrown forward into the view: yes");
	}

	{
		ShadowReachBall const ball = { 0.0f, 9000.0f, 5000.0f, 500.0f };
		float const light[4] = { 0.0f, 8000.0f, 5000.0f, 40000.0f };
		Check(!ShadowReach_CanReach(view, ball, light), "in the room above, and lit from below, so the shadow goes up and away from the box: no");
	}

	{
		ShadowReachBall const ball = { 0.0f, 9000.0f, 5000.0f, 500.0f };
		float const light[4] = { 0.0f, 12000.0f, 5000.0f, 40000.0f };
		Check(ShadowReach_CanReach(view, ball, light), "in the room above, and lit from over it, so the shadow comes down into the box: yes");
	}

	{
		// off the right of the screen, with the light further right, so the shadow crosses the screen
		ShadowReachBall const ball = { 9000.0f, 0.0f, 3000.0f, 500.0f };
		float const light[4] = { 12000.0f, 0.0f, 3000.0f, 30000.0f };
		Check(ShadowReach_CanReach(view, ball, light), "off the side of the screen, with its shadow thrown across it: yes");
	}

	{
		ShadowReachBall const ball = { 9000.0f, 0.0f, 3000.0f, 500.0f };
		float const light[4] = { 8000.0f, 0.0f, 3000.0f, 30000.0f };
		Check(!ShadowReach_CanReach(view, ball, light), "off the side of the screen, with its shadow thrown away from it: no");
	}

	ShadowReachView empty;
	ShadowReach_StartView(&empty);
	ShadowReachBall const anywhere = { 500000.0f, 500000.0f, 500000.0f, 1.0f };
	float const nearby[4] = { 499990.0f, 500000.0f, 500000.0f, 1000.0f };
	Check(ShadowReach_CanReach(empty, anywhere, nearby), "with no planes, a ball within range is always a yes");
}

static void CheckBallsHoldTheirBoxes()
{
	bool allInside = true;

	for (int attempt = 0; attempt < 10000; attempt++)
	{
		float smallest[3], largest[3], ball[4];

		for (int axis = 0; axis < 3; axis++)
		{
			smallest[axis] = RandomBetween(-300000.0f, 300000.0f);
			largest[axis] = smallest[axis] + RandomBetween(0.0f, 100000.0f);
		}

		ShadowReach_BallRoundBox(smallest, largest, ball);

		for (int corner = 0; corner < 8; corner++)
		{
			float const x = ((corner & 1) ? smallest[0] : largest[0]) - ball[0];
			float const y = ((corner & 2) ? smallest[1] : largest[1]) - ball[1];
			float const z = ((corner & 4) ? smallest[2] : largest[2]) - ball[2];

			if (sqrtf(x * x + y * y + z * z) > ball[3])
			{
				allInside = false;
			}
		}
	}

	Check(allInside, "a ball holds all eight corners of its box, for 10,000 boxes");
}

// A place inside the view, by a little more than rounding could account for.
static bool PlaceIsInView(const ShadowReachView &view, const float *place)
{
	for (int i = 0; i < view.count; i++)
	{
		float const *plane = view.planes[i];

		if (plane[0] * place[0] + plane[1] * place[1] + plane[2] * place[2] + plane[3] < 0.05f)
		{
			return false;
		}
	}

	return true;
}

// Fills place with a random point that really is in the ball's shadow, and gives back false if the light can't reach the point it tried.
// Half the points start from the ball's surface, because that's where the edge of the shadow is, and half run right out to the end of the range.
static bool PickPointInShadow(const ShadowReachBall &ball, const float *light, float *place)
{
	float x, y, z, length;

	do
	{
		x = RandomBetween(-1.0f, 1.0f);
		y = RandomBetween(-1.0f, 1.0f);
		z = RandomBetween(-1.0f, 1.0f);
		length = sqrtf(x * x + y * y + z * z);
	}
	while (length > 1.0f || length < 0.01f);

	float const out = (RandomBetween(0.0f, 1.0f) < 0.5f) ? ball.radius / length : ball.radius * RandomBetween(0.0f, 1.0f) / length;
	float const fromLightX = ball.x + x * out - light[0];
	float const fromLightY = ball.y + y * out - light[1];
	float const fromLightZ = ball.z + z * out - light[2];
	float const fromLight = sqrtf(fromLightX * fromLightX + fromLightY * fromLightY + fromLightZ * fromLightZ);

	if (fromLight >= light[3] || fromLight < 1.0f)
	{
		return false;
	}

	float const longest = light[3] / fromLight;
	float const stretch = (RandomBetween(0.0f, 1.0f) < 0.5f) ? longest : RandomBetween(1.0f, longest);

	place[0] = light[0] + fromLightX * stretch;
	place[1] = light[1] + fromLightY * stretch;
	place[2] = light[2] + fromLightZ * stretch;

	return true;
}

static void CheckRealShadows()
{
	const int kScenes = 300000;
	const int kPointsAScene = 40;

	int wronglyRefused = 0;   // the code said no, and a real shadow point was in view
	int saidNo = 0;
	int nothingToSee = 0;     // scenes where none of the points landed in view

	for (int scene = 0; scene < kScenes; scene++)
	{
		ShadowReachView view;

		float const middle[3] = { RandomBetween(-20000.0f, 20000.0f), RandomBetween(-5000.0f, 5000.0f), RandomBetween(0.0f, 30000.0f) };
		float const size[3] = { RandomBetween(500.0f, 15000.0f), RandomBetween(500.0f, 5000.0f), RandomBetween(500.0f, 15000.0f) };
		float const smallest[3] = { middle[0] - size[0], middle[1] - size[1], middle[2] - size[2] };
		float const largest[3] = { middle[0] + size[0], middle[1] + size[1], middle[2] + size[2] };

		ShadowReach_StartView(&view);
		ShadowReach_AddCameraView(&view, RandomBetween(0.4f, 2.0f), RandomBetween(0.4f, 2.0f));
		ShadowReach_AddBox(&view, smallest, largest);

		ShadowReachBall const ball =
		{
			RandomBetween(-60000.0f, 60000.0f), RandomBetween(-30000.0f, 30000.0f), RandomBetween(-40000.0f, 60000.0f), RandomBetween(10.0f, 8000.0f)
		};

		float const light[4] =
		{
			ball.x + RandomBetween(-30000.0f, 30000.0f), ball.y + RandomBetween(-30000.0f, 30000.0f), ball.z + RandomBetween(-30000.0f, 30000.0f),
			RandomBetween(100.0f, 90000.0f)
		};

		bool const answer = ShadowReach_CanReach(view, ball, light);
		bool anyInView = false;

		saidNo += answer ? 0 : 1;

		for (int point = 0; point < kPointsAScene; point++)
		{
			float place[3];

			if (PickPointInShadow(ball, light, place) && PlaceIsInView(view, place))
			{
				anyInView = true;
				break;
			}
		}

		if (!anyInView)
		{
			nothingToSee++;
		}
		else if (!answer)
		{
			wronglyRefused++;
		}
	}

	char line[160];

	sprintf(line, "no real shadow was refused, in %d made-up scenes of %d points each (%d refused)", kScenes, kPointsAScene, wronglyRefused);
	Check(wronglyRefused == 0, line);

	// without this, a function that always said yes would pass
	sprintf(line, "it does leave things out: it said no to %d scenes, and %d had no shadow point in view", saidNo, nothingToSee);
	Check(saidNo > nothingToSee / 2, line);
}

int main()
{
	CheckScenesWorkedByHand();
	CheckBallsHoldTheirBoxes();
	CheckRealShadows();

	if (checksFailed)
	{
		printf("FAILED: %d checks\n", checksFailed);
		return 1;
	}

	printf("all shadow reach checks passed\n");
	return 0;
}
