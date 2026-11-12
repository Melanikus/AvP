// The test of what Windows lets the relay list's calls turn off (F1, PO's hostile read of rc1's client, 2026-11-10). Written 2026-11-10 by
// Melanikus for the AvP port.
//
// WHY. Before our own check of the server's key, Windows builds the server certificate's chain, and to build it, it may fetch addresses
// the certificate itself names (its AIA: where to get the issuer's certificate), even addresses on the player's own network. A hostile
// server, or anyone in between, could so make the player's PC send something where its servers.cfg line didn't say. The key alone is the
// trust (relay_list.cpp, TRUSTED BY ITS KEY ALONE), so no chain is needed: relay_list.cpp turns the building off on each call, or, on a
// Windows without that, has the chain built only from what Windows already holds. With neither, the call isn't made.
//
// WHAT IT TESTS
//
// - That this Windows takes at least one of the two on a request made the way relay_list.cpp makes it (WinHttpOpen, WinHttpConnect, and
//   WinHttpOpenRequest with WINHTTP_FLAG_SECURE), and says which. Making those sends nothing: nothing is connected or sent until
//   WinHttpSendRequest, which this test never calls. The address is 127.0.0.1's discard port, so even a mistake would stay on this PC.
// - By reading the source: relay_list.cpp sets them on the call's request, the second only when the first can't be, before anything is
//   sent, and gives up the call, with its own words, when neither can be set.
//
// WHAT IT CAN'T SHOW: that a fetch named by a certificate really doesn't happen. That needs a TLS server whose certificate names an
// address on this PC, and a count of what that address hears (Secondary's own listener, run by hand on a PC set up for it).
//
// HOW IT RUNS: tests/build.ps1 compiles this with tests/winhttp_chain_test.args. It gives back a non-zero result if any check failed.

#include <windows.h>
#include <winhttp.h>

#include <stdio.h>
#include <string.h>

#include <string>

#include "source_slice.h"

#pragma comment(lib, "winhttp.lib")

static int checksFailed = 0;

static void Check(bool passed, const char *what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what);

	if (!passed)
	{
		checksFailed++;
	}
}

static std::string ReadSource(const char *name)
{
	FILE *file = fopen(name, "rb");
	std::string text;

	if (!file)
	{
		return text;
	}

	char block[4096];
	size_t got = 0;

	while ((got = fread(block, 1, sizeof(block), file)) > 0)
	{
		for (size_t i = 0; i < got; i++)
		{
			if (block[i] != '\r')
			{
				text += block[i];
			}
		}
	}

	fclose(file);
	return text;
}

static void FailedSlice(const std::string &why)
{
	Check(false, why.c_str());
}

static bool Has(const std::string &text, const char *piece)
{
	return text.find(piece) != std::string::npos;
}

static void TestWhatWindowsTakes()
{
	puts("1. what this Windows lets a call turn off (nothing is sent)");

	HINTERNET session = WinHttpOpen(L"AVPx test", WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	HINTERNET connection = session ? WinHttpConnect(session, L"127.0.0.1", 9, 0) : NULL;
	HINTERNET request = connection ? WinHttpOpenRequest(connection, L"GET", L"/", NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
		WINHTTP_FLAG_SECURE) : NULL;
	Check(request != NULL, "a request is made the way relay_list.cpp makes one, with nothing sent");

	// Each of the two, on each kind of handle, as Windows answers (12018 is "not this kind of handle"). On a request Windows refuses
	// both, so relay_list.cpp sets them on the call's session, as soon as it's made (found here, 2026-11-11).
	BOOL yes = TRUE;
	HINTERNET const handles[] = { session, connection, request };
	char const *const handleNames[] = { "session", "connection", "request" };
	bool offOn[3] = {};
	bool cacheOnlyOn[3] = {};

	for (int h = 0; h < 3; h++)
	{
		offOn[h] = handles[h] && WinHttpSetOption(handles[h], WINHTTP_OPTION_DISABLE_CERT_CHAIN_BUILDING, &yes, sizeof(yes)) != FALSE;
		DWORD const offError = offOn[h] ? 0 : GetLastError();
		cacheOnlyOn[h] = handles[h] && WinHttpSetOption(handles[h], WINHTTP_OPTION_SERVER_CERT_CHAIN_BUILD_CACHE_ONLY, &yes, sizeof(yes)) != FALSE;
		DWORD const cacheError = cacheOnlyOn[h] ? 0 : GetLastError();
		printf("  (on a %s: chain building off %s (%lu), built from its cache alone %s (%lu))\n", handleNames[h], offOn[h] ? "taken" : "refused",
			offError, cacheOnlyOn[h] ? "taken" : "refused", cacheError);
	}

	// And on a session just opened, before any connection is made from it (12019 above is "not in this state")
	HINTERNET fresh = WinHttpOpen(L"AVPx test", WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	bool const freshOff = fresh && WinHttpSetOption(fresh, WINHTTP_OPTION_DISABLE_CERT_CHAIN_BUILDING, &yes, sizeof(yes)) != FALSE;
	DWORD const freshOffError = freshOff ? 0 : GetLastError();
	bool const freshCacheOnly = fresh && WinHttpSetOption(fresh, WINHTTP_OPTION_SERVER_CERT_CHAIN_BUILD_CACHE_ONLY, &yes, sizeof(yes)) != FALSE;
	DWORD const freshCacheError = freshCacheOnly ? 0 : GetLastError();
	printf("  (on a session just opened: chain building off %s (%lu), built from its cache alone %s (%lu))\n", freshOff ? "taken" : "refused",
		freshOffError, freshCacheOnly ? "taken" : "refused", freshCacheError);

	if (fresh)
	{
		WinHttpCloseHandle(fresh);
	}

	Check(freshOff || freshCacheOnly, "this Windows takes one of the two on a session just opened, where the game sets them, so it can make its calls here");

	if (request)
	{
		WinHttpCloseHandle(request);
	}

	if (connection)
	{
		WinHttpCloseHandle(connection);
	}

	if (session)
	{
		WinHttpCloseHandle(session);
	}
}

static void TestTheList()
{
	puts("2. where relay_list.cpp turns it off (by reading the source)");

	std::string const source = ReadSource("3dc/relay_list.cpp");
	Check(source.size() > 50000, "relay_list.cpp was found and read");

	std::string const call = SourceSlice(source, "Answer CallChecked(Shared *s, const wchar_t *method, const wchar_t *path, const char *body, char *answer, size_t room, KeyCheck &check)\n\t{",
		"\n\t}\n", kSliceEndAfterTheStart, FailedSlice);
	// On the call's session, before it's connected: the one place this Windows takes them (part 1). The list's thread writes nothing
	// to the log itself; the call's words go back to the main thread with its answer, as every other refusal's do.
	size_t const off = call.find("bool const noChain = WinHttpSetOption(session, WINHTTP_OPTION_DISABLE_CERT_CHAIN_BUILDING, &noFetch, sizeof(noFetch)) != FALSE ||\n"
		"\t\t\tWinHttpSetOption(session, WINHTTP_OPTION_SERVER_CERT_CHAIN_BUILD_CACHE_ONLY, &noFetch, sizeof(noFetch)) != FALSE;");
	size_t const refused = call.find("if (!noChain)\n\t\t{\n\t\t\tCloseAll(NULL, NULL, session);\n\t\t\ta.why = kNoChainOff;\n\t\t\treturn a;\n\t\t}");
	size_t const connected = call.find("WinHttpConnect(");
	size_t const sent = call.find("WinHttpSendRequest(");
	Check(off != std::string::npos && refused != std::string::npos && off < refused && refused < connected && connected < sent,
		"each call's session turns the chain off, or has it built from Windows' own cache, before it connects, and with neither, the call isn't made");
	Check(Has(call, "BOOL noFetch = TRUE;") && call.find("BOOL noFetch = TRUE;") < off, "both are asked for with TRUE");
	Check(Has(source, "return why == kCantReach || why == kTooLong || why == kCertificate || why == kCantStart || why == kNoChainOff;"),
		"and the games page says there's no connection, in the list's own words");
}

int main()
{
	TestWhatWindowsTakes();
	TestTheList();

	if (checksFailed)
	{
		printf("%d checks FAILED\n", checksFailed);
		return 1;
	}

	puts("All checks passed");
	return 0;
}
