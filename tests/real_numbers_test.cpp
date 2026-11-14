// The test for 3dc/real_numbers.h, the bit tests that tell a real number from NaN and from endless. Written 2026-11-14 by Melanikus for the
// AvP port.
//
// WHAT IT TESTS
//
// Each test on real numbers, NaN of every kind (quiet, signalling, negative) and endless both ways, for floats and doubles, and the range
// test on numbers inside, at the ends, past them, and NaN. It's built the way the game ships, with /O2 and /fp:fast (real_numbers_test.args),
// under which comparisons aren't promised to see a NaN, so it shows the bit tests hold where the game runs them. Each value is made from
// its bits at run time, through a volatile, so the compiler can't work any of the answers out while it builds.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this, and the program gives back a non-zero result if any check failed. It needs no game files.

#include "real_numbers.h"
#include "console_value.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int checksFailed = 0;

static void Check(bool passed, char const * what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what);

	if (!passed)
	{
		checksFailed++;
	}
}

// A float or a double with exactly these bits, read through a volatile so the build can't know it
static float FloatOf(uint32_t bits)
{
	volatile uint32_t const atRunTime = bits;
	uint32_t const read = atRunTime;
	float value;
	memcpy(&value, &read, sizeof(value));
	return value;
}

static double DoubleOf(uint64_t bits)
{
	volatile uint64_t const atRunTime = bits;
	uint64_t const read = atRunTime;
	double value;
	memcpy(&value, &read, sizeof(value));
	return value;
}

// The nine kinds of value, and what each of the three tests has to say about it
struct Kind { char const * what; uint32_t floatBits; uint64_t doubleBits; bool nan; bool endless; };

static Kind const kinds[] =
{
	{ "0",                             0x00000000u, 0x0000000000000000ull, false, false },
	{ "-0",                            0x80000000u, 0x8000000000000000ull, false, false },
	{ "the largest real number",       0x7F7FFFFFu, 0x7FEFFFFFFFFFFFFFull, false, false },
	{ "the smallest above 0, a subnormal", 0x00000001u, 0x0000000000000001ull, false, false },
	{ "a quiet NaN",                   0x7FC00000u, 0x7FF8000000000000ull, true,  false },
	{ "a signalling NaN",              0x7F800001u, 0x7FF0000000000001ull, true,  false },
	{ "a NaN with its sign bit set",   0xFFC00000u, 0xFFF8000000000000ull, true,  false },
	{ "endless",                       0x7F800000u, 0x7FF0000000000000ull, false, true },
	{ "endlessly negative",            0xFF800000u, 0xFFF0000000000000ull, false, true },
};

static void TestEveryKind()
{
	puts("0. every kind of value, through all three tests");

	for (size_t i = 0; i < sizeof(kinds) / sizeof(kinds[0]); i++)
	{
		Kind const & kind = kinds[i];
		float const asFloat = FloatOf(kind.floatBits);
		double const asDouble = DoubleOf(kind.doubleBits);
		char what[160];

		snprintf(what, sizeof(what), "%s as a float: NaN %s, NaN or endless %s, a real number %s", kind.what,
			kind.nan ? "yes" : "no", (kind.nan || kind.endless) ? "yes" : "no", (kind.nan || kind.endless) ? "no" : "yes");
		Check(IsNaN(asFloat) == kind.nan && IsNaNOrEndless(asFloat) == (kind.nan || kind.endless) && IsRealNumber(asFloat) == !(kind.nan || kind.endless), what);

		snprintf(what, sizeof(what), "%s as a double: NaN %s, NaN or endless %s, a real number %s", kind.what,
			kind.nan ? "yes" : "no", (kind.nan || kind.endless) ? "yes" : "no", (kind.nan || kind.endless) ? "no" : "yes");
		Check(IsNaN(asDouble) == kind.nan && IsNaNOrEndless(asDouble) == (kind.nan || kind.endless) && IsRealNumber(asDouble) == !(kind.nan || kind.endless), what);
	}
}

static void TestFloats()
{
	puts("1. floats");

	float const zero = FloatOf(0x00000000u);
	float const negativeZero = FloatOf(0x80000000u);
	float const one = FloatOf(0x3F800000u);
	float const largest = FloatOf(0x7F7FFFFFu);
	float const smallest = FloatOf(0x00000001u);   // the smallest above 0, a denormal
	float const quietNaN = FloatOf(0x7FC00000u);
	float const signallingNaN = FloatOf(0x7F800001u);
	float const negativeNaN = FloatOf(0xFFC00000u);
	float const endless = FloatOf(0x7F800000u);
	float const endlesslyNegative = FloatOf(0xFF800000u);

	Check(IsRealNumber(zero) && IsRealNumber(negativeZero), "0 and -0 are real numbers");
	Check(IsRealNumber(one) && IsRealNumber(largest) && IsRealNumber(smallest), "and so are 1, the largest float and the smallest above 0");
	Check(!IsRealNumber(quietNaN) && IsNaN(quietNaN) && IsNaNOrEndless(quietNaN), "a quiet NaN isn't a real number, and is NaN");
	Check(!IsRealNumber(signallingNaN) && IsNaN(signallingNaN), "and neither is a signalling NaN");
	Check(!IsRealNumber(negativeNaN) && IsNaN(negativeNaN), "nor a NaN with its sign bit set");
	Check(!IsRealNumber(endless) && !IsNaN(endless) && IsNaNOrEndless(endless), "endless isn't a real number, and isn't NaN");
	Check(!IsRealNumber(endlesslyNegative) && !IsNaN(endlesslyNegative), "and neither is endlessly negative");
	Check(!IsNaN(one) && !IsNaNOrEndless(largest), "a real number is neither NaN nor endless");

	Check(IsRealNumberFrom(one, -2.0f, 2.0f), "1 is from -2 to 2");
	Check(IsRealNumberFrom(FloatOf(0x40000000u), -2.0f, 2.0f) && IsRealNumberFrom(FloatOf(0xC0000000u), -2.0f, 2.0f), "and so are 2 and -2, the ends");
	Check(!IsRealNumberFrom(FloatOf(0x40000001u), -2.0f, 2.0f), "the next float past 2 isn't");
	Check(!IsRealNumberFrom(FloatOf(0xC0000001u), -2.0f, 2.0f), "and nor is the next past -2");
	Check(!IsRealNumberFrom(quietNaN, -2.0f, 2.0f) && !IsRealNumberFrom(negativeNaN, -2.0f, 2.0f), "NaN is in no range, built the way the game ships");
	Check(!IsRealNumberFrom(endless, -2.0f, 2.0f) && !IsRealNumberFrom(endless, 0.0f, largest), "and nor is endless, even up to the largest float");
}

static void TestDoubles()
{
	puts("2. doubles");

	double const zero = DoubleOf(0x0000000000000000ull);
	double const one = DoubleOf(0x3FF0000000000000ull);
	double const largest = DoubleOf(0x7FEFFFFFFFFFFFFFull);
	double const smallest = DoubleOf(0x0000000000000001ull);
	double const quietNaN = DoubleOf(0x7FF8000000000000ull);
	double const signallingNaN = DoubleOf(0x7FF0000000000001ull);
	double const negativeNaN = DoubleOf(0xFFF8000000000000ull);
	double const endless = DoubleOf(0x7FF0000000000000ull);
	double const endlesslyNegative = DoubleOf(0xFFF0000000000000ull);

	Check(IsRealNumber(zero) && IsRealNumber(one) && IsRealNumber(largest) && IsRealNumber(smallest), "0, 1, the largest double and the smallest above 0 are real numbers");
	Check(!IsRealNumber(quietNaN) && IsNaN(quietNaN), "a quiet NaN isn't, and is NaN");
	Check(!IsRealNumber(signallingNaN) && IsNaN(signallingNaN), "and neither is a signalling NaN");
	Check(!IsRealNumber(negativeNaN) && IsNaN(negativeNaN), "nor a NaN with its sign bit set");
	Check(!IsRealNumber(endless) && !IsNaN(endless) && IsNaNOrEndless(endless), "endless isn't a real number, and isn't NaN");
	Check(!IsRealNumber(endlesslyNegative) && !IsNaN(endlesslyNegative), "and neither is endlessly negative");

	Check(IsRealNumberFrom(one, 0.5, 1.0) && IsRealNumberFrom(DoubleOf(0x3FE0000000000000ull), 0.5, 1.0), "1 and 0.5 are from 0.5 to 1");
	Check(!IsRealNumberFrom(DoubleOf(0x3FF0000000000001ull), 0.5, 1.0), "the next double past 1 isn't");
	Check(!IsRealNumberFrom(quietNaN, 0.5, 1.0) && !IsRealNumberFrom(negativeNaN, 0.5, 1.0), "NaN is in no range, built the way the game ships");
	Check(!IsRealNumberFrom(endless, 0.0, largest), "and nor is endless, even up to the largest double");
}

// 3. The console's decimal (console_value.h), which uses IsNaN, here built the way the game ships. tests/debug_commands_test.cpp tries the
// same with the ordinary rules. (Added 2026-11-14 by Melanikus for the AvP port.)
static void TestTheConsolesDecimal()
{
	puts("3. a decimal typed at the console, built the way the game ships");

	int value = 12345;
	Check(!ConsoleValue_FromDecimal(DoubleOf(0x7FF8000000000000ull), 65536.0, 655, 65536 * 4, &value) && value == 12345, "not a number is refused, and the value stays as it was");
	Check(!ConsoleValue_FromDecimal(DoubleOf(0xFFF8000000000000ull), 65536.0, 655, 65536 * 4, &value) && value == 12345, "and so is one with its sign bit set");
	Check(!ConsoleValue_FromDecimal(DoubleOf(0x7FF0000000000001ull), 1.0, 0, 255, &value) && value == 12345, "and a signalling one");
	Check(ConsoleValue_FromDecimal(DoubleOf(0x7FF0000000000000ull), 65536.0, 655, 65536 * 4, &value) && value == 65536 * 4, "an endless number is the largest value");
	Check(ConsoleValue_FromDecimal(DoubleOf(0xFFF0000000000000ull), 65536.0, 655, 65536 * 4, &value) && value == 655, "and an endlessly small one the smallest");
	Check(ConsoleValue_FromDecimal(DoubleOf(0x3FE0000000000000ull), 65536.0, 655, 65536 * 4, &value) && value == 32768, "0.5 is half speed");
}

int main()
{
	setvbuf(stdout, NULL, _IONBF, 0);

	TestEveryKind();
	TestFloats();
	TestDoubles();
	TestTheConsolesDecimal();

	if (checksFailed)
	{
		printf("real_numbers_test FAILED: %d check(s)\n", checksFailed);
		return 1;
	}

	puts("real_numbers_test passed");
	return 0;
}
