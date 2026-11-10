// A player's identity kept between runs: the file it's kept in, read and written as bytes (#21, CRYPTO.md section 4.1). Written 2026-11-09
// by Melanikus for the AvP port.
//
// WHAT'S KEPT. A player's identity (net_identity.h) has public halves anyone may see, and private halves nobody else may. The file keeps both
// in the player's save folder: the public halves as they are, and the private halves sealed by Windows (DPAPI) so that only the same Windows
// user can open them. net_identity_store.cpp does the sealing and the file itself. This part only lays the bytes out and reads them back,
// with nothing from Windows, so that a test can try every way a file can be wrong.
//
// THE FILE IS HOSTILE INPUT (AVP Tao 3). Anyone who can write to the save folder can change it. So it's read to exactly this layout, every
// size checked before anything is used, and anything else refuses the whole file, with the reason:
//
//   8 bytes    "AVPX-ID2", which says it's this file
//   4 bytes    the layout's version, 1, lowest byte first
//   1,665      the public halves: the P-384 key (97) and the ML-KEM key (1,568), exactly as net_identity.h keeps them
//   4 bytes    how many sealed bytes follow, lowest byte first
//   that many  the private halves, sealed by DPAPI
//   nothing after that
//
// Reading a file this way says only that it's laid out right. Whether the private halves really belong to the public ones is checked after
// they're unsealed (NetIdentity_FromKept), and a file that fails that is refused too.

#ifndef NET_IDENTITY_FILE_H
#define NET_IDENTITY_FILE_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "net_identity.h"

static const char kNetIdentityFileMark[8] = { 'A', 'V', 'P', 'X', '-', 'I', 'D', '2' };
uint32_t const kNetIdentityFileVersion = 1;

// What's sealed: the P-384 private key (48 bytes), then the ML-KEM private key (3,168 bytes)
size_t const kNetIdentitySealedPlainBytes = kNetCryptoP384ScalarBytes + kNetPqKemPrivateBytes;

// What DPAPI's sealing adds to that: its own heading, a salt and a check, at least 16 bytes and never anywhere near a kilobyte. A sealed
// part outside these isn't one this game wrote.
size_t const kNetIdentitySealedLeast = kNetIdentitySealedPlainBytes + 16;
size_t const kNetIdentitySealedMost = kNetIdentitySealedPlainBytes + 1024;

// Everything before the sealed bytes, and the most a whole file can be. A bigger file is refused without looking at what's in it.
size_t const kNetIdentityFileHeadBytes = sizeof(kNetIdentityFileMark) + 4 + kNetIdentityStaticBytes + 4;
size_t const kNetIdentityFileMostBytes = 8192;
static_assert(kNetIdentityFileHeadBytes + kNetIdentitySealedMost <= kNetIdentityFileMostBytes, "the largest good file fits in what's read");

// Why a file was refused. Each has its own words (NetIdentityFile_Words), for the log and the tests.
enum NetIdentityFileRefusal
{
	kNetIdentityFileGood,
	kNetIdentityFileTooBig,        // bigger than any file this game writes, so refused without looking at what's in it
	kNetIdentityFileTooShort,      // stops before its heading ends
	kNetIdentityFileNotOurs,       // doesn't start with "AVPX-ID2"
	kNetIdentityFileOtherVersion,  // a layout this game doesn't know
	kNetIdentityFileSealedSize,    // says its sealed part is a size no sealed identity can be
	kNetIdentityFileWrongSize      // its size isn't exactly its heading and the sealed part it says it has
};

inline const char *NetIdentityFile_Words(NetIdentityFileRefusal why)
{
	switch (why)
	{
		case kNetIdentityFileGood: return "the identity file is laid out right";
		case kNetIdentityFileTooBig: return "the identity file is bigger than any this game writes";
		case kNetIdentityFileTooShort: return "the identity file stops before its heading ends";
		case kNetIdentityFileNotOurs: return "the identity file isn't one this game wrote";
		case kNetIdentityFileOtherVersion: return "the identity file is laid out in a way this game doesn't know";
		case kNetIdentityFileSealedSize: return "the identity file's sealed part isn't a size a sealed identity can be";
		case kNetIdentityFileWrongSize: return "the identity file isn't the size its heading says";
	}

	return "the identity file was refused for a reason with no words";
}

// A file that was read: where its public halves and sealed bytes are, inside the bytes that were read. Nothing is copied.
struct NetIdentityFileView
{
	const uint8_t *statics;     // kNetIdentityStaticBytes of them
	const uint8_t *sealed;
	size_t sealedBytes;
};

inline uint32_t NetIdentityFile_ReadNumber(const uint8_t *at)
{
	return (uint32_t)at[0] | ((uint32_t)at[1] << 8) | ((uint32_t)at[2] << 16) | ((uint32_t)at[3] << 24);
}

inline void NetIdentityFile_PutNumber(uint8_t *at, uint32_t value)
{
	at[0] = (uint8_t)(value & 0xFF);
	at[1] = (uint8_t)((value >> 8) & 0xFF);
	at[2] = (uint8_t)((value >> 16) & 0xFF);
	at[3] = (uint8_t)((value >> 24) & 0xFF);
}

// Reads a file's bytes to exactly the layout above. On a refusal, view is left empty, so nothing in it can be used by mistake.
inline NetIdentityFileRefusal NetIdentityFile_Read(const uint8_t *bytes, size_t size, NetIdentityFileView *view)
{
	if (view)
	{
		view->statics = NULL;
		view->sealed = NULL;
		view->sealedBytes = 0;
	}

	if (size > kNetIdentityFileMostBytes)
	{
		return kNetIdentityFileTooBig;
	}

	if (!bytes || !view || size < kNetIdentityFileHeadBytes)
	{
		return kNetIdentityFileTooShort;
	}

	if (memcmp(bytes, kNetIdentityFileMark, sizeof(kNetIdentityFileMark)) != 0)
	{
		return kNetIdentityFileNotOurs;
	}

	size_t at = sizeof(kNetIdentityFileMark);

	if (NetIdentityFile_ReadNumber(bytes + at) != kNetIdentityFileVersion)
	{
		return kNetIdentityFileOtherVersion;
	}

	at += 4;
	const uint8_t *const statics = bytes + at;
	at += kNetIdentityStaticBytes;

	uint32_t const sealedBytes = NetIdentityFile_ReadNumber(bytes + at);
	at += 4;

	if (sealedBytes < kNetIdentitySealedLeast || sealedBytes > kNetIdentitySealedMost)
	{
		return kNetIdentityFileSealedSize;
	}

	// at is the heading's size here, and sealedBytes is at most a little over 4 KB, so this sum can't overflow
	if (size != at + sealedBytes)
	{
		return kNetIdentityFileWrongSize;
	}

	view->statics = statics;
	view->sealed = bytes + at;
	view->sealedBytes = sealedBytes;
	return kNetIdentityFileGood;
}

// Writes a file's bytes into out, which has room for room of them. Gives back how many were written, or 0, writing nothing, if the sealed
// part isn't a size a sealed identity can be or out hasn't room: so it never writes a file the reader would refuse.
inline size_t NetIdentityFile_Write(const uint8_t statics[kNetIdentityStaticBytes], const uint8_t *sealed, size_t sealedBytes, uint8_t *out, size_t room)
{
	if (!statics || !sealed || !out || sealedBytes < kNetIdentitySealedLeast || sealedBytes > kNetIdentitySealedMost)
	{
		return 0;
	}

	size_t const total = kNetIdentityFileHeadBytes + sealedBytes;

	if (total > room)
	{
		return 0;
	}

	size_t at = 0;
	memcpy(out + at, kNetIdentityFileMark, sizeof(kNetIdentityFileMark));
	at += sizeof(kNetIdentityFileMark);
	NetIdentityFile_PutNumber(out + at, kNetIdentityFileVersion);
	at += 4;
	memcpy(out + at, statics, kNetIdentityStaticBytes);
	at += kNetIdentityStaticBytes;
	NetIdentityFile_PutNumber(out + at, (uint32_t)sealedBytes);
	at += 4;
	memcpy(out + at, sealed, sealedBytes);
	return total;
}

#endif
