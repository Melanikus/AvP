// Modified 2026-11-24 by Melanikus (Vasquez) for the AvP port:
//   LoadedSounds_Teardowns: how many times the game has let go of every loaded sound at once (see jsndsup.cpp)

#ifndef _jsndsup_h
#define _jsndsup_h 1

#include "3dc.h"
#include "inline.h"
#include "module.h"

typedef struct loaded_sound
{
	int sound_num;

	char * wavname;
	int num_attached;

	unsigned long permanent :1;

} LOADED_SOUND;


void LoseSound (LOADED_SOUND const * ls);
LOADED_SOUND const * GetSound (char const * fname);

// How many times the game has let go of every loaded sound at once (LoseAllNonCommonSounds, run when a level closes). That frees each
// LOADED_SOUND whoever still holds it, so a holder notes this number when it gets its sound, and if the number has changed since, the sound
// it holds is gone: it must forget it without touching it or giving it back. Comparing the number is safe where comparing the address isn't:
// the memory a freed sound had can be handed to the next sound loaded, even for the same file. It only ever goes up, and it is 64 bits wide,
// so it can't come round to an old value (a level closed every second would take longer than the universe's age to wrap it).
unsigned long long LoadedSounds_Teardowns (void);

#endif
