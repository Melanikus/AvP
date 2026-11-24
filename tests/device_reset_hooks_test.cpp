// The test of the hooks that let go of what else lives on the card alone before the device is reset (3dc/device_reset_hooks.cpp). Written 2026-11-23 by
// Melanikus (Frost) for the AvP port.
//
// WHAT IT TESTS
//
// Direct3D refuses to reset a device while anything on the card alone is held, and the game then stays lost (Drake's second hole in the level editor's live
// picker panes' safety case). The hooks are how a part of the program the game knows nothing of (the editor's live panes) lets go of its own in time:
//   (1) every hook added is called once by DeviceReset_ReleaseAll, with its own owner, the last added first; called again at each reset
//   (2) refused, nothing added: no owner, no hook, an owner already added
//   (3) one taken away is never called again; taking away one not added is refused
//   (4) at most kMostDeviceResetHooks at once: one more is refused
//   (5) a hook that adds or takes away one while the hooks are being called is refused, and every hook is still called once
//
// HOW IT RUNS
//
// tests/build.ps1 compiles it with device_reset_hooks.cpp alone. The program gives back a non-zero result if anything failed.
//
// RED FIRST: on the stand-in that keeps no hook, every check fails ((2)'s by its count: nothing is ever held).

#include <stdio.h>
#include <string>
#include <vector>

#include "device_reset_hooks.h"

static int checksFailed = 0;

static void Check(bool passed, std::string const &what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what.c_str());

	if (!passed)
	{
		checksFailed++;
	}
}

// each owner is a number; a hook writes the owner's number into the list of calls
static std::vector<int> calls;

static void Note(void *owner)
{
	calls.push_back(*static_cast<int *>(owner));
}

static int owners[kMostDeviceResetHooks + 2];

// a hook that tries to add owner 90 and take away owner 1 while the hooks are being called
static int meddler = 99;
static int ninety = 90;
static bool addedDuring = true, removedDuring = true;

static void Meddle(void *owner)
{
	calls.push_back(*static_cast<int *>(owner));
	addedDuring = DeviceReset_Add(&ninety, Note);
	removedDuring = DeviceReset_Remove(&owners[1]);
}

static std::string Calls()
{
	std::string all;

	for (int one : calls)
	{
		all += (all.empty() ? "" : " ") + std::to_string(one);
	}

	return all.empty() ? "none" : all;
}

int main()
{
	printf("device_reset_hooks_test: what else lives on the card alone, let go before the device is reset\n");

	for (size_t i = 0; i < kMostDeviceResetHooks + 2; i++)
	{
		owners[i] = (int)i;
	}

	// ---- (1) every hook called once, with its own owner, the last added first; and again at the next reset
	bool const added1 = DeviceReset_Add(&owners[1], Note);
	bool const added2 = DeviceReset_Add(&owners[2], Note);
	DeviceReset_ReleaseAll();
	std::string const first = Calls();
	calls.clear();
	DeviceReset_ReleaseAll();
	std::string const second = Calls();
	calls.clear();

	Check(added1 && added2 && DeviceReset_Count() == 2, "(1) two owners' hooks added (" + std::to_string((unsigned long long)DeviceReset_Count()) + " held)");
	Check(first == "2 1", "(1) a reset calls each once, with its own owner, the last added first (called: " + first + ")");
	Check(second == "2 1", "(1) and the next reset calls them again (called: " + second + ")");

	// ---- (2) refused, nothing added
	bool const noOwner = DeviceReset_Add(nullptr, Note);
	bool const noHook = DeviceReset_Add(&owners[3], nullptr);
	bool const twice = DeviceReset_Add(&owners[1], Note);

	Check(!noOwner && !noHook && !twice && DeviceReset_Count() == 2, "(2) no owner, no hook, and an owner already added: each refused, nothing added (" +
		std::to_string((unsigned long long)DeviceReset_Count()) + " held)");

	// ---- (3) taken away: never called again; one not added: refused
	bool const removed = DeviceReset_Remove(&owners[1]);
	bool const removedAgain = DeviceReset_Remove(&owners[1]);
	DeviceReset_ReleaseAll();
	std::string const afterRemove = Calls();
	calls.clear();

	Check(removed && !removedAgain && afterRemove == "2" && DeviceReset_Count() == 1, "(3) owner 1 taken away is never called again; taking it away twice is refused "
		"(called: " + afterRemove + ")");

	DeviceReset_Remove(&owners[2]);

	// ---- (4) at most kMostDeviceResetHooks
	size_t addedMost = 0;

	for (size_t i = 0; i < kMostDeviceResetHooks; i++)
	{
		addedMost += DeviceReset_Add(&owners[i], Note) ? 1 : 0;
	}

	bool const oneMore = DeviceReset_Add(&owners[kMostDeviceResetHooks], Note);

	Check(addedMost == kMostDeviceResetHooks && !oneMore && DeviceReset_Count() == kMostDeviceResetHooks, "(4) " + std::to_string((unsigned long long)kMostDeviceResetHooks) +
		" owners added, and one more refused (" + std::to_string((unsigned long long)addedMost) + " added)");

	for (size_t i = 0; i < kMostDeviceResetHooks; i++)
	{
		DeviceReset_Remove(&owners[i]);
	}

	// ---- (5) a hook that adds or takes away one while the hooks are being called is refused; every hook is still called once
	DeviceReset_Add(&owners[1], Note);
	DeviceReset_Add(&meddler, Meddle);
	DeviceReset_Add(&owners[2], Note);
	DeviceReset_ReleaseAll();
	std::string const meddled = Calls();
	calls.clear();

	Check(meddled == "2 99 1" && !addedDuring && !removedDuring && DeviceReset_Count() == 3, "(5) a hook adding owner 90 and taking away owner 1 while the hooks are "
		"called is refused both, and each hook is called once (called: " + meddled + "; " + std::to_string((unsigned long long)DeviceReset_Count()) + " held)");

	printf(checksFailed ? "device_reset_hooks_test: %d checks FAILED\n" : "device_reset_hooks_test: all checks passed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
