// The custom maps you can play in single player. Written 2026-11-07 by Melanikus for the AvP port. Credit: Melanikus and Bobblen (the custom
// single-player maps follow Bobblen's avp-forever, rewritten).
//
// A custom map is a level file someone has put in avp_rifs/Custom. The single-player menu lists the ones made for single player
// (see 3dc/level_kind.h for how that's told) whose names keep to the rule in 3dc/custom_map_name.h. Each one is read when the list is
// made, as hostile input: a file that's too big, can't be unpacked or doesn't add up is left out, with one line in the log saying why.
//
// A map can come with a picture for the menu: a .RIM picture with the map's name, either beside the map (avp_rifs/Custom/<name>.rim)
// or where Bobblen's avp-forever keeps them (graphics/Menus/<name>_briefing.rim). Both paths are built from the checked name, inside
// fixed folders, and the picture is read with the game's checked picture reader (RimLoader).

#pragma once

#include "TextureManager.h"

// Finds the custom maps made for single player, sorted by name. Each call finds them afresh, so a map added while the game runs shows
// up the next time the menu is opened.
void CustomMaps_Find(void);

// How many were found, and each one's name: NULL for a number that isn't one of them
int CustomMaps_Count(void);
const char *CustomMaps_Name(int which);

// Which of them has this name, in any case, or -1. The name is checked against the rule first, and only ever compared with the list: it's
// never used to build a path itself.
int CustomMaps_Named(const char *name, size_t room);

// Puts "Custom/<name>" into LevelName, for the game to load. False, with LevelName unchanged, for a number that isn't one of them.
bool CustomMaps_SetLevelToLoad(int which);

// The map's own picture, or MISSING_TEXTURE if it has none or its picture can't be read. The picture stays loaded until
// CustomMaps_ForgetPictures.
texID_t CustomMaps_Picture(int which);
void CustomMaps_ForgetPictures(void);

// The size of a map's file and its MD5, so that two players can see if they have the same map. See coop_lobby.h: they're only ever compared.
// False for a number that isn't one of the maps, or a file that can't be read. Each map's file is read once, however often this is asked.
bool CustomMaps_SizeAndSum(int which, unsigned int *size, unsigned char sum[16]);

// Whether the level being played is a custom map, so that finishing it opens no next level and unlocks nothing. Set as one is started,
// cleared as a stock level is.
void CustomMaps_SetPlaying(bool playing);
bool CustomMaps_Playing(void);
