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
// Modified 2026-11-10 by Melanikus for the AvP port (#175): THE SERVER IS A servers.cfg LINE. The game's one server is the first good line
// of servers.cfg in the game's folder (servers_cfg.h, servers_cfg_file.h): one address, where the list answers HTTPS and the relay takes
// the game's messages, and one fingerprint for both of the server's keys (net_server_fingerprint.h). Nothing is looked up by name, and
// nothing goes anywhere but that address. Modified 2026-11-10 by Melanikus for the AvP port (#175 PR 3b): -dev decides nothing here, so
// a player's game uses that line just as a developer's does.
//
// HTTPS: WinHTTP, Windows' own. TLS 1.2 or newer. Redirects, cookies and sign-ins are all turned off, since the list never needs them, and a
// redirect could send the game somewhere else. Each step has 5 seconds.
//
// TRUSTED BY ITS KEY ALONE (the owner's "trust the pin alone"): Windows' own rules for a certificate (who issued it, its name, its dates)
// are switched off for these calls, and the list's key is checked in their place, in WinHTTP's own callback, just before each call's first
// byte goes:
//   1. The first call is always GET /v1/relay-keys, which asks for nothing and sends nothing but the request. Its callback keeps the key
//      the list showed (net_server_key.h). Its answer gives the relay's keys, and the two keys together must make the line's fingerprint.
//      If they don't, nothing more is sent to the server for the rest of the run, by HTTPS or by the relay's own messages.
//   2. Every call after it is closed in its callback, unsent, unless the list shows the very key that matched.

#include "3dc.h"
#include "enet\enet.h"
#include "configFile.h"
#include "logString.h"
#include "networking.h"
#include "coop_lobby.h"
#include "relay_list.h"
#include "net_relay_keys.h"
#include "net_server_fingerprint.h"
#include "net_server_key.h"
#include "servers_cfg_file.h"
#include "servers_trust.h"
#include "net_identity_store.h"

#include <winsock2.h>
#include <winhttp.h>
#include <wincrypt.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <new>

// WinHTTP's library, which comes with Windows, and crypt32, Windows' own, to let go of the certificate WinHTTP hands over
// (CertFreeCertificateContext)
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "crypt32.lib")

// Whether this run is Hudson's, the team's test bot (hudson.cpp's own flag, set only with -dev -hudson; tools/check-hudson-asleep.ps1
// proves a player's game never sets it). A Hudson run uses servers.cfg's line without the player's yes, as nobody is there to give it
// (#175's last part, added 2026-11-10 by Melanikus for the AvP port). Named here alone, as this file reads it and never writes it.
extern bool HudsonRunIsOn;

// RelayList_ModeWords has the game styles in the order of NETGAME_TYPE. If that order ever changes, this stops the build.
static_assert(NGT_Individual == 0 && NGT_CoopDeathmatch == 1 && NGT_LastManStanding == 2 && NGT_PredatorTag == 3 && NGT_Coop == 4 &&
	NGT_AlienTag == 5, "RelayList_ModeWords in relay_list_text.cpp has the game styles in NETGAME_TYPE's order, and that has changed");

// A level's name on the list is at most what the game's own session holds
static_assert(kRelayListLevelLetters == kLevelNameSize - 1, "the list's level names are the game's own, kLevelNameSize - 1 long");

// Added 2026-11-10 by Melanikus for the AvP port (#175): the game's one server, a good line of servers.cfg (servers_cfg_file.h). Modified
// the same day (#175 2b): the player's pick of a line, if it's one of the good lines, or else the first good line, as before there was a
// pick (servers_trust.h, THE PICK).
namespace
{
	// What a yes to a line is, and a pick of it: SHA-256 over servers_trust.h's 68 bytes. False if it couldn't be worked out, which counts
	// as no yes, and as never the pick's line.
	bool LineHash(const ServersCfgServer &line, uint8_t hash[kServersTrustHashBytes])
	{
		uint8_t input[kServersTrustInputBytes];
		NetCryptoPiece const piece = { input, sizeof(input) };
		return ServersTrust_HashInput(line.address, line.listPort, line.relayPort, line.fingerprint, input) && NetCrypto_Sha256(&piece, 1, hash);
	}

	// servers.cfg, read the first time the server is asked about and kept for the run, so a line changed while the game runs can't change
	// the server halfway through a game. Every refusal goes to the log, with the line's number and the reader's own words, never the
	// line's text. The pick is read at the same moment, once a run, so a pick made while the game runs is for the next start, never this
	// one (#175 2b). Only the main thread reads or changes these.
	struct TheServer
	{
		bool read;
		bool there;
		ServersCfgServer line;
		ServersCfgOpening opening;                                     // whether servers.cfg could be read at all (#175 2b)
		int inUse;                                                     // which good line is this run's, from 0, or -1 for none
		uint8_t hashes[kServersCfgMostLines][kServersTrustHashBytes];  // each good line's hash, in the file's order
		bool hashed[kServersCfgMostLines];                             // and whether it could be worked out
		ServersTrustPick pickNow;                                      // the pick as kept now: the one read, or one made since
	};

	TheServer theServer = {};
	ServersCfgFileRead theFile;   // what servers.cfg said, for the server lists (RelayList_LineAt and the rest)

	const ServersCfgServer *Server(void)
	{
		if (!theServer.read)
		{
			theServer.read = true;
			theServer.inUse = -1;
			theServer.opening = ServersCfg_ReadTheGames(&theFile);

			if (theServer.opening != kServersCfgOpened)
			{
				LogString(std::string("Relay list: ") + ServersCfg_OpeningWords(theServer.opening));
			}
			else if (theFile.fileRefusal != kServersCfgFileRead)
			{
				LogString(std::string("Relay list: " SERVERS_CFG_FILE_NAME " isn't used: ") + ServersCfg_FileWords(theFile.fileRefusal));
			}

			for (int i = 0; i < theFile.refusedCount; i++)
			{
				LogString("Relay list: " SERVERS_CFG_FILE_NAME " line " + std::to_string(theFile.refused[i].lineNumber) + " isn't used: " +
					ServersCfg_LineWords(theFile.refused[i].why));
			}

			for (int i = 0; i < theFile.serverCount; i++)
			{
				theServer.hashed[i] = LineHash(theFile.servers[i], theServer.hashes[i]);
			}

			// The player's pick, read as hostile: a file that can't be used counts as none, and so the first good line is used
			const char *why = NULL;
			NetTrustStoreResult const picked = NetIdentityStore_LoadPick(&theServer.pickNow, &why);

			if (picked != kNetTrustStoreRead && picked != kNetTrustStoreNoFile)
			{
				LogString(std::string("Relay list: " SERVERS_PICK_FILE_NAME " isn't used, so the first good line is: ") + (why ? why : "it couldn't be read"));
			}

			theServer.inUse = ServersTrust_LineToUse(theServer.hashes, theServer.hashed, theFile.serverCount, theServer.pickNow);

			if (theServer.inUse >= 0)
			{
				theServer.line = theFile.servers[theServer.inUse];
				theServer.there = true;
				bool const isThePick = theServer.pickNow.there && theServer.hashed[theServer.inUse] &&
					memcmp(theServer.hashes[theServer.inUse], theServer.pickNow.hash, kServersTrustHashBytes) == 0;
				char const *const where = theServer.line.place == kServersCfgOnTheInternet ? "" :
					theServer.line.place == kServersCfgOnThisPC ? ", on this PC" : ", on this network";
				LogString("Relay list: the server is " SERVERS_CFG_FILE_NAME " line " + std::to_string(theServer.line.lineNumber) + where +
					(isThePick ? ", the player's pick" : ""));
			}
		}

		return theServer.there ? &theServer.line : NULL;
	}
}

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
		uint8_t hostId[kNetIdentityIdBytes];   // this run's identity's whole id, as it was when the room opened
	};

	// Added 2026-11-09 by Melanikus for the AvP port: whether two of the list's text fields of the same size say the same, read only within
	// that size, so a field that somehow had no end could never be read past it (Snyk Code: improper null termination). The compiler
	// refuses two fields of different sizes.
	template <size_t N>
	bool SameField(const char (&a)[N], const char (&b)[N])
	{
		return strncmp(a, b, N) == 0;
	}

	// Whether what the list shows of the game is the same: the pass and the round trip aren't shown, so they don't count here
	bool SameShown(const Listing &a, const Listing &b)
	{
		return SameField(a.name, b.name) && SameField(a.level, b.level) && SameField(a.mode, b.mode) && a.gameStyle == b.gameStyle &&
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
	// Modified 2026-11-10 by Melanikus for the AvP port (#175): the secure connection itself failed (such as a TLS version neither side
	// has). Its certificate's rules are switched off, as the server is trusted by its key alone, so a certificate is never the reason
	const char *const kCertificate = "the relay server's secure connection couldn't be made";
	const char *const kTooLong = "the relay server took too long to answer";
	const char *const kTooBig = "the relay server's answer was too big";
	const char *const kSaidNo = "the relay server said no";
	const char *const kCantStart = "the game couldn't start a call to the relay server";
	// Added 2026-11-10 by Melanikus for the AvP port (F1): a Windows that can't be told to fetch nothing for the server's certificate
	const char *const kNoChainOff = "this Windows can't be told to fetch nothing for the relay server's certificate, so nothing was sent to it";
	// Modified 2026-11-10 by Melanikus for the AvP port (#175): the words for the server's key (TRUSTED BY ITS KEY ALONE, above)
	const char *const kKeyChanged = "the relay server showed another key than the one that matched its line, so nothing was sent to it";
	const char *const kKeyUnread = "the relay server's key couldn't be read, so nothing was sent to it";
	const char *const kKeysDontMatch = "the relay server's keys don't match its line in " SERVERS_CFG_FILE_NAME ", so nothing more is sent to it";
	const char *const kNotCheckedYet = "the relay server's keys haven't been checked yet, so nothing was sent to it";

	// Where the server's key stands: not checked yet (only the first call may go), checked and good, or refused for the rest of the run
	int const kKeyNotChecked = 0;
	int const kKeyGood = 1;
	int const kKeyRefused = 2;

	struct Shared;

	// Modified 2026-11-10 by Melanikus for the AvP port (#175): what the key check found, for the call that set it going (TRUSTED BY ITS KEY
	// ALONE, at the top of this file). The first call keeps the key the list showed, to be held to the line after; every other call is
	// held to the key that matched, and closed unsent if the list shows another. Modified 2026-11-11 (all servers' games): each check is
	// its own server's (owner), and is held to that server's key alone.
	struct KeyCheck
	{
		Shared *owner;   // the server whose call this is, set by CallChecked
		bool keepIt;     // the first call: keep the key the list shows
		bool looked;     // the callback ran, and read the list's key
		bool matched;    // the key was kept (the first call), or it was the one that matched (every other call)
		bool closed;     // it wasn't, or it couldn't be read, so the callback closed the call before its first byte went
		uint8_t shown[kNetServerFingerprintMostKeyBytes];
		size_t shownSize;
	};

	// Everything the two threads share. Made once and never freed: the list's thread may still be using it as the game closes.
	// Modified 2026-11-11 by Melanikus for the AvP port (the owner's ruling: Find games lists every server servers.cfg names that the player
	// has said yes to): there's one of these for each such server, each with a thread of its own. The line in use has the one called shared,
	// for everything (hosting, access, the relay's keys, its games); every other has one for its games alone (gamesOnly). Nothing is shared
	// between two of them: each server's key, its checks and its answers are its own, so one server's key can never stand for another's.
	struct Shared
	{
		CRITICAL_SECTION lock;
		HANDLE wake;                     // set when the main thread wants something done

		// The server, from its servers.cfg line, set before the thread starts, and never changed (modified 2026-11-10, #175): the address
		// as text for WinHTTP, the list's port, the line's fingerprint, and the address and port of the relay, as ENetAddress keeps them
		wchar_t server[16];
		INTERNET_PORT port;
		uint8_t lineFingerprint[kNetServerFingerprintBytes];
		uint32_t relayAddress;
		int relayPort;
		int version;                     // the version this copy speaks (kMultiplayerVersion), whose games it lists and asks for

		// The server's key (added 2026-11-10, #175). keyState is the list's thread's to set, and anyone's to read under the lock. Once
		// it's good, the key that matched and the relay's region and keys are set once, and never changed.
		int keyState;
		const char *serverWhy;   // why the last check of the server ended without an answer, or NULL (one of this file's own words; #188)
		uint8_t goodKey[kNetServerFingerprintMostKeyBytes];
		size_t goodKeySize;
		char relayRegion[kRelayListRegionLetters + 1];
		uint8_t relayP384[kNetCryptoP384PublicBytes];
		uint8_t relayKem[kNetPqKemPublicBytes];

		// A relay's keys, asked for by networking.cpp (RelayList_AskForRelayKeys; added 2026-11-09 by Melanikus for the AvP port). Each asking
		// has a number, and an answer says which asking it's for, so an answer to an older one is never taken for the newest.
		bool keysWanted;
		char keysRegion[kRelayListRegionLetters + 1];
		int keysAsked;                   // how many askings there have been
		int keysAnswered;                // which asking the answer below is for: 0 while none has come
		const char *keysWhy;             // NULL: the keys came, whole; or fixed words saying why not
		uint8_t keysP384[kNetCryptoP384PublicBytes];
		uint8_t keysKem[kNetPqKemPublicBytes];

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

		// The relay: the server's check wanted (its one relay is known once the server is checked and good: modified 2026-11-10, #175)
		bool relaysWanted;

		// Finding games: a call wanted, and an answer waiting for the main thread
		bool findWanted;
		bool answerReady;
		RelayListGame games[kRelayListMostGames];
		int count;
		const char *why;

		// Asking for online access: a request wanted, with its body and the fingerprint its answer must be about, and an answer waiting
		// for the main thread (added 2026-11-09, server #24)
		bool accessWanted;
		char accessBody[kRelayAccessMostBodyBytes + 1];
		uint8_t accessFingerprint[kNetIdentityFingerprintBytes];
		bool accessReady;
		bool accessAnswered;
		RelayAccessAnswer accessAnswer;
		const char *accessWhy;

		// News for the main thread to write in the log: the last kMostNews pieces, each a fixed text of the game's own, kept in turn round
		// the ring. newsNumber is how many pieces there have ever been, so piece n is at news[n % kMostNews].
		// Modified 2026-11-09 by Melanikus for the AvP port: each piece is a copy of its text, kept with its end, not a pointer to it, so the
		// main thread never follows a pointer the list's thread handed it, and never reads past a piece's own room (Snyk Code).
		char news[kMostNews][kMostNewsLetters + 1];
		int newsNumber;

		// Added 2026-11-11 (THE VISITS, below): this server's own key checks and answers, so no two servers' calls ever share one, and
		// none outlives a visit
		KeyCheck firstCheck;
		KeyCheck laterCheck;
		char keysAnswer[kRelayKeysMostAnswerBytes];
		char gamesAnswer[kRelayListMostGamesBytes];
		RelayListGame gamesRead[kRelayListMostGames];

		// The line in use's alone (added 2026-11-11): the lines a find visits, as the main thread chose them, each one the player has said
		// yes to (RelayList_FindGames), with their places among the good lines and the line in use's; and the answer of them all, handed
		// over whole once the last visit has ended
		int visitCount;
		ServersCfgServer visitLines[kRelayListMostVisits];
		int visitPlaces[kRelayListMostVisits];
		int visitInUse;
		bool allReady;
		RelayListServerGame allGames[kRelayListMostAllGames];
		int allCount;
		int lineStates[kServersCfgMostLines];
		const char *lineWhys[kServersCfgMostLines];
	};

	Shared *shared = NULL;
	int newsWritten = 0;   // the main thread's: how many pieces of news it has written

	// ---- The list's thread

	// Leaves a piece of news for the main thread. If more than kMostNews come between two of its looks, the oldest go, and it says how many.
	// Modified 2026-11-09 by Melanikus for the AvP port: it keeps a copy of the text, at most kMostNewsLetters of it, and ends the copy.
	// No text is no news. Modified 2026-11-11 (all servers' games): into its own server's news, s.
	void Tell(Shared *s, const char *news)
	{
		if (!news || !s)
		{
			return;
		}

		size_t const letters = strnlen(news, kMostNewsLetters);
		EnterCriticalSection(&s->lock);
		char *const kept = s->news[s->newsNumber % kMostNews];
		memcpy(kept, news, letters);
		kept[letters] = 0;
		s->newsNumber++;
		LeaveCriticalSection(&s->lock);
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

	// (KeyCheck, what the key check found, is above Shared: each server's state holds its own two.)

	// WinHTTP calls this just before it sends a call's first byte, with the secure connection already made, so the list's certificate is
	// there to read (WINHTTP_OPTION_SERVER_CERT_CONTEXT). context is the call's KeyCheck. Only the certificate's key is read from it: its
	// issuer, names and dates are never looked at. WinHTTP calls it on this same thread, the list's, which is the only one that sets the
	// key that matched, so that's read here without the lock.
	void CALLBACK CheckTheKey(HINTERNET request, DWORD_PTR context, DWORD status, LPVOID, DWORD)
	{
		KeyCheck *const check = reinterpret_cast<KeyCheck *>(context);

		if (status != WINHTTP_CALLBACK_STATUS_SENDING_REQUEST || !check || check->closed || !check->owner)
		{
			return;
		}

		// The call's own server (modified 2026-11-11): its key alone is the one a later call is held to
		Shared const *const server = check->owner;

		PCCERT_CONTEXT certificate = NULL;
		DWORD size = sizeof(certificate);
		bool const read = WinHttpQueryOption(request, WINHTTP_OPTION_SERVER_CERT_CONTEXT, &certificate, &size) && certificate &&
			certificate->pbCertEncoded && NetServerKey_OfCertificate(certificate->pbCertEncoded, certificate->cbCertEncoded, check->shown,
				sizeof(check->shown), &check->shownSize);

		if (certificate)
		{
			CertFreeCertificateContext(certificate);
		}

		check->looked = read;

		if (check->keepIt)
		{
			check->matched = read;
		}
		else
		{
			// Every byte is compared whatever the first difference, so the time it takes says nothing
			bool const sameSize = read && check->shownSize == server->goodKeySize;
			uint8_t differences = 0;

			for (size_t i = 0; sameSize && i < check->shownSize; i++)
			{
				differences |= static_cast<uint8_t>(check->shown[i] ^ server->goodKey[i]);
			}

			check->matched = sameSize && differences == 0;
		}

		// Not the key that matched, or none to read: the call is closed here, so nothing of it is sent
		if (!check->matched)
		{
			check->closed = true;
			WinHttpCloseHandle(request);
		}
	}

	// Added 2026-11-09 by Melanikus for the AvP port: the call's request, or NULL once the key's check has closed it, so a request that's
	// been closed is never closed again (CloseAll does nothing with NULL)
	HINTERNET StillOpen(const KeyCheck &check, HINTERNET request)
	{
		return check.closed ? NULL : request;
	}

	// One HTTPS call to the list. body is sent if there is one. What comes back goes into answer, which has room for room bytes: an
	// answer longer than that is refused as too big, without the rest being read. check says whether this is the first call, which keeps
	// the key the list shows (only CheckTheServer makes it), or a later one, which goes only to the key that matched (modified 2026-11-10,
	// #175).
	Answer CallChecked(Shared *s, const wchar_t *method, const wchar_t *path, const char *body, char *answer, size_t room, KeyCheck &check)
	{
		Answer a = { false, 0, kCantStart, 0 };

		// This call is this server's, and its key check is held to this server's key alone (modified 2026-11-11, all servers' games)
		check.owner = s;

		// Nothing goes to a server whose keys didn't match its line, and nothing but the first call goes before they're checked
		if (s->keyState == kKeyRefused || (!check.keepIt && s->keyState != kKeyGood))
		{
			a.why = s->keyState == kKeyRefused ? kKeysDontMatch : kNotCheckedYet;
			return a;
		}

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

		// No chain for the server's certificate (F1, PO's hostile read, 2026-11-10; added by Melanikus for the AvP port). Before the key's
		// check (CheckTheKey), Windows builds the certificate's chain, and to build it, it may fetch addresses the certificate itself names
		// (its AIA), even ones on the player's own network: something sent where the servers.cfg line didn't say, chosen by the server or
		// by anyone in between. The key alone is the trust, so no chain is needed. It's turned off; on a Windows without that, it's built
		// only from what Windows already holds, with nothing fetched; and with neither, the call isn't made. Windows takes them only on a
		// session not yet connected, so they're set here, before WinHttpConnect (tests/winhttp_chain_test tries each kind of handle).
		BOOL noFetch = TRUE;
		bool const noChain = WinHttpSetOption(session, WINHTTP_OPTION_DISABLE_CERT_CHAIN_BUILDING, &noFetch, sizeof(noFetch)) != FALSE ||
			WinHttpSetOption(session, WINHTTP_OPTION_SERVER_CERT_CHAIN_BUILD_CACHE_ONLY, &noFetch, sizeof(noFetch)) != FALSE;

		if (!noChain)
		{
			CloseAll(NULL, NULL, session);
			a.why = kNoChainOff;
			return a;
		}

		HINTERNET connection = WinHttpConnect(session, s->server, s->port, 0);
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

		// The key's check goes in first, and only then is Windows told to pass a certificate whatever its issuer, its name, its dates and
		// what it's for: the key the check reads is what stands in for all of them (the owner's "trust the pin alone")
		check.looked = false;
		check.matched = false;
		check.closed = false;
		check.shownSize = 0;
		DWORD pass = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
			SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;

		if (WinHttpSetStatusCallback(request, CheckTheKey, WINHTTP_CALLBACK_FLAG_SEND_REQUEST, 0) == WINHTTP_INVALID_STATUS_CALLBACK ||
			!WinHttpSetOption(request, WINHTTP_OPTION_SECURITY_FLAGS, &pass, sizeof(pass)))
		{
			CloseAll(request, connection, session);
			return a;
		}

		// Modified 2026-11-09 by Melanikus for the AvP port (Primary's read of the pin): the request goes out with its headers alone. A body,
		// which for a listing holds the room's pass and its owner's token, is written only after the key's check has passed (below), so a
		// list nobody checked can never be sent one. The headers say how long the body will be, but not what's in it.
		DWORD_PTR const context = reinterpret_cast<DWORD_PTR>(&check);
		DWORD const bodyBytes = body ? static_cast<DWORD>(strlen(body)) : 0;
		BOOL const sent = body
			? WinHttpSendRequest(request, L"Content-Type: application/json\r\n", static_cast<DWORD>(-1), WINHTTP_NO_REQUEST_DATA, 0, bodyBytes, context)
			: WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, context);

		// The key's check closed the call before anything of it went: the request is gone already
		if (check.closed)
		{
			a.why = check.looked ? kKeyChanged : kKeyUnread;
			CloseAll(NULL, connection, session);
			return a;
		}

		// A call whose check never ran: nothing more is sent, and what came back isn't used (the check is what stands in for Windows' own)
		if (!check.matched)
		{
			a.why = kKeyUnread;
			CloseAll(StillOpen(check, request), connection, session);
			return a;
		}

		// Only now, with the key checked, does the body go, all of it or the call fails
		DWORD written = 0;
		bool const bodyWent = sent && (!body || (WinHttpWriteData(request, body, bodyBytes, &written) && written == bodyBytes));
		bool const answered = bodyWent && !check.closed && WinHttpReceiveResponse(request, NULL);

		// WinHTTP sends a call again only for a redirect or a sign-in, which are both off. If it ever did, the check ran again, and may have
		// closed the call: then nothing more is done with the request, and it isn't closed a second time.
		if (check.closed)
		{
			a.why = check.looked ? kKeyChanged : kKeyUnread;
			CloseAll(NULL, connection, session);
			return a;
		}

		if (!answered)
		{
			a.why = WhyItFailed(GetLastError());
			CloseAll(StillOpen(check, request), connection, session);
			return a;
		}

		DWORD status = 0;
		DWORD statusSize = sizeof(status);

		if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize,
			WINHTTP_NO_HEADER_INDEX))
		{
			a.why = kCantReach;
			CloseAll(StillOpen(check, request), connection, session);
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
				CloseAll(StillOpen(check, request), connection, session);
				return a;
			}

			if (available == 0)
			{
				break;
			}

			if (got >= room)
			{
				a.why = kTooBig;
				CloseAll(StillOpen(check, request), connection, session);
				return a;
			}

			DWORD const wanted = (available < room - got) ? available : static_cast<DWORD>(room - got);
			DWORD read = 0;

			if (!WinHttpReadData(request, answer + got, wanted, &read))
			{
				a.why = WhyItFailed(GetLastError());
				CloseAll(StillOpen(check, request), connection, session);
				return a;
			}

			if (read == 0)
			{
				break;
			}

			got += read;
		}

		CloseAll(StillOpen(check, request), connection, session);
		a.worked = true;
		a.status = status;
		a.why = NULL;
		a.bytes = got;
		return a;
	}

	// Added 2026-11-10 by Melanikus for the AvP port (#175): the first call, and the check of the server against its line. GET /v1/relay-keys
	// asks for nothing and sends nothing but the request, and gives the keys of the relay on the list's own machine (CRYPTO.md 12.1). The
	// list's key it was shown and those keys must together make the line's fingerprint (net_server_fingerprint.h). If they do, the key and
	// the relay's region and keys are kept for the run. If they don't, nothing more is sent to the server for the rest of the run. A call
	// that couldn't be made or answered is tried again the next time anything wants the server. NULL once the server is checked and good;
	// otherwise fixed words saying why not, which the caller hands on.
	// (Its answer goes into the server's own keysAnswer, in Shared, never a buffer another server's check could share: modified 2026-11-11.)

	// Keeps why the server's check ended without an answer, for RelayList_ServerState, and gives it back (added 2026-11-10, #188)
	const char *CantUse(Shared *s, const char *why)
	{
		EnterCriticalSection(&s->lock);
		s->serverWhy = why;
		LeaveCriticalSection(&s->lock);
		return why;
	}

	const char *CheckTheServer(Shared *s)
	{
		if (s->keyState != kKeyNotChecked)
		{
			return s->keyState == kKeyGood ? NULL : kKeysDontMatch;
		}

		// A new asking: until it ends, the server stands as "no answer yet", not as the last asking left it (#188)
		EnterCriticalSection(&s->lock);
		s->serverWhy = NULL;
		LeaveCriticalSection(&s->lock);

		// This server's own first check, never one another server's could share (modified 2026-11-11: it was a static)
		KeyCheck &first = s->firstCheck;
		memset(&first, 0, sizeof(first));
		first.keepIt = true;
		Answer const a = CallChecked(s, L"GET", L"/v1/relay-keys", NULL, s->keysAnswer, sizeof(s->keysAnswer), first);

		if (!a.worked)
		{
			Tell(s, a.why);
			return CantUse(s, a.why);
		}

		if (a.status != 200)
		{
			Tell(s, kSaidNo);
			return CantUse(s, kSaidNo);
		}

		char region[kRelayListRegionLetters + 1];
		NetRelayStaticKeys keys;
		RelayListRefusal const refusal = RelayList_ReadRelayKeys(s->keysAnswer, a.bytes, region, keys.p384, keys.kem);

		if (refusal != kRelayListAccepted)
		{
			Tell(s, RelayList_RefusalWords(refusal));
			return CantUse(s, RelayList_RefusalWords(refusal));
		}

		bool const good = NetServerFingerprint_IsTheLines(first.shown, first.shownSize, &keys, s->lineFingerprint);

		EnterCriticalSection(&s->lock);

		if (good)
		{
			memcpy(s->goodKey, first.shown, first.shownSize);
			s->goodKeySize = first.shownSize;
			memcpy(s->relayRegion, region, sizeof(region));
			s->relayRegion[kRelayListRegionLetters] = 0;
			memcpy(s->relayP384, keys.p384, sizeof(keys.p384));
			memcpy(s->relayKem, keys.kem, sizeof(keys.kem));
		}

		s->keyState = good ? kKeyGood : kKeyRefused;
		LeaveCriticalSection(&s->lock);
		Tell(s, good ? "the relay server's keys match its line in " SERVERS_CFG_FILE_NAME : kKeysDontMatch);
		return good ? NULL : kKeysDontMatch;
	}

	// Every call after the first: the server is checked against its line first, if it hasn't been yet, and the call goes only to the key
	// that matched. A server that isn't checked and good is sent nothing, and the call says why.
	Answer Call(Shared *s, const wchar_t *method, const wchar_t *path, const char *body, char *answer, size_t room)
	{
		const char *const unchecked = CheckTheServer(s);

		if (unchecked)
		{
			Answer const none = { false, 0, unchecked, 0 };
			return none;
		}

		// This server's own later check, never one another server's could share (modified 2026-11-11: it was a static)
		KeyCheck &check = s->laterCheck;
		memset(&check, 0, sizeof(check));
		check.keepIt = false;
		return CallChecked(s, method, path, body, answer, room, check);
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
		out.hostId = l.hostId;
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
			if (!RelayList_ListingBody(send, onTheList.listed ? onTheList.owner : NULL, body, sizeof(body)))
			{
				Tell(shared, "the game's own words couldn't be made into a listing, so it isn't on the list");
				return false;
			}

			bool const posting = !onTheList.listed;
			Answer const a = Call(shared, posting ? L"POST" : L"PUT", L"/v1/games", body, hostAnswer, sizeof(hostAnswer));

			if (!a.worked)
			{
				Tell(shared, a.why);
				return false;
			}

			if (posting && a.status == 201)
			{
				if (RelayList_ReadOwner(hostAnswer, a.bytes, onTheList.owner) != kRelayListAccepted)
				{
					Tell(shared, "the answer to the game's listing was refused, so it can't be kept up");
					return false;
				}

				onTheList.listed = true;
				Tell(shared, "the game is on the list");
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

		Tell(shared, kSaidNo);
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
		Answer const a = Call(shared, L"DELETE", L"/v1/games", body, hostAnswer, sizeof(hostAnswer));

		if (!a.worked)
		{
			Tell(shared, a.why);
		}
		else if (a.status == 204 || a.status == 404)
		{
			Tell(shared, "the game is off the list");
		}
		else
		{
			Tell(shared, kSaidNo);
		}
	}

	// ---- THE VISITS. Added 2026-11-11 by Melanikus for the AvP port (the owner's ruling: Find games lists every server servers.cfg names
	// that the player has said yes to; "all servers go through the same pipeline"). On a find, this thread visits each of those lines, one
	// after another, in the file's order. A visit is a server's state made fresh from its line alone (NewServer: its address, ports and
	// fingerprint, its key not checked), with its own key checks and answers. It checks the server's key against its line (CheckTheServer),
	// asks for its games held to that key (Call), and copies them out, each tagged with its line. Then everything of it is wiped and given
	// back (ForgetTheVisit), before the next line's visit starts. Nothing outlives a visit, so one server's key, or anything one server
	// said, can never reach another's. Only lines the main thread chose are visited (RelayList_FindGames): each has had the player's yes,
	// so nothing goes to a server before its yes. The line in use keeps its own lasting state (shared) for hosting, access and the relay's
	// keys; for its games it's visited like any other line.

	Shared *NewServer(const ServersCfgServer &line);   // below, with Start
	void ForgetTheVisit(Shared *visit);                // below, with Start

	// One visit's call for the games, held to its own server's key. NULL with *count games in visit->gamesRead, or fixed words saying why not.
	const char *VisitTheLine(Shared *visit, int *count)
	{
		*count = 0;
		wchar_t path[64];
		swprintf(path, sizeof(path) / sizeof(path[0]), L"/v1/games?version=%d", visit->version);

		Answer const a = Call(visit, L"GET", path, NULL, visit->gamesAnswer, sizeof(visit->gamesAnswer));

		if (!a.worked)
		{
			return a.why;
		}

		if (a.status != 200)
		{
			return kSaidNo;
		}

		RelayListRefusal const refusal = RelayList_ReadGames(visit->gamesAnswer, a.bytes, visit->version, visit->gamesRead, count);

		if (refusal != kRelayListAccepted)
		{
			*count = 0;
			return RelayList_RefusalWords(refusal);
		}

		return NULL;
	}

	// The visit's news, into the log through the line in use's news, each piece naming the line it's about by its number in servers.cfg
	void PassOnTheNews(const Shared *visit, int lineNumber)
	{
		int const from = visit->newsNumber > kMostNews ? visit->newsNumber - kMostNews : 0;

		for (int n = from; n < visit->newsNumber; n++)
		{
			std::string const piece = std::string(SERVERS_CFG_FILE_NAME " line ") + std::to_string(lineNumber) + ": " + visit->news[n % kMostNews];
			Tell(shared, piece.c_str());
		}
	}

	void VisitEveryLine(void)
	{
		// The lines to visit, as the main thread chose them when the find was asked, each one the player has said yes to
		ServersCfgServer lines[kRelayListMostVisits];
		int places[kRelayListMostVisits];
		EnterCriticalSection(&shared->lock);
		int const count = shared->visitCount < 0 ? 0 : (shared->visitCount > kRelayListMostVisits ? kRelayListMostVisits : shared->visitCount);
		int const inUse = shared->visitInUse;
		memcpy(lines, shared->visitLines, sizeof(lines));
		memcpy(places, shared->visitPlaces, sizeof(places));
		LeaveCriticalSection(&shared->lock);

		// Each visit's games go straight into the merged answer, under the lock, and the answer is marked ready only after the last visit
		EnterCriticalSection(&shared->lock);
		shared->allReady = false;
		shared->allCount = 0;
		LeaveCriticalSection(&shared->lock);

		for (int v = 0; v < count; v++)
		{
			int const place = places[v] >= 0 && places[v] < kServersCfgMostLines ? places[v] : -1;
			Shared *const visit = place >= 0 ? NewServer(lines[v]) : NULL;
			int got = 0;
			const char *const why = visit ? VisitTheLine(visit, &got) : kCantStart;

			EnterCriticalSection(&shared->lock);

			for (int g = 0; !why && g < got && g < kRelayListMostGames && shared->allCount < kRelayListMostAllGames; g++)
			{
				shared->allGames[shared->allCount].game = visit->gamesRead[g];
				shared->allGames[shared->allCount].line = place;
				shared->allCount++;
			}

			if (place >= 0)
			{
				shared->lineStates[place] = why ? kRelayLineNoAnswer : kRelayLineAnswered;
				shared->lineWhys[place] = why;
			}

			// The line in use's games are also its own answer, for joining (RelayList_FoundGames)
			if (place >= 0 && place == inUse)
			{
				shared->count = why ? 0 : got;
				memcpy(shared->games, visit->gamesRead, sizeof(RelayListGame) * static_cast<size_t>(shared->count));
				shared->why = why;
				shared->answerReady = true;
			}

			LeaveCriticalSection(&shared->lock);

			if (visit)
			{
				PassOnTheNews(visit, lines[v].lineNumber);
				ForgetTheVisit(visit);
			}
		}

		EnterCriticalSection(&shared->lock);
		shared->allReady = true;
		LeaveCriticalSection(&shared->lock);
	}

	// Added 2026-11-09 by Melanikus for the AvP port (protocol version 2's draft, step 7): a relay's static public keys, for networking.cpp.
	// Modified 2026-11-10 (#175): they're the keys the server's check took (CheckTheServer), which its line's fingerprint has already
	// proved, so nothing more is asked for. They're handed over only for the region the check gave, the one relay the server has.
	void ReadTheRelayKeys(const char *region, int asking)
	{
		const char *why = CheckTheServer(shared);
		uint8_t p384[kNetCryptoP384PublicBytes] = {};
		uint8_t kem[kNetPqKemPublicBytes] = {};

		if (!why && strncmp(region, shared->relayRegion, sizeof(shared->relayRegion)) != 0)
		{
			why = "the relay asked about isn't the one the server in " SERVERS_CFG_FILE_NAME " has";
		}

		if (!why)
		{
			memcpy(p384, shared->relayP384, sizeof(p384));
			memcpy(kem, shared->relayKem, sizeof(kem));
		}

		EnterCriticalSection(&shared->lock);
		shared->keysAnswered = asking;
		shared->keysWhy = why;
		memcpy(shared->keysP384, p384, sizeof(p384));
		memcpy(shared->keysKem, kem, sizeof(kem));
		LeaveCriticalSection(&shared->lock);
		Tell(shared, why ? why : "the relay's keys are the ones its server's line proved");
	}

	// The answer to an access request: read as hostile (relay_list_text.h), about this PC's own fingerprint, and kept only if all of it is
	// right. Added 2026-11-09 by Melanikus for the AvP port (server #24).
	char accessAnswerText[kRelayAccessMostAnswerBytes];

	void AskForAccess(const char *body, const uint8_t fingerprint[kNetIdentityFingerprintBytes])
	{
		Answer const a = Call(shared, L"POST", L"/v1/access-requests", body, accessAnswerText, sizeof(accessAnswerText));
		RelayAccessAnswer answer = kRelayAccessWaiting;
		const char *why = a.why;

		if (a.worked)
		{
			RelayListRefusal const refusal = RelayList_ReadAccess(static_cast<int>(a.status), accessAnswerText, a.bytes, fingerprint, &answer);
			why = (refusal == kRelayListAccepted) ? NULL : RelayList_RefusalWords(refusal);
		}

		// Only fixed words go in the log: what the answer was, or why there was none
		Tell(shared, why ? why : RelayAccess_AnswerWords(answer));

		EnterCriticalSection(&shared->lock);
		shared->accessAnswered = (why == NULL);
		shared->accessAnswer = answer;
		shared->accessWhy = why;
		shared->accessReady = true;
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
			endRegion[sizeof(endRegion) - 1] = 0;   // each ends within its room, whatever the copy held (added 2026-11-09)
			endCode[sizeof(endCode) - 1] = 0;
			shared->endWanted = false;

			bool const host = shared->hostWanted;
			Listing const listing = shared->listing;
			int const number = shared->listingNumber;

			bool const relays = shared->relaysWanted;
			shared->relaysWanted = false;
			bool const find = shared->findWanted;
			shared->findWanted = false;

			// A relay's keys, for the channel to the relay (added 2026-11-09 by Melanikus for the AvP port)
			bool const keys = shared->keysWanted;
			shared->keysWanted = false;
			char keysRegion[kRelayListRegionLetters + 1];
			memcpy(keysRegion, shared->keysRegion, sizeof(keysRegion));
			keysRegion[kRelayListRegionLetters] = 0;
			int const keysAsking = shared->keysAsked;
			bool const access = shared->accessWanted;
			shared->accessWanted = false;
			char accessBody[kRelayAccessMostBodyBytes + 1];
			uint8_t accessFingerprint[kNetIdentityFingerprintBytes];
			memcpy(accessBody, shared->accessBody, sizeof(accessBody));
			memcpy(accessFingerprint, shared->accessFingerprint, sizeof(accessFingerprint));
			LeaveCriticalSection(&shared->lock);

			// The game that ended comes off the list, if it's the one on it
			if (end && onTheList.listed && SameField(endRegion, onTheList.region) && SameField(endCode, onTheList.code))
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
				if (!SameField(listing.region, onTheList.region) || !SameField(listing.code, onTheList.code))
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

			// The relay is the server's own, known once the server is checked against its line
			if (relays)
			{
				CheckTheServer(shared);
			}

			if (find)
			{
				VisitEveryLine();
			}

			if (keys)
			{
				ReadTheRelayKeys(keysRegion, keysAsking);
			}

			if (access)
			{
				AskForAccess(accessBody, accessFingerprint);
			}
		}
	}

	// Makes the store and starts the thread, the first time either is needed. False if there's no server, or it can't start. Modified
	// 2026-11-10 by Melanikus for the AvP port (#175): the server is its servers.cfg line (Relay_ServerName says when there's one).
	// THE PLAYER'S YES (#175's last part; servers_trust.h says what it is and how it's kept). Added 2026-11-10 by Melanikus for the AvP
	// port. Until the line has had it, the list isn't started (Start, below), so nothing at all goes to the line's server or its relay.
	// Only the main thread reads or changes these, and the list's thread starts only after the yes.
	ServersTrustYeses yeses;
	bool yesesRead = false;

	// (What a yes to a line is, LineHash, is with the server's own code at the top of this file, as the pick is worked out from it too.)

	// A Hudson run's yes page (yespage=, hudson_plan.h; added 2026-11-10 by Melanikus (Third) for the AvP port, for the owner's rehearsal of
	// beta.2's screens). A Hudson run's line is agreed without a yes, as nobody is there to answer, unless the run asks to see the yes page
	// as a player would (HudsonTest_RelayListShowsTheYesPage, below). Then it's agreed only after the run's own press of Yes, which is held
	// here, for this run alone: never added to the kept yeses, and never written to servers-trusted.cfg, so a test run can't leave a yes
	// for the player of this PC. Both start false, and only a Hudson run ever sets them.
	bool hudsonRunShowsTheYesPage = false;
	bool hudsonRunSaidYes = false;

	// Whether a line may be used: a Hudson run's always (nobody is there to answer), or with yespage= only after its own Yes this run;
	// anyone else's only once the player has said yes to it. The kept yeses are read the first time this is asked; a file that can't be
	// used counts as none, so every line is asked about again.
	bool TheLineIsAgreed(const ServersCfgServer &line)
	{
		if (HudsonRunIsOn)
		{
			return !hudsonRunShowsTheYesPage || hudsonRunSaidYes;
		}

		if (!yesesRead)
		{
			yesesRead = true;
			const char *why = NULL;
			NetTrustStoreResult const kept = NetIdentityStore_LoadTrust(&yeses, &why);

			if (kept != kNetTrustStoreRead && kept != kNetTrustStoreNoFile)
			{
				LogString(std::string("Relay list: " SERVERS_TRUST_FILE_NAME " isn't used, so every server is asked about again: ") + (why ? why : "it couldn't be read"));
			}
		}

		uint8_t hash[kServersTrustHashBytes];
		return LineHash(line, hash) && ServersTrust_Has(yeses, hash);
	}

	// A server's state, made fresh from its servers.cfg line alone: for the line in use (Start) and for each visit (VisitEveryLine). Added
	// 2026-11-11 by Melanikus for the AvP port: it was Start's own, and the visits are made the same way, so a server is written in this
	// one place. NULL if it can't be made. Its key isn't checked yet, so only its first call may go (CallChecked).
	Shared *NewServer(const ServersCfgServer &line)
	{
		Shared *made = new (std::nothrow) Shared;

		if (!made)
		{
			return NULL;
		}

		memset(made, 0, sizeof(*made));
		InitializeCriticalSection(&made->lock);

		// The address as WinHTTP takes it, written out from the line's four numbers: digits and dots only, so never a name it could look up
		swprintf(made->server, sizeof(made->server) / sizeof(made->server[0]), L"%u.%u.%u.%u", (line.address >> 24) & 0xFFu,
			(line.address >> 16) & 0xFFu, (line.address >> 8) & 0xFFu, line.address & 0xFFu);
		made->port = static_cast<INTERNET_PORT>(line.listPort);
		memcpy(made->lineFingerprint, line.fingerprint, sizeof(made->lineFingerprint));

		// The relay, at the same address: as ENetAddress.host keeps it, the first number in the lowest byte (network order on x86)
		made->relayAddress = htonl(line.address);
		made->relayPort = line.relayPort;
		made->keyState = kKeyNotChecked;

		// The version is fixed for the run (added 2026-11-09 by Melanikus for the AvP port; networking.h)
		made->version = kMultiplayerVersion;
		return made;
	}

	// A visit's end: everything of it wiped, then given back, so nothing of one server is left for the next (added 2026-11-11)
	void ForgetTheVisit(Shared *visit)
	{
		DeleteCriticalSection(&visit->lock);
		SecureZeroMemory(visit, sizeof(*visit));
		delete visit;
	}

	bool Start(void)
	{
		if (shared)
		{
			return true;
		}

		if (!Relay_ServerName()[0])
		{
			return false;
		}

		// Nothing is made, and nothing goes to the line's server or its relay, before the player's yes to the line (#175's last part)
		const ServersCfgServer *const line = Server();

		if (!line || !TheLineIsAgreed(*line))
		{
			return false;
		}

		Shared *made = NewServer(*line);

		if (!made)
		{
			return false;
		}

		made->wake = CreateEventW(NULL, FALSE, FALSE, NULL);
		made->endDone = CreateEventW(NULL, FALSE, FALSE, NULL);

		// The thread starts only once its two events are made. Modified 2026-11-10 by Melanikus for the AvP port (#175 2b): when either
		// step fails, everything made for the list is given back in this one place, whichever step it was. It used to be left behind, with
		// its lock and any event made, and the menus ask again with every Find games, so a PC short of handles lost more each time.
		HANDLE thread = NULL;

		if (made->wake && made->endDone)
		{
			shared = made;
			thread = CreateThread(NULL, 0, ListThread, NULL, 0, NULL);
		}

		if (!thread)
		{
			LogString("Relay list: couldn't start, so hosts aren't listed and the list can't be read");
			shared = NULL;

			// No thread was made, so nothing else holds any of this
			if (made->wake)
			{
				CloseHandle(made->wake);
			}

			if (made->endDone)
			{
				CloseHandle(made->endDone);
			}

			DeleteCriticalSection(&made->lock);
			delete made;
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

	// This run's identity's whole id, from RelayList_HostIdentity, and whether one has been given. Only the main thread uses these.
	uint8_t hostIdentity[kNetIdentityIdBytes];
	bool hostIdentityGiven = false;
}

// Modified 2026-11-10 by Melanikus for the AvP port (#175): the server's name is its servers.cfg line's, and there's no server setting in
// AliensVsPredator.cfg at all, which is a text file people share and anyone can edit (tests/settings_file_test.cpp). Modified again the
// same day (#175 PR 3b): every game has the file's first good line as its server, with -dev or without, as protocol version 2 is on for
// players. What keeps it safe is the line itself: the server is trusted by its fingerprint alone (CheckTheServer, above).
const char *Relay_ServerName(void)
{
	const ServersCfgServer *const line = Server();
	return line ? line->name : "";
}

// Added 2026-11-09 by Melanikus for the AvP port: see relay_list.h
void RelayList_HostIdentity(const uint8_t hostId[kNetIdentityIdBytes])
{
	hostIdentityGiven = false;

	if (!hostId)
	{
		return;
	}

	memcpy(hostIdentity, hostId, sizeof(hostIdentity));

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

	// A listing without the host's id would send its joiners to a host they can't check, so it isn't sent at all (modified 2026-11-10 by
	// Melanikus for the AvP port, #175 PR 3a: every listing, since version 1's, which had no id, went).
	if (!hostIdentityGiven)
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
	memcpy(l.hostId, hostIdentity, sizeof(l.hostId));
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

// Modified 2026-11-10 by Melanikus for the AvP port (#175): the server's one relay, at its line's address and relay port, with the region the
// server's check gave. Known only once the server is checked against its line and good.
bool RelayList_Relays(RelayListRelay *relays, int *count)
{
	*count = 0;

	if (!shared)
	{
		return false;
	}

	EnterCriticalSection(&shared->lock);
	bool const known = shared->keyState == kKeyGood;

	if (known)
	{
		memset(&relays[0], 0, sizeof(relays[0]));
		memcpy(relays[0].region, shared->relayRegion, sizeof(relays[0].region));
		snprintf(relays[0].host, sizeof(relays[0].host), "%ls", shared->server);
		relays[0].port = shared->relayPort;
		relays[0].address = shared->relayAddress;
		*count = 1;
	}

	LeaveCriticalSection(&shared->lock);
	return known;
}

// Added 2026-11-10 by Melanikus for the AvP port (#188): see relay_list.h. A list that hasn't started has asked nothing yet.
int RelayList_ServerState(const char **why)
{
	*why = NULL;

	// Added 2026-11-10 by Melanikus for the AvP port (#175's last part): a line the player hasn't said yes to is never asked, so there's
	// no answer to wait for, only the yes
	const ServersCfgServer *const line = Server();

	if (line && !TheLineIsAgreed(*line))
	{
		return kRelayServerNeedsYes;
	}

	if (!shared)
	{
		return kRelayServerAsking;
	}

	EnterCriticalSection(&shared->lock);
	int state = kRelayServerAsking;

	if (shared->keyState == kKeyGood)
	{
		state = kRelayServerGood;
	}
	else if (shared->keyState == kKeyRefused)
	{
		state = kRelayServerCantUse;
		*why = kKeysDontMatch;
	}
	else if (shared->serverWhy)
	{
		state = kRelayServerCantUse;
		*why = shared->serverWhy;
	}

	LeaveCriticalSection(&shared->lock);
	return state;
}

// A line as the yes page and the server lists show it: its name, its whole address, where it is, and its key's two ends. False, with out
// wiped, if the address couldn't be shown whole. Added 2026-11-10 by Melanikus for the AvP port (#175 2b): it was RelayList_LineToAgree's
// own, and the server lists show their lines the same way.
static bool ShowTheLine(const ServersCfgServer *line, RelayListLineToAgree *out)
{
	memset(out, 0, sizeof(*out));

	// The address as the line says it, with its two ports only when they aren't the usual ones, whole or not at all (relay_list_text.h;
	// modified 2026-11-10 by Melanikus for the AvP port: it was written here, into 24 bytes, which could cut an address with both ports).
	// It always fits. If it ever didn't, there'd be no page to show, and still nothing would go to the server: the list starts only after
	// the yes (Start). It's written first, so out is still all 0 if it's refused.
	if (!RelayList_ShownAddress(line->address, line->listPort, line->relayPort, out->address, sizeof(out->address)))
	{
		return false;
	}

	snprintf(out->name, sizeof(out->name), "%s", line->name);
	out->place = line->place;

	// The fingerprint's first 8 digits and its last 8, as a person can read them against what the server's owner gave out
	static char const kDigits[] = "0123456789abcdef";
	char hex[2 * kServersCfgFingerprintBytes + 1];

	for (int i = 0; i < kServersCfgFingerprintBytes; i++)
	{
		hex[2 * i] = kDigits[line->fingerprint[i] >> 4];
		hex[2 * i + 1] = kDigits[line->fingerprint[i] & 0x0F];
	}

	hex[2 * kServersCfgFingerprintBytes] = 0;
	snprintf(out->keyStart, sizeof(out->keyStart), "%.8s", hex);
	snprintf(out->keyEnd, sizeof(out->keyEnd), "%.8s", hex + 2 * kServersCfgFingerprintBytes - 8);
	return true;
}

// Added 2026-11-10 by Melanikus for the AvP port (#175's last part): see relay_list.h
bool RelayList_LineToAgree(RelayListLineToAgree *out)
{
	const ServersCfgServer *const line = Server();

	if (!out || !line || TheLineIsAgreed(*line))
	{
		return false;
	}

	return ShowTheLine(line, out);
}

// Added 2026-11-10 by Melanikus for the AvP port (#175's last part): see relay_list.h
bool RelayList_AgreeToTheLine(void)
{
	const ServersCfgServer *const line = Server();
	uint8_t hash[kServersTrustHashBytes];

	if (!line || !LineHash(*line, hash))
	{
		return false;
	}

	// A Hudson run's Yes (yespage=, above) holds for that run alone. It goes nowhere near the kept yeses or their file, so this PC's
	// player is still asked. Added 2026-11-10 by Melanikus (Third) for the AvP port.
	if (HudsonRunIsOn)
	{
		hudsonRunSaidYes = true;
		LogString("Relay list: the Hudson run said yes to the server in " SERVERS_CFG_FILE_NAME ", for this run only: nothing is kept");
		return true;
	}

	// The kept yeses are read first, so that writing the file again keeps them. With every place taken, the oldest yes goes, and its line
	// is asked about again the next time it's used.
	TheLineIsAgreed(*line);

	if (!ServersTrust_Has(yeses, hash) && yeses.count >= kServersTrustMostYeses)
	{
		memmove(yeses.hashes[0], yeses.hashes[1], sizeof(yeses.hashes[0]) * (kServersTrustMostYeses - 1));
		yeses.count = kServersTrustMostYeses - 1;
	}

	ServersTrust_Add(&yeses, hash);

	if (NetIdentityStore_KeepTrust(yeses))
	{
		LogString("Relay list: the player said yes to the server in " SERVERS_CFG_FILE_NAME ", and it's kept for the next time");
	}
	else
	{
		LogString("Relay list: the player said yes to the server in " SERVERS_CFG_FILE_NAME ", but it couldn't be kept, so it holds for this run only");
	}

	return true;
}

// ---- Every servers.cfg line, and the player's pick of one (#175 2b). Added 2026-11-10 by Melanikus for the AvP port: see relay_list.h.
// These only read what servers.cfg said when it was read (Server, at the top of this file), and the pick; nothing here contacts anything,
// and nothing here changes the line this run uses.

int RelayList_LineCount(void)
{
	Server();
	return theFile.serverCount;
}

bool RelayList_LineAt(int place, RelayListLineShown *out)
{
	if (!out)
	{
		return false;
	}

	memset(out, 0, sizeof(*out));
	Server();

	if (place < 0 || place >= theFile.serverCount)
	{
		return false;
	}

	ServersCfgServer const &line = theFile.servers[place];

	if (!ShowTheLine(&line, &out->line))
	{
		memset(out, 0, sizeof(*out));
		return false;
	}

	out->lineNumber = line.lineNumber;
	out->inUse = place == theServer.inUse;
	out->nextStart = place == ServersTrust_LineToUse(theServer.hashes, theServer.hashed, theFile.serverCount, theServer.pickNow);
	out->agreed = TheLineIsAgreed(line);
	return true;
}

int RelayList_RefusedLineCount(void)
{
	Server();
	return theFile.refusedCount;
}

bool RelayList_RefusedLineAt(int place, int *lineNumber, const char **words)
{
	if (lineNumber)
	{
		*lineNumber = 0;
	}

	if (words)
	{
		*words = NULL;
	}

	Server();

	if (!lineNumber || !words || place < 0 || place >= theFile.refusedCount)
	{
		return false;
	}

	// The line's number and the game's own words for why, never anything from the line, which could say anything at all
	*lineNumber = theFile.refused[place].lineNumber;
	*words = ServersCfg_LineWords(theFile.refused[place].why);
	return true;
}

const char *RelayList_FileWords(void)
{
	Server();

	if (theServer.opening != kServersCfgOpened)
	{
		return ServersCfg_OpeningWords(theServer.opening);
	}

	return theFile.fileRefusal == kServersCfgFileRead ? NULL : ServersCfg_FileWords(theFile.fileRefusal);
}

// The pick is kept for the next start. The line in use, theServer.line, is never changed here: it was set once, when servers.cfg was read,
// and what the list's thread holds of it (its checked key, the relay's region and keys) stays with it until the game starts again.
bool RelayList_PickLine(int place)
{
	Server();

	if (place < 0 || place >= theFile.serverCount || !theServer.hashed[place])
	{
		return false;
	}

	std::string const which = SERVERS_CFG_FILE_NAME " line " + std::to_string(theFile.servers[place].lineNumber);

	if (!NetIdentityStore_KeepPick(theServer.hashes[place]))
	{
		LogString("Relay list: the player picked " + which + ", but the pick couldn't be kept, so the next start uses the line it would have");
		return false;
	}

	theServer.pickNow.there = true;
	memcpy(theServer.pickNow.hash, theServer.hashes[place], kServersTrustHashBytes);
	LogString("Relay list: the player picked " + which + ", for the next start");
	return true;
}

// yespage= (relay_list.h): Hudson's own switch, which only Hudson's files call (tools/check-hudson-asleep.ps1, rule 5), and only in a
// run. Outside one it changes nothing, as TheLineIsAgreed reads it only for a Hudson run. Added 2026-11-10 by Melanikus (Third) for the
// AvP port.
void HudsonTest_RelayListShowsTheYesPage(void)
{
	hudsonRunShowsTheYesPage = true;
}

void RelayList_FindGames(void)
{
	if (!Start())
	{
		noListAnswerWaiting = true;
		return;
	}

	// The lines to visit (added 2026-11-11, THE VISITS): every good line the player has said yes to, in the file's order, at most
	// kRelayListMostVisits of them. A line without its yes isn't visited, so nothing goes to its server. The yeses are the main thread's,
	// so they're asked here, and the list's thread is handed copies of the lines.
	ServersCfgServer lines[kRelayListMostVisits];
	int places[kRelayListMostVisits];
	int states[kServersCfgMostLines];
	int visits = 0;

	for (int place = 0; place < theFile.serverCount && place < kServersCfgMostLines; place++)
	{
		if (!TheLineIsAgreed(theFile.servers[place]))
		{
			states[place] = kRelayLineNoYes;
		}
		else if (visits >= kRelayListMostVisits)
		{
			states[place] = kRelayLineNotAsked;
		}
		else
		{
			lines[visits] = theFile.servers[place];
			places[visits] = place;
			visits++;
			states[place] = kRelayLineAsking;
		}
	}

	EnterCriticalSection(&shared->lock);
	shared->visitCount = visits;
	memcpy(shared->visitLines, lines, sizeof(ServersCfgServer) * static_cast<size_t>(visits));
	memcpy(shared->visitPlaces, places, sizeof(int) * static_cast<size_t>(visits));
	shared->visitInUse = theServer.inUse;

	for (int place = 0; place < theFile.serverCount && place < kServersCfgMostLines; place++)
	{
		shared->lineStates[place] = states[place];
		shared->lineWhys[place] = NULL;
	}

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

// Added 2026-11-11 by Melanikus for the AvP port (THE VISITS): see relay_list.h
bool RelayList_FoundAllGames(RelayListServerGame *games, int *count)
{
	*count = 0;

	if (!shared || !games)
	{
		return false;
	}

	WriteTheNews();

	EnterCriticalSection(&shared->lock);
	bool const ready = shared->allReady;

	if (ready)
	{
		shared->allReady = false;
		*count = shared->allCount < 0 ? 0 : (shared->allCount > kRelayListMostAllGames ? kRelayListMostAllGames : shared->allCount);
		memcpy(games, shared->allGames, sizeof(RelayListServerGame) * static_cast<size_t>(*count));
	}

	LeaveCriticalSection(&shared->lock);
	return ready;
}

int RelayList_LineGamesState(int place, const char **why)
{
	if (why)
	{
		*why = NULL;
	}

	Server();

	if (place < 0 || place >= theFile.serverCount || place >= kServersCfgMostLines)
	{
		return kRelayLineNotAsked;
	}

	// Before any find, a line stands as it would be asked: by its yes
	if (!shared)
	{
		return TheLineIsAgreed(theFile.servers[place]) ? kRelayLineAsking : kRelayLineNoYes;
	}

	EnterCriticalSection(&shared->lock);
	int const state = shared->lineStates[place];

	if (why)
	{
		*why = shared->lineWhys[place];
	}

	LeaveCriticalSection(&shared->lock);
	return state;
}

// Added 2026-11-09 by Melanikus for the AvP port (protocol version 2's draft, step 7): see relay_list.h
bool RelayList_AskForRelayKeys(const char *region)
{
	if (!region || !RelayList_IsRegion(region) || !Start())
	{
		return false;
	}

	EnterCriticalSection(&shared->lock);
	shared->keysWanted = true;
	// The region is at most kRelayListRegionLetters (RelayList_IsRegion, above); it's copied no further than that, and ended (Snyk Code)
	strncpy(shared->keysRegion, region, kRelayListRegionLetters);
	shared->keysRegion[kRelayListRegionLetters] = 0;
	shared->keysAsked++;
	LeaveCriticalSection(&shared->lock);

	Wake();
	return true;
}

int RelayList_RelayKeysCame(uint8_t p384[kNetCryptoP384PublicBytes], uint8_t kem[kNetPqKemPublicBytes], const char **why)
{
	memset(p384, 0, kNetCryptoP384PublicBytes);
	memset(kem, 0, kNetPqKemPublicBytes);
	*why = NULL;

	if (!shared)
	{
		*why = "no relay server is set";
		return kRelayKeysRefused;
	}

	EnterCriticalSection(&shared->lock);
	int came = kRelayKeysWaiting;

	// Only the answer to the newest asking counts: one to an older asking is still waiting, as far as this one goes
	if (shared->keysAsked > 0 && shared->keysAnswered == shared->keysAsked)
	{
		came = shared->keysWhy ? kRelayKeysRefused : kRelayKeysCame;
		*why = shared->keysWhy;

		if (came == kRelayKeysCame)
		{
			memcpy(p384, shared->keysP384, kNetCryptoP384PublicBytes);
			memcpy(kem, shared->keysKem, kNetPqKemPublicBytes);
		}
	}

	LeaveCriticalSection(&shared->lock);
	return came;
}

// Removed 2026-11-10 by Melanikus for the AvP port (#175): RelayList_RelayKeyFingerprint and the -dev setting it read, RelayKey. The relay's
// keys are proved by the server's servers.cfg line now (CheckTheServer), so nothing is read from AliensVsPredator.cfg about a server.

// Modified 2026-11-09 by Melanikus for the AvP port: see relay_list.h (the page of games found's failure screen)
bool RelayList_WhyIsNoConnection(const char *why)
{
	// The very words this file hands out, compared as the same words (each is one of the constants above), so no rewording elsewhere
	// can make it say something else
	return why == kCantReach || why == kTooLong || why == kCertificate || why == kCantStart || why == kNoChainOff;
}

// ---- Asking for online access (server #24). Added 2026-11-09 by Melanikus for the AvP port.

bool RelayList_AskForAccess(const char *name, const uint8_t statics[kNetIdentityStaticBytes], const uint8_t fingerprint[kNetIdentityFingerprintBytes])
{
	// Without the fingerprint the answer has to be about, nothing. (Modified 2026-11-10 by Melanikus for the AvP port, #175 PR 3b: -dev
	// decides nothing online, so this no longer asks it. Only the Online access page calls this, and the page has its own way in.)
	if (!fingerprint)
	{
		return false;
	}

	// Modified 2026-11-10 by Melanikus for the AvP port (PO's ruling): an access request goes only to the server a servers.cfg line names,
	// as only then is the list's key checked before a byte is sent: Call (above) runs CheckTheServer first, and CallChecked closes the call
	// if the key the list shows isn't the one the line's fingerprint matched. The 8-letter access code is safe only then (relay_list_text.h,
	// RelayAccess_Code): to a list Windows merely trusts, someone on the way could drop the player's request and send one with a key made
	// to match its code instead. With no line, nothing is made or sent, and the page says why (access_page.cpp, Ask). With a line the
	// player hasn't said yes to, Start (above) refuses, so no request goes before the yes either.
	if (!Server())
	{
		LogString("Relay list: an access request goes only to the server a " SERVERS_CFG_FILE_NAME " line names, and none does, so nothing was sent");
		return false;
	}

	// The body is made here, on the game's thread, so a name or key that can't be sent is refused before anything starts
	char body[kRelayAccessMostBodyBytes + 1];

	if (!RelayList_AccessBody(name, statics, body, sizeof(body)) || !Start())
	{
		return false;
	}

	EnterCriticalSection(&shared->lock);
	memcpy(shared->accessBody, body, sizeof(body));
	memcpy(shared->accessFingerprint, fingerprint, kNetIdentityFingerprintBytes);
	shared->accessWanted = true;
	LeaveCriticalSection(&shared->lock);

	Wake();
	return true;
}

bool RelayList_AccessCame(bool *answered, RelayAccessAnswer *answer, const char **why)
{
	*answered = false;
	*answer = kRelayAccessWaiting;
	*why = NULL;

	if (!shared)
	{
		return false;
	}

	WriteTheNews();

	EnterCriticalSection(&shared->lock);
	bool const ready = shared->accessReady;

	if (ready)
	{
		shared->accessReady = false;
		*answered = shared->accessAnswered;
		*answer = shared->accessAnswer;
		*why = shared->accessWhy;
	}

	LeaveCriticalSection(&shared->lock);
	return ready;
}
