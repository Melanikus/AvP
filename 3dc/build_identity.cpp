// Which build of the game is running, for the top of the log. Written 2026-11-07 by Melanikus for the AvP port. build_identity.h says why.
#include "build_identity.h"
#include "beta_build.h"
#include "md5.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>

// tools/build-and-package.ps1 writes this file before it builds the game, for every pull request's build, test build and release. It isn't
// in the repository. A build made any other way, such as in Visual Studio on someone's own machine, doesn't have it, and says so.
#if __has_include("build_version.h")
#include "build_version.h"
#endif

#ifndef AVP_RELEASE_VERSION
#define AVP_RELEASE_VERSION "not known (not built by tools/build-and-package.ps1)"
#endif

#ifndef AVP_RELEASE_COMMIT
#define AVP_RELEASE_COMMIT "not known"
#endif

// The program file's size and MD5, read in pieces. MD5 is only a fingerprint here, to tell builds apart, not a security check. Only the
// numbers go in the log: the program's folder holds the player's Windows user name, so it's never written.
static std::string Fingerprint()
{
	wchar_t path[MAX_PATH];
	DWORD const length = ::GetModuleFileNameW(NULL, path, MAX_PATH);

	if (length == 0 || length >= MAX_PATH)
	{
		return "build: this program's file couldn't be found, so it has no fingerprint";
	}

	HANDLE const file = ::CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);

	if (file == INVALID_HANDLE_VALUE)
	{
		return "build: this program's file couldn't be read, so it has no fingerprint";
	}

	LARGE_INTEGER started, finished, ticksPerSecond;
	::QueryPerformanceFrequency(&ticksPerSecond);
	::QueryPerformanceCounter(&started);

	struct MD5Context md5;
	MD5Init(&md5);

	static unsigned char piece[65536];
	unsigned long long size = 0;
	DWORD read = 0;
	bool readAll = true;

	for (;;)
	{
		if (!::ReadFile(file, piece, sizeof(piece), &read, NULL))
		{
			readAll = false;
			break;
		}

		if (read == 0)
		{
			break;
		}

		MD5Update(&md5, piece, read);
		size += read;
	}

	::CloseHandle(file);

	unsigned char digest[16];
	MD5Final(digest, &md5);
	::QueryPerformanceCounter(&finished);

	double const milliseconds = (double)(finished.QuadPart - started.QuadPart) * 1000.0 / (double)ticksPerSecond.QuadPart;
	char line[200];

	if (!readAll)
	{
		_snprintf(line, sizeof(line) - 1, "build: this program's file couldn't all be read, after %llu bytes", size);
	}
	else
	{
		char hex[33];

		for (int i = 0; i < 16; i++)
		{
			_snprintf(hex + i * 2, 3, "%02x", digest[i]);
		}

		hex[32] = '\0';
		_snprintf(line, sizeof(line) - 1, "build: this program is %llu bytes, MD5 %s (worked out in %.1f ms)", size, hex, milliseconds);
	}

	line[sizeof(line) - 1] = '\0';
	return line;
}

// Which Windows this is, from Windows itself. GetVersionEx can't be used: it gives the version the program was marked for, not the one it's
// running on. It's written as "10.0, build 26200", never as four numbers with dots between, which the log would take for a network
// address and leave out (log_files.h).
std::string BuildIdentity_Windows()
{
	typedef LONG(WINAPI *GetVersionFunction)(OSVERSIONINFOW *);

	HMODULE const ntdll = ::GetModuleHandleW(L"ntdll.dll");
	GetVersionFunction const getVersion = ntdll ? (GetVersionFunction)::GetProcAddress(ntdll, "RtlGetVersion") : NULL;

	OSVERSIONINFOW version;
	memset(&version, 0, sizeof(version));
	version.dwOSVersionInfoSize = sizeof(version);

	char line[100];

	if (!getVersion || getVersion(&version) != 0)
	{
		_snprintf(line, sizeof(line) - 1, "build: running on a Windows whose version couldn't be found");
	}
	else
	{
		_snprintf(line, sizeof(line) - 1, "build: running on Windows %lu.%lu, build %lu", version.dwMajorVersion, version.dwMinorVersion, version.dwBuildNumber);
	}

	line[sizeof(line) - 1] = '\0';
	return line;
}

std::string BuildIdentity_Lines()
{
	return std::string("build: version ") + AVP_RELEASE_VERSION + ", commit " + AVP_RELEASE_COMMIT + "\n" + Fingerprint() + "\n" + BuildIdentity_Windows();
}

bool BuildIdentity_IsABeta()
{
	return BetaBuild_VersionIsABeta(AVP_RELEASE_VERSION);
}
