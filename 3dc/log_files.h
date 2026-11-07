// The files the game's log is kept in, and what it writes when the game crashes. Written 2026-11-07 by Melanikus for the AvP port.
//
// KEEPING THE LAST LOGS: the log used to be emptied every time the game started, so a tester whose game crashed, and who started it
// again to see, lost the log that mattered. Now each start moves the logs along by one before beginning a fresh one: avp_log.txt
// becomes avp_log.1.txt, avp_log.1.txt becomes avp_log.2.txt, and so on up to kLogFilesKept, and the oldest is thrown away. So .1 is
// always the run before this one. The names are plain numbers, not dates, because dates sort differently in different languages.
//
// THE FOLDER IS INPUT TOO: anything could be in the log's folder, put there by anyone who can write to it. So only a plain file with
// one name is ever moved, thrown away, emptied or written to. A folder, a link, or a file with a second name somewhere else is left
// alone, and it stops the moving along there. Every name is opened as itself, never as what a link points to. Nothing here can stop the
// game starting: whatever fails is left as it is, and the game carries on with or without its log.
//
// tests/log_files_test.cpp tries these on files it makes. logString.cpp is what the game uses.
#ifndef LOG_FILES_H
#define LOG_FILES_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <string>

// GetUserNameA, which finds the player's Windows user name to leave it out of the log, is in this library
#pragma comment(lib, "advapi32.lib")

// How many old logs are kept, besides the one being written: avp_log.1.txt is the newest of them, and avp_log.5.txt the oldest
static const int kLogFilesKept = 5;

// The name of old log number n, made from the log's own name: "C:/x/avp_log.txt" and 2 give "C:/x/avp_log.2.txt". A name whose file
// part has no dot gets ".2" on the end.
inline std::string LogFiles_OldName(const std::string &logPath, int n)
{
	size_t const slash = logPath.find_last_of("/\\");
	size_t const dot = logPath.find_last_of('.');
	char number[16];
	_snprintf(number, sizeof(number) - 1, ".%d", n);
	number[sizeof(number) - 1] = '\0';

	if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
	{
		return logPath + number;
	}

	return logPath.substr(0, dot) + number + logPath.substr(dot);
}

// Is this open file a plain file with one name: not a folder, not a link, not one of two names for the same file?
inline bool LogFiles_HandleIsPlainFile(HANDLE file)
{
	BY_HANDLE_FILE_INFORMATION what;

	return ::GetFileInformationByHandle(file, &what)
		&& (what.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) == 0
		&& what.nNumberOfLinks == 1;
}

// Is what's at path a plain file with one name? It looks at the name itself, never at what a link points to. Nothing there says false.
inline bool LogFiles_IsPlainFile(const std::string &path)
{
	// FILE_FLAG_BACKUP_SEMANTICS lets a folder be opened too, so that it can be seen to be one and refused
	HANDLE const file = ::CreateFileA(path.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
		OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, NULL);

	if (file == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	bool const plain = LogFiles_HandleIsPlainFile(file);
	::CloseHandle(file);
	return plain;
}

// Is anything at all there, under that name?
inline bool LogFiles_SomethingIsThere(const std::string &path)
{
	return ::GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

// Moves the logs along by one, as the top of this file says. Each file is only moved into a name with nothing there, so nothing is
// written over. Gives back how many files were moved, for the test.
inline int LogFiles_MoveAlong(const std::string &logPath)
{
	int moved = 0;

	// The oldest one kept goes, to make room
	std::string const oldest = LogFiles_OldName(logPath, kLogFilesKept);

	if (LogFiles_IsPlainFile(oldest))
	{
		::DeleteFileA(oldest.c_str());
	}

	for (int n = kLogFilesKept - 1; n >= 0; n--)
	{
		std::string const from = (n == 0) ? logPath : LogFiles_OldName(logPath, n);
		std::string const to = LogFiles_OldName(logPath, n + 1);

		if (LogFiles_IsPlainFile(from) && !LogFiles_SomethingIsThere(to) && ::MoveFileExA(from.c_str(), to.c_str(), 0))
		{
			moved++;
		}
	}

	return moved;
}

// Opens the log to add lines to, making it if it isn't there. Only a plain file with one name is opened. It's shared, so anyone can read,
// copy, rename or delete it while the game runs.
inline HANDLE LogFiles_OpenToAdd(const std::string &path)
{
	HANDLE const file = ::CreateFileA(path.c_str(), FILE_APPEND_DATA | FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, NULL);

	if (file != INVALID_HANDLE_VALUE && !LogFiles_HandleIsPlainFile(file))
	{
		::CloseHandle(file);
		return INVALID_HANDLE_VALUE;
	}

	return file;
}

// Empties the log, making it if it isn't there. Only a plain file with one name is emptied. Says whether it worked.
inline bool LogFiles_Empty(const std::string &path)
{
	HANDLE const file = ::CreateFileA(path.c_str(), GENERIC_WRITE | FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, NULL);

	if (file == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	bool const emptied = LogFiles_HandleIsPlainFile(file) && ::SetEndOfFile(file);
	::CloseHandle(file);
	return emptied;
}

// ---- What the log leaves out about the player ----------------------------------------------------------------------------------------
//
// Testers post their logs where anyone can read them, so nothing that says who they are, or where their machine is, is ever written.
// Every line goes through LogFiles_LeaveOutThePlayer on its way to the file, the crash lines too, so a line added later can't get past
// it. What it leaves out:
//   - Folders that hold the player's Windows user name, or say how their machine is set out. The save folder is written <save folder>,
//     the game's own folder <game folder>, and the player's own folder %USERPROFILE%, along with its short form (C:\Users\MELANI~1).
//     %APPDATA%, %LOCALAPPDATA%, %TEMP% and %OneDrive% go the same way.
//   - Anyone else's folder: whatever follows \Users\, \Documents and Settings\ or \home\ is written <user>.
//   - The machine named at the start of a network path: \\name\share is written \\<computer>\share.
//   - The player's Windows user name, the computer's name and its network's (domain's) name, anywhere: <user>, <computer> and <domain>.
//     A name of 4 letters or more goes even inside a longer word, and a shorter one wherever it's a whole word. A name that's also an
//     everyday word ("admin", "alien", a single letter) goes everywhere, so the log reads oddly ("<user>s versus Predator"). That's on
//     purpose: a log that leaves out too much is safe, and one that leaves something in isn't.
//   - Network addresses, whole. An address such as 192.168.1.20 is written <local address>, <loopback address> or <internet address>,
//     which still tells a home network from the internet, and a port after one is written <port>. Anything that looks like the newer,
//     longer kind of address (fe80::1, or six groups with colons between) is written <address>.
// Times stay: how long things took is what a log is for. So do the names players type in the lobby, which they chose to be seen.
//
// Four numbers with dots between them always count as an address. So the game's own lines never write a version that way: Windows is
// written "10.0, build 26200", and our own version has three parts.
//
// Given a buffer of its own to write into, it never asks for memory, so the crash lines can use it while the game is crashing.

// How many folders and names it can hold. The most it looks for is 9 folders and 7 names.
static const int kLogFilesMostPrivateFolders = 12;
static const int kLogFilesMostPrivateWords = 10;

// What to leave out, found once when the log starts (LogFiles_PrivateGather), or made up by a test. The "written as" words are always
// fixed text, such as "<user>", never anything found on the machine.
struct LogFiles_Private
{
	char folders[kLogFilesMostPrivateFolders][MAX_PATH];
	const char *foldersWrittenAs[kLogFilesMostPrivateFolders];
	int folderCount;
	char words[kLogFilesMostPrivateWords][MAX_PATH];
	const char *wordsWrittenAs[kLogFilesMostPrivateWords];
	int wordCount;
};

inline void LogFiles_PrivateClear(LogFiles_Private &p)
{
	memset(&p, 0, sizeof(p));
}

inline bool LogFiles_IsSlash(char c)
{
	return c == '\\' || c == '/';
}

// A letter, a digit or _: the characters a word is made of
inline bool LogFiles_IsWordCharacter(char c)
{
	return isalnum((unsigned char)c) || c == '_';
}

// Can a folder's name end just before this character? A slash, the end of a line, or a character no file name can have.
inline bool LogFiles_EndsAName(char c)
{
	return c == '\0' || LogFiles_IsSlash(c) || strchr("\"<>|?*:\r\n\t", c) != NULL;
}

// Do these two letters count as the same? Capitals don't matter to Windows.
inline bool LogFiles_SameLetter(char a, char b)
{
	return tolower((unsigned char)a) == tolower((unsigned char)b);
}

// Adds a folder to leave out, written as writtenAs. A slash on the end is dropped, and a folder too short to be more than a drive (C:)
// is left out, so that every C: in the log doesn't go too.
inline void LogFiles_PrivateAddFolder(LogFiles_Private &p, const char *folder, const char *writtenAs)
{
	if (!folder || p.folderCount >= kLogFilesMostPrivateFolders)
	{
		return;
	}

	size_t length = strlen(folder);

	while (length > 0 && LogFiles_IsSlash(folder[length - 1]))
	{
		length--;
	}

	if (length < 3 || length >= MAX_PATH)
	{
		return;
	}

	memcpy(p.folders[p.folderCount], folder, length);
	p.folders[p.folderCount][length] = '\0';
	p.foldersWrittenAs[p.folderCount] = writtenAs;
	p.folderCount++;
}

// Adds a name to leave out wherever it's a whole word, written as writtenAs. A slash on its end is dropped, and the same name twice is
// only kept once.
inline void LogFiles_PrivateAddWord(LogFiles_Private &p, const char *word, const char *writtenAs)
{
	if (!word || !word[0] || p.wordCount >= kLogFilesMostPrivateWords)
	{
		return;
	}

	size_t length = strlen(word);

	while (length > 0 && LogFiles_IsSlash(word[length - 1]))
	{
		length--;
	}

	if (length == 0 || length >= MAX_PATH)
	{
		return;
	}

	for (int i = 0; i < p.wordCount; i++)
	{
		if (strlen(p.words[i]) == length && _strnicmp(p.words[i], word, length) == 0)
		{
			return;
		}
	}

	memcpy(p.words[p.wordCount], word, length);
	p.words[p.wordCount][length] = '\0';
	p.wordsWrittenAs[p.wordCount] = writtenAs;
	p.wordCount++;
}

// The last part of a folder's name: "Mel" from C:\Users\Mel
inline const char *LogFiles_LastPart(const char *folder)
{
	const char *last = folder;

	for (const char *c = folder; *c; c++)
	{
		if (LogFiles_IsSlash(*c) && c[1] && !LogFiles_IsSlash(c[1]))
		{
			last = c + 1;
		}
	}

	return last;
}

// Reads one of Windows' settings for this player (USERPROFILE and the like) into found. Empty if there's no such setting.
inline const char *LogFiles_Setting(const char *name, char *found, DWORD size)
{
	DWORD const length = ::GetEnvironmentVariableA(name, found, size);

	if (length == 0 || length >= size)
	{
		found[0] = '\0';
	}

	return found;
}

// Finds what to leave out on this machine. saveFolder is where the game keeps its saves and its log, and can be NULL.
inline void LogFiles_PrivateGather(LogFiles_Private &p, const char *saveFolder)
{
	LogFiles_PrivateClear(p);

	char found[MAX_PATH];
	char shortForm[MAX_PATH];

	LogFiles_PrivateAddFolder(p, saveFolder, "<save folder>");

	// The game's own folder: where this program is, without its own name
	DWORD const length = ::GetModuleFileNameA(NULL, found, sizeof(found));

	if (length > 0 && length < sizeof(found))
	{
		char *const name = (char *)LogFiles_LastPart(found);

		if (name > found)
		{
			name[-1] = '\0';
			LogFiles_PrivateAddFolder(p, found, "<game folder>");
		}
	}

	// The player's own folder, and its short form, which some of Windows' answers still use. The short form is kept even when it's the
	// same as the long one, where the disk makes no short forms: it costs nothing, and the test can see it was asked for.
	LogFiles_PrivateAddFolder(p, LogFiles_Setting("USERPROFILE", found, sizeof(found)), "%USERPROFILE%");

	if (found[0])
	{
		LogFiles_PrivateAddWord(p, LogFiles_LastPart(found), "<user>");

		DWORD const shortLength = ::GetShortPathNameA(found, shortForm, sizeof(shortForm));

		if (shortLength > 0 && shortLength < sizeof(shortForm))
		{
			LogFiles_PrivateAddFolder(p, shortForm, "%USERPROFILE%");
			LogFiles_PrivateAddWord(p, LogFiles_LastPart(shortForm), "<user>");
		}
	}

	LogFiles_PrivateAddFolder(p, LogFiles_Setting("APPDATA", found, sizeof(found)), "%APPDATA%");
	LogFiles_PrivateAddFolder(p, LogFiles_Setting("LOCALAPPDATA", found, sizeof(found)), "%LOCALAPPDATA%");
	LogFiles_PrivateAddFolder(p, LogFiles_Setting("TEMP", found, sizeof(found)), "%TEMP%");
	LogFiles_PrivateAddFolder(p, LogFiles_Setting("TMP", found, sizeof(found)), "%TEMP%");
	LogFiles_PrivateAddFolder(p, LogFiles_Setting("OneDrive", found, sizeof(found)), "%OneDrive%");

	// The names, from Windows' settings and from Windows itself, which can differ
	LogFiles_PrivateAddWord(p, LogFiles_Setting("USERNAME", found, sizeof(found)), "<user>");

	DWORD size = sizeof(found);

	if (::GetUserNameA(found, &size))
	{
		LogFiles_PrivateAddWord(p, found, "<user>");
	}

	LogFiles_PrivateAddWord(p, LogFiles_Setting("COMPUTERNAME", found, sizeof(found)), "<computer>");

	size = sizeof(found);

	if (::GetComputerNameA(found, &size))
	{
		LogFiles_PrivateAddWord(p, found, "<computer>");
	}

	LogFiles_PrivateAddWord(p, LogFiles_Setting("USERDOMAIN", found, sizeof(found)), "<domain>");
}

// Where the line goes: into a fixed buffer, which is cut short if it fills, or onto a std::string, when grow isn't NULL. Cutting it short
// only ever loses text; nothing that was to be left out is written instead.
struct LogFiles_Out
{
	char *buffer;
	size_t size;
	size_t used;
	std::string *grow;
};

inline void LogFiles_Put(LogFiles_Out &out, const char *text, size_t length)
{
	if (out.grow)
	{
		out.grow->append(text, length);
		return;
	}

	for (size_t i = 0; i < length && out.used + 1 < out.size; i++)
	{
		out.buffer[out.used++] = text[i];
	}
}

inline void LogFiles_PutText(LogFiles_Out &out, const char *text)
{
	LogFiles_Put(out, text, strlen(text));
}

// How long the copy of folder that starts at line[at] is, or 0 if there isn't one. Capitals don't matter, nor which slash, nor a slash
// written twice (C:\\Users\\x, as some messages write it). With mustEnd, it has to stop where a name could end, so that C:\Users\Mel
// isn't found in C:\Users\Melanie.
inline size_t LogFiles_FolderHere(const char *line, size_t length, size_t at, const char *folder, bool mustEnd)
{
	size_t i = at;
	size_t f = 0;

	while (folder[f])
	{
		if (i >= length)
		{
			return 0;
		}

		if (LogFiles_IsSlash(folder[f]))
		{
			if (!LogFiles_IsSlash(line[i]))
			{
				return 0;
			}

			while (i < length && LogFiles_IsSlash(line[i]))
			{
				i++;
			}

			while (LogFiles_IsSlash(folder[f]))
			{
				f++;
			}
		}
		else
		{
			if (!LogFiles_SameLetter(line[i], folder[f]))
			{
				return 0;
			}

			i++;
			f++;
		}
	}

	if (mustEnd && i < length && !LogFiles_EndsAName(line[i]))
	{
		return 0;
	}

	return i - at;
}

// A name this long or longer goes wherever it is, even inside a longer word (Melanikus in MelanikusGames). A shorter one only goes as a
// whole word, or a name such as "ed" would take every "loaded" with it, and the log would be no use to anyone.
static const size_t kLogFilesNamesGoEverywhereFrom = 4;

// How long the copy of word starting at line[at] is, or 0 if it isn't there, or isn't a whole word when it has to be
inline size_t LogFiles_WordHere(const char *line, size_t length, size_t at, const char *word)
{
	bool const wholeOnly = strlen(word) < kLogFilesNamesGoEverywhereFrom;

	if (wholeOnly && at > 0 && LogFiles_IsWordCharacter(line[at - 1]))
	{
		return 0;
	}

	size_t i = 0;

	while (word[i])
	{
		if (at + i >= length || !LogFiles_SameLetter(line[at + i], word[i]))
		{
			return 0;
		}

		i++;
	}

	if (wholeOnly && at + i < length && LogFiles_IsWordCharacter(line[at + i]))
	{
		return 0;
	}

	return i;
}

// Is there a network address starting at line[at]: four numbers of 0 to 255 with dots between, not part of a longer run of numbers and
// dots? Gives back its length, or 0, and what kind of address it is, as it's to be written.
inline size_t LogFiles_AddressHere(const char *line, size_t length, size_t at, const char **writtenAs)
{
	if (at > 0 && (isdigit((unsigned char)line[at - 1]) || line[at - 1] == '.'))
	{
		return 0;
	}

	int parts[4];
	size_t i = at;

	for (int n = 0; n < 4; n++)
	{
		size_t digits = 0;
		int value = 0;

		while (i + digits < length && digits < 4 && isdigit((unsigned char)line[i + digits]))
		{
			value = value * 10 + (line[i + digits] - '0');
			digits++;
		}

		if (digits == 0 || digits > 3 || value > 255)
		{
			return 0;
		}

		parts[n] = value;
		i += digits;

		if (n < 3)
		{
			if (i >= length || line[i] != '.')
			{
				return 0;
			}

			i++;
		}
	}

	if (i < length && (isdigit((unsigned char)line[i]) || (line[i] == '.' && i + 1 < length && isdigit((unsigned char)line[i + 1]))))
	{
		return 0;
	}

	if (parts[0] == 127)
	{
		*writtenAs = "<loopback address>";
	}
	else if (parts[0] == 10 || parts[0] == 0 || (parts[0] == 172 && parts[1] >= 16 && parts[1] <= 31) || (parts[0] == 192 && parts[1] == 168)
		|| (parts[0] == 169 && parts[1] == 254) || (parts[0] == 255 && parts[1] == 255))
	{
		*writtenAs = "<local address>";
	}
	else
	{
		*writtenAs = "<internet address>";
	}

	return i - at;
}

// Is there something starting at line[at] that looks like the newer, longer kind of address: hexadecimal digits and colons, with a
// double colon (fe80::1) or at least five colons, standing on its own? A time such as 19:05:34 has neither, so it stays.
inline size_t LogFiles_LongAddressHere(const char *line, size_t length, size_t at)
{
	if (at > 0 && (LogFiles_IsWordCharacter(line[at - 1]) || line[at - 1] == ':'))
	{
		return 0;
	}

	size_t i = at;
	int colons = 0;
	bool doubleColon = false;
	bool digit = false;

	while (i < length && (isxdigit((unsigned char)line[i]) || line[i] == ':'))
	{
		if (line[i] == ':')
		{
			colons++;
			doubleColon = doubleColon || (i > at && line[i - 1] == ':');
		}
		else
		{
			digit = true;
		}

		i++;
	}

	if (i < length && LogFiles_IsWordCharacter(line[i]))
	{
		return 0;
	}

	return (digit && (doubleColon || colons >= 5)) ? i - at : 0;
}

// After an address: a port such as :23513, or ]:23513 after the long kind in brackets. Gives back how much of the line it is, or 0.
inline size_t LogFiles_PortHere(const char *line, size_t length, size_t at)
{
	size_t i = at;

	if (i < length && line[i] == ']')
	{
		i++;
	}

	if (i >= length || line[i] != ':')
	{
		return 0;
	}

	i++;
	size_t digits = 0;

	while (i + digits < length && isdigit((unsigned char)line[i + digits]))
	{
		digits++;
	}

	return (digits >= 1 && digits <= 5) ? i + digits - at : 0;
}

// The machine named at the start of a network path, \\name\share, standing on its own (not the \\ in C:\\Users, nor \\?\ or \\.\).
// Gives back how long the \\ and the name are, or 0.
inline size_t LogFiles_NetworkPathHere(const char *line, size_t length, size_t at)
{
	if (at + 2 >= length || !LogFiles_IsSlash(line[at]) || !LogFiles_IsSlash(line[at + 1]))
	{
		return 0;
	}

	if (at > 0 && !isspace((unsigned char)line[at - 1]) && !strchr("\"'(<[=,", line[at - 1]))
	{
		return 0;
	}

	size_t i = at + 2;

	if (!LogFiles_IsWordCharacter(line[i]) && line[i] != '-')
	{
		return 0;
	}

	while (i < length && !LogFiles_IsSlash(line[i]) && !isspace((unsigned char)line[i]) && line[i] != '"')
	{
		i++;
	}

	return i - at;
}

// The folders that hold every user's own: whatever follows one of these is a user's name
static const char *const kLogFilesUsersFolders[] = { "\\Users\\", "\\Documents and Settings\\", "\\home\\" };

// Does one of the users' folders start at line[at]? Gives back how long it is, with any extra slashes after it, or 0.
inline size_t LogFiles_UsersFolderHere(const char *line, size_t length, size_t at)
{
	for (size_t u = 0; u < sizeof(kLogFilesUsersFolders) / sizeof(kLogFilesUsersFolders[0]); u++)
	{
		size_t const found = LogFiles_FolderHere(line, length, at, kLogFilesUsersFolders[u], false);

		if (found)
		{
			return found;
		}
	}

	return 0;
}

// Writes line to out, with the player left out, as the top of this part says. At each place in the line the first of these that's
// there wins: one of the player's folders (the longest), a network path's machine, anyone's users' folder, an address, the long kind of
// address, one of the names. Anything else is copied as it is.
inline void LogFiles_LeaveOutThePlayerInto(const char *line, size_t length, const LogFiles_Private &p, LogFiles_Out &out)
{
	size_t at = 0;

	while (at < length)
	{
		// One of the player's folders. The longest wins, so the save folder isn't written as %USERPROFILE%\Documents\...
		size_t longest = 0;
		const char *folderWrittenAs = NULL;

		for (int f = 0; f < p.folderCount; f++)
		{
			size_t const found = LogFiles_FolderHere(line, length, at, p.folders[f], true);

			if (found > longest)
			{
				longest = found;
				folderWrittenAs = p.foldersWrittenAs[f];
			}
		}

		if (longest)
		{
			LogFiles_PutText(out, folderWrittenAs);
			at += longest;
			continue;
		}

		// A network path's machine
		size_t found = LogFiles_NetworkPathHere(line, length, at);

		if (found)
		{
			LogFiles_Put(out, line + at, 2);
			LogFiles_PutText(out, "<computer>");
			at += found;
			continue;
		}

		// Anyone's users' folder, and the name after it, which goes up to the next slash or the end of the line, spaces and all
		found = LogFiles_UsersFolderHere(line, length, at);

		if (found)
		{
			LogFiles_Put(out, line + at, found);
			at += found;

			size_t nameEnd = at;

			while (nameEnd < length && !LogFiles_IsSlash(line[nameEnd]) && !strchr("\"\r\n", line[nameEnd]))
			{
				nameEnd++;
			}

			if (nameEnd > at)
			{
				LogFiles_PutText(out, "<user>");
				at = nameEnd;
			}

			continue;
		}

		// A network address, and its port. These come before the names, so a name made of digits can't cut an address in two.
		const char *addressWrittenAs = NULL;
		found = LogFiles_AddressHere(line, length, at, &addressWrittenAs);

		if (!found)
		{
			found = LogFiles_LongAddressHere(line, length, at);
			addressWrittenAs = "<address>";
		}

		if (found)
		{
			LogFiles_PutText(out, addressWrittenAs);
			at += found;

			size_t const port = LogFiles_PortHere(line, length, at);

			if (port)
			{
				LogFiles_PutText(out, line[at] == ']' ? "]:<port>" : ":<port>");
				at += port;
			}

			continue;
		}

		// One of the names (a short one only as a whole word). The longest wins, in case one name is the start of another.
		longest = 0;
		const char *wordWrittenAs = NULL;

		for (int w = 0; w < p.wordCount; w++)
		{
			size_t const word = LogFiles_WordHere(line, length, at, p.words[w]);

			if (word > longest)
			{
				longest = word;
				wordWrittenAs = p.wordsWrittenAs[w];
			}
		}

		if (longest)
		{
			LogFiles_PutText(out, wordWrittenAs);
			at += longest;
			continue;
		}

		LogFiles_Put(out, line + at, 1);
		at++;
	}
}

// A line with the player left out, for the log's writer
inline std::string LogFiles_LeaveOutThePlayer(const std::string &line, const LogFiles_Private &p)
{
	std::string written;
	written.reserve(line.size() + 32);

	LogFiles_Out out = { NULL, 0, 0, &written };
	LogFiles_LeaveOutThePlayerInto(line.data(), line.size(), p, out);
	return written;
}

// The same into a fixed buffer of size characters, ending with a 0, for when the game is crashing and mustn't ask for memory
inline void LogFiles_LeaveOutThePlayer(const char *line, const LogFiles_Private &p, char *buffer, size_t size)
{
	if (size == 0)
	{
		return;
	}

	LogFiles_Out out = { buffer, size, 0, NULL };
	LogFiles_LeaveOutThePlayerInto(line, strlen(line), p, out);
	buffer[out.used] = '\0';
}

// ---- What the log says when the game crashes ------------------------------------------------------------------------------------------

// The most places a crash line names. Windows' own steps in handing on the crash come first, so there are enough for the game's too.
static const int kLogFilesMostCrashPlaces = 32;

// Where an address is: the file name of the program or library it's in, with no folder (a folder holds the player's Windows user name),
// and how far into it the address is. With those and the .map file kept beside each build, a function can be named. An address in
// nothing that's loaded comes back as "?".
inline void LogFiles_DescribePlace(const void *address, char *words, size_t size)
{
	HMODULE module = NULL;
	char path[MAX_PATH];

	if (!::GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)address, &module)
		|| !module || ::GetModuleFileNameA(module, path, sizeof(path)) == 0)
	{
		_snprintf(words, size - 1, "? %p", address);
		words[size - 1] = '\0';
		return;
	}

	path[sizeof(path) - 1] = '\0';
	char const *name = path;

	for (char const *c = path; *c; c++)
	{
		if (*c == '\\' || *c == '/')
		{
			name = c + 1;
		}
	}

	_snprintf(words, size - 1, "%s+%IX", name, (size_t)((char const *)address - (char const *)module));
	words[size - 1] = '\0';
}

// The crash lines: what kind of crash it was (Windows' exception code), where, and then the places on the stack, each as a file name
// and a distance into it. No memory and no registers are written, only those. words has room for size characters.
inline void LogFiles_DescribeCrash(unsigned long code, const void *where, void *const *places, int count, char *words, size_t size)
{
	char place[MAX_PATH + 32];
	LogFiles_DescribePlace(where, place, sizeof(place));

	int used = _snprintf(words, size - 1, "crash: code %08lX at %s\r\ncrash: on the stack:", code, place);

	if (used < 0)
	{
		words[size - 1] = '\0';
		return;
	}

	for (int i = 0; i < count && i < kLogFilesMostCrashPlaces && (size_t)used < size - 1; i++)
	{
		LogFiles_DescribePlace(places[i], place, sizeof(place));
		int const added = _snprintf(words + used, size - 1 - used, " %s", place);

		if (added < 0)
		{
			break;
		}

		used += added;
	}

	if ((size_t)used < size - 2)
	{
		words[used++] = '\r';
		words[used++] = '\n';
	}

	words[used < (int)size ? used : (int)size - 1] = '\0';
}

#endif
