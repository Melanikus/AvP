# The relay's test vectors

Written 2026-11-08 by Melanikus for the AvP port.

**What this is.** `relay-v1.json` lists the relay's messages (PROTOCOL.md, in the AVPx-Dedicated-Server repository) as exact bytes:

- 24 messages, each with the fields it holds;
- 81 packets that aren't messages, each with the reason a reader has to refuse it;
- 6 passes, which only the list and the relays make and check.

The relay's repository writes the file from its own code (`tools/make-vectors.js`), and its tests fail if the file and the code ever
disagree.

**Why it's here.** `tests/net_relay_test.cpp` reads it. Every message has to read to its fields and write back to the same bytes. Every
refused packet has to be refused for the same reason. So the game and the relay can't each pass their own tests and still read the same
bytes differently.

**Where it comes from.** AVPx-Dedicated-Server, commit `26d0580`, file `test-vectors/relay-v1.json`, made for PROTOCOL.md as at `7014de1`.

- 28,991 bytes, with plain line ends (LF);
- sha256 `c982ae7d94fcf8e0c71fe69180c01beae6fbd9aa6b5b998c010c2d3fdc5dc2f7`.

The gate checks that sha256 (`.github/workflows/tests.yml`, "Check the relay's test vectors are the relay's own"). This folder's
`.gitattributes` stops git from changing the line ends, which would change the bytes.

**Taking a newer one,** when PROTOCOL.md changes:

1. Copy the relay repository's file over this one, byte for byte. Never edit it here: a copy changed to suit the game's code proves nothing.
2. Put its sha256 and commit in the gate's step and in this file.
3. Change `net_relay.h` until `net_relay_test` passes, and its "PROTOCOL.md as at" check with it.

## Protocol version 2: `crypto-v2.json`

Added 2026-11-09 by Melanikus for the AvP port.

**What this is.** Protocol version 2's whole handshakes and sealed messages as exact bytes (CRYPTO.md section 15, in the
AVPx-Dedicated-Server repository):
- handshake A: every input, A1, A2 and A3, h and ck after each of the 16 steps, Split's keys, the transcript;
- A's datagrams: the cookie round and the fragments;
- four sealed control messages;
- handshake B;
- the epoch keys, AES-GCM, and refusals.

Every random choice in it comes from a fixed seed, and each key is given as what the primitive itself takes: a P-384 private key as its
48-byte number, an ML-KEM key pair as FIPS 203's d then z, an encapsulation as its m. So the game makes the same bytes with nothing of
Java's.

**Why it's here.** `tests/net_v2_parts_test.cpp` and `tests/net_v2a_test.cpp` read it: the game's side has to make every byte the
relay's side made, and the game's handshake B is held to it the same way. The relay's repository makes the file from its own code
(`java/test/avpx/v2/CryptoV2Vectors.java`), and its tests fail if the file and the code ever disagree.

**Where it comes from.** AVPx-Dedicated-Server, commit `3fcb588`, file `test-vectors/crypto-v2.json`, made for CRYPTO.md as at `bd0e45d`.

- 190,877 bytes, with plain line ends (LF);
- sha256 `8894d55285ecd1b3baa98ee413c00a2e5ae93d3237690b7a354d3ad4206bcc88`.

The gate checks that sha256 ("Check protocol version 2's test vectors are the relay's own"). A newer one is taken the same way as
`relay-v1.json`'s, above.
