// The settings mlkem-native is built with for the game, and read with by the game. Written 2026-11-09 by Melanikus for the AvP port.
//
// mlkem-native takes every setting from one header. Its own, mlkem/mlkem_native_config.h, isn't used: the library's build
// (CMakeLists.txt) and the game's header (avpx_mlkem.h) both name this one in MLK_CONFIG_FILE, and this one replaces the project's
// whole. So anything not switched on here is off. The project's own file explains every setting at length; these are the ones we use.
//
// What the library gives the game, once built with these: make a key pair, encapsulate (make a shared secret and the ciphertext that
// carries it to the key's owner), and decapsulate (get the same secret back from the ciphertext). The game's wrapper,
// 3dc/pq_key_exchange.cpp, is what the game calls, and it checks every size and every answer.

#ifndef AVPX_MLKEM_CONFIG_H
#define AVPX_MLKEM_CONFIG_H

// ML-KEM-1024, the largest of FIPS 203's three sizes, the one network protocol version 2 uses. The project's own default is 768, the
// middle one, so leaving this out would build a weaker library without a word. avpx_mlkem.h refuses to compile if it's anything else.
#define MLK_CONFIG_PARAMETER_SET 1024

// Every name the library makes public starts with this, so none can clash with any other library's names in the game:
// avpx_mlkem1024_keypair, avpx_mlkem1024_enc, avpx_mlkem1024_dec and the rest. With the size in the name, a game built against a
// library of another size can't link at all.
#define MLK_CONFIG_NAMESPACE_PREFIX avpx_mlkem1024

// The library is built from one file, so its own helper functions can all be private to that file (static). Only the functions in
// mlkem_native.h are left for anything else to call. The project's example of a one-file build does the same.
#define MLK_CONFIG_INTERNAL_API_QUALIFIER static

// No assembly: the library is built from its portable C alone. Microsoft's compiler can't build the project's assembly files anyway,
// and has no inline assembly in the form the project writes it, so this only makes certain of what would happen.
#define MLK_CONFIG_NO_ASM

// Every new key pair is tried before it's given back: something is encapsulated to it and decapsulated again, and if the two secrets
// don't match, the key pair is refused (MLK_ERR_PCT_FAIL) and wiped. FIPS 140-3 asks for this check, the "pairwise consistency
// test". It costs a fraction of a millisecond, once per key pair.
#define MLK_CONFIG_KEYGEN_PCT

// What follows is only read while the library itself is being built (MLK_BUILD_INTERNAL), never by the game's code that calls it.
#if defined(MLK_BUILD_INTERNAL) && !defined(__ASSEMBLER__)

// WHERE RANDOM BYTES COME FROM. Making a key pair takes 64 random bytes and encapsulating takes 32. The library has no random source
// of its own and asks for one. Ours is Windows' own: BCryptGenRandom with BCRYPT_USE_SYSTEM_PREFERRED_RNG, the generator Windows itself
// uses for its own keys, with no handle to open or close. It gives 0 for "here they are" and anything else for "I couldn't", and when
// it can't, the library gives back MLK_ERR_RNG_FAIL and no key. The wrapper turns that into a refusal, never a weak key.
#define MLK_CONFIG_CUSTOM_RANDOMBYTES

#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <windows.h>
#include <bcrypt.h>

// A plain static function: the library is built from one file, so this is that file's own. The project's example writes it with its
// own MLK_INLINE in front, which needs its src/sys.h first; without either, it's the same function to the compiler, and a scanner
// reading this file on its own can read it whole.
static int mlk_randombytes(uint8_t *out, size_t outlen)
{
	// BCryptGenRandom counts in ULONGs, 32 bits. On a 32-bit build size_t is the same, but on a 64-bit one it's bigger, so a request
	// is handed over in pieces that fit. The library only ever asks for 32 or 64 bytes, so this is one piece in practice.
	while (outlen > 0)
	{
		ULONG const piece = (outlen > ULONG_MAX) ? ULONG_MAX : (ULONG)outlen;

		if (!BCRYPT_SUCCESS(BCryptGenRandom(NULL, out, piece, BCRYPT_USE_SYSTEM_PREFERRED_RNG)))
		{
			return -1;
		}

		out += piece;
		outlen -= piece;
	}

	return 0;
}

#endif

#endif
