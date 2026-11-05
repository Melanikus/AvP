// Writing a saved game so that going wrong can't lose the one it replaces. Written 2026-11-05 by Melanikus for the AvP port.
//
// The game used to open the slot's file, which emptied it, and then write without looking at whether that worked. So a full disk
// destroyed the old save and still told the player the game was saved.
//
// Now the new save goes in a file of its own, beside the old one. Only when all of it is safely there does it take the old one's
// place, which Windows does in one step. If anything goes wrong before then, the old save hasn't been touched.
//
// It's on its own so that a test can try it on real files, and make each step fail.
#ifndef SAVE_FILE_WRITE_H
#define SAVE_FILE_WRITE_H

#include <windows.h>
#include <string>

// Windows can write less than it was asked to and still say it worked, so the amount has to be checked as well.
inline bool SaveFile_WriteOnePart(HANDLE file, const void *part, DWORD size)
{
	DWORD written = 0;

	return ::WriteFile(file, part, size, &written, NULL) != 0 && written == size;
}

// Writes the two parts of a saved game to path, one after the other. It gives back true if the save is safely there. If it gives back
// false, whatever was at path before is still there, and nothing else has been left behind.
inline bool SaveFile_WriteSafely(const char *path, const void *first, DWORD firstSize, const void *second, DWORD secondSize)
{
	if (!path)
	{
		return false;
	}

	std::string const newFile = std::string(path) + ".new";
	HANDLE const file = ::CreateFileA(newFile.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	if (file == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	// Flushing asks Windows to put it on the disk now, because a full disk can wait until then to say so
	bool saved = SaveFile_WriteOnePart(file, first, firstSize) && SaveFile_WriteOnePart(file, second, secondSize) && ::FlushFileBuffers(file) != 0;

	if (!::CloseHandle(file))
	{
		saved = false;
	}

	if (saved && !::MoveFileExA(newFile.c_str(), path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
	{
		saved = false;
	}

	if (!saved)
	{
		::DeleteFileA(newFile.c_str());
	}

	return saved;
}

#endif
