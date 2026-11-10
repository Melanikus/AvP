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
#include <string>
#include <algorithm>
#include "configFile.h"
#include "logString.h"
#include "save_folder.h"
#include <assert.h>

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

char windowTitle[256];

char *GetWindowTitle()
{
	assert(strlen(windowTitle) != 0);
	return windowTitle;
}

void SetWindowTitle()
{
	strcpy(windowTitle, "AvPx - Build Date: ");
	strcat(windowTitle, (const char *)__DATE__);
	strcat(windowTitle, " at ");
	strcat(windowTitle, (const char *)__TIME__);
}
