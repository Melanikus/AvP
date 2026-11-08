// Making the folder the game keeps its files in. Written 2026-11-05 by Melanikus for the AvP port.
//
// The game keeps its settings, its log and its saved games in a folder of its own: My Games/Aliens versus Predator, inside
// the player's Documents. This makes that folder and the two folders inside it, and says what went wrong if it can't.
//
// It's on its own so that a test can hand it a folder of the test's choosing, and see what happens when something goes wrong.
// GetSaveFolderPath in utilities.cpp is what the rest of the game calls.
#ifndef SAVE_FOLDER_H
#define SAVE_FOLDER_H

#include <windows.h>
#include <algorithm>
#include <string>
#include <string.h>

static const char *const kSaveFolderNothingGiven = "there's no folder to make the save folder in";
static const char *const kSaveFolderCantMake = "a folder on the way to the save folder can't be made";
static const char *const kSaveFolderTooLong = "the save folder's path is too long";

// Makes one folder. A folder that's there already is fine, and so is any other answer from Windows except "the folder this
// goes in doesn't exist", because that's the only one that means there's no way on. So it says false only for that one, and
// where is told which folder it was, if it was given one.
inline bool SaveFolder_MakeOne(const std::string &path, std::string *where)
{
	if (::CreateDirectoryA(path.c_str(), NULL) != 0 || ::GetLastError() != ERROR_PATH_NOT_FOUND)
	{
		return true;
	}

	if (where)
	{
		*where = path;
	}

	return false;
}

// under is the folder to make it in, and folder is where the answer goes, which has room for size characters.
// It gives back 0 if it worked. Then folder holds the path, with / between its parts and one on the end, ready for a file's name.
// Otherwise it gives back what went wrong, and folder is empty, so nobody can mistake half a path for the whole of one. where can be left out. If it's given, it says which folder was the trouble.
inline const char *SaveFolder_Make(const char *under, char *folder, size_t size, std::string *where)
{
	if (where)
	{
		where->clear();
	}

	if (!folder || size == 0)
	{
		return kSaveFolderNothingGiven;
	}

	folder[0] = 0;

	if (!under || !under[0])
	{
		return kSaveFolderNothingGiven;
	}

	std::string path = under;

	// the game's file names use forward slashes, and Windows takes either
	std::replace(path.begin(), path.end(), '\\', '/');

	path += "/My Games";

	if (!SaveFolder_MakeOne(path, where))
	{
		return kSaveFolderCantMake;
	}

	path += "/Aliens versus Predator";

	if (!SaveFolder_MakeOne(path, where))
	{
		return kSaveFolderCantMake;
	}

	path += "/";

	if (!SaveFolder_MakeOne(path + "User_Profiles/", where) || !SaveFolder_MakeOne(path + "MPConfig/", where))
	{
		return kSaveFolderCantMake;
	}

	if (path.length() > size - 1)
	{
		if (where)
		{
			*where = path;
		}

		return kSaveFolderTooLong;
	}

	// The letters, then the 0 that ends them on a line of its own: the length was checked against the room just above
	memcpy(folder, path.c_str(), path.length());
	folder[path.length()] = 0;
	return 0;
}

#endif
