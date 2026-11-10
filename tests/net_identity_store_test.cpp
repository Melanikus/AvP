// The test of keeping a player's identity between runs (3dc/net_identity_store.cpp and net_identity.h's NetIdentity_FromKept, #21).
// Written 2026-11-09 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// That an identity kept in a file and read back is the same identity: the same public halves and fingerprint, and private halves that
// still agree the same secrets. That what's sealed is the private halves and nothing else, and opens only with the game's own words. Then
// each way the file can go wrong: none there, a folder where the file should be, laid out wrong, sealed bytes changed (Windows refuses to
// unseal them), sealed bytes that open to the wrong size, public halves changed (the private halves no longer belong to them), and a file
// too big. Each gives its own answer, and none leaves half an identity behind. Then that private halves from another identity are refused,
// and that keeping replaces a file whole, with nothing left beside it, or leaves the old one as it was. Then whose the file is: a kept one
// is this Windows user's, for them alone, and one owned by anyone else is refused (Primary's read, finding 3). Last, it reads the store's
// source for what can't be run here: that it never seals for the whole machine or lets Windows ask the player anything, that a new file is
// flushed to the disk before it's put in place (finding 1), and that it's kept in LocalAppData, not Documents (finding 2).
//
// HOW IT RUNS
//
// It uses Windows' own sealing (DPAPI, crypt32) and the game's real cryptography, so it's built and run on GitHub only, with the libraries
// net_v2b_test uses (tests/net_identity_store_test.args). Its files have fixed names in the folder it runs in, and are deleted at the end.
// It never touches the game's own file in LocalAppData: it uses only NetIdentityStore_LoadFrom and _KeepTo, with its own names. Giving a
// file to the Administrators group needs an administrator, which GitHub's runner is. The program gives back a non-zero result if anything
// failed.
//
// Modified 2026-11-09 by Melanikus for the AvP port: wide paths, and Primary's three findings.

#include <windows.h>
#include <wincrypt.h>
#include <aclapi.h>

#include <fstream>
#include <sstream>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

#include "net_identity_store.h"
#include "source_slice.h"

static int checksFailed = 0;

static void Check(bool passed, std::string const &what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what.c_str());

	if (!passed)
	{
		checksFailed++;
	}
}

static void FailedSlice(std::string const &why)
{
	Check(false, why);
}

// The store asks whether this is the game's second copy, and whether it's a Hudson run, for the name of the game's own file, which this
// test never uses. The game's own are in utilities.cpp, which the test reads to check they're there (TestTheFileNames).
bool SaveFolder_IsTheSecondCopy()
{
	return false;
}

bool SaveFolder_IsAHudsonRun()
{
	return false;
}

static const wchar_t *const kKept = L"net_identity_store_test.bin";
static const wchar_t *const kMissing = L"net_identity_store_test_missing.bin";
static const wchar_t *const kFolder = L"net_identity_store_test_folder";
static const wchar_t *const kLink = L"net_identity_store_test_link.bin";

// CreateSymbolicLinkW's flag for making a link without being an administrator, in developer mode. Named by Windows' own headers only for
// Windows 10, newer than this test's settings ask for, so it's written out: its value is 2 there.
static DWORD const kLinkUnprivileged = 0x2;

// The words the store seals with, as CRYPTO.md section 4.1 gives them: written out here, not taken from the store, so a change there shows
static const char kWords[] = "AVPX identity v2";

static DATA_BLOB Words(void)
{
	DATA_BLOB words;
	words.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(kWords));
	words.cbData = static_cast<DWORD>(sizeof(kWords) - 1);
	return words;
}

// Seals bytes the way the store does, so the test can make a sealed part the store didn't write
static std::vector<uint8_t> Seal(std::vector<uint8_t> const &bytes)
{
	DATA_BLOB plain;
	plain.pbData = const_cast<BYTE *>(bytes.data());
	plain.cbData = static_cast<DWORD>(bytes.size());
	DATA_BLOB words = Words();
	DATA_BLOB sealed = { 0, NULL };
	std::vector<uint8_t> out;

	if (CryptProtectData(&plain, NULL, &words, NULL, NULL, CRYPTPROTECT_UI_FORBIDDEN, &sealed))
	{
		out.assign(sealed.pbData, sealed.pbData + sealed.cbData);
		LocalFree(sealed.pbData);
	}

	return out;
}

// An identity's two private halves side by side, as the store seals them: P-384 first, then ML-KEM
static std::vector<uint8_t> PrivateHalves(const NetIdentity &id)
{
	std::vector<uint8_t> halves(kNetIdentitySealedPlainBytes);
	NetIdentity_ExportP384Private(&id, halves.data());
	memcpy(halves.data() + kNetCryptoP384ScalarBytes, id.kemPrivate, kNetPqKemPrivateBytes);
	return halves;
}

// An identity filled with a byte that's never 0, so a refusal that doesn't empty it shows
static void Dirty(NetIdentity *id)
{
	memset(id, 0xEE, sizeof(*id));
}

static std::vector<uint8_t> ReadAll(const wchar_t *path)
{
	std::ifstream file(path, std::ios::binary);
	return std::vector<uint8_t>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

static void WriteAll(const wchar_t *path, std::vector<uint8_t> const &bytes)
{
	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	file.write(reinterpret_cast<const char *>(bytes.data()), (std::streamsize)bytes.size());
}

// Whether two identities agree the same secret with a third party's key, and open the same sealed secret: the same private halves
static bool SameKeys(NetIdentity *a, NetIdentity *b)
{
	uint8_t otherPublic[kNetCryptoP384PublicBytes];
	NetEcdhKey *const other = NetCrypto_EcdhNew(otherPublic);
	uint8_t one[kNetCryptoP384ScalarBytes];
	uint8_t two[kNetCryptoP384ScalarBytes];
	bool same = other && NetCrypto_EcdhAgree(a->p384, otherPublic, one) && NetCrypto_EcdhAgree(b->p384, otherPublic, two) &&
		memcmp(one, two, sizeof(one)) == 0;
	NetCrypto_EcdhFree(other);

	uint8_t ciphertext[kNetPqKemCiphertextBytes];
	uint8_t sealed[kNetPqKemSecretBytes];
	uint8_t opened[kNetPqKemSecretBytes];
	same = same && NetPq_KemEncapsulate(NetIdentity_KemPublic(a), ciphertext, sealed) && NetPq_KemDecapsulate(b->kemPrivate, ciphertext, opened) &&
		memcmp(sealed, opened, sizeof(sealed)) == 0;
	return same;
}

static bool NothingLeft(const NetIdentity &id)
{
	static NetIdentity const empty = {};
	return !id.made && id.p384 == NULL && memcmp(&id, &empty, sizeof(id)) == 0;
}

static void TestKeepAndRead()
{
	puts("Kept, then read back");

	NetIdentity made;
	Check(NetIdentity_Make(&made), "an identity is made");
	DeleteFileW(kKept);
	Check(NetIdentityStore_KeepTo(kKept, &made), "and kept");

	std::vector<uint8_t> const file = ReadAll(kKept);
	NetIdentityFileView view;
	Check(NetIdentityFile_Read(file.data(), file.size(), &view) == kNetIdentityFileGood &&
		memcmp(view.statics, made.statics, kNetIdentityStaticBytes) == 0, "the file is laid out right, with its public halves as they are");

	bool privateInTheClear = false;

	for (size_t i = 0; i + 32 <= file.size(); i++)
	{
		privateInTheClear = privateInTheClear || memcmp(file.data() + i, made.kemPrivate, 32) == 0;
	}

	Check(!privateInTheClear, "and nothing of the private halves can be found in it unsealed");

	NetIdentity read;
	const char *why = "not set";
	Check(NetIdentityStore_LoadFrom(kKept, &read, &why) == kNetIdentityStoreLoaded && why == NULL, "it's read back");
	Check(read.made && memcmp(read.statics, made.statics, kNetIdentityStaticBytes) == 0 &&
		memcmp(read.fingerprint, made.fingerprint, kNetIdentityFingerprintBytes) == 0, "with the same public halves and fingerprint");
	Check(SameKeys(&made, &read), "and private halves that agree the same secrets as the ones made");

	NetIdentity_Free(&read);
	NetIdentity_Free(&made);
}

static void TestTheSealing()
{
	puts("What's sealed, and what opens it");

	NetIdentity made;
	NetIdentity_Make(&made);
	DeleteFileW(kKept);
	NetIdentityStore_KeepTo(kKept, &made);
	std::vector<uint8_t> const file = ReadAll(kKept);
	NetIdentityFileView view;
	bool const laidOut = NetIdentityFile_Read(file.data(), file.size(), &view) == kNetIdentityFileGood;

	DATA_BLOB sealed = { 0, NULL };
	sealed.pbData = laidOut ? const_cast<BYTE *>(view.sealed) : NULL;
	sealed.cbData = laidOut ? static_cast<DWORD>(view.sealedBytes) : 0;

	DATA_BLOB words = Words();
	DATA_BLOB plain = { 0, NULL };
	bool const opened = laidOut && CryptUnprotectData(&sealed, NULL, &words, NULL, NULL, CRYPTPROTECT_UI_FORBIDDEN, &plain);
	std::vector<uint8_t> halves = PrivateHalves(made);
	Check(opened && plain.cbData == kNetIdentitySealedPlainBytes && memcmp(plain.pbData, halves.data(), halves.size()) == 0,
		"the sealed part opens, with CRYPTO.md's words, to the private halves and nothing else: P-384 first, then ML-KEM");

	if (plain.pbData)
	{
		SecureZeroMemory(plain.pbData, plain.cbData);
		LocalFree(plain.pbData);
		plain.pbData = NULL;
	}

	bool const openedBare = laidOut && CryptUnprotectData(&sealed, NULL, NULL, NULL, NULL, CRYPTPROTECT_UI_FORBIDDEN, &plain);
	Check(laidOut && !openedBare, "and without the words, it doesn't open");

	if (plain.pbData)
	{
		SecureZeroMemory(plain.pbData, plain.cbData);
		LocalFree(plain.pbData);
	}

	SecureZeroMemory(halves.data(), halves.size());
	NetIdentity_Free(&made);
}

static void TestFilesThatCantBeUsed()
{
	puts("Files that can't be used, each with its own answer");

	NetIdentity made;
	NetIdentity_Make(&made);
	DeleteFileW(kKept);
	NetIdentityStore_KeepTo(kKept, &made);
	std::vector<uint8_t> const good = ReadAll(kKept);

	NetIdentity read;
	const char *why = NULL;

	DeleteFileW(kMissing);
	Dirty(&read);
	Check(NetIdentityStore_LoadFrom(kMissing, &read, &why) == kNetIdentityStoreNoFile && NothingLeft(read), "no file is no identity kept yet");

	// A folder can't be opened as a file, but it's there: never taken for no file, which would make a new identity in its place
	CreateDirectoryW(kFolder, NULL);
	Dirty(&read);
	Check(NetIdentityStore_LoadFrom(kFolder, &read, &why) == kNetIdentityStoreCantRead && NothingLeft(read),
		"something there that can't be opened is a file that can't be read, never no file");

	std::vector<uint8_t> notOurs = good;
	notOurs[0] = 'B';
	WriteAll(kKept, notOurs);
	Dirty(&read);
	Check(NetIdentityStore_LoadFrom(kKept, &read, &why) == kNetIdentityStoreRefused && NothingLeft(read) && why &&
		strcmp(why, NetIdentityFile_Words(kNetIdentityFileNotOurs)) == 0, "a file laid out wrong is refused, saying how");

	std::vector<uint8_t> sealedChanged = good;
	sealedChanged[kNetIdentityFileHeadBytes + 40] ^= 0x01;
	WriteAll(kKept, sealedChanged);
	Dirty(&read);
	Check(NetIdentityStore_LoadFrom(kKept, &read, &why) == kNetIdentityStoreCantUnseal && NothingLeft(read),
		"a sealed part with one bit changed isn't opened by Windows");

	// Sealed parts the store didn't write, sealed the same way: the private halves with a byte more, and with a byte fewer
	std::vector<uint8_t> halves = PrivateHalves(made);
	std::vector<uint8_t> longer = halves;
	longer.push_back(0);
	std::vector<uint8_t> shorter(halves.begin(), halves.end() - 1);
	std::vector<uint8_t> const sealedLonger = Seal(longer);
	std::vector<uint8_t> const sealedShorter = Seal(shorter);
	std::vector<uint8_t> wrongSize(kNetIdentityFileMostBytes);
	bool refusedBoth = !sealedLonger.empty() && !sealedShorter.empty();

	for (std::vector<uint8_t> const *sealed : { &sealedLonger, &sealedShorter })
	{
		size_t const size = NetIdentityFile_Write(made.statics, sealed->data(), sealed->size(), wrongSize.data(), wrongSize.size());
		WriteAll(kKept, std::vector<uint8_t>(wrongSize.begin(), wrongSize.begin() + size));
		Dirty(&read);
		refusedBoth = refusedBoth && size > 0 && NetIdentityStore_LoadFrom(kKept, &read, &why) == kNetIdentityStoreRefused && NothingLeft(read) &&
			why && strstr(why, "wrong size") != NULL;
	}

	Check(refusedBoth, "a sealed part that opens to a byte more or a byte fewer than the private halves is refused, saying so");
	SecureZeroMemory(halves.data(), halves.size());
	SecureZeroMemory(longer.data(), longer.size());
	SecureZeroMemory(shorter.data(), shorter.size());

	std::vector<uint8_t> kemPublicChanged = good;
	kemPublicChanged[12 + kNetCryptoP384PublicBytes + 100] ^= 0x01;
	WriteAll(kKept, kemPublicChanged);
	Dirty(&read);
	Check(NetIdentityStore_LoadFrom(kKept, &read, &why) == kNetIdentityStoreRefused && NothingLeft(read) && why &&
		strstr(why, "don't belong") != NULL, "public halves changed by a bit are refused: the private halves no longer belong to them");

	std::vector<uint8_t> tooBig(kNetIdentityFileMostBytes + 1, 0);
	WriteAll(kKept, tooBig);
	Dirty(&read);
	Check(NetIdentityStore_LoadFrom(kKept, &read, &why) == kNetIdentityStoreRefused && NothingLeft(read) && why &&
		strcmp(why, NetIdentityFile_Words(kNetIdentityFileTooBig)) == 0, "a file bigger than any this game writes is refused");

	Dirty(&read);
	Check(NetIdentityStore_LoadFrom(L"", &read, &why) == kNetIdentityStoreCantRead && NothingLeft(read), "no path is no identity read");

	NetIdentity_Free(&made);
}

static void TestHalvesFromAnother()
{
	puts("Private halves from another identity");

	NetIdentity a;
	NetIdentity b;
	NetIdentity_Make(&a);
	NetIdentity_Make(&b);

	uint8_t bP384[kNetCryptoP384ScalarBytes];
	Check(NetIdentity_ExportP384Private(&b, bP384), "an identity's P-384 private number can be taken, to be sealed");

	NetIdentity mixed;
	Check(!NetIdentity_FromKept(a.statics, bP384, b.kemPrivate, &mixed) && NothingLeft(mixed),
		"a's public halves with b's private halves are refused, with nothing left");

	uint8_t aP384[kNetCryptoP384ScalarBytes];
	NetIdentity_ExportP384Private(&a, aP384);
	Check(!NetIdentity_FromKept(a.statics, aP384, b.kemPrivate, &mixed) && NothingLeft(mixed), "and so is a's P-384 half with b's ML-KEM half");

	NetIdentity whole;
	Check(NetIdentity_FromKept(a.statics, aP384, a.kemPrivate, &whole) && whole.made &&
		memcmp(whole.fingerprint, a.fingerprint, kNetIdentityFingerprintBytes) == 0, "a's own halves make a whole again, with a's fingerprint");

	// Halves swapped in memory, past anything Windows checks when a key is brought in: only NetIdentity_HalvesMatch can see these
	Check(NetIdentity_HalvesMatch(&a), "an identity's own halves match");
	NetIdentity swapped = a;
	swapped.p384 = b.p384;
	bool const p384Refused = !NetIdentity_HalvesMatch(&swapped);
	swapped.p384 = a.p384;
	memcpy(swapped.kemPrivate, b.kemPrivate, kNetPqKemPrivateBytes);
	bool const kemRefused = !NetIdentity_HalvesMatch(&swapped);
	SecureZeroMemory(&swapped, sizeof(swapped));
	Check(p384Refused, "another identity's P-384 private key doesn't match a's public halves");
	Check(kemRefused, "nor does its ML-KEM private key");

	uint8_t same[16];
	uint8_t other[16];
	memset(same, 0x5A, sizeof(same));
	memcpy(other, same, sizeof(other));
	bool const sameIsSame = NetIdentity_SameSecret(same, other, sizeof(same));
	other[0] ^= 0x01;
	bool const firstDiffers = !NetIdentity_SameSecret(same, other, sizeof(same));
	other[0] = same[0];
	other[15] ^= 0x80;
	bool const lastDiffers = !NetIdentity_SameSecret(same, other, sizeof(same));
	Check(sameIsSame && firstDiffers && lastDiffers, "two secrets are the same only when every byte is, the first and the last too");

	NetIdentity none = {};
	uint8_t out[kNetCryptoP384ScalarBytes];
	memset(out, 0xEE, sizeof(out));
	Check(!NetIdentity_ExportP384Private(&none, out) && out[0] == 0 && out[47] == 0, "an identity never made gives nothing, and out is wiped");
	memset(out, 0xEE, sizeof(out));
	Check(!NetCrypto_EcdhExportPrivate(NULL, out) && out[0] == 0 && out[47] == 0, "and CNG's own export, given no key, wipes out too");

	SecureZeroMemory(aP384, sizeof(aP384));
	SecureZeroMemory(bP384, sizeof(bP384));
	NetIdentity_Free(&whole);
	NetIdentity_Free(&a);
	NetIdentity_Free(&b);
}

static void TestReplacing()
{
	puts("Keeping again");

	NetIdentity first;
	NetIdentity second;
	NetIdentity_Make(&first);
	NetIdentity_Make(&second);
	DeleteFileW(kKept);
	NetIdentityStore_KeepTo(kKept, &first);
	Check(NetIdentityStore_KeepTo(kKept, &second), "a second identity is kept in place of the first");

	NetIdentity read;
	Check(NetIdentityStore_LoadFrom(kKept, &read, NULL) == kNetIdentityStoreLoaded &&
		memcmp(read.fingerprint, second.fingerprint, kNetIdentityFingerprintBytes) == 0, "and it's the second that's read back");
	Check(GetFileAttributesW((std::wstring(kKept) + L".new").c_str()) == INVALID_FILE_ATTRIBUTES, "with nothing left beside it");

	NetIdentity none = {};
	Check(!NetIdentityStore_KeepTo(kKept, &none), "an identity never made isn't kept");
	NetIdentity_Free(&read);
	Check(NetIdentityStore_LoadFrom(kKept, &read, NULL) == kNetIdentityStoreLoaded &&
		memcmp(read.fingerprint, second.fingerprint, kNetIdentityFingerprintBytes) == 0, "and the file is left as it was");

	// A file can't be put in a folder's place, so keeping there fails at the last step: what was written for it mustn't be left behind
	CreateDirectoryW(kFolder, NULL);
	std::wstring const folderNew = std::wstring(kFolder) + L".new";
	DeleteFileW(folderNew.c_str());
	bool const keptInFolder = NetIdentityStore_KeepTo(kFolder, &second);
	DWORD const folderNow = GetFileAttributesW(kFolder);
	Check(!keptInFolder && folderNow != INVALID_FILE_ATTRIBUTES && (folderNow & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
		GetFileAttributesW(folderNew.c_str()) == INVALID_FILE_ATTRIBUTES,
		"keeping that can't put its file in place says no, leaves what was there, and leaves nothing beside it");

	NetIdentity_Free(&read);
	NetIdentity_Free(&first);
	NetIdentity_Free(&second);
}

static std::string ReadSource(const char *name)
{
	std::string path = __FILE__;
	std::string::size_type const tests = path.rfind("tests");

	if (tests == std::string::npos)
	{
		return "";
	}

	std::ifstream file((path.substr(0, tests) + name).c_str(), std::ios::binary);
	std::stringstream all;
	all << file.rdbuf();
	std::string text = all.str();
	std::string plain;

	for (char c : text)
	{
		if (c != '\r')
		{
			plain += c;
		}
	}

	return plain;
}

// The source with every // comment taken out, to the end of its line, so a check of what the code does isn't fooled by a comment
// that names the very thing the code mustn't do. The store has no // inside a string, so nothing of its code goes with them.
static std::string CodeOnly(const std::string &text)
{
	std::string code;
	size_t at = 0;

	while (at < text.size())
	{
		size_t const comment = text.find("//", at);

		if (comment == std::string::npos)
		{
			code += text.substr(at);
			break;
		}

		code += text.substr(at, comment - at);
		size_t const lineEnd = text.find('\n', comment);

		if (lineEnd == std::string::npos)
		{
			break;
		}

		at = lineEnd;
	}

	return code;
}

static void TestTheSource()
{
	puts("What the store must never do, by reading it");

	std::string const store = ReadSource("3dc/net_identity_store.cpp");
	Check(store.size() > 3000, "the store's source was found and read");

	// Its code, not its comments, which say it never does it (Modified 2026-11-09 by Melanikus for the AvP port: the check read the comments
	// too, and failed on the line saying "never CRYPTPROTECT_LOCAL_MACHINE")
	Check(CodeOnly(store).find("CRYPTPROTECT_LOCAL_MACHINE") == std::string::npos && CodeOnly(store).size() > 3000,
		"it never seals for the whole machine: only this Windows user can open it");
	Check(store.find("CryptProtectData(&plain, NULL, &entropy, NULL, NULL, CRYPTPROTECT_UI_FORBIDDEN, &sealed)") != std::string::npos &&
		store.find("CryptUnprotectData(&sealed, NULL, &entropy, NULL, NULL, CRYPTPROTECT_UI_FORBIDDEN, &plain)") != std::string::npos,
		"both ways go with the game's own words, and Windows is never let ask the player anything");
	Check(store.find("const char kSealWords[] = \"AVPX identity v2\";") != std::string::npos, "the game's own words are CRYPTO.md section 4.1's");

	std::string const unseal = SourceSlice(store, "if (plain.pbData)\n\t{", "\n\t}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(unseal.find("SecureZeroMemory(plain.pbData, plain.cbData);") != std::string::npos && unseal.find("SecureZeroMemory") < unseal.find("LocalFree"),
		"the unsealed private halves are wiped before Windows gets their memory back");
	Check(store.find("MoveFileExW(fresh.c_str(), path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)") != std::string::npos,
		"a new file is put in place in one step, never written over the old one");

	// Primary's read, finding 1: a power cut can't be tested, so the source is read for it. The new file's data is flushed to the disk
	// itself, in the writer the keeping calls, before the keeping puts it in place.
	std::string const writer = SourceSlice(store, "bool WriteWholeFile(const wchar_t *path, const uint8_t *bytes, size_t size)", "\n\t}\n", kSliceEndAfterTheStart, FailedSlice);
	std::string const keeping = SourceSlice(store, "bool NetIdentityStore_KeepTo(const wchar_t *path, const NetIdentity *id)", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	size_t const written = keeping.find("WriteWholeFile(fresh.c_str(), file, size) &&");
	Check(writer.find("ok = ok && FlushFileBuffers(file) != FALSE;") != std::string::npos && writer.find("FlushFileBuffers") < writer.find("CloseHandle(file)") &&
		written != std::string::npos && written < keeping.find("MoveFileExW("),
		"the new file is made sure of on the disk itself, not only in Windows' cache, before it's put in place");

	// Finding 2: never in Documents, which can roam to the player's other PCs, where a DPAPI seal won't open
	Check(store.find("SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &local)") != std::string::npos &&
		store.find("GetSaveFolderPath") == std::string::npos,
		"the file is kept in this PC's own LocalAppData, never the save folder");
	Check(store.find("return folder + L\"\\\\\" + NetIdentityStore_FileName(SaveFolder_IsAHudsonRun(), SaveFolder_IsTheSecondCopy());") != std::string::npos,
		"and its name says whether this is a Hudson run, and whether it's the second copy (TestTheFileNames)");

	// Finding 3: the owner is checked through the same handle the file is read through
	std::string const reader = SourceSlice(store, "bool ReadWholeFile(const wchar_t *path,", "\n\t}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(reader.find("if (!OwnedByThisUser(file))") != std::string::npos && reader.find("OwnedByThisUser") < reader.find("ReadFile(file,") &&
		store.find("GetSecurityInfo(file, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION,") != std::string::npos,
		"the file's owner is checked on the handle it's read through, before a byte of it is read");

	// Primary's read of caddf2d1: a link is opened as itself, never followed, and refused as a link before its owner or a byte is looked at
	size_t const linkCheck = reader.find("if ((about.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)");
	Check(reader.find("FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, NULL);") != std::string::npos && linkCheck != std::string::npos &&
		linkCheck < reader.find("if (!OwnedByThisUser(file))") && linkCheck < reader.find("ReadFile(file,"),
		"a link is opened as itself and refused as a link, before anything else is looked at");
}

// This Windows user's SID, from the test's own token
static std::vector<BYTE> ThisUserSid(void)
{
	std::vector<BYTE> sid;
	HANDLE token = NULL;

	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
	{
		return sid;
	}

	std::vector<BYTE> answer(sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE + 16);
	DWORD got = 0;

	if (GetTokenInformation(token, TokenUser, answer.data(), static_cast<DWORD>(answer.size()), &got))
	{
		PSID const user = reinterpret_cast<TOKEN_USER *>(answer.data())->User.Sid;
		sid.assign(static_cast<BYTE *>(user), static_cast<BYTE *>(user) + GetLengthSid(user));
	}

	CloseHandle(token);
	return sid;
}

static void TestWhoseFile()
{
	puts("Whose the file is (Primary's read, finding 3)");

	NetIdentity made;
	NetIdentity_Make(&made);
	DeleteFileW(kKept);
	NetIdentityStore_KeepTo(kKept, &made);

	std::vector<BYTE> user = ThisUserSid();
	PSID owner = NULL;
	PACL list = NULL;
	PSECURITY_DESCRIPTOR descriptor = NULL;
	bool const asked = !user.empty() && GetNamedSecurityInfoW(kKept, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, &owner,
		NULL, &list, NULL, &descriptor) == ERROR_SUCCESS;
	Check(asked && owner && EqualSid(owner, user.data()), "a kept file is owned by this Windows user");

	SECURITY_DESCRIPTOR_CONTROL control = 0;
	DWORD revision = 0;
	bool onlyThisUser = false;

	if (asked && list && list->AceCount == 1 && GetSecurityDescriptorControl(descriptor, &control, &revision))
	{
		void *entry = NULL;

		if (GetAce(list, 0, &entry))
		{
			ACCESS_ALLOWED_ACE const *const allowed = static_cast<ACCESS_ALLOWED_ACE *>(entry);
			onlyThisUser = allowed->Header.AceType == ACCESS_ALLOWED_ACE_TYPE && EqualSid(const_cast<DWORD *>(&allowed->SidStart), user.data());
		}
	}

	Check(onlyThisUser && (control & SE_DACL_PROTECTED) != 0, "and only this user may read or change it, with nothing added from the folder it's in");
	LocalFree(descriptor);

	// Added 2026-11-10 by Melanikus for the AvP port (Take-outs, #191): Windows protects a new file's list of who may use it when the file is
	// made with a list of its own, whether the game asks for that or not, so the check above can't tell that it asked. It must still ask:
	// a file made some other way later (written over in place, its list set after) would take the folder's list. So it's read as written.
	std::string const writer = SourceSlice(ReadSource("3dc/net_identity_store.cpp"), "bool WriteWholeFile(const wchar_t *path,", "\n\t}\n",
		kSliceEndAfterTheStart, FailedSlice);
	Check(writer.find("!SetSecurityDescriptorControl(&descriptor, SE_DACL_PROTECTED, SE_DACL_PROTECTED))") != std::string::npos,
		"SOURCE: the file is made with its list marked protected, so nothing from the folder is ever added to it");

	// Given to the Administrators group, as a file someone else had put there would be: refused, though its keys hold together
	SID_IDENTIFIER_AUTHORITY nt = SECURITY_NT_AUTHORITY;
	PSID administrators = NULL;
	bool const madeSid = AllocateAndInitializeSid(&nt, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &administrators) != FALSE;
	bool const given = madeSid && SetNamedSecurityInfoW(const_cast<wchar_t *>(kKept), SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION, administrators, NULL,
		NULL, NULL) == ERROR_SUCCESS;
	Check(given, "the test could give the file to the Administrators group (GitHub's runner is an administrator, so it can)");

	NetIdentity read;
	Dirty(&read);
	const char *why = NULL;
	Check(given && NetIdentityStore_LoadFrom(kKept, &read, &why) == kNetIdentityStoreRefused && NothingLeft(read) && why &&
		strstr(why, "isn't owned by this Windows user") != NULL, "a file owned by anyone else is refused, even with keys that hold together");

	if (administrators)
	{
		FreeSid(administrators);
	}

	// Back to this user, so the file can be deleted at the end like the others
	if (!user.empty())
	{
		SetNamedSecurityInfoW(const_cast<wchar_t *>(kKept), SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION, user.data(), NULL, NULL, NULL);
	}

	// A link at the file's place, pointing at a good file of this user's: refused as a link, never followed (Primary's read of caddf2d1).
	// Making a link needs an administrator, or Windows' developer mode. GitHub's runner is an administrator, so it can.
	DeleteFileW(kLink);
	bool const linked = CreateSymbolicLinkW(kLink, kKept, kLinkUnprivileged) != FALSE;
	Check(linked, "the test could make a link to the kept file (GitHub's runner is an administrator, so it can)");
	Dirty(&read);
	why = NULL;
	Check(linked && NetIdentityStore_LoadFrom(kLink, &read, &why) == kNetIdentityStoreRefused && NothingLeft(read) && why &&
		strstr(why, "is a link") != NULL, "a link at the file's place is refused as a link, even one to a good file of this user's");
	Check(NetIdentityStore_LoadFrom(kKept, &read, NULL) == kNetIdentityStoreLoaded, "while the file it points at is read as before");
	NetIdentity_Free(&read);
	DeleteFileW(kLink);

	NetIdentity_Free(&made);
}

// The kept file's name, and the game's own answers to what it's named by (added 2026-11-09 by Melanikus for the AvP port). A Hudson run
// has files of its own, so the test marine never reads, replaces or sends the player's identity and its approvals (Fifth's read). And the
// two functions the store asks are really in the game: the first had no definition there at all, only this test's stand-in, so the game
// couldn't link, and nothing else would have said so.
static void TestTheFileNames()
{
	puts("The kept file's name");

	Check(wcscmp(NetIdentityStore_FileName(false, false), L"identity-v2.bin") == 0 &&
		wcscmp(NetIdentityStore_FileName(false, true), L"identity-v2-second-copy.bin") == 0, "the player's own file, and the second copy's");
	Check(wcscmp(NetIdentityStore_FileName(true, false), L"identity-v2-hudson.bin") == 0 &&
		wcscmp(NetIdentityStore_FileName(true, true), L"identity-v2-hudson-second-copy.bin") == 0, "a Hudson run's own two");

	bool allApart = true;
	const wchar_t *const names[] = {NetIdentityStore_FileName(false, false), NetIdentityStore_FileName(false, true),
		NetIdentityStore_FileName(true, false), NetIdentityStore_FileName(true, true)};

	for (size_t a = 0; a < 4; a++)
	{
		for (size_t b = a + 1; b < 4; b++)
		{
			allApart = allApart && _wcsicmp(names[a], names[b]) != 0;
		}
	}

	Check(allApart, "no two of them are the same file, whatever the letters' case, so a Hudson run never opens the player's");

	std::string const utilities = CodeOnly(ReadSource("3dc/utilities.cpp"));
	Check(utilities.find("bool SaveFolder_IsTheSecondCopy()\n{\n\treturn saveFolderIsTheSecondCopy;\n}") != std::string::npos,
		"the game itself says whether it's the second copy (utilities.cpp), so the store links");
	Check(utilities.find("bool SaveFolder_IsAHudsonRun()\n{\n\treturn HudsonRunIsOn;\n}") != std::string::npos,
		"and whether this is a Hudson run");
}

int main()
{
	TestKeepAndRead();
	TestTheSealing();
	TestFilesThatCantBeUsed();
	TestHalvesFromAnother();
	TestReplacing();
	TestWhoseFile();
	TestTheSource();
	TestTheFileNames();

	DeleteFileW(kKept);
	DeleteFileW(kMissing);
	DeleteFileW((std::wstring(kFolder) + L".new").c_str());
	RemoveDirectoryW(kFolder);
	DeleteFileW(kLink);

	if (checksFailed)
	{
		printf("%d checks FAILED\n", checksFailed);
		return 1;
	}

	printf("All checks passed\n");
	return 0;
}
