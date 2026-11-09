// The relay server's list, from the game's side. relay_list.h says what it's for. Written 2026-11-08 by Melanikus for the AvP port.
//
// HOW IT WORKS
//
// The game's main thread says what it wants: this host's game on the list, or off it, or the relays or the games read. A thread of this
// file's own does the HTTPS, and puts each answer where the main thread picks it up. The two share one small store, Shared below, and
// touch it only while holding its lock, and only to copy things in and out. Nothing waits on the network while holding it.
//
// A host's game: the POST puts it on the list and gives back its owner, the proof that the listing is this host's. Only this file's thread
// ever holds the owner. Each newer pass the relay gives the host is sent in a PUT, which keeps the listing alive (the list forgets one
// after 45 seconds), and so is any change to what it shows. When the room ends, a DELETE takes it off.
//
// The game's log isn't written from two threads at once, so the list's thread never writes to it. It leaves its news in the store, and
// the main thread writes it, the next time it calls in here.
//
// The relays: their names are looked up on this file's thread too, as soon as the list names them, so the game's own thread never waits for
// the network's name service. An address that can't be a relay's (RelayList_AddressRefusal: this network, everyone at once, many machines
// at once, set aside, or, outside -dev, private or this machine) leaves its relay out.
//
// HTTPS: WinHTTP, Windows' own. TLS 1.2 or newer. Windows checks the list's certificate, and a certificate it doesn't accept ends the call:
// nothing here can turn that check off. Redirects, cookies and sign-ins are all turned off, since the list never needs them, and a redirect
// could send the game somewhere else. Each step has 5 seconds.

#include "3dc.h"
#include "enet\enet.h"
#include "configFile.h"
#include "logString.h"
#include "networking.h"
#include "coop_lobby.h"
#include "relay_list.h"

#include <ws2tcpip.h>
#include <winhttp.h>
#include <string.h>
#include <wchar.h>
#include <new>

// WinHTTP's library, which comes with Windows
#pragma comment(lib, "winhttp.lib")

// -dev, the developer tools (d3d_render.cpp, set from the command line before the menus start)
extern int DevMode;

// RelayList_ModeWords has the game styles in the order of NETGAME_TYPE. If that order ever changes, this stops the build.
static_assert(NGT_Individual == 0 && NGT_CoopDeathmatch == 1 && NGT_LastManStanding == 2 && NGT_PredatorTag == 3 && NGT_Coop == 4 &&
	NGT_AlienTag == 5, "RelayList_ModeWords in relay_list_text.cpp has the game styles in NETGAME_TYPE's order, and that has changed");

// A level's name on the list is at most what the game's own session holds
static_assert(kRelayListLevelLetters == kLevelNameSize - 1, "the list's level names are the game's own, kLevelNameSize - 1 long");

namespace
{
	// What the list shows of a host's game, with what proves the room is its own
	struct Listing
	{
		char region[kRelayListRegionLetters + 1];
		char code[kRelayCodeLetters + 1];
		uint8_t pass[kRelayListSecretBytes];
		uint64_t passTime;
		int hostMs;
		char name[kRelayListTextLetters + 1];
		char level[kRelayListLevelLetters + 1];
		char mode[kRelayListTextLetters + 1];
		int gameStyle;
		int levelNumber;
		int coopLobby;
		int players;
		int maxPlayers;
		int version;
		uint8_t fingerprint[kNetIdentityFingerprintBytes];   // this run's identity, as it was when the room opened
	};

	// Whether what the list shows of the game is the same: the pass and the round trip aren't shown, so they don't count here
	bool SameShown(const Listing &a, const Listing &b)
	{
		return strcmp(a.name, b.name) == 0 && strcmp(a.level, b.level) == 0 && strcmp(a.mode, b.mode) == 0 && a.gameStyle == b.gameStyle &&
			a.levelNumber == b.levelNumber && a.coopLobby == b.coopLobby && a.players == b.players && a.maxPlayers == b.maxPlayers &&
			a.version == b.version;
	}

	// The game's session, as the list shows it, into a listing that already has its room's region, code and pass
	void ShowSession(const SessionDescription &session, Listing &l)
	{
		RelayList_FitText(session.sessionName, "AVPx game", l.name, kRelayListTextLetters);
		RelayList_FitText(session.levelName, "A level", l.level, kRelayListLevelLetters);
		RelayList_FitText(RelayList_ModeWords(session.gameStyle, session.coopLobby != kCoopLobbyNone), "Game", l.mode, kRelayListTextLetters);

		l.gameStyle = session.gameStyle;
		l.levelNumber = session.level;
		l.coopLobby = session.coopLobby;
		l.maxPlayers = session.maxPlayers < 1 ? 1 : (session.maxPlayers > kRelayMostPlayers ? kRelayMostPlayers : session.maxPlayers);
		l.players = session.nPlayers < 1 ? 1 : (session.nPlayers > l.maxPlayers ? l.maxPlayers : session.nPlayers);
		l.version = static_cast<int>(session.version);
	}

	// The pieces of news from the list's thread the store keeps until the main thread writes them (Tell)
	const int kMostNews = 16;

	// The most letters a piece of news keeps. Each is one of the game's own fixed texts, and the longest is about 100 letters.
	const size_t kMostNewsLetters = 256;

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

		wchar_t server[256];             // the list's name and port, set before the thread starts, and never changed
		INTERNET_PORT port;
		bool developer;                  // -dev: a relay on this machine or this network may be used, for a test
		int version;                     // the version this copy speaks (Net_OurVersion), whose games it lists and asks for
		bool v2Draft;                    // the v2 draft is on (Net_V2DraftOn): only then do listings and games carry a fingerprint

		// The host's game. listingNumber goes up each time the listing changes, a newer pass included.
		bool hostWanted;
		Listing listing;
		int listingNumber;

		// A game that should come off the list, known by its region and code
		bool endWanted;
		char endRegion[kRelayListRegionLetters + 1];
		char endCode[kRelayCodeLetters + 1];

		// How many times the game has asked for that, and how many of those the list's thread has seen to (taken the game off, or found it
		// wasn't on). endDone is set each time the thread catches up, for RelayList_BeforeQuitting to wait on.
		int endsAsked;
		int endsDone;
		HANDLE endDone;

		// The relays: a call wanted, and the last good answer
		bool relaysWanted;
		bool relaysKnown;
		RelayListRelay relays[kRelayListMostRelays];
		int relayCount;

		// Finding games: a call wanted, and an answer waiting for the main thread
		bool findWanted;
		bool answerReady;
		RelayListGame games[kRelayListMostGames];
		int count;
		const char *why;

		// News for the main thread to write in the log: the last kMostNews pieces, each a fixed text of the game's own, kept in turn round
		// the ring. newsNumber is how many pieces there have ever been, so piece n is at news[n % kMostNews].
		// Modified 2026-11-09 by Melanikus for the AvP port: each piece is a copy of its text, kept with its end, not a pointer to it, so the
		// main thread never follows a pointer the list's thread handed it, and never reads past a piece's own room (Snyk Code).
		char news[kMostNews][kMostNewsLetters + 1];
		int newsNumber;
	};

	Shared *shared = NULL;
	int newsWritten = 0;   // the main thread's: how many pieces of news it has written

	// ---- The list's thread

	// Leaves a piece of news for the main thread. If more than kMostNews come between two of its looks, the oldest go, and it says how many.
	// Modified 2026-11-09 by Melanikus for the AvP port: it keeps a copy of the text, at most kMostNewsLetters of it, and ends the copy.
	// No text is no news.
	void Tell(const char *news)
	{
		if (!news)
		{
			return;
		}

		size_t const letters = strnlen(news, kMostNewsLetters);
		EnterCriticalSection(&shared->lock);
		char *const kept = shared->news[shared->newsNumber % kMostNews];
		memcpy(kept, news, letters);
		kept[letters] = 0;
		shared->newsNumber++;
		LeaveCriticalSection(&shared->lock);
	}

	// What a call gave back
	struct Answer
	{
		bool worked;          // it reached the list and got a status
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

		// Finding the list, connecting, sending and each wait for the answer get 5 seconds each
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

		HINTERNET connection = WinHttpConnect(session, shared->server, shared->port, 0);
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

		// The answer, up to room bytes. Anything past room means it's too big, and the rest isn't read.
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

	// What the list's thread knows of the host's game on the list: whether it's on it, which game (region and code), its owner, and
	// which listing it last sent. Only the list's thread touches these.
	struct OnTheList
	{
		bool listed;
		char region[kRelayListRegionLetters + 1];
		char code[kRelayCodeLetters + 1];
		uint8_t owner[kRelayListSecretBytes];
		int listingNumber;
	};

	OnTheList onTheList;

	// The answers to a host's calls: a POST's has the owner, and the others aren't read beyond their status
	char hostAnswer[kRelayListMostOwnerBytes];

	RelayListListing ListingToSend(const Listing &l)
	{
		RelayListListing out;
		out.region = l.region;
		out.code = l.code;
		memcpy(out.pass, l.pass, sizeof(out.pass));
		out.passTime = l.passTime;
		out.name = l.name;
		out.level = l.level;
		out.mode = l.mode;
		out.gameStyle = l.gameStyle;
		out.levelNumber = l.levelNumber;
		out.coopLobby = l.coopLobby;
		out.players = l.players;
		out.maxPlayers = l.maxPlayers;
		out.version = l.version;
		out.hostMs = l.hostMs;
		out.fingerprint = l.fingerprint;
		return out;
	}

	// Puts the host's game on the list with a POST, or renews it with a PUT. True if the list has this listing now.
	bool SendTheListing(const Listing &l)
	{
		char body[kRelayListMostBodyBytes + 1];
		RelayListListing const send = ListingToSend(l);

		// A new listing is POSTed. If a PUT finds the list has forgotten it (404), it's POSTed again, once.
		for (int tries = 0; tries < 2; tries++)
		{
			if (!RelayList_ListingBody(send, shared->v2Draft, onTheList.listed ? onTheList.owner : NULL, body, sizeof(body)))
			{
				Tell("the game's own words couldn't be made into a listing, so it isn't on the list");
				return false;
			}

			bool const posting = !onTheList.listed;
			Answer const a = Call(posting ? L"POST" : L"PUT", L"/v1/games", body, hostAnswer, sizeof(hostAnswer));

			if (!a.worked)
			{
				Tell(a.why);
				return false;
			}

			if (posting && a.status == 201)
			{
				if (RelayList_ReadOwner(hostAnswer, a.bytes, onTheList.owner) != kRelayListAccepted)
				{
					Tell("the answer to the game's listing was refused, so it can't be kept up");
					return false;
				}

				onTheList.listed = true;
				Tell("the game is on the list");
				return true;
			}

			if (!posting && a.status == 204)
			{
				return true;
			}

			if (!posting && a.status == 404)
			{
				onTheList.listed = false;
				continue;
			}

			// 403 (a pass that's no longer good), 409 (listed already, by a POST whose answer was lost) or 503 (the list is full): the
			// next pass, or the list forgetting the old listing, puts it right, so it's tried again later
			break;
		}

		Tell(kSaidNo);
		return false;
	}

	void TakeOffTheList(void)
	{
		char body[kRelayListMostBodyBytes + 1];

		if (!RelayList_EndingBody(onTheList.region, onTheList.code, onTheList.owner, body, sizeof(body)))
		{
			return;
		}

		onTheList.listed = false;

		// 204: taken off. 404: the list had forgotten it already, which is what's wanted too. Either way, a listing the host stops
		// renewing goes from the list by itself within 45 seconds.
		Answer const a = Call(L"DELETE", L"/v1/games", body, hostAnswer, sizeof(hostAnswer));

		if (!a.worked)
		{
			Tell(a.why);
		}
		else if (a.status == 204 || a.status == 404)
		{
			Tell("the game is off the list");
		}
		else
		{
			Tell(kSaidNo);
		}
	}

	// The answers to the two GETs: read as hostile (relay_list_text.h), and only kept if all of it is right
	char gamesAnswer[kRelayListMostGamesBytes];
	char relaysAnswer[kRelayListMostRelaysBytes];
	RelayListGame gamesRead[kRelayListMostGames];
	RelayListRelay relaysRead[kRelayListMostRelays];

	void ReadTheGames(void)
	{
		wchar_t path[64];
		swprintf(path, sizeof(path) / sizeof(path[0]), L"/v1/games?version=%d", shared->version);

		Answer const a = Call(L"GET", path, NULL, gamesAnswer, sizeof(gamesAnswer));
		int count = 0;
		const char *why = a.why;

		if (a.worked && a.status != 200)
		{
			why = kSaidNo;
		}
		else if (a.worked)
		{
			RelayListRefusal const refusal = RelayList_ReadGames(gamesAnswer, a.bytes, shared->version, shared->v2Draft, gamesRead, &count);
			why = (refusal == kRelayListAccepted) ? NULL : RelayList_RefusalWords(refusal);
		}

		EnterCriticalSection(&shared->lock);
		shared->count = why ? 0 : count;
		memcpy(shared->games, gamesRead, sizeof(RelayListGame) * static_cast<size_t>(shared->count));
		shared->why = why;
		shared->answerReady = true;
		LeaveCriticalSection(&shared->lock);
	}

	// Looks a relay's name up, as ENet does: IPv4 only. The first address the name service gives, in network byte order, as ENetAddress.host
	// keeps it. False if the name gives none.
	bool LookUp(const char *name, uint32_t *address)
	{
		addrinfo hints;
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_DGRAM;

		addrinfo *found = NULL;

		if (getaddrinfo(name, NULL, &hints, &found) != 0 || !found)
		{
			return false;
		}

		bool got = false;

		for (const addrinfo *a = found; a && !got; a = a->ai_next)
		{
			if (a->ai_family == AF_INET && a->ai_addr && a->ai_addrlen >= sizeof(sockaddr_in))
			{
				*address = reinterpret_cast<const sockaddr_in *>(a->ai_addr)->sin_addr.s_addr;
				got = true;
			}
		}

		freeaddrinfo(found);
		return got;
	}

	void ReadTheRelays(void)
	{
		Answer const a = Call(L"GET", L"/v1/relays", NULL, relaysAnswer, sizeof(relaysAnswer));
		int count = 0;

		if (!a.worked)
		{
			Tell(a.why);
			return;
		}

		if (a.status != 200)
		{
			Tell(kSaidNo);
			return;
		}

		RelayListRefusal const refusal = RelayList_ReadRelays(relaysAnswer, a.bytes, relaysRead, &count);

		if (refusal != kRelayListAccepted)
		{
			Tell("the list of relays was refused, so the game keeps the last good one, if it has one");
			return;
		}

		// Each relay's name is looked up here, on this thread, so the game's own thread never waits for the network's name service. A
		// relay whose name gives no address, or an address that can't be a relay's (RelayList_AddressRefusal), is left out.
		int kept = 0;

		for (int i = 0; i < count; i++)
		{
			uint32_t address = 0;

			if (!LookUp(relaysRead[i].host, &address))
			{
				Tell("a relay's name gave no address, so that relay is left out");
				continue;
			}

			RelayAddressRefusal const why = RelayList_AddressRefusal(ntohl(address), shared->developer);

			if (why != kRelayAddressUsable)
			{
				Tell(RelayList_AddressWords(why));
				continue;
			}

			relaysRead[kept] = relaysRead[i];
			relaysRead[kept].address = address;
			kept++;
		}

		EnterCriticalSection(&shared->lock);
		memcpy(shared->relays, relaysRead, sizeof(RelayListRelay) * static_cast<size_t>(kept));
		shared->relayCount = kept;
		shared->relaysKnown = true;
		LeaveCriticalSection(&shared->lock);
	}

	DWORD WINAPI ListThread(void *)
	{
		bool tryAgainLater = false;
		onTheList.listed = false;
		onTheList.listingNumber = -1;

		for (;;)
		{
			// A listing that didn't get through is tried again every 10 seconds, for as long as the room is open
			WaitForSingleObject(shared->wake, tryAgainLater ? 10000 : INFINITE);

			EnterCriticalSection(&shared->lock);
			bool const end = shared->endWanted;
			int const endsAsked = shared->endsAsked;
			char endRegion[kRelayListRegionLetters + 1];
			char endCode[kRelayCodeLetters + 1];
			memcpy(endRegion, shared->endRegion, sizeof(endRegion));
			memcpy(endCode, shared->endCode, sizeof(endCode));
			shared->endWanted = false;

			bool const host = shared->hostWanted;
			Listing const listing = shared->listing;
			int const number = shared->listingNumber;

			bool const relays = shared->relaysWanted;
			shared->relaysWanted = false;
			bool const find = shared->findWanted;
			shared->findWanted = false;
			LeaveCriticalSection(&shared->lock);

			// The game that ended comes off the list, if it's the one on it
			if (end && onTheList.listed && strcmp(endRegion, onTheList.region) == 0 && strcmp(endCode, onTheList.code) == 0)
			{
				TakeOffTheList();
			}

			// Every end asked for up to now has been seen to, so a game that's quitting can stop waiting (RelayList_BeforeQuitting)
			if (end)
			{
				EnterCriticalSection(&shared->lock);
				shared->endsDone = endsAsked;
				LeaveCriticalSection(&shared->lock);
				SetEvent(shared->endDone);
			}

			tryAgainLater = false;

			if (host)
			{
				// Another room is another game, not yet on the list. The one before it, if it was, goes when the list forgets it.
				if (strcmp(listing.region, onTheList.region) != 0 || strcmp(listing.code, onTheList.code) != 0)
				{
					onTheList.listed = false;
					memcpy(onTheList.region, listing.region, sizeof(onTheList.region));
					memcpy(onTheList.code, listing.code, sizeof(onTheList.code));
					onTheList.listingNumber = -1;
				}

				if (number != onTheList.listingNumber)
				{
					if (SendTheListing(listing))
					{
						onTheList.listingNumber = number;
					}
					else
					{
						tryAgainLater = true;
					}
				}
			}

			if (relays)
			{
				ReadTheRelays();
			}

			if (find)
			{
				ReadTheGames();
			}
		}
	}

	// Makes the store and starts the thread, the first time either is needed. False if there's no list set, or it can't start.
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
		made->endDone = CreateEventW(NULL, FALSE, FALSE, NULL);

		// The name is letters, digits, - and . only (Relay_IsServerName), so each one is the same as a wide character
		for (size_t i = 0; name[i] && i + 1 < sizeof(made->server) / sizeof(made->server[0]); i++)
		{
			made->server[i] = static_cast<wchar_t>(name[i]);
		}

		made->port = static_cast<INTERNET_PORT>(Relay_ListPort());
		made->developer = DevMode != 0;

		// The version and the v2 draft are fixed for the run, so they're read once here, on the main thread (added 2026-11-09 by Melanikus
		// for the AvP port; networking.h)
		made->version = Net_OurVersion();
		made->v2Draft = Net_V2DraftOn();

		// The name service is Windows' sockets', which have to be started on this thread's behalf too. ENet starts them as well. Each start
		// is counted by Windows, and this one is kept for as long as the game runs, as the thread is.
		WSADATA sockets;

		if (WSAStartup(MAKEWORD(2, 2), &sockets) != 0)
		{
			LogString("Relay list: couldn't start, so hosts aren't listed and the list can't be read");
			return false;
		}

		if (!made->wake || !made->endDone)
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

	// The main thread writes the list's news in the log, every piece since it last looked, in order, each line starting "Relay list: ", so
	// that one search of the log finds them all
	void WriteTheNews(void)
	{
		if (!shared)
		{
			return;
		}

		char news[kMostNews][kMostNewsLetters + 1];

		EnterCriticalSection(&shared->lock);
		int const number = shared->newsNumber;
		memcpy(news, shared->news, sizeof(news));
		LeaveCriticalSection(&shared->lock);

		int const oldestKept = number - kMostNews;

		if (newsWritten < oldestKept)
		{
			LogString("Relay list: " + std::to_string(oldestKept - newsWritten) + " earlier piece(s) of news came too fast to keep");
			newsWritten = oldestKept;
		}

		// Modified 2026-11-09 by Melanikus for the AvP port (Snyk Code): each piece is a copy, kept with its end (Tell). It's ended again here
		// at the last place of its room, so even a piece that had somehow lost its end could never be read past its room.
		for (; newsWritten < number; newsWritten++)
		{
			char *const piece = news[newsWritten % kMostNews];
			piece[kMostNewsLetters] = 0;
			LogString(std::string("Relay list: ") + piece);
		}
	}

	void Wake(void)
	{
		SetEvent(shared->wake);
		WriteTheNews();
	}

	// The page asked with no list to ask: its answer, which needs no thread
	bool noListAnswerWaiting = false;

	// This run's identity's fingerprint, from RelayList_HostIdentity, and whether one has been given. Only the main thread uses these.
	uint8_t hostIdentity[kNetIdentityFingerprintBytes];
	bool hostIdentityGiven = false;
}

// The list's name built into the game: the relay server's own, once its domain is bought. It's empty until then, and then there's no relay.
static const char kRelayBuiltInServer[] = "";

// AliensVsPredator.cfg is a text file people share, and anyone can edit it, so it mustn't be able to point the game at another computer
// (tests/settings_file_test.cpp). A shared file naming another list would send every player who used it through that list's relays. So the
// settings' RelayServer and RelayListPort are read only with -dev, for a test with the list on this machine. Without -dev, the game uses the
// name built into it, at port 443, and a RelayServer in the settings is ignored, with a line in the log to say so.
const char *Relay_ServerName(void)
{
	static char name[254];
	static bool read = false;

	if (!read)
	{
		read = true;
		std::string const setting = Config_GetString("[Network]", "RelayServer", "");

		if (!DevMode)
		{
			memcpy(name, kRelayBuiltInServer, sizeof(kRelayBuiltInServer));

			if (!setting.empty())
			{
				LogString("Relay list: the settings' RelayServer is for tests only, with -dev, so the game's own is used");
			}
		}
		else if (setting.size() < sizeof(name) && Relay_IsServerName(setting.c_str()))
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

int Relay_ListPort(void)
{
	static int port = 0;

	if (port == 0)
	{
		port = kRelayListPort;

		// Like RelayServer, only with -dev (see above)
		if (DevMode)
		{
			int const setting = Config_GetInt("[Network]", "RelayListPort", kRelayListPort);

			if (setting >= 1 && setting <= 65535)
			{
				port = setting;
			}
			else
			{
				LogString("Relay list: the settings' RelayListPort isn't a port from 1 to 65535, so 443 is used");
			}
		}
	}

	return port;
}

// Added 2026-11-09 by Melanikus for the AvP port: see relay_list.h
void RelayList_HostIdentity(const uint8_t fingerprint[kNetIdentityFingerprintBytes])
{
	hostIdentityGiven = false;

	if (!fingerprint)
	{
		return;
	}

	memcpy(hostIdentity, fingerprint, sizeof(hostIdentity));

	// All 0 is what an identity never made looks like, so it counts as none
	for (size_t i = 0; i < sizeof(hostIdentity); i++)
	{
		hostIdentityGiven = hostIdentityGiven || hostIdentity[i] != 0;
	}
}

void RelayList_HostRoomOpened(const char *region, const char code[kRelayCodeLetters], const uint8_t pass[kRelayListSecretBytes], uint64_t passTime,
	int hostMs, const SessionDescription &session)
{
	// What networking.cpp hands over is checked here too: a room the list couldn't take isn't sent
	if (!RelayList_IsRegion(region) || !code || !Relay_IsCode(code, kRelayCodeLetters) || !pass)
	{
		return;
	}

	// A listing without the host's fingerprint would send its joiners to a host they can't check, so it isn't sent at all. Modified
	// 2026-11-09 by Melanikus for the AvP port: only with the v2 draft (networking.h). Version 1's listing has no fingerprint.
	if (Net_V2DraftOn() && !hostIdentityGiven)
	{
		LogString("Relay list: this run has no identity to show joiners yet, so the game isn't put on the list");
		return;
	}

	if (!Start())
	{
		return;
	}

	Listing l;
	memset(&l, 0, sizeof(l));
	memcpy(l.region, region, strlen(region) + 1);
	memcpy(l.code, code, kRelayCodeLetters);
	memcpy(l.pass, pass, sizeof(l.pass));
	l.passTime = passTime;
	l.hostMs = hostMs;
	memcpy(l.fingerprint, hostIdentity, sizeof(l.fingerprint));
	ShowSession(session, l);

	EnterCriticalSection(&shared->lock);
	shared->hostWanted = true;
	shared->listing = l;
	shared->listingNumber++;
	LeaveCriticalSection(&shared->lock);

	Wake();
}

void RelayList_HostKept(const uint8_t pass[kRelayListSecretBytes], uint64_t passTime, int hostMs)
{
	if (!shared || !pass)
	{
		return;
	}

	EnterCriticalSection(&shared->lock);
	bool const wanted = shared->hostWanted;

	if (wanted)
	{
		memcpy(shared->listing.pass, pass, sizeof(shared->listing.pass));
		shared->listing.passTime = passTime;
		shared->listing.hostMs = hostMs;
		shared->listingNumber++;
	}

	LeaveCriticalSection(&shared->lock);

	if (wanted)
	{
		Wake();
	}
}

void RelayList_HostSessionNow(const SessionDescription &session)
{
	if (!shared)
	{
		return;
	}

	bool changed = false;

	EnterCriticalSection(&shared->lock);

	if (shared->hostWanted)
	{
		Listing now = shared->listing;
		ShowSession(session, now);

		if (!SameShown(now, shared->listing))
		{
			shared->listing = now;
			shared->listingNumber++;
			changed = true;
		}
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
		shared->endsAsked++;
		memcpy(shared->endRegion, shared->listing.region, sizeof(shared->endRegion));
		memcpy(shared->endCode, shared->listing.code, sizeof(shared->endCode));
		ended = true;
	}

	LeaveCriticalSection(&shared->lock);

	if (ended)
	{
		SetEvent(shared->wake);
	}

	WriteTheNews();
}

// Modified 2026-11-09 by Melanikus for the AvP port: see relay_list.h. The list's thread does the DELETE, so this only waits for it.
void RelayList_BeforeQuitting(unsigned int mostMs)
{
	if (!shared)
	{
		return;
	}

	// A game still on the list comes off it, even if the networking code never said the room had ended. If it did, this does nothing.
	RelayList_HostRoomClosed();

	DWORD const started = GetTickCount();

	for (;;)
	{
		EnterCriticalSection(&shared->lock);
		bool const waiting = shared->endsDone != shared->endsAsked;
		LeaveCriticalSection(&shared->lock);

		if (!waiting)
		{
			break;
		}

		DWORD const waited = GetTickCount() - started;

		if (waited >= mostMs)
		{
			LogString("Relay list: the game closed before it could come off the list, so the list forgets it by itself, within 45 seconds");
			break;
		}

		WaitForSingleObject(shared->endDone, mostMs - waited);
	}

	// What the thread said about it (the game is off the list, or why not) goes in the log before the game closes
	WriteTheNews();
}

void RelayList_FindRelays(void)
{
	if (!Start())
	{
		return;
	}

	EnterCriticalSection(&shared->lock);
	shared->relaysWanted = true;
	LeaveCriticalSection(&shared->lock);

	Wake();
}

bool RelayList_Relays(RelayListRelay *relays, int *count)
{
	*count = 0;

	if (!shared)
	{
		return false;
	}

	EnterCriticalSection(&shared->lock);
	bool const known = shared->relaysKnown;

	if (known)
	{
		*count = shared->relayCount;
		memcpy(relays, shared->relays, sizeof(RelayListRelay) * static_cast<size_t>(shared->relayCount));
	}

	LeaveCriticalSection(&shared->lock);
	return known;
}

void RelayList_FindGames(void)
{
	if (!Start())
	{
		noListAnswerWaiting = true;
		return;
	}

	EnterCriticalSection(&shared->lock);
	shared->findWanted = true;
	LeaveCriticalSection(&shared->lock);

	Wake();
}

bool RelayList_FoundGames(RelayListGame *games, int *count, const char **why)
{
	*count = 0;
	*why = NULL;

	if (noListAnswerWaiting)
	{
		noListAnswerWaiting = false;
		*why = Relay_ServerName()[0] ? kCantStart : kNoServer;
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
