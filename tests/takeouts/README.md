# Testing the tests

A test that passes only shows the code does what the test looks for. These two tools show whether a test would catch the code going wrong. You need what building the tests already needs:
Visual Studio with the C++ tools, and PowerShell.

## Take-outs

`run.ps1 <test>` makes a copy of the code, breaks one thing in it, builds the test against that copy and runs it, then puts the thing back and does the next. The test should fail every time.

    tests/takeouts/run.ps1 rim_test

- **noticed:** the test failed, or crashed, or ran until it was stopped. That's what should happen.
- **NOT NOTICED:** the test still passed with that thing broken, so that check isn't really being tested. Add a row to the test that catches it. If the change truly makes no difference to what
  the code does, that's worth knowing too: take out the row, or the code.
- **it should match exactly once:** the code has moved on. Update the row in the list.

Each list is `<test>.json`: what to copy, and rows that each name a change, with the file, the exact text and what to put in its place. Lines end in `\n` here, whatever the file uses. The repository
itself is never touched.

Rows whose name starts `SOURCE:` check a line that only a real game can run, such as what's asked of a graphics card. The test reads the source for it, so it only shows the line is there. What
that line does on a card needs a run of the game.

## The address sanitizer

`sanitize.ps1 <test>` builds a test so that any read or write outside what the program was given stops it at once.

    tests/takeouts/sanitize.ps1 relief_map_file_test

A test only sees what it looks for, and this sees the rest. It's slower, so it isn't part of the ordinary run. Run it after changing anything that handles a file.

## What these don't cover

Some checks read the game's own films and pictures, which aren't in the repository and can't be. Those are in `tests/stock`, and need a copy of the game.
