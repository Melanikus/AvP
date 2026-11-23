// RED STUB (Drake 2026-11-23, card 6): the names the plain write test calls, doing exactly what the game did before it (a plain
// CreateFile, a plain fopen), so that the test shows the old way failing. The next commit puts the real PlainWrite in its place.
#ifndef PLAIN_WRITE_H
#define PLAIN_WRITE_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <string>

inline HANDLE PlainWrite_Open(const std::string &path, DWORD access, DWORD share, DWORD disposition, DWORD flags, std::string *why)
{
	if (why)
	{
		why->clear();
	}

	return ::CreateFileA(path.c_str(), access, share, NULL, disposition, flags, NULL);
}

inline bool PlainWrite_ModeWrites(const char *mode)
{
	return mode && (mode[0] == 'w' || mode[0] == 'a' || strchr(mode, '+') != NULL);
}

inline FILE *PlainWrite_FOpen(const std::string &path, const char *mode, std::string *why = NULL)
{
	if (why)
	{
		why->clear();
	}

	return fopen(path.c_str(), mode);
}

#endif
