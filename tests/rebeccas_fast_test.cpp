// Rebecca's test, built the way the game ships. Written 2026-11-14 by Melanikus for the AvP port.
//
// It's the same test as rebeccas_test.cpp, word for word, with the same game files, but built with /O2 and the compiler's fast rules for
// fractions (/fp:fast, rebeccas_fast_test.args), as win32/VS2026/AvP.vcxproj builds the game's Release. Under those, comparisons aren't
// promised to see a value that isn't a number, so every check here that such a value is refused shows it's refused where the game runs.
// rebeccas_test itself stays built with the ordinary rules, so the two together show the checks hold either way.

#include "rebeccas_test.cpp"
