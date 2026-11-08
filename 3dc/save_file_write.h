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

static const int kSaveFileTries = 5;

// Puts the new file in the old one's place, which Windows does in one step. A virus scanner often opens a new file just after it's closed, and while it has it open this fails with one of two errors.
// So on those it tries a few more times, waiting a tenth of a second between tries, which comes to under half a second. That only happens on a save, and only when it's needed.
inline bool SaveFile_TakeThePlaceOf(const char *newFile, const char *path)
{
	for (int tries = 0; tries < kSaveFileTries; tries++)
	{
		if (::MoveFileExA(newFile, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
		{
			return true;
		}

		DWORD const why = ::GetLastError();

		if (why != ERROR_SHARING_VIOLATION && why != ERROR_ACCESS_DENIED)
		{
			return false;
		}

		// after the last try there's nothing to wait for
		if (tries + 1 < kSaveFileTries)
		{
			::Sleep(100);
		}
	}

	return false;
}

// Writes the two parts of a saved game to path, one after the other. It gives back true if the save is safely there. If it gives back
// false, whatever was at path before is still there. The new file is taken away as well, unless something such as a virus scanner still has it open. Then it stays beside
// the old one until the next save writes over it. Nothing in the game reads a file with that name.
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

	if (saved && !SaveFile_TakeThePlaceOf(newFile.c_str(), path))
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
