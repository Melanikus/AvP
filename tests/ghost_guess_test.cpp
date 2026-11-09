// The test for 3dc/ghost_guess.h, and a baseline of how figures move when news is late (issue #88). Written 2026-11-08 by Melanikus for
// the AvP port.
//
// WHAT IT DOES: part 1 checks the guess's sums one at a time. Part 2 runs them the way a network game does. A sender moves 60 times a
// second and sends a message each time; a model of ENet's channel delivers them, late or lost as each case says; and a receiver draws the
// sender's figure 60 times a second, twice: as another player or an alien (guessed between messages), and as one of the other NPCs, whose
// shared message has no speed (not guessed). It measures how far
// each drawn figure is from where the real one is, how long it stands still, and how far it moves in one frame. The table it prints is the
// baseline #88's smoothing has to beat. Its checks are only what reading the code says must happen today.
//
// WHAT'S THE GAME'S, AND WHAT'S A MODEL:
// - the game's own: the guess's sums (ghost_guess.h, which the game calls), the approximate length of a speed (Approximate3dMagnitude,
//   3dc/maths.cpp), and the marine's speed (read from 3dc/avp/pmove.cpp).
// - models, said as such:
//   - the sender's rule for a full message or a small one (as AddNetMsg_PlayerState decides, pldnet.cpp; a SOURCE row checks its numbers
//     are still these);
//   - ENet's reliable, in-order channel, and its resends after a round trip plus four times its variation (ENet 1.3.18 protocol.c);
//   - the game moving a figure in open space, with nothing to bump into.
//
// tests/build.ps1 compiles this with the game's maths files and the game's settings, and the program gives back a non-zero result if any
// check failed. It uses no network, no files but the source it reads, and none of the rest of the game.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "ghost_guess.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// One frame at 60 a second, in the game's frame time (65536 to a second) and in milliseconds
static const int kFrameTime = ONE_FIXED / 60;
static const double kFrameMs = 1000.0 / 60.0;

static VECTORCH Vector(int x, int y, int z)
{
	VECTORCH v = { x, y, z };
	return v;
}

// ---- Part 1: the sums, one at a time

static void TheSums()
{
	// A message takes the sender's time since the last one off the clock
	{
		int timer = 5000, timerLast = 3000;
		unsigned int lastSender = 1000;
		GhostGuess_Message(&timer, &timerLast, &lastSender, false, 1600);
		Check(timer == 4400 && timerLast == 0 && lastSender == 1600, "a message takes the sender's time since the last one off the clock");
	}

	{
		int timer = 5000, timerLast = 3000;
		unsigned int lastSender = 1000;
		GhostGuess_Message(&timer, &timerLast, &lastSender, true, 1600);
		Check(timer == -ONE_FIXED - 600 && timerLast == 0, "a change of speed starts the clock again, from a second before nothing");
	}

	{
		int timer = 5000, timerLast = 3000;
		unsigned int lastSender = 2000;
		GhostGuess_Message(&timer, &timerLast, &lastSender, false, 1500);
		Check(timer == -ONE_FIXED && lastSender == 1500, "a sender's time that goes backwards starts the clock again");
	}

	// A frame moves the clock on and gives the speed for that frame
	{
		int timer = 0, timerLast = 0;
		bool stopped = true;
		VECTORCH const speed = GhostGuess_FrameSpeed(&timer, &timerLast, Vector(6000, 0, -3000), kFrameTime, &stopped);
		Check(!stopped && speed.vx == 6000 && speed.vz == -3000 && timer == kFrameTime && timerLast == kFrameTime, "a whole frame of guess: the whole speed");
	}

	{
		int timer = -ONE_FIXED, timerLast = 0;
		bool stopped = true;
		VECTORCH const speed = GhostGuess_FrameSpeed(&timer, &timerLast, Vector(6000, 0, 0), kFrameTime, &stopped);
		Check(!stopped && speed.vx == 0 && timer == 0, "a clock started again from below nothing guesses nothing this frame");
	}

	{
		int timer = kGhostGuessLongest - 300, timerLast = kGhostGuessLongest - 300;
		bool stopped = true;
		VECTORCH const speed = GhostGuess_FrameSpeed(&timer, &timerLast, Vector(6000, 0, 0), kFrameTime, &stopped);
		Check(timer == kGhostGuessLongest && speed.vx == WideMulNarrowDiv(6000, 300, kFrameTime), "the guess runs out partway through a frame: only that part of the speed");

		VECTORCH const after = GhostGuess_FrameSpeed(&timer, &timerLast, Vector(6000, 0, 0), kFrameTime, &stopped);
		Check(after.vx == 0 && timer == kGhostGuessLongest, "and after half a second, none: the figure stands still");
	}

	{
		int timer = 9000, timerLast = 4000;
		bool stopped = false;
		VECTORCH const speed = GhostGuess_FrameSpeed(&timer, &timerLast, Vector(0, 0, 0), kFrameTime, &stopped);
		Check(stopped && timer == 0 && timerLast == 0 && speed.vx == 0, "a figure with no speed stops its guess, and starts from nothing");
	}

	Check(kGhostGuessLongest == ONE_FIXED / 2, "the guess goes on for half a second at most");
}

// ---- Part 2: a sender, a channel and a receiver

// The marine's speed, from the game's movement code: what it asks for when running
static int marineSpeed = 15000;

// The paths the sender takes: straight on, or straight on and then a quarter turn
enum Path { kStraight, kTurn };

static const int kSenderFrames = 180;   // three seconds
static const int kTurnFrame = 90;

static VECTORCH SenderSpeed(Path path, int frame)
{
	if (path == kTurn && frame >= kTurnFrame)
	{
		return Vector(0, 0, marineSpeed);
	}

	return Vector(marineSpeed, 0, 0);
}

// How the network treats the messages in one case
struct NetCase
{
	const char *name;
	Path path;
	double delayMs;        // one way
	double jitterMs;       // each message's delay moves by up to this, either way
	double varianceMs;     // what ENet thinks the round trip varies by, for its resends
	int lostFrom, lostTo;  // the sender's frames whose message is lost on its first try, or -1
	double stallFrom, stallTo; // nothing gets through in this time, and what would have waits for its end, or 0 and 0
	bool inOrder;          // ENet's reliable channel, as the game uses. false: an unsequenced one, which #88 shouldn't use
};

static const NetCase kCases[] = {
	{ "on time, 75 ms",            kStraight, 75, 0,  10, -1, -1, 0, 0, true },
	{ "jitter 30 ms",              kStraight, 75, 30, 10, -1, -1, 0, 0, true },
	{ "one lost",                  kStraight, 75, 0,  10, 60, 60, 0, 0, true },
	{ "five lost",                 kStraight, 75, 0,  10, 60, 64, 0, 0, true },
	{ "0.7 s stall",               kStraight, 75, 0,  10, -1, -1, 1000, 1700, true },
	{ "turn, on time",             kTurn,     75, 0,  10, -1, -1, 0, 0, true },
	{ "out of order (unsequenced)", kStraight, 75, 30, 10, -1, -1, 0, 0, false },
};

// What the sender sends each frame, as one message (one packet)
struct Sent
{
	double sentMs;
	double arrivesMs;      // when it's handed to the game
	bool full;             // a full player state, with place and speed; otherwise a small one, which only turns the figure
	VECTORCH place;        // where the sender is
	VECTORCH speed;        // as the message carries it: in tenths of a metre a second
};

static unsigned int randomState = 1;

static double RandomBetween(double low, double high)
{
	randomState = randomState * 1103515245u + 12345u;
	return low + (high - low) * ((randomState >> 8) & 0xFFFF) / 65535.0;
}

static double Distance(const VECTORCH &a, const VECTORCH &b)
{
	double const x = (double)a.vx - b.vx, y = (double)a.vy - b.vy, z = (double)a.vz - b.vz;
	return sqrt(x * x + y * y + z * z);
}

// The game moves a figure by its speed times the frame's time, in its dynamics. Here in open space, with nothing to bump into.
static void Move(VECTORCH *place, const VECTORCH &speed)
{
	place->vx += (int)(((long long)speed.vx * kFrameTime) >> 16);
	place->vy += (int)(((long long)speed.vy * kFrameTime) >> 16);
	place->vz += (int)(((long long)speed.vz * kFrameTime) >> 16);
}

struct Measures
{
	double worstMm, meanMm, longestHoldMs, biggestStepMm;
	int backwards;
};

struct Result
{
	Measures player, npc;
};

static Result RunCase(const NetCase &netCase)
{
	randomState = 7;

	// The sender: where it is each frame, and what it sends
	std::vector<VECTORCH> trueAt(kSenderFrames + 1);
	std::vector<Sent> sent;
	VECTORCH place = Vector(0, 0, 0);
	VECTORCH lastFullSpeed = Vector(0, 0, 0);
	unsigned int counter = 0, lastFull = 0;
	double const resendAfter = 2 * netCase.delayMs + 4 * netCase.varianceMs;

	for (int frame = 0; frame <= kSenderFrames; frame++)
	{
		VECTORCH speed = SenderSpeed(netCase.path, frame);
		trueAt[frame] = place;

		// The rule for a full message, as AddNetMsg_PlayerState has it: a quarter of a second since the last, or a change of speed of more
		// than 100. (It also sends one for a jump or a change of gun, which these paths don't have.)
		counter += kFrameTime;
		VECTORCH change = lastFullSpeed;
		SubVector(&speed, &change);
		bool const full = frame == 0 || counter - lastFull > ONE_FIXED / 4 || Approximate3dMagnitude(&change) > 100;

		if (full)
		{
			lastFull = counter;
			lastFullSpeed = speed;
		}

		Sent s;
		s.sentMs = frame * kFrameMs;
		s.full = full;
		s.place = place;
		s.speed = Vector(speed.vx / 100, speed.vy / 100, speed.vz / 100);

		double arrives = s.sentMs + netCase.delayMs + RandomBetween(-netCase.jitterMs, netCase.jitterMs);

		if (frame >= netCase.lostFrom && frame <= netCase.lostTo)
		{
			arrives = s.sentMs + resendAfter + netCase.delayMs;   // lost, and sent again after ENet's wait
		}

		if (arrives >= netCase.stallFrom && arrives < netCase.stallTo)
		{
			arrives = netCase.stallTo;
		}

		// In order: nothing is handed over before what was sent before it
		if (netCase.inOrder && !sent.empty() && arrives < sent.back().arrivesMs)
		{
			arrives = sent.back().arrivesMs;
		}

		s.arrivesMs = arrives;
		sent.push_back(s);
		Move(&place, speed);
	}

	// The receiver: the player's figure, guessed, and the NPC's, not
	VECTORCH player = Vector(0, 0, 0), npc = Vector(0, 0, 0), playerSpeed = Vector(0, 0, 0);
	int timer = 0, timerLast = 0, senderTime = 0;
	unsigned int lastSenderTime = 0;
	bool seen = false;

	std::vector<bool> handed(sent.size(), false);
	Result result = {};
	double sumPlayer = 0, sumNpc = 0;
	int counted = 0;
	double holdPlayer = 0, holdNpc = 0;
	VECTORCH lastPlayer = player, lastNpc = npc, lastTrue = trueAt[0];

	for (int frame = 0; frame <= kSenderFrames; frame++)
	{
		// A third of a frame in, so that no message is handed over exactly as a frame starts, where rounding could put it in either
		double const now = frame * kFrameMs + kFrameMs / 3;

		// This frame's messages, in the order they're handed over
		std::vector<size_t> order;

		for (size_t i = 0; i < sent.size(); i++)
		{
			if (!handed[i] && sent[i].arrivesMs <= now)
			{
				order.push_back(i);
			}
		}

		std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) { return sent[a].arrivesMs < sent[b].arrivesMs; });

		for (size_t i : order)
		{
			handed[i] = true;
			Sent const &s = sent[i];

			// The frame time comes first in a sender's batch, then its state (pldnet.cpp)
			senderTime += kFrameTime;

			if (s.full)
			{
				VECTORCH speed = Vector(s.speed.vx * 100, s.speed.vy * 100, s.speed.vz * 100);
				VECTORCH change = playerSpeed;
				SubVector(&speed, &change);

				GhostGuess_Message(&timer, &timerLast, &lastSenderTime, Approximate3dMagnitude(&change) > 1000, senderTime);
				playerSpeed = speed;
				player = s.place;
				seen = true;
			}

			npc = s.place;
		}

		// The guess for this frame, then the move
		bool stopped = false;
		VECTORCH const speed = GhostGuess_FrameSpeed(&timer, &timerLast, playerSpeed, kFrameTime, &stopped);
		Move(&player, speed);

		// Where the real one is now: between the sender's frames on either side
		int const before = (std::min)(kSenderFrames - 1, (int)(now / kFrameMs));
		double const part = now / kFrameMs - before;
		VECTORCH trueNow;
		trueNow.vx = (int)(trueAt[before].vx + part * (trueAt[before + 1].vx - trueAt[before].vx));
		trueNow.vy = (int)(trueAt[before].vy + part * (trueAt[before + 1].vy - trueAt[before].vy));
		trueNow.vz = (int)(trueAt[before].vz + part * (trueAt[before + 1].vz - trueAt[before].vz));

		// Measured from 0.3 s, once the first news has come, until the end
		if (seen && now >= 300)
		{
			double const ePlayer = Distance(player, trueNow), eNpc = Distance(npc, trueNow);
			result.player.worstMm = (std::max)(result.player.worstMm, ePlayer);
			result.npc.worstMm = (std::max)(result.npc.worstMm, eNpc);
			sumPlayer += ePlayer;
			sumNpc += eNpc;
			counted++;

			bool const trueMoved = Distance(trueNow, lastTrue) > 1;
			holdPlayer = (trueMoved && Distance(player, lastPlayer) < 1) ? holdPlayer + kFrameMs : 0;
			holdNpc = (trueMoved && Distance(npc, lastNpc) < 1) ? holdNpc + kFrameMs : 0;
			result.player.longestHoldMs = (std::max)(result.player.longestHoldMs, holdPlayer);
			result.npc.longestHoldMs = (std::max)(result.npc.longestHoldMs, holdNpc);
			result.player.biggestStepMm = (std::max)(result.player.biggestStepMm, Distance(player, lastPlayer));
			result.npc.biggestStepMm = (std::max)(result.npc.biggestStepMm, Distance(npc, lastNpc));

			// A step against the way the real one is going
			VECTORCH const way = SenderSpeed(netCase.path, before);
			double const playerAlong = (double)(player.vx - lastPlayer.vx) * way.vx + (double)(player.vz - lastPlayer.vz) * way.vz;
			double const npcAlong = (double)(npc.vx - lastNpc.vx) * way.vx + (double)(npc.vz - lastNpc.vz) * way.vz;
			result.player.backwards += playerAlong < 0 ? 1 : 0;
			result.npc.backwards += npcAlong < 0 ? 1 : 0;
		}

		lastPlayer = player;
		lastNpc = npc;
		lastTrue = trueNow;
	}

	result.player.meanMm = counted ? sumPlayer / counted : 0;
	result.npc.meanMm = counted ? sumNpc / counted : 0;
	return result;
}

static void PrintRow(const char *name, const char *figure, const Measures &m)
{
	printf("  %-28s %-7s %9.0f %9.0f %10.0f %10.0f %6d\n", name, figure, m.worstMm, m.meanMm, m.longestHoldMs, m.biggestStepMm, m.backwards);
}

static void TheBaseline()
{
	double const step = marineSpeed * (double)kFrameTime / ONE_FIXED;   // how far the real one goes in a frame
	double const lateBy = marineSpeed * 0.075;                           // how far it goes in 75 ms

	printf("\n  The baseline (a marine at %d mm/s; distances in mm, times in ms):\n", marineSpeed);
	printf("  %-28s %-7s %9s %9s %10s %10s %6s\n", "case", "figure", "worst", "average", "longest", "biggest", "back");
	printf("  %-28s %-7s %9s %9s %10s %10s %6s\n", "", "", "distance", "distance", "standstill", "step", "steps");

	std::vector<Result> results;

	for (const NetCase &netCase : kCases)
	{
		results.push_back(RunCase(netCase));
		PrintRow(netCase.name, "player", results.back().player);
		PrintRow("", "NPC", results.back().npc);
	}

	printf("\n");

	// What reading the code says must happen today. Each has two frames' room either way.
	Result const &onTime = results[0];
	Check(fabs(onTime.npc.worstMm - lateBy) <= 2 * step && onTime.npc.longestHoldMs == 0,
		"on time, an NPC is drawn where the real one was 75 ms ago, and moves every frame");
	Check(fabs(onTime.player.worstMm - lateBy) <= 2 * step && onTime.player.longestHoldMs == 0,
		"on time, a player's figure is guessed to where it was 75 ms ago, and moves every frame");

	double const resendAfter = 2 * 75 + 4 * 10;
	Result const &oneLost = results[2];
	Check(fabs(oneLost.npc.longestHoldMs - resendAfter) <= 2 * kFrameMs, "one lost message holds an NPC still for ENet's wait before it sends again");
	Check(oneLost.player.longestHoldMs < oneLost.npc.longestHoldMs, "and a player's figure is guessed through it");

	Result const &stall = results[4];
	Check(stall.npc.longestHoldMs >= 700 - 2 * kFrameMs, "a 0.7 s stall holds an NPC still for all of it");
	Check(stall.player.longestHoldMs >= 200 - 2 * kFrameMs && stall.player.longestHoldMs <= 500 + 2 * kFrameMs,
		"a player's figure is guessed for at most half a second of it, then stands still: stuck, then a jump");
	Check(stall.npc.biggestStepMm >= marineSpeed * 0.7 - 2 * step, "and when the stall ends, the NPC jumps the whole way at once");

	Check(results[1].npc.backwards == 0 && stall.npc.backwards == 0, "in order, an NPC is never drawn going back the way it came");
	Check(results[6].npc.backwards > 0, "with an unsequenced channel it is, which is why #88 should keep its messages in order");
}

// ---- The game's own numbers, read from its source

static std::string ReadSourceFile(char const *name)
{
	// /FC in this test's settings makes __FILE__ the whole path of this file, so the repository's top is found from it
	std::string path = __FILE__;
	std::string::size_type const tests = path.rfind("tests");

	if (tests == std::string::npos)
	{
		return "";
	}

	std::ifstream file((path.substr(0, tests) + name).c_str(), std::ios::binary);
	std::stringstream all;
	all << file.rdbuf();

	// A file has one kind of line ending or the other depending on how it was cloned, and these rows shouldn't care
	std::string text = all.str();
	text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
	return text;
}

static bool Has(std::string const &text, std::string const &words)
{
	return text.find(words) != std::string::npos;
}

static bool ComesBefore(std::string const &text, std::string const &first, std::string const &second)
{
	std::string::size_type const one = text.find(first);
	std::string::size_type const two = text.find(second);

	return one != std::string::npos && two != std::string::npos && one < two;
}

static void TheGameUsesIt()
{
	std::string const movement = ReadSourceFile("3dc/avp/pmove.cpp");
	std::string::size_type const at = movement.find("#define MARINE_MOVESCALE ");
	Check(at != std::string::npos, "SOURCE: the marine's speed is read from the movement code");

	if (at != std::string::npos)
	{
		marineSpeed = atoi(movement.c_str() + at + strlen("#define MARINE_MOVESCALE "));
	}

	Check(marineSpeed > 0 && marineSpeed / 100 < 512, "SOURCE: and it's one a message can carry: in tenths of a metre a second, in 10 bits");

	std::string const ghosts = ReadSourceFile("3dc/avp/win95/pldghost.cpp");
	Check(Has(ghosts, "dynPtr->LinVelocity = GhostGuess_FrameSpeed(&ghostData->extrapTimer, &ghostData->extrapTimerLast, ghostData->velocity, NormalFrameTime, &stopped);"),
		"SOURCE: each frame, the game moves a player's figure at the speed ghost_guess.h gives");

	std::string const net = ReadSourceFile("3dc/avp/win95/pldnet.cpp");
	Check(Has(net, "GhostGuess_Message(&ghostData->extrapTimer, &ghostData->extrapTimerLast, &ghostData->lastTimeRead, Approximate3dMagnitude(&diff) > 1000, playerTimer);"),
		"SOURCE: a player's full message changes the clock through ghost_guess.h, with a change of speed of more than 1000");
	Check(Has(net, "GhostGuess_Message(&ghostData->extrapTimer, &ghostData->extrapTimerLast, &ghostData->lastTimeRead, diff > 1500 || -diff > 1500, playerTimer);"),
		"SOURCE: so does the message about an alien the host's AI runs, with its own rule for a change");

	// The model's numbers are still the game's
	Check(Has(net, "static const int kSendsEachSecond = 60;"), "SOURCE: the game sends 60 times a second, as the model does");
	Check(Has(net, "TimeCounterForExtrapolation - TimeOfLastPlayerState > ONE_FIXED / 4)") && Has(net, "if (Approximate3dMagnitude(&diff) > 100)\n\t\t\t{\n\t\t\t\t//the player's velocity has changed , so need a full update."),
		"SOURCE: a full message goes after a quarter of a second, or a change of speed of more than 100, as the model has it");
	Check(ComesBefore(net, "\tAddNetMsg_FrameTimer();", "\t\t\t\tAddNetMsg_PlayerState(sbPtr);"), "SOURCE: a sender's frame time comes before its state, as the model has it");
	Check(Has(net, "messagePtr->velocity_x = dynPtr->LinVelocity.vx / 100;") && Has(net, "velocity.vx = messagePtr->velocity_x * 100;"), "SOURCE: and a speed goes in tenths of a metre a second, as the model sends it");
}

int main()
{
	TheSums();
	TheGameUsesIt();
	TheBaseline();

	if (checksFailed)
	{
		printf("FAILED: %d checks\n", checksFailed);
		return 1;
	}

	printf("all ghost guess checks passed\n");
	return 0;
}
