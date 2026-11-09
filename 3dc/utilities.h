// Modified 2026-11-05 by Melanikus for the AvP port:
//   says what GetSaveFolderPath gives back when there's no save folder
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
//   took out avp_GetCommandLineArgs
#ifndef _avp_utilities_h_
#define _avp_utilities_h_

#include <stdio.h>
#include <string>
#include <stdint.h>
#include "os_header.h"

#ifdef _XBOX
#define MB_OK			0x00000000L
#define MB_SYSTEMMODAL	0x00001000L
#endif

namespace Util {
	void ChangeSlashes(std::string &theString);
	void FtoBslash(std::string &theString);
	int LowercaseAndChangeSlash(char c);
	std::string IntToString(const int value);
	int StringToInt(const std::string &string);
}

FILE *avp_fopen(const char *fileName, const char *mode);
FILE *avp_open_userfile(const char *fileName, const char *mode);
HANDLE avp_CreateFile(LPCTSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
HANDLE avp_FindFirstFile(LPCTSTR lpFileName, LPWIN32_FIND_DATA lpFindFileData);
DWORD avp_GetFileAttributes(LPCTSTR lpFileName);
// The folder the game keeps its files in, ending in a slash. It's never null. It's empty if the game has nowhere to keep them, and then they go in the folder it started from.
char *GetSaveFolderPath();

// What went wrong finding that folder, or null if nothing did.
char const *GetSaveFolderProblem();

// This copy of the game is a second one on the same machine (-dev -secondcopy): its files go in My Games/Aliens versus Predator (second
// copy), a fixed name, so two copies don't share a log, settings or saved games. It has to come before anything asks for the folder, the
// log first of all, and gives back false, changing nothing, if it comes too late. (Added 2026-11-08 by Melanikus for the AvP port.)
bool SaveFolder_UseTheSecondCopy();
bool SaveFolder_IsTheSecondCopy();
void avp_MessageBox(const char* message, int type);
void avp_exit(int code);

void SetWindowTitle();
char *GetWindowTitle();

#endif
