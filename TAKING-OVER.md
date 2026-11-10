# Taking over: building, trusting and releasing the game when we're gone

Written 2026-11-09 by Melanikus, at the owner's word: "20 years from now, we won't be there to enforce tao 1, our keys and tokens will have expired, be mindful of that".

**Who this is for:** you, years from now, with this repository and none of our accounts, keys or tokens. Nobody can give you permission, and you don't need it. This page is how you build the game, decide which servers it trusts, and release it under your own name.

**Two repositories go together.** The game is here. Its online server is in **AVPx-Dedicated-Server** (github.com/Melanikus/AVPx-Dedicated-Server when this was written), with its own TAKING-OVER.md for running a server and approving players. The design both are built from, CRYPTO.md and PROTOCOL.md, is written there, and **copied here, byte for byte, in `docs/protocol/`**. COPIED.md there names the server commit each came from, with its sha256, and `tests/protocol_copy_test` fails if a copy drifts. So this repository alone is enough to understand and rebuild online play. If you find the other one too, keep them together.

**Marked [BETA.2]:** parts being built for beta.2 (BETA2-PLAN, "Built to outlive us") that may not be in your copy. If one is missing, the design is still what's written here.

---

## 1. What you need

- **Windows,** and **Visual Studio with its C++ tools** (README.md names the version). [BETA.2: the exact version, pinned with where to get it and how to check it; today the workflows take the newest one installed.]
- **This repository,** cloned whole.
- **Your own copy of the original game's data:** *Aliens versus Predator Classic 2000*, from Steam (PLAYTEST.md). The data isn't ours and isn't in this repository. Only the program is.
- **Nothing of ours:** no GitHub account of ours, no token, no signing certificate. Section 6 says what each of our gates proved, and what can replace it.

## 2. Build it from source

- **The libraries** are open source, and built from their source by vcpkg, pinned: `vcpkg.json`'s baseline, the ports in `vcpkg-overlay-ports`, the build settings in `vcpkg-overlay-triplets`. Nothing precompiled of ours is in the repository. The one binary package is Microsoft's own D3DX, a platform library, which is allowed (the owner's ruling).
- **The build** is `tools/build-and-package.ps1`, the same script the workflows run. game-build.yml shows the steps on a clean machine.
- **The tests:** `tests/build.ps1` builds and runs every test program in `tests/`. Each returns 0 only if every check in it passed.
- **Take-outs:** `tests/takeouts/run.ps1` breaks the code on purpose, one row at a time, and each test must notice (tests/takeouts/README.md). Run it after any change you make: it's how you know a test tests something.
- **Without downloads:** RELEASING.md's "Building on a machine without vcpkg" builds against libraries already built, checked by their SHA-256 list. [BETA.2: every library's source archived with its hash, so a machine with no network builds the libraries too.]

## 3. Which servers the game trusts

The game trusts a server only through what it carries itself. Nobody's word over the network counts, and no company's signature does either.

- **servers.cfg [BETA.2, issue #175]:** one line a server, every field required:
  - the server's address;
  - the **list's pin:** the SHA-256 of its certificate's public key (SPKI);
  - the **relay's id:** 96 hex digits, the SHA-384 of the relay's two static public keys (CRYPTO.md section 12.1).
  A release ships the lines of the servers it vouches for, and a player may add a line for any other server. **A public server's address may be in a shipped file. A person's home address never is.**
- **The pin alone** (the owner's ruling, 2026-11-09): with a pin, the game ignores the certificate's dates, name and issuer, and checks only that its key is the pinned one. **Why:** a date can only end a server, never save one. In 20 years nobody may be there to make a new certificate, and a date would stop every copy of the game reaching the server on that day, for good, with the key still safe. A key that's stolen or changed is answered by a new servers.cfg line, and for a server a release ships, a new release.
- **The relay's keys** come from the list (`GET /v1/relay-keys`), and the game uses that relay only if they hash to the line's relay id.
- **Your own server:** run one as the server repository's TAKING-OVER.md says. It ends with your servers.cfg line. Ship it in your release, or give it to your players.
- **For tests only,** `-dev` reads a list address, a pin and a relay id from the settings file instead (docs/online-play.md). A release never takes them from there.

## 4. Players, and how they're let in

- **A player's identity** is made by their game, the first time it goes online, and kept in their save folder. The private halves are sealed by Windows (DPAPI), so another account on the same PC can't read them. [BETA.2: `3dc/net_identity.h`, with the version 2 pull requests.] It's theirs, not the server's: a server never sees the private halves.
- **Every player is approved by a person, always** (the owner's ruling). The game sends a request, and shows the player an **access code**, the first 80 bits of their fingerprint, 16 letters. They give it to the server's owner by a way the owner knows is them, and the owner approves that code (the server's TAKING-OVER.md, section 4). The game then shows "approved".
- **Between two players,** the game shows a 6-digit check code on both screens (CRYPTO.md section 8). If the two codes match when the players read them to each other, nobody is in the middle.

## 5. Releasing, and signing it as yourself

- **release.yml** makes a release, only when started by hand on the protected branch. It stops unless the commit may be released (RELEASE-BLOCKERS.md must say `Nothing.`), builds the package, scans it, and leaves a draft that a person publishes (RELEASING.md).
- **Signing:** our releases are to be signed with a code-signing certificate in the owner's name [BETA.2: Certum's; beta.1 was unsigned]. That certificate is his, and it will have expired. **Sign yours with a code-signing certificate in your own name.** An unsigned build still runs, but Windows warns before it does ("More info", then "Run anyway"), and players can't tell it from anyone else's.
- **Where it came from:** GitHub's attestation (`gh attestation verify <file> --repo <your repository>`) says a file came out of a given repository's workflow. Yours names your repository, not ours. On a host without attestations, publish the SHA-256 of every file, signed with your own key, and say where your public key is.
- **Your network version:** each release that speaks online carries a network version number. A number that was used is never used again, even for a build that never shipped, so two different programs never claim the same one.

## 6. The gates: what each proves, and what can replace it

A gate's tool may be gone by the time you read this. **The rule for that, which you keep:** a gate whose tool can't be reached **fails**, and says so: "<tool> is unreachable (<what failed>): this gate can't pass without it. Replace it with an equivalent: see TAKING-OVER.md, section 7.<n>". It never passes silently. Today every gate already fails closed, but with the tool's own error, not those words. [BETA.2: each outside call gets that message, and a test that it does.] AGENTS.md says the same for an AI that works here.

| Gate (its workflow) | What it proves | What it needs from outside (section 7) |
|---|---|---|
| **Unit tests** (tests.yml) | every test passes; Hudson can't run in a player's game; online play happens only the safe way; every shader compiles with no warning; the release checks' own tests pass | the Windows runner image (7.6), Visual Studio (7.7), vcpkg's downloads (7.5) |
| **Tests under the address sanitizer** (tests.yml) | the tests pass with every memory error caught (out of bounds, after it's freed, and the like), with the network libraries built with the sanitizer too | the same |
| **Release checks on Ubuntu** (tests.yml) | the same release checks on Linux; the code forms the scanners can't read are refused (CODING-STANDARD.md rule 1) | the Ubuntu runner image (7.6) |
| **Take-outs** (takeouts.yml) | every take-out row's guard is noticed when it's taken out | the Windows image, vcpkg |
| **Build the game** (game-build.yml) | it builds on a clean machine, from source, and the package is exactly what a release ships | the Windows image, Visual Studio, vcpkg |
| **Snyk Code** (snyk.yml) | no known kind of bug in our code, no comment or file telling a scanner to look away, and every C and C++ file read | Snyk (7.1) |
| **Snyk Open Source** (snyk.yml) | no published vulnerability in what the repository holds, and no precompiled code in it | Snyk (7.1) |
| **The vcpkg libraries, built and scanned** (snyk.yml) | every library builds from its pinned source, each download checked by its hash; and no finding or published vulnerability in each library's source as we build it. A library Snyk doesn't know fails, unless its own project's advisories were checked | vcpkg's downloads (7.5), Snyk (7.1), GitHub's API for the advisories (7.4) |
| **Build** (build.yml, test builds) | only avpx_main is built, only once its required checks passed on that commit, and what was built is attested | GitHub's API (7.4), the attestation service (7.3) |
| **Release** (release.yml) | the commit may be released (RELEASE-BLOCKERS.md), the package, at least 20 antivirus engines' answers read as hostile and none flagging it, and a draft only, which a person publishes | VirusTotal (7.2), the attestation service (7.3), GitHub's API (7.4) |
| **Branch protection** (GitHub's) | nothing reaches avpx_main without every gate, for everyone, admins too | GitHub. On another host: a protected branch with every gate required. On none: a rule you keep, that the branch moves only to a commit whose gates you ran and saw pass |

**`.github/snyk-accepted.json`** holds the only exceptions: reports where Snyk's own data is wrong, which no code change can clear. Each names the report and has an end date, and when the date passes, the gate fails again until someone checks it afresh. Keep it that way: an exception that never ends is a hole.

## 7. Each outside tool, and what can replace it

A replacement must prove at least what the tool did, fail on a finding, and fail when it can't run. Treat it as untrusted until checked: get it from its makers, check its signature or hash, pin its version and hash, and show it fails on a copy with a known problem put in on purpose (AGENTS.md).

### 7.1 Snyk

- **What it does for us:** static analysis of our code (Snyk Code), known vulnerabilities in what we hold (Snyk Open Source), and both for every library's source as we build it.
- **What it needs:** its command-line tool, downloaded at a pinned version and checked by sha256; Snyk's service; and the `SNYK_TOKEN` secret. The token will have expired.
- **Replace it with:** for our code, open-source static analysis that fails on a finding and on a file it can't read, such as CodeQL for C and C++, clang-tidy's security checks, or cppcheck. Two different tools see different things. For known vulnerabilities, OSV-Scanner against the OSV database, over the vendored code and every vcpkg port's version and source. Keep the blind-spot rule: a file or library the tool didn't read fails the gate.

### 7.2 VirusTotal

- **What it does for us:** at least 20 antivirus engines look at the package before the owner publishes, and their answers are read as hostile.
- **What it needs:** VirusTotal's service, and its API key in the `release` environment. The key will have expired.
- **Replace it with:** the multi-engine scanning service of your day, or several engines' own command-line scanners, and fail on any flag. A flag is reported to the engine's maker as a false positive only once you've checked it is one. A flagged package is never published while the flag stands.

### 7.3 The attestation service

- **What it does for us:** GitHub's attestation (Sigstore, through GitHub) says that a file came out of this repository's own workflow. `gh attestation verify` checks it.
- **Replace it with:** a list of every published file's SHA-256, signed with a key of your own, with the public key published where your players can find it. That signature, not a service, then says the files are yours (section 5).

### 7.4 GitHub's API

- **What it does for us:** a build checks, through it, that the commit's required checks passed. The library check reads its security advisories.
- **Replace it with:** your host's own way of asking the same, or with no host, the rule in the branch protection row above. For the advisories: the projects' own advisory pages, or OSV, read as hostile.

### 7.5 vcpkg's downloads

- **What it does for us:** each library is built from its source archive, fetched from its project's release, and checked by the hash in its port. vcpkg itself comes from its repository at the manifest's baseline.
- **When it's gone:** upstream archives may disappear. vcpkg stops on a missing download or a wrong hash, so the build fails, and it never builds from something else.
- **Replace it with:** the archived copies, each checked by the same hash. [BETA.2: every library's source archive, kept with its hash beside each release and in an archive repository; until then, RELEASING.md's "Building on a machine without vcpkg".] A copy of the archive you found anywhere is the right one if its hash is the port's.

### 7.6 The runner images

- **What they do for us:** the workflows run on images pinned by name (`windows-2025-vs2026`, `ubuntu-24.04`).
- **When one is retired:** GitHub has no runner for the name, and the job doesn't start. [BETA.2: what GitHub shows then is being checked, so that a retirement is said plainly once.]
- **Replace it with:** the next image, pinned by name, and the toolchain's own pin (7.7), so the same compiler is proven.

### 7.7 Visual Studio

- **What it does for us:** the compiler and its libraries, for every build and test.
- **The pin:** [BETA.2: the exact version, where to get it, how to check it, and the compiler's version recorded in each build's BUILD.txt; today the workflows take the newest one installed.]
- **When it's gone:** a later Visual Studio. Run every test, the sanitizer and the take-outs on it before anything ships, and record its version.

## 8. How the crypto gets upgraded

The design is the server repository's CRYPTO.md, and its TAKING-OVER.md section 8 says how a new version is made. In short: one suite, never negotiated. A new version is a new design, a new number in every message's header, new labels, new test vectors and new proofs, all at once, and a version never accepts another's messages. **What it means here:**
- The game holds the same vectors file as the server, byte for byte (`tests/vectors/crypto-v2.json` [BETA.2, with the version 2 pull requests]). Its SHA-256 must match the server's, or the two don't speak the same protocol.
- A game of a new version is a new release with a new network version number (section 5), and servers.cfg lines for servers of that version.
- **A player's identity** carries its version in its id (`"AVPX-ID-2"`). A new version makes new identities, and players ask to play again.

## 9. What you inherit, and what you replace

| Ours | Yours |
|---|---|
| our servers.cfg lines | keep the ones whose servers still run and whose keys you trust; add your own |
| our signing certificate and attestation identity | your own (section 5) |
| our GitHub settings, tokens and environments | your own host's, or the rules in section 6 |
| our names in the code and the docs | leave them: they say who wrote what, and when. The 1999 authors' names stay too |

Whatever you change, keep the rules this was built on: **security first**, and when it's in the way of anything else, security wins. **Input is hostile:** everything from outside the code is checked before it's trusted. And **anyone after you must be able to do what this page let you do.**
