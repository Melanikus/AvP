// The list's key, from the certificate a server's list shows the game: its SubjectPublicKeyInfo, as DER (#175). Written 2026-11-10 by
// Melanikus for the AvP port.
//
// WHY ONLY THE KEY. The game trusts a server's list by its key alone (the owner's "trust the pin alone"): net_server_fingerprint.h turns
// the key, with the relay's id, into the number the servers.cfg line holds. The certificate's issuer, its names and its dates are
// never looked at, so an expired certificate with the right key is the right list, and a certificate that's in date with another key
// isn't. tests/net_relay_keys_test.cpp proves both, with real certificates.
//
// Windows' own certificate code (crypt32) reads the certificate and writes its key out as DER again, which for a certificate written in
// DER, as every server's is, gives exactly the bytes the certificate holds. The test checks that against an implementation apart from
// Windows'.

#ifndef NET_SERVER_KEY_H
#define NET_SERVER_KEY_H

#include <stddef.h>
#include <stdint.h>

// The most a certificate may be. A server's is a few hundred bytes. This is far past any real one.
size_t const kNetServerKeyMostCertificateBytes = 64 * 1024;

// The key of the certificate in certificate (certificateSize bytes of DER) into key, which has room for room bytes, and its size into
// keySize. False, with keySize 0 and key wiped, if it isn't a certificate Windows can read, it's over the most, or its key doesn't fit.
bool NetServerKey_OfCertificate(const uint8_t *certificate, size_t certificateSize, uint8_t *key, size_t room, size_t *keySize);

#endif
