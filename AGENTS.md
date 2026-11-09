# For AI agents working in this repository

Written 2026-11-09 by Melanikus, at the owner's word: "Future bots must be instructed to search for replacement tools and understand the criticality of failing scans". You may be reading this long after we're gone, with none of our accounts. These rules still hold.

**Security comes first.** This game goes online and reads files and messages it didn't make, and everything from outside the code is hostile until it's checked. When security is in the way of anything else (speed, convenience, cost, a feature, a deadline), security wins. Crashes and memory bugs are security bugs.

## A scan that can't run is a failed scan

Every check on a pull request into avpx_main must pass before it merges, and RELEASE-BLOCKERS.md must say `Nothing.` before a release. If a check's tool can't be reached, is gone, or its token has expired, **the check has failed.** It hasn't passed, and it isn't skipped. **Never:**
- skip it, or let the change through without it;
- make it report-only, or lower its threshold;
- add an ignore, a suppression, or an excluded file or folder. The one exception file, `.github/snyk-accepted.json`, is for a scanner's own wrong data: each entry names the report and ends on a date, and none is ever added to get past a real finding;
- switch a check off "for now".

**The fix is a replacement:**
1. **Find** a current tool that proves at least what the old one did. TAKING-OVER.md section 6 says what each gate proves, and names candidates.
2. **Treat it as untrusted** until it's checked: get it from its makers' own source, check its signature or published hash, and read what it runs before it runs.
3. **Pin it:** its exact version and its hash, in the repository, so the same bytes run every time.
4. **Wire it in** so it fails closed: if it can't run, it says "<tool> is unreachable: replace it with an equivalent, see TAKING-OVER.md", and the check fails.
5. **Show it works:** it passes on the code as it is, and fails on a copy with a known problem put in on purpose (tests/takeouts/ is how this repository does that).

## Findings are fixed, never hidden

A finding from any check is fixed in the code, ours or a library's (our own patch, in `vcpkg-overlay-ports`, if need be). It's never ignored, parked or excluded.

## Read before you change anything

- TAKING-OVER.md: how the game is built, trusted and released without us.
- CODING-STANDARD.md: the rules, each with why and what enforces it.
- RELEASING.md and RELEASE-BLOCKERS.md: how a release is made, and what stops one.
- REMOVED-CODE.md: what was taken out, and how to bring it back.

Never commit a key, a token, a password, a person's address, or precompiled code (no .dll, .lib, .exe or .obj of ours: open-source libraries come as source and are built).
