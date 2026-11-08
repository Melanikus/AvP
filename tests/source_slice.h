// A piece of the game's source, for the tests that read it. Written 2026-11-08 by Melanikus for the AvP port (SOURCE-SLICE-1, #128).
//
// Many tests can't run the code they check, because it needs the whole game, so they read its source instead: they take the text from one
// marker up to the next, and look inside it. Each test used to carry its own copy of that, and every copy gave back an empty text, saying
// nothing, when a marker wasn't there. A check that something IS in the text then failed, which is right. But a check that something is NOT
// in it ("no Network", "no while (1)") passed on nothing. That's how a check could stand for days unable to fail (shoulder_lamp_test, #128).
//
// This is the one copy. If either marker is missing, it says so as a failed check of the test's own, naming the marker, and gives back an
// empty text. A marker goes missing when the code it names is moved or renamed, and the test then has to be pointed at where it went.
//
// The copies didn't all look for the end in the same place, and each test keeps its own way, so no piece of text it reads changes:
// - from the start of the first marker (kSliceEndFromTheStart),
// - from one character on (kSliceEndFromOneOn),
// - or from just after the first marker (kSliceEndAfterTheStart).
// They differ only when the end marker could be found inside the first one.
#ifndef SOURCE_SLICE_H
#define SOURCE_SLICE_H

#include <string>

enum SourceSliceEnd
{
	kSliceEndFromTheStart,
	kSliceEndFromOneOn,
	kSliceEndAfterTheStart
};

// The text from from up to the next to after it, the end looked for as endFrom says. fail is the test's own way of failing a check, given
// what's missing in words.
inline std::string SourceSlice(const std::string &text, const std::string &from, const std::string &to, SourceSliceEnd endFrom,
	void (*fail)(const std::string &why))
{
	std::string::size_type const start = text.find(from);

	if (start == std::string::npos)
	{
		fail("SOURCE: the code this test reads has \"" + from + "\"");
		return std::string();
	}

	std::string::size_type const lookFrom = endFrom == kSliceEndAfterTheStart ? start + from.size() : (endFrom == kSliceEndFromOneOn ? start + 1 : start);
	std::string::size_type const end = text.find(to, lookFrom);

	if (end == std::string::npos)
	{
		fail("SOURCE: the code this test reads has \"" + to + "\" after \"" + from + "\"");
		return std::string();
	}

	return text.substr(start, end - start);
}

#endif
