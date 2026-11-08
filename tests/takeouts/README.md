# Testing the tests

A test that passes only shows the code does what the test looks for. These two tools show whether a test would catch the code going wrong. You need what building the tests already needs:
Visual Studio with the C++ tools, and PowerShell.

## Take-outs

`run.ps1 <test>` makes a copy of the code, breaks one thing in it, builds the test against that copy and runs it, then puts the thing back and does the next. The test should fail every time.

    tests/takeouts/run.ps1 rim_test

- **noticed:** the test failed, or crashed, or ran until it was stopped. That's what should happen.
- **NOT NOTICED:** the test still passed with that thing broken, so that check isn't really being tested. Add a row to the test that catches it. If the change truly makes no difference to what
  the code does, that's worth knowing too: take out the row, or the code.
- **CAN'T FIND ITS TEXT, or FINDS ITS TEXT MORE THAN ONCE:** the code has moved on. See below.
- **CAN'T RUN:** the row names a file that isn't there, or its change leaves code that doesn't build. See below.

## When the Take-outs check fails

A row that can't run fails the check, the same as one that isn't noticed. That's on purpose: if the code a row breaks has moved, the check it guards may be untested now, and nobody would know.
The log names the list, the row (counted from 0), the file and the text, and says what to do. Fix it in the same pull request as the change that moved the code:

- **The code is still there, in different words** (a renamed variable, a new argument): change the row's `"from"` to the code as it is now, and check its `"to"` still breaks it the way the
  row's name says. This finds the places most like the old text, and with `--write` puts the best one in the row:

      node tests/takeouts/find-moved-row.js npc_crashes_test 2
      node tests/takeouts/find-moved-row.js npc_crashes_test 2 --write

  It never decides for you: it changes only `"from"`, never `"to"`. Read the place, look at the change with `git diff`, and run the row: `tests/takeouts/run.ps1 npc_crashes_test -From 2 -To 2`.
- **What it checks moved to another place, or is gone:** remove the row, and add one that breaks the check where it is now. If the check is gone on purpose, say why in the pull request.
- **Its text is there more than once:** make `"from"` longer, with the lines around the one you mean, until it's there only once.
- **NOT NOTICED:** the test still passes with that broken. Make the test notice it, or, if it truly doesn't matter any more, remove the row and say why.
- **It didn't build:** a row's `"to"` has to leave code that builds, so the only thing broken is what the row is about.

On GitHub the same words are in the job's log (Take-outs for `<list>`, Run the list), and each failed row is a note on the pull request's page.

**On GitHub,** the Take-outs check (`.github/workflows/takeouts.yml`) runs the lists a pull request can affect, each on a clean machine, and fails if
any row isn't noticed or can't be run. A pull request that changes the check or `run.ps1` runs every list. Anyone who can run workflows can run any of them, or
all, by hand: Actions, Take-outs, Run workflow.

Each list is `<test>.json`: what to copy, and rows that each name a change, with the file, the exact text and what to put in its place. Lines end in `\n` here, whatever the file uses. The repository
itself is never touched.

Rows whose name starts `SOURCE:` check a line that only a real game can run, such as what's asked of a graphics card. The test reads the source for it, so it only shows the line is there. What
that line does on a card needs a run of the game.

## The address sanitizer

`sanitize.ps1 <test>` builds a test so that any read or write outside what the program was given stops it at once.

    tests/takeouts/sanitize.ps1 relief_map_file_test

A test only sees what it looks for, and this sees the rest. Every pull request runs it on every test, in the Tests check's "Tests under the address
sanitizer", split across four machines because it's slower than the ordinary run, and a test that doesn't pass under it stops the merge. Run it here
too, after changing anything that handles a file, to find out before the pull request does.

Each test is run from a window of its own there. Run several from one window here and that's fine too: Visual Studio's setup is done once, and the
script stops and says why if the setup didn't give it the compiler. A test built for the sanitizer has the C runtime built in, as every test does,
but the sanitizer's own library uses the runtime's DLLs while it runs, so a pass under the sanitizer says nothing about running without them.

The same check makes sure the gate can fail: in its own copy of the repository it writes a test that reads one byte past the end of a block, and
one that fails a check, and neither may pass. When a test doesn't pass, the script prints everything the test and the sanitizer said, because the
folder that keeps it is gone once an automatic check ends. And it refuses a name that isn't one of the tests before it empties any folder.

The sanitizer only watches code compiled with it, so the script refuses a test that links an object or a library compiled without it, and a test
whose settings turn the sanitizer off for its own code (the compiler has /fno-sanitize=address). It reads each object, those the test links and
those its own build makes, for the mark the compiler leaves in what it compiles with the sanitizer. A library vcpkg builds, such as ENet for the network tests, is
taken from its copy built with the sanitizer, in `vcpkg_installed/x86-windows-static-asan`, which the automatic check makes
(`vcpkg-overlay-triplets/x86-windows-static-asan.cmake`). So here, without that copy, the network tests can't be run under the sanitizer, and the
script says so. Windows' own libraries, Microsoft's D3DX and Microsoft's C runtime are Microsoft's, and aren't compiled with it.

## What these don't cover

Some checks read the game's own films and pictures, which aren't in the repository and can't be. Those are in `tests/stock`, and need a copy of the game.
