// The game's post-quantum key exchange, ML-KEM-1024 (FIPS 203). Written 2026-11-09 by Melanikus for the AvP port.
//
// WHAT IT'S FOR. Network protocol version 2 makes each session's keys from two key exchanges at once: the classical one (ECDH on
// P-384) and this one. A session's keys stay secret unless both are broken, so a future quantum computer that breaks the classical one
// still can't read a session recorded today. ML-KEM works in three steps:
// - make a key pair: a public key to send to the other side, and a secret key to keep;
// - encapsulate to the other side's public key: this makes a fresh 32-byte shared secret, and a ciphertext that carries it;
// - the other side decapsulates the ciphertext with its secret key, and gets the same 32-byte secret.
//
// WHY A WRAPPER. The library, mlkem-native (vcpkg-overlay-ports/mlkem-native), takes every buffer at a fixed size, without its length,
// and checks no pointer, so a short buffer from the network would be read past its end. Its answers are numbers, 0 for "done", and
// Microsoft's compiler doesn't make anyone look at them. So these functions:
// - take every buffer with its length, and refuse unless it's exactly the size ML-KEM-1024 uses, before the library sees anything;
// - look at every answer, and take anything but "done" as a refusal;
// - on a refusal, wipe every buffer they were to fill, to zeros, so nothing half-made can be taken for a key or a secret: the length
//   given, but never more than ML-KEM-1024's own size for that buffer, so a length that's wrong can't make the wipe write past the
//   buffer's end (Fourth's cold read, 2026-11-09);
// - give back true for done and false for refused, and the compiler refuses to build a call that ignores which ([[nodiscard]] with
//   the game's /we4834).
// The buffers mustn't overlap each other: a call whose buffers share a byte is refused (inputs that are only read may share with each
// other), as the library may write one while it still reads another.
//
// tests/pq_mlkem_test.cpp runs NIST's and Wycheproof's published answers, and the library's own, through these and the library.

#ifndef PQ_KEY_EXCHANGE_H
#define PQ_KEY_EXCHANGE_H

#include <stddef.h>
#include <stdint.h>

// The sizes of ML-KEM-1024, from FIPS 203 (and mlkem_native.h, which pq_key_exchange.cpp checks they match)
constexpr size_t kPqPublicKeyBytes = 1568;
constexpr size_t kPqSecretKeyBytes = 3168;
constexpr size_t kPqCiphertextBytes = 1568;
constexpr size_t kPqSharedSecretBytes = 32;

// Makes a new key pair from Windows' random generator, and checks it works before giving it back. False if the generator failed or
// the check did, and then both buffers are zeros.
[[nodiscard]] bool Pq_NewKeyPair(uint8_t *publicKey, size_t publicKeyLength, uint8_t *secretKey, size_t secretKeyLength);

// Makes a fresh shared secret for the owner of publicKey, and the ciphertext to send them. False for a public key of the wrong size, or
// one FIPS 203's check refuses (a number in it too big to be one ML-KEM makes), or if the random generator failed. Then the ciphertext
// and the secret are zeros.
[[nodiscard]] bool Pq_Encapsulate(uint8_t const *publicKey, size_t publicKeyLength, uint8_t *ciphertext, size_t ciphertextLength,
                                  uint8_t *sharedSecret, size_t sharedSecretLength);

// Gets the shared secret back out of a ciphertext with our secret key. False for a ciphertext or secret key of the wrong size, or a
// secret key that fails FIPS 203's check of itself, and then the secret is zeros.
// A ciphertext that's been changed is NOT refused: by ML-KEM's design it gives a different secret, unpredictable to whoever changed it,
// and the session's next step, which needs both sides to have the same secret, is what fails.
[[nodiscard]] bool Pq_Decapsulate(uint8_t const *ciphertext, size_t ciphertextLength, uint8_t const *secretKey, size_t secretKeyLength,
                                  uint8_t *sharedSecret, size_t sharedSecretLength);

#endif
