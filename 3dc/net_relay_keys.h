// A relay's static public keys, and the check that they really are that relay's (protocol version 2's draft, step 7; CRYPTO.md 12.1).
// Written 2026-11-09 by Melanikus for the AvP port.
//
// WHY. Handshake A proves the relay to the game, but only if the game holds the relay's real static public keys: whoever holds the
// private halves of the keys the game uses is "the relay" to it. Until there's a signed directory of the relays' keys (a later card), the
// game fetches them from the list server (GET /v1/relay-keys, relay_list.cpp) and holds them to a fingerprint a tester types into the
// -dev settings: RelayKey=<region>:<32 hex digits>. Keys whose fingerprint isn't that one, or keys for a region with no fingerprint given,
// are never used: nothing is sent to that relay, and there's no going back to version 1.
//
// THE FINGERPRINT is the first 16 bytes of SHA-384 over "AVPX relay keys v2" (its 18 letters, with no 0 after them), then the P-384 key's
// 97 bytes, then the ML-KEM-1024 key's 1,568: 1,683 bytes in all. It's written as 32 hex digits in small letters. The relay server's own
// test is held to the same vector: fc352802066e547c261e37fe63f11486 for crypto-v2.json's handshakeA.relayStatic (Primary, 2026-11-09).
//
// It's game-free: tests/net_relay_keys_test.cpp tries every case.
#ifndef NET_RELAY_KEYS_H
#define NET_RELAY_KEYS_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "net_crypto.h"
#include "net_identity.h"      // NetIdentity_FingerprintFromHex: the one rule for 32 hex digits
#include "net_handshake_a.h"   // NetRelayStaticKeys
#include "relay_list_text.h"   // RelayList_IsRegion and kRelayListRegionLetters: the one rule for a region

char const kNetRelayKeysWords[] = "AVPX relay keys v2";
size_t const kNetRelayKeysFingerprintBytes = 16;

// The most letters the setting can have: a region's 16, the colon, and 32 hex digits
size_t const kNetRelayKeysMostSettingLetters = kRelayListRegionLetters + 1 + 2 * kNetRelayKeysFingerprintBytes;

static_assert(kNetRelayKeysFingerprintBytes == kNetIdentityFingerprintBytes, "a relay's fingerprint is written the way an identity's is");

// The fingerprint of a relay's keys. False (with out wiped) only if SHA-384 itself failed.
inline bool NetRelayKeys_Fingerprint(const NetRelayStaticKeys *keys, uint8_t out[kNetRelayKeysFingerprintBytes])
{
	NetCryptoPiece const pieces[3] = {
		{ reinterpret_cast<const uint8_t *>(kNetRelayKeysWords), sizeof(kNetRelayKeysWords) - 1 },
		{ keys->p384, kNetCryptoP384PublicBytes },
		{ keys->kem, kNetPqKemPublicBytes },
	};
	uint8_t whole[kNetCryptoHashBytes];

	if (!NetCrypto_Sha384(pieces, 3, whole))
	{
		NetCrypto_Wipe(out, kNetRelayKeysFingerprintBytes);
		return false;
	}

	memcpy(out, whole, kNetRelayKeysFingerprintBytes);
	return true;
}

// Whether a relay's keys are the ones a fingerprint names: their fingerprint is worked out, then compared byte by byte, every byte
// whatever it finds. False if they aren't, or if SHA-384 itself failed.
inline bool NetRelayKeys_AreThese(const NetRelayStaticKeys *keys, const uint8_t fingerprint[kNetRelayKeysFingerprintBytes])
{
	uint8_t made[kNetRelayKeysFingerprintBytes];

	if (!keys || !fingerprint || !NetRelayKeys_Fingerprint(keys, made))
	{
		return false;
	}

	uint8_t differ = 0;

	for (size_t i = 0; i < kNetRelayKeysFingerprintBytes; i++)
	{
		differ |= static_cast<uint8_t>(made[i] ^ fingerprint[i]);
	}

	return differ == 0;
}

// The -dev setting, "<region>:<32 hex digits>", such as "newyork:fc352802066e547c261e37fe63f11486". The region is the list's kind (1 to
// 16 small letters, digits and -), and the digits are small letters with nothing after them. False, with region "" and fingerprint all 0,
// for anything else: a setting the game can't be sure of is no setting.
inline bool NetRelayKeys_ReadSetting(const char *setting, char region[kRelayListRegionLetters + 1], uint8_t fingerprint[kNetRelayKeysFingerprintBytes])
{
	region[0] = '\0';
	memset(fingerprint, 0, kNetRelayKeysFingerprintBytes);

	if (!setting)
	{
		return false;
	}

	// Read no further than a setting this long could go, so one with no end is never read past
	size_t const letters = strnlen(setting, kNetRelayKeysMostSettingLetters + 1);

	if (letters > kNetRelayKeysMostSettingLetters)
	{
		return false;
	}

	const char *const colon = static_cast<const char *>(memchr(setting, ':', letters));

	if (!colon)
	{
		return false;
	}

	size_t const regionLetters = static_cast<size_t>(colon - setting);

	if (regionLetters == 0 || regionLetters > kRelayListRegionLetters)
	{
		return false;
	}

	char named[kRelayListRegionLetters + 1];
	memcpy(named, setting, regionLetters);
	named[regionLetters] = '\0';
	uint8_t read[kNetRelayKeysFingerprintBytes];

	if (!RelayList_IsRegion(named) || !NetIdentity_FingerprintFromHex(colon + 1, read))
	{
		return false;
	}

	memcpy(region, named, regionLetters + 1);
	memcpy(fingerprint, read, kNetRelayKeysFingerprintBytes);
	return true;
}

#endif
