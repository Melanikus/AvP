// A server's servers.cfg fingerprint: the one number in a servers.cfg line that proves the server is the right one (#175; the server's
// CRYPTO.md, "the servers.cfg fingerprint"). Written 2026-11-10 by Melanikus for the AvP port.
//
// WHAT IT IS. A server has two keys: its list's (the public key in the list's TLS certificate) and its relay's (the relay's two static
// public keys, whose 48-byte id is net_relay_keys.h's fingerprint). The line's one number covers both:
//   SHA-256("AVPx servers.cfg line 1" || one 0 byte || the list certificate's SubjectPublicKeyInfo, as DER || the relay's 48-byte id)
// "line 1" is the format's version: a later format would say "line 2". Nothing needs a length in between: the words end at their 0,
// DER says its own length, and the id is always 48 bytes.
//
// WHY THE KEY, NOT THE CERTIFICATE. The owner's ruling, "trust the pin alone": the certificate's issuer, names and dates aren't looked
// at, so nothing expires out from under a player in twenty years, and a server can renew its certificate with the same key without a new
// line. relay_list.cpp takes the SubjectPublicKeyInfo of the certificate it's served, asks for the relay's keys over that same connection,
// and calls NetServerFingerprint_IsTheLines. Until it's true, nothing more is sent to that server.
//
// THE KNOWN ANSWER (tests/net_relay_keys_test.cpp, TestTheServersCfgFingerprint, and the server's CRYPTO.md): nyc3's list key and relay
// id of 2026-11-10 give bb0e2958dade79033a90f34811c84837b404708cc004c5b1d37d9a26f28d663e.
//
// It's game-free: that test tries every case. (Modified 2026-11-10 by Melanikus for the AvP port: it named a test file that was never
// written, net_server_fingerprint_test.cpp; Primary's review of #175, N-3.)

#ifndef NET_SERVER_FINGERPRINT_H
#define NET_SERVER_FINGERPRINT_H

#include <stddef.h>
#include <stdint.h>

#include "net_crypto.h"
#include "net_relay_keys.h"   // NetRelayKeys_Fingerprint, the relay's id

char const kNetServerFingerprintWords[] = "AVPx servers.cfg line 1";
size_t const kNetServerFingerprintBytes = kNetCryptoSha256Bytes;   // 32

// The most a SubjectPublicKeyInfo may be. A P-256 key's is 91 bytes, a P-384 key's 120, and an RSA key of 4,096 bits about 550: 4 KB
// is far past any real one, and refuses a certificate that tries to make the game hash a great deal.
size_t const kNetServerFingerprintMostKeyBytes = 4096;

// The fingerprint, from the list's key (its SubjectPublicKeyInfo, as DER: spkiSize bytes at spki) and the relay's id. False, with out
// wiped, for no key, a key over kNetServerFingerprintMostKeyBytes, or if SHA-256 itself failed.
inline bool NetServerFingerprint_Make(const uint8_t *spki, size_t spkiSize, const uint8_t relayId[kNetRelayKeysFingerprintBytes],
	uint8_t out[kNetServerFingerprintBytes])
{
	if (!out)
	{
		return false;
	}

	if (!spki || spkiSize == 0 || spkiSize > kNetServerFingerprintMostKeyBytes || !relayId)
	{
		NetCrypto_Wipe(out, kNetServerFingerprintBytes);
		return false;
	}

	uint8_t const zero = 0;
	NetCryptoPiece const pieces[4] = {
		{ reinterpret_cast<const uint8_t *>(kNetServerFingerprintWords), sizeof(kNetServerFingerprintWords) - 1 },
		{ &zero, 1 },
		{ spki, spkiSize },
		{ relayId, kNetRelayKeysFingerprintBytes },
	};

	return NetCrypto_Sha256(pieces, 4, out);
}

// Whether a server is the one its line names: the list's key as it was served, and the relay's keys as the list gave them over that
// same connection, make the line's fingerprint. Every byte is compared, whatever the first difference, so the time it takes says
// nothing. False if they don't, or if anything couldn't be worked out.
inline bool NetServerFingerprint_IsTheLines(const uint8_t *spki, size_t spkiSize, const NetRelayStaticKeys *relayKeys,
	const uint8_t lines[kNetServerFingerprintBytes])
{
	if (!relayKeys || !lines)
	{
		return false;
	}

	uint8_t relayId[kNetRelayKeysFingerprintBytes];
	uint8_t made[kNetServerFingerprintBytes];

	if (!NetRelayKeys_Fingerprint(relayKeys, relayId) || !NetServerFingerprint_Make(spki, spkiSize, relayId, made))
	{
		return false;
	}

	uint8_t differences = 0;

	for (size_t i = 0; i < kNetServerFingerprintBytes; i++)
	{
		differences |= static_cast<uint8_t>(made[i] ^ lines[i]);
	}

	return differences == 0;
}

#endif
