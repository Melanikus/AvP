# SipHash, the reference code, kept as it is

Our ENet port uses these files for the tokens a host gives a CONNECT (patch 006, `ENetProtocolCookie`). A token is
SipHash-2-4 of the sender's address, port and connection number and a slot of time, under a key only the host knows. The
four files are the reference code by Jean-Philippe Aumasson and Daniel J. Bernstein, copied byte for byte and never
edited here. Crypto is the one thing this project never writes for itself.

- **From:** https://github.com/veorq/SipHash, commit `32d067603b93b47828700880649198e0bfbbcffa` (2026-03-07), copied on
  2026-11-07.
- **Licence:** the authors offer CC0, MIT or Apache 2.0 with LLVM exceptions. We take it under **CC0 1.0** (public domain).
  `LICENSE_CC0` is their copy of it.
- **The files, with their SHA-256:**

  | File | SHA-256 | What it is |
  |---|---|---|
  | `siphash.c` | `94b6c1f3a24072a8a5afc4fea7fb9e95c5fc2e99657b6b712d991029ffdaa88c` | SipHash itself |
  | `siphash.h` | `30d4269093e5886cb9e7ea826899ef02daddac682e73c335c53dd748b9e20bfb` | its one function, `siphash` |
  | `vectors.h` | `212c44114a63c6d84710b8627f3bc5ce155698accf2ea7bbff1c4b69c9f48d31` | the answers the authors published, only data |
  | `LICENSE_CC0` | `5537d4d10b76b81b6e8dfd8b644480a4b1efa332fbb0cdb61126c5be781ef7b4` | the licence |

- **How it's used:** `portfile.cmake` copies `siphash.c` and `siphash.h` into ENet's source before building, and patch 006
  adds `siphash.c` to ENet's list of files. `tests/enet_hostile_test.cpp` checks the `siphash` that ENet was built with
  against all 64 answers in `vectors.h`, the same way the authors' own `test.c` does.
- **Not copied:** HalfSipHash, the authors' test program and their makefile. We don't use them.
- **Read before use (2026-11-07, Primary):** `siphash.c` does no input or output, allocates nothing, and calls only
  `assert` (on the output size, which is always 8 here). `vectors.h` is nothing but `#include <stdint.h>` and arrays of
  bytes. Its first answers match the paper's: 0 bytes `726fdb47dd0e0e31`, 1 byte `74f839c593dc67fd`, 15 bytes
  `a129ca6149be45e5`.
- **To update:** copy the four files from a newer commit, put its hash and the new SHA-256s here, read the changes, and
  run `enet_hostile_test`. Its SipHash check must still give all 64 answers.
