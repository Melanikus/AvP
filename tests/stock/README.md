# Checks that need a copy of the game

The game's own films and pictures aren't in the repository, and can't be. These two programs read them, so they need a copy of the game to point at. Neither holds anything from the game, and
neither writes anywhere. Use a copy: nothing here needs the game's own folder.

    tests/stock/build.ps1

## Does every film still open, play and close?

    film_close_check <folder of films>

It opens, decodes and closes every film through the real decoders, built with the address sanitizer, alone and two at once. Any read or write outside what the decoders were given stops it.
Films are loose files, in the game's `FMVs` folder. Last run on 2026-11-04: 23 Bink and 54 Smacker films, with no errors.

## Does every picture still come out the same?

    rim_fingerprints <folder of pictures> > after.txt

It prints a fingerprint of every pixel of every picture through the real `RimLoader`. Run it on two versions of the loader, and compare the two lists. Any difference means the change reached
the game's own pictures. Last run on 2026-11-05: all 1,028 the same.

Most of the game's pictures are inside its fastfiles, so they have to be copied out first, and `rimpng extract` in `tools/rimpng` does that.

To compare with an older loader, check that version of `3dc/RimLoader.cpp` and `3dc/RimLoader.h` out, build again, and run it again.
