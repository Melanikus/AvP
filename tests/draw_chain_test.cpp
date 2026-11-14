// The test of R_DrawScreenToChain (3dc/d3d_render.cpp, R-DRAW-CHAIN-1). Written 2026-11-14 by Melanikus (Apone) for the AvP port.
//
// WHAT IT'S FOR: the map editor, AVP_OpenInteng, draws its panels with the game's own text and menu drawing into a swap chain of its
// own, between the game's frames. The game never calls this; the editor does. Whatever it changes on the game's device and in the
// game's screen settings, the game's next frame has to find as it was: the render target, the depth buffer, the viewport, the screen
// block and the orthographic matrix. And it must never take or wipe the game's own quads, so it only draws with no frame open and the
// screen list empty.
//
// HOW: read, not run. The function needs the game's real device, so this reads its source: it checks what refuses to draw, that
// nothing gives back between saving the game's settings and putting them back, that each one is put back, and that every surface it
// asks the device for is let go of. tests/takeouts/draw_chain_test.json takes each of those out in turn, and each must fail here.

#include <stdio.h>
#include <string.h>
#include <string>
#include "source_slice.h"

static int checksFailed = 0;

static void Check(bool passed, const char *what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what);

	if (!passed)
	{
		checksFailed++;
	}
}

// A source file, as text, from the top of the repository, with its CRs taken out. The names are this file's own, never from outside.
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
	Check(false, why.c_str());
}

static bool Has(const std::string &text, const char *what)
{
	return text.find(what) != std::string::npos;
}

// How many times what is in text
static size_t Count(const std::string &text, const char *what)
{
	size_t count = 0;

	for (std::string::size_type at = text.find(what); at != std::string::npos; at = text.find(what, at + 1))
	{
		count++;
	}

	return count;
}

static void TestTheFunction()
{
	puts("R_DrawScreenToChain, read:");

	std::string const source = ReadSource("3dc/d3d_render.cpp");
	std::string const function = SourceSlice(source, "bool R_DrawScreenToChain(", "\n}\n", kSliceEndAfterTheStart, FailedSlice);

	// What refuses to draw
	std::string const refusal = SourceSlice(function, "if (!chain", "return false;", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(refusal, "|| frameIsOpen"), "it draws nothing with one of the game's frames open");
	Check(Has(refusal, "|| orthoList->GetSize() != 0"), "or with anything already on the screen list, so the game's own quads are never taken or wiped");
	Check(Has(function, "uint32_t const mostSide = 16384;") && Has(refusal, "width < 1 || width > mostSide || height < 1 || height > mostSide"),
		"or at a width or height outside 1 to 16384");

	// From saving the game's settings to putting them back, there's one way out, at the end
	std::string const between = SourceSlice(function, "// What's put back.", "// Putting back, on every path", kSliceEndAfterTheStart, FailedSlice);
	Check(!between.empty() && !Has(between, "return"), "nothing gives back between saving the game's settings and putting them back");
	// (The slice stops just before the function's closing brace, so its last words are its last line.)
	std::string::size_type const lastReturn = function.rfind("\treturn presented;");
	Check(Count(function, "return") == 2 && lastReturn != std::string::npos && lastReturn + strlen("\treturn presented;") == function.size(),
		"it gives back only from the refusal, and at the very end");

	// Each saved before it's changed
	std::string::size_type const screenSaved = function.find("SCREENDESCRIPTORBLOCK const gameScreen = ScreenDescriptorBlock;");
	std::string::size_type const screenChanged = function.find("ScreenDescriptorBlock.SDB_");
	Check(screenSaved != std::string::npos && screenChanged != std::string::npos && screenSaved < screenChanged,
		"the game's screen block is kept before it's changed for the chain");
	Check(Has(between, "D3DXMATRIX const gameOrtho = d3d.matOrtho;") && Has(between, "GetViewport(&gameViewport)")
		&& Has(between, "GetRenderTarget(0, &gameTarget)") && Has(between, "GetDepthStencilSurface(&gameDepth)"),
		"and so are its orthographic matrix, viewport, render target and depth buffer");

	// Each put back, on every path
	std::string const back = SourceSlice(function, "// Putting back, on every path", "\treturn presented;", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(back, "ScreenDescriptorBlock = gameScreen;"), "the screen block is put back");
	Check(Has(back, "d3d.matOrtho = gameOrtho;"), "the orthographic matrix is put back");
	Check(Has(back, "SetRenderTarget(0, gameTarget);"), "the render target is put back");
	Check(Has(back, "SetDepthStencilSurface(gameDepth);"), "the depth buffer is put back");
	Check(Has(back, "SetViewport(&gameViewport);"), "the viewport is put back");

	// Every surface it asked for is let go of
	Check(Has(back, "gameTarget->Release();"), "the render target it asked for is let go of");
	Check(Has(back, "gameDepth->Release();"), "the depth buffer it asked for is let go of");
	Check(Has(back, "chainBuffer->Release();"), "the chain's back buffer it asked for is let go of");

	// The screen list's buffers are closed again, and the list given back empty, before the scene ends
	std::string const drawing = SourceSlice(function, "fill(context);", "EndScene()", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(drawing, "d3d.orthoVB->Unlock();") && Has(drawing, "d3d.orthoIB->Unlock();"), "the screen list's buffers are closed after the panels are added");
	std::string const afterDraw = SourceSlice(function, "orthoList->Draw();", "EndScene()", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(afterDraw, "orthoList->Reset();") && Has(afterDraw, "orthoVBOffset = 0;"), "and the list is the game's again, empty, as its next frame expects");
}

int main()
{
	// Print each line straight away
	setvbuf(stdout, NULL, _IONBF, 0);

	TestTheFunction();

	if (checksFailed)
	{
		printf("%d checks FAILED\n", checksFailed);
		return 1;
	}

	puts("All checks passed");
	return 0;
}
