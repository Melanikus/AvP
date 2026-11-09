// For tests only: the ML-KEM-1024 calls with fixed randomness, so a test can make the very keys and encapsulations
// tests/vectors/crypto-v2.json writes down. Written 2026-11-09 by Melanikus for the AvP port.
//
// WHY ONLY HERE. The game itself never takes randomness from outside: net_pq_native.cpp has no such call, and handshakes are handed what
// the game drew for real (net_handshake_a.h, net_peer_v2.h). The library's own "derand" calls, which take the randomness as bytes, are
// reached from tests alone, through this file, so the game the players get can't be told which values to use. A P-384 key from fixed
// bytes needs nothing here: net_crypto.h's NetCrypto_EcdhFromPrivate does it, and says it's for test vectors only.
//
// The test vectors give each value as the primitive takes it (FIPS 203): a key pair as its seeds d then z (64 bytes), an encapsulation as
// its m (32 bytes). Both of these are checked by NIST's and Wycheproof's own answers in tests/pq_mlkem_test.cpp.

#ifndef NET_V2_TEST_KEYS_H
#define NET_V2_TEST_KEYS_H

#include <stddef.h>
#include <stdint.h>

#include <mlkem-native/avpx_mlkem.h>

#include "net_pq.h"

size_t const kNetV2TestKemSeedBytes = 64;   // d, then z
size_t const kNetV2TestKemMBytes = 32;

// An ML-KEM-1024 key pair from its 64-byte seed (FIPS 203's ML-KEM.KeyGen_internal(d, z))
inline bool NetV2Test_KemPairFromSeed(const uint8_t seed[kNetV2TestKemSeedBytes], uint8_t publicOut[kNetPqKemPublicBytes],
	uint8_t privateOut[kNetPqKemPrivateBytes])
{
	return seed && publicOut && privateOut && avpx_mlkem1024_keypair_derand(publicOut, privateOut, seed) == 0;
}

// An encapsulation to publicKey from a given m (FIPS 203's ML-KEM.Encaps_internal(ek, m)): the same m always gives the same bytes
inline bool NetV2Test_KemEncapsulateFrom(const uint8_t publicKey[kNetPqKemPublicBytes], const uint8_t m[kNetV2TestKemMBytes],
	uint8_t ciphertextOut[kNetPqKemCiphertextBytes], uint8_t secretOut[kNetPqKemSecretBytes])
{
	return publicKey && m && ciphertextOut && secretOut && avpx_mlkem1024_enc_derand(ciphertextOut, secretOut, publicKey, m) == 0;
}

#endif
