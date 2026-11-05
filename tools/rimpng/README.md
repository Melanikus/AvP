# rimpng: making hand-made maps for the game

This tool copies the game's pictures out as PNGs, so you can paint or generate maps from them, and turns your finished maps into files the game can read. It only reads what you tell it to, and writes only where you tell it to. Give it copies of the game's files, not the game's own folder.

## Building it

You need Visual Studio with the C++ tools, and nothing else. In PowerShell, from anywhere:

    tools/rimpng/build.ps1

The program lands in a folder called `AvP-test-out\rimpng` next to the repository. `tests/rimpng_test.cpp` builds the same code in the automatic checks, so it can't quietly stop building.

## Using it

**Copy the pictures out of the game's fastfiles.** Point it at a folder of copied `.ffl` files:

    rimpng extract <folder of copied .ffl files> <folder for the pictures>

**Turn pictures into PNGs,** one or a whole folder:

    rimpng topng <picture.rim> <picture.png>
    rimpng topng <folder of pictures> <folder for the PNGs>

**Turn a finished map into a file for the game:**

    rimpng tomap <map.png> <name_Normal.rim> [--flip-green] [--flip-both] [--side N]
    rimpng tomap <depth.png> <name_Depth.rim>

It tells you what's wrong if it can't use a file, and it won't write a map the game would refuse.

## What the game reads, and where

Put the map beside the picture it belongs to, with the ending added to its name, loose in the game's folder:

| Picture | Relief map | Depth map |
|---|---|---|
| `graphics/Envrnmts/Derelict/Hive01.rim` | `graphics/Envrnmts/Derelict/Hive01_Normal.rim` | `graphics/Envrnmts/Derelict/Hive01_Depth.rim` |

A picture can have either, both or neither, and each is read on its own. A film has neither, because it changes while it plays.

Both are read while a level loads. They're only read at all if the level is loaded with a lighting that draws maps, or with `-dev`.

**The relief map** says which way each pixel of the surface leans. Red leans to the right of the picture, green to the bottom (the way tools call "DirectX", or "Y-"), and blue faces straight out. A flat pixel is 128, 128, 255. If your image's green means "up" (the way tools call "OpenGL"), use `--flip-green`. `--flip-both` turns every slope round.

**The depth map** says how deep each pixel is. White is the level of the polygon, and black is as deep as the surface goes. It should be grey: the game averages red, green and blue.

`tomap` can also write `name_Normal.dds`, which takes a quarter of the memory, but **the game doesn't read a `.dds` yet**. It notices a `name_Depth.dds` and says so in the log, and doesn't read that either.

## The rules

- A map is square, a power of two, from 4 to 2048 a side. `--side N` makes one that size. It needn't be the size of its picture.
- A file can be up to 32 MB.
- The most a level's maps can do: try 2,048 files, and spend three times the limit below on reading and unpacking them. A map that's used and a map that's refused cost the same. After that no more are read until the next level.
- All the maps kept can come to 256 MB in all, counting smaller copies, at 4 bytes a pixel. A 1024 x 1024 map takes about 5.6 MB.
- A `.rim` picture has its own limits, which are separate: 8192 on a side and 2048 x 2048 in all, and 256 MB for all the pictures a level opens.

If the game doesn't use a map, `avp_log.txt` says why, in a line that starts `texture relief:`. The log is overwritten every time the game starts, so copy it first.

## How far this has been tested

The checks on a file are the game's own, in `3dc/relief_map_file.h`, and `tests/relief_map_file_test.cpp` and `tests/texture_relief_test.cpp` try to break them. Whether a map looks right on a wall is something only the game can show.
