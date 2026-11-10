// A test that the smart gun's and the plasma caster's sight sits on its target, and that the gun fires where the sight is (#179).
// Written 2026-11-09 by Melanikus for the AvP port. Modified 2026-11-10 by Melanikus for the AvP port: every line it reads must be compiled.
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
// The first fix (2026-11-09) put the triangle's 3/4 in a second copy of RenderPredatorTargetingSegment that sat under
// #ifdef USE_D3DVIEWTRANSFORM, which nothing defines, so the game never compiled it. This test read only the text, found the 3/4, and
// passed, while the game Bobblen147 played on beta.1 still drew the triangle 4/3 out. The copy is gone now, and this test checks that
// every line it reads is one the game compiles.
//
// WHAT IT TESTS
//   - SOURCE: the game's sums are the ones this test works through: the sight's 4/3 (SmartTarget), the aim's 3/4, the triangle's 3/4,
//     and the projection's two scales (UpdateProjectionMatrix). A take-out row for each shows each check notices its line going.
//   - COMPILED: each of those lines is found once in its function, and inside no #if this test can't answer. It can answer only #if 0,
//     #if 1, and #ifdef / #ifndef USE_D3DVIEWTRANSFORM, after checking that nothing in 3dc or win32 defines USE_D3DVIEWTRANSFORM (no
//     #define of it, and no project file naming it). Any other #if, an #elif, or a line found twice fails the check, so whoever moves a
//     line this test reads into one has to come and look. A take-out row puts the triangle's line back under the #ifdef, as the first
//     fix had it, and another defines USE_D3DVIEWTRANSFORM: both must be noticed.
//   - THE SUMS: for targets all over the screen, on 4:3, 16:9 and 21:9 screens and at three fields of view, worked through in the game's
//     own fixed-point steps (MUL_FIXED and WideMulNarrowDiv, as mathline.cpp has them): the aim points at the target to within a pixel,
//     and the triangle lands on the target's own place on the screen to within 2 pixels. The same sums without the 3/4 miss, so the
//     test can tell the two apart.
//
// WHAT IT DOESN'T: the smart gun's box is drawn flat on the screen at the sight's point (hud.cpp), not through the projection; whether
// that matches the world on a wide screen needs a look in the game, and isn't changed here. Nor does it prove what the game shows: that
// takes a look in the game, the triangle against the marine at several heights.
//
// None of the game runs here: the test reads the game's source (/FC makes __FILE__ the whole path of this file, which is how the source
// is found) and does the sums itself.
#include <filesystem>
#include <fstream>
#include <math.h>
#include <sstream>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <vector>
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

// The top of the repository: the folder above tests/, from this file's own whole path
static std::filesystem::path Top()
{
	return std::filesystem::path(__FILE__).parent_path().parent_path();
}

// A file's text with its line ends as \n alone, or nothing if it can't be read
static std::string ReadFile(const std::filesystem::path &path)
{
	std::ifstream file(path, std::ios::binary);
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

static std::string ReadSourceFile(const char *name)
{
	return ReadFile(Top() / name);
}

static bool Has(const std::string &text, const char *what)
{
	return text.find(what) != std::string::npos;
}

// ---- Whether a line this test reads is compiled

static char const kViewTransform[] = "USE_D3DVIEWTRANSFORM";

// What an #if says about the lines under it, as far as this test can tell
enum Branch
{
	kTaken,
	kNotTaken,
	kUnknown
};

// A line with its spaces and tabs in front taken off
static std::string Trimmed(const std::string &line)
{
	std::string::size_type const first = line.find_first_not_of(" \t");
	return first == std::string::npos ? std::string() : line.substr(first);
}

// The first word of text: up to a space, a tab or a slash (a comment after the name)
static std::string FirstWord(const std::string &text)
{
	return text.substr(0, text.find_first_of(" \t/"));
}

// Whether what is compiled for sure: it's in piece (a function this test took from file) exactly once, and every #if, #ifdef and
// #ifndef still open above it is one this test can answer, and taken. Lines are read from the top of the file. A # inside a /* */
// comment would be read as a line of the preprocessor's too, which can only make this say "not for sure", never the other way.
static bool IsCompiled(const std::string &file, const std::string &piece, const std::string &what)
{
	std::string::size_type const pieceAt = piece.empty() ? std::string::npos : file.find(piece);
	std::string::size_type const inPiece = piece.find(what);

	if (pieceAt == std::string::npos || inPiece == std::string::npos || piece.find(what, inPiece + 1) != std::string::npos)
	{
		return false;
	}

	std::string::size_type const at = pieceAt + inPiece;
	std::string::size_type const lineStart = at == 0 ? 0 : (file.rfind('\n', at - 1) == std::string::npos ? 0 : file.rfind('\n', at - 1) + 1);
	std::vector<Branch> open;
	std::string::size_type start = 0;

	while (start < lineStart)
	{
		std::string::size_type end = file.find('\n', start);
		end = end == std::string::npos || end > lineStart ? lineStart : end;
		std::string const line = Trimmed(file.substr(start, end - start));
		start = end + 1;

		if (line.empty() || line[0] != '#')
		{
			continue;
		}

		std::string const words = Trimmed(line.substr(1));
		std::string const directive = words.substr(0, words.find_first_not_of("abcdefghijklmnopqrstuvwxyz"));
		std::string const name = FirstWord(Trimmed(words.substr(directive.size())));

		if (directive == "if")
		{
			open.push_back(name == "0" ? kNotTaken : (name == "1" ? kTaken : kUnknown));
		}
		else if (directive == "ifdef")
		{
			open.push_back(name == kViewTransform ? kNotTaken : kUnknown);
		}
		else if (directive == "ifndef")
		{
			open.push_back(name == kViewTransform ? kTaken : kUnknown);
		}
		else if (directive == "else" && !open.empty())
		{
			open.back() = open.back() == kTaken ? kNotTaken : (open.back() == kNotTaken ? kTaken : kUnknown);
		}
		else if (directive == "elif" && !open.empty())
		{
			open.back() = kUnknown;
		}
		else if (directive == "endif" && !open.empty())
		{
			open.pop_back();
		}
	}

	for (Branch b : open)
	{
		if (b != kTaken)
		{
			return false;
		}
	}

	return true;
}

// Where USE_D3DVIEWTRANSFORM is defined, if anywhere in 3dc or win32: a #define or #undef of it in a C or C++ file, or a project file
// that names it at all (where /D and PreprocessorDefinitions live). Empty when nowhere, which is what IsCompiled takes it to be.
static std::string WhereTheViewTransformIsDefined()
{
	for (const char *folder : { "3dc", "win32" })
	{
		std::error_code error;

		for (std::filesystem::recursive_directory_iterator entry(Top() / folder, error), done; !error && entry != done; entry.increment(error))
		{
			if (!entry->is_regular_file())
			{
				continue;
			}

			std::string const kind = entry->path().extension().string();
			bool const code = kind == ".c" || kind == ".cpp" || kind == ".h" || kind == ".hpp" || kind == ".inl";
			bool const project = kind == ".vcxproj" || kind == ".props" || kind == ".targets";

			if (!code && !project)
			{
				continue;
			}

			std::string const text = ReadFile(entry->path());

			if (project && Has(text, kViewTransform))
			{
				return entry->path().string();
			}

			std::string::size_type start = 0;

			while (code && start < text.size())
			{
				std::string::size_type const end = text.find('\n', start) == std::string::npos ? text.size() : text.find('\n', start);
				std::string const line = Trimmed(text.substr(start, end - start));
				start = end + 1;

				if (!line.empty() && line[0] == '#')
				{
					std::string const words = Trimmed(line.substr(1));

					if ((words.compare(0, 6, "define") == 0 || words.compare(0, 5, "undef") == 0) && Has(words, kViewTransform))
					{
						return entry->path().string();
					}
				}
			}
		}

		if (error)
		{
			return std::string("(") + folder + " couldn't be read through)";
		}
	}

	return std::string();
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

// ---- The source says what's worked through below, in lines the game compiles

static void TheSource()
{
	printf("The game's sums (read, not run: they need the whole game)\n");
	std::string const targeting = ReadSourceFile("3dc/avp/targeting.cpp");
	std::string const kshape = ReadSourceFile("3dc/kshape.cpp");
	std::string const render = ReadSourceFile("3dc/d3d_render.cpp");
	Check(targeting.size() > 10000 && kshape.size() > 100000 && render.size() > 100000, "the three sources were found and read");

	std::string const where = WhereTheViewTransformIsDefined();
	Check(where.empty(), "COMPILED: nothing in 3dc or win32 defines USE_D3DVIEWTRANSFORM, so what's under #ifdef USE_D3DVIEWTRANSFORM is never compiled" +
		(where.empty() ? std::string() : " (it's defined in " + where + ")"));

	std::string const sight = SourceSlice(targeting, "void SmartTarget(int speed,int projectile_speed)\n{", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	char const *const sightParts[] = { "targetView.vy*4,", "(targetView.vz*3)", "targetView.vx,", "VDBPtr->VDB_ProjX," };
	bool sightHas = true;
	bool sightCompiled = true;

	for (const char *part : sightParts)
	{
		sightHas = sightHas && Has(sight, part);
		sightCompiled = sightCompiled && IsCompiled(targeting, sight, part);
	}

	Check(sightHas, "SOURCE: the sight's point is the target's place with its height stretched to 4/3, as the 1999 game works it out");
	Check(sightCompiled, "COMPILED: and those lines of SmartTarget are compiled");

	std::string const aim = SourceSlice(targeting, "void CalculateWhereGunIsPointing(TEMPLATE_WEAPON_DATA *twPtr, PLAYER_WEAPON_DATA *weaponPtr)\n{", "\n}\n",
		kSliceEndAfterTheStart, FailedSlice);
	char const *const aimY = "GunMuzzleDirectionInVS.vy =\n\t\t(((GunMuzzleSightY-(ScreenDescriptorBlock.SDB_Height<<15))/(VDBPtr->VDB_ProjY))*3)/4;";
	char const *const aimX = "(GunMuzzleSightX-(ScreenDescriptorBlock.SDB_Width<<15))/(VDBPtr->VDB_ProjX);";
	Check(Has(aim, aimY) && Has(aim, aimX), "SOURCE: the gun's aim takes the 4/3 off the sight's height again, as the 1999 game does");
	Check(IsCompiled(targeting, aim, aimY) && IsCompiled(targeting, aim, aimX), "COMPILED: and the aim's two lines are compiled");

	std::string const triangle = SourceSlice(kshape, "void RenderPredatorTargetingSegment(int theta, int scale, int drawInRed)\n{", "\n}\n",
		kSliceEndAfterTheStart, FailedSlice);
	char const *const triangleY = "\tcentreY = MUL_FIXED((((SmartTargetSightY-(ScreenDescriptorBlock.SDB_Height<<15)) / Global_VDB_Ptr->VDB_ProjY) * 3) / 4, z);\n";
	char const *const triangleX = "\t\tcentreX = MUL_FIXED((SmartTargetSightX-(ScreenDescriptorBlock.SDB_Width<<15)) / Global_VDB_Ptr->VDB_ProjX, z);\n";
	Check(Has(triangle, triangleY) && Has(triangle, triangleX), "SOURCE: the Predator's triangle takes the 4/3 off too, for this game's projection");
	Check(IsCompiled(kshape, triangle, triangleY) && IsCompiled(kshape, triangle, triangleX),
		"COMPILED: and the triangle's two lines are compiled (the first fix's 3/4 sat under #ifdef USE_D3DVIEWTRANSFORM, and never was)");

	std::string const projection = SourceSlice(render, "void UpdateProjectionMatrix()\n{", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	char const *const projectionParts[] = { "float yScale = cot(fovY/2.0f);", "float xScale = originalYScale / aspect;", "d3d.matProjection._11 = xScale;",
		"d3d.matProjection._22 = yScale;" };
	bool projectionHas = true;
	bool projectionCompiled = true;

	for (const char *part : projectionParts)
	{
		projectionHas = projectionHas && Has(projection, part);
		projectionCompiled = projectionCompiled && IsCompiled(render, projection, part);
	}

	Check(projectionHas,
		"SOURCE: and the projection the world and the triangle go through is the one worked through here: its height fixed by the field of view, its width by the screen's shape");
	Check(projectionCompiled, "COMPILED: and the projection's lines are compiled");
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
	int const centreY = withTheThreeQuarters ? MulFixed(((centredY / projY) * 3) / 4, z) : MulFixed(centredY / projY, z);
	int const centreX = MulFixed(centredX / projX, z);
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
	setvbuf(stdout, NULL, _IONBF, 0);   // every line out at once, so a test that stops still shows how far it got
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
