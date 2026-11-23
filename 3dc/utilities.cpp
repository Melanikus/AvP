// Modified 2026-11-23 by Melanikus (Drake) for the AvP port (card 6, Tao 1 and 3):
//   avp_open_userfile, avp_fopen and avp_CreateFile write only through plain_write.h: never through a link, into a second name of
//   another file, or to a device. Reading is as it was.
// Modified 2026-11-22 by Melanikus for the AvP port:
//   the window title names the build's version (tools/build-and-package.ps1 writes it), not the time the compiler ran, so the same commit makes the same bytes
// Modified 2026-11-14 by Melanikus for the AvP port:
//   the map editor can give the game a folder of its own to keep its files in (SaveFolder_UseThisFolder, SAVE-FOLDER-1)
// Modified 2026-11-08 by Melanikus for the AvP port:
//   a second copy of the game on the same machine keeps its files in a folder of its own, by a fixed name (-dev -secondcopy)
// Modified 2026-11-04 by Melanikus for the AvP port:
//   a run keeps its files beside the game, and has nobody to show a message to
// Modified 2026-11-05 by Melanikus for the AvP port:
//   the save folder is never missing: without one the path is empty and the log says why once, where before the log and this went round until the game crashed
// Copyright (C) 2010 Barry Duncan. All Rights Reserved.
// The original author of this code can be contacted at: bduncan22@hotmail.com

// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
// INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
// FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// DEVELOPERS AND CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Modified 2026-11-05 by Melanikus for the AvP port:
//   took out avp_GetCommandLineArgs, which nothing called, and which read a command line from the settings file
//   what went wrong finding the save folder is kept for the log to say, because the log asks for that folder first
#include "utilities.h"
#include "hudson.h"
#include "plain_write.h"
#include <string>
#include <algorithm>
#include "configFile.h"
#include "logString.h"
#include "save_folder.h"
#include <assert.h>
#include <stdio.h>

#ifdef _WIN32
	#include <shlobj.h>
	#include <shlwapi.h>
#endif

static char saveFolder[MAX_PATH] = {0};
static bool noSaveFolder = false;

// Whether this is the second copy, whose files go in a folder of their own (SaveFolder_UseTheSecondCopy, save_folder.h)
static bool saveFolderIsTheSecondCopy = false;

// What went wrong finding the save folder. It isn't logged there, because the log asks for the save folder before it can write anything. See TheLogWriter in logString.cpp.
// It's plain characters because this can be asked for before the game's own start, when a std::string in this file might not be ready.
static char saveFolderProblem[MAX_PATH + 100] = {0};

// TODO: remove this
extern void ReleaseDirect3D();

#ifdef _WIN32
	extern HWND	hWndMain;
#endif

namespace Util {

void ChangeSlashes(std::string &theString)
{
	// change backslashes in path to forwardslashes
	std::replace(theString.begin(), theString.end(), '\\', '/');
}

void FtoBslash(std::string &theString)
{
	// change forwardslashes in path to backslashes
	std::replace(theString.begin(), theString.end(), '/', '\\');
}

/* 
 *	For passing to std::transform.
 *  Changes c to lowercase if is an uppercase letter
 *  or changes c to a forwardslash if it's a backslash
 *  else it leaves c unmodified.
 */
int LowercaseAndChangeSlash(char c)
{ 
	if ((c >= 'A') && (c <= 'Z'))
	{
		// to lowercase
		return c + 32;
	}

	if (c == '\\') {
		return '/';
	}

	return c;
}

// converts an int to a string and returns it
std::string IntToString(const int value)
{
	std::stringstream stream;

	// format int as a string
	stream << value;

	return stream.str();
}

// parses an int from a string and returns it
int StringToInt(const std::string &string)
{
	std::stringstream ss;
	int value = 0;

	// copy string to stringstream
	ss << string;
	// copy from stringstream to int
	ss >> value;
	
	return value;
}

}

char const *GetSaveFolderProblem()
{
	return noSaveFolder ? saveFolderProblem : NULL;
}

bool SaveFolder_UseTheSecondCopy()
{
	// Once the folder has been found, or found not to be there, everything since has gone where it said, so it isn't changed
	if (saveFolder[0] != 0 || noSaveFolder)
	{
		return false;
	}

	saveFolderIsTheSecondCopy = true;
	return true;
}

// Added 2026-11-14 by Melanikus for the AvP port (SAVE-FOLDER-1): a folder of the caller's choosing, the map editor's own. save_folder.h
// checks it (SaveFolder_WhyNotUsable), and what it gives back, ending in one '\', becomes the save folder, so GetSaveFolderPath just
// gives it from then on. A refusal changes nothing, and nothing falls back to My Games here: the caller stops.
bool SaveFolder_UseThisFolder(const char *folder)
{
	// Once the folder has been found, or found not to be there, everything since has gone where it said, so it isn't changed. (The same
	// test as SaveFolder_UseTheSecondCopy's, written the other way round, so a take-out row can name each on its own.)
	if (noSaveFolder || saveFolder[0] != 0)
	{
		return false;
	}

	// Checked into a space of its own, so a refused folder never leaves half a path where the save folder goes
	char checked[MAX_PATH];
	if (SaveFolder_WhyNotUsable(folder, checked, sizeof(checked)) != 0)
	{
		return false;
	}

	strcpy_s(saveFolder, sizeof(saveFolder), checked);
	return true;
}

// Added 2026-11-09 by Melanikus for the AvP port: what the kept online identity's file is named by (net_identity_store.cpp), which lives
// outside the save folder and needs a name of its own for each. The first was asked for there and defined nowhere, so the game couldn't
// link; net_identity_store_test now checks both are here.
bool SaveFolder_IsTheSecondCopy()
{
	return saveFolderIsTheSecondCopy;
}

bool SaveFolder_IsAHudsonRun()
{
	return HudsonRunIsOn;
}

char *GetSaveFolderPath()
{
	// check if we've got the path previously and use it again, or if we've found there isn't one
	if (*saveFolder || noSaveFolder) {
		return saveFolder;
	}

#ifdef _XBOX
	// just blank the char array
//	saveFolder[0] = '\0';
	strcat(saveFolder, "D:\\");
	return saveFolder;
#endif
#ifdef _WIN32

	// The game's folder is made in the player's Documents. Documents is found in a space of its own, so that if anything goes wrong there's no half a path left where the game's folder goes.
	// A Hudson run has a folder of its own beside the game in place of Documents, so it never touches the player's.
	char documents[MAX_PATH] = {0};
	char const *whatWentWrong = HudsonRunIsOn ? "Can't find the run's own folder" : "Can't find the player's Documents folder";
	std::string where;
	bool const found = HudsonRunIsOn ? Hudson_RunFolder(documents, sizeof(documents)) : SUCCEEDED(::SHGetFolderPath(NULL, CSIDL_MYDOCUMENTS, NULL, SHGFP_TYPE_CURRENT, documents));

	if (found)
	{
		whatWentWrong = SaveFolder_Make(documents, saveFolder, sizeof(saveFolder), &where, saveFolderIsTheSecondCopy);
	}

	if (whatWentWrong)
	{
		// With nowhere to keep its files the game can still run. The path stays empty, so its files go in the folder it started from, and the folders for saved games and network settings are made there.
		// If they can't be, a file that won't open is handled like any other.
		noSaveFolder = true;
		_snprintf(saveFolderProblem, sizeof(saveFolderProblem) - 1, "%s%s%s", whatWentWrong, where.empty() ? "" : ": ", where.c_str());
		SaveFolder_MakeOne("User_Profiles/", NULL);
		SaveFolder_MakeOne("MPConfig/", NULL);
	}

	return saveFolder;
#endif
}

/* 
   for files that should not be saved in the AvP game folder (ie /Program Files/ on windows.
   for files such as user profile data, config files, screenshots etc
*/
FILE *avp_open_userfile(const char *fileName, const char *mode)
{
#ifdef _XBOX

	finalPath.append("d:/");
	finalPath.append(fileName);

	// change forwardslashes in path to backslashes
	std::replace(finalPath.begin(), finalPath.end(), '/', '\\');

	return fopen(finalPath.c_str(), mode);
#endif

#ifdef _WIN32

	std::string sFileName(fileName);
	std::string savePath(GetSaveFolderPath());
	std::string finalPath;

	size_t savePathSize = savePath.length();

	// check if the path passed already contains the save folder path
	size_t found = sFileName.find(savePath);
	if (savePathSize >= found) {
		// contains the path already
		finalPath = sFileName;
	}
	else {
		finalPath = savePath + sFileName;
	}

	// a writing mode ("w", "a", or any with "+") only through PlainWrite_FOpen: never through a link (plain_write.h says why)
	if (PlainWrite_ModeWrites(mode))
	{
		return PlainWrite_FOpen(finalPath, mode);
	}

	return fopen(finalPath.c_str(), mode);
#endif
}

FILE *avp_fopen(const char *fileName, const char *mode)
{
	std::string finalPath;

#ifdef _XBOX

	finalPath.append("d:/");
	finalPath.append(fileName);

	// change forwardslashes in path to backslashes
	std::replace(finalPath.begin(), finalPath.end(), '/', '\\');

	return fopen(finalPath.c_str(), mode);
#endif
#ifdef _WIN32

	finalPath += fileName;

	// a writing mode ("w", "a", or any with "+") only through PlainWrite_FOpen: never through a link (plain_write.h says why)
	if (PlainWrite_ModeWrites(mode))
	{
		return PlainWrite_FOpen(finalPath, mode);
	}

	return fopen(finalPath.c_str(), mode);
#endif
}

DWORD avp_GetFileAttributes(LPCTSTR lpFileName)
{
#ifdef _XBOX

	std::string finalPath;

	finalPath.append("d:/");
	finalPath.append(lpFileName);

	// change forwardslashes in path to backslashes
	std::replace(finalPath.begin(), finalPath.end(), '/', '\\');

	return GetFileAttributes(finalPath.c_str());
#endif
#ifdef _WIN32
	return GetFileAttributes(lpFileName);
#endif
}

HANDLE avp_CreateFile(LPCTSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
#ifdef _XBOX

	std::string finalPath;

	finalPath.append("d:/");
	finalPath.append(lpFileName);

	// change forwardslashes in path to backslashes
	std::replace(finalPath.begin(), finalPath.end(), '/', '\\');

	return CreateFile(finalPath.c_str(), dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);

#endif
#ifdef _WIN32
	// Asked to write (or to make or empty a file): only through PlainWrite_Open, never through a link (plain_write.h says why). Every
	// caller gives no security attributes and no template file, so the open is the same but for that.
	DWORD const writes = GENERIC_WRITE | GENERIC_ALL | FILE_WRITE_DATA | FILE_APPEND_DATA;

	if ((dwDesiredAccess & writes) != 0 || dwCreationDisposition == CREATE_ALWAYS || dwCreationDisposition == CREATE_NEW
		|| dwCreationDisposition == TRUNCATE_EXISTING)
	{
		return PlainWrite_Open(lpFileName, dwDesiredAccess, dwShareMode, dwCreationDisposition, dwFlagsAndAttributes, NULL);
	}

	return CreateFile(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
#endif
}

HANDLE avp_FindFirstFile(LPCTSTR lpFileName, LPWIN32_FIND_DATA lpFindFileData)
{
#ifdef _XBOX

	std::string finalPath;

	finalPath.append("d:/");
	finalPath.append(lpFileName);

	// change forwardslashes in path to backslashes
	std::replace(finalPath.begin(), finalPath.end(), '/', '\\');

	return FindFirstFile(finalPath.c_str(), lpFindFileData);
#endif
#ifdef _WIN32
	return FindFirstFile(lpFileName, lpFindFileData);
#endif
}

void avp_MessageBox(const char* message, int type)
{
#ifdef _WIN32
	Hudson_StopIfTheGameWantsToAsk(message);
	ReleaseDirect3D();
	MessageBox(hWndMain, message, "AvP Error", type);
#endif
}

void avp_exit(int code)
{
#ifdef _WIN32
	exit(code);
#endif
}

// The build's version, as 3dc/build_identity.cpp has it: tools/build-and-package.ps1 writes build_version.h before it builds (it isn't in the repository).
// What comes through is one of: a release's version as tools/check-release.ps1 lets it through (three numbers, 1.0.0, or a beta of them, 1.0.0-beta.0); the
// script's fixed words for any other build, "a test build, not a release"; or, with no build_version.h at all, the words below. The title is made by
// snprintf into its own size, with the version as an argument and never as the format, so no version, whatever its letters, can overrun the title or
// be read as a format.
#if __has_include("build_version.h")
#include "build_version.h"
#endif
#ifndef AVP_RELEASE_VERSION
#define AVP_RELEASE_VERSION "not known (not built by tools/build-and-package.ps1)"
#endif

char windowTitle[256];

char *GetWindowTitle()
{
	assert(strlen(windowTitle) != 0);
	return windowTitle;
}

void SetWindowTitle()
{
	// the build's version, never the compiler's __DATE__ and __TIME__: those put the time of each build into the program, so no two builds of one commit were
	// the same bytes, and a scan of one could not speak for another (Bishop 2026-11-22, the editor's reproducible builds)
	snprintf(windowTitle, sizeof(windowTitle), "AvPx %s", AVP_RELEASE_VERSION);
}
