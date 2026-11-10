// A player's identity kept between runs: the sealing (DPAPI) and the file. net_identity_store.h says what it's for, where it's kept, and
// why. Written 2026-11-09 by Melanikus for the AvP port (#21). Modified 2026-11-09 by Melanikus for the AvP port: Primary's three findings
// (%LOCALAPPDATA% rather than Documents, the file's owner checked and set, and its data flushed to the disk before it's put in place), and
// his fourth: a link at the file's place is refused as a link, never followed.

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

// Whether this is the game's second copy on this PC (-dev -secondcopy), and whether it's a Hudson run (-dev -hudson), both from
// utilities.cpp. Named here alone, rather than through utilities.h, so that net_identity_store_test can build this file with stand-ins.
// Modified 2026-11-09 by Melanikus for the AvP port: the first had no definition in the game at all (only the test had one), so the
// game couldn't link; utilities.cpp defines both now, and the test checks it does.
bool SaveFolder_IsTheSecondCopy();
bool SaveFolder_IsAHudsonRun();

namespace
{
	// The folder in %LOCALAPPDATA%, and the file's name in it: one for the game, and one for its second copy. Modified 2026-11-09 by
	// Melanikus for the AvP port (Fifth's read): a Hudson run has two of its own, so a run of the test marine never reads, replaces or
	// sends the player's own identity, whose approvals are the player's. Making a new identity in a run would have lost them.
	const wchar_t *const kFolderName = L"AVPx";
	const wchar_t *const kFileName = L"identity-v2.bin";
	const wchar_t *const kSecondCopyFileName = L"identity-v2-second-copy.bin";
	const wchar_t *const kHudsonFileName = L"identity-v2-hudson.bin";
	const wchar_t *const kHudsonSecondCopyFileName = L"identity-v2-hudson-second-copy.bin";

	// What only this game adds to what it seals (DPAPI's "entropy"), so another program sealing for the same user can't be handed this blob
	// and open it as its own
	const char kSealWords[] = "AVPX identity v2";

	// Fixed words for why a file was refused, beyond its layout (net_identity_file.h has those)
	const char *const kNotThisUsers = "the identity file isn't owned by this Windows user, so it may not be theirs";
	const char *const kIsALink = "the identity file is a link, and the game only ever writes a plain file";
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
	// there (gone set), is refused (refused set to the words why: a link, or not this user's), or couldn't be read.
	bool ReadWholeFile(const wchar_t *path, uint8_t *bytes, size_t room, size_t *size, bool *gone, const char **refused)
	{
		*size = 0;
		*gone = false;
		*refused = NULL;

		// Others may read it while it's read here, but nobody may write it. A link is opened as itself, never followed to what it points at
		// (FILE_FLAG_OPEN_REPARSE_POINT), so that it can be seen to be one, just below.
		HANDLE const file = CreateFileW(path, GENERIC_READ | READ_CONTROL, FILE_SHARE_READ, NULL, OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, NULL);

		if (file == INVALID_HANDLE_VALUE)
		{
			// Gone only when Windows says there's no such file or folder: a file that's there but can't be opened (in use, a folder, or not
			// allowed) is never taken for the first run, which would replace it
			DWORD const error = GetLastError();
			*gone = error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
			return false;
		}

		// The game only ever writes a plain file here. A link of any kind (a symbolic link, or another "reparse point") was put there by
		// something else, so it's refused as a link, whatever it points at (Primary's read of caddf2d1, Tao 3).
		BY_HANDLE_FILE_INFORMATION about;

		if (!GetFileInformationByHandle(file, &about))
		{
			CloseHandle(file);
			return false;
		}

		if ((about.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
		{
			CloseHandle(file);
			*refused = kIsALink;
			return false;
		}

		if (!OwnedByThisUser(file))
		{
			CloseHandle(file);
			*refused = kNotThisUsers;
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

	// Keeps bytes at path: written whole under a name of its own beside it (WriteWholeFile), then put in place of whatever was at path in a
	// single step, so a crash or a full disk never leaves half a file. False, with path as it was and nothing left beside it, if any of
	// that was refused. (Added 2026-11-10 by Melanikus for the AvP port, #175's last part: the identity and the kept yeses are both kept
	// this one way.)
	bool KeepWholeFile(const wchar_t *path, const uint8_t *bytes, size_t size)
	{
		std::wstring const fresh = std::wstring(path) + L".new";
		bool const kept = WriteWholeFile(fresh.c_str(), bytes, size) &&
			MoveFileExW(fresh.c_str(), path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;

		if (!kept)
		{
			DeleteFileW(fresh.c_str());
		}

		return kept;
	}

	// A file of this PC's own: %LOCALAPPDATA%\AVPx\ and its name. With makeFolder, the AVPx folder is made if it isn't there yet. An empty
	// path when Windows can't say where LocalAppData is. (Modified 2026-11-10 by Melanikus for the AvP port, #175's last part: the kept
	// yeses' file is found the same way, so this takes the file's name.)
	std::wstring KeptPath(const wchar_t *name, bool makeFolder)
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

		return folder + L"\\" + name;
	}

	std::wstring KeptIdentityPath(bool makeFolder)
	{
		return KeptPath(NetIdentityStore_FileName(SaveFolder_IsAHudsonRun(), SaveFolder_IsTheSecondCopy()), makeFolder);
	}

	// The kept yeses' file names, and the words for a file there that the game didn't write (#175's last part)
	const wchar_t *const kTrustFileName = L"servers-trusted.cfg";
	const wchar_t *const kTrustSecondCopyFileName = L"servers-trusted-second-copy.cfg";
	const char *const kTrustNotThisUsers = "servers-trusted.cfg isn't owned by this Windows user, so it may not be theirs";
	const char *const kTrustIsALink = "servers-trusted.cfg is a link, and the game only ever writes a plain file";
	const char *const kTrustCantRead = "servers-trusted.cfg couldn't be read";

	// And the pick's (#175 2b)
	const wchar_t *const kPickFileName = L"servers-picked.cfg";
	const wchar_t *const kPickSecondCopyFileName = L"servers-picked-second-copy.cfg";
	const char *const kPickNotThisUsers = "servers-picked.cfg isn't owned by this Windows user, so it may not be theirs";
	const char *const kPickIsALink = "servers-picked.cfg is a link, and the game only ever writes a plain file";
	const char *const kPickCantRead = "servers-picked.cfg couldn't be read";
}

const wchar_t *NetIdentityStore_FileName(bool hudsonRun, bool secondCopy)
{
	if (hudsonRun)
	{
		return secondCopy ? kHudsonSecondCopyFileName : kHudsonFileName;
	}

	return secondCopy ? kSecondCopyFileName : kFileName;
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
	const char *refused = NULL;

	if (!ReadWholeFile(path, bytes, sizeof(bytes), &size, &gone, &refused))
	{
		if (gone)
		{
			return kNetIdentityStoreNoFile;
		}

		if (refused)
		{
			*said = refused;
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
	return KeepWholeFile(path, file, size);
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

// ---- The player's yeses to servers.cfg lines (#175's last part). Added 2026-11-10 by Melanikus for the AvP port: see
// net_identity_store.h. The same reading and writing as the identity's, with the yeses' own rules (servers_trust.h) for what's inside.

const wchar_t *NetIdentityStore_TrustFileName(bool secondCopy)
{
	return secondCopy ? kTrustSecondCopyFileName : kTrustFileName;
}

// The words for a kept file of hashes that the game didn't write, or couldn't read: the yeses' file's, or the pick's (#175 2b)
struct KeptHashesWords
{
	const char *notThisUsers;
	const char *isALink;
	const char *cantRead;
};

// A kept file of hashes, the yeses' or the pick's, read whole into keptText, as the identity is read: through one handle, a plain file
// owned by this Windows user. Read, with size set, or why not. Modified 2026-11-10 by Melanikus for the AvP port (#175 2b): this was the
// yeses' loader's own, and now the pick's loader shares it.
// One byte more than the most a good file can be, so a bigger one is read only that far and refused as too big. Off the stack for its
// size; only the main thread reads it.
static uint8_t keptText[kServersTrustMostBytes + 1];

static NetTrustStoreResult ReadKeptHashes(const wchar_t *path, const KeptHashesWords &words, size_t *size, const char **said)
{
	*size = 0;

	if (!path || !path[0])
	{
		*said = words.cantRead;
		return kNetTrustStoreCantRead;
	}

	bool gone = false;
	const char *refused = NULL;

	if (!ReadWholeFile(path, keptText, sizeof(keptText), size, &gone, &refused))
	{
		if (gone)
		{
			return kNetTrustStoreNoFile;
		}

		if (refused)
		{
			*said = refused == kIsALink ? words.isALink : words.notThisUsers;
			return kNetTrustStoreRefused;
		}

		*said = words.cantRead;
		return kNetTrustStoreCantRead;
	}

	return kNetTrustStoreRead;
}

// What servers_trust.cpp's reader made of a kept file's text, as this file says it: read, or refused with its words
static NetTrustStoreResult TextRead(ServersTrustRefusal read, const char **said)
{
	if (read != kServersTrustRead)
	{
		*said = ServersTrust_Words(read);
		return kNetTrustStoreRefused;
	}

	return kNetTrustStoreRead;
}

NetTrustStoreResult NetIdentityStore_LoadTrustFrom(const wchar_t *path, ServersTrustYeses *out, const char **why)
{
	const char *unused = NULL;
	const char **const said = why ? why : &unused;
	*said = NULL;

	if (!out)
	{
		return kNetTrustStoreCantRead;
	}

	out->count = 0;
	KeptHashesWords const words = { kTrustNotThisUsers, kTrustIsALink, kTrustCantRead };
	size_t size = 0;
	NetTrustStoreResult const kept = ReadKeptHashes(path, words, &size, said);

	if (kept != kNetTrustStoreRead)
	{
		return kept;
	}

	return TextRead(ServersTrust_Read(reinterpret_cast<const char *>(keptText), size, out, NULL), said);
}

bool NetIdentityStore_KeepTrustTo(const wchar_t *path, const ServersTrustYeses &yeses)
{
	if (!path || !path[0])
	{
		return false;
	}

	// The file's whole text, as servers_trust.h has it. Off the stack for its size; only the main thread keeps.
	static char text[kServersTrustMostBytes];
	size_t const size = ServersTrust_Write(yeses, text, sizeof(text));

	if (size == 0)
	{
		return false;
	}

	// Written whole under a name of its own, made sure of on the disk, then put in place of the old one in a single step
	return KeepWholeFile(path, reinterpret_cast<const uint8_t *>(text), size);
}

NetTrustStoreResult NetIdentityStore_LoadTrust(ServersTrustYeses *out, const char **why)
{
	if (SaveFolder_IsAHudsonRun())
	{
		if (out)
		{
			out->count = 0;
		}

		return kNetTrustStoreNoFile;
	}

	return NetIdentityStore_LoadTrustFrom(KeptPath(NetIdentityStore_TrustFileName(SaveFolder_IsTheSecondCopy()), false).c_str(), out, why);
}

bool NetIdentityStore_KeepTrust(const ServersTrustYeses &yeses)
{
	return !SaveFolder_IsAHudsonRun() && NetIdentityStore_KeepTrustTo(KeptPath(NetIdentityStore_TrustFileName(SaveFolder_IsTheSecondCopy()), true).c_str(), yeses);
}

// ---- The player's pick of a servers.cfg line (#175 2b). Added 2026-11-10 by Melanikus for the AvP port: see net_identity_store.h. The
// same reading and writing as the yeses', with the pick's own rules (servers_trust.h) for what's inside.

const wchar_t *NetIdentityStore_PickFileName(bool secondCopy)
{
	return secondCopy ? kPickSecondCopyFileName : kPickFileName;
}

NetTrustStoreResult NetIdentityStore_LoadPickFrom(const wchar_t *path, ServersTrustPick *out, const char **why)
{
	const char *unused = NULL;
	const char **const said = why ? why : &unused;
	*said = NULL;

	if (!out)
	{
		return kNetTrustStoreCantRead;
	}

	out->there = false;
	memset(out->hash, 0, sizeof(out->hash));
	KeptHashesWords const words = { kPickNotThisUsers, kPickIsALink, kPickCantRead };
	size_t size = 0;
	NetTrustStoreResult const kept = ReadKeptHashes(path, words, &size, said);

	if (kept != kNetTrustStoreRead)
	{
		return kept;
	}

	return TextRead(ServersTrust_ReadPick(reinterpret_cast<const char *>(keptText), size, out, NULL), said);
}

bool NetIdentityStore_KeepPickTo(const wchar_t *path, const uint8_t hash[kServersTrustHashBytes])
{
	if (!path || !path[0] || !hash)
	{
		return false;
	}

	char text[512];
	size_t const size = ServersTrust_WritePick(hash, text, sizeof(text));

	if (size == 0)
	{
		return false;
	}

	// Written whole under a name of its own, made sure of on the disk, then put in place of the old one in a single step
	return KeepWholeFile(path, reinterpret_cast<const uint8_t *>(text), size);
}

NetTrustStoreResult NetIdentityStore_LoadPick(ServersTrustPick *out, const char **why)
{
	// A Hudson run reads no pick, as it reads no yes: its servers.cfg is its own copy's
	if (SaveFolder_IsAHudsonRun())
	{
		if (out)
		{
			out->there = false;
			memset(out->hash, 0, sizeof(out->hash));
		}

		return kNetTrustStoreNoFile;
	}

	return NetIdentityStore_LoadPickFrom(KeptPath(NetIdentityStore_PickFileName(SaveFolder_IsTheSecondCopy()), false).c_str(), out, why);
}

bool NetIdentityStore_KeepPick(const uint8_t hash[kServersTrustHashBytes])
{
	return !SaveFolder_IsAHudsonRun() && NetIdentityStore_KeepPickTo(KeptPath(NetIdentityStore_PickFileName(SaveFolder_IsTheSecondCopy()), true).c_str(), hash);
}
