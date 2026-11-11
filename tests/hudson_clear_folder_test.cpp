// Runs tests/hudson_clear_folder_test.ps1, which tries the one thing a Hudson run deletes on folders made for the purpose. Written 2026-11-05 by Melanikus for the AvP port.
//
// WHY THIS IS A PROGRAM: the automatic checks run every program tests/build.ps1 builds, and only those. What's being tested is a PowerShell script, so the test has to be one too. This small program is what gets it
// run, and its result is the script's result. /FC in this test's settings makes __FILE__ the whole path of this file, which is how the script is found.
//
// PowerShell is started by its whole path, from where Windows says it lives, and with no command prompt in between. A bare name would be looked for in the current folder first, and a file there could be run instead.

#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>

int main()
{
	std::string script = __FILE__;
	std::string::size_type const extension = script.rfind(".cpp");
	char const *const windows = getenv("SystemRoot");

	if (extension == std::string::npos || !windows)
	{
		printf("FAILED: can't tell where this test is, or where Windows is\n");
		return 1;
	}

	script.replace(extension, 4, ".ps1");

	std::string const powershell = std::string(windows) + "\\System32\\WindowsPowerShell\\v1.0\\powershell.exe";

	// the words are joined up with spaces as they stand, so a path that may hold a space needs its own quotes
	std::string const quotedScript = "\"" + script + "\"";

	fflush(stdout);

	intptr_t const result = _spawnl(_P_WAIT, powershell.c_str(), "powershell.exe", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", quotedScript.c_str(), (const char *)NULL);

	if (result != 0)
	{
		printf("FAILED: the script gave back %ld\n", (long)result);
		return 1;
	}

	return 0;
}
