# Our game files

The pictures, models and sounds the game needs that aren't part of the player's own copy of Aliens versus Predator. They're laid
out here the way the game folder wants them, so a tester pastes the package into their game folder and each lands where the game
looks for it. Written 2026-11-07 by Melanikus for the AvP port.

**What goes in the package is exactly what `SHA256SUMS.txt` lists,** each with its SHA-256. `tools/build-and-package.ps1` checks
every file against its line before copying it in, and refuses a file here that isn't listed. `tools/check-release-package.ps1`
refuses a package that hasn't got every listed file, byte for byte. So a file can't change without its line changing too, and that
line shows in the pull request that changes it. `.gitattributes` stops git changing a line ending in any of them.

**None of them replaces a file of the player's own game.** The player's `avp_huds\marwep.rif` stays theirs: without
`MelPyth_MarWep.rif`, the guns it holds come from that file instead.

## Where they came from

They're from **MelPyth M5**, by Melanikus and **Astro_Muppet** (Mupp), who was its coder. Every RIF and 3D model in M5 was made by hand
by Melanikus in 3ds Max R3, as its modeller and animator.

**Melanikus's files, unchanged** (their hashes match the M5 files Melanikus gave us):

- `avp_huds/MelPyth_MarHUD.RIF`, `avp_huds/Staff_Hud.RIF`, `avp_huds/StaffPickup.RIF`
- 15 pictures in `graphics/MelPyth/HUDs/Marine/`. `SYMBOLS.RIM` is in capitals, as the game asks for it.

**Ours, made from Melanikus's files** by the tools on the co-op branch:

- `avp_huds/MelPyth_MarWep.rif`: the pulse rifle with its counter, the cudgel and the pistols.
- `avp_huds/ShotgunPickup.rif`: Melanikus's file, without `girl.rim` and its 6 polygons, by Melanikus's ruling.
- `MelPyth/fastfile/common.ffl`: only the 4 new sounds from M5. The game's own sounds come from the player's copy.

## How they were checked

Every file here is read by the game as data from outside the code, so each went through the game's own loaders before it came in:

- The 5 model files through the real level loader (`stock_load_test`), plain and with the address sanitizer: all 5 load
  (58,889 chunks). Ten damaged copies of `MelPyth_MarWep.rif` were all refused first, with no crash and nothing from the sanitizer.
- `common.ffl` through the game's real sound loading (`LoadNewWeaponSounds` and `ExtractWavFile`): all 4 sounds load, and four
  damaged copies were refused with the right reasons.
- The 15 pictures are byte for byte the ones that passed the same loader.

Not covered by those: the loaders don't build the models' moving parts, and nothing has been seen in the game from this folder yet.
