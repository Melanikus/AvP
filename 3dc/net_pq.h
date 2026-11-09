// The post-quantum half of the relay's protocol version 2 cryptography: what the game asks for, and nothing more. Written 2026-11-09 by
// Melanikus for the AvP port.
// Modified 2026-11-09 by Melanikus for the AvP port: net_pq_native.cpp is the real one now, and a key pair's randomness is said truly.
//
// WHAT THIS IS. Version 2 (CRYPTO.md, in the AVPx-Dedicated-Server repository) puts a post-quantum secret beside every classical one
// (net_crypto.h), so that a session holds even against a future quantum computer, unless both halves are broken. The game needs:
//   - ML-KEM-1024 (FIPS 203), the post-quantum key agreement: a key pair, "encapsulate" (make a secret for a public key, and the
//     ciphertext that carries it), and "decapsulate" (get the secret back with the private key);
//   - ML-DSA-87 (FIPS 204), the post-quantum signature: checking only, since the game signs nothing (CRYPTO.md section 4).
//
// WHERE IT COMES FROM. The user decided (CRYPTO.md section 3): built from source into the game, from the PQ Code Package's mlkem-native and
// mldsa-native, pinned and read in full before anything of them is built or run (CRYPTO.md section 3.1). The game never uses Windows' own
// ML-KEM or ML-DSA, even where it has them: one code path for every player (section 3.2).
//
// UNTIL THAT'S DONE, the game is built with net_pq_none.cpp, which refuses every call, and NetPq_Available() is false. Version 2 needs
// both halves, so with it nothing runs at all: there's no classical-only fallback, which would be a way to attack it (CRYPTO.md section 3).
// The real one, net_pq_native.cpp, takes its place once the libraries have been read, with nothing else changing. It's written (over
// Secondary's wrapper, pq_key_exchange.h and pq_signature_check.h); the stand-in stays for the tests that need version 2 refused.
//
// HOW IT ANSWERS: as net_crypto.h does. True or false, and on false every output has been wiped to 0. A key given that isn't one (the
// wrong size of anything is impossible here, since every size is fixed) is refused: encapsulate checks the public key as FIPS 203 section
// 7.2 says, and the handshake ends on that. Decapsulate never fails on a bad ciphertext: as FIPS 203 says, it gives a pseudo-random secret
// instead ("implicit rejection"), so nobody on the network can tell, and the handshake then fails on its next tag.

#ifndef NET_PQ_H
#define NET_PQ_H

#include <stddef.h>
#include <stdint.h>

// The sizes, in bytes (CRYPTO.md section 2; FIPS 203 and 204)
size_t const kNetPqKemPublicBytes = 1568;       // an ML-KEM-1024 encapsulation key, the public half
size_t const kNetPqKemPrivateBytes = 3168;      // its decapsulation key, the private half
size_t const kNetPqKemCiphertextBytes = 1568;
size_t const kNetPqKemSecretBytes = 32;
size_t const kNetPqDsaPublicBytes = 2592;       // an ML-DSA-87 public key
size_t const kNetPqDsaSignatureBytes = 4627;

// Whether this game has a post-quantum half at all. False with net_pq_none.cpp: then version 2 can't be used, and the game says so.
bool NetPq_Available();

// Which post-quantum half this game was built with, in fixed words for the log
const char *NetPq_Name();

// A new ML-KEM-1024 key pair, from the operating system's randomness (drawn inside the library, from BCryptGenRandom, the same source as
// NetCrypto_Random). A failure of the randomness is a refusal.
bool NetPq_KemKeypair(uint8_t publicOut[kNetPqKemPublicBytes], uint8_t privateOut[kNetPqKemPrivateBytes]);

// A fresh secret for publicKey, and the ciphertext that carries it. False for a public key FIPS 203 section 7.2 refuses.
bool NetPq_KemEncapsulate(const uint8_t publicKey[kNetPqKemPublicBytes], uint8_t ciphertextOut[kNetPqKemCiphertextBytes], uint8_t secretOut[kNetPqKemSecretBytes]);

// The secret a ciphertext carries, with privateKey. False only for a private key that fails FIPS 203 section 7.3's own check of itself.
bool NetPq_KemDecapsulate(const uint8_t privateKey[kNetPqKemPrivateBytes], const uint8_t ciphertext[kNetPqKemCiphertextBytes], uint8_t secretOut[kNetPqKemSecretBytes]);

// Whether signature is publicKey's ML-DSA-87 signature of message: pure ML-DSA (FIPS 204's ML-DSA.Verify) with the EMPTY context, the
// only one the relay's Java can sign with (CRYPTO.md section 5.1 point 16, and section 12). Nothing is lost by that: the message is the
// 48-byte digest D = SHA-384("AVPX <what> v2" || the bytes), whose first words already say what kind of thing was signed, so a signature
// over one kind of thing can't be passed off as another.
bool NetPq_DsaVerify(const uint8_t publicKey[kNetPqDsaPublicBytes], const uint8_t *message, size_t messageSize,
	const uint8_t signature[kNetPqDsaSignatureBytes]);

#endif
