// A PLAIN FILE NAME: is a name, given to the game to save or read a file in one of its own folders, only a file's name, and never a
// way to another folder, a device, or a part of a file? Written 2026-11-23 by Melanikus (Drake) for the AvP port (card 6, Tao 1 and 3).
//
// WHY: the multiplayer settings are saved as <save folder>MPConfig/<name>.cfg, and the name comes from outside the code. The player types
// it (any character the keyboard gives, and anything Ctrl+V pastes), and every multiplayer start saves "Previous Game" under a name read
// from language.txt, which anyone can change. Put into the path as it was, a name like ..\..\Windows\x wrote a file in another folder.
// So a name is used only when it's a plain file name, as below; otherwise the caller says why in the log and saves or reads nothing.
//
// A PLAIN FILE NAME:
//   - isn't empty, and is at most kPlainFileNameMost bytes;
//   - holds no control character (below 32, or 127) and none of \ / : * ? " < > | : Windows refuses those in a name, \ and / lead into
//     other folders, and : names a hidden part of a file (a "stream");
//   - isn't "." or "..", the names of a folder and the one above it;
//   - doesn't end in a dot or a space, which Windows drops, so the name would be another one;
//   - isn't a device's name: CON, PRN, AUX, NUL, CONIN$, CONOUT$, COM0 to COM9 and LPT0 to LPT9 (and COM or LPT with a raised 1, 2
//     or 3), in any case, and also with an ending after a dot ("nul.cfg"), as Windows takes those for the device.
// Letters above 127 are fine: they are other languages' letters, as language.txt's own names have.
//
// tests/plain_file_name_test.cpp tries these, and reads mp_config.cpp to see that its paths are only made from such names.
#ifndef PLAIN_FILE_NAME_H
#define PLAIN_FILE_NAME_H

#include <string.h>
#include <string>

// The longest name taken, in bytes. A name the player types is at most 20 (menu_data.cpp's MP_Config_Name field), and language.txt's
// names are short; this leaves room for longer names of files already in the folder, and keeps any path made from one well inside MAX_PATH.
static const size_t kPlainFileNameMost = 100;

// Is the part of the name before its first dot (the dots and spaces at its end left off) one of Windows' device names?
inline bool PlainFileName_IsDeviceName(const char *name)
{
	size_t length = strcspn(name, ".");

	while (length > 0 && (name[length - 1] == ' ' || name[length - 1] == '.'))
	{
		length--;
	}

	std::string stem(name, length);

	for (size_t i = 0; i < stem.size(); i++)
	{
		if (stem[i] >= 'a' && stem[i] <= 'z')
		{
			stem[i] = (char)(stem[i] - 'a' + 'A');
		}
	}

	static const char *const kDevices[] = { "CON", "PRN", "AUX", "NUL", "CONIN$", "CONOUT$" };

	for (size_t i = 0; i < sizeof(kDevices) / sizeof(kDevices[0]); i++)
	{
		if (stem == kDevices[i])
		{
			return true;
		}
	}

	// COM and LPT with one digit after them, 0 to 9, or the raised 1, 2 and 3 of the Windows letters (bytes 0xB9, 0xB2, 0xB3)
	if (stem.size() == 4 && (stem.compare(0, 3, "COM") == 0 || stem.compare(0, 3, "LPT") == 0))
	{
		unsigned char const last = (unsigned char)stem[3];
		return (last >= '0' && last <= '9') || last == 0xB9 || last == 0xB2 || last == 0xB3;
	}

	return false;
}

// "" when name is a plain file name, as the top of this file says; else why it isn't, in words for the log
inline std::string PlainFileName_Why(const char *name)
{
	if (!name || !name[0])
	{
		return "it is empty";
	}

	size_t const length = strnlen(name, kPlainFileNameMost + 1);

	if (length > kPlainFileNameMost)
	{
		return "it is longer than " + std::to_string((unsigned long long)kPlainFileNameMost) + " characters";
	}

	for (size_t i = 0; i < length; i++)
	{
		unsigned char const c = (unsigned char)name[i];

		if (c < 32 || c == 127)
		{
			return "it holds a control character (number " + std::to_string((unsigned long long)c) + ")";
		}

		if (strchr("\\/:*?\"<>|", c))
		{
			return std::string("it holds the character ") + (char)c + ", which a file's name can't have (\\ and / lead into other folders)";
		}
	}

	if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
	{
		return "it is the name of a folder, not of a file";
	}

	if (name[length - 1] == '.' || name[length - 1] == ' ')
	{
		return "it ends in a dot or a space, which Windows leaves off, so it would be another name";
	}

	if (PlainFileName_IsDeviceName(name))
	{
		return "it is the name of a device, not of a file";
	}

	return std::string();
}

// The name as the log can show it: at most kPlainFileNameMost bytes of it, with each control character shown as '?', so a hostile name
// can't break the log's lines
inline std::string PlainFileName_Printable(const char *name)
{
	if (!name)
	{
		return "(none)";
	}

	std::string shown(name, strnlen(name, kPlainFileNameMost));

	for (size_t i = 0; i < shown.size(); i++)
	{
		unsigned char const c = (unsigned char)shown[i];

		if (c < 32 || c == 127)
		{
			shown[i] = '?';
		}
	}

	return shown;
}

#endif
