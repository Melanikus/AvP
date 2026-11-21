// The thinking behind Hudson playing a level on foot, with no game in it, so that a test can run all of it. hudson_play.cpp joins it to the game.
// Written 2026-11-06 by Melanikus for the AvP port.
//
// A level's list of what to do is tools/hudson/chains/<level>.play, and the README beside it gives the format. It comes from outside the code, so
// everything in it is checked before it's used, and a file with anything wrong in it is refused whole, for a reason that names the line.

#ifndef HUDSON_PLAY_STEPS_H
#define HUDSON_PLAY_STEPS_H

#include <math.h>
#include <stddef.h>
#include <string>
#include <vector>

enum HudsonPlayKind
{
	kHudsonPlayPress,     // walk up to a switch, look at it and press the use key
	kHudsonPlayWalkInto,  // walk to a place, because going there sets something off
	kHudsonPlayPickUp,    // walk onto something
	kHudsonPlayDestroy,   // get within sight of something, and shoot it until it's gone
	kHudsonPlayKill,      // the same, for someone
	kHudsonPlayGoTo,      // walk to a place, and nothing else: no object of its name (the lane probe v3's list, added 2026-11-20 by Melanikus (Frost))
	kHudsonPlayMoveTo,    // "teleport": he's put at a place, a logged cheat, for a lift he can't board (the full Genshd1 run, added 2026-11-20 by Melanikus (Frost))
	kHudsonPlayJumpUp,    // "jump": he walks straight at a place higher than a step, no grid, and jumps up onto it (the bypass dais in med02, 780 mm; Drake, Frost 2026-11-20)
	kHudsonPlayDrop       // "drop": he walks straight at a place, no grid, till he has fallen 2 m or is there (a ledge's edge: Genshd1's glass; Frost 2026-11-20)
};

struct HudsonPlayStep
{
	HudsonPlayKind kind;
	int x, y, z;
	std::string name;
};

// Limits on the file. Far more than any level has: the longest of the 32 has 19 steps.
static const size_t kHudsonPlayMostBytes = 256 * 1024;
static const size_t kHudsonPlayLongestLine = 300;
static const size_t kHudsonPlayLongestName = 80;
static const size_t kHudsonPlayMostSteps = 256;
static const int kHudsonPlayFurthest = 64000000; // 64 km from the middle of the world, in millimetres

// A whole number that fits, and nothing else. A word from a file can be anything.
inline bool HudsonPlay_ReadWholeNumber(std::string const &word, int *number)
{
	size_t at = 0;
	bool const below = word[0] == '-'; // an empty word's first character is nothing, so this is safe

	if (below)
	{
		at = 1;
	}

	// Eight digits are as many as the furthest place has, and eight digits can't be too big for the sum below
	if (word.size() == at || word.size() - at > 8)
	{
		return false;
	}

	int value = 0;

	for (; at < word.size(); at++)
	{
		if (word[at] < '0' || word[at] > '9')
		{
			return false;
		}

		value = value * 10 + (word[at] - '0');
	}

	if (value > kHudsonPlayFurthest)
	{
		return false;
	}

	*number = below ? -value : value;
	return true;
}

// Reads one line. Gives back an empty text if it's fine, or else why it isn't.
inline std::string HudsonPlay_ReadLine(std::string const &line, std::vector<HudsonPlayStep> *steps)
{
	if (line.size() > kHudsonPlayLongestLine)
	{
		return "the line is too long";
	}

	if (line.empty() || line[0] == '#')
	{
		return std::string();
	}

	size_t const bar = line.find(" | ");

	if (bar == std::string::npos)
	{
		return "a step needs a bar and a name";
	}

	std::string const name = line.substr(bar + 3);

	if (name.size() > kHudsonPlayLongestName)
	{
		return "that can't be a name";
	}

	// Only plain letters, numbers and punctuation, because it goes into the log. A second bar would mean the line isn't what it looks like.
	for (size_t i = 0; i < name.size(); i++)
	{
		if (name[i] < ' ' || name[i] > '~' || name[i] == '|')
		{
			return "that can't be a name";
		}
	}

	std::vector<std::string> words;
	size_t from = 0;
	std::string const before = line.substr(0, bar);

	while (from <= before.size())
	{
		size_t end = before.find(' ', from);

		if (end == std::string::npos)
		{
			end = before.size();
		}

		words.push_back(before.substr(from, end - from));
		from = end + 1;
	}

	HudsonPlayStep step;

	step.x = step.y = step.z = 0;
	step.name = name;

	if (words[0] == "press")
	{
		step.kind = kHudsonPlayPress;
	}
	else if (words[0] == "walk")
	{
		step.kind = kHudsonPlayWalkInto;
	}
	else if (words[0] == "pickup")
	{
		step.kind = kHudsonPlayPickUp;
	}
	else if (words[0] == "destroy")
	{
		step.kind = kHudsonPlayDestroy;
	}
	else if (words[0] == "kill")
	{
		step.kind = kHudsonPlayKill;
	}
	else if (words[0] == "goto")
	{
		step.kind = kHudsonPlayGoTo;
	}
	else if (words[0] == "teleport")
	{
		step.kind = kHudsonPlayMoveTo;
	}
	else if (words[0] == "jump")
	{
		step.kind = kHudsonPlayJumpUp;
	}
	else if (words[0] == "drop")
	{
		step.kind = kHudsonPlayDrop;
	}
	else
	{
		return "this file doesn't use that word";
	}

	// The word and three numbers: across, up and down, and along
	if (words.size() != 4)
	{
		return "a step needs a place";
	}

	if (!HudsonPlay_ReadWholeNumber(words[1], &step.x) || !HudsonPlay_ReadWholeNumber(words[2], &step.y) || !HudsonPlay_ReadWholeNumber(words[3], &step.z))
	{
		return "that isn't a place";
	}

	if (steps->size() >= kHudsonPlayMostSteps)
	{
		return "too many steps";
	}

	steps->push_back(step);
	return std::string();
}

// Reads the whole file. A file with anything wrong in it is refused whole, because a level played with a step missing would only look like a level that can't be finished.
inline bool HudsonPlay_ReadSteps(std::string const &text, std::vector<HudsonPlayStep> *steps, std::string *whyNot)
{
	steps->clear(); // anything left from an earlier file goes

	if (text.size() > kHudsonPlayMostBytes)
	{
		*whyNot = "the file is too big";
		return false;
	}

	int lineNumber = 0;
	size_t from = 0;

	while (from < text.size())
	{
		size_t end = text.find('\n', from);

		if (end == std::string::npos)
		{
			end = text.size();
		}

		std::string line = text.substr(from, end - from);

		from = end + 1;
		lineNumber++;

		// A file saved on another kind of machine can end its lines differently, and spaces at the end of a line can't be seen
		while (!line.empty() && (line[line.size() - 1] == '\r' || line[line.size() - 1] == ' '))
		{
			line.erase(line.size() - 1);
		}

		std::string const problem = HudsonPlay_ReadLine(line, steps);

		if (!problem.empty())
		{
			*whyNot = "line " + std::to_string(lineNumber) + ": " + problem;
			steps->clear(); // nothing from a refused file is kept
			return false;
		}
	}

	if (steps->empty())
	{
		*whyNot = "there are no steps";
		return false;
	}

	return true;
}

// ---- Steering. The game measures angles so that 4096 is a whole turn.

static const int kHudsonPlayWholeTurn = 4096;

// How far he has to turn to face that way, from -2048 to 2048. A number above nothing means to his right, which is the way the game's right turn goes.
inline int HudsonPlay_AngleToTurn(int forwardX, int forwardZ, int wantedX, int wantedZ)
{
	// Facing nowhere, or wanting to go nowhere, there's nothing to turn towards
	if ((forwardX == 0 && forwardZ == 0) || (wantedX == 0 && wantedZ == 0))
	{
		return 0;
	}

	double const facing = atan2((double)forwardX, (double)forwardZ);
	double const wanted = atan2((double)wantedX, (double)wantedZ);
	double turn = (wanted - facing) * (double)kHudsonPlayWholeTurn / (2.0 * 3.14159265358979323846);

	// The short way round: three quarters of a turn to the right is a quarter of a turn to the left
	while (turn > kHudsonPlayWholeTurn / 2)
	{
		turn -= kHudsonPlayWholeTurn;
	}

	while (turn < -kHudsonPlayWholeTurn / 2)
	{
		turn += kHudsonPlayWholeTurn;
	}

	return (int)turn;
}

// What to give the game's turn control so that this frame turns him that far and no further. The control goes from -65536 to 65536. Full is
// fullTurnsPerSecond, in the game's angle units, and frameTime is how long a frame takes, where 65536 is a second. A frame that takes no time can't turn at all.
inline int HudsonPlay_TurnControl(int angle, int fullTurnsPerSecond, int frameTime)
{
	if (frameTime <= 0 || fullTurnsPerSecond <= 0)
	{
		return 0;
	}

	double const wanted = (double)angle * 65536.0 * 65536.0 / ((double)fullTurnsPerSecond * (double)frameTime);

	if (wanted > 65536.0)
	{
		return 65536;
	}

	if (wanted < -65536.0)
	{
		return -65536;
	}

	return (int)wanted;
}

// He only walks once he's roughly facing the way he's going, or he'd go round in circles at a corner
inline bool HudsonPlay_MayWalk(int angle)
{
	return angle > -512 && angle < 512;
}

// How hard he walks, as the game's control where 65536 is flat out. He runs at about 13 m/s and turns half a circle in a second, which on its own is a turn about 8 m across, far too wide
// for a stair or a doorway. So the further he has to turn, the slower he goes: flat out when nearly facing it, and a quarter of that when he's as far off as he may walk at all.
static const int kHudsonPlayFacingItWithin = 60;     // about 5 degrees
static const int kHudsonPlaySlowestWalk = 16384;

inline int HudsonPlay_WalkControl(int angle)
{
	int const off = angle < 0 ? -angle : angle;

	if (off <= kHudsonPlayFacingItWithin)
	{
		return 65536;
	}

	if (off >= 512)
	{
		return kHudsonPlaySlowestWalk;
	}

	return 65536 - (int)((double)(65536 - kHudsonPlaySlowestWalk) * (off - kHudsonPlayFacingItWithin) / (512 - kHudsonPlayFacingItWithin));
}

// Close enough to look along a gun and fire, or to have a switch in the middle of his view
inline bool HudsonPlay_AimedWell(int angle)
{
	return angle > -60 && angle < 60;
}

// A FIGHT HE CAN'T WIN FROM HERE MUSTN'T STOP HIM WALKING. Added 2026-11-09, from the owner watching a run: "I see the path, it works, he's
// just not following it". He stood still beside his way for minutes, because an alien he could see now and then, but not hurt, kept
// starting a fight: every time it came back into sight, it counted as a new fight, so the fight's own time limit never came, and between
// the moments he saw it, he was never turned back to his way long enough to walk.
// So: out of sight for less than kHudsonPlaySameFightWithin and back again is still the same fight, with the same clock. And firing at it
// for kHudsonPlayCantHurtItAfter without its health going down means he can't hit it from where he is: he leaves it alone and walks on.
// That isn't a cheat (nothing is done for him), so it isn't counted as one; the log says so each time.
static const unsigned long kHudsonPlaySameFightWithin = 3000;  // ms
static const unsigned long kHudsonPlayCantHurtItAfter = 8000;  // ms

// True if a threat seen again is the same fight as the one before: the same one, out of sight for less than the limit
inline bool HudsonPlay_IsTheSameFight(bool sameThreat, unsigned long outOfSightFor)
{
	return sameThreat && outOfSightFor < kHudsonPlaySameFightWithin;
}

// True if he's been fighting it long enough to know he can't hurt it from here: its health hasn't gone down at all since the fight began
inline bool HudsonPlay_CantHurtIt(unsigned long fightingFor, int healthAtStart, int healthNow)
{
	return fightingFor >= kHudsonPlayCantHurtItAfter && healthNow >= healthAtStart;
}

// FLARES (flares=yes; added 2026-11-20 by Melanikus (Frost), the owner, watching the lane probe: "throwing flares when aliens approach him"): one at an alien he can
// see within kHudsonPlayFlareWithin, once he faces it within kHudsonPlayFlareFacing (the game throws a flare along his view), and none again for kHudsonPlayFlareEvery.
// The game keeps 4 flares in the world at most (pmove.cpp's ThrowAFlare).
static const int kHudsonPlayFlareWithin = 8000;              // mm
static const int kHudsonPlayFlareFacing = 256;               // of a whole turn's 4096: 22.5 degrees either way
static const unsigned long kHudsonPlayFlareEvery = 6000;     // ms

inline bool HudsonPlay_FlareIsDue(int distance, bool seen, unsigned long sinceTheLast, bool thrownBefore)
{
	return seen && distance < kHudsonPlayFlareWithin && (!thrownBefore || sinceTheLast >= kHudsonPlayFlareEvery);
}

// angleToTurn as HudsonPlay_AngleToTurn gives it: how far he still has to turn to face it
inline bool HudsonPlay_FacesItToThrow(int angleToTurn)
{
	return angleToTurn <= kHudsonPlayFlareFacing && angleToTurn >= -kHudsonPlayFlareFacing;
}

// AIM AT THE NEAREST (aimnearest=yes; added 2026-11-20 by Melanikus (Frost), the owner: "I want hudson to run backwards pointing (but not firing) always at the nearest
// alien"): of the aliens within kHudsonPlayAimWithin, the nearest he can see, else the nearest. He keeps the one he has unless another is under
// kHudsonPlayAimSwitchShare of its distance, or he has lost sight of his and sees another, so his view doesn't flick between two. -1: none near, and he faces along his
// way as before. His view turns kHudsonPlayAimTurnsOneIn of the way to it each frame, as his view goes after his way, so a recording isn't jittery.
static const int kHudsonPlayAimWithin = 20000;            // mm
static const double kHudsonPlayAimSwitchShare = 0.75;
static const int kHudsonPlayAimTurnsOneIn = 4;

struct HudsonPlayAimCandidate
{
	int id;
	int distance;   // mm
	bool seen;
};

inline int HudsonPlay_PickTheNearest(std::vector<HudsonPlayAimCandidate> const &aliens, int current)
{
	int seenId = -1, seenDistance = kHudsonPlayAimWithin, anyId = -1, anyDistance = kHudsonPlayAimWithin;
	HudsonPlayAimCandidate const *kept = 0;

	for (size_t i = 0; i < aliens.size(); i++)
	{
		HudsonPlayAimCandidate const &a = aliens[i];

		if (a.distance >= kHudsonPlayAimWithin)
		{
			continue;
		}

		kept = a.id == current ? &a : kept;

		if (a.seen && a.distance < seenDistance)
		{
			seenId = a.id;
			seenDistance = a.distance;
		}

		if (a.distance < anyDistance)
		{
			anyId = a.id;
			anyDistance = a.distance;
		}
	}

	int const pick = seenId >= 0 ? seenId : anyId;
	int const pickDistance = seenId >= 0 ? seenDistance : anyDistance;
	bool const lostSightOfHis = kept && !kept->seen && seenId >= 0;

	if (kept && pick != current && !lostSightOfHis && !(pickDistance < kept->distance * kHudsonPlayAimSwitchShare))
	{
		return current;
	}

	return pick;
}

// HOLD HIS GROUND (holdground=yes; added 2026-11-20 by Melanikus (Frost), Bishop for the owner, the snowplow: an alien in Attack is held still by its own state, and he,
// unkillable, walked it 50 to 100 m down a corridor, twice in run 200126): while an alien within kHudsonPlayHoldWithin is ahead of him on his way (on his way's side of
// him), he doesn't walk on: under kHudsonPlayBackOffWithin he backs off from it, else he stands, as a clawed player would. At most kHudsonPlayHoldMost at a time, then he
// walks on for kHudsonPlayHoldRest before he holds again: a clawed player pushes past or dies, and the run mustn't stand still for good. The snowplow check
// (snowplow.cjs) needs 3 s of shoving, more than the rest.
static const int kHudsonPlayHoldWithin = 1500;              // mm
static const int kHudsonPlayBackOffWithin = 1000;           // mm
static const unsigned long kHudsonPlayHoldMost = 4000;      // ms
static const unsigned long kHudsonPlayHoldRest = 2000;      // ms

enum HudsonPlayHold
{
	kHudsonPlayWalkOn,
	kHudsonPlayStand,
	kHudsonPlayBackOff
};

// toAlien: from him to it, across; way: the way he's going (to the point he aims at along it); distance: to it
inline bool HudsonPlay_AlienAheadOnHisWay(int toAlienX, int toAlienZ, int wayX, int wayZ, int distance)
{
	return distance <= kHudsonPlayHoldWithin && (double)toAlienX * wayX + (double)toAlienZ * wayZ > 0.0;
}

// SHOOTING (shootwithin=, shootdelay=; added 2026-11-20 by Melanikus (Frost), the owner 21:33: "his crosshair on top of an alien for more tha 500ms", "attack things
// tha are really close ( less than say 2m) and attackihg him", "If they are not attacking hi keeps moving and observing"): he fires only at an alien nearer than
// within that is attacking him (in Attack), once his crosshair has been on it for more than delay; he holds his fire otherwise.
// distance: -1 for none; crosshairOnFor: how long his aim has been on it (HudsonPlay_AimedWell)
// A BURST THAT HOLDS (Frost 2026-11-20; the full run 2026-11-20-235943: facer01's burst began at 3.2 m and ended the next frame when its leap put his crosshair past
// the 5.3 degree window, "5.0 -> 5.0, alive", and it latched): a burst that has begun goes on while his crosshair is within 3 times HudsonPlay_AimedWell's window.
// And he's on a target when he's within that window, or when the miss across, at its distance, is under its half-width (a facehugger at 1.1 m is 10 degrees wide)
inline bool HudsonPlay_KeepsFiring(bool begun, int angle)
{
	return begun && angle > -180 && angle < 180;
}

inline bool HudsonPlay_OnTarget(int angle, int distance, int halfWidth)
{
	if (angle > -60 && angle < 60)
	{
		return true;
	}

	double const radians = (double)angle * 6.2831853 / 4096.0;

	return angle > -1024 && angle < 1024 && (double)distance * fabs(sin(radians)) <= (double)halfWidth;
}

inline bool HudsonPlay_ShootNow(int distance, bool attacking, unsigned long crosshairOnFor, int within, unsigned long delay)
{
	return distance >= 0 && distance < within && attacking && crosshairOnFor > delay;
}

// THE REAR GUARD (rearguard=MM; added 2026-11-20 by Melanikus (Frost), the owner 22:24 and 22:27: "sprint to the end and just look back (and turn fast, 3x faster
// than he does today) if an alien comes within 5 meters", "run backwards ... following his plotted course focusing the closest wall crawling item? Focus on the
// ceiling ones"). An alien's surface by its down's y, as the lane probe reads it: under -46000 a ceiling (0), over 46000 the floor (2), else a wall or slope (1)
inline int HudsonPlay_SurfaceRank(int downY)
{
	return downY < -46000 ? 0 : downY > 46000 ? 2 : 1;
}

struct HudsonPlayRearCandidate
{
	int id;
	int distance;   // mm
	int rank;       // HudsonPlay_SurfaceRank
};

// Of the aliens within the rear guard (the caller leaves out the rest): the nearest on a ceiling, else the nearest on a wall, else the nearest. -1: none
// FACEHUGGERS (the owner 22:51: "He must kill facehuggers"): a hugger within kHudsonPlayHuggerWithin comes first for the rear guard, before any alien (its rank
// is under the ceiling's), and he shoots it from there; an alien from shootwithin=. Beyond kHudsonPlayHuggerSeenWithin only one he can see counts. Frost 2026-11-20
static const int kHudsonPlayHuggerRank = -1;
static const int kHudsonPlayHuggerWithin = 10000;       // mm
static const int kHudsonPlayHuggerSeenWithin = 3000;    // mm

inline int HudsonPlay_ShootsFrom(bool hugger, int shootingWithin)
{
	return hugger && shootingWithin < kHudsonPlayHuggerWithin ? kHudsonPlayHuggerWithin : shootingWithin;
}

inline int HudsonPlay_PickTheRearTarget(std::vector<HudsonPlayRearCandidate> const &aliens)
{
	int best = -1, bestRank = 3, bestDistance = 0;

	for (size_t i = 0; i < aliens.size(); i++)
	{
		if (aliens[i].rank < bestRank || (aliens[i].rank == bestRank && aliens[i].distance < bestDistance))
		{
			best = aliens[i].id;
			bestRank = aliens[i].rank;
			bestDistance = aliens[i].distance;
		}
	}

	return best;
}

// The aim's turn control this frame: a quarter of the angle (kHudsonPlayAimTurnsOneIn), at most the game's full key turn; the rear guard's three times that
inline int HudsonPlay_AimTurnControl(int angle, bool rearGuard, int fullTurnsPerSecond, int frameTime)
{
	int const control = HudsonPlay_TurnControl(angle / kHudsonPlayAimTurnsOneIn, fullTurnsPerSecond, frameTime);

	return rearGuard ? 3 * control : control;
}

// Is a destroy (or kill) step's target gone? Not there, being taken away (please_destroy_me), or its health run out from more than none. A one-hit Static
// (integrity 0, Genshd1's SHED04 glass) has no health to begin with, so its health alone says nothing (the 22:14 run read all seven panes gone at once and
// never shot them). Added 2026-11-20 by Melanikus (Frost)
inline bool HudsonPlay_TargetIsGone(bool there, bool beingTakenAway, int healthAtStart, int healthNow)
{
	return !there || beingTakenAway || (healthAtStart > 0 && healthNow <= 0);
}

// THE LIFT RULE (added 2026-11-20 by Melanikus (Frost), the owner: "have him not teleport until he fails to get in the lift"): he's moved past a lift, to the landing
// of the stop he needed (a logged cheat), only when he has failed it: kHudsonPlayMostBoardings failed boardings, or kHudsonPlayMostAtALift at it without riding,
// or one ride without getting off at his stop
static const int kHudsonPlayMostBoardings = 3;
static const unsigned long kHudsonPlayMostAtALift = 60000;   // ms

inline bool HudsonPlay_RideFailed(int boardFailures, unsigned long atTheLiftFor, bool rodeOnce, bool gotOff)
{
	return boardFailures >= kHudsonPlayMostBoardings || (!rodeOnce && atTheLiftFor >= kHudsonPlayMostAtALift) || (rodeOnce && !gotOff);
}

// Where he stands on a lift's car, from its shape's top and bottom (y down): on a flat car, its top; on a car taller than kHudsonPlayCageTaller (a cage, Genshd1's
// liftcar01, 4.3 m), its floor, the shape's bottom. Added 2026-11-20 by Melanikus (Frost): his finder took liftcar01's roof for its floor, so its foot had no landing
static const int kHudsonPlayCageTaller = 2500;   // mm

inline int HudsonPlay_CarStandOffset(int shapeMinY, int shapeMaxY)
{
	return shapeMaxY - shapeMinY >= kHudsonPlayCageTaller ? shapeMaxY : shapeMinY;
}

// heldFor: how long this hold has lasted (0: he isn't holding); sinceTheLast: since the last hold ended
inline HudsonPlayHold HudsonPlay_Hold(bool aheadOnHisWay, int distance, unsigned long heldFor, unsigned long sinceTheLast)
{
	if (!aheadOnHisWay || heldFor >= kHudsonPlayHoldMost || (heldFor == 0 && sinceTheLast < kHudsonPlayHoldRest))
	{
		return kHudsonPlayWalkOn;
	}

	return distance < kHudsonPlayBackOffWithin ? kHudsonPlayBackOff : kHudsonPlayStand;
}

// Has he reached a point on his way? Across, he has to be close. Up and down it's more generous, because where the game says he is and where the grid says
// the floor is differ by about his height. But a point on the floor above isn't one he's reached, however close it is across.
inline bool HudsonPlay_ReachedPoint(int acrossX, int acrossZ, int upOrDown, int closeAcross, int closeUpOrDown)
{
	if (upOrDown > closeUpOrDown || upOrDown < -closeUpOrDown)
	{
		return false;
	}

	return (double)acrossX * acrossX + (double)acrossZ * acrossZ <= (double)closeAcross * closeAcross;
}

// ---- Following the way. The way is a line on the floor, and he follows the line. Each frame he finds where he is along it and aims a short way further on. He never aims back at a part
// he's passed, so if he overshoots a corner he just comes back to the line further along, and he doesn't turn round for it.

static const int kHudsonPlayAimAhead = 1000;              // mm along the way. Close, so that the corner he cuts is small: about a third of this on a square corner
static const int kHudsonPlayLookFurther = 3000;           // mm along the way: which way it's going, for slowing down and for where he looks
static const int kHudsonPlayPointReachedAcross = 400;     // mm: at the way's last point when he's this close across
static const int kHudsonPlayPointReachedUpOrDown = 450;   // mm: MAXIMUM_STEP_HEIGHT, the most the game lifts him up a step
static const int kHudsonPlayOnTheSameLevel = 900;         // mm: a part of the way further up or down than this, about two treads, is another floor, even if it's right beside him
static const size_t kHudsonPlayPartsToLookAhead = 12;     // parts of the way, about 12 m. Looking further could find the way coming back past him, and skip what's between

// How far he is across from the line between two points of the way, in mm. Past either end it's the distance to that end, and up and down isn't counted, because the band goes up the
// stairs with him. "alongOut" is how far along the line that nearest place is, from 0 at its start to 1 at its end.
inline int HudsonPlay_HowFarOffTheLine(int fromX, int fromZ, int toX, int toZ, int hereX, int hereZ, double *alongOut = 0)
{
	double const lineX = (double)toX - fromX, lineZ = (double)toZ - fromZ;
	double const hisX = (double)hereX - fromX, hisZ = (double)hereZ - fromZ;
	double const lengthSquared = lineX * lineX + lineZ * lineZ;
	double along = lengthSquared > 0.0 ? (hisX * lineX + hisZ * lineZ) / lengthSquared : 0.0;

	along = along < 0.0 ? 0.0 : (along > 1.0 ? 1.0 : along);

	if (alongOut)
	{
		*alongOut = along;
	}

	double const offX = hisX - along * lineX, offZ = hisZ - along * lineZ;

	return (int)sqrt(offX * offX + offZ * offZ);
}

struct HudsonPlayAim
{
	int x, y, z;            // the place on the way to make for
	int furtherX, furtherZ; // a place further on, which says which way the way is going
	size_t nextPoint;       // the number of the way's next point ahead of him, or how many points there are if he's at its end
	int off;                // how far across he is from the way, in mm
	bool onTheWay;          // false if no part of the way near him is on his level, so he's fallen off it or never got on
};

// The place on the way this far along from a part of it. It stops at the way's end. "part" is the number of the point the part ends at.
template <typename Point>
inline void HudsonPlay_PlaceFurtherAlong(std::vector<Point> const &way, size_t part, double along, double distance, double *x, double *y, double *z)
{
	double const fromX = way[part - 1].x, fromY = way[part - 1].y, fromZ = way[part - 1].z;
	double startX = fromX + along * (way[part].x - fromX), startY = fromY + along * (way[part].y - fromY), startZ = fromZ + along * (way[part].z - fromZ);
	double left = distance;

	for (size_t i = part; i < way.size(); i++)
	{
		double const lineX = way[i].x - startX, lineZ = way[i].z - startZ;
		double const length = sqrt(lineX * lineX + lineZ * lineZ);

		if (length >= left && length > 0.0)
		{
			double const share = left / length;

			*x = startX + share * lineX;
			*y = startY + share * (way[i].y - startY);
			*z = startZ + share * lineZ;
			return;
		}

		left -= length;
		startX = way[i].x;
		startY = way[i].y;
		startZ = way[i].z;
	}

	*x = startX;
	*y = startY;
	*z = startZ;
}

// Works out where he should make for. "from" is the number of the way's next point as he last knew it. It only ever looks forward from there. Point is anything with whole numbers x, y and z.
template <typename Point>
inline HudsonPlayAim HudsonPlay_AimAlongTheWay(std::vector<Point> const &way, size_t from, int hereX, int hereY, int hereZ)
{
	HudsonPlayAim aim = { hereX, hereY, hereZ, hereX, hereZ, way.size(), 0, true };

	if (way.empty() || from >= way.size())
	{
		return aim;
	}

	Point const &last = way[way.size() - 1];

	// The very end of the way: reached across and on its level, and only from the last part of it, or a way that comes back past its own end would stop early
	bool const atTheLastPart = from + kHudsonPlayPartsToLookAhead >= way.size();

	if (atTheLastPart && HudsonPlay_ReachedPoint(last.x - hereX, last.z - hereZ, last.y - hereY, kHudsonPlayPointReachedAcross, kHudsonPlayPointReachedUpOrDown))
	{
		return aim;
	}

	// A way of one point is just that point
	if (way.size() == 1)
	{
		aim.x = last.x;
		aim.y = last.y;
		aim.z = last.z;
		aim.furtherX = last.x;
		aim.furtherZ = last.z;
		aim.nextPoint = 0;
		aim.off = HudsonPlay_HowFarOffTheLine(last.x, last.z, last.x, last.z, hereX, hereZ);
		return aim;
	}

	size_t const first = from < 1 ? 1 : from;
	size_t best = first;
	double bestAlong = 0.0;
	int bestOff = -1;

	for (size_t i = first; i < way.size() && i < first + kHudsonPlayPartsToLookAhead; i++)
	{
		double along = 0.0;
		int const off = HudsonPlay_HowFarOffTheLine(way[i - 1].x, way[i - 1].z, way[i].x, way[i].z, hereX, hereZ, &along);
		double const upOrDown = way[i - 1].y + along * ((double)way[i].y - way[i - 1].y) - hereY;

		if (upOrDown <= kHudsonPlayOnTheSameLevel && upOrDown >= -kHudsonPlayOnTheSameLevel && (bestOff < 0 || off < bestOff))
		{
			best = i;
			bestAlong = along;
			bestOff = off;
		}
	}

	// Nothing near him is on his level: all he can do is make for the point he was making for
	if (bestOff < 0)
	{
		aim.onTheWay = false;
		aim.x = way[first].x;
		aim.y = way[first].y;
		aim.z = way[first].z;
		aim.furtherX = way[first].x;
		aim.furtherZ = way[first].z;
		aim.nextPoint = first;
		aim.off = HudsonPlay_HowFarOffTheLine(way[first - 1].x, way[first - 1].z, way[first].x, way[first].z, hereX, hereZ);
		return aim;
	}

	double x = 0.0, y = 0.0, z = 0.0;

	HudsonPlay_PlaceFurtherAlong(way, best, bestAlong, (double)kHudsonPlayAimAhead, &x, &y, &z);
	aim.x = (int)x;
	aim.y = (int)y;
	aim.z = (int)z;
	HudsonPlay_PlaceFurtherAlong(way, best, bestAlong, (double)kHudsonPlayLookFurther, &x, &y, &z);
	aim.furtherX = (int)x;
	aim.furtherZ = (int)z;
	aim.nextPoint = best;
	aim.off = bestOff;
	return aim;
}

// The two controls that move him in a direction whichever way he's facing. "ahead" and "side" are how much of that direction is in front of him and to his right, and they make a length of
// one. howHard is how fast, where 65536 is flat out. Forward and sidestep are the same speed for a marine, so this just shares howHard between them.
inline void HudsonPlay_WalkControls(double ahead, double side, int howHard, int *forwardControl, int *sideControl)
{
	*forwardControl = (int)(ahead * howHard);
	*sideControl = (int)(side * howHard);
}

// rearguard=: is an alien within the rear guard chasing him (the owner 22:36: "always facing aliens chasing him")? It is when it's after him (Approach, Hunt or
// Attack with him its target), or when it's nearer now than half a second ago by more than kHudsonPlayClosingBy. 0 for then means there's no reading yet.
// Frost 2026-11-20
static const int kHudsonPlayClosingBy = 250;   // mm in half a second: half a metre a second, more than an idle alien's shuffle

inline bool HudsonPlay_IsChasing(bool afterHim, int distanceNow, int distanceHalfASecondAgo)
{
	return afterHim || (distanceHalfASecondAgo > 0 && distanceHalfASecondAgo - distanceNow > kHudsonPlayClosingBy);
}

// Which way of his facing a second's move went (the owner 22:41: "he should move looking at aliens as fast as he can run forward"): forwards within 45 degrees of
// it, backwards over 135, else sideways. facingX and facingZ are his view's ahead (OrientMat mat31, mat33). Frost 2026-11-20
enum HudsonPlayMoveKind
{
	kHudsonPlayMovedForwards,
	kHudsonPlayMovedSideways,
	kHudsonPlayMovedBackwards
};

inline HudsonPlayMoveKind HudsonPlay_MoveKind(int facingX, int facingZ, double movedX, double movedZ)
{
	double const facing = sqrt((double)facingX * facingX + (double)facingZ * facingZ), moved = sqrt(movedX * movedX + movedZ * movedZ);

	if (facing <= 0.0 || moved <= 0.0)
	{
		return kHudsonPlayMovedSideways;
	}

	double const along = ((double)facingX * movedX + (double)facingZ * movedZ) / (facing * moved);

	return along >= 0.7071 ? kHudsonPlayMovedForwards : along <= -0.7071 ? kHudsonPlayMovedBackwards : kHudsonPlayMovedSideways;
}

// How much of the way to where he should be looking his view turns each frame. A small share, so that it swings round smoothly and the small corrections his feet make don't shake it.
static const int kHudsonPlayViewTurnsOneIn = 6;

// ---- Getting stuck. Every so often it's asked how far he's come. Someone who's trying to walk and hasn't got anywhere is stuck, and the
// answer is a list of things to try before he gives up. A fight isn't trying to walk, so it doesn't count.

// A STEP HE CAN'T GET ONTO (Frost 2026-11-20; shft21 in three full runs: his grid joins the floor to a stair's first step across its side, 383 mm up, and the game
// stops him there, Drake: poly 99 and the step sides): stuck kHudsonPlayStucksBeforeShutting times with the same way point ahead, a step up (more than 150 mm)
// within 2.4 m across, and that step is left out of his grid, a learned shut box like a shut door's, so the way goes round (by the stair's foot)
static const int kHudsonPlayStucksBeforeShutting = 3;

// THE JUMP STEP (Frost 2026-11-20; the medlab section run 2026-11-20-232611: his jump at full speed, 812 mm from the place, carried him over the 3.9 m dais and off its
// far side): how hard he walks at it, by how far across it is: flat out till 1.5 m, half speed nearer, nothing within 600 mm (over the place); and when he's up:
// on the floor, a step or less under its height, within 1.5 m across
// THE AIM THROUGH A STEP (Bishop 23:4x: only getting on, riding and getting off a lift pause the aim; the medlab section run's facehugger latch came in the jump step,
// which had none): a press or a destroy has to face its switch or its glass, so it has his view in the even seconds and the aim the odd ones (the full run's try 2
// stood at genlev04 240 s when the aim never let go of it); every other step leaves it to the aim. now in ms. Frost 2026-11-20
inline bool HudsonPlay_StepTakesHisView(bool pressOrDestroy, unsigned long now)
{
	return pressOrDestroy && (now / 1000) % 2 == 0;
}

inline int HudsonPlay_JumpWalk(int across)
{
	return across > 1500 ? 65536 : across > 600 ? 32768 : 0;
}

inline bool HudsonPlay_JumpedUp(int across, int below, bool onTheFloor)
{
	return onTheFloor && across <= 1500 && below <= 450;
}

inline bool HudsonPlay_ShutsTheStepAhead(int stucksAtThePoint, int across, int rise)
{
	return stucksAtThePoint >= kHudsonPlayStucksBeforeShutting && across <= 2400 && rise > 150;   // 2.4 m: 004422's second step was 2142 mm ahead
}

static const int kHudsonPlayStuckWithin = 800;          // mm: he hasn't really moved if he's still this close
static const unsigned int kHudsonPlayStuckAfter = 1000; // milliseconds of trying to walk. At his speed that's about 13 m, so he isn't just slow.

enum HudsonPlayRescue
{
	kHudsonPlayBackUp,
	kHudsonPlayFindTheWayAgain,
	kHudsonPlayStepLeft,
	kHudsonPlayStepRight,
	kHudsonPlayCrouch,
	kHudsonPlayJump,
	kHudsonPlayTeleport      // a cheat: everything else has failed, so he's moved along his way
};

inline HudsonPlayRescue HudsonPlay_NextRescue(int timesStuckHere)
{
	switch (timesStuckHere)
	{
		// The small things first, because they're usually enough and looking for a new way can send him a long way round. He gets a second go at them before he's moved, because being moved is a cheat.
		// Nudges left and right first, a back-up last (the owner 11-20: "he should just try to nudge himself left and right first before running back or move
		// backward less"; a first back-up dropped him into a lift's shaft twice). Changed 2026-11-20 by Melanikus (Frost)
		case 0: return kHudsonPlayStepLeft;
		case 1: return kHudsonPlayStepRight;
		case 2: return kHudsonPlayFindTheWayAgain;
		case 3: return kHudsonPlayCrouch;
		case 4: return kHudsonPlayJump;
		case 5: return kHudsonPlayBackUp;
		case 6: return kHudsonPlayStepRight;
		case 7: return kHudsonPlayStepLeft;
		case 8: return kHudsonPlayFindTheWayAgain;
		case 9: return kHudsonPlayBackUp;
		default: return kHudsonPlayTeleport;
	}
}

inline char const *HudsonPlay_RescueWords(HudsonPlayRescue rescue)
{
	switch (rescue)
	{
		case kHudsonPlayBackUp: return "he backs up";
		case kHudsonPlayFindTheWayAgain: return "he looks for the way again";
		case kHudsonPlayStepLeft: return "he steps to his left";
		case kHudsonPlayStepRight: return "he steps to his right";
		case kHudsonPlayCrouch: return "he crouches";
		case kHudsonPlayJump: return "he jumps";
		default: return "he's moved on along his way";
	}
}

// ---- Cheats, for when he can't get on. A short hop along his way goes at least this far, in mm, so that it gets him past what stopped him.

static const int kHudsonPlayHopAtLeast = 3000;

// Which point of the way a hop lands on: the first that far along from the one he's going to, or the last if the way's shorter than that. Only across is measured,
// because going up a stair isn't what gets him past something. The way must have a point in it, and "from" must be one of its points.
template <typename Point>
inline size_t HudsonPlay_PointToHopTo(std::vector<Point> const &way, size_t from, int atLeast)
{
	double gone = 0.0;

	for (size_t i = from; i + 1 < way.size(); i++)
	{
		double const x = (double)way[i + 1].x - way[i].x, z = (double)way[i + 1].z - way[i].z;

		gone += sqrt(x * x + z * z);

		if (gone >= (double)atLeast)
		{
			return i + 1;
		}
	}

	return way.size() - 1;
}

#endif
