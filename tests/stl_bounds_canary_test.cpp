// A canary for the standard library's bounds checks (#140, card STL-HARDEN-1). Written 2026-11-08 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// Every test is built with the standard library's own bounds checks (/D_MSVC_STL_HARDENING=1 in its .args), and
// tests/every_test/every_test.h refuses to compile a test without them. This one proves, by running, that the checks really do stop a
// program: it starts a copy of itself that asks an empty std::vector for its first item, one place past its end, and that copy has to stop
// at once with the code the standard library's checks stop a program with, 0xC0000409 (a "fail-fast"). If the copy prints what was there
// and ends normally, the checks aren't working, and every other test's "past the end" checks could be passing by luck.
//
// It runs the copy with _spawnl and waits for it, so it needs nothing from the script that runs the tests.
#include <windows.h>

#include <process.h>
#include <stdio.h>
#include <string.h>
#include <vector>

static int checksFailed = 0;

static void Check(bool good, const char *what)
{
	printf("%s  %s\n", good ? "ok    " : "FAILED", what);
	checksFailed += good ? 0 : 1;
}

// What the copy does: asks an empty list for its first item. An empty list has none. Where its first item would be is its end, so this
// looks one place past the end, on purpose, which is what the bounds checks are there to stop. The standard library checks front() with
// the same switch as a look at a place by number (_MSVC_STL_HARDENING_VECTOR), and stops the program the same way.
//
// It used to look at place 3 of a list of 3, by number. A code scanner reads a list's size used as a place in it as a mistake, as it
// should, because in real code it nearly always is one. This copy's mistake is meant, and asking a list that is plainly empty for its first item
// says so to anyone reading.
static int LookPastTheEnd()
{
	std::vector<int> const empty;
	printf("an empty list's first item is %d\n", empty.front());
	return 0;   // only reached if nothing stopped it
}

int main(int argc, char **argv)
{
	if (argc > 1 && strcmp(argv[1], "past-the-end") == 0)
	{
		return LookPastTheEnd();
	}

	// This program's own path, from Windows, whatever the folder it was started from
	char self[MAX_PATH] = { 0 };
	DWORD const length = GetModuleFileNameA(NULL, self, MAX_PATH);
	Check(length > 0 && length < MAX_PATH, "the canary knows where it is");

	intptr_t const code = _spawnl(_P_WAIT, self, self, "past-the-end", NULL);
	Check(code != -1, "it starts a copy of itself");
	Check((unsigned int)code == 0xC0000409u, "and the copy, looking one place past the end of a list, is stopped with 0xC0000409 by the standard library's checks");

	if (checksFailed)
	{
		printf("The bounds checks canary FAILED: %d check(s); the copy gave back 0x%08X\n", checksFailed, (unsigned int)code);
		return 1;
	}

	printf("The bounds checks canary passed\n");
	return 0;
}
