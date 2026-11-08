// A test of the revolver's cylinder and where a scope's picture goes: the sums in 3dc/avp/revolver.h and 3dc/avp/weapon_scope.h. Written 2026-11-06 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// Which chamber a round goes into, which one it's fired from, what falls out on a reload, how many rounds go in during one loading movement, that a saved game can't put a
// number in the cylinder that isn't one, and that a scope's cross lands on the middle of the screen. It doesn't need the game, a model or a graphics card, because the sums
// have no part of the game in them. What the gun does with them in the game can only be seen by playing it.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this file with the settings in tests/revolver_test.args. The program prints a line for each check and gives back a non-zero result if any check failed.

#include <stdio.h>

#include "revolver.h"
#include "weapon_scope.h"

static int checksFailed = 0;

static void Check(bool passed, const char *what)
{
	printf("%s: %s\n", passed ? "ok" : "FAILED", what);

	if (!passed)
	{
		checksFailed++;
	}
}

static RevolverCylinder EmptyCylinderAt(int place)
{
	RevolverCylinder cylinder;
	cylinder.place = place;
	Revolver_EmptyChambers(&cylinder);
	return cylinder;
}

static int CountLive(RevolverCylinder const &cylinder)
{
	int live = 0;

	for (int i = 0; i < kRevolverChambers; i++)
	{
		live += cylinder.live[i];
	}

	return live;
}

static int CountSomething(RevolverCylinder const &cylinder)
{
	int something = 0;

	for (int i = 0; i < kRevolverChambers; i++)
	{
		something += cylinder.somethingIn[i];
	}

	return something;
}

static void CheckTheRing()
{
	Check(Revolver_PlaceForward(0, 1) == 1, "one place forward from the first chamber is the second");
	Check(Revolver_PlaceForward(4, 1) == 0, "one place forward from the last chamber is the first");
	Check(Revolver_PlaceForward(3, 4) == 2, "several places forward go round the ring");
	Check(Revolver_PlaceBack(0, 1) == 4, "one place back from the first chamber is the last");
	Check(Revolver_PlaceBack(2, 1) == 1, "one place back is the chamber before");
	Check(Revolver_PlaceBack(2, 7) == 0, "more places back than there are chambers still lands in the ring");
	Check(Revolver_PlaceForward(1, 5) == 1, "a whole turn comes back to the same chamber");
}

static void CheckLoadingAndFiring()
{
	RevolverCylinder cylinder = EmptyCylinderAt(2);

	Check(CountSomething(cylinder) == 0 && CountLive(cylinder) == 0, "an empty cylinder holds nothing");

	Revolver_MarkLoaded(&cylinder);
	Check(cylinder.somethingIn[2] == 1 && cylinder.live[2] == 1, "a round goes into the chamber at the place");
	Check(cylinder.place == 3, "and the cylinder turns on to the next");
	Check(Revolver_ChamberToFire(&cylinder) == 2, "so the next shot comes from the round just loaded");

	Revolver_MarkLoaded(&cylinder);
	Revolver_MarkLoaded(&cylinder);
	Check(CountLive(cylinder) == 3, "three rounds loaded are three live rounds");
	Check(cylinder.place == 0, "and the cylinder has come round past the last chamber");

	Revolver_MarkShot(&cylinder);
	Check(cylinder.place == 4, "a shot turns the cylinder back one place");
	Check(cylinder.live[4] == 0, "that chamber's round isn't live any more");
	Check(cylinder.somethingIn[4] == 1, "but its spent case stays in the chamber");
	Check(CountLive(cylinder) == 2, "and the other rounds are still live");
	Check(Revolver_HasSpentCase(&cylinder), "so there's a spent case to throw out");

	Revolver_MarkShot(&cylinder);
	Revolver_MarkShot(&cylinder);
	Check(cylinder.place == 2, "firing all three brings the cylinder back to where it started");
	Check(CountLive(cylinder) == 0 && CountSomething(cylinder) == 3, "three spent cases are left in the gun");
}

static void CheckFillingFromTheRoundCount()
{
	RevolverCylinder cylinder = EmptyCylinderAt(3);

	Revolver_FillFromRounds(&cylinder, 2);
	Check(cylinder.live[2] == 1 && cylinder.live[1] == 1, "rounds sit in the chambers just behind the place");
	Check(cylinder.somethingIn[2] == 1 && cylinder.somethingIn[1] == 1, "and those chambers hold something");
	Check(CountLive(cylinder) == 2 && CountSomething(cylinder) == 2, "and no others");
	Check(cylinder.place == 3, "and the cylinder doesn't turn");
	Check(cylinder.live[Revolver_ChamberToFire(&cylinder)] == 1, "the next shot comes from a live round");

	Revolver_FillFromRounds(&cylinder, 5);
	Check(CountLive(cylinder) == 5, "a full gun has five live rounds");

	Revolver_FillFromRounds(&cylinder, 0);
	Check(CountSomething(cylinder) == 0, "no rounds means nothing in any chamber");

	// The biggest number there is, because going round the ring for each one would never end.
	Revolver_FillFromRounds(&cylinder, 2147483647);
	Check(CountLive(cylinder) == 5, "a count far too big from a saved game fills the five chambers and stops");

	Revolver_FillFromRounds(&cylinder, -7);
	Check(CountSomething(cylinder) == 0, "a negative count from a saved game means no rounds");

	// The sums have to agree: rounds put in by count are fired from the same chambers as rounds that were loaded one at a time.
	RevolverCylinder loaded = EmptyCylinderAt(1);
	Revolver_MarkLoaded(&loaded);
	Revolver_MarkLoaded(&loaded);

	RevolverCylinder counted = EmptyCylinderAt(loaded.place);
	Revolver_FillFromRounds(&counted, 2);

	bool same = counted.place == loaded.place;

	for (int i = 0; i < kRevolverChambers; i++)
	{
		same = same && counted.live[i] == loaded.live[i] && counted.somethingIn[i] == loaded.somethingIn[i];
	}

	Check(same, "filling by count gives the same chambers as loading one at a time");
}

static void CheckWhatComesOut()
{
	RevolverCylinder cylinder = EmptyCylinderAt(0);

	Check(!Revolver_HasSpentCase(&cylinder), "an empty gun has no spent case");
	Check(Revolver_WhatComesOut(&cylinder, 0) == kRevolverNothingComesOut, "and nothing comes out of an empty chamber");

	Revolver_MarkLoaded(&cylinder);
	Revolver_MarkLoaded(&cylinder);
	Check(!Revolver_HasSpentCase(&cylinder), "live rounds aren't spent cases");
	Check(Revolver_WhatComesOut(&cylinder, 1) == kRevolverLiveRoundComesOut, "a live round comes out as a live round");

	Revolver_MarkShot(&cylinder);
	Check(Revolver_HasSpentCase(&cylinder), "one shot leaves a spent case");
	Check(Revolver_WhatComesOut(&cylinder, 1) == kRevolverSpentCaseComesOut, "which comes out as a spent case");
	Check(Revolver_WhatComesOut(&cylinder, 0) == kRevolverLiveRoundComesOut, "and the live round beside it comes out as a live round");
	Check(Revolver_WhatComesOut(&cylinder, 4) == kRevolverNothingComesOut, "and an empty chamber still gives nothing");

	Check(Revolver_SpareAfterTippingOut(10, 3) == 13, "live rounds that fall out go back with the spares");
	Check(Revolver_SpareAfterTippingOut(10, 0) == 10, "no live rounds change nothing");
	Check(Revolver_SpareAfterTippingOut(254, 5) == 255, "the spares stop at what a byte can hold, instead of going round to a small number");
	Check(Revolver_SpareAfterTippingOut(10, -3) == 10, "a negative count can't take spares away");
}

static void CheckHowManyGoIn()
{
	int const most = 5 * kRevolverOne;

	Check(Revolver_RoundsToLoad(0, 0, most, 20) == 1, "one round goes in at the end of a loading movement");
	Check(Revolver_RoundsToLoad(-100, 0, most, 20) == 1, "and still one if the clock ran a little past the end");
	Check(Revolver_RoundsToLoad(-kRevolverOne, 0, most, 20) == 2, "a frame a whole movement too long loads a second round");
	Check(Revolver_RoundsToLoad(-3 * kRevolverOne, 0, most, 20) == 4, "three whole movements too long loads four");
	Check(Revolver_RoundsToLoad(-100 * kRevolverOne, 0, most, 20) == 5, "however long the frame, no more than the gun can hold");
	Check(Revolver_RoundsToLoad(-100 * kRevolverOne, 3 * kRevolverOne, most, 20) == 2, "and no more than there's room for");
	Check(Revolver_RoundsToLoad(0, most, most, 20) == 0, "a full gun takes none");
	Check(Revolver_RoundsToLoad(-3 * kRevolverOne, 0, most, 2) == 2, "and no more than there are spares");
	Check(Revolver_RoundsToLoad(0, 0, most, 0) == 0, "no spares means none");
	Check(Revolver_RoundsToLoad(0, 6 * kRevolverOne, most, 20) == 0, "a gun holding more than it should takes none");
	Check(Revolver_RoundsToLoad(0, 0, most, -1) == 0, "a spare count below nothing takes none, and doesn't give back a negative number");
	Check(Revolver_RoundsToLoad(2 * kRevolverOne, 0, most, 20) == 1, "a clock that somehow hasn't run out still counts as one movement");

	Check(Revolver_KeepsLoading(0, most, 20, false), "with room and spares the gun keeps loading");
	Check(!Revolver_KeepsLoading(most, most, 20, false), "a full gun stops");
	Check(!Revolver_KeepsLoading(0, most, 0, false), "no spares stops it");
	Check(!Revolver_KeepsLoading(0, most, 20, true), "and so does the player asking for something else");
}

static void CheckTheHand()
{
	Check(Revolver_AnimationTimeFromClock(kRevolverOne) == 0, "a clock at the start is the start of the animation");
	Check(Revolver_AnimationTimeFromClock(0) == kRevolverLastMoment, "a clock run out is its last moment");
	Check(Revolver_AnimationTimeFromClock(kRevolverOne / 2) == kRevolverLastMoment / 2, "half way down is half way along");
	Check(Revolver_AnimationTimeFromClock((kRevolverOne * 3) / 4) == kRevolverLastMoment / 4, "and a quarter of the way down is a quarter of the way along, so it runs forwards");
	Check(Revolver_AnimationTimeFromClock(-500) == kRevolverLastMoment, "a clock that ran past the end stays at the last moment");
	Check(Revolver_AnimationTimeFromClock(kRevolverOne + 500) == 0, "and one above the start stays at the start");

	Check(Revolver_RoundsToShow(0, 0, 100, 48055) == 0, "before the moment it goes in, the round isn't in the cylinder");
	Check(Revolver_RoundsToShow(0, 0, 48055, 48055) == 1, "at that moment it is");
	Check(Revolver_RoundsToShow(0, 1, 60000, 48055) == 0, "and it only appears once");
	Check(Revolver_RoundsToShow(1, 1, 100, 48055) == 0, "the next movement starts with nothing new to show");
	Check(Revolver_RoundsToShow(1, 1, 50000, 48055) == 1, "and shows the next round when its moment comes");
	Check(Revolver_RoundsToShow(3, 1, 100, 48055) == 2, "rounds a long frame loaded without being shown all appear");
	Check(Revolver_RoundsToShow(0, 2, 100, 48055) == 0, "and more shown than loaded asks for none, never a negative number");
}

static void CheckTheCylinderOnTheModel()
{
	Check(Revolver_CylinderAnimationTime(0) == 0, "the first chamber is the start of the turn");
	Check(Revolver_CylinderAnimationTime(1) == kRevolverLastMoment / 5, "the second is a fifth of the way round");
	Check(Revolver_CylinderAnimationTime(4) == (4 * kRevolverLastMoment) / 5, "the last is four fifths of the way round");
	Check(Revolver_CylinderAnimationTime(7) == Revolver_CylinderAnimationTime(2), "a place outside the ring is brought back into it");
}

static void CheckTheZoom()
{
	Check(Revolver_ZoomTowards(1.0f, 0.5f, 3.0f, 0.1f) < 0.71f && Revolver_ZoomTowards(1.0f, 0.5f, 3.0f, 0.1f) > 0.69f, "the zoom walks down at its rate");
	Check(Revolver_ZoomTowards(0.5f, 1.0f, 3.0f, 0.1f) > 0.79f && Revolver_ZoomTowards(0.5f, 1.0f, 3.0f, 0.1f) < 0.81f, "and back up at its rate");
	Check(Revolver_ZoomTowards(0.55f, 0.5f, 3.0f, 0.1f) == 0.5f, "it stops on its target going down");
	Check(Revolver_ZoomTowards(0.95f, 1.0f, 3.0f, 0.1f) == 1.0f, "and going up");
	Check(Revolver_ZoomTowards(0.5f, 0.5f, 3.0f, 0.1f) == 0.5f, "and stays there");
}

static void CheckSavedGames()
{
	RevolverCylinder good = EmptyCylinderAt(4);
	Revolver_MarkLoaded(&good);
	Revolver_MarkLoaded(&good);
	Revolver_MarkLoaded(&good);
	Revolver_MarkShot(&good);
	Check(Revolver_SavedCylinderIsGood(&good), "a cylinder the game itself made is good");

	RevolverCylinder saved = good;
	saved.place = 5;
	Check(!Revolver_SavedCylinderIsGood(&saved), "the place can't be past the last chamber");

	saved = good;
	saved.place = -1;
	Check(!Revolver_SavedCylinderIsGood(&saved), "or before the first");

	saved = good;
	saved.somethingIn[2] = 2;
	Check(!Revolver_SavedCylinderIsGood(&saved), "a chamber's something-in flag can only be 0 or 1");

	saved = good;
	saved.somethingIn[4] = -1;
	Check(!Revolver_SavedCylinderIsGood(&saved), "and that goes for every chamber, and for negative numbers");

	saved = good;
	saved.live[3] = 7;
	saved.somethingIn[3] = 1;
	Check(!Revolver_SavedCylinderIsGood(&saved), "a chamber's live flag can only be 0 or 1");

	saved = good;
	saved.somethingIn[2] = 0;
	saved.live[2] = 1;
	Check(!Revolver_SavedCylinderIsGood(&saved), "a chamber can't hold a live round with nothing in it");

	saved = EmptyCylinderAt(0);
	saved.somethingIn[4] = 2;
	Check(!Revolver_SavedCylinderIsGood(&saved), "the last chamber is looked at, as well as the first");

	saved = EmptyCylinderAt(0);
	saved.live[4] = 2;
	saved.somethingIn[4] = 1;
	Check(!Revolver_SavedCylinderIsGood(&saved), "for its live flag too");

	// What happens to the numbers that are good: a place and flags the check lets through can't take the gun outside its chambers
	Check(Revolver_SavedCylinderIsGood(&good) && Revolver_ChamberToFire(&good) >= 0 && Revolver_ChamberToFire(&good) < kRevolverChambers, "and a good one always fires from a real chamber");
}

static void CheckCountingLiveRounds()
{
	RevolverCylinder cylinder = EmptyCylinderAt(0);

	Check(Revolver_LiveRounds(&cylinder) == 0, "an empty gun has no live rounds");

	Revolver_MarkLoaded(&cylinder);
	Revolver_MarkLoaded(&cylinder);
	Check(Revolver_LiveRounds(&cylinder) == 2, "each round loaded is one more");

	Revolver_MarkShot(&cylinder);
	Check(Revolver_LiveRounds(&cylinder) == 1, "and each shot is one fewer, though its case is still there");

	Revolver_EmptyChambers(&cylinder);
	cylinder.live[4] = 1;
	Check(Revolver_LiveRounds(&cylinder) == 1, "every chamber is counted, up to the last");
}

// Where the cross lands on the screen, in the same units. The picture's texels are the view's width over 128, and the cross is 64.5 and 47.5 of them along.
static double CrossX(WeaponScopeLayout const &layout)
{
	return layout.pictureX + (layout.viewWidth * 64.5) / 128.0;
}

static double CrossY(WeaponScopeLayout const &layout)
{
	return layout.pictureY + (layout.viewHeight * 47.5) / 96.0;
}

static bool CrossIsInTheMiddle(WeaponScopeLayout const &layout, int width, int height)
{
	double const x = CrossX(layout) - width / 2.0;
	double const y = CrossY(layout) - height / 2.0;

	return x > -1.0 && x < 1.0 && y > -1.0 && y < 1.0;
}

static void CheckTheScopesPicture()
{
	WeaponScopeLayout layout = WeaponScope_LayoutFor(1280, 720);

	Check(layout.viewHeight == 720 && layout.viewWidth == 960, "at 1280 by 720, the view is the height of the screen, and keeps its shape, so that the ring stays round");
	Check(layout.pictureWidth == 960, "the picture is as wide as the view");
	Check(layout.pictureHeight == 960, "and a third taller than it, because only its top three quarters are the view");
	Check(CrossIsInTheMiddle(layout, 1280, 720), "the cross lands on the middle of the screen, within a pixel");

	layout = WeaponScope_LayoutFor(640, 480);

	Check(layout.viewWidth == 640 && layout.viewHeight == 480, "on a 4:3 screen the view fills the screen");
	Check(layout.pictureX < 0 && layout.pictureX > -4, "and the picture starts just off its left edge, because the cross is just right of the picture's middle");
	Check(layout.pictureY > 0 && layout.pictureY < 4, "and just below its top, because the cross is just above");
	Check(CrossIsInTheMiddle(layout, 640, 480), "and the cross is in the middle");

	layout = WeaponScope_LayoutFor(1920, 1080);

	Check(layout.viewHeight == 1080 && layout.viewWidth == 1440, "the view keeps its shape on any wide screen");
	Check(CrossIsInTheMiddle(layout, 1920, 1080), "with the cross in the middle");

	layout = WeaponScope_LayoutFor(600, 1000);

	Check(layout.viewWidth == 600 && layout.viewHeight == 450, "on a tall screen the view fits the width");
	Check(layout.pictureY > 200, "with room above it");
	Check(CrossIsInTheMiddle(layout, 600, 1000), "and the cross is in the middle");
}

int main()
{
	CheckTheRing();
	CheckLoadingAndFiring();
	CheckFillingFromTheRoundCount();
	CheckWhatComesOut();
	CheckHowManyGoIn();
	CheckTheHand();
	CheckTheCylinderOnTheModel();
	CheckTheZoom();
	CheckSavedGames();
	CheckCountingLiveRounds();
	CheckTheScopesPicture();

	if (checksFailed)
	{
		printf("%d checks failed\n", checksFailed);
		return 1;
	}

	puts("All checks passed");
	return 0;
}
