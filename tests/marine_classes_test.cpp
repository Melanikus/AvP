// Tests the marine classes M5 adds to multiplayer: the numbers and choices in 3dc/avp/marine_classes.h, and that the game is
// wired to them. Written 2026-11-06 by Melanikus for the AvP port.
//
// Nothing here needs the game or its data. It reads the source for the wiring. That the menus look right, and that a class
// really starts with these guns, takes a run: gun= with class=, which starts a skirmish.

#include "avp/marine_classes.h"

#include <stdio.h>

#include <fstream>
#include <sstream>
#include <string>

static int checks = 0;
static int failures = 0;

static void Check(bool good, const char *what)
{
	checks++;
	failures += good ? 0 : 1;
	printf("  %s  %s\n", good ? "ok" : "FAILED", what);
}

static std::string ReadSource(const char *name)
{
	const std::string path = __FILE__;
	const std::string::size_type tests = path.rfind("tests");
	if (tests == std::string::npos)
	{
		return "";
	}
	std::ifstream in((path.substr(0, tests) + name).c_str(), std::ios::binary);
	std::stringstream all;
	all << in.rdbuf();
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

static bool Has(const std::string &text, const char *what)
{
	return text.find(what) != std::string::npos;
}

static int Count(const std::string &text, const char *what)
{
	int found = 0;
	const std::string wanted = what;
	for (std::string::size_type at = text.find(wanted); at != std::string::npos; at = text.find(wanted, at + wanted.size()))
	{
		found++;
	}
	return found;
}

static void TestSlider()
{
	Check(ClassSlider_ToClass(kClassSliderFirstClass) == 1, "the first class on the slider is stock's first class");
	Check(ClassSlider_ToClass(10) == kClassPistols, "stock's last class is where it always was");
	Check(ClassSlider_ToClass(11) == kClassShotgun, "then the shotgun");
	Check(ClassSlider_ToClass(12) == kClassRevolver, "then the revolver");
	Check(ClassSlider_ToClass(kClassSliderLast) == kClassM79, "and the M79 is last");

	Check(ClassSlider_NewNameIndex(0) == -1, "a plain marine's name is stock's");
	Check(ClassSlider_NewNameIndex(10) == -1, "and so is stock's last class's");
	Check(ClassSlider_NewNameIndex(11) == 0, "the shotgun has the first new name");
	Check(ClassSlider_NewNameIndex(12) == 1, "the revolver has the second");
	Check(ClassSlider_NewNameIndex(13) == 2, "the M79 has the third");
	Check(ClassSlider_NewNameIndex(14) == -1, "and there's no fourth");
}

static void TestStarts()
{
	Check(kClassShotgunStartRounds == 60, "a shotgun marine starts with 60 spare rounds, M5's 5 boxes of 12");
	Check(kClassRevolverStartRounds == 100, "a revolver marine starts with 100, M5's 20 boxes of 5");
	Check(kClassM79StartEachKind == 15, "an M79 marine starts with 15 of each kind");
	Check(kSidearmRevolverStartRounds == 50, "a revolver as a sidearm has 50, M5's 10 boxes of 5");
	Check(kSidearmPistolStartMagazines == 10, "a pistol as a sidearm has 10 magazines, as stock's does");
}

static void TestSidearm()
{
	Check(Sidearm_Choose(false, kClassShotgun, true, true, kSidearmRevolver) == kSidearmNone, "only a marine has one");
	Check(Sidearm_Choose(true, kClassGeneral, true, true, kSidearmRevolver) == kSidearmNone, "a marine with no class has none");
	Check(Sidearm_Choose(true, kClassShotgun, false, true, kSidearmRevolver) == kSidearmNone, "none when the host has turned them off");

	Check(Sidearm_Choose(true, kClassPistols, true, true, kSidearmPistol) == kSidearmRevolver, "a pistols marine gets the revolver, whatever he chose");
	Check(Sidearm_Choose(true, kClassPistols, true, false, kSidearmRevolver) == kSidearmNone, "and nothing if it isn't allowed");

	Check(Sidearm_Choose(true, kClassShotgun, true, true, kSidearmPistol) == kSidearmPistol, "another class gets the pistol if he chose it");
	Check(Sidearm_Choose(true, kClassShotgun, true, true, kSidearmRevolver) == kSidearmRevolver, "and the revolver if he chose that");
	Check(Sidearm_Choose(true, kClassShotgun, true, false, kSidearmRevolver) == kSidearmPistol, "but the pistol if the revolver isn't allowed");
	Check(Sidearm_Choose(true, 2, true, true, kSidearmRevolver) == kSidearmRevolver, "a stock class can choose it too");
	Check(Sidearm_Choose(true, kClassShotgun, true, true, 99) == kSidearmPistol, "a choice that isn't one is the pistol");
	Check(Sidearm_Choose(true, kClassShotgun, true, true, -1) == kSidearmPistol, "and so is one below 0");
}

static void TestSettingsFile()
{
	Check(Setting_FromFile(0, 0, 8, 8) == 0, "the lowest number is kept");
	Check(Setting_FromFile(8, 0, 8, 3) == 8, "and the highest");
	Check(Setting_FromFile(9, 0, 8, 3) == 3, "one above the highest is ignored");
	Check(Setting_FromFile(-1, 0, 8, 3) == 3, "and so is one below the lowest");
	Check(Setting_FromFile(2147483647, 0, 1, 1) == 1, "and any size a file can say");
}

static void TestWiring()
{
	const std::string net = ReadSource("3dc/avp/win95/pldnet.cpp");
	const std::string netHeader = ReadSource("3dc/avp/win95/pldnet.h");
	const std::string menus = ReadSource("3dc/avp/win95/frontend/menus.cpp");
	const std::string menuData = ReadSource("3dc/avp/win95/frontend/menu_data.cpp");
	const std::string config = ReadSource("3dc/avp/win95/frontend/mp_config.cpp");
	const std::string inventory = ReadSource("3dc/avp/inventory.cpp");
	const std::string visible = ReadSource("3dc/avp/pvisible.cpp");
	const std::string text = ReadSource("3dc/avp/language.cpp");
	const std::string cheat = ReadSource("3dc/avp/win95/cheat.cpp");

	Check(Has(menus, "netGameData.myCharacterSubType = (NETGAME_SPECIALISTCHARACTERTYPE)ClassSlider_ToClass(MP_Species);"), "the menu turns the class slider into a class with ClassSlider_ToClass, the sum checked above");
	Check(!Has(menus, "(MP_Species-2)"), "and keeps no copy of that sum of its own");
	Check(Has(netHeader, "#define NUM_PC_SUBTYPES 12"), "the game knows there are 12 classes");
	Check(!Has(netHeader, "NGSCT_Sniper"), "with no sniper among them");
	Check(Has(net, "static_assert(NGSCT_Pistols == kClassPistols"), "and the compiler checks the two lists agree");

	Check(Has(net, "\n\tnetGameData.maxMarineShotgun = 8;"), "a new class's limit starts at 8");
	Check(Has(net, "\n\t\tnetGameData.maxMarineM79 = 8;"), "and skirmish forces it to 8");
	Check(Has(net, "\tnetGameData.allowStaff = 0;"), "the staff is off until a host allows it");
	Check(Has(net, "\tFALSE,  // BOOL allowStaff;"), "from the very start too");
	Check(Has(net, "netGameData.maxMarineRevolver = 0;"), "a class that isn't allowed has no places");
	Check(Has(net, "CharacterSubTypesAvailable[NGSCT_M79] = netGameData.maxMarineM79;"), "each class's limit counts");
	Check(Has(net, "CharacterSubTypesAvailable[NGSCT_Shotgun]--;"), "and a player in it takes a place");
	Check(Has(net, "if (CharacterSubTypesAvailable[NGSCT_Revolver]) {"), "the slider stops at a class with room");
	Check(Has(net, "if (*species > kClassSliderLast) {"), "and comes round after the last");
	Check(Has(net, "*species = kClassSliderLast;"), "both ways");
	Check(Has(net, "case 13:\n\t\t\t\tnetGameData.myCharacterType = NGCT_Marine;"), "a host can start as a new class");
	Check(Has(menus, "case 13:\n\t\t\t\t\t\tnetGameData.myCharacterType = NGCT_Marine;"), "and so can someone joining");

	Check(Has(menus, "TEXTSTRING_MULTIPLAYER_MARINE_SHOTGUN + ClassSlider_NewNameIndex("), "the slider shows the new names");
	Check(Count(menuData, "{kClassSliderLast},\t{&MP_Species}") == 2, "both class menus reach the last class");
	Check(Count(menuData, "{&MP_Sidearm}") == 2, "and both have the sidearm");
	Check(Count(menuData, "{&netGameData.maxMarineShotgun}") == 2, "the host and the joiner see the limit");
	Check(Count(menuData, "{&netGameData.allowM79}") == 3, "the host, the joiner and skirmish see the allow row");
	Check(Count(menuData, "{&netGameData.allowStaff}") == 3, "and Allow Staff");
	Check(Has(text, "\"Marine - Shotgun\""), "the names are in the game's text");
	Check(Has(text, "\"Allow Staff\""), "and Allow Staff");

	Check(Has(config, "*setting = Setting_FromFile(number, lowest, highest, *setting);"), "a setting from a file is checked");
	Check(Has(config, "if (file.ReadBytes((uint8_t*)&number, sizeof(number)) == sizeof(number))"), "and one that isn't there keeps its default");
	Check(config.find("ReadLaterSetting(file, &netGameData.maxMarineShotgun, 0, 8);") > config.find("netGameData.customLevelName[0] = 0;"), "the new settings come after the old ones, so an old file still reads");
	Check(Has(config, "ReadLaterSetting(file, &netGameData.allowStaff, 0, 1);"), "Allow Staff is read");
	Check(config.find("file.PutInt32LE(netGameData.maxMarineShotgun);") > config.find("file.WriteBytes((uint8_t*)netGameData.customLevelName, length);"), "and they're written after the old ones too");
	Check(Has(config, "file.PutInt32LE(netGameData.allowStaff);"), "Allow Staff is written");

	Check(!Has(inventory, "SPECIALIST_PISTOLS"), "the classes use the sidearm, not the old pistol rule");
	Check(Count(inventory, "StartWithSidearmOrCudgel(playerStatusPtr);") == 9, "nine classes have the sidearm or the cudgel");
	Check(Has(inventory, "a=StartWithClassGun(playerStatusPtr, WEAPON_AUTOSHOTGUN, kClassShotgunStartRounds);"), "the shotgun class starts with the shotgun");
	Check(Has(inventory, "a=StartWithClassGun(playerStatusPtr, WEAPON_BEAMCANNON, kClassRevolverStartRounds);"), "the revolver class with the revolver");
	Check(Has(inventory, "M79_SetEveryKind(&playerStatusPtr->WeaponSlot[a], kClassM79StartEachKind);"), "the M79 class with its three kinds");
	Check(Has(inventory, "slot = SlotForThisWeapon(WEAPON_PULSERIFLE);\n\t\tspares = 5;"), "a gun with no model file gives the pulse rifle, not a bad slot");
	Check(!Has(inventory, "a=SlotForThisWeapon(WEAPON_BEAMCANNON);"), "and nothing takes the revolver away again at the start");
	Check(Has(inventory, "if (!SameAsSinglePlayer() && netGameData.allowStaff)"), "a Predator has the staff when it's allowed");
	Check(Has(inventory, "ammoID!=AMMO_PLAYER_SHOTGUN && !IsMySidearmsAmmo(ammoID)"), "a shotgun marine picks up its ammo and his sidearm's");
	Check(Has(inventory, "ammoID!=AMMO_M79_STANDARD && ammoID!=AMMO_M79_FIRE && ammoID!=AMMO_M79_FRAG && !IsMySidearmsAmmo(ammoID)"), "an M79 marine its three kinds");
	Check(Has(inventory, "weaponID!=WEAPON_AUTOSHOTGUN && !IsMySidearm(weaponID)"), "the shotgun class picks up its gun");
	Check(Has(inventory, "weaponID!=WEAPON_BEAMCANNON && !IsMySidearm(weaponID)"), "the revolver class picks up its gun");
	Check(Has(inventory, "weaponID!=WEAPON_MYSTERYGUN && !IsMySidearm(weaponID)"), "the M79 class picks up its gun");
	Check(Has(inventory, "if (!SameAsSinglePlayer() && ammoID == AMMO_SNIPER)"), "nobody picks up the sniper's ammo in a network game, except the mode that plays as single player");
	Check(Has(inventory, "if (IsKeptOutOfNetworkGames(weaponID))"), "or its gun");

	Check(Has(visible, "objectstatusptr->subType==WEAPON_SONICCANNON||"), "the sniper's gun doesn't appear in a network game");
	Check(Has(visible, "objectstatusptr->subType==AMMO_SNIPER||"), "nor its ammo");
	Check(Has(visible, "(!netGameData.allowStaff && objectstatusptr->subType==WEAPON_PRED_STAFF)"), "a staff that isn't allowed doesn't appear");
	Check(Has(visible, "(!netGameData.allowM79 && objectstatusptr->subType==WEAPON_MYSTERYGUN)"), "nor a gun that isn't");
	Check(Has(visible, "(!netGameData.allowRevolver && objectstatusptr->subType==AMMO_REVOLVER)"), "nor its ammo");

	Check(Has(cheat, "if (!SameAsSinglePlayer() && wdPtr->WeaponIDNumber==WEAPON_SONICCANNON) continue;"), "the test command leaves the sniper out of a network game");

	const std::string ghost = ReadSource("3dc/avp/win95/pldghost.cpp");
	const std::string setup = ReadSource("3dc/avp/win95/npcsetup.cpp");

	Check(Count(ghost, "root_section = MarineWithNewGun(\"Marine with Shotgun\");") == 2, "a marine with the shotgun has its model, alive and dead");
	Check(Count(ghost, "root_section = MarineWithNewGun(\"Sniper Marine\");") == 2, "and with the sniper rifle, in a mirror");
	Check(Count(ghost, "root_section = MarineWithNewGun(\"Revolver Marine\");") == 2, "and the revolver");
	Check(Count(ghost, "root_section = MarineWithNewGun(\"M79 Marine\");") == 2, "and the M79");
	Check(Has(ghost, "return found ? found : GetNamedHierarchyFromLibrary(\"hnpcmarine\",\"marine with pulse rifle\");"), "a missing model file gives the pulse rifle, not nothing");
	Check(Has(ghost, "Sound_Play(SID_WEAPON_SHOTGUN_FIRE,\"hd\""), "others hear the shotgun");
	Check(Has(ghost, "Sound_Play(SID_WEAPON_REVOLVER_FIRE,\"hd\""), "and the revolver");
	Check(Has(ghost, "case(WEAPON_MYSTERYGUN):"), "and the M79");
	Check(Has(ghost, "if (sbPtr->shapeIndex<0)\n"), "a dropped gun whose model isn't there still has a shape");
	Check(!Has(ghost, "Unexpected weapon type"), "and a number from another machine that isn't a gun has one too");
	Check(Has(visible, "toolsdata.shapeIndex=GetLoadedShapeMSL(\"shotgunpick\");"), "a new gun on the floor has a shape");
	Check(Has(visible, "type==WEAPON_MYSTERYGUN && !netGameData.allowM79"), "unless the host doesn't allow it");
	Check(Has(setup, "{ \"ShotgunBot.rif\", \"MelPyth_hnpcmarine.rif\" }"), "both model files are looked for");
	Check(Has(setup, "if (Load_HNPC[HNPC_Marine])\n"), "whenever the marine is");
	Check(setup.find("LoadedNPC tempnpc(HNPC_FileNames[i]);") < setup.find("marinesWithNewGuns"), "and after him, so that the stock marine's looks are found first");
	Check(Has(setup, "LoadedNPC tempnpc(\"ShotgunPickup.rif\");"), "and the dropped gun's file, in a network game");
	Check(Has(net, "(weaponPtr->WeaponIDNumber == WEAPON_BEAMCANNON) ||"), "others see the revolver's flash");
	Check(Has(net, "case AMMO_REVOLVER :\n\t\t\treturn ICON_MARINE_PISTOL;"), "a kill by the revolver has a picture");
	Check(Has(net, "case AMMO_M79_DIRECT_HIT :"), "and by the M79");

}

int main()
{
	TestSlider();
	TestStarts();
	TestSidearm();
	TestSettingsFile();
	TestWiring();

	printf("%d checks, %d failed\n", checks, failures);
	return failures == 0 ? 0 : 1;
}
