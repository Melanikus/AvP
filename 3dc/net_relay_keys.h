// A relay's static public keys, and their fingerprint, the relay's id (protocol version 2's draft, step 7; CRYPTO.md 12.1).
// Written 2026-11-09 by Melanikus for the AvP port.
// Modified 2026-11-10 by Melanikus for the AvP port (#175):
//   the keys are proved by the server's servers.cfg line now (net_server_fingerprint.h, which this id is part of), so the -dev setting
//   RelayKey and its reader are gone, with the check that held the keys to it
//
// WHY. Handshake A proves the relay to the game, but only if the game holds the relay's real static public keys: whoever holds the
// private halves of the keys the game uses is "the relay" to it. Until there's a signed directory of the relays' keys (a later card), the
// game fetches them from the list server (GET /v1/relay-keys, relay_list.cpp), and they're taken only if they and the list's own key make
// the fingerprint in the server's servers.cfg line. This id is the relay's half of that.
//
// THE ID is the whole of SHA-384 (48 bytes) over "AVPX relay keys v2" (its 18 letters, with no 0 after them), then the P-384 key's 97
// bytes, then the ML-KEM-1024 key's 1,568: 1,683 bytes in all. It's all 48 bytes, not a short piece of them, because a machine compares it
// (PO's ruling, 2026-11-09: every comparison a machine makes uses the full digest, and only what people read is short). For
// crypto-v2.json's handshakeA.relayStatic it's
// fc352802066e547c261e37fe63f114863debbc76b5b29575ab69af80c72eeb91dd9a9dddd8b38218b21df00b4bea5a54; its first 32 digits are the vector the
// relay server's test was first held to (Primary, 2026-11-09).
//
// It's game-free: tests/net_relay_keys_test.cpp tries every case.
#ifndef NET_RELAY_KEYS_H
#define NET_RELAY_KEYS_H

#include <stddef.h>
#include <stdint.h>

#include "net_crypto.h"
#include "net_handshake_a.h"   // NetRelayStaticKeys

char const kNetRelayKeysWords[] = "AVPX relay keys v2";
size_t const kNetRelayKeysFingerprintBytes = kNetCryptoHashBytes;   // all 48 bytes of SHA-384

// The id of a relay's keys. False (with out wiped) only if SHA-384 itself failed.
inline bool NetRelayKeys_Fingerprint(const NetRelayStaticKeys *keys, uint8_t out[kNetRelayKeysFingerprintBytes])
{
	NetCryptoPiece const pieces[3] = {
		{ reinterpret_cast<const uint8_t *>(kNetRelayKeysWords), sizeof(kNetRelayKeysWords) - 1 },
		{ keys->p384, kNetCryptoP384PublicBytes },
		{ keys->kem, kNetPqKemPublicBytes },
	};

	if (!NetCrypto_Sha384(pieces, 3, out))
	{
		NetCrypto_Wipe(out, kNetRelayKeysFingerprintBytes);
		return false;
	}

	return true;
}

#endif
