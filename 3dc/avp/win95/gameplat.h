#ifndef _included_gameplat_h_ /* Is this your first time? */
#define _included_gameplat_h_ 1

/*
	This contains project platfrom specifc externs
	and prototypes. Only valid when we are calling one
	p\p function from another.
*/

extern void LoadRifFile();     // the game's: a level that can't be loaded stops the game (exit code 0x3421)
extern bool TryLoadRifFile();  // the same, but gives back false instead of stopping (the editor's)

extern int WindowMode;

#endif
