/* host_secret.c: where an ENet host's secret keys come from. Written 2026-11-07 by Melanikus for the AvP port. This file is
   ours, not ENet's.

   Patch 006 (006-a-token-before-a-place.patch) gives each host secret keys, which only it knows, to make the tokens it hands a
   joiner. They have to come from the system's secure random source: anyone who could guess them could make a token for a
   made-up address, and fill the game's places. On Windows that source is BCryptGenRandom, and elsewhere it's /dev/urandom.

   It's a file of its own, copied into ENet's source by portfile.cmake, rather than more lines in ENet's own unix.c and win32.c.
   Snyk's scan of the libraries recognises ENet by how many of its own files are still as they came, and every file we change
   counts against that. */

#define ENET_BUILDING_LIB 1
#include "enet/enet.h"

#ifdef _WIN32

#include <windows.h>
#include <bcrypt.h>

/* Links Windows' library for it, bcrypt.lib, into whatever uses ENet, so nothing else has to name it */
#pragma comment(lib, "bcrypt.lib")

int
enet_host_secret (enet_uint8 * secret, size_t size)
{
    /* ENet asks for 16 bytes at a time */
    return BCRYPT_SUCCESS (BCryptGenRandom (NULL, secret, (ULONG) size, BCRYPT_USE_SYSTEM_PREFERRED_RNG)) ? 0 : -1;
}

#else

#include <stdio.h>

int
enet_host_secret (enet_uint8 * secret, size_t size)
{
    FILE * source = fopen ("/dev/urandom", "rb");
    size_t got;

    if (source == NULL)
      return -1;

    got = fread (secret, 1, size, source);
    fclose (source);

    return got == size ? 0 : -1;
}

#endif
