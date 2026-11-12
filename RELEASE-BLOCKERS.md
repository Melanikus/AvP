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

- **The save loader's proof (SAVE-HOSTILE-1).** Its checks are in, but the test runs that prove them on broken saves haven't been done yet, and a new save doesn't yet say which level it belongs to.
