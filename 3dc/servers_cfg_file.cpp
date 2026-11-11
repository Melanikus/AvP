// Finding and reading servers.cfg (servers_cfg_file.h says where it is and how it's read). Written 2026-11-10 by Melanikus for the AvP port
// (#175).

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string.h>
#include <vector>

#include "servers_cfg_file.h"

namespace
{
	// The longest path Windows has (with its \\?\ form), in characters, its 0 counted
	size_t const kMostPathLetters = 32768;

	bool IsSeparator(wchar_t letter)
	{
		return letter == L'\\' || letter == L'/';
	}

	// Whether the letters from start, count of them, are "bin", capitals or not
	bool IsBin(const wchar_t *start, size_t count)
	{
		return count == 3 && (start[0] == L'b' || start[0] == L'B') && (start[1] == L'i' || start[1] == L'I') && (start[2] == L'n' || start[2] == L'N');
	}
}

bool ServersCfg_GameFolderOf(const wchar_t *programPath, wchar_t *folder, size_t room)
{
	if (!folder || room == 0)
	{
		return false;
	}

	folder[0] = L'\0';

	if (!programPath)
	{
		return false;
	}

	size_t const length = wcsnlen(programPath, kMostPathLetters);

	if (length == 0 || length == kMostPathLetters)
	{
		return false;
	}

	// The separator before the program's own name, and the one before the folder it's in
	size_t last = length;

	while (last > 0 && !IsSeparator(programPath[last - 1]))
	{
		last--;
	}

	// The program's name has to be there, and a folder before it
	if (last == length || last < 2)
	{
		return false;
	}

	size_t const programFolderEnd = last - 1;
	size_t before = programFolderEnd;

	while (before > 0 && !IsSeparator(programPath[before - 1]))
	{
		before--;
	}

	if (before < 2 || !IsBin(programPath + before, programFolderEnd - before))
	{
		return false;
	}

	// The game's folder is everything before the separator in front of bin
	size_t const gameFolderLength = before - 1;

	if (gameFolderLength + 1 > room)
	{
		return false;
	}

	memcpy(folder, programPath, gameFolderLength * sizeof(wchar_t));
	folder[gameFolderLength] = L'\0';
	return true;
}

ServersCfgOpening ServersCfg_ReadFolder(const wchar_t *folder, ServersCfgFileRead *out)
{
	ServersCfg_Read(NULL, 0, out);

	if (!folder || !folder[0])
	{
		return kServersCfgNoFolder;
	}

	// A wide string joined to a plain one is wide, all of it: L"\\servers.cfg"
	static wchar_t const kName[] = L"\\" SERVERS_CFG_FILE_NAME;
	size_t const folderLength = wcsnlen(folder, kMostPathLetters);
	size_t const nameLength = sizeof(kName) / sizeof(kName[0]) - 1;

	if (folderLength + nameLength + 1 > kMostPathLetters)
	{
		return kServersCfgNoFolder;
	}

	std::vector<wchar_t> path(folder, folder + folderLength);
	path.insert(path.end(), kName, kName + nameLength + 1);

	HANDLE const file = CreateFileW(path.data(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (file == INVALID_HANDLE_VALUE)
	{
		DWORD const error = GetLastError();

		if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
		{
			return kServersCfgNotThere;
		}

		// A folder can't be opened this way: Windows says access is denied, so its attributes tell the two apart
		DWORD const attributes = GetFileAttributesW(path.data());
		return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) ? kServersCfgNotAFile : kServersCfgCantRead;
	}

	// Only a plain file on a disk: not a device, a pipe or a console
	if (GetFileType(file) != FILE_TYPE_DISK)
	{
		CloseHandle(file);
		return kServersCfgNotAFile;
	}

	// At most one byte more than the reader takes, so a bigger file is told apart without reading it all
	std::vector<char> text(static_cast<size_t>(kServersCfgMostBytes) + 1);
	size_t got = 0;
	bool readWell = true;

	while (got < text.size())
	{
		DWORD read = 0;
		DWORD const wanted = static_cast<DWORD>(text.size() - got);

		if (!ReadFile(file, text.data() + got, wanted, &read, NULL))
		{
			readWell = false;
			break;
		}

		if (read == 0)
		{
			break;
		}

		got += read;
	}

	CloseHandle(file);

	if (!readWell)
	{
		return kServersCfgCantRead;
	}

	ServersCfg_Read(got > 0 ? text.data() : NULL, got, out);
	return kServersCfgOpened;
}

ServersCfgOpening ServersCfg_ReadTheGames(ServersCfgFileRead *out)
{
	ServersCfg_Read(NULL, 0, out);

	std::vector<wchar_t> program(kMostPathLetters);
	DWORD const length = GetModuleFileNameW(NULL, program.data(), static_cast<DWORD>(program.size()));

	if (length == 0 || length >= program.size())
	{
		return kServersCfgNoFolder;
	}

	std::vector<wchar_t> folder(kMostPathLetters);

	if (!ServersCfg_GameFolderOf(program.data(), folder.data(), folder.size()))
	{
		return kServersCfgNoFolder;
	}

	return ServersCfg_ReadFolder(folder.data(), out);
}

const char *ServersCfg_OpeningWords(ServersCfgOpening opening)
{
	switch (opening)
	{
		case kServersCfgOpened: return "it was read";
		case kServersCfgNotThere: return "there's no " SERVERS_CFG_FILE_NAME " in the game's folder, so there are no servers";
		case kServersCfgNoFolder: return "the game isn't in a folder called bin, so there's no game folder to find " SERVERS_CFG_FILE_NAME " in";
		case kServersCfgNotAFile: return SERVERS_CFG_FILE_NAME " in the game's folder isn't a plain file, so it isn't read";
		case kServersCfgCantRead: return SERVERS_CFG_FILE_NAME " in the game's folder couldn't be read";
	}

	return "it was refused for a reason this game has no words for";
}
