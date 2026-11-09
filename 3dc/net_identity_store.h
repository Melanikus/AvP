// A player's identity kept between runs, on this PC only, its private halves sealed by Windows for that Windows user only (#21, CRYPTO.md
// section 4.1). Written 2026-11-09 by Melanikus for the AvP port.
//
// WHY. A player's identity (net_identity.h) is what proves them to the other side of a game. If it's new every run, friends, and a server
// that has approved a player, see a stranger every time. So it's kept: the public halves as they are, the private halves sealed with DPAPI
// (CryptProtectData), so only the same Windows user on the same machine can open them. Someone who copies the file, a backup or the disk
// gets nothing they can use. A program running as that same user could open it, as it could any key a program keeps for its user.
//
// WHERE. %LOCALAPPDATA%\AVPx\identity-v2.bin, never the save folder in Documents. Windows' OneDrive backup often moves Documents into
// OneDrive and copies it to the player's other PCs, and a DPAPI seal only opens on the PC that made it: two PCs would keep finding the
// other's file unopenable and replacing it, and the player's approval would go each time (Primary's read, finding 2). LocalAppData is the
// folder Windows keeps for one PC and never roams. A copy of the game started with -dev -secondcopy keeps identity-v2-second-copy.bin
// there instead, so two copies on one PC are two players, as their save folders are.
//
// THE FILE is laid out as net_identity_file.h says, and read as hostile input:
// - It's opened once, and everything is checked through that one handle, so it can't be swapped between a check and the read.
// - It has to be a plain file. A link (a symbolic link, or another "reparse point") is opened as itself, never followed, and refused as a
//   link: the game only ever writes a plain file, so a link was put there by something else.
// - Its owner has to be this Windows user. A file someone else put there, even with keys that hold together, is refused: whoever made it
//   would know its private halves (Primary's read, finding 3). The game writes it owned by this user, readable and writable by this user
//   alone: not even SYSTEM or the Administrators group is on its list, since an administrator can take a file over anyway, and backups
//   read past the list. The cost, which is right for a sealed secret: a file restored onto another Windows account or PC is refused as not
//   this user's, and the player confirms a new identity there.
// - A file that can't be used is never half used: what's in it is refused whole, and the player is asked before a new identity replaces it
//   (their friends and any approval would see a new fingerprint).
//
// It's written to a file of its own first, made sure of on the disk itself (FlushFileBuffers, not only Windows' cache), and only then put
// in place of the old one in one step, so a crash, a power cut or a full disk never leaves half a file, or one full of zeros (Primary's
// read, finding 1).
//
// The private halves exist unsealed only in memory, for as long as it takes, and are wiped (SecureZeroMemory) as soon as they're done
// with. Nothing about them, and no fingerprint, goes in the log.
//
// Modified 2026-11-09 by Melanikus for the AvP port: Primary's three findings (the file's place, its owner, and its data on the disk), and
// his fourth, a link refused as a link.

#ifndef NET_IDENTITY_STORE_H
#define NET_IDENTITY_STORE_H

#include <wchar.h>

#include "net_identity.h"
#include "net_identity_file.h"

// What reading the kept identity found
enum NetIdentityStoreResult
{
	kNetIdentityStoreLoaded,      // the kept identity, whole, its halves checked against each other
	kNetIdentityStoreNoFile,      // none is kept yet: the first run, or the file was taken away
	kNetIdentityStoreRefused,     // a file is there, but not one this game can use: laid out wrong, not this user's, or its halves don't belong together
	kNetIdentityStoreCantUnseal,  // Windows won't open it for this user: another Windows account or PC, or the password was reset by someone else
	kNetIdentityStoreCantRead     // a file is there, but it couldn't be read: in use, or not allowed
};

// The words for a result, for the log and the page that asks the player
const char *NetIdentityStore_Words(NetIdentityStoreResult result);

// Reads the identity kept at path into out. why, if it's given, is fixed words saying what was wrong when the answer isn't Loaded (never
// anything from the file). out is whole only for Loaded; for any other answer nothing of it is left.
NetIdentityStoreResult NetIdentityStore_LoadFrom(const wchar_t *path, NetIdentity *out, const char **why);

// Keeps id at path: seals its private halves, writes the file under a name of its own beside path (owned by this user, for this user
// alone), makes sure it's on the disk, then puts it in place of whatever was at path. False, with path as it was, if any of that was refused.
bool NetIdentityStore_KeepTo(const wchar_t *path, const NetIdentity *id);

// The same, at this PC's own file (WHERE, above), which is what the game uses (networking.cpp's RelayIdentityReady)
NetIdentityStoreResult NetIdentityStore_Load(NetIdentity *out, const char **why);

// Makes a new identity and keeps it, replacing any kept before: for the first run, and for a player who has asked for a new one or
// confirmed one. False, with nothing of out left, if it couldn't be made or kept: an identity that isn't kept would be a stranger next run.
bool NetIdentityStore_MakeAndKeep(NetIdentity *out);

#endif
