// A player's identity kept between runs: the sealing (DPAPI) and the file. net_identity_store.h says what it's for. Written 2026-11-09 by
// Melanikus for the AvP port (#21).

#include <windows.h>
#include <wincrypt.h>

#include <stdio.h>
#include <string.h>
#include <string>

#include "net_identity_store.h"

#pragma comment(lib, "crypt32.lib")

// The save folder, ending with a /, or "" when there's none (utilities.cpp). Named here alone, rather than through utilities.h, so that
// net_identity_store_test can build this file with a stand-in for it.
char *GetSaveFolderPath();

namespace
{
	// The file's name in the save folder
	const char *const kIdentityFileName = "identity-v2.bin";

	// What only this game adds to what it seals (DPAPI's "entropy"), so another program sealing for the same user can't be handed this blob
	// and open it as its own
	const char kSealWords[] = "AVPX identity v2";

	// Fixed words for why a file was refused, beyond its layout (net_identity_file.h has those)
	const char *const kUnsealedWrongSize = "the identity file's sealed part opened to the wrong size";
	const char *const kHalvesDontMatch = "the identity file's private halves don't belong to its public ones";

	DATA_BLOB Entropy(void)
	{
		DATA_BLOB entropy;
		entropy.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(kSealWords));
		entropy.cbData = static_cast<DWORD>(sizeof(kSealWords) - 1);
		return entropy;
	}

	// Reads a file into bytes, which has room for room of them, and no more than that. A file bigger than room is read only as far as room,
	// which is one byte more than any good file, so the layout's reader (NetIdentityFile_Read) refuses it as too big. False if it isn't
	// there (gone set) or couldn't be read.
	bool ReadWholeFile(const char *path, uint8_t *bytes, size_t room, size_t *size, bool *gone)
	{
		*size = 0;
		*gone = false;

		FILE *const file = fopen(path, "rb");

		if (!file)
		{
			// Gone only when Windows says there's no such file or folder: a file that's there but can't be opened (in use, or not
			// allowed) is never taken for the first run, which would replace it
			DWORD const attributes = GetFileAttributesA(path);
			DWORD const error = (attributes == INVALID_FILE_ATTRIBUTES) ? GetLastError() : ERROR_SUCCESS;
			*gone = error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
			return false;
		}

		size_t const got = fread(bytes, 1, room, file);
		bool const failed = ferror(file) != 0;
		fclose(file);

		if (failed)
		{
			return false;
		}

		*size = got;
		return true;
	}

	// Writes bytes to path whole, and makes sure they're on the disk, not only in Windows' cache, before saying yes
	bool WriteWholeFile(const char *path, const uint8_t *bytes, size_t size)
	{
		FILE *const file = fopen(path, "wb");

		if (!file)
		{
			return false;
		}

		bool ok = fwrite(bytes, 1, size, file) == size;
		ok = (fflush(file) == 0) && ok;
		ok = (fclose(file) == 0) && ok;
		return ok;
	}
}

const char *NetIdentityStore_Words(NetIdentityStoreResult result)
{
	switch (result)
	{
		case kNetIdentityStoreLoaded: return "the online identity kept on this PC was read";
		case kNetIdentityStoreNoFile: return "no online identity is kept on this PC yet";
		case kNetIdentityStoreRefused: return "the online identity kept on this PC isn't one this game can use";
		case kNetIdentityStoreCantUnseal: return "the online identity kept on this PC couldn't be unlocked on this Windows account";
		case kNetIdentityStoreCantRead: return "the online identity kept on this PC couldn't be read";
	}

	return "the online identity's answer has no words";
}

NetIdentityStoreResult NetIdentityStore_LoadFrom(const char *path, NetIdentity *out, const char **why)
{
	const char *unused = NULL;
	const char **const said = why ? why : &unused;
	*said = NULL;

	if (!out)
	{
		return kNetIdentityStoreCantRead;
	}

	memset(out, 0, sizeof(*out));

	if (!path || !path[0])
	{
		*said = NetIdentityStore_Words(kNetIdentityStoreCantRead);
		return kNetIdentityStoreCantRead;
	}

	// The file as read: public halves and sealed bytes, nothing unsealed. Kept off the stack for its size; only the main thread loads.
	static uint8_t bytes[kNetIdentityFileMostBytes + 1];
	size_t size = 0;
	bool gone = false;

	if (!ReadWholeFile(path, bytes, sizeof(bytes), &size, &gone))
	{
		if (gone)
		{
			return kNetIdentityStoreNoFile;
		}

		*said = NetIdentityStore_Words(kNetIdentityStoreCantRead);
		return kNetIdentityStoreCantRead;
	}

	NetIdentityFileView view;
	NetIdentityFileRefusal const layout = NetIdentityFile_Read(bytes, size, &view);

	if (layout != kNetIdentityFileGood)
	{
		*said = NetIdentityFile_Words(layout);
		return kNetIdentityStoreRefused;
	}

	DATA_BLOB sealed;
	sealed.pbData = const_cast<BYTE *>(view.sealed);
	sealed.cbData = static_cast<DWORD>(view.sealedBytes);
	DATA_BLOB entropy = Entropy();
	DATA_BLOB plain = { 0, NULL };

	if (!CryptUnprotectData(&sealed, NULL, &entropy, NULL, NULL, CRYPTPROTECT_UI_FORBIDDEN, &plain))
	{
		*said = NetIdentityStore_Words(kNetIdentityStoreCantUnseal);
		return kNetIdentityStoreCantUnseal;
	}

	NetIdentityStoreResult result = kNetIdentityStoreLoaded;

	if (plain.cbData != kNetIdentitySealedPlainBytes)
	{
		*said = kUnsealedWrongSize;
		result = kNetIdentityStoreRefused;
	}
	else if (!NetIdentity_FromKept(view.statics, plain.pbData, plain.pbData + kNetCryptoP384ScalarBytes, out))
	{
		*said = kHalvesDontMatch;
		result = kNetIdentityStoreRefused;
	}

	// The private halves, unsealed, are wiped before Windows is given its memory back
	if (plain.pbData)
	{
		SecureZeroMemory(plain.pbData, plain.cbData);
		LocalFree(plain.pbData);
	}

	return result;
}

bool NetIdentityStore_KeepTo(const char *path, const NetIdentity *id)
{
	if (!path || !path[0] || !id || !id->made)
	{
		return false;
	}

	// The private halves, side by side, only for as long as it takes to seal them
	uint8_t plainBytes[kNetIdentitySealedPlainBytes];

	if (!NetIdentity_ExportP384Private(id, plainBytes))
	{
		return false;
	}

	memcpy(plainBytes + kNetCryptoP384ScalarBytes, id->kemPrivate, kNetPqKemPrivateBytes);

	DATA_BLOB plain;
	plain.pbData = plainBytes;
	plain.cbData = static_cast<DWORD>(sizeof(plainBytes));
	DATA_BLOB entropy = Entropy();
	DATA_BLOB sealed = { 0, NULL };

	// For this Windows user only: never CRYPTPROTECT_LOCAL_MACHINE, which would let anyone on the machine open it
	bool const sealedOk = CryptProtectData(&plain, NULL, &entropy, NULL, NULL, CRYPTPROTECT_UI_FORBIDDEN, &sealed) != FALSE;
	SecureZeroMemory(plainBytes, sizeof(plainBytes));

	if (!sealedOk)
	{
		return false;
	}

	// The file as it will be written: public halves and sealed bytes, nothing unsealed. Off the stack for its size; only the main thread keeps.
	static uint8_t file[kNetIdentityFileMostBytes];
	size_t const size = NetIdentityFile_Write(id->statics, sealed.pbData, sealed.cbData, file, sizeof(file));

	SecureZeroMemory(sealed.pbData, sealed.cbData);
	LocalFree(sealed.pbData);

	if (size == 0)
	{
		return false;
	}

	// Written whole under a name of its own, then put in place of the old one in a single step
	std::string const fresh = std::string(path) + ".new";
	bool const kept = WriteWholeFile(fresh.c_str(), file, size) &&
		MoveFileExA(fresh.c_str(), path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;

	if (!kept)
	{
		DeleteFileA(fresh.c_str());
	}

	return kept;
}

// The save folder's own file: the folder ends with a /, ready for a file's name. An empty path when there's no save folder.
static std::string KeptIdentityPath(void)
{
	const char *const folder = GetSaveFolderPath();
	return (folder && folder[0]) ? std::string(folder) + kIdentityFileName : std::string();
}

NetIdentityStoreResult NetIdentityStore_Load(NetIdentity *out, const char **why)
{
	return NetIdentityStore_LoadFrom(KeptIdentityPath().c_str(), out, why);
}

bool NetIdentityStore_MakeAndKeep(NetIdentity *out)
{
	if (!out)
	{
		return false;
	}

	if (!NetIdentity_Make(out))
	{
		return false;
	}

	if (!NetIdentityStore_KeepTo(KeptIdentityPath().c_str(), out))
	{
		NetIdentity_Free(out);
		return false;
	}

	return true;
}
