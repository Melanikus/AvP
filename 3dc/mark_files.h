// The tester's mark: one key that says "this looked weird" in the log, with a picture of the game's own screen beside it. Written
// 2026-11-07 by Melanikus for the AvP port.
//
// WHAT IT'S FOR: testers finishing the campaign send us their logs. When something looks wrong, they press the mark key (F2), and the
// log gets one line, MARK and a number, saying when and where they were and what they had, and the game saves what was on its screen
// as mark-<number>.png in the same folder. Then "Mel, this looked weird at mark 12" points straight at it.
//
// THE PICTURES: each mark gets the next number after the highest picture already in the folder, so a new run never writes over an old
// one, and only the newest kLogFilesMarkPicturesKept are kept: older ones are thrown away. Only the game's own picture of its frame is
// saved, never the desktop or anything else on the screen.
//
// THE FOLDER IS INPUT: like the logs (log_files.h), anything could be in the folder. So a picture is only written as a new file, never
// over anything, and only into a plain folder, not a link to one. Only a plain file with one name is ever thrown away. Names that only
// look like a mark's are left alone, and nothing here can stop the game: a mark that can't be saved says so on the screen, and that's all.
// At most one mark a second, so holding the key down can't fill the disk.
//
// tests/mark_files_test.cpp tries all of this on files it makes. mark_key.cpp is what the game uses.
//
// Modified 2026-11-09 by Melanikus for the AvP port:
//   a mark can be of a menu page too (Mark_ThisPage, for -dev -onlinescreens' pictures): its line names the page, not a player
#ifndef MARK_FILES_H
#define MARK_FILES_H

#include "log_files.h"

// How many pictures are kept, the newest ones
static const int kMarkPicturesKept = 20;

// The highest mark number there can be. A folder that has one already gets no more marks, rather than going round to 1 and writing over
static const int kMarkHighestNumber = 999999;

// The most names in the folder that are looked at, so a folder holding millions of files can't hold the game up
static const int kMarkMostNamesLookedAt = 10000;

// The fewest milliseconds between two marks
static const unsigned long kMarkFewestMillisecondsApart = 1000;

// The picture's name for mark n, in folder (which ends with a slash, or is empty for the current folder)
inline std::string Mark_PictureName(const std::string &folder, int n)
{
	char name[32];
	_snprintf(name, sizeof(name) - 1, "mark-%d.png", n);
	name[sizeof(name) - 1] = '\0';
	return folder + name;
}

// If name is exactly a mark's picture name, mark-<1 to 6 digits>.png with no leading 0, gives back its number, and otherwise 0.
// Capitals don't matter, as they don't to Windows: MARK-5.PNG is the same file as mark-5.png.
inline int Mark_NumberInName(const char *name)
{
	if (_strnicmp(name, "mark-", 5) != 0 || name[5] < '1' || name[5] > '9')
	{
		return 0;
	}

	int number = 0;
	int digits = 0;
	const char *c = name + 5;

	while (*c >= '0' && *c <= '9')
	{
		if (++digits > 6)
		{
			return 0;
		}

		number = number * 10 + (*c - '0');
		c++;
	}

	return _stricmp(c, ".png") == 0 ? number : 0;
}

// The highest number among the marks' pictures in folder, or 0 if it has none. Only plain files count, and only the first
// kMarkMostNamesLookedAt names are looked at.
inline int Mark_HighestInFolder(const std::string &folder)
{
	WIN32_FIND_DATAA found;
	HANDLE const search = ::FindFirstFileA((folder + "mark-*.png").c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
	{
		return 0;
	}

	int highest = 0;
	int looked = 0;

	do
	{
		int const number = Mark_NumberInName(found.cFileName);

		if (number > highest && (found.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) == 0)
		{
			highest = number;
		}
	}
	while (++looked < kMarkMostNamesLookedAt && ::FindNextFileA(search, &found));

	::FindClose(search);
	return highest;
}

// The number the next mark gets: one more than the highest picture in the folder, or 1. 0 means no more marks can be saved there.
inline int Mark_NextNumber(const std::string &folder)
{
	int const highest = Mark_HighestInFolder(folder);
	return highest >= kMarkHighestNumber ? 0 : highest + 1;
}

// Throws away the pictures older than the newest kMarkPicturesKept, now that newest is the newest. Each is checked to be a plain file
// with one name first, and only names the folder really has are tried, at most kMarkMostNamesLookedAt. Gives back how many went, for
// the test.
inline int Mark_KeepTheNewest(const std::string &folder, int newest)
{
	int const oldestKept = newest - kMarkPicturesKept + 1;
	int gone = 0;

	WIN32_FIND_DATAA found;
	HANDLE const search = ::FindFirstFileA((folder + "mark-*.png").c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
	{
		return 0;
	}

	int looked = 0;

	do
	{
		int const number = Mark_NumberInName(found.cFileName);
		std::string const name = folder + found.cFileName;

		if (number && number < oldestKept && LogFiles_IsPlainFile(name) && ::DeleteFileA(name.c_str()))
		{
			gone++;
		}
	}
	while (++looked < kMarkMostNamesLookedAt && ::FindNextFileA(search, &found));

	::FindClose(search);
	return gone;
}

// Is folder a plain folder, there and not a link to one somewhere else? An empty folder means the current one, which is.
inline bool Mark_FolderIsPlain(const std::string &folder)
{
	if (folder.empty())
	{
		return true;
	}

	DWORD const what = ::GetFileAttributesA(folder.c_str());
	return what != INVALID_FILE_ATTRIBUTES && (what & FILE_ATTRIBUTE_DIRECTORY) != 0 && (what & FILE_ATTRIBUTE_REPARSE_POINT) == 0;
}

// Writes a picture's bytes as a new file at path. Nothing that's there already is written over or followed, and a file that's only
// half written is taken away again. Says whether it worked.
inline bool Mark_WritePicture(const std::string &path, const void *bytes, unsigned long size)
{
	HANDLE const file = ::CreateFileA(path.c_str(), GENERIC_WRITE | FILE_READ_ATTRIBUTES, 0, NULL, CREATE_NEW,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, NULL);

	if (file == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	DWORD written = 0;
	bool const wrote = LogFiles_HandleIsPlainFile(file) && ::WriteFile(file, bytes, size, &written, NULL) && written == size;
	::CloseHandle(file);

	if (!wrote)
	{
		::DeleteFileA(path.c_str());
	}

	return wrote;
}

// Is key one of the bytes of a player's controls? Each control is one byte holding its key, so the whole of them can be looked through.
inline bool Mark_KeyIsIn(const unsigned char *controls, size_t count, unsigned char key)
{
	for (size_t i = 0; i < count; i++)
	{
		if (controls[i] == key)
		{
			return true;
		}
	}

	return false;
}

// Is it too soon after the last mark for another? now and last are in milliseconds, and last is 0 when there's been none.
inline bool Mark_TooSoon(unsigned long now, unsigned long last)
{
	return last != 0 && now - last < kMarkFewestMillisecondsApart;
}

// What the game knows at the moment of a mark, for its line in the log
struct Mark_Moment
{
	int number;
	int hours, minutes, seconds;   // the time of day
	const char *level;             // the level's name
	const char *species;           // "marine", "alien" or "predator"
	int x, y, z;                   // where the player is, in the game's units
	int facing;                    // which way they face, 0 to 4095 round a full turn
	int health;                    // their health, as the HUD shows it
	int weapon;                    // the weapon they hold, by its number in the game
	const char *network;           // the other players' connections and round trips, or NULL out of a network game
	const char *page;              // a menu page's name, fixed words of the game's own, for a mark with no player; NULL in a level
};

// The mark's line, with no line end, in words of size characters, which always end with a 0
inline void Mark_DescribeMoment(const Mark_Moment &m, char *words, size_t size)
{
	if (size == 0)
	{
		return;
	}

	// A menu page's mark: there's no player, so it says the page, the time and the picture, and nothing else
	if (m.page)
	{
		int const pageWords = _snprintf(words, size - 1, "MARK %d at %02d:%02d:%02d: the page %s, picture mark-%d.png", m.number, m.hours, m.minutes,
			m.seconds, m.page, m.number);

		words[(pageWords < 0 || (size_t)pageWords >= size) ? size - 1 : (size_t)pageWords] = '\0';
		return;
	}

	int const used = _snprintf(words, size - 1, "MARK %d at %02d:%02d:%02d: %s, a %s at (%d, %d, %d) facing %d, health %d, weapon %d, picture mark-%d.png%s%s",
		m.number, m.hours, m.minutes, m.seconds, m.level ? m.level : "no level", m.species ? m.species : "player", m.x, m.y, m.z, m.facing,
		m.health, m.weapon, m.number, m.network ? "; " : "", m.network ? m.network : "");

	words[(used < 0 || (size_t)used >= size) ? size - 1 : (size_t)used] = '\0';
}

#endif
