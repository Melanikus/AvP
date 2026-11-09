// The settings mldsa-native is built with for the game, and read with by the game. Written 2026-11-09 by Melanikus for the AvP port.
//
// mldsa-native takes every setting from one header. Its own, mldsa/mldsa_native_config.h, isn't used: the library's build
// (CMakeLists.txt) and the game's header (avpx_mldsa.h) both name this one in MLD_CONFIG_FILE, and this one replaces the project's
// whole. So anything not switched on here is off. The project's own file explains every setting at length; these are the ones we use.
//
// What the library gives the game, once built with these: one thing, checking a signature. The game never signs anything, so it has
// no way to: making keys and signing are left out of the build altogether. The game's wrapper, 3dc/pq_signature_check.cpp, is what the
// game calls, and it checks every size and the answer.

#ifndef AVPX_MLDSA_CONFIG_H
#define AVPX_MLDSA_CONFIG_H

// ML-DSA-87, the largest of FIPS 204's three sizes, the one network protocol version 2 uses. The project's own default is 44, the
// smallest, so leaving this out would check signatures at the lowest level without a word. avpx_mldsa.h refuses to compile if it's
// anything else.
#define MLD_CONFIG_PARAMETER_SET 87

// Every name the library makes public starts with this, so none can clash with any other library's names in the game:
// avpx_mldsa87_verify and the rest. With the size in the name, a game built against a library of another size can't link at all.
#define MLD_CONFIG_NAMESPACE_PREFIX avpx_mldsa87

// The library is built from one file, so its own helper functions can all be private to that file (static). Only the functions in
// mldsa_native.h are left for anything else to call.
#define MLD_CONFIG_INTERNAL_API_QUALIFIER static

// Checking only. No key making (NO_KEYPAIR_API), no signing (NO_SIGN_API), and so nothing that needs random bytes
// (NO_RANDOMIZED_API): the library needs no random source in the game at all. Code that isn't built can't go wrong in the game.
#define MLD_CONFIG_NO_KEYPAIR_API
#define MLD_CONFIG_NO_SIGN_API
#define MLD_CONFIG_NO_RANDOMIZED_API

// No assembly: the library is built from its portable C alone. Microsoft's compiler can't build the project's assembly files anyway,
// and has no inline assembly in the form the project writes it, so this only makes certain of what would happen.
#define MLD_CONFIG_NO_ASM

// Less memory, a little more time. Checking a signature takes 12,512 bytes with this, against 68,800 without, by the project's own
// count (MLD_TOTAL_ALLOC_87_VERIFY in mldsa_native.h). The game checks a few signatures a session, so the time doesn't matter, and the
// smaller amount on the stack does. It's set here, outside the part only the library's build reads, because mldsa_native.h reads it too
// when it says how much memory each call takes.
#define MLD_CONFIG_REDUCE_RAM

#endif
