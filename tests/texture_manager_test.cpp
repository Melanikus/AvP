// The test of the texture manager's slots around a lost device (3dc/TextureManager.cpp). Written 2026-11-23 by Melanikus (Frost) for the AvP port.
//
// WHAT IT TESTS
//
// When Direct3D loses its device (another program takes the screen, the window is resized, the computer sleeps), every texture that lives only on the card
// (TextureUsage_Dynamic, D3DPOOL_DEFAULT) has to be released before the device can be reset, and made again after (d3_func.cpp ReleaseVolatileResources and
// CreateVolatileResources: Tex_ReleaseDynamicTextures, then Tex_ReloadDynamicTextures). Its owner keeps its number, its texID_t, all the while. The texture
// manager hands each slot of its list to one texture at a time, and gives a slot again once it is free. These are the rules that keep a number from ever
// meaning someone else's texture (the class of bug Dietrich found in CloudyImageNumber, and Drake's first hole in the editor's live picker panes' safety case):
//   (A) a texture its owner released is never made again by the reset: its slot is free, and stays free
//   (B) one texture the reset can't make again doesn't stop the others being made again
//   (C) a slot whose texture is released only for the reset is still its owner's: a texture made in between is never given it
//   (D) the owner releasing a texture that is waiting for the reset frees its slot then, so the reset doesn't make it again
// The card is stood in: the test's own R_CreateTexture and R_ReleaseTexture make and free stand-in textures, count what was made, and can be told to fail one.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles it with the manager's own TextureManager.cpp (and the RimLoader and FileStream it reads pictures with). The program gives back a
// non-zero result if anything failed.
//
// RED FIRST: on avpx_main bb7d2b29, (A), (B), (C) and (D) all fail.

#include <stdio.h>
#include <string>
#include <vector>

#include "TextureManager.h"
#include "FastFile.h"
#include "texture_relief.h"

#ifndef _MSVC_STL_HARDENING
#error "texture_manager_test needs the standard library's bounds checks: build it with /D_MSVC_STL_HARDENING=1"
#endif

static int checksFailed = 0;

static void Check(bool passed, std::string const &what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what.c_str());

	if (!passed)
	{
		checksFailed++;
	}
}

// ---- the game's numbers for its two special textures (d3_func.cpp makes them as the device starts; the test does, below)
texID_t NO_TEXTURE = 0;
texID_t MISSING_TEXTURE = 1;

// ---- THE CARD, stood in: what R_CreateTexture made, and the one name it is told to fail
static int cardMade = 0;                     // stand-in textures made so far
static std::vector<std::string> madeNames;   // the name of each texture made, in order
static std::string failName;                 // a texture of this name can't be made

bool R_CreateTexture(uint32_t width, uint32_t height, uint32_t bitsPerPixel, enum TextureUsage usageType, Texture &texture)
{
	texture.texture = NULL;

	if (!failName.empty() && texture.name == failName)
	{
		return false;
	}

	texture.texture = new StandInCardTexture();
	texture.texture->number = ++cardMade;
	texture.bitsPerPixel = (uint8_t)bitsPerPixel;
	texture.width = (uint16_t)width;
	texture.height = (uint16_t)height;
	texture.realWidth = (uint16_t)width;
	texture.realHeight = (uint16_t)height;
	texture.usage = usageType;
	texture.isValid = true;
	madeNames.push_back(texture.name);
	return true;
}

void R_ReleaseTexture(Texture &texture)
{
	delete texture.texture;
	texture.texture = NULL;
}

void R_UnsetTexture(texID_t)
{
}

bool R_LockTexture(Texture &, uint8_t **, uint32_t *, enum TextureLock)
{
	return false;
}

bool R_UnlockTexture(Texture &)
{
	return false;
}

bool R_CreateTextureWithCopies(uint32_t, uint32_t, Texture &)
{
	return false;
}

bool R_LockTextureCopy(Texture &, uint32_t, uint8_t **, uint32_t *)
{
	return false;
}

bool R_UnlockTextureCopy(Texture &, uint32_t)
{
	return false;
}

bool R_CreateTextureFromFile(const std::string &, Texture &)
{
	return false;
}

bool R_CreateTallFontTexture(AVPTEXTURE &, enum TextureUsage, Texture &)
{
	return false;
}

bool R_CreateTextureFromAvPTexture(AVPTEXTURE &, enum TextureUsage, Texture &)
{
	return false;
}

// ---- the rest of the game the manager reaches, stood in
void Con_PrintError(const std::string &)
{
}

void Con_PrintMessage(const std::string &)
{
}

FastFileHandle *FF_Find(const std::string &)
{
	return 0;
}

const std::string &FF_GetFastFileName(int32_t)
{
	static std::string none;
	return none;
}

namespace Util
{
	void FtoBslash(std::string &text)
	{
		for (size_t i = 0; i < text.size(); i++)
		{
			text[i] = (text[i] == '/') ? '\\' : text[i];
		}
	}
}

// the relief pictures (texture_relief.cpp): the manager tells them a texture went; nothing is held here
TextureRelief::TextureRelief(size_t, size_t, size_t)
{
}

void TextureRelief::ForgetPicture(texID_t)
{
}

TextureRelief textureRelief;

// ---- the test
static bool Valid(texID_t id)
{
	return Tex_GetTextureDetails(id).isValid;
}

// a lost device as d3_func.cpp handles it: every texture that lives only on the card released, then (after the reset) made again; the names made again
// are given back
static void ReleaseForTheReset()
{
	Tex_ReleaseDynamicTextures();
}

static std::vector<std::string> MakeAgainAfterTheReset()
{
	size_t const before = madeNames.size();

	Tex_ReloadDynamicTextures();
	return std::vector<std::string>(madeNames.begin() + before, madeNames.end());
}

static std::string Names(std::vector<std::string> const &names)
{
	std::string all;

	for (std::string const &name : names)
	{
		all += (all.empty() ? "\"" : ", \"") + name + "\"";
	}

	return all.empty() ? "none" : all;
}

int main()
{
	printf("texture_manager_test: the texture manager's slots around a lost device\n");

	// the game's two special textures first, as d3_func.cpp makes them as the device starts
	StandInCardTexture *const missing = new StandInCardTexture();
	StandInCardTexture *const white = new StandInCardTexture();
	MISSING_TEXTURE = Tex_AddTexture("missing", missing, 1, 1, 32, TextureUsage_Normal);
	NO_TEXTURE = Tex_AddTexture("white", white, 1, 1, 32, TextureUsage_Normal);

	// Each check starts on slots of its own, so no other check's failure can make it pass or fail.

	// ---- (C) a slot whose texture is released only for the reset is still its owner's
	texID_t const b = Tex_Create("dynamic_b", 64, 64, 32, TextureUsage_Dynamic);
	texID_t const c = Tex_Create("dynamic_c", 64, 64, 32, TextureUsage_Dynamic);
	ReleaseForTheReset();
	texID_t const e = Tex_Create("normal_e", 32, 32, 32, TextureUsage_Normal);

	Check(e != MISSING_TEXTURE && e != b && e != c, "(C) a texture made while dynamic_b and dynamic_c wait for the reset is given another slot (it got slot " +
		std::to_string((unsigned long long)e) + "; theirs are " + std::to_string((unsigned long long)b) + " and " + std::to_string((unsigned long long)c) + ")");

	std::vector<std::string> const madeC = MakeAgainAfterTheReset();

	Check(Valid(b) && Valid(c) && Valid(e) && Tex_GetName(b) == "dynamic_b" && Tex_GetName(c) == "dynamic_c" && Tex_GetName(e) == "normal_e",
		"(C) after the reset each slot holds its own texture again: dynamic_b, dynamic_c and normal_e (slots " + std::to_string((unsigned long long)b) + ", " +
		std::to_string((unsigned long long)c) + " and " + std::to_string((unsigned long long)e) + " hold \"" + Tex_GetName(b) + "\", \"" + Tex_GetName(c) + "\" and \"" +
		Tex_GetName(e) + "\"; made again: " + Names(madeC) + ")");

	// ---- (B) one the reset can't make again doesn't stop the others
	texID_t const f = Tex_Create("dynamic_f", 64, 64, 32, TextureUsage_Dynamic);
	texID_t const g = Tex_Create("dynamic_g", 64, 64, 32, TextureUsage_Dynamic);
	ReleaseForTheReset();
	failName = "dynamic_f";
	std::vector<std::string> const madeB = MakeAgainAfterTheReset();
	failName.clear();

	Check(f < g && !Valid(f) && Valid(g), "(B) the reset can't make dynamic_f again, and still makes dynamic_g again after it (made again: " + Names(madeB) + ")");

	// ---- (D) the owner releasing one that waits for the reset frees its slot then
	ReleaseForTheReset();
	Tex_Release(g);
	std::vector<std::string> const madeD = MakeAgainAfterTheReset();
	bool madeG = false;

	for (std::string const &name : madeD)
	{
		madeG = madeG || name == "dynamic_g";
	}

	Check(!Valid(g) && !madeG, "(D) dynamic_g, released by its owner while it waited for the reset, is not made again (made again: " + Names(madeD) + ")");

	// ---- (A) released by its owner: never made again by the reset
	texID_t const a = Tex_Create("dynamic_a", 64, 64, 32, TextureUsage_Dynamic);
	Tex_Release(a);
	ReleaseForTheReset();
	std::vector<std::string> const madeA = MakeAgainAfterTheReset();
	bool madeNameless = false;

	for (std::string const &name : madeA)
	{
		madeNameless = madeNameless || name.empty();
	}

	Check(a != MISSING_TEXTURE && !Valid(a) && !madeNameless, "(A) a texture its owner released is not made again by the reset, nor anything in its slot (made again: " +
		Names(madeA) + ")");

	Tex_DeInit();

	printf(checksFailed ? "texture_manager_test: %d checks FAILED\n" : "texture_manager_test: all checks passed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
