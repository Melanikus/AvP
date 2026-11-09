// A player's identity kept between runs: the sealing (DPAPI) and the file. net_identity_store.h says what it's for, where it's kept, and
// why. Written 2026-11-09 by Melanikus for the AvP port (#21). Modified 2026-11-09 by Melanikus for the AvP port: Primary's three findings
// (%LOCALAPPDATA% rather than Documents, the file's owner checked and set, and its data flushed to the disk before it's put in place).

#include <windows.h>
#include <wincrypt.h>
#include <aclapi.h>
#include <shlobj.h>

#include <string.h>
#include <string>

#include "net_identity_store.h"

#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

// Whether this is the game's second copy on this PC (utilities.cpp, -dev -secondcopy). Named here alone, rather than through utilities.h,
// so that net_identity_store_test can build this file with a stand-in for it.
bool SaveFolder_IsTheSecondCopy();

namespace
{
	// The folder in %LOCALAPPDATA%, and the file's name in it: one for the game, and one for its second copy
	const wchar_t *const kFolderName = L"AVPx";
	const wchar_t *const kFileName = L"identity-v2.bin";
	const wchar_t *const kSecondCopyFileName = L"identity-v2-second-copy.bin";

	// What only this game adds to what it seals (DPAPI's "entropy"), so another program sealing for the same user can't be handed this blob
	// and open it as its own
	const char kSealWords[] = "AVPX identity v2";

	// Fixed words for why a file was refused, beyond its layout (net_identity_file.h has those)
	const char *const kNotThisUsers = "the identity file isn't owned by this Windows user, so it may not be theirs";
	const char *const kUnsealedWrongSize = "the identity file's sealed part opened to the wrong size";
	const char *const kHalvesDontMatch = "the identity file's private halves don't belong to its public ones";

	DATA_BLOB Entropy(void)
	{
		DATA_BLOB entropy;
		entropy.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(kSealWords));
		entropy.cbData = static_cast<DWORD>(sizeof(kSealWords) - 1);
		return entropy;
	}

	// This Windows user's SID, from the game's own token, copied into sid, which has room for room bytes
	bool ThisUser(BYTE *sid, DWORD room)
	{
		HANDLE token = NULL;

		if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
		{
			return false;
		}

		// A TOKEN_USER and the SID it points to, kept in DWORDs so the TOKEN_USER is lined up as Windows needs
		DWORD answer[(sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE) / sizeof(DWORD) + 1];
		DWORD got = 0;
		bool const asked = GetTokenInformation(token, TokenUser, answer, sizeof(answer), &got) != FALSE;
		CloseHandle(token);

		if (!asked)
		{
			return false;
		}

		PSID const user = reinterpret_cast<TOKEN_USER *>(answer)->User.Sid;
		return user && IsValidSid(user) && CopySid(room, sid, user) != FALSE;
	}

	// Whether the open file's owner is this Windows user. It's asked of the handle the file is read through, so the file can't be swapped
	// for another between this check and the reading.
	bool OwnedByThisUser(HANDLE file)
	{
		BYTE user[SECURITY_MAX_SID_SIZE];

		if (!ThisUser(user, sizeof(user)))
		{
			return false;
		}

		PSID owner = NULL;
		PSECURITY_DESCRIPTOR descriptor = NULL;

		if (GetSecurityInfo(file, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION, &owner, NULL, NULL, NULL, &descriptor) != ERROR_SUCCESS)
		{
			return false;
		}

		bool const same = owner && IsValidSid(owner) && EqualSid(owner, user);
		LocalFree(descriptor);
		return same;
	}

	// Reads a file into bytes, which has room for room of them, and no more than that. A file bigger than room is read only as far as room,
	// which is one byte more than any good file, so the layout's reader (NetIdentityFile_Read) refuses it as too big. False if it isn't
	// there (gone set), isn't this user's (notTheirs set), or couldn't be read.
	bool ReadWholeFile(const wchar_t *path, uint8_t *bytes, size_t room, size_t *size, bool *gone, bool *notTheirs)
	{
		*size = 0;
		*gone = false;
		*notTheirs = false;

		// Others may read it while it's read here, but nobody may write it
		HANDLE const file = CreateFileW(path, GENERIC_READ | READ_CONTROL, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

		if (file == INVALID_HANDLE_VALUE)
		{
			// Gone only when Windows says there's no such file or folder: a file that's there but can't be opened (in use, a folder, or not
			// allowed) is never taken for the first run, which would replace it
			DWORD const error = GetLastError();
			*gone = error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
			return false;
		}

		if (!OwnedByThisUser(file))
		{
			CloseHandle(file);
			*notTheirs = true;
			return false;
		}

		size_t got = 0;
		bool failed = false;

		while (got < room)
		{
			DWORD read = 0;

			if (!ReadFile(file, bytes + got, static_cast<DWORD>(room - got), &read, NULL))
			{
				failed = true;
				break;
			}

			if (read == 0)
			{
				break;
			}

			got += read;
		}

		CloseHandle(file);

		if (failed)
		{
			return false;
		}

		*size = got;
		return true;
	}

	// Writes bytes to path, a new file, owned by this Windows user and readable and writable by this user alone, and makes sure they're on
	// the disk itself before saying yes
	bool WriteWholeFile(const wchar_t *path, const uint8_t *bytes, size_t size)
	{
		BYTE user[SECURITY_MAX_SID_SIZE];

		if (!ThisUser(user, sizeof(user)))
		{
			return false;
		}

		// Who may do what with the file: one entry, this user, everything. Protected, so nothing from the folder above is added to it.
		DWORD acl[(sizeof(ACL) + sizeof(ACCESS_ALLOWED_ACE) + SECURITY_MAX_SID_SIZE) / sizeof(DWORD) + 1];
		ACL *const list = reinterpret_cast<ACL *>(acl);
		SECURITY_DESCRIPTOR descriptor;

		if (!InitializeAcl(list, sizeof(acl), ACL_REVISION) ||
			!AddAccessAllowedAce(list, ACL_REVISION, FILE_ALL_ACCESS, user) ||
			!InitializeSecurityDescriptor(&descriptor, SECURITY_DESCRIPTOR_REVISION) ||
			!SetSecurityDescriptorOwner(&descriptor, user, FALSE) ||
			!SetSecurityDescriptorDacl(&descriptor, TRUE, list, FALSE) ||
			!SetSecurityDescriptorControl(&descriptor, SE_DACL_PROTECTED, SE_DACL_PROTECTED))
		{
			return false;
		}

		SECURITY_ATTRIBUTES attributes;
		attributes.nLength = sizeof(attributes);
		attributes.lpSecurityDescriptor = &descriptor;
		attributes.bInheritHandle = FALSE;

		// A new file only, since Windows gives a file its owner and its list only as it's made: one left over from before goes first
		DeleteFileW(path);
		HANDLE const file = CreateFileW(path, GENERIC_WRITE, 0, &attributes, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);

		if (file == INVALID_HANDLE_VALUE)
		{
			return false;
		}

		DWORD written = 0;
		bool ok = WriteFile(file, bytes, static_cast<DWORD>(size), &written, NULL) != FALSE && written == size;

		// On the disk itself, not only in Windows' cache, before the file is put in place: so a power cut can't leave the kept identity the
		// right size and full of zeros (Primary's read, finding 1)
		ok = ok && FlushFileBuffers(file) != FALSE;
		ok = (CloseHandle(file) != FALSE) && ok;
		return ok;
	}

	// This PC's own file: %LOCALAPPDATA%\AVPx\ and the file's name. With makeFolder, the AVPx folder is made if it isn't there yet. An empty
	// path when Windows can't say where LocalAppData is.
	std::wstring KeptIdentityPath(bool makeFolder)
	{
		PWSTR local = NULL;

		if (SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &local) != S_OK || !local)
		{
			CoTaskMemFree(local);
			return std::wstring();
		}

		std::wstring const folder = std::wstring(local) + L"\\" + kFolderName;
		CoTaskMemFree(local);

		if (makeFolder && !CreateDirectoryW(folder.c_str(), NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
		{
			return std::wstring();
		}

		return folder + L"\\" + (SaveFolder_IsTheSecondCopy() ? kSecondCopyFileName : kFileName);
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

NetIdentityStoreResult NetIdentityStore_LoadFrom(const wchar_t *path, NetIdentity *out, const char **why)
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
	bool notTheirs = false;

	if (!ReadWholeFile(path, bytes, sizeof(bytes), &size, &gone, &notTheirs))
	{
		if (gone)
		{
			return kNetIdentityStoreNoFile;
		}

		if (notTheirs)
		{
			*said = kNotThisUsers;
			return kNetIdentityStoreRefused;
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

bool NetIdentityStore_KeepTo(const wchar_t *path, const NetIdentity *id)
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

	// Written whole under a name of its own, made sure of on the disk, then put in place of the old one in a single step
	std::wstring const fresh = std::wstring(path) + L".new";
	bool const kept = WriteWholeFile(fresh.c_str(), file, size) &&
		MoveFileExW(fresh.c_str(), path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;

	if (!kept)
	{
		DeleteFileW(fresh.c_str());
	}

	return kept;
}

NetIdentityStoreResult NetIdentityStore_Load(NetIdentity *out, const char **why)
{
	return NetIdentityStore_LoadFrom(KeptIdentityPath(false).c_str(), out, why);
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

	if (!NetIdentityStore_KeepTo(KeptIdentityPath(true).c_str(), out))
	{
		NetIdentity_Free(out);
		return false;
	}

	return true;
}
