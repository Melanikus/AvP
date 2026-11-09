// The relay server's list, from the game's side. relay_list.h says what it's for. Written 2026-11-08 by Melanikus for the AvP port.
//
// HOW IT WORKS
//
// The game's main thread says what it wants: this host's game on the list, or off it, or the list read. A thread of this file's own does
// the HTTPS and puts the answer where the main thread picks it up. The two share one small store, Shared below, and touch it only while
// holding its lock, and only to copy things in and out. Nothing waits on the network while holding it.
//
// The game's log isn't written from two threads at once, so the list's thread never writes to it. It leaves its news in the store, and
// the main thread writes it, the next time it calls in here.
//
// HTTPS: WinHTTP, Windows' own. TLS 1.2 or newer. Windows checks the server's certificate, and a certificate it doesn't accept ends the
// call: nothing here can turn that check off. Redirects, cookies and sign-ins are all turned off, since the list never needs them, and a
// redirect could send the game somewhere else. Each step has 5 seconds.

#include "3dc.h"
#include "enet\enet.h"
#include "configFile.h"
#include "logString.h"
#include "networking.h"
#include "coop_lobby.h"
#include "relay_list.h"

#include <winhttp.h>
#include <string.h>
#include <wchar.h>
#include <new>

// WinHTTP's library, which comes with Windows
#pragma comment(lib, "winhttp.lib")

// RelayList_ModeWords has the game styles in the order of NETGAME_TYPE. If that order ever changes, this stops the build.
static_assert(NGT_Individual == 0 && NGT_CoopDeathmatch == 1 && NGT_LastManStanding == 2 && NGT_PredatorTag == 3 && NGT_Coop == 4 &&
	NGT_AlienTag == 5, "RelayList_ModeWords in relay_list_text.cpp has the game styles in NETGAME_TYPE's order, and that has changed");

namespace
{
	// What the list shows of a host's game
	struct Listing
	{
		char name[kRelayListTextLetters + 1];
		char level[kRelayListTextLetters + 1];
		char mode[kRelayListTextLetters + 1];
		int players;
		int maxPlayers;
		int version;
	};

	bool SameListing(const Listing &a, const Listing &b)
	{
		return strcmp(a.name, b.name) == 0 && strcmp(a.level, b.level) == 0 && strcmp(a.mode, b.mode) == 0 && a.players == b.players &&
			a.maxPlayers == b.maxPlayers && a.version == b.version;
	}

	// The game's session, as the list shows it
	Listing ListingOf(const SessionDescription &session)
	{
		Listing l;
		memset(&l, 0, sizeof(l));
		RelayList_FitText(session.sessionName, "AVPx game", l.name);
		RelayList_FitText(session.levelName, "A level", l.level);
		RelayList_FitText(RelayList_ModeWords(session.gameStyle, session.coopLobby != kCoopLobbyNone), "Game", l.mode);

		l.maxPlayers = session.maxPlayers < 1 ? 1 : (session.maxPlayers > kRelayMostPlayers ? kRelayMostPlayers : session.maxPlayers);
		l.players = session.nPlayers < 1 ? 1 : (session.nPlayers > l.maxPlayers ? l.maxPlayers : session.nPlayers);
		l.version = static_cast<int>(session.version);
		return l;
	}

	// Words for the log and the page. Fixed words of the game's own: what a server says is never shown or written down.
	const char *const kNoServer = "no relay server is set";
	const char *const kCantReach = "the relay server couldn't be reached";
	const char *const kCertificate = "the relay server's certificate wasn't accepted";
	const char *const kTooLong = "the relay server took too long to answer";
	const char *const kTooBig = "the relay server's answer was too big";
	const char *const kSaidNo = "the relay server said no";
	const char *const kCantStart = "the game couldn't start a call to the relay server";

	// Everything the two threads share. Made once and never freed: the list's thread may still be using it as the game closes.
	struct Shared
	{
		CRITICAL_SECTION lock;
		HANDLE wake;                     // set when the main thread wants something done

		wchar_t server[256];             // the server's name, set before the thread starts, and never changed

		// The host's game. listingNumber goes up each time what the list should show changes.
		bool hostWanted;
		uint8_t token[kRelayTokenBytes];
		Listing listing;
		int listingNumber;

		// A room that has ended, whose game should come off the list
		bool endWanted;
		uint8_t endToken[kRelayTokenBytes];

		// Finding games: a call wanted, and an answer waiting for the main thread
		bool findWanted;
		bool answerReady;
		RelayListGame games[kRelayListMostGames];
		int count;
		const char *why;

		// News for the main thread to write in the log, and its number, which goes up with each new piece
		const char *news;
		int newsNumber;
	};

	Shared *shared = NULL;
	int newsWritten = 0;   // the main thread's: the news it has written

	// ---- The list's thread

	void Tell(const char *news)
	{
		EnterCriticalSection(&shared->lock);
		shared->news = news;
		shared->newsNumber++;
		LeaveCriticalSection(&shared->lock);
	}

	// What a call gave back
	struct Answer
	{
		bool worked;          // it reached the server and got a status
		DWORD status;
		const char *why;      // when it didn't work
		size_t bytes;
	};

	void CloseAll(HINTERNET request, HINTERNET connection, HINTERNET session)
	{
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

	const char *WhyItFailed(DWORD error)
	{
		if (error == ERROR_WINHTTP_SECURE_FAILURE)
		{
			return kCertificate;
		}

		if (error == ERROR_WINHTTP_TIMEOUT)
		{
			return kTooLong;
		}

		return kCantReach;
	}

	// One HTTPS call to the list. body is sent if there is one. What comes back goes into answer, which has room for room bytes: an
	// answer longer than that is refused as too big, without the rest being read.
	Answer Call(const wchar_t *method, const wchar_t *path, const char *body, char *answer, size_t room)
	{
		Answer a = { false, 0, kCantStart, 0 };

		HINTERNET session = WinHttpOpen(L"AVPx", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);

		if (!session)
		{
			return a;
		}

		// Finding the server, connecting, sending and each wait for the answer get 5 seconds each
		WinHttpSetTimeouts(session, 5000, 5000, 5000, 5000);

		// TLS 1.2 or newer, never older. An older Windows that doesn't know 1.3's flag gets 1.2 alone.
		DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
#ifdef WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3
		DWORD withThree = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;

		if (!WinHttpSetOption(session, WINHTTP_OPTION_SECURE_PROTOCOLS, &withThree, sizeof(withThree)))
#endif
		{
			if (!WinHttpSetOption(session, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols)))
			{
				CloseAll(NULL, NULL, session);
				return a;
			}
		}

		HINTERNET connection = WinHttpConnect(session, shared->server, kRelayListPort, 0);
		HINTERNET request = connection ? WinHttpOpenRequest(connection, method, path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
			WINHTTP_FLAG_SECURE) : NULL;

		if (!request)
		{
			CloseAll(request, connection, session);
			return a;
		}

		// No redirects, no cookies, and no signing in: the list needs none of them, and never sends the game anywhere else
		DWORD off = WINHTTP_DISABLE_COOKIES | WINHTTP_DISABLE_REDIRECTS | WINHTTP_DISABLE_AUTHENTICATION;
		DWORD neverSignIn = WINHTTP_AUTOLOGON_SECURITY_LEVEL_HIGH;

		if (!WinHttpSetOption(request, WINHTTP_OPTION_DISABLE_FEATURE, &off, sizeof(off)) ||
			!WinHttpSetOption(request, WINHTTP_OPTION_AUTOLOGON_POLICY, &neverSignIn, sizeof(neverSignIn)))
		{
			CloseAll(request, connection, session);
			return a;
		}

		DWORD const bodyBytes = body ? static_cast<DWORD>(strlen(body)) : 0;
		BOOL const sent = body
			? WinHttpSendRequest(request, L"Content-Type: application/json\r\n", static_cast<DWORD>(-1), const_cast<char *>(body), bodyBytes, bodyBytes, 0)
			: WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);

		if (!sent || !WinHttpReceiveResponse(request, NULL))
		{
			a.why = WhyItFailed(GetLastError());
			CloseAll(request, connection, session);
			return a;
		}

		DWORD status = 0;
		DWORD statusSize = sizeof(status);

		if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize,
			WINHTTP_NO_HEADER_INDEX))
		{
			a.why = kCantReach;
			CloseAll(request, connection, session);
			return a;
		}

		// The answer, up to room bytes. One byte more than room means it's too big, and the rest isn't read.
		size_t got = 0;

		for (;;)
		{
			DWORD available = 0;

			if (!WinHttpQueryDataAvailable(request, &available))
			{
				a.why = WhyItFailed(GetLastError());
				CloseAll(request, connection, session);
				return a;
			}

			if (available == 0)
			{
				break;
			}

			if (got >= room)
			{
				a.why = kTooBig;
				CloseAll(request, connection, session);
				return a;
			}

			DWORD const wanted = (available < room - got) ? available : static_cast<DWORD>(room - got);
			DWORD read = 0;

			if (!WinHttpReadData(request, answer + got, wanted, &read))
			{
				a.why = WhyItFailed(GetLastError());
				CloseAll(request, connection, session);
				return a;
			}

			if (read == 0)
			{
				break;
			}

			got += read;
		}

		CloseAll(request, connection, session);
		a.worked = true;
		a.status = status;
		a.why = NULL;
		a.bytes = got;
		return a;
	}

	// The answers to a host's calls aren't used, beyond their status. This is where they go.
	char hostAnswer[kRelayListMostBodyBytes];

	// Puts the host's game on the list, or changes what it shows. listed says whether it's on it already, and is changed to say so after.
	// True if the list shows the listing now.
	bool PutOnTheList(const uint8_t token[kRelayTokenBytes], const Listing &l, bool *listed)
	{
		char body[kRelayListMostBodyBytes + 1];

		if (!RelayList_ListingBody(token, l.name, l.level, l.mode, l.players, l.maxPlayers, l.version, body, sizeof(body)))
		{
			Tell("Relay list: the game's own words couldn't be made into a listing, so it isn't on the list");
			return false;
		}

		// A new game is POSTed and a listed one PUT. If the list says otherwise (409: it's listed already, 404: it isn't), the other is
		// tried, once.
		for (int tries = 0; tries < 2; tries++)
		{
			Answer const a = Call(*listed ? L"PUT" : L"POST", L"/v1/games", body, hostAnswer, sizeof(hostAnswer));

			if (!a.worked)
			{
				Tell(a.why);
				return false;
			}

			if ((!*listed && a.status == 201) || (*listed && a.status == 200))
			{
				*listed = true;
				return true;
			}

			if ((!*listed && a.status == 409) || (*listed && a.status == 404))
			{
				*listed = !*listed;
				continue;
			}

			break;
		}

		Tell(kSaidNo);
		return false;
	}

	void TakeOffTheList(const uint8_t token[kRelayTokenBytes])
	{
		char body[kRelayListMostBodyBytes + 1];

		if (!RelayList_EndingBody(token, body, sizeof(body)))
		{
			return;
		}

		// 204: taken off. 403: the server knows no room with that token, which is what's wanted too. A room that ends takes its game off
		// the list anyway, so a call that fails here leaves nothing behind for long.
		Answer const a = Call(L"DELETE", L"/v1/games", body, hostAnswer, sizeof(hostAnswer));

		if (!a.worked)
		{
			Tell(a.why);
		}
		else if (a.status == 204 || a.status == 403)
		{
			Tell("Relay list: the game is off the list");
		}
		else
		{
			Tell(kSaidNo);
		}
	}

	// The answer to a GET: read as hostile (relay_list_text.h), and only kept if all of it is right
	char findAnswer[kRelayListMostAnswerBytes];
	RelayListGame foundGames[kRelayListMostGames];

	void ReadTheList(void)
	{
		wchar_t path[64];
		swprintf(path, sizeof(path) / sizeof(path[0]), L"/v1/games?version=%d", kMultiplayerVersion);

		Answer const a = Call(L"GET", path, NULL, findAnswer, sizeof(findAnswer));
		int count = 0;
		const char *why = a.why;

		if (a.worked && a.status != 200)
		{
			why = kSaidNo;
		}
		else if (a.worked)
		{
			RelayListRefusal const refusal = RelayList_ReadGames(findAnswer, a.bytes, kMultiplayerVersion, foundGames, &count);
			why = (refusal == kRelayListAccepted) ? NULL : RelayList_RefusalWords(refusal);
		}

		EnterCriticalSection(&shared->lock);
		memcpy(shared->games, foundGames, sizeof(RelayListGame) * static_cast<size_t>(why ? 0 : count));
		shared->count = why ? 0 : count;
		shared->why = why;
		shared->answerReady = true;
		LeaveCriticalSection(&shared->lock);
	}

	DWORD WINAPI ListThread(void *)
	{
		// What this thread has put on the list: whether the game is on it, for which room, and which listing it shows
		bool listed = false;
		uint8_t listedToken[kRelayTokenBytes] = {};
		int listedNumber = -1;
		bool tryAgainLater = false;

		for (;;)
		{
			// A listing that didn't get through is tried again every 10 seconds, for as long as the room is open
			WaitForSingleObject(shared->wake, tryAgainLater ? 10000 : INFINITE);

			EnterCriticalSection(&shared->lock);
			bool const end = shared->endWanted;
			uint8_t endToken[kRelayTokenBytes];
			memcpy(endToken, shared->endToken, sizeof(endToken));
			shared->endWanted = false;

			bool const host = shared->hostWanted;
			uint8_t token[kRelayTokenBytes];
			memcpy(token, shared->token, sizeof(token));
			Listing const listing = shared->listing;
			int const number = shared->listingNumber;

			bool const find = shared->findWanted;
			shared->findWanted = false;
			LeaveCriticalSection(&shared->lock);

			if (end)
			{
				TakeOffTheList(endToken);

				if (memcmp(endToken, listedToken, sizeof(endToken)) == 0)
				{
					listed = false;
				}
			}

			tryAgainLater = false;

			if (host)
			{
				// Another room is another game, not yet on the list
				if (memcmp(token, listedToken, sizeof(token)) != 0)
				{
					listed = false;
					memcpy(listedToken, token, sizeof(token));
					listedNumber = -1;
				}

				if (number != listedNumber)
				{
					bool const wasListed = listed;

					if (PutOnTheList(token, listing, &listed))
					{
						listedNumber = number;

						if (!wasListed)
						{
							Tell("Relay list: the game is on the list");
						}
					}
					else
					{
						tryAgainLater = true;
					}
				}
			}

			if (find)
			{
				ReadTheList();
			}
		}
	}

	// Makes the store and starts the thread, the first time either is needed. False if there's no relay server, or it can't start.
	bool Start(void)
	{
		if (shared)
		{
			return true;
		}

		const char *const name = Relay_ServerName();

		if (!name[0])
		{
			return false;
		}

		Shared *made = new (std::nothrow) Shared;

		if (!made)
		{
			return false;
		}

		memset(made, 0, sizeof(*made));
		InitializeCriticalSection(&made->lock);
		made->wake = CreateEventW(NULL, FALSE, FALSE, NULL);

		// The name is letters, digits, - and . only (Relay_IsServerName), so each one is the same as a wide character
		for (size_t i = 0; name[i] && i + 1 < sizeof(made->server) / sizeof(made->server[0]); i++)
		{
			made->server[i] = static_cast<wchar_t>(name[i]);
		}

		if (!made->wake)
		{
			LogString("Relay list: couldn't start, so hosts aren't listed and the list can't be read");
			return false;
		}

		shared = made;
		HANDLE const thread = CreateThread(NULL, 0, ListThread, NULL, 0, NULL);

		if (!thread)
		{
			LogString("Relay list: couldn't start, so hosts aren't listed and the list can't be read");
			shared = NULL;
			return false;
		}

		CloseHandle(thread);
		return true;
	}

	// The main thread writes the list's news in the log
	void WriteTheNews(void)
	{
		if (!shared)
		{
			return;
		}

		EnterCriticalSection(&shared->lock);
		int const number = shared->newsNumber;
		const char *const news = shared->news;
		LeaveCriticalSection(&shared->lock);

		if (number != newsWritten && news)
		{
			newsWritten = number;
			LogString(news);
		}
	}

	// The page asked with no relay server set: its answer, which needs no thread
	bool noServerAnswerWaiting = false;
}

const char *Relay_ServerName(void)
{
	static char name[254];
	static bool read = false;

	if (!read)
	{
		read = true;
		std::string const setting = Config_GetString("[Network]", "RelayServer", "");

		if (setting.size() < sizeof(name) && Relay_IsServerName(setting.c_str()))
		{
			memcpy(name, setting.c_str(), setting.size() + 1);
		}
		else if (!setting.empty())
		{
			LogString("Relay list: the settings' RelayServer isn't a server's name, so there's no relay");
		}
	}

	return name;
}

void RelayList_HostRoomOpened(const uint8_t token[kRelayTokenBytes], const char code[kRelayCodeLetters], const SessionDescription &session)
{
	(void)code;   // the list gives a game its code from the token: it's here for networking.cpp to say which room this is

	if (!token || !Start())
	{
		return;
	}

	Listing const l = ListingOf(session);

	EnterCriticalSection(&shared->lock);
	shared->hostWanted = true;
	memcpy(shared->token, token, sizeof(shared->token));
	shared->listing = l;
	shared->listingNumber++;
	LeaveCriticalSection(&shared->lock);

	SetEvent(shared->wake);
	WriteTheNews();
}

void RelayList_HostSessionNow(const SessionDescription &session)
{
	if (!shared)
	{
		return;
	}

	Listing const l = ListingOf(session);
	bool changed = false;

	EnterCriticalSection(&shared->lock);

	if (shared->hostWanted && !SameListing(l, shared->listing))
	{
		shared->listing = l;
		shared->listingNumber++;
		changed = true;
	}

	LeaveCriticalSection(&shared->lock);

	if (changed)
	{
		SetEvent(shared->wake);
	}

	WriteTheNews();
}

void RelayList_HostRoomClosed(void)
{
	if (!shared)
	{
		return;
	}

	bool ended = false;

	EnterCriticalSection(&shared->lock);

	if (shared->hostWanted)
	{
		shared->hostWanted = false;
		shared->endWanted = true;
		memcpy(shared->endToken, shared->token, sizeof(shared->endToken));
		ended = true;
	}

	LeaveCriticalSection(&shared->lock);

	if (ended)
	{
		SetEvent(shared->wake);
	}

	WriteTheNews();
}

void RelayList_FindGames(void)
{
	if (!Start())
	{
		noServerAnswerWaiting = true;
		return;
	}

	EnterCriticalSection(&shared->lock);
	shared->findWanted = true;
	LeaveCriticalSection(&shared->lock);

	SetEvent(shared->wake);
}

bool RelayList_FoundGames(RelayListGame *games, int *count, const char **why)
{
	*count = 0;
	*why = NULL;

	if (noServerAnswerWaiting)
	{
		noServerAnswerWaiting = false;
		*why = kNoServer;
		return true;
	}

	if (!shared)
	{
		return false;
	}

	WriteTheNews();

	EnterCriticalSection(&shared->lock);
	bool const ready = shared->answerReady;

	if (ready)
	{
		shared->answerReady = false;
		*count = shared->count;
		*why = shared->why;
		memcpy(games, shared->games, sizeof(RelayListGame) * static_cast<size_t>(shared->count));
	}

	LeaveCriticalSection(&shared->lock);
	return ready;
}
