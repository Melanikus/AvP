// A player's identity kept between runs, in their save folder, its private halves sealed by Windows for that Windows user only (#21,
// CRYPTO.md section 4.1). Written 2026-11-09 by Melanikus for the AvP port.
//
// WHY. A player's identity (net_identity.h) is what proves them to the other side of a game. If it's new every run, friends, and a server
// that has approved a player, see a stranger every time. So it's kept: the public halves as they are, the private halves sealed with DPAPI
// (CryptProtectData), so only the same Windows user on the same machine can open them. Someone who copies the save folder, a backup or the
// disk gets nothing they can use. A program running as that same user could open it, as it could any key a program keeps for its user.
//
// THE FILE is <save folder>/identity-v2.bin, laid out as net_identity_file.h says, and read as hostile input: anyone who can write to the
// save folder can change it. A file that can't be used is never half used: what's in it is refused whole, and the player is asked before a
// new identity replaces it (their friends and any approval would see a new fingerprint). A copy of the game started with -dev -secondcopy
// has its own save folder, so its own identity.
//
// It's written to a file of its own first, and only then put in place of the old one, so a crash or a full disk never leaves half a file.
//
// The private halves exist unsealed only in memory, for as long as it takes, and are wiped (SecureZeroMemory) as soon as they're done
// with. Nothing about them, and no fingerprint, goes in the log.

#ifndef NET_IDENTITY_STORE_H
#define NET_IDENTITY_STORE_H

#include "net_identity.h"
#include "net_identity_file.h"

// What reading the kept identity found
enum NetIdentityStoreResult
{
	kNetIdentityStoreLoaded,      // the kept identity, whole, its halves checked against each other
	kNetIdentityStoreNoFile,      // none is kept yet: the first run, or the file was taken away
	kNetIdentityStoreRefused,     // a file is there, but not one this game can use: laid out wrong, or its halves don't belong together
	kNetIdentityStoreCantUnseal,  // Windows won't open it for this user: another Windows account or PC, or the password was reset by someone else
	kNetIdentityStoreCantRead     // a file is there, but it couldn't be read: in use, or not allowed
};

// The words for a result, for the log and the page that asks the player
const char *NetIdentityStore_Words(NetIdentityStoreResult result);

// Reads the identity kept at path into out. why, if it's given, is fixed words saying what was wrong when the answer isn't Loaded (never
// anything from the file). out is whole only for Loaded; for any other answer nothing of it is left.
NetIdentityStoreResult NetIdentityStore_LoadFrom(const char *path, NetIdentity *out, const char **why);

// Keeps id at path: seals its private halves, writes the file under a name of its own beside path, then puts it in place of whatever was
// at path. False, with path as it was, if any of that was refused.
bool NetIdentityStore_KeepTo(const char *path, const NetIdentity *id);

// The same, at the save folder's own file, which is what the game uses (networking.cpp's RelayIdentityReady)
NetIdentityStoreResult NetIdentityStore_Load(NetIdentity *out, const char **why);

// Makes a new identity and keeps it, replacing any kept before: for the first run, and for a player who has asked for a new one or
// confirmed one. False, with nothing of out left, if it couldn't be made or kept: an identity that isn't kept would be a stranger next run.
bool NetIdentityStore_MakeAndKeep(NetIdentity *out);

#endif
