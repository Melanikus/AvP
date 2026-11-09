// When one of the game's checks on itself finds something has gone wrong that can't be put right, this says so in the log and stops
// the game. Written 2026-11-08 by Melanikus for the AvP port (issue #87). See fail.h. Credit: this does the job of Jamie Lokier's
// fail(), from 1995; it's new code, written from what the game's callers need, not from his.
#include "fail.h"
#include "logString.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

size_t Fail_Words(char *words, size_t size, const char *format, va_list arguments)
{
	if (!words || size == 0)
	{
		return 0;
	}

	if (!format)
	{
		format = "something went wrong, and its message is missing";
	}

	// vsnprintf never writes past size, counting its zero, and says how long the whole message would have been. It gives a number below
	// zero only if the format itself can't be read.
	int const wanted = vsnprintf(words, size, format, arguments);

	if (wanted < 0)
	{
		snprintf(words, size, "%s", "something went wrong, and its message couldn't be put together");
		return strlen(words);
	}

	words[size - 1] = '\0';

	return (size_t)wanted < size ? (size_t)wanted : size - 1;
}

void fail(const char *format, ...)
{
	char words[kFailWordsLongest];
	va_list arguments;

	va_start(arguments, format);
	Fail_Words(words, sizeof(words), format, arguments);
	va_end(arguments);

	// The callers' messages mostly end with a line break of their own, and the log adds one, so a break at the end is left off
	size_t length = strlen(words);

	while (length > 0 && (words[length - 1] == '\n' || words[length - 1] == '\r'))
	{
		words[--length] = '\0';
	}

	// The log writes an error at once, before anything else can happen
	LogErrorString(std::string("the game stopped itself: ") + words);

	fprintf(stderr, "%s\n", words);
	fflush(stderr);

	exit(EXIT_FAILURE);
}
