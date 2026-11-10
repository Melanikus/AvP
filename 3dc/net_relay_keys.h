// A relay's static public keys, and the check that they really are that relay's (protocol version 2's draft, step 7; CRYPTO.md 12.1).
// Written 2026-11-09 by Melanikus for the AvP port.
//
// WHY. Handshake A proves the relay to the game, but only if the game holds the relay's real static public keys: whoever holds the
// private halves of the keys the game uses is "the relay" to it. Until there's a signed directory of the relays' keys (a later card), the
// game fetches them from the list server (GET /v1/relay-keys, relay_list.cpp) and holds them to a fingerprint a tester types into the
// -dev settings: RelayKey=<region>:<96 hex digits>. Keys whose fingerprint isn't that one, or keys for a region with no fingerprint given,
// are never used: nothing is sent to that relay, and there's no going back to version 1.
//
// THE FINGERPRINT is the whole of SHA-384 (48 bytes) over "AVPX relay keys v2" (its 18 letters, with no 0 after them), then the P-384
// key's 97 bytes, then the ML-KEM-1024 key's 1,568: 1,683 bytes in all. It's written as 96 hex digits in small letters. It's all 48 bytes,
// not a short piece of them, because a machine compares it (PO's ruling, 2026-11-09: every comparison a machine makes uses the full
// digest, and only what people read is short). For crypto-v2.json's handshakeA.relayStatic it's
// fc352802066e547c261e37fe63f114863debbc76b5b29575ab69af80c72eeb91dd9a9dddd8b38218b21df00b4bea5a54; its first 32 digits are the vector the
// relay server's test was first held to (Primary, 2026-11-09).
//
// It's game-free: tests/net_relay_keys_test.cpp tries every case.
#ifndef NET_RELAY_KEYS_H
#define NET_RELAY_KEYS_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "net_crypto.h"
#include "net_handshake_a.h"   // NetRelayStaticKeys
#include "relay_list_text.h"   // RelayList_IsRegion and kRelayListRegionLetters: the one rule for a region

char const kNetRelayKeysWords[] = "AVPX relay keys v2";
size_t const kNetRelayKeysFingerprintBytes = kNetCryptoHashBytes;                  // all 48 bytes of SHA-384
size_t const kNetRelayKeysFingerprintHexLetters = 2 * kNetRelayKeysFingerprintBytes;   // 96

// The most letters the setting can have: a region's 16, the colon, and 96 hex digits
size_t const kNetRelayKeysMostSettingLetters = kRelayListRegionLetters + 1 + kNetRelayKeysFingerprintHexLetters;

// The fingerprint of a relay's keys. False (with out wiped) only if SHA-384 itself failed.
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

// A fingerprint from its text: exactly 96 hex digits in small letters, and nothing after them. Only then is out written, so a text that's
// refused leaves out all 0.
inline bool NetRelayKeys_FingerprintFromHex(const char *text, uint8_t out[kNetRelayKeysFingerprintBytes])
{
	memset(out, 0, kNetRelayKeysFingerprintBytes);

	if (!text)
	{
		return false;
	}

	// The loop stops at the first letter that isn't a hex digit, so it never reads past a shorter text's end
	for (size_t i = 0; i < kNetRelayKeysFingerprintHexLetters; i++)
	{
		char const c = text[i];

		if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
		{
			return false;
		}
	}

	if (text[kNetRelayKeysFingerprintHexLetters] != '\0')
	{
		return false;
	}

	for (size_t i = 0; i < kNetRelayKeysFingerprintBytes; i++)
	{
		char const high = text[2 * i];
		char const low = text[2 * i + 1];
		int const h = high <= '9' ? high - '0' : high - 'a' + 10;
		int const l = low <= '9' ? low - '0' : low - 'a' + 10;
		out[i] = static_cast<uint8_t>((h << 4) | l);
	}

	return true;
}

// The -dev setting, "<region>:<96 hex digits>". The region is the list's kind (1 to 16 small letters, digits and -), and the digits are
// small letters with nothing after them. False, with region "" and fingerprint all 0, for anything else: a setting the game can't be sure
// of is no setting.
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

	if (!RelayList_IsRegion(named) || !NetRelayKeys_FingerprintFromHex(colon + 1, read))
	{
		return false;
	}

	memcpy(region, named, regionLetters + 1);
	memcpy(fingerprint, read, kNetRelayKeysFingerprintBytes);
	return true;
}

#endif
