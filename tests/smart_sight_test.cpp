// A test that the smart gun's and the plasma caster's sight sits on its target, and that the gun fires where the sight is (#179).
// Written 2026-11-09 by Melanikus for the AvP port.
//
// WHAT WENT WRONG
//
// A sight locked on a target is a point on the screen (SmartTarget, targeting.cpp). The 1999 game works that point out with the target's
// height stretched to 4/3, because it drew its whole world 4/3 tall: its screens were 4:3, and a pixel there is 4/3 as tall as the sum
// without the stretch would make it. Two things then turn the point back into something else:
//   - the gun's aim (CalculateWhereGunIsPointing, targeting.cpp) turns it into a direction to fire in, and takes the 4/3 off again with a
//     3/4. In this port that 3/4 had been left out, so a locked gun aimed 4/3 too steeply, more the further up or down the target was.
//   - the Predator's targeting triangle (RenderPredatorTargetingSegment, kshape.cpp) is drawn at the point. The 1999 game drew it in its
//     own stretched space, where the point needed no change. This port draws the world and the triangle through one projection with no
//     stretch (d3d.matProjection, d3d_render.cpp), so the triangle needs the 3/4 too, or it stands 4/3 as far from the middle as its
//     target (Bobblen147's picture: Waterfall, a Predator at a standing marine).
//
// WHAT IT TESTS
//   - SOURCE: the game's sums are the ones this test works through: the sight's 4/3 (SmartTarget), the aim's 3/4, the triangle's 3/4,
//     and the projection's two scales (UpdateProjectionMatrix). A take-out row for each shows each check notices its line going.
//   - THE SUMS: for targets all over the screen, on 4:3, 16:9 and 21:9 screens and at three fields of view, worked through in the game's
//     own fixed-point steps (MUL_FIXED and WideMulNarrowDiv, as mathline.cpp has them): the aim points at the target to within a pixel,
//     and the triangle lands on the target's own place on the screen to within 2 pixels. The same sums without the 3/4 miss, so the
//     test can tell the two apart.
//
// WHAT IT DOESN'T: the smart gun's box is drawn flat on the screen at the sight's point (hud.cpp), not through the projection; whether
// that matches the world on a wide screen needs a look in the game, and isn't changed here.
//
// None of the game runs here: the test reads the game's source (/FC makes __FILE__ the whole path of this file, which is how the source
// is found) and does the sums itself.
#include <fstream>
#include <math.h>
#include <sstream>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include "source_slice.h"

static int checksFailed = 0;

static void Check(bool good, const std::string &what)
{
	printf("%s  %s\n", good ? "ok    " : "FAILED", what.c_str());
	checksFailed += good ? 0 : 1;
}

static void FailedSlice(const std::string &why)
{
	Check(false, why);
}

static std::string ReadSourceFile(const char *name)
{
	std::string const path = __FILE__;
	std::string::size_type const tests = path.rfind("tests");

	if (tests == std::string::npos)
	{
		return std::string();
	}

	std::ifstream file((path.substr(0, tests) + name).c_str(), std::ios::binary);
	std::stringstream all;
	all << file.rdbuf();
	std::string plain;

	for (char c : all.str())
	{
		if (c != '\r')
		{
			plain += c;
		}
	}

	return plain;
}

static bool Has(const std::string &text, const char *what)
{
	return text.find(what) != std::string::npos;
}

// ---- The game's fixed-point steps, as mathline.cpp has them: 16.16 numbers, 65536 is 1

static int const kOneFixed = 65536;

static int MulFixed(int a, int b)
{
	return static_cast<int>((static_cast<int64_t>(a) * b) >> 16);
}

static int WideMulNarrowDiv(int a, int b, int c)
{
	return static_cast<int>((static_cast<int64_t>(a) * b) / c);
}

// ---- The source says what's worked through below

static void TheSource()
{
	printf("The game's sums (read, not run: they need the whole game)\n");
	std::string const targeting = ReadSourceFile("3dc/avp/targeting.cpp");
	std::string const kshape = ReadSourceFile("3dc/kshape.cpp");
	std::string const render = ReadSourceFile("3dc/d3d_render.cpp");
	Check(targeting.size() > 10000 && kshape.size() > 100000 && render.size() > 100000, "the three sources were found and read");

	std::string const sight = SourceSlice(targeting, "void SmartTarget(int speed,int projectile_speed)\n{", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(sight, "targetView.vy*4,") && Has(sight, "(targetView.vz*3)") && Has(sight, "targetView.vx,") && Has(sight, "VDBPtr->VDB_ProjX,"),
		"SOURCE: the sight's point is the target's place with its height stretched to 4/3, as the 1999 game works it out");
	std::string const aim = SourceSlice(targeting, "void CalculateWhereGunIsPointing(TEMPLATE_WEAPON_DATA *twPtr, PLAYER_WEAPON_DATA *weaponPtr)\n{", "\n}\n",
		kSliceEndAfterTheStart, FailedSlice);
	Check(Has(aim, "GunMuzzleDirectionInVS.vy =\n\t\t(((GunMuzzleSightY-(ScreenDescriptorBlock.SDB_Height<<15))/(VDBPtr->VDB_ProjY))*3)/4;") &&
		Has(aim, "(GunMuzzleSightX-(ScreenDescriptorBlock.SDB_Width<<15))/(VDBPtr->VDB_ProjX);"),
		"SOURCE: the gun's aim takes the 4/3 off the sight's height again, as the 1999 game does");
	std::string const triangle = SourceSlice(kshape, "void RenderPredatorTargetingSegment(int theta, int scale, int drawInRed)\n{", "#else", kSliceEndAfterTheStart,
		FailedSlice);
	Check(Has(triangle, "centreY = MUL_FIXED((((centeredSmartTargetSightY - (ScreenDescriptorBlock.SDB_Height)) / Global_VDB_Ptr->VDB_ProjY) * 3) / 4, z);") &&
		Has(triangle, "centreX = MUL_FIXED((centeredSmartTargetSightX - (ScreenDescriptorBlock.SDB_Width)) / Global_VDB_Ptr->VDB_ProjX, z);"),
		"SOURCE: the Predator's triangle takes the 4/3 off too, for this game's projection");
	std::string const projection = SourceSlice(render, "void UpdateProjectionMatrix()\n{", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(projection, "float yScale = cot(fovY/2.0f);") && Has(projection, "float xScale = originalYScale / aspect;") &&
		Has(projection, "d3d.matProjection._11 = xScale;") && Has(projection, "d3d.matProjection._22 = yScale;"),
		"SOURCE: and the projection the world and the triangle go through is the one worked through here: its height fixed by the field of view, its width by the screen's shape");
}

// ---- The sums, worked through

struct Screen
{
	int width;
	int height;
	const char *name;
};

// Where the sight's point is, for a target at (vx, vy, vz) in view space, as SmartTarget puts it (speed 0: straight there)
static void SightOf(const Screen &screen, int vx, int vy, int vz, int *sightX, int *sightY)
{
	int const projX = screen.width / 2;
	int const projY = screen.height / 2;
	int const screenX = WideMulNarrowDiv(vx, projX, vz);
	int const screenY = WideMulNarrowDiv(vy * 4, projY, vz * 3);
	*sightX = (screen.width / 2 + screenX) << 16;
	*sightY = (screen.height / 2 + screenY) << 16;
}

// The gun's aim from the sight's point: a direction whose depth is 1 (65536), as CalculateWhereGunIsPointing works it out
static void AimOf(const Screen &screen, int sightX, int sightY, bool withTheThreeQuarters, int *aimX, int *aimY)
{
	int const projX = screen.width / 2;
	int const projY = screen.height / 2;
	*aimX = (sightX - (screen.width << 15)) / projX;
	*aimY = withTheThreeQuarters ? (((sightY - (screen.height << 15)) / projY) * 3) / 4 : (sightY - (screen.height << 15)) / projY;
}

// Where the triangle's middle lands on the screen, in pixels from the middle, drawn as RenderPredatorTargetingSegment draws it with no
// zoom, through a projection with these two scales
static void TriangleOnTheScreen(const Screen &screen, int sightX, int sightY, bool withTheThreeQuarters, double xScale, double yScale, double *x,
	double *y)
{
	int const projX = screen.width / 2;
	int const projY = screen.height / 2;
	int z = kOneFixed - 0;   // a scale of 0: the triangle at its smallest, locked on
	z = MulFixed(MulFixed(z, z), 2048);
	int const centredX = sightX - (screen.width << 15);
	int const centredY = sightY - (screen.height << 15);
	int const centreY = withTheThreeQuarters ? MulFixed((((centredY - screen.height) / projY) * 3) / 4, z) : MulFixed((centredY - screen.height) / projY, z);
	int const centreX = MulFixed((centredX - screen.width) / projX, z);
	*x = static_cast<double>(centreX) * xScale / z * (screen.width / 2.0);
	*y = static_cast<double>(centreY) * yScale / z * (screen.height / 2.0);
}

static void TheSums()
{
	printf("The sight, the aim and the triangle, worked through\n");
	Screen const screens[] = { { 640, 480, "640x480" }, { 1280, 720, "1280x720" }, { 1920, 1080, "1920x1080" }, { 2560, 1080, "2560x1080" } };
	int const fieldsOfView[] = { 75, 90, 60 };   // the Marine's and Predator's own 75, and two the console's fov can give
	double const pi = 3.14159265358979323846;

	int targets = 0;
	int aimMisses = 0;
	int oldAimMisses = 0;
	int triangleMisses = 0;
	int oldTriangleMisses = 0;
	double worstAim = 0.0;
	double worstTriangle = 0.0;

	for (const Screen &screen : screens)
	{
		for (int fov : fieldsOfView)
		{
			double const yScale = 1.0 / tan(fov * pi / 360.0);
			double const xScale = yScale / (static_cast<double>(screen.width) / screen.height);

			// Targets across most of the screen (the sight follows only to 14/32 of it either way), near and far
			for (int vz = 3000; vz <= 30000; vz += 9000)
			{
				for (int across = -6; across <= 6; across += 3)
				{
					for (int up = -6; up <= 6; up += 2)
					{
						// The target's place, chosen by where the world draws it: up to 0.4 of the way to each edge
						double const wantX = across / 15.0;
						double const wantY = up / 15.0;
						int const vx = static_cast<int>(wantX / xScale * vz);
						int const vy = static_cast<int>(wantY / yScale * vz);
						int sightX = 0;
						int sightY = 0;
						SightOf(screen, vx, vy, vz, &sightX, &sightY);

						if ((sightX >> 16) - screen.width / 2 <= -(screen.width * 14) / 32 || (sightX >> 16) - screen.width / 2 >= (screen.width * 14) / 32 ||
							(sightY >> 16) - screen.height / 2 <= -(screen.height * 14) / 32 || (sightY >> 16) - screen.height / 2 >= (screen.height * 14) / 32)
						{
							continue;   // SmartTarget lets go of a target this far out
						}

						targets++;

						// The aim: its direction against the target's own, in pixels at this screen (one pixel of the sight is the most
						// rounding can cost)
						double const pixelX = kOneFixed / (screen.width / 2.0);
						double const pixelY = kOneFixed / (screen.height / 2.0);
						double const trueX = static_cast<double>(vx) / vz * kOneFixed;
						double const trueY = static_cast<double>(vy) / vz * kOneFixed;
						int aimX = 0;
						int aimY = 0;
						AimOf(screen, sightX, sightY, true, &aimX, &aimY);
						double const aimOff = fmax(fabs(aimX - trueX) / pixelX, fabs(aimY - trueY) / pixelY);
						worstAim = fmax(worstAim, aimOff);
						aimMisses += aimOff > 1.5 ? 1 : 0;
						AimOf(screen, sightX, sightY, false, &aimX, &aimY);
						oldAimMisses += fmax(fabs(aimX - trueX) / pixelX, fabs(aimY - trueY) / pixelY) > 1.5 ? 1 : 0;

						// The triangle against the target, both through the same projection
						double const targetX = static_cast<double>(vx) / vz * xScale * (screen.width / 2.0);
						double const targetY = static_cast<double>(vy) / vz * yScale * (screen.height / 2.0);
						double x = 0.0;
						double y = 0.0;
						TriangleOnTheScreen(screen, sightX, sightY, true, xScale, yScale, &x, &y);
						double const triangleOff = fmax(fabs(x - targetX), fabs(y - targetY));
						worstTriangle = fmax(worstTriangle, triangleOff);
						triangleMisses += triangleOff > 2.0 ? 1 : 0;
						TriangleOnTheScreen(screen, sightX, sightY, false, xScale, yScale, &x, &y);
						oldTriangleMisses += fmax(fabs(x - targetX), fabs(y - targetY)) > 2.0 ? 1 : 0;
					}
				}
			}
		}
	}

	char line[200];
	snprintf(line, sizeof(line), "%d targets on 4 screens at 3 fields of view were worked through", targets);
	Check(targets > 300, line);
	snprintf(line, sizeof(line), "the gun aims at every one of them, to within a pixel and a half of the sight (the worst: %.2f)", worstAim);
	Check(aimMisses == 0, line);
	snprintf(line, sizeof(line), "and the triangle lands on every one, to within 2 pixels (the worst: %.2f)", worstTriangle);
	Check(triangleMisses == 0, line);
	snprintf(line, sizeof(line), "without the 3/4, the aim misses %d of them and the triangle %d: the sums can tell the two apart", oldAimMisses, oldTriangleMisses);
	Check(oldAimMisses > targets / 2 && oldTriangleMisses > targets / 2, line);
}

int main()
{
	TheSource();
	TheSums();

	if (checksFailed > 0)
	{
		printf("%d check%s FAILED\n", checksFailed, checksFailed == 1 ? "" : "s");
		return 1;
	}

	printf("Every check passed\n");
	return 0;
}
