// What join-films believes about its command line and about a film: see join_films_reads.h. Like the rest of tools/hudson, this
// never goes in the game.
// Written 2026-11-09 by Melanikus for the AvP port, moved unchanged from 3dc/avp/hudson_plan.cpp (written there 2026-11-07), with
// the names changed to say where they now live.

#include <string.h>

#include "hudson_plan.h"
#include "join_films_reads.h"

// How join-films reads a number of seconds from its command line, which comes from outside, so nothing in it is believed. A point, not a comma, comes before the thousandths, whatever the machine's language.
const char *const kJoinFilmsSecondsMissing = "no seconds were given";
const char *const kJoinFilmsSecondsHaveAComma = "seconds take a point, not a comma: 7.4, not 7,4";
const char *const kJoinFilmsSecondsTooManyFigures = "seconds have at most four figures before the point and three after it";
const char *const kJoinFilmsSecondsNotANumber = "seconds are only figures, with at most one point and a figure on each side of it";
const char *const kJoinFilmsSecondsTooMany = "more than an hour, which is longer than any film a run makes";

const char *JoinFilms_ReadSeconds(const char *text, long *milliseconds)
{
	static const int kMostFiguresBeforeThePoint = 4; // an hour is 3600 seconds
	static const int kMostFiguresAfterThePoint = 3;  // thousandths of a second: a millisecond
	static const long kMostMilliseconds = 3600000;   // an hour

	if (!text || !milliseconds || !text[0])
	{
		return kJoinFilmsSecondsMissing;
	}

	if (strchr(text, ','))
	{
		return kJoinFilmsSecondsHaveAComma;
	}

	const char *at = text;
	long whole = 0;
	int figuresBefore = 0;

	// Only 0 to 9 count as figures, whatever the machine's language says. Counting them first means the number can never grow too big to hold.
	while (*at >= '0' && *at <= '9')
	{
		if (++figuresBefore > kMostFiguresBeforeThePoint)
		{
			return kJoinFilmsSecondsTooManyFigures;
		}

		whole = whole * 10 + (*at - '0');
		at++;
	}

	if (figuresBefore == 0)
	{
		return kJoinFilmsSecondsNotANumber;
	}

	long thousandths = 0;

	if (*at == '.')
	{
		long worth = 100; // the first figure after the point is tenths of a second, which is a hundred milliseconds each
		int figuresAfter = 0;

		at++;

		while (*at >= '0' && *at <= '9')
		{
			if (++figuresAfter > kMostFiguresAfterThePoint)
			{
				return kJoinFilmsSecondsTooManyFigures;
			}

			thousandths += (*at - '0') * worth;
			worth /= 10;
			at++;
		}

		if (figuresAfter == 0)
		{
			return kJoinFilmsSecondsNotANumber;
		}
	}

	// Anything else left, a sign, a space, a letter or a second point, means it isn't a number of seconds
	if (*at != 0)
	{
		return kJoinFilmsSecondsNotANumber;
	}

	long const total = whole * 1000 + thousandths;

	if (total > kMostMilliseconds)
	{
		return kJoinFilmsSecondsTooMany;
	}

	*milliseconds = total;
	return 0;
}

bool JoinFilms_RowsFit(long long firstRow, long long pitch, int rows, long long rowBytes, long long bufferStart, long long bufferLength)
{
	if (rows <= 0 || rowBytes <= 0 || bufferLength <= 0)
	{
		return false;
	}

	long long const step = pitch < 0 ? -pitch : pitch;

	// A row longer than the step from one row to the next would run into the next one
	if (step < rowBytes)
	{
		return false;
	}

	// Rows stored bottom up go down in memory from the first, so the lowest place is the last row's
	long long const lastRow = firstRow + (long long)(rows - 1) * pitch;
	long long const lowest = pitch < 0 ? lastRow : firstRow;
	long long const highest = (pitch < 0 ? firstRow : lastRow) + rowBytes;

	return lowest >= bufferStart && highest <= bufferStart + bufferLength;
}

bool JoinFilms_ApertureFits(int left, int top, int width, int height, int frameWidth, int frameHeight)
{
	return left >= 0 && top >= 0 && width > 0 && height > 0 && width <= frameWidth - left && height <= frameHeight - top;
}

bool JoinFilms_ReadStampedNumber(const unsigned char *pixels, int pitch, int width, int height, unsigned long *number)
{
	int const across = kHudsonStampSquare * 4 * kHudsonStampFiguresInANumber;
	int const down = kHudsonStampSquare * 5;

	if (!pixels || !number || width < kHudsonStampFromTheEdge + across || height < kHudsonStampFromTheEdge + down || pitch < width * 4)
	{
		return false;
	}

	unsigned long found = 0;

	for (int place = 0; place < kHudsonStampFiguresInANumber; place++)
	{
		unsigned char rows[5];

		for (int row = 0; row < 5; row++)
		{
			rows[row] = 0;

			for (int square = 0; square < 3; square++)
			{
				// The middle of each square, which a film's squeezing blurs least, and lit if it's more bright than dark
				int const x = kHudsonStampFromTheEdge + place * kHudsonStampSquare * 4 + square * kHudsonStampSquare + kHudsonStampSquare / 2;
				int const y = kHudsonStampFromTheEdge + row * kHudsonStampSquare + kHudsonStampSquare / 2;
				unsigned char const *const pixel = pixels + (size_t)y * pitch + (size_t)x * 4;
				int const brightness = (pixel[0] + pixel[1] + pixel[2]) / 3;

				rows[row] = (unsigned char)((rows[row] << 1) | (brightness > 127 ? 1 : 0));
			}
		}

		int figure = -1;

		for (int f = 0; f < 10 && figure < 0; f++)
		{
			figure = memcmp(rows, kHudsonStampFigures[f], 5) == 0 ? f : -1;
		}

		if (figure < 0)
		{
			return false;
		}

		found = found * 10 + (unsigned long)figure;
	}

	*number = found;
	return true;
}
