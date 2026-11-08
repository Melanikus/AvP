// Tests for the keys on the rendering page. The first half tries view_keys.cpp, which knows nothing about the game, and every refusal has to give its own reason. The second half reads the game's own files,
// to see that the page, the settings file and the keys are joined up the way view_keys.h says. Written 2026-11-05 by Melanikus for the AvP port.
#include "view_keys.h"

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

// Made-up keys, because these rules don't care what a number means. The game's own numbers are checked against its own files further down.
enum
{
	kNoKey = 255,
	kLastRealKey = 200,
	kEscape = 0,
	kConsole = 41,
	kUsualRendering = 38,
	kUsualShadows = 35,
	kUsualDevViews = 36,
	kFree = 60,
	kAlsoFree = 61,
	kFire = 70
};

static const unsigned char gameKeeps[] = { kEscape, kConsole };
static const unsigned char controls[] = { kFire, kNoKey };
static const int usualKeys[kViewKeyLines] = { kUsualRendering, kUsualShadows, kUsualDevViews };

static ViewKeyRules TheRules()
{
	ViewKeyRules const rules = { kNoKey, kLastRealKey, gameKeeps, 2, controls, 2 };

	return rules;
}

static ViewKeyAnswer Judge(int line, int key)
{
	return ViewKey_Judge(line, key, usualKeys, TheRules());
}

static void TestJudging()
{
	Check(Judge(kViewKeyRendering, kFree) == kViewKeyTaken, "a key nothing has can be chosen");
	Check(Judge(kViewKeyDevViews, kFree) == kViewKeyTaken, "on any line");
	Check(Judge(kViewKeyRendering, kUsualRendering) == kViewKeyTaken, "a line's own key can be chosen again");
	Check(Judge(kViewKeyRendering, kNoKey) == kViewKeyTaken, "and no key at all is always fine");
	Check(Judge(kViewKeyRendering, kLastRealKey) == kViewKeyTaken, "the last real key is a key");

	Check(Judge(kViewKeyRendering, -1) == kViewKeyNotAKey, "a number below any key is refused as not a key");
	Check(Judge(kViewKeyRendering, kLastRealKey + 1) == kViewKeyNotAKey, "and so is one past the last");
	Check(Judge(kViewKeyRendering, 256) == kViewKeyNotAKey, "and one past the end of the game's table of keys");
	Check(Judge(kViewKeyRendering, kEscape) == kViewKeyTheGameKeepsIt, "Escape is the game's");
	Check(Judge(kViewKeyRendering, kConsole) == kViewKeyTheGameKeepsIt, "and so is the last on the game's list");
	Check(Judge(kViewKeyRendering, kUsualShadows) == kViewKeyIsOnAnotherLine, "a key on the line below is refused as being on another line");
	Check(Judge(kViewKeyShadows, kUsualRendering) == kViewKeyIsOnAnotherLine, "and on the line above");
	Check(Judge(kViewKeyRendering, kUsualDevViews) == kViewKeyIsOnAnotherLine, "and on the last line");
	Check(Judge(kViewKeyRendering, kFire) == kViewKeyIsAControl, "a control's key is refused as a control");
	Check(Judge(kViewKeyRendering, kFree) != kViewKeyIsAControl, "and a key no control has isn't");

	// Which reason comes first matters only so that the reason given is the truest one
	unsigned char const controlOnGamesKey[] = { kEscape };
	ViewKeyRules const both = { kNoKey, kLastRealKey, gameKeeps, 2, controlOnGamesKey, 1 };
	Check(ViewKey_Judge(kViewKeyRendering, kEscape, usualKeys, both) == kViewKeyTheGameKeepsIt, "a key the game keeps says that, even if a control has it too");

	unsigned char const controlOnALine[] = { kUsualShadows };
	ViewKeyRules const onALine = { kNoKey, kLastRealKey, gameKeeps, 2, controlOnALine, 1 };
	Check(ViewKey_Judge(kViewKeyRendering, kUsualShadows, usualKeys, onALine) == kViewKeyIsOnAnotherLine, "a key on another line says that, even if a control has it too");

	// Two lines with no key aren't a clash
	int const noKeys[kViewKeyLines] = { kNoKey, kNoKey, kNoKey };
	Check(ViewKey_Judge(kViewKeyShadows, kNoKey, noKeys, TheRules()) == kViewKeyTaken, "any number of lines can have no key");
	Check(ViewKey_Judge(kViewKeyShadows, kFree, noKeys, TheRules()) == kViewKeyTaken, "and that doesn't stop them having one");
	Check(ViewKey_IsInList(kFire, controls, 2) && ViewKey_IsInList(kNoKey, controls, 2) && !ViewKey_IsInList(kFree, controls, 2) && !ViewKey_IsInList(kNoKey, controls, 1) && !ViewKey_IsInList(kFire, controls, 0), "the list search looks at the whole list and no further");
}

static bool ReadsAs(const char *word, int wanted)
{
	int key = -99;

	return ViewKey_FromSavedWord(word, &key) && key == wanted;
}

static bool IsRefused(const char *word)
{
	int key = -99;

	return !ViewKey_FromSavedWord(word, &key) && key == -99;
}

static void TestReadingTheFile()
{
	Check(ReadsAs("0", 0), "0 is a number");
	Check(ReadsAs("7", 7), "one digit");
	Check(ReadsAs("38", 38), "two");
	Check(ReadsAs("100", 100), "a zero that isn't in front");
	Check(ReadsAs("255", 255), "and the highest there can be");

	Check(IsRefused(0), "nothing at all is refused, and the key is left alone");

	char const *const refused[] = { "", " ", "256", "999", "1000", "99999999999999999999", "4294967297", "-1", "+1", " 38", "38 ", "3 8", "038", "00", "0x26", "3.8", "38a", "a38", "L", "KEY_L", "\t38", "38\n" };

	for (char const *const word : refused)
	{
		Check(IsRefused(word), std::string("[") + word + "] is refused, and the key is left alone");
	}
}

static void Settle(const char *first, const char *second, const char *third, int keys[kViewKeyLines], bool wasRefused[kViewKeyLines])
{
	char const *const words[kViewKeyLines] = { first, second, third };

	ViewKey_SettleSaved(words, usualKeys, TheRules(), keys, wasRefused);
}

static void TestSettling()
{
	int keys[kViewKeyLines];
	bool refused[kViewKeyLines];

	Settle("38", "35", "36", keys, refused);
	Check(keys[0] == kUsualRendering && keys[1] == kUsualShadows && keys[2] == kUsualDevViews && !refused[0] && !refused[1] && !refused[2], "the usual keys are kept, and nothing's refused");

	Settle("60", "61", "62", keys, refused);
	Check(keys[0] == 60 && keys[1] == 61 && keys[2] == 62 && !refused[0] && !refused[1] && !refused[2], "chosen keys are kept");

	Settle("255", "255", "255", keys, refused);
	Check(keys[0] == kNoKey && keys[1] == kNoKey && keys[2] == kNoKey && !refused[0] && !refused[1] && !refused[2], "and so is having no key, on every line");

	Settle("L", "35", "36", keys, refused);
	Check(keys[0] == kUsualRendering && refused[0] && !refused[1] && !refused[2], "a word that isn't a number goes back to the usual key, and only that line is refused");

	Settle("38", "L", "36", keys, refused);
	Check(keys[1] == kUsualShadows && refused[1] && !refused[0] && !refused[2], "on any line");

	Settle("38", "35", "L", keys, refused);
	Check(keys[2] == kUsualDevViews && refused[2] && !refused[0] && !refused[1], "the last too");

	Settle("0", "35", "36", keys, refused);
	Check(keys[0] == kUsualRendering && refused[0], "a key the game keeps goes back to the usual one");

	Settle("201", "35", "36", keys, refused);
	Check(keys[0] == kUsualRendering && refused[0], "and so does a number that's no key");

	Settle("60", "60", "36", keys, refused);
	Check(keys[0] == 60 && !refused[0] && keys[1] == kUsualShadows && refused[1], "of two lines with the same key, the lower goes back");

	Settle("35", "35", "36", keys, refused);
	Check(keys[0] == kUsualShadows && !refused[0] && keys[1] == kNoKey && refused[1], "and with nothing to go back to, because the line above has its usual key, it has none");

	Settle("L", "38", "36", keys, refused);
	Check(keys[0] == kUsualRendering && refused[0] && keys[1] == kUsualShadows && refused[1], "a line sent back to its usual key counts against the lines below it");

	int const usualOnGamesKey[kViewKeyLines] = { kEscape, kUsualShadows, kUsualDevViews };
	char const *const words[kViewKeyLines] = { "L", "35", "36" };
	ViewKey_SettleSaved(words, usualOnGamesKey, TheRules(), keys, refused);
	Check(keys[0] == kNoKey && refused[0], "a usual key that isn't allowed either means no key, rather than a key that's refused");

	// The file's read before any profile is, so no controls are known. What's kept still has to get past a control later, which is what ViewKey_Acts is for.
	Settle("70", "35", "36", keys, refused);
	Check(keys[0] == kUsualRendering && refused[0], "a control's key is refused when controls are known");

	char const *const fromAFile[kViewKeyLines] = { "70", "35", "36" };
	ViewKeyRules const noControls = { kNoKey, kLastRealKey, gameKeeps, 2, 0, 0 };
	ViewKey_SettleSaved(fromAFile, usualKeys, noControls, keys, refused);
	Check(keys[0] == kFire && !refused[0], "and kept when they aren't");
}

static void TestActing()
{
	ViewKeyRules const rules = TheRules();

	Check(ViewKey_Acts(kFree, true, rules), "a line's key does something when it's pressed");
	Check(!ViewKey_Acts(kFree, false, rules), "and nothing when it isn't");
	ViewKeyRules const noControls = { kNoKey, kLastRealKey, gameKeeps, 2, 0, 0 };

	Check(!ViewKey_Acts(kNoKey, true, noControls), "a line with no key never does, whatever the table of keys says, even with no control having a blank");
	Check(!ViewKey_Acts(kFire, true, rules), "and a control with the same key wins");
}

static void TestJoiningTheGame()
{
	std::string const settings = ReadSourceFile("3dc/view_settings.cpp");
	std::string const loading = Between(settings, "void ViewSettings_Load()", "void ViewSettings_MenuFrame()");
	std::string const render = ReadSourceFile("3dc/d3d_render.cpp");
	std::string const menus = ReadSourceFile("3dc/avp/win95/frontend/menus.cpp");
	std::string const menuNames = ReadSourceFile("3dc/avp/win95/frontend/menus.h");
	std::string const menuData = ReadSourceFile("3dc/avp/win95/frontend/menu_data.cpp");
	std::string const language = ReadSourceFile("3dc/avp/language.cpp");
	std::string const textNames = ReadSourceFile("3dc/avp/langenum.h");
	std::string const keyNames = ReadSourceFile("3dc/win95/platform.h");
	std::string const userIO = ReadSourceFile("3dc/avp/win95/user_io.cpp");

	// the usual keys, and what the game keeps
	Check(Has(settings, "static const int kUsualKeys[kViewKeyLines] = { KEY_L, KEY_H, KEY_J };"), "L is the rendering mode, H the shadows and J the dev views, until the player says otherwise");
	Check(Has(settings, "\"RenderingKey\", \"ShadowsKey\", \"DevViewsKey\""), "and each has its own line in the settings file");

	std::string const keeps = Between(settings, "static const unsigned char kKeysTheGameKeeps[]", "};");

	for (char const *const key : { "KEY_ESCAPE", "KEY_0", "KEY_1", "KEY_2", "KEY_3", "KEY_4", "KEY_5", "KEY_6", "KEY_7", "KEY_8", "KEY_9", "KEY_GRAVE", "KEY_JOYSTICK_BUTTON_9", "KEY_F5", "KEY_F7", "KEY_F8", "KEY_F9", "KEY_F10" })
	{
		Check(Has(keeps, key), std::string("the game keeps ") + key);
	}

	std::string const fixedKeys = Between(userIO, "FIXED_INPUT_CONFIGURATION FixedInputConfig =", "};");

	for (char const *const key : { "KEY_0", "KEY_1", "KEY_2", "KEY_3", "KEY_4", "KEY_5", "KEY_6", "KEY_7", "KEY_8", "KEY_9", "KEY_ESCAPE" })
	{
		Check(Has(fixedKeys, key), std::string("and that's because ") + key + " is on the list of keys that can't be changed");
	}

	Check(Has(userIO, "DebouncedKeyboardInput[KEY_GRAVE]"), "the console's key is read by name");
	Check(Has(userIO, "DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_9]"), "and so is a game pad's menu button");
	Check(Has(render, "DebouncedKeyboardInput[KEY_F8]") && Has(render, "DebouncedKeyboardInput[KEY_F7]") && Has(render, "DebouncedKeyboardInput[KEY_F9]") && Has(render, "DebouncedKeyboardInput[KEY_F10]") && Has(render, "DebouncedKeyboardInput[KEY_F5]"), "and so are the developers' tools");

	// the game's table of keys has to fit these rules
	Check(Has(settings, "rules.noKey = KEY_VOID;"), "no key is the game's own blank key");
	Check(Has(settings, "rules.lastRealKey = KEY_DIACRITIC_UMLAUT;") && Has(keyNames, "KEY_DIACRITIC_UMLAUT,\n\n\n\tKEY_VOID=255,"), "and the last real key really is the last");
	Check(Has(keyNames, "KEY_VOID=255,") && Has(keyNames, "MAX_NUMBER_OF_INPUT_KEYS,"), "so every key allowed is inside the game's table of 256");

	// the settings file is hostile
	Check(Has(loading, "ViewKey_SettleSaved(wordsToRead, kUsualKeys, TheRules(0, 0), viewKeys, wasRefused);"), "what the file says goes through the rules");
	Check(Has(loading, "Config_GetString(kHeading, kKeySettings[line], std::to_string(kUsualKeys[line]))"), "an old file with no such lines gets the usual keys, and gets the lines");
	Check(!Has(loading, "Config_GetInt"), "it's read as a word, because the file's own number reader takes anything");
	Check(Has(loading, "LogErrorString(std::string(\"the settings file's \") + kKeySettings[line] + \" line isn't a key that line can have, so it has gone back\");"), "a line sent back is in the log, without repeating what the file said");
	Check(Has(loading, "Config_SetString(kHeading, kKeySettings[line], std::to_string(viewKeys[line]));"), "and the file gets put right");

	// the menu
	std::string const page = Between(menuData, "static AVPMENU_ELEMENT AvPMenu_Rendering[] =", "};");
	Check(ComesBefore(page, "TEXTSTRING_RENDERING_AUTOGENERATE}", "{AVPMENU_ELEMENT_VIEWKEY,\t{TEXTSTRING_RENDERING_KEY_RENDERING},\t{kViewKeyRendering},"), "the page has the rendering key, after its other lines");
	Check(ComesBefore(page, "{kViewKeyRendering},", "{AVPMENU_ELEMENT_VIEWKEY,\t{TEXTSTRING_RENDERING_KEY_SHADOWS},\t{kViewKeyShadows},"), "then the shadows key");
	Check(ComesBefore(page, "{kViewKeyShadows},", "{AVPMENU_ELEMENT_VIEWKEY,\t{TEXTSTRING_RENDERING_KEY_DEVVIEWS},\t{kViewKeyDevViews},"), "then the dev views key");
	// However many other lines the page has, the key lines start right after them
	int const otherLines = CountOf(Between(page, "{", "AVPMENU_ELEMENT_VIEWKEY"), "AVPMENU_ELEMENT_TEXTSLIDER");

	Check(Has(menuData, "AVPMENU_ELEMENT &element = AvPMenu_Rendering[kFirstViewKeyElement + line];") && otherLines >= 3
		&& Has(menuData, ("static const int kFirstViewKeyElement = " + std::to_string(otherLines) + ";").c_str()), "and what a line says is changed on the right one");
	Check(Has(menuNames, "AVPMENU_ELEMENT_LOADGAME,\n\tAVPMENU_ELEMENT_VIEWKEY,"), "it's a new kind of line, at the end, so nothing else's number changes");
	Check(Has(menuNames, "unsigned int\t\tUserChangingViewKey :1;"), "with its own state");
	Check(Has(menus, "AvPMenus.UserChangingKeyConfig = 0;\n\tAvPMenus.UserChangingViewKey = 0;"), "which is cleared along with the controls page's");

	std::string const choosing = Between(menus, "else if (AvPMenus.UserChangingViewKey)", "else if (AvPMenus.UserChangingKeyConfig)");
	Check(Has(choosing, "if (DebouncedKeyboardInput[KEY_ESCAPE])\n\t\t{\n\t\t\tAvPMenus.UserChangingViewKey = 0;"), "Escape stops choosing, and changes nothing");
	Check(Has(choosing, "for (int key = 0; key < KEY_VOID; key++)"), "any other key is what was chosen, and only real ones are looked at");
	Check(Has(choosing, "ViewSettings_PressedKeyForLine(AvPMenus.MenuElements[AvPMenus.CurrentlySelectedElement].MaxValue, key);"), "and the rules decide, not the menu");
	Check(!Has(choosing, "PlayerInput"), "and it can't write to a player's controls");

	std::string const interacting = Between(menus, "case AVPMENU_ELEMENT_VIEWKEY:\n\t\t{\n\t\t\tif (interactionID", "break;");
	Check(Has(interacting, "if (interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT)") && Has(interacting, "AvPMenus.UserChangingViewKey = 1;"), "Enter starts choosing, and left and right do nothing");

	std::string const clearing = Between(menus, "case AVPMENU_ELEMENT_VIEWKEY:\n\t\t\t\t{\n\t\t\t\t\tViewSettings_PressedKeyForLine", "break;");
	Check(Has(clearing, ".MaxValue, KEY_VOID);"), "Backspace means no key");

	std::string const drawing = Between(menus, "case AVPMENU_ELEMENT_VIEWKEY:\n\t\t{\n\t\t\tint const line = elementPtr->MaxValue;", "break;");
	Check(Has(drawing, "ViewSettings_LineIsHeldByAControl(line)") && Has(drawing, "RenderText_Coloured"), "a key a control has too is drawn in another colour");
	Check(Has(drawing, "AvPMenus.UserChangingViewKey && e==AvPMenus.CurrentlySelectedElement"), "and the line being chosen shows that");

	std::string const assignments = Between(menus, "void CheckForKeysWithMultipleAssignments(void)\n{", "void HandleCheatModeFeatures(void)\n{");
	Check(Has(assignments, "if (ViewSettings_KeyIsOnALine((configPtr[column])[row]))\n\t\t\t{\n\t\t\t\tMultipleAssignments[column][row] = 1;"), "the controls page shows it too, so that a control can't take a key without saying so");

	// choosing
	std::string const pressed = Between(settings, "void ViewSettings_PressedKeyForLine(int line, int key)", "\n}");
	Check(Has(pressed, "if (line < 0 || line >= kViewKeyLines)"), "a line that isn't one of the three is ignored");
	Check(Has(pressed, "EveryControl(controls);") && Has(pressed, "ViewKey_Judge(line, key, viewKeys, TheRules(controls, sizeof(controls)))"), "every species' controls count");
	Check(Has(pressed, "if (lastAnswer[line] == kViewKeyTaken)"), "a refused key changes nothing");
	Check(ComesBefore(pressed, "viewKeys[line] = key;", "Config_SetString(kHeading, kKeySettings[line], std::to_string(key));") && ComesBefore(pressed, "Config_SetString(kHeading", "Config_Save();"), "and a taken one is saved");
	Check(Has(Between(settings, "void ViewSettings_ChoosingKey(int line)", "\n}"), "lastAnswer[line] = kViewKeyTaken;"), "starting again forgets the last refusal");
	Check(Has(Between(settings, "void ViewSettings_MenuFrame()", "if (MenuAutoGenerate != lastAutoGenerate)"), "RenderingPage_SayWhatKeyLineDoes(line, lastAnswer[line], ViewSettings_LineIsHeldByAControl(line));"), "and the page says what became of a key");

	std::string const everyControl = Between(settings, "static void EveryControl(", "\n}");
	for (char const *const config : { "MarineInputPrimaryConfig", "MarineInputSecondaryConfig", "PredatorInputPrimaryConfig", "PredatorInputSecondaryConfig", "AlienInputPrimaryConfig", "AlienInputSecondaryConfig" })
	{
		Check(Has(everyControl, config), std::string("the rules look at ") + config);
	}

	// what the page says
	std::string const says = Between(menuData, "void RenderingPage_SayWhatKeyLineDoes(", "\n}");
	Check(Has(says, "case kViewKeyTheGameKeepsIt:\n\tcase kViewKeyNotAKey:\n\t\telement.HelpString = TEXTSTRING_RENDERING_KEY_REFUSED_GAME;"), "a key the game keeps says so");
	Check(Has(says, "case kViewKeyIsOnAnotherLine:\n\t\telement.HelpString = TEXTSTRING_RENDERING_KEY_REFUSED_LINE;"), "and one on another line");
	Check(Has(says, "case kViewKeyIsAControl:\n\t\telement.HelpString = TEXTSTRING_RENDERING_KEY_REFUSED_CONTROL;"), "and a control's");
	Check(Has(says, "element.HelpString = heldByAControl ? TEXTSTRING_RENDERING_KEY_HELD : usualHelp[line];"), "and a key a control took afterwards");

	for (char const *const name : { "TEXTSTRING_RENDERING_KEY_RENDERING,", "TEXTSTRING_RENDERING_KEY_RENDERING_HELP,", "TEXTSTRING_RENDERING_KEY_SHADOWS,", "TEXTSTRING_RENDERING_KEY_SHADOWS_HELP,", "TEXTSTRING_RENDERING_KEY_DEVVIEWS,", "TEXTSTRING_RENDERING_KEY_DEVVIEWS_HELP,", "TEXTSTRING_RENDERING_KEY_REFUSED_GAME,", "TEXTSTRING_RENDERING_KEY_REFUSED_LINE,", "TEXTSTRING_RENDERING_KEY_REFUSED_CONTROL,", "TEXTSTRING_RENDERING_KEY_HELD," })
	{
		Check(textNames.find(name) != std::string::npos && textNames.find(name) > textNames.find("TEXTSTRING_RENDERING_TITLE,") && textNames.find(name) < textNames.find("MAX_NO_OF_TEXTSTRINGS"), std::string(name) + " is one of our own lines of text");
	}

	Check(Has(language, "\"Rendering key\",") && Has(language, "\"Shadows key\",") && Has(language, "\"Dev views key\","), "the lines have names");
	Check(Has(language, "It only works when the game was started with -dev, and never in a network game."), "and the dev views say when they work");

	// pressing them
	std::string const canAct = Between(render, "static bool PlayerKeysCanAct()", "\n}");
	std::string const devCanAct = Between(render, "static bool DevKeysCanAct()", "\n}");
	Check(!Has(canAct, "DevMode") && !Has(canAct, "I_No_Network"), "the rendering and shadows keys work for everyone, with no -dev, and in a network game");
	Check(Has(canAct, "AvP.MainLoopRunning") && Has(canAct, "!InGameMenusAreRunning()") && Has(canAct, "IOFOCUS_AcceptControls()") && Has(canAct, "IOFOCUS_NEWCONSOLE | IOFOCUS_OSK"), "but only in a level being played, with no menu open, and not while typing");
	Check(Has(devCanAct, "DevMode") && Has(devCanAct, "AvP.Network == I_No_Network") && Has(devCanAct, "PlayerKeysCanAct()"), "the dev views need -dev as well, and no network game");
	Check(Has(render, "if (PlayerKeysCanAct())\n\t{\n\t\tif (ViewSettings_KeyPressed(kViewKeyRendering))\n\t\t{\n\t\t\tlightingAsked = kAskedNext;"), "the rendering key is the next lighting");
	Check(Has(render, "if (ViewSettings_KeyPressed(kViewKeyShadows))\n\t\t{\n\t\t\tshadowsAsked = kAskedNext;"), "the shadows key switches the shadows");
	Check(Has(render, "if (DevKeysCanAct() && ViewSettings_KeyPressed(kViewKeyDevViews))"), "and the dev views' key the view");
	Check(!Has(render, "DebouncedKeyboardInput[KEY_L]") && !Has(render, "DebouncedKeyboardInput[KEY_H]") && !Has(render, "DebouncedKeyboardInput[KEY_J]"), "and nothing in the drawing code reads those letters any more");

	std::string const pressing = Between(settings, "bool ViewSettings_KeyPressed(int line)", "\n}");
	Check(Has(pressing, "if (line < 0 || line >= kViewKeyLines)"), "a line that isn't one of the three never acts");
	Check(Has(pressing, "ViewKey_Acts(key, DebouncedKeyboardInput[key] != 0, TheRules(controls, sizeof(controls)))"), "and a control wins, during a level");
	Check(Has(Between(settings, "static void TheControlsInUse(", "\n}"), "switch (AvP.PlayerType)"), "the controls that count there are those of whoever is being played");

	// the developers' tag
	Check(Has(render, "\"KEYS: %s VIEW, F7 TAG, F8 CAPTURE\", GetDescriptionOfKey(static_cast<unsigned char>(ViewSettings_KeyOnLine(kViewKeyDevViews))));"), "the tag says which key the dev views really have");
	Check(Has(render, "strncpy(lightingKey, GetDescriptionOfKey(static_cast<unsigned char>(ViewSettings_KeyOnLine(kViewKeyRendering))), sizeof(lightingKey) - 1);"), "and keeps the rendering key's name, because the next one can be written over it");
	Check(Has(render, "\"KEYS: %s LIGHTING, %s SHADOWS\", lightingKey, GetDescriptionOfKey(static_cast<unsigned char>(ViewSettings_KeyOnLine(kViewKeyShadows))));"), "and says which the other two have");

	// the game builds it
	Check(Has(ReadSourceFile("win32/VS2026/AvP.vcxproj"), "3dc\\view_keys.cpp"), "the game builds these rules");
	Check(Has(ReadSourceFile("win32/VS2026/AvP.vcxproj.filters"), "3dc\\view_keys.cpp"), "and the project's list of files shows them");
}

int main()
{
	TestJudging();
	TestReadingTheFile();
	TestSettling();
	TestActing();
	TestJoiningTheGame();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");

	return checksFailed ? 1 : 0;
}
