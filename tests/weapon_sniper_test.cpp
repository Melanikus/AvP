// The sniper rifle's thinking, without the game. Written 2026-11-06 by Melanikus for the AvP port.
//
// weapon_sniper_sums.h holds the zoom's sums and what the rifle does next, and knows nothing about the rest of the game. So
// this test can walk the rifle through a player using it, one step at a time, and check each. What a real game does with
// those answers, such as the model's movements, the scope's picture and the view zooming, needs a run of the game.

#include <math.h>
#include <stdio.h>

#include <string>

#include "weapon_sniper_sums.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static bool Near(float a, float b, float within = 0.001f)
{
	return fabsf(a - b) <= within;
}

static int const kOneSecond = 65536;

static void TestTheZoom()
{
	puts("the zoom");

	Check(Near(SniperZoom_In(1.0f, kOneSecond / 2), 6.0f), "half a second zooming in goes from 1 to 6, ten a second");
	Check(SniperZoom_In(19.9f, kOneSecond) == 20.0f, "and it stops at exactly 20");
	Check(Near(SniperZoom_Out(20.0f, kOneSecond / 7), 10.0f), "zooming out goes at 70 a second");
	Check(SniperZoom_Out(1.5f, kOneSecond) == 1.0f, "and stops at exactly 1, so the step can see it's there");
	Check(Near(SniperZoom_Back(1.0f, 15.0f, kOneSecond / 10), 8.0f), "going back goes at 70 a second too");
	Check(SniperZoom_Back(14.9f, 15.0f, kOneSecond) == 15.0f, "and stops at exactly the zoom it had");
	Check(SniperZoom_Back(1.0f, 1.0f, kOneSecond) == 1.0f, "going back to no zoom stays at no zoom");

	Check(SniperZoom_ViewScale(1.0f) == 1.0f, "no zoom leaves the view as it is");
	Check(Near(SniperZoom_ViewScale(20.0f), 0.05f), "20 times makes the view a twentieth");

	SniperRifleState rifle;
	SniperRifle_Start(&rifle);
	rifle.zoom = 5.0f;
	rifle.zoomToGoBackTo = 9.0f;
	Sniper_MoveZoom(&rifle, kSniperIdle, kOneSecond);
	Sniper_MoveZoom(&rifle, kSniperWorkingBolt, kOneSecond);
	Sniper_MoveZoom(&rifle, kSniperWaitingForTrigger, kOneSecond);
	Check(rifle.zoom == 5.0f, "idle, the bolt and waiting for the trigger leave the zoom alone");
	Sniper_MoveZoom(&rifle, kSniperZoomingIn, kOneSecond / 10);
	Check(Near(rifle.zoom, 6.0f), "zooming in moves it");
	Sniper_MoveZoom(&rifle, kSniperZoomingBack, kOneSecond);
	Check(rifle.zoom == 9.0f, "going back moves it to the chosen zoom");
	Sniper_MoveZoom(&rifle, kSniperZoomingOut, kOneSecond);
	Check(rifle.zoom == 1.0f, "zooming out moves it to none");
}

// A saved game is a file from outside the game, so its number could be anything at all
static void TestAZoomFromASave()
{
	puts("a zoom from a saved game");

	Check(SniperZoom_FromSave(1.0f) == 1.0f, "no zoom is fine");
	Check(SniperZoom_FromSave(20.0f) == 20.0f, "and so is the most zoom");
	Check(SniperZoom_FromSave(7.5f) == 7.5f, "and anything between");
	Check(SniperZoom_FromSave(0.0f) == 1.0f, "nothing at all would divide by nothing, so it's made no zoom");
	Check(SniperZoom_FromSave(0.99f) == 1.0f, "below 1 is made no zoom");
	Check(SniperZoom_FromSave(-5.0f) == 1.0f, "a number below nothing too");
	Check(SniperZoom_FromSave(20.01f) == 1.0f, "over 20 too");

	float const notANumber = sqrtf(-1.0f);
	Check(SniperZoom_FromSave(notANumber) == 1.0f, "and something that isn't a number");
	float const huge = 1e30f * 1e30f;
	Check(SniperZoom_FromSave(huge) == 1.0f, "and infinity");
}

static void TestTheSlowdown()
{
	puts("how much the zoom slows the player");

	int const freeSpeed = 57344;    // seven eighths, as the rifle's own row has it

	Check(SniperZoom_Slowed(freeSpeed, kSniperFullZoomMoving, 1.0f) == freeSpeed, "no zoom doesn't slow anything");
	Check(SniperZoom_Slowed(freeSpeed, kSniperFullZoomMoving, 20.0f) == kSniperFullZoomMoving, "full zoom gives moving a fifth");
	Check(SniperZoom_Slowed(freeSpeed, kSniperFullZoomTurning, 20.0f) == kSniperFullZoomTurning, "turning a tenth");
	Check(SniperZoom_Slowed(freeSpeed, kSniperFullZoomJumping, 20.0f) == kSniperFullZoomJumping, "and jumping a half");

	int const halfWay = SniperZoom_Slowed(freeSpeed, kSniperFullZoomMoving, 10.5f);
	Check(abs(halfWay - (freeSpeed + kSniperFullZoomMoving) / 2) <= 1, "half way up the zoom is half way between");

	Check(kSniperFullZoomMoving == 13107 && kSniperFullZoomTurning == 6553 && kSniperFullZoomJumping == 32768, "the numbers are a fifth, a tenth and a half");
}

static void TestTheStepsThatWait()
{
	puts("the steps that wait for something");

	SniperRifleState rifle;
	SniperRifle_Start(&rifle);

	Check(!SniperStep_MayEnd(&rifle, kSniperWaitingForTrigger, true, false), "the scope stays up with the trigger held");
	Check(SniperStep_MayEnd(&rifle, kSniperWaitingForTrigger, false, true), "and it's let go when the trigger is, whatever secondary's doing");
	Check(!SniperStep_MayEnd(&rifle, kSniperZoomingIn, false, true), "zooming in goes on with secondary held");
	Check(SniperStep_MayEnd(&rifle, kSniperZoomingIn, true, false), "and ends when it's let go, whatever the trigger's doing");

	rifle.zoom = 5.0f;
	rifle.zoomToGoBackTo = 9.0f;
	Check(!SniperStep_MayEnd(&rifle, kSniperZoomingBack, false, false), "going back isn't over until it gets there");
	rifle.zoom = 9.0f;
	Check(SniperStep_MayEnd(&rifle, kSniperZoomingBack, false, false), "and then it is");

	rifle.zoom = 2.0f;
	Check(!SniperStep_MayEnd(&rifle, kSniperZoomingOut, false, false), "zooming out isn't over above no zoom");
	rifle.zoom = 1.0f;
	Check(SniperStep_MayEnd(&rifle, kSniperZoomingOut, false, false), "and then it is");

	Check(SniperStep_MayEnd(&rifle, kSniperWorkingBolt, true, true), "the bolt only has its timer");
	Check(SniperStep_MayEnd(&rifle, kSniperScopeRising, true, true), "and so does the scope");
}

static void TestATapOfSecondary()
{
	puts("tapping secondary fire");

	SniperRifleState rifle;
	SniperRifle_Start(&rifle);
	rifle.zoomToGoBackTo = 9.0f;    // from the last time the scope was up

	Check(Sniper_WhenIdle(&rifle, false, false) == kSniperIdle, "with nothing pressed, nothing happens");
	Check(Sniper_WhenIdle(&rifle, true, false) == kSniperScopeRising, "a press brings the scope up");
	Check(!rifle.scopeIsUp, "which isn't up until it's finished rising");

	Check(Sniper_AfterScopeRose(&rifle, false) == kSniperIdle, "let go by then, and it's ready, with no zoom");
	Check(rifle.scopeIsUp, "and the scope's up");
	Check(rifle.zoomToGoBackTo == 1.0f, "and no zoom is what it'd go back to");

	Check(Sniper_WhenIdle(&rifle, false, false) == kSniperIdle, "it stays up with nothing pressed");
	Check(Sniper_WhenIdle(&rifle, true, false) == kSniperZoomingOut, "pressing again takes it down");
	Check(Sniper_AfterZoomingOut(&rifle) == kSniperScopeLowering, "through no zoom, to lowering");
	Check(!rifle.scopeIsUp, "and from then it isn't up");
	Check(Sniper_AfterScopeLowered(&rifle) == kSniperIdle, "then it's ready");
	Check(Sniper_WhenIdle(&rifle, false, false) == kSniperIdle, "and it doesn't come back by itself");
}

static void TestHoldingSecondary()
{
	puts("holding secondary fire");

	SniperRifleState rifle;
	SniperRifle_Start(&rifle);

	Check(Sniper_WhenIdle(&rifle, true, false) == kSniperScopeRising, "the scope comes up");
	Check(Sniper_AfterScopeRose(&rifle, true) == kSniperZoomingIn, "still held, it zooms in");

	Sniper_MoveZoom(&rifle, kSniperZoomingIn, kOneSecond);
	Check(Near(rifle.zoom, 11.0f), "for as long as it's held");
	Check(Sniper_AfterZoomingIn(&rifle) == kSniperIdle, "let go, it's ready");
	Check(rifle.zoomToGoBackTo == rifle.zoom, "and it remembers the zoom chosen");
	Check(rifle.scopeIsUp, "with the scope still up");
}

// The scope has to come down for the bolt and go back to the same zoom, with nothing asked of the player
static void TestAShotThroughTheScope()
{
	puts("a shot through the scope");

	SniperRifleState rifle;
	SniperRifle_Start(&rifle);
	rifle.scopeIsUp = true;
	rifle.zoom = 12.0f;
	rifle.zoomToGoBackTo = 12.0f;

	Check(Sniper_AfterRecoil(&rifle) == kSniperWaitingForTrigger, "after the recoil, the scope stays up while the trigger's held");
	Check(rifle.scopeIsUp && rifle.zoom == 12.0f, "at the same zoom");

	Check(Sniper_AfterWaitingForTrigger(&rifle) == kSniperZoomingOut, "let go, it zooms out");
	Check(rifle.spentCaseToThrow, "with a case to throw out");
	Check(rifle.scopeShouldComeBack, "and it means to come back");

	Sniper_MoveZoom(&rifle, kSniperZoomingOut, kOneSecond);
	Check(Sniper_AfterZoomingOut(&rifle) == kSniperScopeLowering, "then it's lowered");
	Check(Sniper_AfterScopeLowered(&rifle) == kSniperWorkingBolt, "for the bolt");
	Check(rifle.afterLowering == kSniperIdle, "and next time it's lowered it's for no reason, unless one's given");

	Check(Sniper_WhenIdle(&rifle, false, false) == kSniperScopeRising, "after the bolt, it comes back by itself");
	Check(Sniper_AfterScopeRose(&rifle, false) == kSniperZoomingBack, "and goes back to its zoom");
	Sniper_MoveZoom(&rifle, kSniperZoomingBack, kOneSecond);
	Check(rifle.zoom == 12.0f, "the same zoom it had");
	Check(rifle.scopeIsUp, "with the scope up");
	Check(Sniper_WhenIdle(&rifle, false, false) == kSniperIdle, "and there it stays");
}

static void TestAShotWithTheScopeDown()
{
	puts("a shot with the scope down");

	SniperRifleState rifle;
	SniperRifle_Start(&rifle);

	Check(Sniper_AfterRecoil(&rifle) == kSniperWorkingBolt, "the bolt comes straight after the recoil");
	Check(rifle.spentCaseToThrow, "and throws out a case");
	Check(!rifle.scopeShouldComeBack, "and the scope has no reason to come up");
	Check(Sniper_WhenIdle(&rifle, false, false) == kSniperIdle, "so it doesn't");
}

static void TestLeavingTheScopeAfterAShot()
{
	puts("taking the scope down yourself, after a shot");

	SniperRifleState rifle;
	SniperRifle_Start(&rifle);
	rifle.scopeIsUp = true;
	rifle.zoom = 8.0f;
	rifle.zoomToGoBackTo = 8.0f;
	rifle.scopeShouldComeBack = true;

	Check(Sniper_WhenIdle(&rifle, true, false) == kSniperZoomingOut, "pressing secondary takes it down");
	Check(!rifle.scopeShouldComeBack, "and it doesn't mean to come back");
}

static void TestReloading()
{
	puts("reloading");

	SniperRifleState rifle;
	SniperRifle_Start(&rifle);

	// With the scope down, and rounds left
	Check(Sniper_BeforeReload(&rifle, true) == kSniperReloading, "with the scope down, a reload just starts");
	Check(!rifle.reloadBeganEmpty, "there being a round in the gun already");
	Check(Sniper_AfterReload(&rifle, false) == kSniperIdle, "and no bolt's needed after");

	// From empty
	Check(Sniper_BeforeReload(&rifle, false) == kSniperReloading, "from empty, too");
	Check(rifle.reloadBeganEmpty, "but it notes that");
	rifle.spentCaseToThrow = true;
	Check(Sniper_AfterReload(&rifle, false) == kSniperWorkingBolt, "and the bolt's worked afterwards, to get a round in");
	Check(!rifle.spentCaseToThrow, "with no case to throw out");
	Check(!rifle.reloadBeganEmpty, "and that's done with");

	// With the scope up
	SniperRifle_Start(&rifle);
	rifle.scopeIsUp = true;
	rifle.zoom = 15.0f;
	rifle.zoomToGoBackTo = 15.0f;
	Check(Sniper_BeforeReload(&rifle, false) == kSniperZoomingOut, "with the scope up, it comes down first");
	Check(!rifle.reloadBeganEmpty, "and hasn't begun the reload yet");
	Check(rifle.scopeShouldComeBack, "and it means to come back");
	Sniper_MoveZoom(&rifle, kSniperZoomingOut, kOneSecond);
	Check(Sniper_AfterZoomingOut(&rifle) == kSniperScopeLowering, "zooms out and lowers");
	Check(Sniper_AfterScopeLowered(&rifle) == kSniperReloading, "and then the reload comes");
	Check(Sniper_BeforeReload(&rifle, false) == kSniperReloading, "which starts, this time");
	Check(rifle.reloadBeganEmpty, "having noted it's from empty");
	Check(Sniper_AfterReload(&rifle, false) == kSniperWorkingBolt, "then the bolt");
	Check(Sniper_WhenIdle(&rifle, false, false) == kSniperScopeRising, "then the scope comes back");
	Check(Sniper_AfterScopeRose(&rifle, false) == kSniperZoomingBack, "to its zoom");

	// With rounds left and the scope up, there's no bolt, and the scope comes straight back
	SniperRifle_Start(&rifle);
	rifle.scopeIsUp = true;
	Sniper_BeforeReload(&rifle, true);
	Sniper_AfterZoomingOut(&rifle);
	Sniper_AfterScopeLowered(&rifle);
	Sniper_BeforeReload(&rifle, true);
	Check(Sniper_AfterReload(&rifle, false) == kSniperScopeRising, "with rounds left there's no bolt, and the scope comes straight back");
}

static void TestChangingWeapon()
{
	puts("changing weapon");

	SniperRifleState rifle;
	SniperRifle_Start(&rifle);

	Check(Sniper_BeforePuttingAway(&rifle) == kSniperPuttingAway, "with the scope down, it's just put away");

	rifle.scopeIsUp = true;
	rifle.zoom = 3.0f;
	Check(Sniper_BeforePuttingAway(&rifle) == kSniperZoomingOut, "with the scope up, it comes down first");
	Sniper_MoveZoom(&rifle, kSniperZoomingOut, kOneSecond);
	Check(Sniper_AfterZoomingOut(&rifle) == kSniperScopeLowering, "zooms out and lowers");
	Check(Sniper_AfterScopeLowered(&rifle) == kSniperPuttingAway, "then it's put away");

	// During a reload from empty, the bolt can't be worked before it's put away, so it's owed
	SniperRifle_Start(&rifle);
	Sniper_BeforeReload(&rifle, false);
	Check(Sniper_AfterReload(&rifle, true) == kSniperPuttingAway, "after a reload from empty, a change of weapon goes first");
	Check(rifle.boltOwedWhenDrawn, "and the bolt is owed");
	Check(!rifle.reloadBeganEmpty, "and not owed twice");

	rifle.spentCaseToThrow = true;
	Check(Sniper_WhenDrawn(&rifle) == kSniperWorkingBolt, "it's worked when the rifle comes back out");
	Check(!rifle.spentCaseToThrow, "with no case to throw out");
	Check(!rifle.boltOwedWhenDrawn, "and then it's paid");
	Check(Sniper_WhenDrawn(&rifle) == kSniperIdle, "so the next time it's drawn there's none");

	SniperRifle_Start(&rifle);
	Sniper_BeforeReload(&rifle, true);
	Check(Sniper_AfterReload(&rifle, true) == kSniperPuttingAway, "with rounds left it's put away too");
	Check(!rifle.boltOwedWhenDrawn, "owing nothing");
}

static void TestRoundsToLoad()
{
	puts("how many rounds a reload takes");

	unsigned int const oneRound = 65536;
	unsigned int const full = 6 * oneRound;

	Check(Sniper_RoundsToLoad(0, full, 50) == 6, "an empty rifle takes six");
	Check(Sniper_RoundsToLoad(2 * oneRound, full, 50) == 4, "with two left it takes four, as the reload key can ask for");
	Check(Sniper_RoundsToLoad(0, full, 3) == 3, "and it can only take what he has");
	Check(Sniper_RoundsToLoad(5 * oneRound, full, 1) == 1, "down to the last one");
	Check(Sniper_RoundsToLoad(0, full, 0) == 0, "none if he has none");
	Check(Sniper_RoundsToLoad(full, full, 50) == 0, "none if it's full");
	Check(Sniper_RoundsToLoad(100 * oneRound, full, 50) == 0, "and none, not a number below nothing, if a save has more in it than it holds");
	Check(Sniper_RoundsToLoad(0, full, -5) == 0, "nor from spares below nothing");
	Check(Sniper_RoundsToLoad(0xffffffffu, full, 50) == 0, "nor with the biggest number there is in the gun");
}

static void TestAReloadThatsDue()
{
	puts("a reload that's due");

	SniperRifleState rifle;
	SniperRifle_Start(&rifle);
	rifle.scopeShouldComeBack = true;
	Check(Sniper_WhenIdle(&rifle, false, true) == kSniperIdle, "the scope doesn't come back just to go down again");
	Check(Sniper_WhenIdle(&rifle, false, false) == kSniperScopeRising, "but it does with no reload due");

	SniperRifle_Start(&rifle);
	Check(Sniper_WhenIdle(&rifle, true, true) == kSniperIdle, "a press doesn't raise it with a reload due either");
	rifle.scopeIsUp = true;
	rifle.scopeShouldComeBack = true;
	Check(Sniper_WhenIdle(&rifle, true, true) == kSniperIdle, "nor take it down");
	Check(rifle.scopeShouldComeBack, "and it still means to come back afterwards");
}

// A saved game is a file from outside the game, so every number in the rifle's state could be anything
static void TestASavedState()
{
	puts("a saved game's copy of the rifle's state");

	SniperRifleState rifle;
	SniperRifle_Start(&rifle);
	Check(SniperRifle_StateIsGood(&rifle), "a new rifle's is fine");

	rifle.zoom = 7.5f;
	rifle.zoomToGoBackTo = 20.0f;
	rifle.scopeIsUp = 1;
	rifle.scopeShouldComeBack = 1;
	rifle.spentCaseToThrow = 1;
	rifle.reloadBeganEmpty = 1;
	rifle.boltOwedWhenDrawn = 1;
	Check(SniperRifle_StateIsGood(&rifle), "and so is one using the top of everything");

	int const reasons[] = { kSniperIdle, kSniperWorkingBolt, kSniperReloading, kSniperPuttingAway };
	for (int reason : reasons)
	{
		rifle.afterLowering = reason;
		Check(SniperRifle_StateIsGood(&rifle), "a real reason for the scope to come down is fine");
	}

	float const notANumber = sqrtf(-1.0f);
	float const badZooms[] = { 0.0f, -1.0f, 20.5f, notANumber };

	for (float const zoom : badZooms)
	{
		SniperRifle_Start(&rifle);
		rifle.zoom = zoom;
		Check(!SniperRifle_StateIsGood(&rifle), "a bad zoom is refused");
		SniperRifle_Start(&rifle);
		rifle.zoomToGoBackTo = zoom;
		Check(!SniperRifle_StateIsGood(&rifle), "and a bad zoom to go back to");
	}

	int SniperRifleState::*const yesesAndNos[] =
	{
		&SniperRifleState::scopeIsUp, &SniperRifleState::scopeShouldComeBack, &SniperRifleState::spentCaseToThrow,
		&SniperRifleState::reloadBeganEmpty, &SniperRifleState::boltOwedWhenDrawn
	};

	for (int SniperRifleState::*const field : yesesAndNos)
	{
		SniperRifle_Start(&rifle);
		rifle.*field = 2;
		Check(!SniperRifle_StateIsGood(&rifle), "a yes or no of 2 is refused");
		rifle.*field = -1;
		Check(!SniperRifle_StateIsGood(&rifle), "and of -1");
	}

	int const badReasons[] = { -1, kSniperScopeRising, kSniperScopeLowering, kSniperWaitingForTrigger, kSniperZoomingIn, kSniperZoomingBack, kSniperZoomingOut, kSniperPuttingAway + 1, 1000 };

	for (int reason : badReasons)
	{
		SniperRifle_Start(&rifle);
		rifle.afterLowering = reason;
		Check(!SniperRifle_StateIsGood(&rifle), "a reason the scope never comes down for is refused");
	}
}

static void TestStartingOver()
{
	puts("starting over");

	SniperRifleState rifle;
	SniperRifle_Start(&rifle);
	Check(rifle.zoom == 1.0f && rifle.zoomToGoBackTo == 1.0f, "a new rifle has no zoom");
	Check(!rifle.scopeIsUp && !rifle.scopeShouldComeBack, "and its scope's down");
	Check(rifle.afterLowering == kSniperIdle, "with no reason to lower it");
	Check(!rifle.spentCaseToThrow && !rifle.reloadBeganEmpty && !rifle.boltOwedWhenDrawn, "and nothing's owed");
}

int main()
{
	TestTheZoom();
	TestAZoomFromASave();
	TestTheSlowdown();
	TestTheStepsThatWait();
	TestATapOfSecondary();
	TestHoldingSecondary();
	TestAShotThroughTheScope();
	TestAShotWithTheScopeDown();
	TestLeavingTheScopeAfterAShot();
	TestReloading();
	TestChangingWeapon();
	TestRoundsToLoad();
	TestAReloadThatsDue();
	TestASavedState();
	TestStartingOver();

	if (checksFailed)
	{
		printf("FAILED: %d checks\n", checksFailed);
		return 1;
	}

	puts("All checks passed");
	return 0;
}
