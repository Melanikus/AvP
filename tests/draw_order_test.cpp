// The test of what's drawn in front of what: the stars behind the level (#176) and the coronas behind the gun (#178).
// Written 2026-11-09 by Melanikus for the AvP port. Both were reported by Bobblen147.
//
// WHAT IT TESTS
//
// The stars: particle_depth.h's ParticleDepth_AtTheBack, on the game's own numbers. A star sits on a ball about 65 metres round the camera.
// Moved out to the back, it must land on exactly the same pixel, it must not be cut off by the far edge of what the camera draws, and every
// cliff nearer than where it's moved to must be in front of it, by more than the depth picture can tell apart. A corner the card won't draw
// anyway must be left where it is. Then, reading the game's source as text, that the stars really go through it: they're marked "drawn at
// the back" in particle.cpp's table, they reach D3D_Particle_Output, and that moves all three of each corner's numbers. The camera's
// nearest, farthest and depth sum this test uses are read from d3d_render.cpp too, so the two can't drift apart.
//
// The coronas, all by reading: every corona goes on its own screen list, not the HUD's, and a frame draws that list after the particles
// and before the gun, whose drawing clears the depth and covers it. The HUD comes after the gun, over everything.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this file on its own. The renderer itself brings most of the game with it, so the parts of it checked here are
// read, not run. The program gives back a non-zero result if anything failed.

#include <float.h>
#include <fstream>
#include <math.h>
#include <sstream>
#include <stdio.h>
#include <string>

#include "particle_depth.h"
#include "source_slice.h"

static int checksFailed = 0;

static void Check(bool passed, std::string const &what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what.c_str());

	if (!passed)
	{
		checksFailed++;
	}
}

static void FailedSlice(std::string const &why)
{
	Check(false, why);
}

// A file of the repository as text, with Windows line ends taken out, since a copy of the repository can have them or not
static std::string ReadSourceFile(char const *name)
{
	// /FC in this test's settings makes __FILE__ the whole path of this file, so the repository's top is found from it
	std::string path = __FILE__;
	std::string::size_type const tests = path.rfind("tests");

	if (tests == std::string::npos)
	{
		return "";
	}

	std::ifstream file((path.substr(0, tests) + name).c_str(), std::ios::binary);
	std::stringstream all;
	all << file.rdbuf();

	std::string text = all.str();
	std::string plain;

	for (char c : text)
	{
		if (c != '\r')
		{
			plain += c;
		}
	}

	return plain;
}

static bool Has(std::string const &text, std::string const &what)
{
	return text.find(what) != std::string::npos;
}

// The camera's nearest and farthest, as d3d_render.cpp has them. The source checks below make sure these are the game's own.
static const float kNearest = 64.0f;
static const float kFarthest = 1000000.0f;

// The depth the graphics card gives a point this far in front of the camera: 0 at the nearest, 1 at the farthest. It's the camera's own sum
// (UpdateProjectionMatrix in d3d_render.cpp), in floats as the game works it out. Anything past 1 is cut off.
static float DepthOf(float distance)
{
	float const scale = kFarthest / (kFarthest - kNearest);
	float const shift = -kNearest * kFarthest / (kFarthest - kNearest);
	return (distance * scale + shift) / distance;
}

// How far apart two depths have to be for the depth picture to tell them apart: it keeps 24 bits
static const float kOneDepthStep = 1.0f / 16777216.0f;

static void TestTheStars()
{
	puts("A star moved out to the back:");

	// A star on its ball, straight ahead and a little to the side, with a corner as RenderStarfield makes them: 256 either way
	float const starDistance = 65536.0f;
	float const starSide = 3000.0f + 256.0f;
	float const starUp = -1200.0f - 256.0f;

	float const out = ParticleDepth_AtTheBack(starDistance, kNearest, kFarthest);
	float const movedDistance = starDistance * out;

	Check(DepthOf(100000.0f) > DepthOf(starDistance), "where it was, at 65 metres, a cliff 100 metres away was behind it: that's #176");
	Check(fabsf(movedDistance - kFarthest * kParticleAtTheBackShare) < 1.0f, "it's moved out to 99% of the farthest the camera draws, 990 metres");
	Check(fabsf((starSide * out) / movedDistance - starSide / starDistance) < 1e-6f &&
		fabsf((starUp * out) / movedDistance - starUp / starDistance) < 1e-6f,
		"and lands on exactly the same pixel, across and up, because all three of its numbers are moved together");

	float const starDepth = DepthOf(movedDistance);
	Check(starDepth < 1.0f - 4.0f * FLT_EPSILON, "its depth stays clear of the far edge by more than rounding can carry it, so it's never cut off");

	bool allInFront = true;
	float const cliffs[] = { 100.0f, 300.0f, 650.0f, 900.0f, 980.0f };

	for (float metres : cliffs)
	{
		allInFront = allInFront && DepthOf(metres * 1000.0f) + kOneDepthStep < starDepth;
	}

	Check(allInFront, "a cliff 100, 300, 650, 900 or 980 metres away is in front of it, by more than the depth picture can tell apart");

	float const tooFar = 2000000.0f;
	Check(DepthOf(tooFar * ParticleDepth_AtTheBack(tooFar, kNearest, kFarthest)) < 1.0f - 4.0f * FLT_EPSILON,
		"a corner further than the camera draws comes in to the back too, and is drawn, as 1999's \"at the back\" was");
	Check(ParticleDepth_AtTheBack(kNearest, kNearest, kFarthest) > 1.0f, "a corner at the camera's nearest is moved out like any other");
}

static void TestWhatsLeftAlone()
{
	puts("A corner the card won't draw anyway:");

	Check(ParticleDepth_AtTheBack(63.9f, kNearest, kFarthest) == 1.0f, "nearer than the camera's nearest: left where it is, for the card to cut off");
	Check(ParticleDepth_AtTheBack(0.0f, kNearest, kFarthest) == 1.0f, "at the camera itself: left where it is, and nothing is divided by 0");
	Check(ParticleDepth_AtTheBack(-65536.0f, kNearest, kFarthest) == 1.0f,
		"behind the camera: left where it is, never turned round to the front, where it would be drawn wrongly");
	Check(ParticleDepth_AtTheBack(NAN, kNearest, kFarthest) == 1.0f, "not a number at all: left where it is");
}

// The game's source, read as text: the parts of the renderer that need the whole game to run
static void TestTheGameUsesIt()
{
	puts("Where the game moves the stars, by reading:");

	std::string const render = ReadSourceFile("3dc/d3d_render.cpp");
	std::string const particles = ReadSourceFile("3dc/avp/particle.cpp");
	std::string const shapes = ReadSourceFile("3dc/kshape.cpp");
	Check(render.size() > 100000 && particles.size() > 50000 && shapes.size() > 50000, "the renderer, the particles and the shapes were found and read, from the top of the repository");

	Check(Has(render, "\nconst float zNear = 64.0f;\nconst float zFar  = 1000000.0f;\n"), "the camera's nearest and farthest are the ones this test uses");
	// Each function is found by its first line and its opening brace, so a line that only names it can't be taken for it
	std::string const projection = SourceSlice(render, "void UpdateProjectionMatrix()\n{", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(projection, "d3d.matProjection._33 = zFar/(zFar-zNear);") && Has(projection, "d3d.matProjection._43 = -zNear*zFar/(zFar-zNear);") &&
		Has(projection, "d3d.matProjection._34 = 1.0f;") && Has(projection, "d3d.matProjection._44 = 0.0f;"), "and so is the camera's depth sum");

	std::string const output = SourceSlice(render, "static void D3D_Particle_Output(PARTICLE *particlePtr, PARTICLEVERTEX *renderVerticesPtr)\n{", "\n}\n",
		kSliceEndAfterTheStart, FailedSlice);
	Check(Has(output, "float const outToTheBack = particleDescPtr->IsDrawnAtBack ? ParticleDepth_AtTheBack(vertices->z, zNear, zFar) : 1.0f;"),
		"each particle's corner is moved out only when the particle is drawn at the back, by the camera's own nearest and farthest");
	Check(Has(output, "particleVertex[pVb].x = vertices->x * outToTheBack;") && Has(output, "particleVertex[pVb].y = -vertices->y * outToTheBack;") &&
		Has(output, "particleVertex[pVb].z = vertices->z * outToTheBack;"), "and all three of its numbers are moved together, so it stays on the same pixel");

	std::string const drawn = SourceSlice(render, "void DrawParticles()\n{", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(drawn, "D3D_Particle_Output(&particleBucket[i][j].particle, &particleBucket[i][j].vertices[0]);"), "every particle collected in a frame goes through it");

	std::string const star = SourceSlice(particles, "/* PARTICLE_STAR */", "/* PARTICLE_", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(star, "//IsDrawnInFront:1;\n\t\t0,\n\t\t//IsDrawnAtBack:1;\n\t\t1,\n\t},"), "the star is marked drawn at the back, and not in front");

	std::string const starfield = SourceSlice(shapes, "void RenderStarfield()\n{", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(starfield, "particle.ParticleID = PARTICLE_STAR;") && Has(starfield, "AddParticle(&particle, VerticesBuffer);"),
		"and the starry sky is drawn as stars, collected with the other particles");
}

// The coronas (#178): the glow round a light, which 1999 drew as a light flare in front of the level but before the gun. They had gone on
// the HUD's list, which is drawn last of all, so they showed through the gun and over the HUD.
static void TestTheCoronas()
{
	puts("Where the game draws the coronas, by reading:");

	std::string const render = ReadSourceFile("3dc/d3d_render.cpp");
	std::string const particles = ReadSourceFile("3dc/avp/particle.cpp");
	std::string const shapes = ReadSourceFile("3dc/kshape.cpp");
	std::string const setup = ReadSourceFile("win32/d3_func.cpp");
	Check(render.size() > 100000 && setup.size() > 50000, "the renderer and the graphics card's setup were found and read");

	// What a corona is: a light flare, which 1999 drew in front of the level. A corona is a flat square on the screen, and the screen's
	// projection puts its depth of 1 at the very front, so nothing the level draws can hide it.
	std::string const flare = SourceSlice(particles, "/* PARTICLE_LIGHTFLARE */", "/* PARTICLE_", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(flare, "//IsDrawnInFront:1;\n\t\t1,\n\t\t//IsDrawnAtBack:1;\n\t\t0,"), "the light flare is marked drawn in front, as 1999 drew it");
	std::string const lightFlare = SourceSlice(shapes, "void RenderLightFlare(VECTORCH *positionPtr, uint32_t colour)\n{", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(lightFlare, "particle.ParticleID = PARTICLE_LIGHTFLARE;") && Has(lightFlare, "AddCorona(&particle, &tempVector);"), "and a light's flare is drawn as a corona");
	Check(Has(setup, "D3DXMatrixOrthoLH(&d3d.matOrtho, 2.0f, -2.0f, 1.0f, 10.0f);"), "the screen's projection runs its depth from 1, at the very front, to 10");

	// Its own list, on the screen list's buffers, made, opened and let go of with the others
	Check(Has(render, "\tcoronaList   = new RenderList(\"coronaList\",   50,  &d3d.orthoVB, &d3d.orthoIB, &d3d.orthoDecl);\n"),
		"the coronas have a screen list of their own, on the screen list's buffers");
	std::string const open = SourceSlice(render, "static bool LockExecuteBuffer()\n{", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(open, "\torthoList->Reset();\n\tcoronaList->Reset();\n"), "it's emptied with the screen list whenever the lists are opened");
	std::string const letGo = SourceSlice(render, "void RenderListDeInit()\n{", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(letGo, "delete coronaList;\n\tcoronaList = 0;"), "and let go of with the other lists");

	std::string const coronas = SourceSlice(render, "void DrawCoronas()\n{", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(coronas, "coronaList->AddCommand(kCommandZWriteDisable);") && Has(coronas, "if (!coronaList->AddItem(4, SpecialFXImageNumber,") &&
		Has(coronas, "coronaList->CreateOrthoIndices(orthoIndex);") && Has(coronas, "coronaList->AddCommand(kCommandZWriteEnable);"),
		"every corona goes on that list, with depth writes off round them as before");
	Check(!coronas.empty() && !Has(coronas, "orthoList"), "and none on the HUD's list, which is drawn after the gun");
	Check(Has(coronas, "orthoVertex[orthoVBOffset].z = 1.0f;"), "each corner is at the screen's depth of 1, the very front");

	// The order a frame is drawn in
	std::string const execute = SourceSlice(render, "static bool ExecuteBuffer()\n{", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	size_t const particlesDrawn = execute.find("particleList->Draw();");
	size_t const coronasDrawn = execute.find("coronaList->Draw();");
	size_t const depthCleared = execute.find("R_ClearZBuffer();");
	size_t const gunDrawn = execute.find("weaponList->Draw();");
	size_t const hudDrawn = execute.find("orthoList->Draw();");
	Check(particlesDrawn != std::string::npos && coronasDrawn != std::string::npos && depthCleared != std::string::npos &&
		gunDrawn != std::string::npos && hudDrawn != std::string::npos,
		"a frame's drawing draws the particles, the coronas, the gun and the HUD");
	Check(particlesDrawn < coronasDrawn && coronasDrawn < depthCleared && depthCleared < gunDrawn && gunDrawn < hudDrawn,
		"in that order: the coronas after the particles, then the gun's depth is cleared and the gun covers them, and the HUD goes over all of it");
	Check(execute.find("coronaList->Draw();", coronasDrawn + 1) == std::string::npos, "the coronas are drawn once a drawing, not twice");

	std::string const coronaDraw = SourceSlice(execute, "if (coronaList->GetSize())", "coronaList->Draw();", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(coronaDraw, "d3d.effectSystem->SetActive(d3d.orthoEffect);") &&
		Has(coronaDraw, "d3d.effectSystem->SetVertexShaderConstant(d3d.orthoEffect, 0, CONST_MATRIX, &d3d.matOrtho);"),
		"with the screen's own shaders and projection, as the HUD's list is");
}

int main()
{
	TestTheStars();
	TestWhatsLeftAlone();
	TestTheGameUsesIt();
	TestTheCoronas();

	if (checksFailed)
	{
		printf("%d checks FAILED\n", checksFailed);
		return 1;
	}

	printf("All checks passed\n");
	return 0;
}
