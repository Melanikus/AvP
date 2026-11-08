// Tests for how big the HUD is drawn (card HUD-SCALE-1). The first part tries hud_scale.cpp, which knows nothing about the game: the scale
// for each screen and HUD size, the slider, the settings file's word, which is hostile, and the alien's bar. The second part reads the
// game's own files, to see that everything that draws the HUD takes the scale from that one place. Written 2026-11-08 by Melanikus for
// the AvP port. Reported by Bobblen; follows his avp-forever (commit a53f482), rewritten.
#include "hud_scale.h"

#include <climits>
#include <fstream>
#include <sstream>
#include <stdio.h>
#include <string>

static int checksFailed = 0;

static void Check(bool good, const std::string &what)
{
	printf("%s  %s\n", good ? "ok    " : "FAILED", what.c_str());
	checksFailed += good ? 0 : 1;
}

static std::string ReadSourceFile(char const *name)
{
	// /FC in this test's settings makes __FILE__ the whole path of this file, so everything else can be found from it
	std::string path = __FILE__;
	std::string::size_type const tests = path.rfind("tests");

	if (tests == std::string::npos)
	{
		return "";
	}

	std::ifstream file((path.substr(0, tests) + name).c_str(), std::ios::binary);
	std::stringstream all;
	all << file.rdbuf();

	// Files here have Windows' line ends, and what's looked for is written with plain ones
	std::string text = all.str(), plain;

	for (char const c : text)
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

static int CountOf(const std::string &text, const char *what)
{
	int count = 0;

	for (std::string::size_type at = text.find(what); at != std::string::npos; at = text.find(what, at + 1))
	{
		count++;
	}

	return count;
}

static std::string Between(const std::string &text, const char *from, const char *to)
{
	std::string::size_type const start = text.find(from);
	std::string::size_type const end = start == std::string::npos ? start : text.find(to, start);

	return (start == std::string::npos || end == std::string::npos) ? std::string() : text.substr(start, end - start);
}

static bool ComesBefore(const std::string &text, const char *first, const char *second)
{
	std::string::size_type const a = text.find(first), b = text.find(second);

	return a != std::string::npos && b != std::string::npos && a < b;
}

// The game's own fixed-point sums (mathline.cpp), written out here so the old numbers can be worked out the way the game did
static int MulFixed(int a, int b)
{
	return static_cast<int>(((static_cast<long long>(a) * b) >> 16) & 0xFFFFFFFF);
}

static int DivFixed(int a, int b)
{
	return static_cast<int>(((static_cast<long long>(a) << 16) / b) & 0xffffffff);
}

// The screens the card is about, and a few more of other shapes
struct Screen
{
	int width;
	int height;
};

static const Screen screens[] =
{
	{ 640, 480 }, { 800, 600 }, { 1024, 768 }, { 1280, 720 }, { 1280, 1024 }, { 1920, 1080 }, { 1920, 1200 }, { 2560, 1440 }, { 3440, 1440 },
	{ 3840, 2160 }, { 5120, 1440 }, { 768, 1024 }
};

static void TestTheScale()
{
	// The user's ruling: the usual size is the HUD a 1280 by 1024 screen has always had, which was width / 640
	Check(HUDScale_ForScreen(1024, kHUDSizeUsual) == DivFixed(1280, 640), "at the usual size, a 1280 by 1024 screen has exactly the old HUD");
	Check(HUDScale_ForScreen(1024, kHUDSizeUsual) == 131072, "which is twice its size on a 640 by 480 screen");
	Check(HUDScale_ForScreen(768, kHUDSizeUsual) == 98304, "1024 by 768 is 1.5, where it was 1.6");
	Check(HUDScale_ForScreen(480, kHUDSizeUsual) == 61440, "640 by 480 is 0.9375");
	Check(HUDScale_ForScreen(1440, kHUDSizeUsual) == 184320, "3440 by 1440 is 2.8125, where it was 5.375");

	// It follows the height, not the width, at every height there is: 0.9375 times height / 480 is height * 128, exactly
	bool followsHeight = true;

	for (int height = 1; height <= kHUDTallestScreen; height++)
	{
		followsHeight = followsHeight && HUDScale_ForScreen(height, kHUDSizeUsual) == height * 128;
	}

	Check(followsHeight, "at the usual size, every height from 1 to the tallest gives height * 128");

	// The player's size is a percentage of the usual one, rounded down once, at the end
	Check(HUDScale_ForScreen(1024, 50) == 65536, "50% is half the usual size");
	Check(HUDScale_ForScreen(1024, 150) == 196608, "150% is half as big again");
	Check(HUDScale_ForScreen(1081, 110) == 152204, "a size that doesn't come out whole is rounded down: 1081 * 128 * 1.1 = 152204.8");

	// The screen's height is kept to the heights there are
	Check(HUDScale_ForScreen(0, kHUDSizeUsual) == 128, "no height is drawn as if it were 1 row");
	Check(HUDScale_ForScreen(-5, kHUDSizeUsual) == 128, "and so is a height below nothing");
	Check(HUDScale_ForScreen(INT_MIN, kHUDSizeUsual) == 128, "even the lowest number there is");
	Check(HUDScale_ForScreen(kHUDTallestScreen + 1, kHUDSizeUsual) == kHUDTallestScreen * 128, "a height past the tallest is drawn as the tallest");
	Check(HUDScale_ForScreen(INT_MAX, kHUDSizeLargest) == kHUDTallestScreen * 192, "even the highest number there is, at the largest size, without the sum running over");

	// And the size to the sizes there are
	Check(HUDScale_ForScreen(1024, 0) == HUDScale_ForScreen(1024, 50), "a size below the smallest is the smallest");
	Check(HUDScale_ForScreen(1024, INT_MIN) == HUDScale_ForScreen(1024, 50), "even the lowest number there is");
	Check(HUDScale_ForScreen(1024, 200) == HUDScale_ForScreen(1024, 150), "a size above the largest is the largest");
	Check(HUDScale_ForScreen(1024, INT_MAX) == HUDScale_ForScreen(1024, 150), "even the highest number there is");
	Check(HUDScale_ForScreen(1024, 55) == HUDScale_ForScreen(1024, 60), "a size between two steps goes to the nearest, a half going up");
	Check(HUDScale_ForScreen(1024, 54) == HUDScale_ForScreen(1024, 50), "and under a half going down");

	// Bigger screens and bigger sizes never make it smaller
	bool growsWithHeight = true, growsWithSize = true;

	for (int height = 1; height < kHUDTallestScreen; height++)
	{
		for (int size = kHUDSizeSmallest; size <= kHUDSizeLargest; size += kHUDSizeStep)
		{
			growsWithHeight = growsWithHeight && HUDScale_ForScreen(height + 1, size) >= HUDScale_ForScreen(height, size);
			growsWithSize = growsWithSize && (size == kHUDSizeLargest || HUDScale_ForScreen(height, size + kHUDSizeStep) >= HUDScale_ForScreen(height, size));
		}
	}

	Check(growsWithHeight, "a taller screen never has a smaller HUD");
	Check(growsWithSize, "and a bigger HUD size never makes it smaller");

	// The tallest piece, the Predator's column of 6 symbols, fits on every screen at the largest size. The game draws each symbol
	// MUL_FIXED(51, scale) rows below the last (hud.cpp).
	bool columnFits = true;

	for (int height = 1; height <= kHUDTallestScreen; height++)
	{
		int const symbol = MulFixed(kPredatorSymbolRows, HUDScale_ForScreen(height, kHUDSizeLargest));
		columnFits = columnFits && kPredatorColumnSymbols * symbol <= height;
	}

	Check(columnFits, "the Predator's columns fit on every screen at the largest HUD size");
	Check(kPredatorColumnSymbols * MulFixed(kPredatorSymbolRows, DivFixed(3440, 640)) > 1440, "where in 1999 they ran off a 3440 by 1440 screen");
}

static void TestTheSlider()
{
	Check(kHUDSizeSliderPlaces == 10 && kHUDSizeUsualPlace == 5, "the slider has 11 places, and the usual size is the middle one");
	Check(HUDSize_AtSliderPlace(0) == 50 && HUDSize_AtSliderPlace(5) == 100 && HUDSize_AtSliderPlace(10) == 150, "its places are 50% to 150% in steps of 10");

	bool roundTrip = true;

	for (int place = 0; place <= kHUDSizeSliderPlaces; place++)
	{
		roundTrip = roundTrip && HUDSize_SliderPlace(HUDSize_AtSliderPlace(place)) == place;
	}

	Check(roundTrip, "each place's size is shown at that place");

	Check(HUDSize_AtSliderPlace(-1) == 50 && HUDSize_AtSliderPlace(INT_MIN) == 50, "a place before the first is the first");
	Check(HUDSize_AtSliderPlace(11) == 150 && HUDSize_AtSliderPlace(INT_MAX) == 150, "and one after the last is the last");
	Check(HUDSize_SliderPlace(-3) == 0 && HUDSize_SliderPlace(999) == 10 && HUDSize_SliderPlace(55) == 1, "a size there isn't is shown at the nearest place");
}

// What the settings file's word gives, and whether it was already written as the menu writes it
static bool Reads(const char *word, int size, bool plain)
{
	int got = -1;
	bool const gotPlain = HUDSize_FromSavedWord(word, &got);

	return got == size && gotPlain == plain;
}

static void TestTheSettingsWord()
{
	// What the menu writes is taken as it is
	Check(Reads("100", 100, true), "the usual size, as the menu writes it, is taken as it is");
	Check(Reads("50", 50, true) && Reads("150", 150, true) && Reads("70", 70, true), "and so is any other size there is");

	// A number that isn't a size is brought to one, and written back
	Check(Reads("0", 50, false), "0 is the smallest size, and written back");
	Check(Reads("999", 150, false), "999 is the largest size, and written back");
	Check(Reads("55", 60, false) && Reads("54", 50, false) && Reads("145", 150, false) && Reads("144", 140, false), "a number between steps goes to the nearest, a half going up");
	Check(Reads("050", 50, false), "a size with a 0 in front is that size, written back as the menu writes it");

	// Anything else isn't a number at all, and is the usual size
	Check(Reads(nullptr, 100, false), "no word at all is the usual size");
	Check(Reads("", 100, false), "and so is an empty word");
	Check(Reads("abc", 100, false), "and letters");
	Check(Reads("-5", 100, false), "and a minus sign");
	Check(Reads("+100", 100, false), "and a plus sign");
	Check(Reads(" 100", 100, false), "and a space in front");
	Check(Reads("100 ", 100, false), "and a space after");
	Check(Reads("100x", 100, false), "and anything after the number");
	Check(Reads("1e3", 100, false), "and a number written with a power of ten");
	Check(Reads("10.5", 100, false), "and a fraction");
	Check(Reads("0x64", 100, false), "and a number written in hexadecimal");
	Check(Reads("\xEF\xBC\x91\xEF\xBC\x90\xEF\xBC\x90", 100, false), "and digits from another alphabet");
	Check(Reads("1000", 100, false), "four digits are too many, so it's not read as a number");
	Check(Reads("0100", 100, false), "even with a 0 in front");
	Check(Reads("99999999999", 100, false), "and a number too big for the game to hold is never added up");

	std::string const longDigits(200, '1'), longLetters(200, 'a');
	Check(Reads(longDigits.c_str(), 100, false), "200 digits are the usual size");
	Check(Reads(longLetters.c_str(), 100, false), "and 200 letters");
}

// The alien's bar as the game drew it in 1999 (d3d_hud.cpp on main fa4db6c4, lines 790-801): the screen's whole width, a thickness of
// its length / 32, shrinking towards its middle line
static HUDBar BarIn1999(int width, int height, unsigned int health)
{
	HUDBar bar;

	bar.length = (health < 100) ? static_cast<int>(static_cast<long long>(width) * static_cast<int>(health) / 100) : width;
	bar.thickness = bar.length / 32;
	bar.left = (width - bar.length) / 2;
	bar.top = height - width / 32 + bar.left / 32;

	return bar;
}

static bool SameBar(const HUDBar &a, const HUDBar &b)
{
	return a.left == b.left && a.top == b.top && a.length == b.length && a.thickness == b.thickness;
}

static bool OnTheScreen(const HUDBar &bar, int width, int height)
{
	return bar.left >= 0 && bar.top >= 0 && bar.length >= 0 && bar.thickness >= 0 && bar.left + bar.length <= width && bar.top + bar.thickness <= height;
}

static void TestTheAlienBar()
{
	int const usual = HUDScale_ForScreen(1024, kHUDSizeUsual);

	// On a 1280 by 1024 screen it's the old bar, to the pixel, at every health, and for the half bar drawn on top of it
	bool sameAsBefore = true;

	for (unsigned int health = 0; health <= 100; health++)
	{
		sameAsBefore = sameAsBefore && SameBar(HUDAlienBar_Place(1280, 1024, usual, health), BarIn1999(1280, 1024, health));
		sameAsBefore = sameAsBefore && SameBar(HUDAlienBar_Place(1280, 1024, usual, health / 2), BarIn1999(1280, 1024, health / 2));
	}

	Check(sameAsBefore, "on a 1280 by 1024 screen at the usual size, the bar is the old one at every health");

	// The game gives its health as an unsigned number, so a health below nothing comes in huge: the whole bar, as in 1999
	unsigned int const belowNothing = static_cast<unsigned int>(-5);
	Check(SameBar(HUDAlienBar_Place(1280, 1024, usual, belowNothing), BarIn1999(1280, 1024, belowNothing)), "a health below nothing is the whole bar, as it always was");
	Check(SameBar(HUDAlienBar_Place(1280, 1024, usual, 250), HUDAlienBar_Place(1280, 1024, usual, 100)), "and so is a health above 100");

	// On a wider screen it's as wide as a 4:3 screen of that height, in the middle
	HUDBar const wide = HUDAlienBar_Place(1920, 1080, HUDScale_ForScreen(1080, kHUDSizeUsual), 100);
	Check(wide.length == 1440 && wide.left == 240, "on a 1920 by 1080 screen it's 1440 long, in the middle");
	Check(wide.top + wide.thickness == 1080, "and the whole bar sits on the screen's bottom edge");

	HUDBar const ultraWide = HUDAlienBar_Place(3440, 1440, HUDScale_ForScreen(1440, kHUDSizeUsual), 100);
	Check(ultraWide.length == 1920 && ultraWide.left == 760, "on a 3440 by 1440 screen it's 1920 long, in the middle");

	HUDBar const narrow = HUDAlienBar_Place(768, 1024, HUDScale_ForScreen(1024, kHUDSizeUsual), 100);
	Check(narrow.length == 768 && narrow.left == 0, "and it's never longer than a screen narrower than 4:3");

	// The HUD size changes its thickness, never its length
	HUDBar const big = HUDAlienBar_Place(1920, 1080, HUDScale_ForScreen(1080, kHUDSizeLargest), 100);
	HUDBar const small = HUDAlienBar_Place(1920, 1080, HUDScale_ForScreen(1080, kHUDSizeSmallest), 100);
	Check(big.length == wide.length && small.length == wide.length, "the HUD size never changes its length");
	Check(big.thickness > wide.thickness && small.thickness < wide.thickness, "only its thickness");

	// No bar ever leaves the screen
	bool staysOn = true;

	for (const Screen &screen : screens)
	{
		for (int size = kHUDSizeSmallest; size <= kHUDSizeLargest; size += kHUDSizeStep)
		{
			int const scale = HUDScale_ForScreen(screen.height, size);

			for (unsigned int health = 0; health <= 100; health++)
			{
				staysOn = staysOn && OnTheScreen(HUDAlienBar_Place(screen.width, screen.height, scale, health), screen.width, screen.height);
			}

			staysOn = staysOn && OnTheScreen(HUDAlienBar_Place(screen.width, screen.height, scale, belowNothing), screen.width, screen.height);
		}
	}

	Check(staysOn, "no bar leaves the screen, on any of the screens, at any size or health");

	// Nothing to draw on, or nothing to draw: no bar, and no dividing by nothing
	HUDBar const none = { 0, 0, 0, 0 };
	Check(SameBar(HUDAlienBar_Place(0, 0, usual, 100), none), "no screen is no bar");
	Check(SameBar(HUDAlienBar_Place(-5, 1024, usual, 100), none) && SameBar(HUDAlienBar_Place(1280, -5, usual, 100), none), "and nor is a screen of less than nothing");
	Check(SameBar(HUDAlienBar_Place(1280, 1024, 0, 100), none) && SameBar(HUDAlienBar_Place(1280, 1024, -65536, 100), none), "and nor is a scale of nothing, or less");
	Check(SameBar(HUDAlienBar_Place(INT_MAX, INT_MAX, INT_MAX, belowNothing), HUDAlienBar_Place(INT_MAX, INT_MAX, INT_MAX, 100)), "the biggest numbers there are still give the whole bar, without the sums running over");
}

// The game's files, read as text: everything that draws the HUD takes its size from HUD_FollowTheScreen
static void TestJoiningTheGame()
{
	std::string const hud3d = ReadSourceFile("3dc/avp/win95/d3d_hud.cpp");
	std::string const hud3dHeader = ReadSourceFile("3dc/avp/win95/d3d_hud.h");
	std::string const game = ReadSourceFile("3dc/avp/game.cpp");
	std::string const hud = ReadSourceFile("3dc/avp/hud.cpp");
	std::string const menuData = ReadSourceFile("3dc/avp/win95/frontend/menu_data.cpp");
	std::string const menus = ReadSourceFile("3dc/avp/win95/frontend/menus.cpp");
	std::string const langenum = ReadSourceFile("3dc/avp/langenum.h");
	std::string const language = ReadSourceFile("3dc/avp/language.cpp");
	std::string const settings = ReadSourceFile("3dc/view_settings.cpp");

	Check(!hud3d.empty() && !game.empty() && !hud.empty() && !menuData.empty() && !menus.empty() && !langenum.empty() && !language.empty() && !settings.empty(), "the game's files are there to read");

	// One place works the scale out, and gives it to both of the game's numbers for it, for every species
	std::string const follow = Between(hud3d, "void HUD_FollowTheScreen(bool evenIfUnchanged)\n{", "\n}\n");
	Check(Has(follow, "int const height = ScreenDescriptorBlock.SDB_Height;") && Has(follow, "int const size = ViewSettings_HUDSize();"), "the scale comes from the screen's height and the player's HUD size");
	Check(Has(follow, "HUDScaleFactor = HUDScale_ForScreen(height, size);") && Has(follow, "MotionTrackerScale = HUDScaleFactor;"), "and goes to both the HUD's scale and the motion tracker's");
	Check(Has(follow, "if (!evenIfUnchanged && height == heightBefore && size == sizeBefore)\n\t{\n\t\treturn;"), "asked again with nothing changed, it leaves them alone");
	Check(Has(hud3dHeader, "void HUD_FollowTheScreen(bool evenIfUnchanged);") && Has(hud3dHeader, "extern fixed_t HUDScaleFactor;"), "hud.cpp can ask for it, and read the scale");

	Check(Has(Between(hud3d, "void D3D_InitialiseMarineHUD(void)\n{", "\n}\n"), "HUD_FollowTheScreen(true);"), "the marine's HUD asks when it's set up");
	Check(Has(Between(game, "void StartGame(void)\n{", "\n}\n"), "HUD_FollowTheScreen(true);"), "and every level's start asks, whatever the species");
	Check(Has(Between(hud, "void MaintainHUD(void)\n{", "HandleParticleSystem();"), "HUD_FollowTheScreen(false);"), "and every frame asks, before anything is drawn");

	// Nothing works out its own scale from the screen's width any more
	Check(!Has(hud3d, "SDB_Width, 640") && !Has(hud3d, "SDB_Width,640"), "d3d_hud.cpp has no scale of its own");
	Check(!Has(game, "SDB_Width, 640") && !Has(game, "SDB_Width,640") && !Has(game, "MotionTrackerScale ="), "nor has game.cpp");
	Check(!Has(hud, "SDB_Width, 640") && !Has(hud, "SDB_Width,640"), "nor has hud.cpp");

	// The pieces that use it
	std::string const predator = Between(hud, "void DisplayPredatorHealthAndEnergy(void)\n{", "if (weaponPtr->WeaponIDNumber == WEAPON_PRED_RIFLE)");
	Check(Has(predator, "int scale = HUDScaleFactor;") && Has(predator, "int size = MUL_FIXED(kPredatorSymbolRows,scale);"), "the Predator's symbols are drawn at the HUD's scale, at the size the test checks");
	Check(CountOf(predator, "for (i=0; i<kPredatorColumnSymbols; i++)") == 2 && !Has(predator, "i<6;"), "and both its columns have the number of symbols the test checks");

	std::string const gunsight = Between(hud3d, "void D3D_BLTGunSightToHUD(", "Draw_HUDImage(&imageDesc);");
	Check(Has(gunsight, "imageDesc.Scale  = HUDScaleFactor;"), "the gunsight is drawn at the HUD's scale");
	Check(Has(gunsight, "int const sizeOnTheScreen = MUL_FIXED(HUDScaleFactor, gunsightSize);") && Has(gunsight, "screenX = (screenX-(sizeOnTheScreen/2));") && Has(gunsight, "screenY = (screenY-(sizeOnTheScreen/2));"), "and stays centred on where it points");

	Check(Has(hud3d, "HUDAlienBar_Place(ScreenDescriptorBlock.SDB_Width, ScreenDescriptorBlock.SDB_Height, HUDScaleFactor, health));"), "the alien's bar is placed by the rules above");
	Check(Has(hud3d, "HUDAlienBar_Place(ScreenDescriptorBlock.SDB_Width, ScreenDescriptorBlock.SDB_Height, HUDScaleFactor, health / 2));"), "and so is the half bar on top of it");
	Check(!Has(hud3d, "SDB_Width/32"), "and nothing is drawn at the old width / 32 any more");
	Check(!Has(hud3d, "DrawPredatorEnergyBar(void)") && !Has(hud3dHeader, "DrawPredatorEnergyBar"), "the Predator's energy bars that nothing ever called are gone");

	// The menu: the page's last line, a slider that moves one place at a time
	std::string const page = Between(menuData, "static AVPMENU_ELEMENT AvPMenu_Rendering[] =", "};");
	char const slider[] = "{AVPMENU_ELEMENT_SLIDER,\t{TEXTSTRING_RENDERING_HUDSIZE},\t{kHUDSizeSliderPlaces},\t{&MenuHUDSize},\t{0},\tTEXTSTRING_RENDERING_HUDSIZE_HELP},";
	Check(ComesBefore(page, "TEXTSTRING_RENDERING_KEY_DEVVIEWS_HELP},", slider) && ComesBefore(page, slider, "{AVPMENU_ELEMENT_ENDOFMENU}"), "the rendering page's last line is the HUD size, after the keys");
	Check(CountOf(page, "AVPMENU_ELEMENT_SLIDER") == 1, "and it's the page's only slider");

	std::string const moving = Between(menus, "case AVPMENU_ELEMENT_SLIDER:\n\t\t{\n\t\t\tif ((interactionID", "break;");
	Check(Has(moving, "if (*elementPtr->SliderValuePtr<elementPtr->MaxSliderValue)\n\t\t\t\t{\n\t\t\t\t\t*elementPtr->SliderValuePtr+=1;"), "a slider moves up one place, and never past its last");
	Check(Has(moving, "if (*elementPtr->SliderValuePtr>0)\n\t\t\t\t{\n\t\t\t\t\t*elementPtr->SliderValuePtr-=1;"), "and down one, and never before its first");

	// Its words come after everything the player's language.txt holds, at the end of the list that's compiled
	std::string const ids = Between(langenum, "#if (1)", "#else");
	Check(Has(ids, "TEXTSTRING_COOP_MAP_COMPLETE,\n\tTEXTSTRING_RENDERING_HUDSIZE,\n\tTEXTSTRING_RENDERING_HUDSIZE_HELP,\n\n\tMAX_NO_OF_TEXTSTRINGS"), "the line's two text ids are at the end of the list");
	Check(Has(language, "\t\"Map complete\",\n\t\"HUD size\",\n\t\"How big the health, ammo, motion tracker and other on-screen readings are. The middle is the usual size.\",\n};"), "and its words, as approved, at the end of ours");

	// The settings file is hostile
	std::string const loading = Between(settings, "void ViewSettings_Load()", "void ViewSettings_MenuFrame()");
	Check(Has(settings, "static const char kHUDSizeSetting[] = \"HUDSize\";"), "the size is kept as HUDSize");
	Check(Has(loading, "Config_GetString(kHeading, kHUDSizeSetting, std::to_string(kHUDSizeUsual))") && !Has(loading, "Config_GetInt"), "it's read as a word, because the file's own number reader takes anything");
	Check(Has(loading, "if (!HUDSize_FromSavedWord(hudSizeWord.c_str(), &hudSize))"), "and what it says goes through the rules above");

	std::string const logLine = Between(loading, "LogErrorString(\"the settings file's HUDSize line", ";");
	Check(!logLine.empty() && !Has(logLine, "hudSizeWord"), "a line that isn't a size is in the log, without repeating what the file said");
	Check(Has(loading, "Config_SetString(kHeading, kHUDSizeSetting, std::to_string(hudSize));"), "and the file gets put right");

	std::string const menuFrame = Between(settings, "void ViewSettings_MenuFrame()", "int ViewSettings_HUDSize()");
	Check(Has(menuFrame, "hudSize = HUDSize_AtSliderPlace(MenuHUDSize);"), "the slider's place is turned into a size through the rules, too");

	// The game builds the rules
	Check(Has(ReadSourceFile("win32/VS2026/AvP.vcxproj"), "3dc\\hud_scale.cpp"), "the game builds these rules");
	Check(Has(ReadSourceFile("win32/VS2026/AvP.vcxproj.filters"), "3dc\\hud_scale.cpp"), "and the project's list of files shows them");
}

int main()
{
	TestTheScale();
	TestTheSlider();
	TestTheSettingsWord();
	TestTheAlienBar();
	TestJoiningTheGame();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");

	return checksFailed ? 1 : 0;
}
