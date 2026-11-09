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
