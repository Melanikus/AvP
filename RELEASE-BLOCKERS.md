# What's stopping a release

The release workflow reads this file, and it won't make a release while anything is listed under the heading below. This is how "nothing
is released while we know about an open hole" gets held to by a machine, and not by somebody remembering.

**When something on the list is really done,** take its line out in the same pull request as the fix. **When nothing is left,** the only
line under the heading should be the word `Nothing.` An empty list isn't taken to mean that, because a file cut short by a mistake would
look the same.

**Plain words only.** This file is public, so it says what's in the way, and not how to use a hole that's still open. Issue #20 says the
same for people following along.

RELEASING.md says how a release is made.

## What's in the way

- Level, memory and hardening fixes haven't merged. Custom levels are something people download, so they're treated as hostile, and until these land, a crafted level can still crash the game.
- The shader compiler doesn't ship with the game yet, so the package may not start on a Windows without the old DirectX runtime.
- Save files and the network still have known issues to fix or to rule out.
- CodeQL isn't one of the required checks yet. When it is, its name goes in tools/check-release.ps1 too.
- The notes that ship in the package (PLAYTEST.md) still call it a test build and say known security bugs are open. They have to say what's true on the day.
- Nobody has started the package on a clean Windows. No check here can prove it starts there, because the machines that run the checks have files a player's may not. A person has to try it, and then take this line out.
