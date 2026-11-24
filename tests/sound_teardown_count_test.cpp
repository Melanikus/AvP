// The test for LoadedSounds_Teardowns (3dc/avp/win95/jsndsup.h): the count of the times the game let go of every loaded sound at once.
// Written 2026-11-24 by Melanikus (Vasquez) for the AvP port.
//
// WHAT IT TESTS
//
// When a level closes, LoseAllNonCommonSounds frees every LOADED_SOUND, however many holders each still has. A holder that kept its pointer
// and used it afterwards read freed memory: the level editor did, and played the wrong sound in a loop (its sanitizer saw the read as a
// heap-use-after-free). Asking the game's list whether the pointer is still in it isn't safe: the memory of a freed sound can be handed to
// the next sound loaded, even for the same file, and the old pointer then looks alive. So the game counts its teardowns, and a holder
// compares the count it noted with the count now. This checks that the count goes up at every teardown, an empty one too, and at nothing
// else: not when a sound is loaded, not when a holder gives one back.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this with the game's own jsndsup.cpp, its list and its memory (list_tem.cpp, our_mem.cpp), and the program gives
// back a non-zero result if any check failed. It needs no game files and no sound device: the part of the game that loads a sample and plays
// it is stood in for below.

#include <stdio.h>

#include <string>

#include "3dc.h"
#include "inline.h"
#include "module.h"
#include "psndplat.h"
#include "list_tem.hpp"
#include "jsndsup.h"

extern List<LOADED_SOUND *> loaded_sounds;   // jsndsup.cpp: every sound loaded now
extern void LoseAllNonCommonSounds();        // jsndsup.cpp, as projload.cpp declares it

// ---- Stand-ins for the sound device's side of the game (psnd.cpp, psndproj.cpp, openal_audio.cpp) and for the messages our_mem.cpp gives
int SoundSwitchedOn = 1;                    // there is a sound device
SOUNDSAMPLEDATA GameSounds[SID_MAXIMUM];    // the game's sample slots
static int unloaded = 0;                    // how many samples were unloaded

void PlatUnloadGameSound(SOUNDINDEX index)
{
	unloaded++;
	GameSounds[index].loaded = 0;
}

int FindAndLoadWavFile(int, const char *)   // the file is found and loaded
{
	return 1;
}

void InitialiseBaseFrequency(SOUNDINDEX)
{
}

void Sound_ForgetAddressesInside(const void *, size_t)   // psnd.cpp's, which our_mem.cpp calls on every free: no sound plays here
{
}

void LogErrorString(const std::string &text)
{
	printf("  (game log) %s\n", text.c_str());
}

void LogErrorStringC(const char *text)
{
	printf("  (game log) %s\n", text ? text : "");
}

void avp_MessageBox(const char *message, int)
{
	printf("  (game message box) %s\n", message ? message : "");
}

void avp_exit(int code)   // our_mem.cpp stops the game when memory runs out: here that is a failed run
{
	printf("SOME CHECKS FAILED: the game's code stopped the program (code %d)\n", code);
	exit(1);
}

static int checksFailed = 0;

static void Check(bool good, const std::string &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static void TestTheCount()
{
	unsigned long long const atStart = LoadedSounds_Teardowns();

	LOADED_SOUND const *const first = GetSound("sound/teardown.wav");

	Check(first != nullptr && LoadedSounds_Teardowns() == atStart, "loading a sound is not a teardown");

	LOADED_SOUND const *const again = GetSound("sound/teardown.wav");

	Check(again == first && first->num_attached == 2, "a second holder of the same file gets the same sound");

	LoseSound(again);
	Check(loaded_sounds.size() == 1 && LoadedSounds_Teardowns() == atStart, "a holder giving its sound back is not a teardown, and the sound stays for the other");

	// what a holder does: note the count when it gets its sound
	unsigned long long const noted = LoadedSounds_Teardowns();

	LoseAllNonCommonSounds();
	Check(loaded_sounds.size() == 0 && unloaded == 1, "the teardown let go of the sound, though a holder still had it");
	Check(LoadedSounds_Teardowns() == noted + 1, "and it counted: the holder sees a number other than the one it noted, so it knows its sound is gone");

	LoseAllNonCommonSounds();
	Check(LoadedSounds_Teardowns() == noted + 2, "a teardown with nothing loaded counts too, so a holder never has to know what was loaded");

	// the case the count is for: the file loaded anew may get the freed sound's very memory, so its address tells the old holder nothing
	LOADED_SOUND const *const anew = GetSound("sound/teardown.wav");

	printf("  (the sound loaded anew %s the freed one's address)\n", anew == first ? "HAS" : "does not have");
	Check(anew != nullptr && LoadedSounds_Teardowns() != noted, "a sound loaded after the teardown leaves the number changed for the holder from before it");

	LoseSound(anew);
	Check(loaded_sounds.size() == 0 && LoadedSounds_Teardowns() == noted + 2, "and giving that one back is not a teardown either");
}

int main()
{
	setvbuf(stdout, nullptr, _IONBF, 0);   // every line out at once: if the game's code crashes, the checks before it still show

	printf("The count of the game's sound teardowns:\n");

	TestTheCount();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");

	return checksFailed ? 1 : 0;
}
