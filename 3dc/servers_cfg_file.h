// Finding and reading servers.cfg, the game's list of servers (servers_cfg.h says what it holds and every rule). Written 2026-11-10 by
// Melanikus for the AvP port (#175).
//
// WHERE IT IS: in the game's folder, the one that holds bin\, where the game's program is. So it's found from the program's own path,
// never from a setting, the command line or the folder the game was started in: nothing outside the game's folder can say where the
// list of servers is. If the program isn't in a folder called bin, there's no game folder to look in, and so no servers.
//
// HOW IT'S READ (AVP Tao 3): only a plain file is opened (not a folder, a device or a pipe), and at most 16 KB and one byte of it is
// read, so a file of any size costs the game no more than that. What's read goes to servers_cfg.h's reader.

#ifndef SERVERS_CFG_FILE_H
#define SERVERS_CFG_FILE_H

#include <stddef.h>
#include <wchar.h>

#include "servers_cfg.h"

// What became of looking for the file
enum ServersCfgOpening
{
	kServersCfgOpened,       // read: out holds what the reader made of it
	kServersCfgNotThere,     // there's no servers.cfg in the game's folder, so no servers
	kServersCfgNoFolder,     // the program isn't in a folder called bin, or its path couldn't be had, so there's nowhere to look
	kServersCfgNotAFile,     // something called servers.cfg is there, but it isn't a plain file
	kServersCfgCantRead      // it's there, but it couldn't be opened or read
};

// The game's folder from the program's path: the folder that holds the folder the program is in, which has to be called bin (capitals
// or not). C:\Games\AvP\bin\AvP_Classic.exe gives C:\Games\AvP. False, with folder empty, for anything else, or if folder (room
// characters, its 0 counted) can't hold it.
bool ServersCfg_GameFolderOf(const wchar_t *programPath, wchar_t *folder, size_t room);

// Reads folder\servers.cfg into out. Everything in out is written whatever happens: with anything but kServersCfgOpened it holds no
// servers.
ServersCfgOpening ServersCfg_ReadFolder(const wchar_t *folder, ServersCfgFileRead *out);

// The game's own: its folder from its program's path, then ReadFolder
ServersCfgOpening ServersCfg_ReadTheGames(ServersCfgFileRead *out);

// The words for each, for the log
const char *ServersCfg_OpeningWords(ServersCfgOpening opening);

#endif
