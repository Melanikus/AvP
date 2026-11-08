// A level's own log: what the game said while loading it, written beside the level's file, so whoever made the level can see why it
// didn't load, or what it left out. Written 2026-11-07 by Melanikus for the AvP port (card LEVELLOG-1).
//
// WHEN IT'S WRITTEN: always when a level can't be loaded, and when one loads with something in it refused (a decal, a sound, a
// texture). A level that loads cleanly writes nothing.
//
// WHERE: beside the level, as <name>_errorlog.txt in the same folder. If that folder can't be written (a level under Program Files or
// Steam), the same name in the game's own folder, Documents/My Games/Aliens versus Predator, and the game's log says where it went.
//
// THE NAME COMES FROM OUTSIDE: a custom level's name is the name of a file someone downloaded. So the name is checked before any file
// is named after it, and the writer never follows a link, and never writes over a file that isn't one of these logs. The checks and
// the writer are here, on their own, so that tests/level_log_test.cpp can try them on names and folders of its choosing. level_log.cpp
// is what the game calls.
#ifndef LEVEL_LOG_H
#define LEVEL_LOG_H

#include <windows.h>
#include <string>
#include <string.h>

// The longest file name a level's log can be named after. Real levels' names are far shorter, and this leaves room for the ending
// and the folder on any path the game uses.
static const size_t kLevelLogLongestName = 64;

// What a level's log is called: the level's file name, then this
static const char *const kLevelLogEnding = "_errorlog.txt";

// The name used in the game's own folder when the level's name can't be used: one file, written over each time
static const char *const kLevelLogFallbackName = "level_errorlog.txt";

// Every one of these logs starts with this line. The writer only ever writes over a file that starts with it, so a file of that name
// that the game didn't write is left alone.
static const char *const kLevelLogFirstLine = "Aliens versus Predator: what the game said while loading a level\r\n";

// The most a log holds. Past these, the rest is left out, and the last line says how many lines were.
static const size_t kLevelLogMostBytes = 64 * 1024;
static const int kLevelLogMostLines = 500;

// The only folders a level's name can start with: none for one of the game's own levels, and this for a custom one (env_info.cpp
// puts it in front of the name of a file found in avp_rifs/custom)
static const char *const kLevelLogCustomFolder = "Custom/";

// Is this file name safe to name a file after, in a folder of the game's choosing? It has to be 1 to kLevelLogLongestName characters,
// with none of the characters Windows uses to mean a folder, a drive, a device or more than one file, no character below a space, no
// "..", no space at the start or the end, and no dot at the end, which Windows quietly takes off. And it mustn't be the name of a
// device, such as NUL or COM1, which Windows treats as the device whatever folder it's in. That part is the name up to its first dot.
inline bool LevelLog_NameIsSafe(const char *name)
{
	if (!name)
	{
		return false;
	}

	size_t const length = strlen(name);

	if (length == 0 || length > kLevelLogLongestName || name[0] == ' ' || name[length - 1] == ' ' || name[length - 1] == '.')
	{
		return false;
	}

	for (size_t i = 0; i < length; i++)
	{
		unsigned char const c = (unsigned char)name[i];

		if (c < 32 || c == 127 || strchr("/\\:*?\"<>|", c))
		{
			return false;
		}
	}

	if (strstr(name, ".."))
	{
		return false;
	}

	char base[kLevelLogLongestName + 1];
	size_t baseLength = strcspn(name, ".");
	memcpy(base, name, baseLength);
	base[baseLength] = '\0';

	static const char *const devices[] = { "CON", "PRN", "AUX", "NUL" };

	for (size_t i = 0; i < sizeof(devices) / sizeof(devices[0]); i++)
	{
		if (_stricmp(base, devices[i]) == 0)
		{
			return false;
		}
	}

	// COM0 to COM9 and LPT0 to LPT9
	if (baseLength == 4 && (_strnicmp(base, "COM", 3) == 0 || _strnicmp(base, "LPT", 3) == 0) && base[3] >= '0' && base[3] <= '9')
	{
		return false;
	}

	return true;
}

// Splits the name the game loads a level by into its folder and its file name: "invasion_a" is no folder and "invasion_a", and
// "Custom/mymap" is "Custom/" and "mymap". It says false for any other folder, and for a file name that isn't safe.
inline bool LevelLog_SplitName(const char *levelName, std::string *folder, std::string *file)
{
	if (!levelName)
	{
		return false;
	}

	size_t const customLength = strlen(kLevelLogCustomFolder);
	char const *fileName = levelName;

	if (_strnicmp(levelName, kLevelLogCustomFolder, customLength) == 0)
	{
		fileName = levelName + customLength;
		*folder = kLevelLogCustomFolder;
	}
	else
	{
		*folder = "";
	}

	if (!LevelLog_NameIsSafe(fileName))
	{
		return false;
	}

	*file = fileName;
	return true;
}

// Where a level's log goes beside the level: levelsFolder is the folder the levels are kept in, with no slash at the end. Empty if the
// level's name can't be used.
inline std::string LevelLog_PathBesideTheLevel(const char *levelsFolder, const char *levelName)
{
	std::string folder, file;

	if (!levelsFolder || !LevelLog_SplitName(levelName, &folder, &file))
	{
		return std::string();
	}

	return std::string(levelsFolder) + "/" + folder + file + kLevelLogEnding;
}

// Where a level's log goes in the game's own folder: gamesFolder is that folder with a slash at the end, or empty for the folder the
// game started from. A level's name that can't be used gives the one fixed name.
inline std::string LevelLog_PathInTheGamesFolder(const char *gamesFolder, const char *levelName)
{
	std::string folder, file;
	std::string const start = gamesFolder ? gamesFolder : "";

	if (!LevelLog_SplitName(levelName, &folder, &file))
	{
		return start + kLevelLogFallbackName;
	}

	return start + file + kLevelLogEnding;
}

// What the writer says when it doesn't write
static const char *const kLevelLogCantOpen = "the file can't be opened or made there";
static const char *const kLevelLogNotAPlainFile = "something with that name is there that isn't a plain file, such as a folder or a link, so it's left alone";
static const char *const kLevelLogNotOurs = "a file with that name is there that the game didn't write, so it's left alone";
static const char *const kLevelLogCantWrite = "the file couldn't be written";

// Writes text to path as a level's log, and gives back 0 if it did, or what stopped it. text has to start with kLevelLogFirstLine. It
// opens the name itself, never what it points to if it's a link, and only writes over a plain file with one name that starts with
// that line. Anything else there is left as it is. It writes at most kLevelLogMostBytes.
inline const char *LevelLog_WriteFile(const std::string &path, const std::string &text)
{
	// FILE_FLAG_OPEN_REPARSE_POINT opens a link itself, so its target is never touched, and nothing else can open the file meanwhile
	HANDLE const file = ::CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, NULL);

	if (file == INVALID_HANDLE_VALUE)
	{
		return kLevelLogCantOpen;
	}

	BY_HANDLE_FILE_INFORMATION what;
	const char *problem = 0;

	if (!::GetFileInformationByHandle(file, &what))
	{
		problem = kLevelLogCantOpen;
	}
	else if ((what.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) != 0 || what.nNumberOfLinks != 1)
	{
		// A second name for the same file could be anywhere, so a file with one is left alone too
		problem = kLevelLogNotAPlainFile;
	}
	else if (what.nFileSizeHigh != 0 || what.nFileSizeLow != 0)
	{
		// There's a file already: only one that starts with our first line is ours to write over
		size_t const firstLineLength = strlen(kLevelLogFirstLine);
		char start[128];
		DWORD read = 0;

		if (!::ReadFile(file, start, (DWORD)firstLineLength, &read, NULL) || read != firstLineLength || memcmp(start, kLevelLogFirstLine, firstLineLength) != 0)
		{
			problem = kLevelLogNotOurs;
		}
	}

	if (!problem)
	{
		DWORD const size = (DWORD)(text.size() < kLevelLogMostBytes ? text.size() : kLevelLogMostBytes);
		DWORD written = 0;

		if (::SetFilePointer(file, 0, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER
			|| !::WriteFile(file, text.data(), size, &written, NULL) || written != size
			|| !::SetEndOfFile(file) || !::FlushFileBuffers(file))
		{
			problem = kLevelLogCantWrite;
		}
	}

	::CloseHandle(file);
	return problem;
}

// Gathers what the game says while a level loads, up to the caps. The game's log calls Note for every line it writes.
struct LevelLogLines
{
	bool gathering;
	std::string text;
	int lines;
	int linesLeftOut;
	int errors;

	void Start()
	{
		gathering = true;
		text.clear();
		lines = 0;
		linesLeftOut = 0;
		errors = 0;
	}

	// isError is true for the lines the game's log writes as errors: what a loader refused is said that way
	void Note(const std::string &line, bool isError)
	{
		if (!gathering)
		{
			return;
		}

		if (isError)
		{
			errors++;
		}

		// Room is kept for the first line, the facts about the file and the last line
		if (lines >= kLevelLogMostLines || text.size() + line.size() > kLevelLogMostBytes - 2048)
		{
			linesLeftOut++;
			return;
		}

		text += line;
		lines++;
	}

	void Stop()
	{
		gathering = false;
	}
};

// The game's side, in level_log.cpp. LoadRifFile starts gathering before it reads a level and finishes when it's done; the game's log
// hands every line to LevelLog_NoteLine. levelsFolder is where the levels are kept (avp_rifs), levelName the name the level was asked
// for by, and levelFile the path it was read from.
void LevelLog_StartGathering();
void LevelLog_NoteLine(const std::string &line, bool isError);
void LevelLog_Finish(const char *levelsFolder, const char *levelName, const char *levelFile, bool failed);

#endif
