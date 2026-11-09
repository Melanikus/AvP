# The online server's design, copied here

Written 2026-11-09 by Melanikus for the AvP port, at the owner's ruling (through PO): the game repository alone must be enough to understand and rebuild online play, in case the server's repository is ever gone.

**What these are.** The two files beside this one are the design the game's online play is built from. They're written and kept in the server's repository, AVPx-Dedicated-Server:
- `CRYPTO.md`: version 2, how games are kept secret, sealed and fair. Every handshake, key and refusal.
- `PROTOCOL.md`: every message the server says and hears, the list's calls, and the limits.

**They're copies, byte for byte. Never edit them here.** A copy changed to suit the game would prove nothing, and the two repositories would quietly stop agreeing. To change the design, change it in the server's repository, then take a new copy.

**Where they come from:**

From: AVPx-Dedicated-Server, commit `8b90047af4636478ce39dad91dcbdd26bf01d0ee` (branch v2)

- `CRYPTO.md`: sha256 `2e76871f7eccea7fa2d7bb9aaec9498ff172de94b58045ea9d73127b63f5c5e1`, 79491 bytes
- `PROTOCOL.md`: sha256 `4b215959725ae0d82949f085bcffac9c50a62eabab00b529eb83ad7c67c0d3e8`, 35273 bytes

**The check.** `tests/protocol_copy_test` reads the lines above. Every file listed must be here, of exactly that size and that sha256, and every file here must be listed. Its take-out list (`tests/takeouts/protocol_copy_test.json`) shows the test notices each of those going wrong. So a copy that drifts, by an edit here or a broken copy, fails a required check, and never passes silently. This folder's `.gitattributes` stops git from changing the line ends, which would change the bytes.

**Taking a newer copy** is a deliberate commit, like the relay's test vectors (tests/vectors/README.md):
1. Copy the server repository's file over the one here, byte for byte: `git show <commit>:CRYPTO.md > docs/protocol/CRYPTO.md`, for each file.
2. Put the new commit, sha256 and size in the lines above (`sha256sum`, `wc -c`).
3. Say in the commit what changed in the design, and change the game's code to match it, in the same pull request or before it.

**The line format, which the test reads:** `From: AVPx-Dedicated-Server, commit` and the 40-digit commit in backticks; then one line a file, `- ` and the file's name in backticks, `: sha256 ` and 64 small hex digits in backticks, `, ` and its size, then ` bytes`. A name is letters, digits, `.`, `_` and `-` only.
