// Checking the post-quantum half of a signature, ML-DSA-87 (FIPS 204). Written 2026-11-09 by Melanikus for the AvP port.
//
// WHAT IT'S FOR. Network protocol version 2 signs what the game has to be able to trust (the list of games, the list of relays, a
// release) twice: with ECDSA on P-384, the classical kind, and with ML-DSA-87, which a quantum computer isn't known to break. Both must
// check, or the thing is refused. This is the ML-DSA half. The game only ever checks: it signs nothing, and the library is built with
// no way to (vcpkg-overlay-ports/mldsa-native).
//
// WHY A WRAPPER. The library takes the signature and the public key at a fixed size, without their lengths, and checks no pointer, so a
// short signature from the network would be read past its end. Its answer is a number, 0 for "it checks", and Microsoft's compiler
// doesn't make anyone look at it. So this:
// - takes every buffer with its length, and refuses a signature or a key that isn't exactly the size ML-DSA-87 uses, before the
//   library sees it;
// - refuses a missing message or context that claims to have bytes;
// - says yes only when the library's answer is exactly 0, and the compiler refuses to build a call that ignores the answer
//   ([[nodiscard]] with the game's /we4834).
//
// tests/pq_mldsa_test.cpp runs NIST's and Wycheproof's published answers, and the library's own, through this and the library.

#ifndef PQ_SIGNATURE_CHECK_H
#define PQ_SIGNATURE_CHECK_H

#include <stddef.h>
#include <stdint.h>

// The sizes of ML-DSA-87, from FIPS 204 (and mldsa_native.h, which pq_signature_check.cpp checks they match)
constexpr size_t kPqSignatureBytes = 4627;
constexpr size_t kPqSignaturePublicKeyBytes = 2592;

// True only if signature is a good ML-DSA-87 signature, by the holder of publicKey, of message with context, where the context is the
// short label that says what kind of thing was signed. Pure ML-DSA, as FIPS 204 section 5.2 has it. The message and the context may be
// empty, and then may be NULL. False for anything else: a signature or key of the wrong size, a context longer than 255 bytes, a
// signature that doesn't check.
[[nodiscard]] bool Pq_SignatureIsValid(uint8_t const *signature, size_t signatureLength, uint8_t const *message, size_t messageLength,
                                       uint8_t const *context, size_t contextLength, uint8_t const *publicKey, size_t publicKeyLength);

#endif
