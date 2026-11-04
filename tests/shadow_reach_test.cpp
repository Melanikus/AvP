// The test for 3dc/shadow_reach.cpp, which decides which walls out of view can be left out of a light's shadows.
// Written 2026-11-04 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// That code may say yes too often, but it may never say no to a shadow that really lands where the camera can see, because that shadow would
// vanish from the screen. So the heart of this test builds real shadows: it takes a light and a ball, and picks points that really are in the
// ball's shadow, many of them on the very edge of it.
//
// The code only ever says no because of one plane. So the strongest check there is: take a real shadow point, stand a plane right next to it at
// any angle at all, with the point just inside, and the answer has to be yes. If that holds for every plane, no view made of planes can go wrong.
// The camera and the box are then checked in made-up scenes, and in a few worked by hand. All of that would pass if the code always said yes, so
// it also checks that it says no often.
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
		float const light[4] = { 0.0f, 0.0f, -99000.0f, 90000.0f };
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

	{
		// The hardest spot: the cone's edge, where it meets the end of the range. From a light at 0 0 0, a ball 1000 away and 500 in size makes a cone
		// 30 degrees from its middle, so at a range of 2000 its edge is at 1732 1000 0. Only that corner of the shadow is inside this plane.
		ShadowReachView corner;
		ShadowReach_StartView(&corner);
		corner.planes[0][0] = -0.2588f;
		corner.planes[0][1] = 0.9659f;
		corner.planes[0][2] = 0.0f;
		corner.planes[0][3] = -500.0f;
		corner.count = 1;

		ShadowReachBall const ball = { 1000.0f, 0.0f, 0.0f, 500.0f };
		float const light[4] = { 0.0f, 0.0f, 0.0f, 2000.0f };
		Check(ShadowReach_CanReach(corner, ball, light), "only the edge of the cone, at the end of the range, is inside the plane: yes");
	}

	{
		// The camera's planes are allowed to stand outside the real screen, and never inside it. At 1000 away, the screen's corners are here.
		const float kAcross = 1.0f / 1.0f, kUp = 1.0f / 1.3f;
		bool allInside = true;

		for (int corner = 0; corner < 4; corner++)
		{
			float const place[3] = { ((corner & 1) ? 1000.0f : -1000.0f) * kAcross, ((corner & 2) ? 1000.0f : -1000.0f) * kUp, 1000.0f };

			ShadowReachView camera;
			ShadowReach_StartView(&camera);
			ShadowReach_AddCameraView(&camera, 1.0f, 1.3f);

			for (int i = 0; i < camera.count; i++)
			{
				float const *plane = camera.planes[i];

				if (plane[0] * place[0] + plane[1] * place[1] + plane[2] * place[2] + plane[3] < 1.0f)
				{
					allInside = false;
				}
			}
		}

		Check(allInside, "all four corners of the real screen are inside the camera's planes");
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

// Fills place with a point on the very edge of the ball's shadow, at the end of the light's range. That's the part of the shadow furthest from
// its middle, so it's the part a wrong sum would lose. Gives back false if the range ends before the shadow does. The cone is drawn in a hair, so
// that the point is really in the shadow and not just next to it.
static bool PickPointOnShadowEdge(const ShadowReachBall &ball, const float *light, float *place)
{
	float const toX = ball.x - light[0], toY = ball.y - light[1], toZ = ball.z - light[2];
	float const distance = sqrtf(toX * toX + toY * toY + toZ * toZ);

	if (distance <= ball.radius * 1.01f)
	{
		return false;
	}

	float const middle[3] = { toX / distance, toY / distance, toZ / distance };
	float const touching = sqrtf(distance * distance - ball.radius * ball.radius);
	float const reach = light[3] * 0.9999f;

	if (reach <= touching)
	{
		return false;
	}

	// any direction square to the cone's middle
	float side[3], sideLength;

	do
	{
		side[0] = RandomBetween(-1.0f, 1.0f);
		side[1] = RandomBetween(-1.0f, 1.0f);
		side[2] = RandomBetween(-1.0f, 1.0f);

		float const along = side[0] * middle[0] + side[1] * middle[1] + side[2] * middle[2];

		side[0] -= middle[0] * along;
		side[1] -= middle[1] * along;
		side[2] -= middle[2] * along;
		sideLength = sqrtf(side[0] * side[0] + side[1] * side[1] + side[2] * side[2]);
	}
	while (sideLength < 0.01f);

	float const outwards = ball.radius / distance * 0.999f;
	float const forwards = sqrtf(1.0f - outwards * outwards);

	for (int axis = 0; axis < 3; axis++)
	{
		place[axis] = light[axis] + (middle[axis] * forwards + side[axis] / sideLength * outwards) * reach;
	}

	return true;
}

// Stands one plane right next to a real shadow point, at a random angle, with the point just inside. The answer has to be yes, every time.
static void CheckPlanesNextToShadows()
{
	const int kScenes = 2000000;

	int tried = 0;
	int wronglyRefused = 0;

	for (int scene = 0; scene < kScenes; scene++)
	{
		float const radius = RandomBetween(10.0f, 8000.0f);
		ShadowReachBall const ball = { RandomBetween(-100000.0f, 100000.0f), RandomBetween(-100000.0f, 100000.0f), RandomBetween(-100000.0f, 100000.0f), radius };

		float away[3], awayLength;

		do
		{
			away[0] = RandomBetween(-1.0f, 1.0f);
			away[1] = RandomBetween(-1.0f, 1.0f);
			away[2] = RandomBetween(-1.0f, 1.0f);
			awayLength = sqrtf(away[0] * away[0] + away[1] * away[1] + away[2] * away[2]);
		}
		while (awayLength > 1.0f || awayLength < 0.01f);

		float const distance = radius * 1.02f + RandomBetween(0.0f, 60000.0f);
		float const light[4] =
		{
			ball.x + away[0] / awayLength * distance, ball.y + away[1] / awayLength * distance, ball.z + away[2] / awayLength * distance,
			RandomBetween(100.0f, 90000.0f)
		};

		float place[3];
		bool const onTheEdge = (scene % 2) == 0;

		if (!(onTheEdge ? PickPointOnShadowEdge(ball, light, place) : PickPointInShadow(ball, light, place)))
		{
			continue;
		}

		float arrow[3], arrowLength;

		do
		{
			arrow[0] = RandomBetween(-1.0f, 1.0f);
			arrow[1] = RandomBetween(-1.0f, 1.0f);
			arrow[2] = RandomBetween(-1.0f, 1.0f);
			arrowLength = sqrtf(arrow[0] * arrow[0] + arrow[1] * arrow[1] + arrow[2] * arrow[2]);
		}
		while (arrowLength > 1.0f || arrowLength < 0.01f);

		ShadowReachView view;
		ShadowReach_StartView(&view);
		view.planes[0][0] = arrow[0] / arrowLength;
		view.planes[0][1] = arrow[1] / arrowLength;
		view.planes[0][2] = arrow[2] / arrowLength;

		// half a unit inside, which is far more than the rounding at these sizes
		view.planes[0][3] = 0.5f - (view.planes[0][0] * place[0] + view.planes[0][1] * place[1] + view.planes[0][2] * place[2]);
		view.count = 1;

		tried++;

		if (!ShadowReach_CanReach(view, ball, light))
		{
			wronglyRefused++;
		}
	}

	char line[160];

	snprintf(line, sizeof(line), "a plane right next to a real shadow point always gets a yes: %d planes tried, %d refused", tried, wronglyRefused);
	Check(wronglyRefused == 0, line);

	snprintf(line, sizeof(line), "enough of those scenes had a point to try: %d", tried);
	Check(tried > kScenes / 4, line);
}

static void CheckRealShadows()
{
	const int kScenes = 300000;
	const int kPointsAScene = 40;

	int wronglyRefused = 0;   // the code said no, and a real shadow point was in view
	int saidNo = 0;
	int nothingToSee = 0;     // scenes where none of the points landed in view
	int shadowInView = 0;     // scenes where one did, which are the ones that can catch a mistake

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

		// The ball is put a short way outside the box, where the answer is hard: a shadow here may just reach in, or just miss. A ball and a light
		// picked from anywhere nearly always have nothing to do with the view, and that would prove very little.
		float const out = RandomBetween(0.0f, 12000.0f);
		float const side = RandomBetween(-1.0f, 1.0f);

		ShadowReachBall const ball =
		{
			middle[0] + ((side > 0.0f) ? size[0] + out : -size[0] - out) * ((scene % 3 == 0) ? 1.0f : RandomBetween(-1.0f, 1.0f)),
			middle[1] + ((side > 0.0f) ? size[1] + out : -size[1] - out) * ((scene % 3 == 1) ? 1.0f : RandomBetween(-1.0f, 1.0f)),
			middle[2] + ((side > 0.0f) ? size[2] + out : -size[2] - out) * ((scene % 3 == 2) ? 1.0f : RandomBetween(-1.0f, 1.0f)),
			RandomBetween(10.0f, 3000.0f)
		};

		float const light[4] =
		{
			ball.x + RandomBetween(-12000.0f, 12000.0f), ball.y + RandomBetween(-12000.0f, 12000.0f), ball.z + RandomBetween(-12000.0f, 12000.0f),
			RandomBetween(1000.0f, 60000.0f)
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
			continue;
		}

		shadowInView++;

		if (!answer)
		{
			wronglyRefused++;
		}
	}

	char line[160];

	snprintf(line, sizeof(line), "no real shadow was refused: %d scenes of %d had a shadow in view, and %d of those were refused", shadowInView, kScenes, wronglyRefused);
	Check(wronglyRefused == 0, line);

	// a test that hardly ever saw a shadow in view would pass whatever the code did
	snprintf(line, sizeof(line), "enough of the scenes had a shadow in view to mean something: %d", shadowInView);
	Check(shadowInView > kScenes / 20, line);

	// without this, a function that always said yes would pass
	snprintf(line, sizeof(line), "it does leave things out: it said no to %d scenes, and %d had no shadow point in view", saidNo, nothingToSee);
	Check(saidNo > nothingToSee / 4, line);
}

int main()
{
	CheckScenesWorkedByHand();
	CheckBallsHoldTheirBoxes();
	CheckPlanesNextToShadows();
	CheckRealShadows();

	if (checksFailed)
	{
		printf("FAILED: %d checks\n", checksFailed);
		return 1;
	}

	printf("all shadow reach checks passed\n");
	return 0;
}
