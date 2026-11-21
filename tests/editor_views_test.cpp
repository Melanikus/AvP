// The level editor's views for the live plot (renderer.h): world lines per view, a view drawn after the main view, and its box cleared of depth as well as colour.
// Written 2026-11-21 by Melanikus (Frost) for the AvP port.
//
// WHY (the owner, 2026-11-21): "I want to see the modules updating on the map as they are build and the little image should show progress as volumes are being placed
// on it". While Alien auto waypoint plots, the editor draws its main view with the waypoint lines of every module written so far, then a small inset view drawn AFTER
// it, with the module being planned. Until now the editor's lines were one list drawn only in a frame's last drawing, so the main view lost its lines whenever the
// inset was drawn last, and nothing cleared the main view's depth out of the inset's box, so the main view's walls could hide the inset's.
//
// WHAT THIS TESTS, by reading renderer.h and d3d_render.cpp (the drawing needs the whole game and a graphics card):
// - renderer.h names the four calls; slot 0 is R_SetEditorWorldLines' own list, so the editor's existing calls draw as before;
// - each slot is a list of its own with its own copy on the card, and a slot outside 0..1 is refused;
// - a drawing gives lines only to the slot it was asked for: the frame's last drawing to R_SetEditorFinalLinesSlot's (0 at the start, as before), an
//   R_EditorFlushWorldWithLines drawing to its own, and the game's own early drawings (a full list) to none;
// - R_EditorClearBox clears colour AND depth, only in an open frame, only inside the window's viewport;
// - THE GAME IS UNCHANGED: none of its own files names the new calls, so its lines stay empty and its drawings take the same paths.
// Run it from the top of the repository, because that's where the source it reads is found from.

#include <stdio.h>
#include <string.h>
#include <filesystem>
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

static std::string ReadSource(const std::string &name)
{
	FILE *file = fopen(name.c_str(), "rb");
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

static int Count(const std::string &text, const char *what)
{
	int found = 0;

	for (size_t at = text.find(what); at != std::string::npos; at = text.find(what, at + 1))
	{
		found++;
	}

	return found;
}

int main()
{
	puts("The level editor's views for the live plot, by reading the renderer:");

	std::string const header = ReadSource("3dc/renderer.h");
	std::string const render = ReadSource("3dc/d3d_render.cpp");
	Check(header.size() > 10000 && render.size() > 100000, "renderer.h and d3d_render.cpp were found and read, from the top of the repository");

	// the four calls
	Check(Has(header, "\nvoid R_SetEditorWorldLinesSlot\t(int slot, const EditorWorldLine *lines, int count);\n"), "renderer.h names R_SetEditorWorldLinesSlot");
	Check(Has(header, "\nvoid R_EditorFlushWorldWithLines\t(int slot);\n"), "and R_EditorFlushWorldWithLines");
	Check(Has(header, "\nvoid R_SetEditorFinalLinesSlot\t(int slot);\n"), "and R_SetEditorFinalLinesSlot");
	Check(Has(header, "\nvoid R_EditorClearBox\t\t\t(int x, int y, int width, int height, unsigned int colour);\n"), "and R_EditorClearBox");
	Check(Has(header, "\nvoid R_SetEditorWorldLines\t\t(const EditorWorldLine *lines, int count);\n") && Has(header, "\nvoid R_EditorFlushWorld\t\t\t(void);\n"),
		"the calls the editor already makes are still there, as they were");

	// the slots
	Check(Has(render, "\nstatic const int kEditorLineSlots = 2;"), "two slots: 0, R_SetEditorWorldLines' own list, and 1, for a view drawn after the main view");
	std::string const slotType = SourceSlice(render, "struct EditorLineSlot\n{", "\n};\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(slotType, "std::vector<EditorWorldLine> lines;") && Has(slotType, "IDirect3DVertexBuffer9 *buffer;") && Has(slotType, "bool changed;"),
		"each slot keeps its own lines and its own copy on the card, so one slot's lines changing never sends the other's again");

	std::string const oldSetter = SourceSlice(render, "void R_SetEditorWorldLines(const EditorWorldLine *lines, int count)\n{", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(oldSetter, "\tR_SetEditorWorldLinesSlot(0, lines, count);") && Count(oldSetter, ";") == 1, "R_SetEditorWorldLines is slot 0, and only that: the editor's existing lines draw as before");

	std::string const setter = SourceSlice(render, "void R_SetEditorWorldLinesSlot(int slot, const EditorWorldLine *lines, int count)\n{", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(setter.find("if (slot < 0 || slot >= kEditorLineSlots)\n\t{\n\t\treturn;\n\t}") == 0 || setter.find("\tif (slot < 0 || slot >= kEditorLineSlots)\n\t{\n\t\treturn;\n\t}") != std::string::npos,
		"a slot outside 0..1 is refused before anything is read or kept");
	Check(Has(setter, "kEditorWorldLinesMost"), "and a slot keeps at most kEditorWorldLinesMost lines, as the one list did");

	// which drawing gives lines to which slot
	Check(Has(render, "\nstatic int editorFinalLinesSlot = 0;"), "the frame's last drawing gives its lines to slot 0 until the editor says otherwise, as before");
	Check(Has(render, "\nstatic int editorLinesThisFlush = -1;"), "and an early drawing gives lines to none unless R_EditorFlushWorldWithLines asks");
	std::string const execute = SourceSlice(render, "static void ExecuteBuffer()\n{", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(execute, "int const linesSlot = toyFinalFlush ? editorFinalLinesSlot : editorLinesThisFlush;") &&
		Has(execute, "if (linesSlot >= 0 && linesSlot < kEditorLineSlots && !editorLineSlots[linesSlot].lines.empty())") &&
		Has(execute, "DrawEditorWorldLines(editorLineSlots[linesSlot]);"),
		"a drawing draws only its own slot's lines, and none when that slot is empty (the game never sets lines)");
	Check(!Has(execute, "if (toyFinalFlush && !editorWorldLines.empty())"), "the one list of before is gone from the drawing");

	std::string const flushLists = SourceSlice(render, "static void FlushLists()\n{", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(!Has(flushLists, "editorLinesThisFlush"), "the game's own early drawing (a list full in the middle of a frame) never asks for lines");
	Check(Count(render, "editorLinesThisFlush = ") == 3, "it starts at -1, and only R_EditorFlushWorldWithLines sets the early drawing's slot, and puts it back");
	std::string const flushWith = SourceSlice(render, "void R_EditorFlushWorldWithLines(int slot)\n{", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	size_t const asks = flushWith.find("editorLinesThisFlush = slot;");
	size_t const flushes = flushWith.find("FlushLists();");
	size_t const back = flushWith.find("editorLinesThisFlush = -1;");
	Check(Has(flushWith, "if (slot < 0 || slot >= kEditorLineSlots)") && asks != std::string::npos && flushes > asks && back != std::string::npos && back > flushes,
		"R_EditorFlushWorldWithLines refuses a slot outside 0..1, asks for its slot, draws, and asks for none again");
	std::string const flushWorld = SourceSlice(render, "void R_EditorFlushWorld()\n{", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(flushWorld, "\tFlushLists();") && Count(flushWorld, ";") == 1, "R_EditorFlushWorld is as it was: everything waiting drawn, no lines");

	std::string const finalSlot = SourceSlice(render, "void R_SetEditorFinalLinesSlot(int slot)\n{", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(finalSlot, "if (slot >= -1 && slot < kEditorLineSlots)"), "R_SetEditorFinalLinesSlot takes -1 (none) or a slot, and nothing else");

	// the box cleared of depth as well as colour
	std::string const clear = SourceSlice(render, "void R_EditorClearBox(int x, int y, int width, int height, unsigned int colour)\n{", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(clear, "if (!frameIsOpen || !device || width <= 0 || height <= 0)"), "R_EditorClearBox does nothing outside an open frame, or for an empty box");
	Check(Has(clear, "D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER"), "it clears colour AND depth, so the main view's walls can't hide the view drawn after it");
	Check(Has(clear, "GetViewport(&whole)") && Has(clear, "device->Clear(1, &box,"), "one box, kept inside the window's viewport");

	// the game is unchanged: none of its own files names the new calls
	const char *const newCalls[4] = { "R_SetEditorWorldLinesSlot", "R_EditorFlushWorldWithLines", "R_SetEditorFinalLinesSlot", "R_EditorClearBox" };
	int gameFiles = 0, named = 0;
	std::string namedWhere;

	for (const char *const folder : { "3dc", "win32" })
	{
		for (const std::filesystem::directory_entry &entry : std::filesystem::recursive_directory_iterator(folder))
		{
			std::string const path = entry.path().generic_string();
			std::string const kind = entry.path().extension().string();

			if (!entry.is_regular_file() || (kind != ".cpp" && kind != ".c" && kind != ".h" && kind != ".hpp") || path == "3dc/d3d_render.cpp" || path == "3dc/renderer.h")
			{
				continue;
			}

			gameFiles++;
			std::string const text = ReadSource(path);

			for (const char *const call : newCalls)
			{
				if (Has(text, call))
				{
					named++;
					namedWhere += " " + path + ":" + call;
				}
			}
		}
	}

	Check(gameFiles > 500 && named == 0, "none of the game's own files names the new calls (" + std::to_string(gameFiles) + " read)" + namedWhere);

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
