// The test of making a direction one unit long, and of what's given back for no direction (NORMALISE-ZERO-1). Written 2026-11-08 by
// Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// - normalise.h, the sums of Normalise, Normalise2d, FNormalise and FNormalise2d: a vector with a direction comes back that way and one
//   unit long, with the answers the game has always given; a vector of nothing stays nothing, where it used to come back as -2147483648
//   or "not a number" in every part; a float vector holding "not a number" or infinity comes back as nothing.
// - The game's own files, read as text: the four functions use those sums, and the two that make a matrix from a direction give back the
//   matrix that turns nothing for a vector of nothing, and MatrixFromZVector makes a real one for a vector straight up or down.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this with tests/normalise_test.args, and the program gives back a non-zero result if any check failed.
// tests/takeouts/normalise_test.json proves each check can fail.

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>

#include <fstream>
#include <sstream>
#include <string>

#include "normalise.h"

static int checksFailed = 0;

static void Check(bool good, const std::string &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// A file of the game's source, by its place from the top of the repository, with Windows' line ends taken out
static std::string ReadSourceFile(const char *name)
{
	// /FC in this test's settings makes __FILE__ the whole path of this file, so everything else can be found from it
	std::string path = __FILE__;
	std::string::size_type const tests = path.rfind("tests");

	if (tests == std::string::npos)
	{
		return "";
	}

	std::ifstream file((path.substr(0, tests) + name).c_str(), std::ios::binary);
	std::stringstream all;
	all << file.rdbuf();

	std::string text = all.str(), plain;

	for (char const c : text)
	{
		if (c != '\r')
		{
			plain += c;
		}
	}

	return plain;
}

static bool Has(const std::string &text, const char *what)
{
	return text.find(what) != std::string::npos;
}

static std::string Between(const std::string &text, const char *from, const char *to)
{
	std::string::size_type const start = text.find(from);
	std::string::size_type const end = start == std::string::npos ? start : text.find(to, start);

	return (start == std::string::npos || end == std::string::npos) ? std::string() : text.substr(start, end - start);
}

static bool Whole3Gives(int x, int y, int z, int wantX, int wantY, int wantZ)
{
	Normalise_Whole3(&x, &y, &z);
	return x == wantX && y == wantY && z == wantZ;
}

static bool Whole2Gives(int x, int y, int wantX, int wantY)
{
	Normalise_Whole2(&x, &y);
	return x == wantX && y == wantY;
}

static void TestWholeNumbers()
{
	puts("Normalise and Normalise2d, in whole numbers:");

	Check(Whole3Gives(0, 0, 0, 0, 0, 0), "a vector of nothing stays nothing, where it came back as -2147483648 in every part");
	Check(Whole3Gives(1, 0, 0, 65536, 0, 0), "one step along is a whole one along");
	Check(Whole3Gives(0, -7, 0, 0, -65536, 0), "seven steps down is a whole one down");
	Check(Whole3Gives(3, 4, 0, 39321, 52428, 0), "three and four across is 0.6 and 0.8 of a whole one, cut to whole numbers, as it always was");
	Check(Whole3Gives(1, 1, 1, 37837, 37837, 37837), "and the same each way is 0.577 of a whole one each way");
	Check(Whole3Gives(INT_MIN, 0, 0, -65536, 0, 0), "the largest vector there can be is still one unit long");

	Check(Whole2Gives(0, 0, 0, 0), "flat: a vector of nothing stays nothing");
	Check(Whole2Gives(3, 4, 39321, 52428), "and three and four is 0.6 and 0.8");
	Check(Whole2Gives(-1, 0, -65536, 0), "and one step back is a whole one back");
}

static bool Float3Gives(float x, float y, float z, float wantX, float wantY, float wantZ)
{
	Normalise_Float3(&x, &y, &z);
	return fabsf(x - wantX) < 1e-6f && fabsf(y - wantY) < 1e-6f && fabsf(z - wantZ) < 1e-6f;
}

static bool Float2Gives(float x, float y, float wantX, float wantY)
{
	Normalise_Float2(&x, &y);
	return fabsf(x - wantX) < 1e-6f && fabsf(y - wantY) < 1e-6f;
}

static void TestFloats()
{
	puts("FNormalise and FNormalise2d, in floats:");

	// Made while the test runs, from numbers the compiler can't fold, so it can't warn about them or work them out its own way
	volatile float minusOne = -1.0f;
	volatile float largest = FLT_MAX;
	float const notANumber = sqrtf(minusOne);
	float const infinity = largest * 2.0f;

	Check(Float3Gives(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f), "a vector of nothing stays nothing, where it came back as \"not a number\"");
	Check(Float3Gives(3.0f, 4.0f, 0.0f, 0.6f, 0.8f, 0.0f), "three and four across is 0.6 and 0.8");
	Check(Float3Gives(0.0f, 0.0f, -2.5f, 0.0f, 0.0f, -1.0f), "and two and a half back is one back");
	Check(Float3Gives(notANumber, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f), "a vector holding \"not a number\" has no direction, so it's nothing, and it can't spread");
	Check(Float3Gives(infinity, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f), "and so is one holding infinity");
	Check(Float3Gives(1e-30f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f), "and one too short for its length to be worked out in floats");

	Check(Float2Gives(0.0f, 0.0f, 0.0f, 0.0f), "flat: a vector of nothing stays nothing");
	Check(Float2Gives(0.0f, 4.0f, 0.0f, 1.0f), "and four up is one up");
	Check(Float2Gives(notANumber, 0.0f, 0.0f, 0.0f), "and one holding \"not a number\" is nothing");

	Check(!Normalise_LengthIsReal(0.0f) && !Normalise_LengthIsReal(-1.0f), "a length of nothing, or below it, can't be divided by");
	Check(!Normalise_LengthIsReal(notANumber) && !Normalise_LengthIsReal(infinity), "and nor can \"not a number\" or infinity");
	Check(Normalise_LengthIsReal(1.0f) && Normalise_LengthIsReal(FLT_MAX), "but any real length above nothing can");
}

static void TestTheGamesFiles()
{
	puts("The game's own files:");

	std::string const functions = ReadSourceFile("3dc/win95/plspecfn.cpp");
	std::string const weapons = ReadSourceFile("3dc/avp/weapons.cpp");
	std::string const maths = ReadSourceFile("3dc/maths.cpp");
	std::string const fromDirection = Between(weapons, "void MakeMatrixFromDirection(VECTORCH *directionPtr, MATRIXCH *matrixPtr)\n{", "CrossProduct(directionPtr, &XVector, &YVector);");
	std::string const fromZ = Between(maths, "void MatrixFromZVector(VECTORCH *v, MATRIXCH *m)\n{", "MakeNormal(&zero, &XVector, v, &YVector);");

	Check(!functions.empty() && !fromDirection.empty() && !fromZ.empty(), "the game's source can be read");

	Check(Has(functions, "void Normalise(VECTORCH *nvector)\n{\n\tNormalise_Whole3(&nvector->vx, &nvector->vy, &nvector->vz);\n}"), "Normalise uses the sums tested above");
	Check(Has(functions, "void Normalise2d(VECTOR2D *nvector)\n{\n\tNormalise_Whole2(&nvector->vx, &nvector->vy);\n}"), "and so does Normalise2d");
	Check(Has(functions, "void FNormalise(VECTORCHF *n)\n{\n\tNormalise_Float3(&n->vx, &n->vy, &n->vz);\n}"), "and FNormalise");
	Check(Has(functions, "void FNormalise2d(VECTOR2DF *n)\n{\n\tNormalise_Float2(&n->vx, &n->vy);\n}"), "and FNormalise2d");

	Check(Has(fromDirection, "if (directionPtr->vx == 0 && directionPtr->vy == 0 && directionPtr->vz == 0)\n\t{\n\t\textern MATRIXCH IdentityMatrix;\n\t\t*matrixPtr = IdentityMatrix;\n\t\treturn;\n\t}"),
		"MakeMatrixFromDirection gives back the matrix that turns nothing for a direction of nothing");
	Check(Has(fromZ, "if (v->vx == 0 && v->vy == 0 && v->vz == 0)\n\t{\n\t\t*m = IdentityMatrix;\n\t\treturn;\n\t}"), "and so does MatrixFromZVector");
	Check(Has(fromZ, "if (v->vx == 0 && v->vz == 0)\n\t{") && Has(fromZ, "\t\tXVector.vx = ONE_FIXED;\n\t\tXVector.vy = 0;\n\t\tXVector.vz = 0;\n"),
		"and a vector straight up or down gets the world's own across, not a side of nothing made into -2147483648s");
}

int main()
{
	TestWholeNumbers();
	TestFloats();
	TestTheGamesFiles();

	puts(checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
