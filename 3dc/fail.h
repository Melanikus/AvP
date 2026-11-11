// When one of the game's checks on itself finds something has gone wrong that can't be put right, this says so in the log and stops
// the game. Written 2026-11-08 by Melanikus for the AvP port (issue #87). Credit: this does the job of Jamie Lokier's fail(), from 1995,
// whose own terms let only Rebellion use it. It's new code, written from what the game's callers need, not from his.
//
// WHO CALLS IT: the checks in the game's lists (list_tem.hpp) and shared objects (refobj.hpp, reflist.hpp), such as "this list has no
// entry there" or "this object was let go of more times than it was taken". They're only in a build made for finding bugs, one without
// NDEBUG. A release build, the one players get, leaves those checks out altogether, so it never calls this.
//
// WHAT IT DOES: it puts the message together from a format and its values, the way printf does, and writes it to the game's log as an
// error at once, so it's there however the game ends. It also writes it to the console, for a game started from one. Then it ends the
// game with the code that means it failed (EXIT_FAILURE). It never comes back.
//
// fail.cpp needs nothing from the game but the log, so tests/fail_test.cpp can try it with a log of its own.

#ifndef FAIL_H
#define FAIL_H

#include <stdarg.h>
#include <stddef.h>

// The longest message, in characters. A longer one is cut short. The callers' longest is about 70, with a type's name added.
static const size_t kFailWordsLongest = 1024;

// Puts a failure's message together into words, which has room for size characters, and always ends it with a zero. It works from
// format and the values in arguments, as vprintf does. A missing format gives a message saying so, and a message too long is cut
// short to fit, never written past the end. Gives back how many characters it wrote, not counting the zero.
size_t Fail_Words(char *words, size_t size, const char *format, va_list arguments);

// list_tem.hpp turns "fail" into "if (0)" in a release build, so that its checks come to nothing. A file that includes this after it
// mustn't declare the function then, because the declaration would read "if (0)" too.
#ifndef fail

// Writes the message to the log as an error, and ends the game with EXIT_FAILURE. It never returns.
[[noreturn]] void fail(const char *format, ...);

#endif

#endif
