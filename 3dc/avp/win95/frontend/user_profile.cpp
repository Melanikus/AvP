/* KJL 15:17:31 10/12/98 - user profile stuff */
// AvP port by Barry Duncan.
// Modified 2026-11-03 by Melanikus for the AvP port:
//   a saved profile is loaded only when complete, its name ends inside its box and cannot lead outside
//   the profile folder, its look speeds stay within the menu sliders' range; long folder paths are refused;
//   deleting a profile skips a save file whose full name is too long, so it cannot overflow or hit another file;
//   a profile file whose inner name is not its own file name is skipped, so it cannot delete another profile
// Modified 2026-11-04 by Melanikus for the AvP port:
//   a profile's difficulties are limited as it's read, and its multiplayer name has an end
// Modified 2026-11-05 by Melanikus for the AvP port:
//   a profile saved before the shoulder lamp gets its key without anything else changing
#include "list_tem.hpp"
#include "3dc.h"
#include "inline.h"
#include "module.h"
#include "strategy_def.h"
#include "user_profile.h"
#include "language.h"
#include "gammacontrol.h"
#include "psnd.h"
#include "MusicPlayer.h"
#define UseLocalAssert TRUE
#include "ourasert.h"
#include "pldnet.h"
#include <time.h>
#include "utilities.h"
#include "shoulder_lamp.h"
#include "logString.h"
#include "profile_checks.h"

static int LoadUserProfiles(void);

void EmptyUserProfilesList();
static int MakeNewUserProfile(void);
static void InsertProfileIntoList(AVP_USER_PROFILE *profilePtr);
static int ProfileIsMoreRecent(AVP_USER_PROFILE *profilePtr, AVP_USER_PROFILE *profileToTestAgainstPtr);
static void SetDefaultProfileOptions(AVP_USER_PROFILE *profilePtr);

extern int FmvSoundVolume;
extern int EffectsSoundVolume;
extern int MoviesAreActive;
extern int IntroOutroMoviesAreActive;
extern char MP_PlayerName[];
extern int AutoWeaponChangeOn;


List<AVP_USER_PROFILE *> UserProfilesList;
static AVP_USER_PROFILE DefaultUserProfile =
{
	"",
};
static AVP_USER_PROFILE *CurrentUserProfilePtr;

extern void ExamineSavedUserProfiles(void)
{
	// delete any existing profiles
	EmptyUserProfilesList();

	LoadUserProfiles();

	// this creates the "New Profile" entry which allows you to make new profiles ingame
	AVP_USER_PROFILE *profilePtr = new AVP_USER_PROFILE;
	*profilePtr = DefaultUserProfile;

	::GetLocalTime(&profilePtr->TimeLastUpdated);
	::SystemTimeToFileTime(&profilePtr->TimeLastUpdated, &profilePtr->FileTime);

	strncpy(profilePtr->Name, GetTextString(TEXTSTRING_USERPROFILE_NEW), MAX_SIZE_OF_USERS_NAME);
	profilePtr->Name[MAX_SIZE_OF_USERS_NAME] = 0;
	SetDefaultProfileOptions(profilePtr);

	InsertProfileIntoList(profilePtr);
}

extern int NumberOfUserProfiles(void)
{
	int n = UserProfilesList.size();

	LOCALASSERT(n>0);

	return n-1;
}

extern AVP_USER_PROFILE *GetFirstUserProfile(void)
{
	CurrentUserProfilePtr=UserProfilesList.first_entry();
	return CurrentUserProfilePtr;
}

extern AVP_USER_PROFILE *GetNextUserProfile(void)
{
	if (CurrentUserProfilePtr == UserProfilesList.last_entry())
	{
		CurrentUserProfilePtr = UserProfilesList.first_entry();
	}
	else
	{
		CurrentUserProfilePtr = UserProfilesList.next_entry(CurrentUserProfilePtr);
	}
	return CurrentUserProfilePtr;
}

void EmptyUserProfilesList()
{
	while (UserProfilesList.size())
	{
		delete UserProfilesList.first_entry();
		UserProfilesList.delete_first_entry();
	}
}

// A profile's name becomes part of file paths: its .prf file and its .sav files. A name with a folder separator or a
// drive colon in it, such as ..\..\x, would reach files outside the profile folder. A broken or edited profile file
// could hold such a name, and it can be typed in the menu, but the file of a real profile cannot be named that way.
// So those names are never used in a path.
extern BOOL UserProfileNameIsSafe(const char *name)
{
	return (strpbrk(name, "/\\:") == NULL);
}

// Deleting a profile removes <name>.prf and <name>_1.sav and so on, using the name stored inside the profile. The game
// always saves a profile as <name>.prf, so that name is also the name of its file. A file where the two differ was
// made by hand: left in the list, deleting it would remove some other player's profile and saved games.
static BOOL UserProfileNameMatchesFile(const char *name, const char *fileName)
{
	size_t const length = strlen(name);

	return (_strnicmp(fileName, name, length) == 0) && (_stricmp(fileName + length, USER_PROFILES_SUFFIX) == 0);
}

// See ShoulderLamp_KeyFromProfile. The lamp's own byte is the last of the marine's, so the ones before it are all the others.
static void SettleShoulderLampKeys(PLAYER_INPUT_CONFIGURATION *primary, PLAYER_INPUT_CONFIGURATION *secondary)
{
	unsigned char const *const primaryKeys = (unsigned char const *)primary;
	unsigned char const *const secondaryKeys = (unsigned char const *)secondary;
	bool defaultIsTaken = false;

	for (int i = 0; i < NUMBER_OF_MARINE_INPUTS - 1; i++)
	{
		defaultIsTaken = defaultIsTaken || primaryKeys[i] == KEY_G || secondaryKeys[i] == KEY_G;
	}

	primary->Marine_ShoulderLamp = ShoulderLamp_KeyFromProfile(primary->Marine_ShoulderLamp, KEY_ESCAPE, KEY_G, defaultIsTaken, KEY_VOID);
	secondary->Marine_ShoulderLamp = ShoulderLamp_KeyFromProfile(secondary->Marine_ShoulderLamp, KEY_ESCAPE, KEY_VOID, false, KEY_VOID);
}

extern int SaveUserProfile(AVP_USER_PROFILE *profilePtr)
{
	if (!UserProfileNameIsSafe(profilePtr->Name)) {
		return 0;
	}

	// avp_open_userfile will add SaveFolderPath for us
	char *filename = new char [strlen(USER_PROFILES_PATH) + strlen(profilePtr->Name) + strlen(USER_PROFILES_SUFFIX) + 1];

	strcpy(filename, USER_PROFILES_PATH);
	strcat(filename, profilePtr->Name);
	strcat(filename, USER_PROFILES_SUFFIX);

	FILE* file = avp_open_userfile(filename, "wb");

	delete[] filename;

	if (!file) {
		return 0;
	}

	SaveSettingsToUserProfile(profilePtr);

	fwrite(profilePtr, sizeof(AVP_USER_PROFILE), 1, file);
	fclose(file);

	return 1;
}

extern void DeleteUserProfile(int number)
{
	AVP_USER_PROFILE *profilePtr = GetFirstUserProfile();

	for (int i=0; i<number; i++) profilePtr = GetNextUserProfile();

	if (!UserProfileNameIsSafe(profilePtr->Name)) {
		return;
	}

	char *savePath = GetSaveFolderPath();
	size_t savePathLength = strlen(savePath);

	char *filename = new char[savePathLength + strlen(USER_PROFILES_PATH)+strlen(profilePtr->Name)+strlen(USER_PROFILES_SUFFIX)+1];
	strcpy(filename, savePath);
	strcat(filename, USER_PROFILES_PATH);
	strcat(filename, profilePtr->Name);
	strcat(filename, USER_PROFILES_SUFFIX);

	::DeleteFile(filename);

	// now delete all the user's savegame files
	delete[] filename;

	filename = new char[MAX_PATH];

	for (int i=0; i < NUMBER_OF_SAVE_SLOTS; i++)
	{
		// the save folder can be a long path: _snprintf stops at the end of filename and says so with a negative
		// number. A name that did not fit is skipped, because a shortened name could be some other file.
		if (_snprintf(filename, MAX_PATH - 1, "%s%s%s_%d.sav", savePath, USER_PROFILES_PATH, profilePtr->Name, i+1) >= 0)
		{
			filename[MAX_PATH - 1] = '\0';
			::DeleteFile(filename);
		}
	}

	delete[] filename;
}

static void InsertProfileIntoList(AVP_USER_PROFILE *profilePtr)
{
	if (UserProfilesList.size())
	{
		AVP_USER_PROFILE *profileInListPtr = GetFirstUserProfile();

		for (int i=0; i < UserProfilesList.size(); i++, profileInListPtr = GetNextUserProfile())
		{
			if (ProfileIsMoreRecent(profilePtr, profileInListPtr))
			{
				UserProfilesList.add_entry_before(profilePtr, profileInListPtr);
				return;
			}
		}
	}
	UserProfilesList.add_entry(profilePtr);
}

static int ProfileIsMoreRecent(AVP_USER_PROFILE *profilePtr, AVP_USER_PROFILE *profileToTestAgainstPtr)
{
	if (::CompareFileTime(&profilePtr->FileTime, &profileToTestAgainstPtr->FileTime) == 1)
	{
		return 1;
	}
		
	return 0;
}

#include "FileStream.h"

static int LoadUserProfiles(void)
{
#if 0 // filestream test
	std::vector<std::string> profileFiles;
	std::string path = GetSaveFolderPath();
	path += "User_Profiles/";

//	path += USER_PROFILES_WILDCARD_NAME;

	if (FindFiles(path + "*.prf", profileFiles))
	{
		for (size_t i = 0; i < profileFiles.size(); i++)
		{
			FileStream findFile;

			findFile.Open(path + profileFiles[i], FileStream::FileRead, true);
			if (!findFile.IsGood())
				continue;

			AVP_USER_PROFILE *profilePtr = new AVP_USER_PROFILE;

			// read in profile file
			findFile.ReadBytes((uint8_t*)profilePtr, sizeof(AVP_USER_PROFILE));

			// do extra stuff
/* TODO
			FILETIME ftLocal;
			FileTimeToLocalFileTime(&wfd.ftLastWriteTime, &ftLocal);
			FileTimeToSystemTime(&ftLocal, &profilePtr->TimeLastUpdated);
			profilePtr->FileTime = ftLocal;
*/
			InsertProfileIntoList(profilePtr);
		}
	}

	return 1;
#else

	TCHAR strPath[MAX_PATH];

	// A save folder path too long to hold the search pattern is not cut short, which could search the wrong
	// place: it goes to the log, and the game carries on as if there were no profiles.
	if (_snprintf(strPath, MAX_PATH - 1, "%s%s", GetSaveFolderPath(), USER_PROFILES_WILDCARD_NAME) < 0)
	{
		LogErrorString("the path for finding user profiles is too long");
		return 0;
	}
	strPath[MAX_PATH - 1] = '\0';

	const char* load_name = &strPath[0];

	// allow a wildcard search
	WIN32_FIND_DATA wfd;

	HANDLE hFindFile = avp_FindFirstFile(load_name, &wfd);
	if (INVALID_HANDLE_VALUE == hFindFile)
	{
//		printf("File Not Found: <%s>\n",load_name);
		return 0;
	}

	// get any path in the load_name
	ptrdiff_t nPathLen = 0;
	const char* pColon = strrchr(load_name,':');
	if (pColon) nPathLen = pColon - load_name + 1; // n chars before :
	const char* pBackSlash = strrchr(load_name,'/');
	if (pBackSlash)
	{
		ptrdiff_t nLen = pBackSlash - load_name + 1;
		if (nLen > nPathLen) nPathLen = nLen;
	}
	const char* pSlash = strrchr(load_name,'/');
	if (pSlash)
	{
		ptrdiff_t nLen = pSlash - load_name + 1;
		if (nLen > nPathLen) nPathLen = nLen;
	}

	do
	{
		if
		(
			!(wfd.dwFileAttributes &
				(FILE_ATTRIBUTE_DIRECTORY
				|FILE_ATTRIBUTE_SYSTEM
				|FILE_ATTRIBUTE_HIDDEN
				|FILE_ATTRIBUTE_READONLY))
				// not a directory, hidden or system file
		)
		{
			/* 
				generate the full file name.
				fill pszFullPath with characters up until last / (this will remove the *.prf from the path)
				then adds the found profile name from avp_FindFirstFile - eg "Bob.prf"
			*/
			char* pszFullPath = new char [nPathLen + strlen(wfd.cFileName)+1];
			strncpy(pszFullPath,load_name,nPathLen);
			strcpy(pszFullPath + nPathLen,wfd.cFileName);

			//make sure the file is a rif file
			HANDLE rif_file;
			rif_file = avp_CreateFile(pszFullPath, GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS, 0);
			if (rif_file==INVALID_HANDLE_VALUE)
			{
//				printf("couldn't open %s\n",pszFullPath);
				delete[] pszFullPath;
				continue;
			}

			AVP_USER_PROFILE *profilePtr = new AVP_USER_PROFILE;
			DWORD bytes_read;

			// A profile file is copied straight into the profile in memory. A broken or
			// edited file could be shorter than a whole profile, which would leave the
			// rest of it as leftover memory, so such a file is skipped like an unreadable
			// one. Every profile the game saved is complete, so it still loads.
			if (!ReadFile(rif_file, profilePtr, sizeof(AVP_USER_PROFILE), &bytes_read, 0) || bytes_read != sizeof(AVP_USER_PROFILE))
			{
				CloseHandle (rif_file);
				delete[] pszFullPath;
				delete profilePtr;
				continue;
			}

			// The name box holds MAX_SIZE_OF_USERS_NAME (15) characters and then a zero
			// byte that marks where the name stops. The game never saves a longer name
			// (its text field stops at 15), but the file might not have that zero byte,
			// so we always put one in the last place. Without it, code that reads the
			// name would run on past the end of the box.
			profilePtr->Name[MAX_SIZE_OF_USERS_NAME] = 0;

			// The same goes for the multiplayer name, and the levels' difficulties are limited too. See profile_checks.h.
			// These others go through a check where they're used: the cheat modes can't hang the menu (profile_checks.h), every key number has a place in the key lists, a volume is held to its table,
			// a menu setting picks its text through GetTextString, which refuses a number off the end, and a personal best is only ever a number to print.
			profilePtr->MultiplayerCallsign[sizeof(profilePtr->MultiplayerCallsign) - 1] = 0;
			ProfileChecks_LimitLevels(profilePtr->LevelCompleted);

			// a name that could reach outside the profile folder, or that is not this file's own name: skip the file,
			// like an unreadable one
			if (!UserProfileNameIsSafe(profilePtr->Name) || !UserProfileNameMatchesFile(profilePtr->Name, wfd.cFileName))
			{
				CloseHandle (rif_file);
				delete[] pszFullPath;
				delete profilePtr;
				continue;
			}

			FILETIME ftLocal;
			FileTimeToLocalFileTime(&wfd.ftLastWriteTime, &ftLocal);
			FileTimeToSystemTime(&ftLocal, &profilePtr->TimeLastUpdated);
			profilePtr->FileTime = ftLocal;
			InsertProfileIntoList(profilePtr);
			CloseHandle (rif_file);
			delete[] pszFullPath;
		}
	}
	while (::FindNextFile(hFindFile, &wfd));

	if (ERROR_NO_MORE_FILES != GetLastError())
	{
		//	printf("Error finding next file\n");
	}

	::FindClose(hFindFile);

	return 1;
#endif
}

static void SetDefaultProfileOptions(AVP_USER_PROFILE *profilePtr)
{
	// set Gamma
	RequestedGammaSetting = 128;
	
	// controls
	MarineInputPrimaryConfig     = DefaultMarineInputPrimaryConfig;
	MarineInputSecondaryConfig   = DefaultMarineInputSecondaryConfig;
	PredatorInputPrimaryConfig   = DefaultPredatorInputPrimaryConfig;
	PredatorInputSecondaryConfig = DefaultPredatorInputSecondaryConfig;
	AlienInputPrimaryConfig      = DefaultAlienInputPrimaryConfig;
	AlienInputSecondaryConfig    = DefaultAlienInputSecondaryConfig;
	ControlMethods               = DefaultControlMethods;
	JoystickControlMethods       = DefaultJoystickControlMethods;

	FmvSoundVolume     = ONE_FIXED/512;
	EffectsSoundVolume = VOLUME_DEFAULT;
	musicVolume        = CDDA_VOLUME_DEFAULT;
	MoviesAreActive    = 1;
	AutoWeaponChangeOn = TRUE;
	IntroOutroMoviesAreActive = 1;

	strcpy(MP_PlayerName, "DeadMeat");

	SetToDefaultDetailLevels();

	for (int a=0; a < I_MaxDifficulties; a++)
	{
		for (int b=0; b < AVP_ENVIRONMENT_END_OF_LIST; b++)
		{
			profilePtr->PersonalBests[a][b] = DefaultLevelGameStats;
		}
	}

	SaveSettingsToUserProfile(profilePtr);
}

extern void GetSettingsFromUserProfile(void)
{
	RequestedGammaSetting = UserProfilePtr->GammaSetting;

	MarineInputPrimaryConfig      = UserProfilePtr->MarineInputPrimaryConfig;
	MarineInputSecondaryConfig    = UserProfilePtr->MarineInputSecondaryConfig;
	SettleShoulderLampKeys(&MarineInputPrimaryConfig, &MarineInputSecondaryConfig);
	PredatorInputPrimaryConfig    = UserProfilePtr->PredatorInputPrimaryConfig;
	PredatorInputSecondaryConfig  = UserProfilePtr->PredatorInputSecondaryConfig;
	AlienInputPrimaryConfig       = UserProfilePtr->AlienInputPrimaryConfig;
	AlienInputSecondaryConfig     = UserProfilePtr->AlienInputSecondaryConfig;
	ControlMethods                = UserProfilePtr->ControlMethods;
	JoystickControlMethods        = UserProfilePtr->JoystickControlMethods;

	// the profile is read from a .prf file as raw bytes: keep the look sensitivities in their menu sliders' range
	// (0 to 3 x the default, menu_data.cpp), since they multiply the mouse / pad deltas every frame (user_io.cpp)
	if (ControlMethods.MouseXSensitivity > DEFAULT_MOUSEX_SENSITIVITY*3)
		ControlMethods.MouseXSensitivity = DEFAULT_MOUSEX_SENSITIVITY*3;
	if (ControlMethods.MouseYSensitivity > DEFAULT_MOUSEY_SENSITIVITY*3)
		ControlMethods.MouseYSensitivity = DEFAULT_MOUSEY_SENSITIVITY*3;
	if (JoystickControlMethods.JoystickTrackerBallHorizontalSensitivity > DEFAULT_TRACKERBALL_HORIZONTAL_SENSITIVITY*3)
		JoystickControlMethods.JoystickTrackerBallHorizontalSensitivity = DEFAULT_TRACKERBALL_HORIZONTAL_SENSITIVITY*3;
	if (JoystickControlMethods.JoystickTrackerBallVerticalSensitivity > DEFAULT_TRACKERBALL_VERTICAL_SENSITIVITY*3)
		JoystickControlMethods.JoystickTrackerBallVerticalSensitivity = DEFAULT_TRACKERBALL_VERTICAL_SENSITIVITY*3;

	MenuDetailLevelOptions        = UserProfilePtr->DetailLevelSettings;
	FmvSoundVolume                = UserProfilePtr->FmvSoundVolume;
	EffectsSoundVolume            = UserProfilePtr->EffectsSoundVolume;
	musicVolume                   = UserProfilePtr->MusicVolume;
	MoviesAreActive               = UserProfilePtr->MoviesAreActive;
	IntroOutroMoviesAreActive     = UserProfilePtr->IntroOutroMoviesAreActive;
	AutoWeaponChangeOn            = !UserProfilePtr->AutoWeaponChangeDisabled;
	strncpy(MP_PlayerName, UserProfilePtr->MultiplayerCallsign, 15);

	SetDetailLevelsFromMenu();
}


extern void SaveSettingsToUserProfile(AVP_USER_PROFILE *profilePtr)
{
	profilePtr->GammaSetting = RequestedGammaSetting;

	profilePtr->MarineInputPrimaryConfig     = MarineInputPrimaryConfig;
	profilePtr->MarineInputSecondaryConfig   = MarineInputSecondaryConfig;
	profilePtr->PredatorInputPrimaryConfig   = PredatorInputPrimaryConfig;
	profilePtr->PredatorInputSecondaryConfig = PredatorInputSecondaryConfig;
	profilePtr->AlienInputPrimaryConfig      = AlienInputPrimaryConfig;
	profilePtr->AlienInputSecondaryConfig    = AlienInputSecondaryConfig;
	profilePtr->ControlMethods               = ControlMethods;
	profilePtr->JoystickControlMethods       = JoystickControlMethods;
	profilePtr->DetailLevelSettings          = MenuDetailLevelOptions;
	profilePtr->FmvSoundVolume               = FmvSoundVolume;
	profilePtr->EffectsSoundVolume           = EffectsSoundVolume;
	profilePtr->MusicVolume                  = musicVolume;
	profilePtr->MoviesAreActive              = MoviesAreActive;
	profilePtr->IntroOutroMoviesAreActive    = IntroOutroMoviesAreActive;
	profilePtr->AutoWeaponChangeDisabled     = !AutoWeaponChangeOn;
	strncpy(profilePtr->MultiplayerCallsign, MP_PlayerName, 15);
}

extern void FixCheatModesInUserProfile(AVP_USER_PROFILE *profilePtr)
{
	for (int a=0; a < MAX_NUMBER_OF_CHEATMODES; a++) 
	{
		if (profilePtr->CheatMode[a] == 2)
		{
			profilePtr->CheatMode[a] = 1;
		}
	}
}
