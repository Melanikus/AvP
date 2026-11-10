// What join-films believes about its command line and about a film, which both come from outside it. They're apart from
// join-films.cpp so that tests/hudson_plan_test.cpp can try each one. Like the rest of tools/hudson, this never goes in the game.
// Written 2026-11-09 by Melanikus for the AvP port, moved from 3dc/avp/hudson_plan.cpp, where they were until the release's check
// (tools/check-new-functions.ps1) found that nothing in the game calls them.
//
// join-films is built from join-films.cpp and join_films_reads.cpp, with 3dc/avp as a folder to search, for hudson_plan.h.

#pragma once

// Reads back the number HudsonPlan_StampNumber wrote, from a film's picture, which its squeezing has blurred a little. False if what's there isn't ten of its figures.
bool JoinFilms_ReadStampedNumber(const unsigned char *pixels, int pitch, int width, int height, unsigned long *number);

// The most milliseconds JoinFilms_ReadSeconds gives: an hour, longer than any film a run makes
const long kJoinFilmsMostMilliseconds = 3600000;

// Reads a number of seconds, such as 7.4 or 85.967, as milliseconds. Zero if it's good, or else why it isn't, which is one of these.
const char *JoinFilms_ReadSeconds(const char *text, long *milliseconds);
extern const char *const kJoinFilmsSecondsMissing;
extern const char *const kJoinFilmsSecondsHaveAComma;
extern const char *const kJoinFilmsSecondsTooManyFigures;
extern const char *const kJoinFilmsSecondsNotANumber;
extern const char *const kJoinFilmsSecondsTooMany;

// Whether every row of a picture lies inside the buffer holding it. A film says where its rows are, and a damaged one could say they go past the end, so join-films reads nothing until this says yes.
// pitch is the step from one row to the next, negative when the rows are stored bottom up. Places are addresses, and everything is counted in bytes.
bool JoinFilms_RowsFit(long long firstRow, long long pitch, int rows, long long rowBytes, long long bufferStart, long long bufferLength);

// Whether the part of a picture a film says is shown lies inside the whole picture
bool JoinFilms_ApertureFits(int left, int top, int width, int height, int frameWidth, int frameHeight);
