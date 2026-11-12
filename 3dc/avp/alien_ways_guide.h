// The guide: which aliens are following a way across the alien ways map (alien_ways_map.h), what each one's way is, and the one search that's
// running for them. Written 2026-11-08 by Melanikus for the AvP port ("For Aliens, Floor is lava", #109 and #118).
//
// It knows nothing of the game, so a test can drive it with made-up aliens. alien_ways.cpp is the game's side: it tells the guide where each
// alien is and where it's going, and does what the guide says.
//
// ONE FRAME:
//
//      AlienWays_Frame, before any alien moves
//        Frame: every alien nothing has asked about for kAlienWaysForgetAfter is let go, and named, so the game gives it its usual gravity
//        |      back. One that missed only a frame or a few is kept, way and all, and named apart (MissedLastFrame), so the game can let go
//        |      at once of one that's in a state that never asks. Then the searches get up to kAlienWaysLooksPerFrame polygons between them,
//        |      first asked, first served.
//      each alien that's coming for its target (bh_near.cpp)
//        Steer: says where to head for now, or that it has no way yet, and then the game's own code steers it as it always has.
//               It asks for a search when there's no way, or the way's 2 s old, or the goal has moved 3 m, or the alien is 3 m off the way.
//
// AN ALIEN STOPS BEING GUIDED (and gets its usual gravity back) when:
// - Steer says no. It's still coming for its target, so the game's own code sets its gravity that same frame, as it always has.
// - Nothing asks Steer for it for kAlienWaysForgetAfter, half a second: it's gone far, or gone, or it stays in a state that doesn't ask.
//   A frame or a few with no asking don't let it go. A hunting alien skips asking now and then, and when each skip dropped its way, the way
//   was searched for again nearly every frame and never followed (Sixth's 5-alien film, 2026-11-08). Through those frames it's still on its
//   way for Guided, so the game keeps it crawling, and its gravity off.
// - The game lets it go (Forget), at the first frame it's in a state that never asks, such as attacking or dying: the game gives it its
//   usual gravity back then, so a dying alien doesn't hang on a ceiling. Most of those states set their own gravity as they start anyway;
//   the ones that don't, avoiding something and taunting, are the ones this is for.
// - The level ends: Clear, and the map and every alien go with it.
//
// A WAY THAT'S BEING FOLLOWED IS NEVER DROPPED BY A SEARCH THAT FAILS: only a new way replaces it. So an alien on a ceiling doesn't let go
// because its way's renewal found nothing.
//
// ALL ON THE GAME'S THREAD. Nothing here asks for memory after it's made: every way is kept in room set aside for it.
#ifndef ALIEN_WAYS_GUIDE_H
#define ALIEN_WAYS_GUIDE_H

#include <stddef.h>
#include <stdint.h>

#include "alien_ways_map.h"
#include "alien_ways_lookahead.h"

#include <vector>

class NpcGrid;

// Who an alien is, in two numbers the game picks. The game uses where the alien is kept and its name, so a new alien kept where a dead one was
// is never taken for it.
struct AlienWaysWho
{
	uint64_t place;
	uint64_t name;
};

// Some of the polygons handed to the map's Build, from first up to end: one module's, say. See AlienWaysMap::NearestPolygon.
struct AlienWaysRange
{
	size_t first, end;
};

// The rules. Each is a first guess, to be tuned from the log's lines and from film.
int const kAlienWaysFloorPrice = 10;              // the floor costs 10 times as much as a wall or a ceiling. Step 2c's knee.
size_t const kAlienWaysMostFollowers = 64;        // aliens with a way at once. One more is steered by the game's own code.
int const kAlienWaysLooksPerFrame = 500;          // polygons all the searches may look at in one frame, between them

// Modified 2026-11-08 by Melanikus for the AvP port: the same for the grid (npc_grid.h, -alienwaysgrid; #145, for #109).
// Cells all the searches may look at in one frame, between them, and the most one search may look at in all before it gives up. A cell is
// a lot smaller than a polygon, so there are many more of them. Primary's first guesses, for the first film to measure (each search's line
// in the log says how many cells it looked at, in how many frames and how many ms):
// - a look is about 1 microsecond in C++, so 5,000 a frame is about 5 ms;
// - a search holds about 60 bytes for each cell it sees, a few for each one it looks at, so 200,000 looks can hold about 35 MB while it runs,
//   on top of the grid. A search that would look at more stops there, and that alien's next search goes to the polygon map.
int const kAlienWaysGridLooksPerFrame = 5000;
int const kAlienWaysGridMostLooks = 200000;
int const kAlienWaysNearEnough = 2000;            // mm: how far an alien, or its goal, can be from a polygon and still start on it. An alien
                                                  // starts on the nearest polygon of the kind it's standing on, by its down, if there's one that near.
int const kAlienWaysReached = 600;                // mm across the surface the alien is on: a point that near has been reached...
int const kAlienWaysBodyHeight = 1500;            // ...as long as it's no more than this far above or below the alien's own middle
int const kAlienWaysOffTheWay = 3000;             // mm: an alien this far from its way has lost it, and asks again
int const kAlienWaysAskAgainAfter = 2000;         // ms: a way this old is searched for again, and followed till the new one comes
int const kAlienWaysGoalMoved = 3000;             // mm: a goal that's moved this far since the search is searched for again
int const kAlienWaysHeadway = 100;                // mm: getting this much nearer to the next point counts as headway...
// THE SHORTER WAITS (modified 2026-11-11 by Melanikus (Third) for the AvP port, #109): the owner, after his play on 26457ece, "maybe
// reduce the time to re-path when stuck". No headway for 900 ms (was 1500) is stuck, and the rest after a stuck at a way's start is
// 1000 ms (was 3000). 900 and not less: headway is 100 mm nearer the next point, and an alien slowing to turn at a corner can make
// none for half a second, so the stuck test has to give it more than that; and it comes after the slide's 500 ms, so a slide is tried
// first. Shorter waits only bring the watchdog's re-plan sooner. They can't stop it (R-LAVA-9). THE CAP stays 3 re-plans a minute: it's the
// guard on the searches' time, and his play never used it up.
int const kAlienWaysNoHeadwayFor = 900;           // ...and with none for this many ms, the alien is stuck: its way is dropped...
int const kAlienWaysRestAfterStuck = 1000;        // ...and the game's own code steers it for this many ms before it asks again
int const kAlienWaysForgetAfter = 500;            // ms: an alien nothing has asked about for this long is let go. Shorter gaps keep it.

// Modified 2026-11-11 by Melanikus (Third) for the AvP port: THE CLOCK and THE WATCHDOG (#109; PO's ruling R-LAVA-P3, in AvP-docs\RULINGS.md).
//
// THE CLOCK. Until now, every new way started the no-headway clock again. A way is searched for again whenever its goal moves 3 m, which is
// every half second or so when the marine runs. So an alien stuck at one spot kept getting new ways, and was never found stuck. In the
// owner's own play (2026-11-11, genshd1), one alien got 12 ways in 5 seconds from the same spot, and was found stuck only when he stopped
// running. Now a new way that comes while the alien is following one keeps the clock going, unless the alien has moved kAlienWaysMovedOn
// since it last made headway.
int const kAlienWaysMovedOn = 1000;               // mm
//
// THE WATCHDOG. What an alien found stuck does next:
// - The polygon at the point it couldn't reach is SHUT FOR IT, and for no other alien, for kAlienWaysShutFor. Its searches keep off it,
//   on the grid and on the map, except when that polygon is the one it's on, or its goal's. It can have up to kAlienWaysMostShut such
//   polygons at once; one more takes the place of the one shut longest ago.
// - If it was stuck after it had reached a point of this way, it RE-PLANS AT ONCE: it asks for a new way from where it is, with nothing
//   in between, instead of resting.
// - If it was stuck before it reached any point of this way, it's stuck where the way starts. A search from the same spot would start on
//   the same cell, so it rests kAlienWaysRestAfterStuck first, as before, while the game's own code moves it. In the owner's play that
//   rest got it out every time. Its next search keeps off the polygon it was heading for.
// - THE CAP: at most kAlienWaysMostReplans re-plans in any kAlienWaysReplansWithin. Stuck again past that, it rests, as before. So an
//   alien can never spin from one re-plan to the next.
// Each alien's shut polygons and re-plans are its own. Nothing is shared between aliens.
// THE WATCHDOG'S MEMORY (modified 2026-11-11 by Melanikus (Third) for the AvP port, #109): an alien let go keeps them, by who it is, for as
// long as any of them would have lasted (a minute at most), and has them back if it comes back. The game lets an alien go the moment it's
// in a state that never asks for a way, and 1999's code puts one there for a second or two (Avoidance, a jump), so without this an alien
// back from one had nothing shut and a fresh cap (Primary's count of the owner's watchdog play: the same lip searched again, and "re-plan
// 1 of 3" twice in 15 s). A dead alien never comes back, so its memory just runs out; at most kAlienWaysMostFollowers memories are kept,
// the one ending first giving way, so nothing piles up across a level (PO's ruling). The 1999 attack and pounce aren't touched: the guide is only asked while an alien is coming for its target.
int const kAlienWaysShutFor = 20000;              // ms
size_t const kAlienWaysMostShut = 4;
int const kAlienWaysMostReplans = 3;
int const kAlienWaysReplansWithin = 60000;        // ms: a minute

// THE HAND-OVER. Added 2026-11-11 by Melanikus (Third) for the AvP port (#109).
// The owner: "I would expect this place to have a quick/bugless alien reaching you, seems though our weight is causing him to move away
// when he's in 'pounce' range". The game's pounce (AlienIsAbleToPounce, bh_near.cpp) has to start on a flat floor, and with the floor
// dear a guided alien keeps off it all the way in. So a guided alien in pounce range could never pounce.
// Now, when an alien's goal is its target itself, it can see it clearly (AlienWaysAsk::targetInSight: the game's own line-of-sight test
// for a pounce), and it's within kAlienWaysHandOverRange, the guide lets it go. The last stretch is then the game's own, as in 1999: its
// curve at the target, still climbing what it touches, its pounce and its attack. The owner's ruling R-LAVA-5 (2026-11-11): "The
// original game should only take over (walk directly/pounce) when we're in clear view/end of the path". 8 m is both where 1999's pounce can start (ALIEN_POUNCE_STARTMAXRANGE) and where 1999's alien starts curving at its target
// (ALIEN_CURVETOPLAYERDIST). The guide takes it back only beyond kAlienWaysTakeBackRange, so an alien near the line doesn't flip between
// the two every frame.
int const kAlienWaysHandOverRange = 8000;         // mm
int const kAlienWaysTakeBackRange = 10000;        // mm

// THE REST, HELD FROM AVOIDING. Added 2026-11-11 by Melanikus (Third) for the AvP port (#109; PO's go, after Primary's placed runs of the
// owner's spots: in its 3 s rest after a stuck, 1999's Avoidance carried one alien from 19 m off its target to 37 m, and its next way was
// 62.5 m against 36.7 m).
// - While an alien rests after it was found stuck, or waits for a re-plan's way (RestingAfterStuck), the game's own code still moves it.
//   But it isn't let send it off to avoid something: the game asks AlienWays_HoldsBackAvoiding (alien_ways.h), as it does while a way
//   steers it.
// - So that it can't stand pushing a wall for the whole rest: if it hasn't moved kAlienWaysRestMovedOn in kAlienWaysRestStillFor, the rest
//   ends and it re-plans at once, within THE CAP. The polygon it was stuck at is shut for it already, so the new way goes round it. With
//   its re-plans used up, it rests on.
int const kAlienWaysRestStillFor = 500;           // ms (was 1000: THE SHORTER WAITS, above; modified 2026-11-11 by Melanikus (Third))
int const kAlienWaysRestMovedOn = 300;            // mm: a cell of the grid

// THE SLIDE. Added 2026-11-11 by Melanikus (Third) for the AvP port (#109). The owner's ruling R-LAVA-4b ("if there is nothing we can do
// we'll get them to 'cheat' a bit (clip through places they're stuck)"), and how it must be: "Linearly collision free (animated with
// walking) interpolates to the next node's postion and orientation", "When it gets stuck for more than 0.5 secs", "Should look like
// normally walking, we're just... giving it a free of collisions nudge to the next node", "as long as they're small gaps and we keep the
// collisioning system on otherwise". His ruling R-LAVA-7: no finer grid; the snags the grid can't see are the slide's job.
// - An alien on its way that makes no headway (kAlienWaysHeadway nearer the point it's heading for) for kAlienWaysSlideAfterStuck slides to
//   that point: the one its stuck line would name, never a later one, as a way round a thin wall can have points on both sides of it close
//   together.
// - Only a small gap: that point less than kAlienWaysMostSlide from where the alien's box touches what it's on (its place is the box's
//   foot, half a metre out from a wall and nearly a metre under a ceiling: Secondary's review, S1), and the alien no more than
//   kAlienWaysMostOffTheWayToSlide across its surface from the stretch it's on, as a straight line from further off is what could cross a
//   thin wall. Never across a THROUGH join (npc_grid.h) on the grid: onto a face hidden behind the one it's on (S6). Otherwise the
//   watchdog takes it, as before. The game refuses one whose end box isn't clear, or whose line goes into a module that isn't a way on
//   from the one before (alien_ways.cpp; S5 and S3).
// - The game moves it at its own speed, so its walk or crawl plays as ever, straight to that point, with the level's walls not in its way
//   and no gravity, as 1999's queen climbs out of her pit (bh_queen.cpp): it's still a body, so it can be hit, and can hit. Its body turns
//   on the way onto the point's face. Collisions and gravity are back as it arrives, or if it takes longer than kAlienWaysMostSlideMs.
int const kAlienWaysSlideAfterStuck = 500;        // ms
int const kAlienWaysMostSlide = 1000;             // mm
int const kAlienWaysMostOffTheWayToSlide = 500;   // mm
int const kAlienWaysMostSlideMs = 1500;           // ms

// THE SLIDE, ONCE IT GOES. Added 2026-11-11 by Melanikus (Third) for the AvP port (#109), from the owner's play on 26457ece (rehearsal\
// 2026-11-11-owner-play-slide\THIRD-READ.md) and PO's list. There, twice, a way the alien had asked for just before its slide came in
// while it slid. The new way's points took the old way's place under the slide, so the slide no longer went where its checks had said,
// and the alien stuck at its own point. And once a slide was started three times in a second, at shft14, which the owner saw as a flicker.
// - THE SLIDE'S OWN POINT: a slide keeps its point and the one after it as they were when it began. Nothing that changes its way can
//   move them.
// - HELD FROM A NEW WAY: a way that comes while the alien slides isn't taken (its log line says so). The alien asks again the moment
//   the slide is over, so it's only late, never lost.
// - Nothing else ends a slide part way. Steer's slide comes first, before the hand-over, which waits the moment it takes to get there.
//   The slide ends only by getting there, by kAlienWaysMostSlideMs, by Hunt or Wander (Secondary's S2), or by a let-go.
// - A REFUSED POINT: a point a slide was refused to, or gave up on after kAlienWaysMostSlideMs, isn't slid to again (within
//   kAlienWaysHeadway) till a re-plan's way comes. A re-plan goes round a shut polygon, so that's where a slide to it is worth trying again.
// R-LAVA-9, the owner's ruling (2026-11-11): "choose a better path and it was awesome" mustn't be "hacked out". What he saw, at shft14 and
// BEND-01, was a refused slide, then a stuck, a re-plan at once with the polygon shut, and a way that worked. None of the above can stop
// that. Being found stuck drops the way and any slide, and until the re-plan's way comes the alien has no way, so no slide can start
// and nothing can hold the re-plan's way back. On top of that, no slide starts while a re-plan's search is waiting, and a re-plan's way
// clears the refused point.

// THE SLIDE'S END, CLEAR. Added 2026-11-11 by Melanikus (Third) for the AvP port (#109; Secondary's re-review of 94421180, S5b, and PO's
// ruling). On the slide's first film (94421180 at SHED06, 3 runs) 10 of its 13 slides were refused, each as its box at the end "wouldn't be
// clear of the level". The end was exactly as far out from its face as the box reaches (450 mm from a wall, 900 under a ceiling, 0 over
// a floor), so the ray from the box's middle to that face met it right at its reach, and a millimetre of rounding refused it or not, at
// random. Near a corner, the box went into the other face too, by 0.15 to 0.3 m (Secondary's geometry of the owner's snags).
// - The end is kAlienWaysSlideClear further out from its face, so its own face is never in its box.
// - PUSH CLEAR: where the box at the end is still in the level, it's pushed clear rather than refused. Each ray from the box's middle that's
//   stopped short of the box's face asks to move the box out of what it met (AlienWays_PushOutOf), and the rays are sent again, up to
//   kAlienWaysPushRounds times. A push of more than kAlienWaysMostPush in all is refused, and so is a box that still isn't clear. Then every
//   other check of the slide is made again for where it ends now: its gap under kAlienWaysMostSlide and never onto a hidden face (the
//   guide's, PushSlide), and its line only where an alien gets to (the game's). It slides only with all of them passed, so the level's
//   walls come back on with its box clear of every face: it never comes back on inside one ("keep the collisioning system on otherwise").
int const kAlienWaysSlideClear = 25;              // mm
// 500 mm: the owner's ruling R-LAVA-6, now 0.5 m (2026-11-11, from Secondary's copy of his two inside-corner refusals, which needed 0.37
// and 0.47 m). It was 300. The slide's own 1 m bound and every check made again after a push stay as they are.
int const kAlienWaysMostPush = 500;               // mm
int const kAlienWaysPushRounds = 3;

// One of the rays the game sends from the box's middle at a slide's end that was stopped short: its way (of length 1), how far short of
// the box's face it was stopped (mm), and the facing of what it met (of length 1), or all 0 when that isn't known
struct AlienWaysBoxHit
{
	double way[3];
	double inBy;
	double facing[3];
};

// PUSH CLEAR (above): how far to move the box so it's out of everything its rays met, in mm. Each hit asks to be moved out from the face
// it met, along that face's facing turned toward the box's middle, by as far as the box goes past the face that way (its inBy along the
// ray, times how squarely the ray met the face), and kAlienWaysSlideClear more, so the face isn't met right at the box's reach again.
// With no facing known, back along its ray. Then, axis by axis, the most any hit asks one way and the most any asks the other are added,
// so two rays meeting one face (one to the box's side and one to its corner) don't push twice, and hits on two opposite faces cancel as
// far as they can: no push makes a box fit a gap narrower than it, and the rays sent again say so. Added 2026-11-11 by Melanikus (Third)
// for the AvP port, #109.
void AlienWays_PushOutOf(const AlienWaysBoxHit *hits, int count, double push[3]);

// An alien's number in the claims (alien_ways_claims.h) is its place in the guide, so the two have to hold the same number of aliens.
// Checked here, where both are known, so the game's own build stops if one of them is changed alone.
static_assert(kAlienWaysClaimsMostAliens == static_cast<int>(kAlienWaysMostFollowers), "the claims hold as many aliens as the guide has places");

// FLANKERS (the user's rule, 2026-11-08: "1 in 3, up to 2.5x", "the 4th and 5th are long route sneaking bastards"). The first aliens on one
// target go the direct way, with the other aliens' claims at their usual weight and a polygon at most 1.5 times its price. Every alien after
// them is a flanker: the others' claims weigh three times as much for it, and a polygon can cost it up to 2.5 times, so it comes round by
// another way where the level has one. An alien's place in that order is its place among the aliens with the same target when its first
// search for it started, and it keeps it while that's its target.
int const kAlienWaysDirectAliens = 3;
double const kAlienWaysFlankerClaimStep = 3.0 * kAlienWaysClaimPriceStep;
double const kAlienWaysFlankerMostPriceFactor = 2.5;

// The kinds of door the game has (ModuleIsADoor, in pfarlocs.cpp), and whether one lets an alien through now. It's the rule the game's own
// routes for aliens follow (AIModuleAdmitsPheromones, in pheromone.cpp), which is why a hunting alien never plans through a locked door:
// - a proximity door opens by itself for an alien that comes near it, so it lets one through unless it's locked, open yet or not;
// - a lift's door, and a door worked by a switch, only while they're open: an alien can't open either. open is as the game says it
//   (GetState, in bh_types.cpp): for a lift's door, anything but shut; for a switch's door, only all the way open.
// A search keeps off a door that doesn't (DOORS, in alien_ways_map.h).
enum AlienWaysDoorKind
{
	kAlienWaysProximityDoor,
	kAlienWaysLiftDoor,
	kAlienWaysSwitchDoor
};

bool AlienWays_DoorLetsAnAlienThrough(AlienWaysDoorKind kind, bool locked, bool open);

// Whether an alien whose down is down has made the step from the stretch that starts at point from onto the one that starts at point onto.
// Where both facings are known (AlienWaysPoint::facing), it goes by the planes:
// - two polygons facing within kAlienWaysSamePlaneDegrees of each other are one surface, and there's no step to make (AlienWays_SameSurface,
//   in alien_ways_map.h, which the aim asks too);
// - otherwise the step is made once the alien is clinging to the new polygon, closely enough: its down within kAlienWaysOnThePlaneDegrees
//   of that polygon's facing turned round. So a step off a bevel, the strip between a wall and the floor that faces half up and the map
//   counts as floor, onto the wall beside it, about 45 degrees, is made straight away; by kind alone it waited for the alien to turn onto
//   the wall, and the alien, pushed by 1999's gravity, never did (Third's read: 12 of 23 stucks at Third's 5-alien spot, 2026-11-08).
// Where either facing isn't known it goes by kind, as before: made if the two are the same kind, or the alien is on the new kind by its down.
int const kAlienWaysOnThePlaneDegrees = 55;
bool AlienWays_StepMade(const AlienWaysPoint &from, const AlienWaysPoint &onto, AlienWaysXYZ down);

// What an alien tells Steer, every frame it's coming for its target
struct AlienWaysAsk
{
	AlienWaysWho who;
	AlienWaysXYZ at;                // where it is
	AlienWaysXYZ down;              // which way is down for it, as the game keeps it: a length of 65536 (its OrientMat's second row)
	AlienWaysRange atPolygons;      // where to look for the polygon it's on: the module it's in
	AlienWaysXYZ goal;              // where it's going
	AlienWaysRange goalPolygons[2]; // where to look for the polygon the goal's on: the alien's module and the target's. The nearer wins.
	bool goalIsTheTarget;           // the goal is the target itself, not a point on the way to it: only so the log can say which
	bool hasTarget;                 // it's coming for a target, whether or not the goal is the target itself...
	AlienWaysWho target;            // ...and which: the aliens coming for one target are counted, for the flankers (kAlienWaysDirectAliens)
	bool targetInSight;             // it can see its target clearly, by the game's own test, worked out only when the goal is the target
	                                // and it's near (THE HAND-OVER). Added 2026-11-11 by Melanikus (Third) for the AvP port, #109.
};

// How one search went, for the log
enum AlienWaysOutcome
{
	kAlienWaysOutcomeFound,
	kAlienWaysOutcomeNoWay,
	kAlienWaysOutcomeAllFloor,      // a way, but all of it on the floor: the game's own code does that as well, and climbs when it can
	kAlienWaysOutcomeNoPolygon,     // the alien, or its goal, wasn't near enough to any polygon to start on
	kAlienWaysOutcomeRefused,       // the map wouldn't start the search: why says why
	kAlienWaysOutcomeStuck,         // not a search: an alien made no headway, so its way was dropped (from: where it was, to: where it was heading)
	kAlienWaysOutcomeHandedOver,    // not a search: an alien in clear sight of its target, near it, was left to the game's own last stretch
	                                // (THE HAND-OVER; from: where it was, to: its target). Added 2026-11-11 by Melanikus (Third), #109.
	kAlienWaysOutcomeRestEnded,     // not a search: an alien resting after a stuck didn't get on, so it re-plans now (THE REST, HELD FROM
	                                // AVOIDING; from: where it is; replan: which re-plan). Added 2026-11-11 by Melanikus (Third), #109.
	kAlienWaysOutcomeSlide          // not a search: an alien stuck on its way slides to the point it was heading for (THE SLIDE; from: where
	                                // it is; to: that point). Added 2026-11-11 by Melanikus (Third), #109.
};

// What the watchdog did with an alien found stuck (THE WATCHDOG, above). Added 2026-11-11 by Melanikus (Third) for the AvP port, #109.
enum AlienWaysAfterStuck
{
	kAlienWaysReplans,              // it asks for a new way at once
	kAlienWaysRestsAtTheStart,      // it was stuck before reaching any point of its way: it rests first
	kAlienWaysRestsCapped           // its re-plans in the last minute are used up: it rests
};

struct AlienWaysReport
{
	AlienWaysOutcome outcome;
	const char *why;                // for kAlienWaysOutcomeRefused
	size_t follower;                // which of the guide's places the alien has, so lines about one alien can be told apart
	AlienWaysWho who;               // the alien itself, so the game can say more about it
	AlienWaysXYZ from, to;
	int frames;                     // how many frames the search was worked on
	long long waitedMs;             // from the ask to the answer, in the game's time
	long long microseconds;         // the computer's time the search itself took, in all
	int looks;                      // polygons looked at
	int points;
	bool cutShort;                  // the way was longer than the map gives, and stops short of the goal
	bool toTheTarget;               // the goal was the target itself (AlienWaysAsk::goalIsTheTarget)
	int fromKind;                   // the kind of polygon the search started on (an AlienWaysKind), or -1 if none was near
	int toKind;                     // the kind of polygon the goal is on, or -1 if none was near: so a refused line can say which end it was
	double floorMm, wallMm, ceilingMm;
	int polygons[kAlienWaysMostWayPoints];   // the map polygons the way is on, in order, each once in a row, for measuring how ways stack
	int polygonCount;
	bool flanker;                   // the search was a flanker's (kAlienWaysDirectAliens)
	double cost;                    // what the way found cost, by the search's own prices, or -1
	int arrivalDegrees;             // for a way found: the way it comes into its goal from, seen from above, in degrees (0 is from +x, 90 from
	                                // +z), so a film can tell whether a flanker came from another side; -1 if it can't be told
	bool onGrid;                    // the search was the grid's (UseGrid): looks are cells, cost is -1, and polygons holds the cell of each
	                                // point of the way instead
	bool tooFar;                    // on the grid: it looked at kAlienWaysGridMostLooks cells without reaching the goal (outcome NoWay)

	// Added 2026-11-11 by Melanikus (Third) for the AvP port (THE WATCHDOG, above; #109).
	// For kAlienWaysOutcomeStuck: what it does next; which re-plan of the last minute this is (1 to kAlienWaysMostReplans; 0 if it rests);
	// and the polygon shut for it, or -1 if the point's polygon isn't known.
	AlienWaysAfterStuck afterStuck;
	int replan;
	int shutPolygon;
	// For a search's outcome: the search was asked for by a re-plan
	bool afterReplan;
	// For a way found: it came while the alien slid, so it wasn't taken, and the alien asks again once it's there (THE SLIDE, ONCE IT
	// GOES; added 2026-11-11 by Melanikus (Third) for the AvP port, #109)
	bool heldForSlide;
};

class AlienWaysGuide
{
public:
	AlienWaysGuide();

	// Forgets every alien, every way and the search. At a level's start and end. The grid too: UseGrid hands it again.
	void Clear();

	// Modified 2026-11-08 by Melanikus for the AvP port: searching the grid (#145, for #109).
	// Which grid to search (npc_grid.h), or nothing for the polygon map. Asked once a frame, before Frame. While there's a grid, every search
	// goes to it, finding the cells under the alien and its goal itself, inside kAlienWaysGridLooksPerFrame cells a frame; the map is still
	// needed, ready, for its doors and its joins, which the claims use. A search running when the grid changes is dropped, and its alien asks
	// again.
	void UseGrid(NpcGrid *grid);

	// Once a frame, before any alien moves, with how long the last frame took. Every alien that was guided last frame and wasn't steered since
	// is let go and written into lapsed, up to mostLapsed of them, so the game can give each its usual gravity back. It gives back how many.
	// Then the searches carry on, looking at no more than looksAllowed polygons. With no map ready, there are no searches.
	size_t Frame(AlienWaysMap *map, int stepMs, int looksAllowed, AlienWaysWho *lapsed, size_t mostLapsed);

	// An alien asks how to go. It gives back true, and the point to head for in towards, when the alien has a way to follow now. False means
	// the game's own code steers it this frame; it may have asked for a search, which takes a frame or a few.
	bool Steer(const AlienWaysMap *map, const AlienWaysAsk &ask, AlienWaysXYZ *towards);

	// Modified 2026-11-08 by Melanikus for the AvP port: keeps an alien going along its way on a frame its state didn't ask (#109).
	// An alien's state can skip asking for a frame: a hunting alien that finds no next module by the marine scent goes to Wander for a frame
	// and back, every other frame. Steer isn't asked then, the game stands it still and lets go of its grip, and a step from one surface
	// onto another, which needs it to cling for a few frames running, never gets made. So on such a frame the game asks Drive instead, and
	// gets the point to head for along the way it already has, worked out as Steer works it out.
	// It gives back false, and the game does as it always did, unless Steer wasn't asked for it this frame and it has a way it has taken up
	// (one resting after being stuck has none). It never counts as asking, so Frame still lets it go at the half second since it was last
	// asked, just as without Drive. A way it has used up or left is for the next Steer to drop: Drive only says false.
	bool Drive(const AlienWaysMap *map, const AlienWaysWho &who, AlienWaysXYZ at, AlienWaysXYZ down, AlienWaysXYZ *towards);

	// The guide's place for an alien, from 0 to kAlienWaysMostFollowers - 1, or -1 if it has none. For the log's watch on one alien.
	int PlaceOf(const AlienWaysWho &who) const { return Find(who); }

	// Reports from Steer, of aliens found stuck since the last Frame, come out with the next Frame's reports.

	// Whether the last Steer for this alien gave back true
	bool Guided(const AlienWaysWho &who) const;

	// Whether Steer was asked for this alien since the last Frame, and gave back true: its way steers it in this frame. Unlike Guided, it's
	// false in a frame nothing asked Steer for it, even before the next Frame lets it go.
	bool GuidedThisFrame(const AlienWaysWho &who) const;

	// Modified 2026-11-08 by Melanikus for the AvP port: for the mover (alien_ways.cpp, #149).
	// Whether this alien was found stuck lately and is still in the kAlienWaysRestAfterStuck that follows, when the game's own code has it.
	// The mover leaves it to that code then, rather than heading it straight for its goal again, which is how it got stuck.
	// Modified 2026-11-11 by Melanikus (Third) for the AvP port (THE WATCHDOG, above; #109): also while it waits for a re-plan's way, which
	// is the same: it was found stuck, and has no way yet.
	bool RestingAfterStuck(const AlienWaysWho &who) const;

	// Whether this alien is handed over to the game's own last stretch now (THE HAND-OVER, above). The mover leaves it to that code as
	// well. Added 2026-11-11 by Melanikus (Third) for the AvP port (#109).
	bool HandedOver(const AlienWaysWho &who) const;

	// THE SLIDE (above). Whether this alien slides now, and if so: the point it slides to, with its kind and facing, where it set off from,
	// and the point of its way after that one (the same point, if it's the way's last), for the game to move it and turn its body. And the
	// game's word that the slide is over: arrived (it got to the point, which it has then reached), or not (refused, as its end is in a
	// module the alien's can't see into; the watchdog has it from there). Added 2026-11-11 by Melanikus (Third) for the AvP port (#109).
	bool Sliding(const AlienWaysWho &who, AlienWaysPoint *to, AlienWaysXYZ *from, AlienWaysXYZ *next) const;
	void SlideEnded(const AlienWaysWho &who, bool arrived);

	// PUSH CLEAR (above): the game has pushed the slide's end clear of the level by push (mm). The guide makes its own checks again for the
	// point that gives, as it made them when the slide began: no push of more than kAlienWaysMostPush, the gap from where the alien's box
	// touched as it set off under kAlienWaysMostSlide, and no THROUGH join (npc_grid.h) on the grid from where it set off onto it, on its
	// point's own face (modified 2026-11-11 by Melanikus (Third)). On yes it keeps the push, and Sliding gives the point moved by it from
	// then on. On no it keeps nothing, why says why in words, and the game refuses the slide. Added 2026-11-11 by Melanikus (Third) for
	// the AvP port, #109.
	bool PushSlide(const AlienWaysWho &who, AlienWaysXYZ push, const char **why);

	// Whether an alien the guide has is to keep clinging to what it's on when no way steers it this frame: resting after a stuck, waiting
	// for its next way, or handed over but still further off than kAlienWaysHandOverRange. Only the game's own curve at its target, within
	// that range and in clear sight, decides its gravity, as in 1999. Without this, 1999's Approach gives a far alien standard gravity every
	// frame, and one on a wall or a ceiling falls (Secondary's read of the owner's play: 7 of his 16 stucks). Added 2026-11-11 by Melanikus
	// (Third) for the AvP port (#109).
	bool KeepsClinging(const AlienWaysWho &who) const;

	// Where every guided alien is now, as it last told Steer, with the kind of surface it's on by its down, and which of the guide's places it
	// has: up to most of them, for the log's once-a-second line (lanes, #138)
	size_t GuidedPlaces(size_t *slots, AlienWaysXYZ *at, int *kinds, size_t most) const;

	// Which alien has one of the guide's places, for the log to name it by its id (alien_ways_stuck.h): false, with who untouched, for a place
	// that isn't one or that no alien has. Added 2026-11-11 by Melanikus for the AvP port (#109).
	bool WhoAt(size_t slot, AlienWaysWho *who) const;

	// What every guided alien's way is now, for the developer view: up to most of them. The points are good until the next Frame or Clear.
	size_t Shown(const AlienWaysPoint **points, int *counts, int *headings, size_t most) const;

	// The last way of every alien that isn't guided now (found stuck, resting, or between its searches), for the view to draw dimmed, so a
	// line doesn't vanish the moment its alien stops following it (the owner, 2026-11-11). A way is kept until its alien has a new one or
	// its place is let go. The points are good until the next Frame or Clear. Added 2026-11-11 by Melanikus (Third) for the AvP port (#109).
	size_t Kept(const AlienWaysPoint **points, int *counts, size_t most) const;

	// The way the guide holds now for the alien at place (the N of the log's "alien N"), and how many points it has; 0 and nothing if none.
	// For the log to say where a way goes, as places. Added 2026-11-11 by Melanikus (Third) for the AvP port (#109).
	int WayOf(size_t place, const AlienWaysPoint **points) const;

	// How the searches that ended in the last Frame went
	size_t Reports(const AlienWaysReport **reports) const { *reports = report; return reportCount; }

	// How many times an alien was turned away because all kAlienWaysMostFollowers places were taken, since the last Clear
	size_t TurnedAway() const { return turnedAway; }

	// The aliens the last Frame kept though nothing had asked Steer about them since the Frame before: kept, way and all, until
	// kAlienWaysForgetAfter. Good until the next Frame or Clear. The game counts them by state for the log, and lets go at once of one in
	// a state that never asks (Forget).
	size_t MissedLastFrame(const AlienWaysWho **who) const { *who = missed; return missedCount; }

	// Lets an alien go now, as if nothing had asked about it for kAlienWaysForgetAfter. Gives back whether it was guided, so the game can
	// give it its usual gravity back. An alien the guide doesn't have gives back false.
	bool Forget(const AlienWaysWho &who);

	// How far along its way Steer aims an alien, in mm (alien_ways_lookahead.h): from its nearest place on the way, a little further on, so it
	// curves round bends instead of running corner to corner. 0, as a guide starts, aims at the way's next point itself, as before. Whether a
	// point is reached, and whether the alien is stuck, still go by the next point. Kept through Clear: it's a setting, not part of a level.
	void SetAimAhead(int mm) { aimAheadMm = mm; }

	// Whether aliens slide (THE SLIDE, above). Off as a guide starts, so a test asks for it; the game turns it on at each level's start. Kept
	// through Clear, as a setting. Added 2026-11-11 by Melanikus (Third) for the AvP port (#109).
	void SetSlides(bool on) { slidesOn = on; }

private:
	struct Follower
	{
		bool used;
		AlienWaysWho who;
		bool steered;                       // Steer was asked for it since the last Frame
		long long steeredAt;                // when Steer was last asked for it
		bool guided;                        // the last Steer gave back true
		bool queued;                        // it wants a search
		long long askedAt;                  // when it last asked for one
		long long restUntil;                // no way, and no asking, till then
		long long stuckUntil;               // found stuck: the game's own code has it till then (RestingAfterStuck)
		AlienWaysAsk last;                  // what it last told Steer: a search starts from this

		int count, heading;                 // the way: count points, heading for point heading
		int keptCount;                      // the last way found's points, kept for the view after it's dropped (Kept); 0 for none
		bool fresh;                         // the way has just come, and the alien hasn't taken it up yet
		long long wayAt;                    // when the way came
		AlienWaysXYZ wayGoal;               // the goal it was searched for
		double nearest;                     // the nearest the alien has been to the point it's heading for
		long long headwayAt;                // when it last got kAlienWaysHeadway nearer
		AlienWaysPoint points[kAlienWaysMostWayPoints];

		// Added 2026-11-11 by Melanikus (Third) for the AvP port (THE CLOCK and THE WATCHDOG, above; #109)
		AlienWaysXYZ headwayFrom;           // where it was when it last made headway
		bool passedAPoint;                  // it has reached a point of this way since it took the way up
		uint32_t pointCells[kAlienWaysMostWayPoints]; // the grid's cell at each point of the way, or kNpcGridNoCell (THE SLIDE, S6)
		int pointPolygons[kAlienWaysMostWayPoints];   // the map polygon at each point of the way, or -1: on the grid, the one under the
		                                    // point's cell
		int shutPolygon[kAlienWaysMostShut];          // the polygons shut for it alone...
		long long shutUntil[kAlienWaysMostShut];      // ...each until then. One whose time has come is free.
		long long replanCountsUntil[kAlienWaysMostReplans];   // each re-plan counts against the cap until a minute after it; then it's free
		bool replanning;                    // the search it's waiting for was asked for by a re-plan
		bool handedOver;                    // THE HAND-OVER: the game's own last stretch has it (added 2026-11-11 by Melanikus (Third))
		AlienWaysXYZ restFrom;              // THE REST, HELD FROM AVOIDING: where it was when it last moved on in its rest...
		long long restMovedAt;              // ...and when (added 2026-11-11 by Melanikus (Third))
		bool sliding;                       // THE SLIDE: it slides now (added 2026-11-11 by Melanikus (Third))...
		int slidePoint;                     // ...to this point of its way...
		long long slideAt;                  // ...since then...
		AlienWaysXYZ slideFrom;             // ...from here...
		AlienWaysXYZ slideDown;             // ...with this down...
		AlienWaysXYZ slidePush;             // ...and its point pushed by this (PUSH CLEAR; added 2026-11-11 by Melanikus (Third))
		AlienWaysPoint slideTo;             // THE SLIDE'S OWN POINT: the point it slides to and the one after it, as they were when it
		AlienWaysPoint slideNext;           // began (THE SLIDE, ONCE IT GOES; added 2026-11-11 by Melanikus (Third))
		AlienWaysXYZ refusedTo;             // A REFUSED POINT: where a slide was last refused to, or gave up on...
		bool refusedValid;                  // ...not slid to again till a re-plan's way comes (added 2026-11-11 by Melanikus (Third))
		long long slideTriedFor;            // the headway clock (headwayAt) a slide was last tried for, so one refused isn't tried every frame
		bool clings;                        // KeepsClinging (added 2026-11-11 by Melanikus (Third))

		bool ranked;                        // it has a place among the aliens coming for target (FLANKERS, above)...
		AlienWaysWho target;
		bool flanker;                       // ...and it's past the first kAlienWaysDirectAliens of them

		bool mapNext;                       // its last search on the grid was refused, found no way or looked at too many cells, so its
		                                    // next goes to the map
	};

	int Find(const AlienWaysWho &who) const;
	int Take(const AlienWaysWho &who);
	void LetGo(int f);
	void DropTheWay(Follower *follower);
	void StartNext(AlienWaysMap *map);
	void Finish(AlienWaysMap *map, AlienWaysSearch result);
	AlienWaysSearch WorkOnGrid(int looksAllowed);
	const std::vector<uint8_t> *ShutForTheSearch(const AlienWaysMap &map, const Follower &follower, int from, int to);
	void NoNewWay(Follower *follower, AlienWaysXYZ goal);

	// THE WATCHDOG (above). Added 2026-11-11 by Melanikus (Third) for the AvP port, #109.
	void ForgetTheWatchdog(Follower *follower);
	void Remember(const Follower &follower);
	void Recall(Follower *follower);
	void ShutForIt(Follower *follower, int polygon);
	int TakeAReplan(Follower *follower);
	void AddReport(const AlienWaysReport &line);

	long long now;
	Follower followers[kAlienWaysMostFollowers];
	size_t turnedAway;

	// The one search that's running, and for which alien, or -1
	int searching;
	AlienWaysXYZ searchFrom, searchGoal;
	bool searchToTheTarget;
	int searchFromKind;
	long long searchAskedAt, searchMicroseconds;
	int searchFrames;
	bool searchFlanker;                 // the search running is a flanker's

	// The grid (UseGrid), whether the search running is on it, whether it gave up for looking at too many cells, the polygons the search
	// keeps off, a byte for each map polygon (ShutForTheSearch: the doors shut when it started, and the polygons shut for its alien; it has to
	// stay as it is until the search ends), and a found way's map polygons, each once in a row, for its claims
	NpcGrid *grid;
	bool searchOnGrid;
	bool searchTooFar;
	std::vector<uint8_t> searchShut;
	std::vector<int> gridWayPolygons;

	AlienWaysReport report[kAlienWaysMostFollowers];
	size_t reportCount;
	bool reportsRead;                   // the reports have been read by a Frame, so the next report starts a new list

	// The aliens the last Frame kept though nothing had asked about them (MissedLastFrame)
	AlienWaysWho missed[kAlienWaysMostFollowers];
	size_t missedCount;

	// What each alien's way claims of the map, so the others' searches pay more there and take other ways where they can (lanes, #138;
	// alien_ways_claims.h). Made once a level, the first Frame there's a map: claimsTried says it was tried, claimsOn that it worked. Without
	// them the searches go as they did before. An alien's place here is its place in followers.
	AlienWaysClaims claims;
	bool claimsTried, claimsOn;

	// THE WATCHDOG'S MEMORY (above): what aliens let go had shut for them and their re-plans, each until its last one ends. One whose time
	// has come is free.
	struct Remembered
	{
		AlienWaysWho who;
		long long until;
		int shutPolygon[kAlienWaysMostShut];
		long long shutUntil[kAlienWaysMostShut];
		long long replanCountsUntil[kAlienWaysMostReplans];
	};

	Remembered remembered[kAlienWaysMostFollowers];

	int aimAheadMm;                     // SetAimAhead
	bool slidesOn;                      // SetSlides (added 2026-11-11 by Melanikus (Third), #109)
	static_assert(kAlienWaysClaimsMostAliens == static_cast<int>(kAlienWaysMostFollowers), "the claims have room for every alien the guide has");
};

#endif
