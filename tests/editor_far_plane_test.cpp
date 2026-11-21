// The level editor's polygon view distance (R_SetEditorFarPlane, renderer.h). Written 2026-11-18 by Melanikus (Frost) for the AvP port.
//
// WHY: inteng's Construction > Options > Use Polygon View Distance (0x1f6) has the view distance cut the level polygon by polygon (FUN_00474ee0, every frame: the
// view's hazing bit 0x100, VDB_H1 = VDB_H2 = the view distance), not each object as a whole. The level editor asks the renderer for a far plane at that distance.
// The projection is made in d3d_render.cpp, which needs the whole game to run, so this reads it, and works the depth sum out itself:
// - renderer.h names the call; R_SetEditorFarPlane takes only a distance between the camera's nearest and the game's farthest (anything else, NaN too, is the game's own);
// - UpdateProjectionMatrix keeps the game's own sum and, only while the editor asks, puts the editor's far plane in its place;
// - that sum gives depth 0 at the nearest and 1 at the far plane, so what is past the distance is cut away by the card.

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include "source_slice.h"

static int checksFailed = 0;

static void Check(bool passed, const std::string &what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what.c_str());

	if (!passed)
	{
		checksFailed++;
	}
}

static std::string ReadSource(const char *name)
{
	FILE *file = fopen(name, "rb");
	std::string text;

	if (!file)
	{
		return text;
	}

	char block[4096];
	size_t got = 0;

	while ((got = fread(block, 1, sizeof(block), file)) > 0)
	{
		for (size_t i = 0; i < got; i++)
		{
			if (block[i] != '\r')
			{
				text += block[i];
			}
		}
	}

	fclose(file);
	return text;
}

static void FailedSlice(const std::string &why)
{
	Check(false, why);
}

static bool Has(const std::string &text, const char *what)
{
	return text.find(what) != std::string::npos;
}

// The depth the card gets for a point this far ahead, from the projection's third column as UpdateProjectionMatrix makes it
static double DepthAt(double z, double nearest, double farthest)
{
	double const m33 = farthest / (farthest - nearest);
	double const m43 = -nearest * farthest / (farthest - nearest);
	return (m33 * z + m43) / z;
}

int main()
{
	puts("The level editor's far plane, by reading the renderer:");

	std::string const header = ReadSource("3dc/renderer.h");
	std::string const render = ReadSource("3dc/d3d_render.cpp");
	Check(header.size() > 10000 && render.size() > 100000, "renderer.h and d3d_render.cpp were found and read, from the top of the repository");

	Check(Has(header, "\nvoid R_SetEditorFarPlane\t\t(float millimetres);\n"), "renderer.h names R_SetEditorFarPlane");
	Check(Has(render, "\nstatic float editorFarPlane = 0.0f;\n"), "it starts as the game's own far plane");

	std::string const setter = SourceSlice(render, "void R_SetEditorFarPlane(float millimetres)\n{", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(setter, "editorFarPlane = (millimetres > zNear && millimetres < zFar) ? millimetres : 0.0f;"),
		"it takes only a distance between the camera's nearest and the game's farthest: anything else, NaN as well, gives the game's own back");

	std::string const projection = SourceSlice(render, "void UpdateProjectionMatrix()\n{", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	size_t const gameSum = projection.find("d3d.matProjection._43 = -zNear*zFar/(zFar-zNear);");
	size_t const editorSum = projection.find("if (editorFarPlane > 0.0f)");
	Check(gameSum != std::string::npos && Has(projection, "d3d.matProjection._33 = zFar/(zFar-zNear);"), "the projection keeps the game's own depth sum");
	Check(editorSum != std::string::npos && editorSum > gameSum && Has(projection, "d3d.matProjection._33 = editorFarPlane/(editorFarPlane-zNear);") &&
		Has(projection, "d3d.matProjection._43 = -zNear*editorFarPlane/(editorFarPlane-zNear);"), "and only while the editor asks, after it, puts the editor's far plane in its place");

	// The sum itself: depth 0 at the nearest, 1 at the far plane, past 1 beyond it (cut away)
	double const nearest = 64.0, farthest = 10000.0;
	Check(fabs(DepthAt(nearest, nearest, farthest)) < 1e-9 && fabs(DepthAt(farthest, nearest, farthest) - 1.0) < 1e-9, "the sum gives depth 0 at the nearest and 1 at a 10 m far plane");
	Check(DepthAt(farthest * 1.01, nearest, farthest) > 1.0 && DepthAt(farthest * 0.99, nearest, farthest) < 1.0, "so a point past 10 m is cut away and one short of it is drawn");

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
