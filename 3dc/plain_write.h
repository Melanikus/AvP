// A PLAIN WRITE: opening a file to write it in a way that never writes through a link. Written 2026-11-23 by Melanikus (Drake) for
// the AvP port (card 6, Tao 1 and 3; Vasquez found that FileStream opened its files with a plain CreateFile, and the sweep that followed
// found the same in every way the game writes a file).
//
// WHY: a plain CreateFile or fopen follows whatever is at the name. If something put a link there (a symbolic link to another file),
// or made the name a second name of another file (a hard link), or made the folder a junction to another folder, the game emptied and
// wrote THAT file instead, wherever it was. The game writes its settings, profiles, saves and logs in the player's Documents and in its
// own folder, where other programs write too, so anything there may have been put there by someone else.
//
// HOW (PlainWrite_Open):
//   1. The folder it goes in is opened as itself (never followed), looked at, and held while the file is opened: it must be a folder and
//      not a link. Holding it (no FILE_SHARE_DELETE) means nobody can rename it, or swap it for a junction, meanwhile.
//   2. The name is opened as itself, only to look at it.
//      - Nothing there: a new file is made, as itself (CREATE_NEW, which never follows a link and fails if anything at all appeared).
//      - Something there: it must be a file on a disk, not a folder, not a link, with exactly one name. Then it's opened for writing,
//        and that open file must be the very same file (the same disk and file number), still with one name. Only then, if it's to be
//        emptied, is it emptied. So a file swapped for a link in between, or given a second name, is never emptied or written.
//   3. Everything is written through that one handle.
//
// WHAT COUNTS AS A LINK: a reparse point that stands for another name, a symbolic link or a junction (or a mount point). Windows calls
// these "name surrogates", and the handle says so. A hard link is a file with more than one name, and it's refused too. OTHER reparse
// points are not links, and are written as usual: OneDrive keeps the files of a synced Documents folder as reparse points of its own,
// and the save folder is in Documents, so refusing every reparse point would stop OneDrive's players saving anything. (The game's
// network identity avoids the question by living in %LOCALAPPDATA%, which OneDrive doesn't sync; net_identity_store.h says why. The
// save folder can't move there: players look for their saves in Documents.)
//
// PlainWrite_FOpen does the same for the C library's FILE, so that fopen's "w", "a" and "+" modes go through it too.
//
// tests/plain_write_test.cpp tries both, and FileStream's write, on plain files, folders and a device, and on GitHub's machines (where
// making links is allowed: antivirus programs watch for it) on symbolic links, hard links and junctions.
#ifndef PLAIN_WRITE_H
#define PLAIN_WRITE_H

#include <windows.h>
// _open_osfhandle and _close: the C library's <io.h> says them by including <corecrt_io.h>, which is included here by its own name,
// because the game has an io.h of its own (3dc/win95/io.h) that comes first on the include path and hides the C library's
#include <corecrt_io.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <string>

// Does this open file or folder stand for another name (a symbolic link, a junction or a mount point)? Asked of the handle itself. When
// Windows can't say, it's taken as a link.
inline bool PlainWrite_HandleIsLink(HANDLE handle)
{
	FILE_ATTRIBUTE_TAG_INFO tag = {};

	if (!::GetFileInformationByHandleEx(handle, FileAttributeTagInfo, &tag, sizeof(tag)))
	{
		return true;
	}

	return (tag.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 && IsReparseTagNameSurrogate(tag.ReparseTag);
}

// "" when the open handle is a file on a disk, not a folder, not a link, with exactly one name; else why not. what is filled in.
inline std::string PlainWrite_FileWhy(HANDLE file, BY_HANDLE_FILE_INFORMATION *what)
{
	if (::GetFileType(file) != FILE_TYPE_DISK)
	{
		return "it isn't a file on a disk (it is a device's name)";
	}

	if (!::GetFileInformationByHandle(file, what))
	{
		return "Windows can't say what it is";
	}

	if ((what->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
	{
		return "a folder has that name";
	}

	if (PlainWrite_HandleIsLink(file))
	{
		return "it is a link to somewhere else";
	}

	if (what->nNumberOfLinks != 1)
	{
		return "it is one of " + std::to_string((unsigned long long)what->nNumberOfLinks) + " names of the same file (a hard link)";
	}

	return std::string();
}

// The folder a path's file goes in: all before its last \ or /, or "." (the folder the game runs in) when it has none
inline std::string PlainWrite_FolderOf(const std::string &path)
{
	size_t const slash = path.find_last_of("\\/");

	if (slash == std::string::npos)
	{
		return ".";
	}

	if (slash == 0 || (slash == 2 && path[1] == ':'))
	{
		return path.substr(0, slash + 1);   // the top of a disk: "\" or "C:\"
	}

	return path.substr(0, slash);
}

inline std::string PlainWrite_Error(const char *what, DWORD error)
{
	return std::string(what) + " (Windows error " + std::to_string((unsigned long long)error) + ")";
}

// Opens path to write it, as the top of this file says. access, share and flags are CreateFile's (flags: its attributes and FILE_FLAG_s
// for the open file; never a flag that follows or skips a link, which this decides). disposition is CreateFile's too:
//   CREATE_ALWAYS     made if it isn't there, and emptied if it is (fopen's "w")
//   OPEN_ALWAYS       made if it isn't there, kept as it is if it is (fopen's "a")
//   CREATE_NEW        made, only if nothing is there
//   OPEN_EXISTING     opened, only if it is there, kept as it is (fopen's "r+")
//   TRUNCATE_EXISTING opened, only if it is there, and emptied
// Gives back the open file, or INVALID_HANDLE_VALUE with why set (words for the log) and Windows' last error set. Nothing that was there
// is written or emptied unless it is the plain file judged here.
inline HANDLE PlainWrite_Open(const std::string &path, DWORD access, DWORD share, DWORD disposition, DWORD flags, std::string *why)
{
	std::string ignored;
	std::string &whyNot = why ? *why : ignored;
	whyNot.clear();
	DWORD error = ERROR_ACCESS_DENIED;

	flags &= ~(DWORD)(FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_DELETE_ON_CLOSE | FILE_FLAG_POSIX_SEMANTICS);

	if ((flags & 0xFFFF) == 0)
	{
		flags |= FILE_ATTRIBUTE_NORMAL;
	}

	// 1. The folder, held: others may read and write in it, but not rename or delete it (no FILE_SHARE_DELETE) while it's held
	std::string const folderPath = PlainWrite_FolderOf(path);
	HANDLE const folder = ::CreateFileA(folderPath.c_str(), FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
		OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);

	if (folder == INVALID_HANDLE_VALUE)
	{
		error = ::GetLastError();
		whyNot = PlainWrite_Error("the folder it goes in can't be opened", error);
		::SetLastError(error);
		return INVALID_HANDLE_VALUE;
	}

	BY_HANDLE_FILE_INFORMATION aboutFolder;

	if (!::GetFileInformationByHandle(folder, &aboutFolder) || (aboutFolder.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
	{
		whyNot = "what it goes in isn't a folder";
	}
	else if (PlainWrite_HandleIsLink(folder))
	{
		whyNot = "the folder it goes in is a link to somewhere else (a junction or a symbolic link)";
	}

	HANDLE file = INVALID_HANDLE_VALUE;

	// 2. The name, opened as itself only to be looked at (a folder too, so that it can be seen to be one)
	if (whyNot.empty())
	{
		HANDLE const looked = ::CreateFileA(path.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
			OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);

		if (looked == INVALID_HANDLE_VALUE)
		{
			DWORD const lookError = ::GetLastError();

			if (lookError != ERROR_FILE_NOT_FOUND)
			{
				error = lookError;
				whyNot = PlainWrite_Error("what has that name can't be looked at", lookError);
			}
			else if (disposition == OPEN_EXISTING || disposition == TRUNCATE_EXISTING)
			{
				error = ERROR_FILE_NOT_FOUND;
				whyNot = "it isn't there";
			}
			else
			{
				// Nothing there: a new file, made as itself. CREATE_NEW fails if anything appeared there meanwhile, a link included.
				file = ::CreateFileA(path.c_str(), access | FILE_READ_ATTRIBUTES, share, NULL, CREATE_NEW, flags | FILE_FLAG_OPEN_REPARSE_POINT, NULL);

				if (file == INVALID_HANDLE_VALUE)
				{
					error = ::GetLastError();
					whyNot = PlainWrite_Error("it can't be made", error);
				}
				else
				{
					BY_HANDLE_FILE_INFORMATION what;
					whyNot = PlainWrite_FileWhy(file, &what);
				}
			}
		}
		else
		{
			BY_HANDLE_FILE_INFORMATION before;

			if (disposition == CREATE_NEW)
			{
				error = ERROR_FILE_EXISTS;
				whyNot = "something is already there";
			}
			else
			{
				whyNot = PlainWrite_FileWhy(looked, &before);
			}

			if (whyNot.empty())
			{
				// Opened to be written, without emptying it yet (OPEN_EXISTING). OneDrive's own reparse points are followed here, as usual.
				file = ::CreateFileA(path.c_str(), access | FILE_READ_ATTRIBUTES, share, NULL, OPEN_EXISTING, flags, NULL);

				if (file == INVALID_HANDLE_VALUE)
				{
					error = ::GetLastError();
					whyNot = PlainWrite_Error("it can't be opened to be written", error);
				}
				else
				{
					BY_HANDLE_FILE_INFORMATION now;
					whyNot = PlainWrite_FileWhy(file, &now);

					if (whyNot.empty() && (now.dwVolumeSerialNumber != before.dwVolumeSerialNumber || now.nFileIndexHigh != before.nFileIndexHigh
						|| now.nFileIndexLow != before.nFileIndexLow))
					{
						whyNot = "it was swapped for another file while it was being opened";
					}

					// Only now, the very file judged above, is it emptied
					if (whyNot.empty() && (disposition == CREATE_ALWAYS || disposition == TRUNCATE_EXISTING) && !::SetEndOfFile(file))
					{
						error = ::GetLastError();
						whyNot = PlainWrite_Error("it can't be emptied", error);
					}
				}
			}

			::CloseHandle(looked);
		}
	}

	::CloseHandle(folder);

	if (!whyNot.empty())
	{
		if (file != INVALID_HANDLE_VALUE)
		{
			::CloseHandle(file);
		}

		::SetLastError(error);
		return INVALID_HANDLE_VALUE;
	}

	return file;
}

// Is this fopen mode one that writes ("w", "a", or any with "+")?
inline bool PlainWrite_ModeWrites(const char *mode)
{
	return mode && (mode[0] == 'w' || mode[0] == 'a' || strchr(mode, '+') != NULL);
}

// fopen for a writing mode, through PlainWrite_Open: "w" empties (or makes) the file, "a" adds to its end (or makes it), "r+" opens one
// that is there; "b" for bytes as they are, else text, as fopen. Others may read and write the file while it's open, as with fopen.
// NULL, with why set, when it's refused or can't be opened. A mode that only reads is refused: that's fopen's job.
inline FILE *PlainWrite_FOpen(const std::string &path, const char *mode, std::string *why = NULL)
{
	std::string ignored;
	std::string &whyNot = why ? *why : ignored;

	if (!PlainWrite_ModeWrites(mode))
	{
		whyNot = "PlainWrite_FOpen only opens files to write them";
		return NULL;
	}

	bool const both = strchr(mode, '+') != NULL;
	DWORD const access = both ? (GENERIC_READ | GENERIC_WRITE) : GENERIC_WRITE;
	DWORD const disposition = mode[0] == 'w' ? CREATE_ALWAYS : mode[0] == 'a' ? OPEN_ALWAYS : OPEN_EXISTING;
	int ioFlags = (both ? _O_RDWR : _O_WRONLY) | (strchr(mode, 'b') ? _O_BINARY : _O_TEXT);

	if (mode[0] == 'a')
	{
		ioFlags |= _O_APPEND;
	}

	HANDLE const file = PlainWrite_Open(path, access, FILE_SHARE_READ | FILE_SHARE_WRITE, disposition, FILE_ATTRIBUTE_NORMAL, &whyNot);

	if (file == INVALID_HANDLE_VALUE)
	{
		return NULL;
	}

	// The C library takes the handle over: closing the FILE closes it
	int const descriptor = _open_osfhandle((intptr_t)file, ioFlags);

	if (descriptor == -1)
	{
		::CloseHandle(file);
		whyNot = "the C library can't take the file";
		return NULL;
	}

	FILE *const stream = _fdopen(descriptor, mode);

	if (!stream)
	{
		_close(descriptor);
		whyNot = "the C library can't take the file";
		return NULL;
	}

	return stream;
}

#endif
