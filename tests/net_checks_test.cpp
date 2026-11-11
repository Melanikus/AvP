// Modified 2026-11-06 by Melanikus for the AvP port:
//   the checks added since: names, chat lines, a client's budget, typed addresses, who has asked lately, and a sound's address
// Modified 2026-11-08 by Melanikus for the AvP port:
//   how long ago a joiner heard about a lift car, by its own clock, and that bh_platform_lift.cpp counts it that way (#90)
// Modified 2026-11-10 by Melanikus for the AvP port:
//   which addresses are the player's home network, at the edges of every range (#188)
//   who waits at a lift's far end, against every stock lift, and that a deathmatch lift still calls at 1999's 5 m (#189)
// The test of the checks on what arrives from other players' machines. Written 2026-11-04 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// A damage message carries an ammunition number that another player's machine wrote. The game uses it as a place in its table of ammunition, so it has to be
// checked first. This tries the check on every number the message can hold, and on the numbers either side of each end of the table. It also tests the limit on
// how much a flood of bad messages can write to the log.
//
// WHAT IT DOESN'T TEST
//
// pldnet.cpp is far too tied to the rest of the game to compile here. So this shows the check gives the right answers, and nothing here shows that the three
// handlers ask it. For those, there's only the game building, and reading the code.

#include <stdio.h>
#include <string.h>

#include <fstream>
#include <sstream>
#include <string>

#include "3dc.h"
#include "equipment.h"
#include "net_checks.h"
#include "avp/sound_address.h"

// The field the number travels in, in all three damage messages (pldnet.h).
struct AmmunitionField
{
	short ammo_id : 11;
};

// Every real number has to fit that field, or the sender's own numbers would arrive changed.
static_assert(AMMO_NONE >= -1024 && AMMO_FIREDAMAGE_POSTMAX <= 1023, "an ammunition number no longer fits the field in a damage message");

static int checksFailed = 0;

static void Check(bool passed, char const * what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what);

	if (!passed)
	{
		checksFailed++;
	}
}

// A file of the game's source, by its place from the top of the repository, with Windows' line ends taken out. /FC in this test's settings
// makes __FILE__ the whole path of this file, so the others can be found from it.
static std::string ReadSourceFile(const char *name)
{
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

// With friendly fire off, whose weapons can't hurt you
static void TestOtherSpeciesAsFriends()
{
	puts("friendly fire off, and the other species' weapons");

	Check(NetOtherSpeciesCanBeAFriend(true, false), "in a co-op game, a marine and a Predator can be on one side");
	Check(!NetOtherSpeciesCanBeAFriend(true, true), "in the mode everyone is the same species, so those weapons can only be an NPC's");
	Check(!NetOtherSpeciesCanBeAFriend(false, false), "in other games, the other species is the other side");
	Check(!NetOtherSpeciesCanBeAFriend(false, true), "and single player has nobody to be friends with");
}

// What happens to a hit on a breakable track object, in each kind of game
static void TestBreakableHits()
{
	puts("hitting a breakable track object");

	Check(NetBreakableHitGoes(false, true, false, false) == kNetHitCounts, "in single player, it counts");
	Check(NetBreakableHitGoes(true, false, false, false) == kNetHitIsIgnored, "a network game outside the mode ignores it on the host");
	Check(NetBreakableHitGoes(true, false, true, false) == kNetHitIsIgnored, "and on another machine");
	Check(NetBreakableHitGoes(true, false, true, true) == kNetHitIsIgnored, "even if a message says it came from the host");
	Check(NetBreakableHitGoes(true, true, false, false) == kNetHitCounts, "in the mode, it counts on the host");
	Check(NetBreakableHitGoes(true, true, true, false) == kNetHitGoesToTheHost, "and another machine tells the host");
	Check(NetBreakableHitGoes(true, true, true, true) == kNetHitCounts, "and counts when the host says so");
}

// How near a platform lift's far end someone has to be, for a network game to send it to them
static void TestLiftSync()
{
	puts("a joiner's lift car following the host's");

	Check(NetLiftEndIsKnown(0) && NetLiftEndIsKnown(1), "a lift's two ends are known");
	Check(!NetLiftEndIsKnown(2) && !NetLiftEndIsKnown(-1) && !NetLiftEndIsKnown(255), "and nothing else is");

	Check(NetLiftHeightIsOnItsTrack(-3000, -3000, 2000, 1000) && NetLiftHeightIsOnItsTrack(2000, -3000, 2000, 1000), "both ends of its track are on it");
	Check(NetLiftHeightIsOnItsTrack(-4000, -3000, 2000, 1000) && NetLiftHeightIsOnItsTrack(3000, -3000, 2000, 1000), "and so is up to the margin past either");
	Check(!NetLiftHeightIsOnItsTrack(-4001, -3000, 2000, 1000) && !NetLiftHeightIsOnItsTrack(3001, -3000, 2000, 1000), "but no further");
	Check(!NetLiftHeightIsOnItsTrack(2147483647, -3000, 2000, 1000) && !NetLiftHeightIsOnItsTrack(-2147483647 - 1, -3000, 2000, 1000), "and the largest and smallest heights a message can hold are off it");

	Check(NetLiftHostHeightNow(1000, false, -3000, 5000, 400) == 1000, "a car at rest stays where it was said to be");
	Check(NetLiftHostHeightNow(1000, true, -3000, 5000, 400) == -1000, "one going up has gone 5 m a second towards its top");
	Check(NetLiftHostHeightNow(-1000, true, 2000, 5000, 400) == 1000, "and one going down towards its bottom");
	Check(NetLiftHostHeightNow(1000, true, -3000, 5000, 60000) == -3000, "and it stops at its end");
	Check(NetLiftHostHeightNow(1000, true, -3000, 5000, -50) == 1000, "a time before the message counts as none");

	// How long ago the host's message was heard, by this machine's own clock in ms (#90)
	Check(NetLiftMsSince(1500, 1000) == 500 && NetLiftMsSince(1000, 1000) == 0, "the time since the host spoke is the clock now take away the clock then");
	Check(NetLiftMsSince(200, 0xFFFFFF00u) == 456, "and it stays right when the clock goes back to 0, every 49.7 days");
	Check(NetLiftMsSince(1000 + 100000, 1000) == kNetLiftLongestMsSince && kNetLiftLongestMsSince == 60000, "and it's never counted as more than a minute");

	// The pair test's car (#90): the host's car going up at 5 m a second, heard about at 1000 ms. The joiner's frames went on for 634 ms,
	// then it stood still for 476 ms. Its frame times, added up, came to about 634 + 33 = 667 ms, because the frame after a pause is
	// counted as an ordinary one, so its car was put 3.3 m up while the host's was 5.5 m up. By the clock it's 1110 ms, and 5.5 m.
	int const heardAt = 1000;
	int const now = heardAt + 634 + 476;

	Check(NetLiftHostHeightNow(0, true, -10000, 5000, NetLiftMsSince(now, heardAt)) == -5550, "after a pause, the host's car is still put where it is, 5.5 m up");
	Check(NetLiftHostHeightNow(0, true, -10000, 5000, 634 + 33) == -3335, "where adding up frame times left it 2.2 m short");

	// The game's own lines: the age is taken by the clock, from when the host's message came, and the car's step in a frame stays in game time
	std::string const lift = ReadSourceFile("3dc/avp/bh_platform_lift.cpp");
	std::string const liftHeader = ReadSourceFile("3dc/avp/bh_platform_lift.h");

	Check(!lift.empty() && !liftHeader.empty(), "SOURCE: the lift's files can be read");
	Check(Has(lift, "\tlift->syncHeardAt = timeGetTime();\n\tlift->syncHeard = TRUE;"), "SOURCE: a host's message about a car notes the clock when it came");
	Check(Has(lift, "\tint const msSince = NetLiftMsSince(timeGetTime(), lift->syncHeardAt);\n"), "SOURCE: and the car follows it by the clock since then");
	Check(!Has(lift, "syncAge") && !Has(liftHeader, "syncAge"), "SOURCE: and not by frame times added up, as before");
	Check(Has(lift, "int const mostThisFrame = MUL_FIXED(PLATFORMLIFT_SPEED + (PLATFORMLIFT_SPEED >> 2), NormalFrameTime);"),
		"SOURCE: but how far the car may move in a frame stays in game time, which a rider on it moves by");

	Check(NetLiftFollowStep(0, 2000, 100, false, 450) == 2000, "with nobody on it and more than a step away, the car goes straight there");
	Check(NetLiftFollowStep(0, 2000, 100, true, 450) == 100, "with someone on it, it travels, however far");
	Check(NetLiftFollowStep(0, -2000, 100, true, 450) == -100, "in either direction");
	Check(NetLiftFollowStep(0, 300, 100, false, 450) == 100, "within a step, it travels too");
	Check(NetLiftFollowStep(0, 80, 100, true, 450) == 80, "and arrives without going past");
}

// Every platform lift in the 23 single-player levels that have one, from the game's own level files (#189). Each is
// { level, name, top, bottom, minX, maxX, minZ, maxZ, radius }, in the game's mm, and a bigger y is lower down:
//   top, bottom: the two ends of the car's travel;
//   minX, maxX, minZ, maxZ: how far the car's shape reaches from its middle, seen from above;
//   radius: the size of the car's whole shape, which the call used until #189.
// Made by tools/levels/lift-table.js, which says where each number is in a level file, run in the game's avp_rifs folder:
//   node <the repository>/tools/levels/lift-table.js area52 derelict derelict_a escape escape_p fall fall_m furyall furyall_a genshd1 invasion
//     invasion_a invasion_p odobenus stat101 sulaco sulaco_a sulaco_p temple temple_m temple_p vaults vaults_m
struct StockLift
{
	char const *level;
	char const *name;
	int top, bottom, minX, maxX, minZ, maxZ, radius;
};

static StockLift const kStockLifts[] =
{
	{ "area52", "CANSLIFT", 9028, 23077, -2338, 2338, -2338, 2337, 8462 },
	{ "area52", "skblift", -15888, -5310, -2338, 2338, -1558, 1558, 8779 }, // starts at the bottom
	{ "area52", "LIB-LIFT", 10367, 16588, -1125, 1992, -3116, 3117, 3724 },
	{ "area52", "LIFT1", -9301, -889, -3116, 3117, -3428, 3429, 5338 }, // starts at the bottom
	{ "derelict", "lift-SKB", -18269, 1211, -4675, 4674, -2338, 2338, 20302 },
	{ "derelict", "lft-VIEWC", -18269, 5506, -2337, 2338, -4675, 4675, 25163 },
	{ "derelict", "lift-SKA", -18270, -13193, -4675, 4675, -2337, 2338, 5744 },
	{ "derelict", "lft-VIEWA", -13205, 5503, -2337, 2338, -4675, 4675, 19479 },
	{ "derelict", "lft-COMM", -6581, 1621, -1948, 1948, -3896, 3896, 9038 },
	{ "derelict", "lft-JOCKEY", 1818, 30921, -1948, 1948, -1948, 1948, 36932 },
	{ "derelict", "liftcar01", 29650, 45229, -1948, 1948, -1948, 1948, 3612 },
	{ "derelict_a", "lift-SKB", -18269, 1211, -4675, 4674, -2338, 2338, 20302 },
	{ "derelict_a", "lft-VIEWC", -18269, 5506, -2337, 2338, -4675, 4675, 25163 },
	{ "derelict_a", "lift-SKA", -18270, -13193, -4675, 4675, -2337, 2338, 5744 },
	{ "derelict_a", "lft-VIEWA", -13205, 5503, -2337, 2338, -4675, 4675, 19479 },
	{ "derelict_a", "lft-COMM", -6581, 1621, -1948, 1948, -3896, 3896, 9038 },
	{ "escape", "lift01", -31160, -19479, -1948, 1948, -1948, 1948, 3612 }, // starts at the bottom
	{ "escape_p", "lift01", -31160, -19479, -1948, 1948, -1948, 1948, 3612 }, // starts at the bottom
	{ "fall", "lift", -2076, 25226, -3896, 3897, -3896, 3897, 6639 },
	{ "fall_m", "lift", -2076, 25226, -3896, 3897, -3896, 3897, 6639 },
	{ "furyall", "LEADLIFT", 10895, 38179, -1949, 1948, -2598, 5195, 5586 },
	{ "furyall", "sqlift", 6719, 37133, -1948, 1949, -2728, 2727, 3375 }, // starts at the bottom
	{ "furyall", "mainlift", -15168, 35583, -4540, 4540, -3027, 3027, 6938 },
	{ "furyall", "plift", -20918, -9237, -3897, 3896, -3897, 3897, 11532 }, // starts at the bottom
	{ "furyall", "lift", 8479, 20183, -3897, 3897, -3897, 3897, 5542 }, // starts at the bottom
	{ "furyall_a", "LEADLIFT", 10895, 38179, -1949, 1948, -2598, 5195, 5586 },
	{ "furyall_a", "sqlift", 6719, 37133, -1948, 1949, -2728, 2727, 3375 }, // starts at the bottom
	{ "furyall_a", "mainlift", -15180, 35505, -4540, 4540, -3027, 3027, 6938 },
	{ "furyall_a", "plift", -20918, -9237, -3897, 3896, -3897, 3897, 11532 }, // starts at the bottom
	{ "genshd1", "liftplt", -13272, 2325, -3904, 3905, -3896, 3897, 5530 }, // starts at the bottom
	{ "genshd1", "SHDLFT", -3642, 1735, -1949, 1948, -1948, 1949, 2763 }, // starts at the bottom
	{ "genshd1", "liftcar01", -24, 15590, -1948, 1949, -1949, 1948, 3614 }, // starts at the bottom
	{ "invasion", "Platlift", -16344, -8556, -2050, 2136, -2211, 2259, 3139 }, // starts at the bottom
	{ "invasion", "toplftsh01", -27351, -16198, -1866, 1835, -2079, 2080, 2666 }, // starts at the bottom
	{ "invasion", "Platlift2", -10938, -3152, -1949, 1948, -2145, 2144, 11378 },
	{ "invasion", "Platlift3", -1801, 11038, -1949, 1948, -1951, 1950, 18715 }, // starts at the bottom
	{ "invasion", "3rdLift01", -1872, 11381, -1948, 1948, -3898, 3898, 15989 },
	{ "invasion_a", "Platlift", -16344, -8556, -2050, 2136, -2211, 2259, 3139 }, // starts at the bottom
	{ "invasion_a", "Platlift2", -11014, -3122, -1949, 1948, -2145, 2144, 11378 }, // starts at the bottom
	{ "invasion_a", "Platlift3", -1819, 10986, -1949, 1948, -1951, 1950, 18715 }, // starts at the bottom
	{ "invasion_a", "3rdLift01", -1872, 11381, -1948, 1948, -3898, 3898, 15989 },
	{ "invasion_p", "Platlift", -16344, -8556, -2050, 2136, -2211, 2259, 3139 }, // starts at the bottom
	{ "invasion_p", "toplftsh01", -27378, -16203, -1866, 1835, -2079, 2080, 2666 },
	{ "invasion_p", "Platlift2", -11014, -3122, -1949, 1948, -2145, 2144, 11378 }, // starts at the bottom
	{ "invasion_p", "Platlift3", -1819, 10986, -1949, 1948, -1951, 1950, 18715 }, // starts at the bottom
	{ "invasion_p", "3rdLift01", -1872, 11381, -1948, 1948, -3898, 3898, 15989 },
	{ "odobenus", "DoorLift01", -2212, 5456, -5845, 5845, -1949, 1948, 8201 }, // starts at the bottom
	{ "odobenus", "Lift", 2113, 13833, -1786, 1787, -1786, 1787, 1860 }, // starts at the bottom
	{ "odobenus", "lIFT2", 5919, 13426, -1754, 1753, -1754, 1753, 10233 }, // starts at the bottom
	{ "stat101", "liftcarP02", -115621, -68891, -4092, 4091, -3613, 3613, 6953 }, // starts at the bottom
	{ "sulaco", "finallift", -4870, 10728, -1948, 1949, -5845, 5845, 6164 },
	{ "sulaco", "platlift04", -5587, 6110, -1949, 1948, -3447, 450, 4061 }, // starts at the bottom
	{ "sulaco", "platlift02", -3621, 8050, -3448, 449, -1948, 1948, 4061 },
	{ "sulaco", "platlift", -3604, 4162, -3447, 449, -1949, 1948, 4061 }, // starts at the bottom
	{ "sulaco_a", "finallift", -4870, 10728, -1948, 1949, -5845, 5845, 6164 },
	{ "sulaco_a", "platlift04", -5587, 6110, -1949, 1948, -3447, 450, 4061 }, // starts at the bottom
	{ "sulaco_a", "platlift02", -3621, 8050, -3448, 449, -1948, 1948, 4061 },
	{ "sulaco_a", "platlift", -3604, 4162, -3447, 449, -1949, 1948, 4061 }, // starts at the bottom
	{ "sulaco_p", "finallift", -4837, 6591, -1948, 1949, -5845, 5845, 6164 }, // starts at the bottom
	{ "sulaco_p", "platlift04", -5587, 6110, -1949, 1948, -3447, 450, 4061 }, // starts at the bottom
	{ "sulaco_p", "platlift02", -3652, 8019, -3448, 449, -1948, 1948, 4061 },
	{ "sulaco_p", "platlift", -3604, 4162, -3447, 449, -1949, 1948, 4061 }, // starts at the bottom
	{ "temple", "genlift01", 51483, 60085, -1948, 1949, -1949, 1948, 2839 }, // starts at the bottom
	{ "temple", "genlift", 31679, 51181, -1948, 1949, -1948, 1948, 2941 },
	{ "temple", "biglift", 59600, 94280, -4286, 4287, -1948, 1949, 4744 }, // starts at the bottom
	{ "temple_m", "genlift", 31692, 51175, -1948, 1949, -1948, 1948, 2941 },
	{ "temple_m", "biglift", 59600, 94280, -4286, 4287, -1948, 1949, 4744 }, // starts at the bottom
	{ "temple_p", "genlift01", 51483, 60085, -1948, 1949, -1949, 1948, 2839 }, // starts at the bottom
	{ "temple_p", "genlift", 31679, 51172, -1948, 1949, -1948, 1948, 2941 },
	{ "temple_p", "biglift", 59600, 94280, -4286, 4287, -1948, 1949, 4744 }, // starts at the bottom
	{ "temple_p", "powlift", 56987, 89230, -6624, 6625, -1948, 1948, 7658 },
	{ "vaults", "LIFT01", 1718, 13713, -2338, 2338, -2338, 2337, 4237 }, // starts at the bottom
	{ "vaults", "ALIENS-LFT", 11907, 18450, -2052, 1064, -2805, 2805, 5860 },
	{ "vaults_m", "ALIENS-LFT", 11907, 18450, -2052, 1064, -2805, 2805, 5860 },
};

// In the mode that plays as single player: does someone at x, y, z wait at the far end of a car at rest at nearEnd, whose middle
// is at carX, carZ? The game's own distance only counts outside the mode, so it's given as one that would say no there.
static bool WaitsInTheMode(int x, int y, int z, int carX, int carZ, int nearEnd, int farEnd, int footprint)
{
	return NetLiftIsWaitingAtTheFarEnd(999999, x, y, z, carX, carZ, nearEnd, farEnd, footprint, true);
}

static void TestLiftCallReach()
{
	puts("calling a platform lift");

	// Outside the mode it's 1999's rule, by the game's own distance, and nothing else is looked at
	Check(kNetLiftCallReach == 5000, "the call reaches 5 m, as the game always said");
	Check(NetLiftIsWaitingAtTheFarEnd(4999, 0, 0, 0, 0, 0, 0, 30000, 2755, false), "outside the mode, nearer than 5 m to the far end's middle waits there");
	Check(!NetLiftIsWaitingAtTheFarEnd(5000, 0, 30000, 0, 0, 0, 0, 30000, 2755, false), "and 5 m away doesn't, even standing in the shaft at the far end");
	Check(NetLiftIsWaitingAtTheFarEnd(4999, 99999, -99999, 99999, 0, 0, 0, 0, 0, false), "the place, the ends and the car's size don't matter there: a deathmatch lift is as it was");

	// The car seen from above
	Check(NetLiftFootprintRadius(-1948, 1948, -1948, 1948) == 2755, "a square car 3.9 m across reaches 2.755 m to its corners, rounded up");
	Check(NetLiftFootprintRadius(-4675, 4674, -2338, 2338) == 5228, "a long one, such as Derelict's lift-SKB, reaches 5.228 m");
	Check(NetLiftFootprintRadius(-3447, 449, -1949, 1948) == 3960, "a car off its middle counts by its further side");
	Check(NetLiftFootprintRadius(-1948, 1948, -3447, 450) == 3960, "along z too");
	Check(NetLiftFootprintRadius(-3, 3, -4, 4) == 5, "a corner a whole number of mm away is exactly that, not one more");
	Check(NetLiftFootprintRadius(0, 0, 0, 0) == 0, "a car with no size has none");
	Check(NetLiftFootprintRadius(-2147483647 - 1, 2147483647, -2147483647 - 1, 2147483647) == 141422,
		"the biggest numbers a level's file can hold count as 100 m each way, and don't overflow");

	// In the mode. A made-up lift: a 30 m shaft from -3000 down to 27000, a 3.9 m car with its middle at 1000, 2000. So someone
	// waits at the far end within 5 m of its height, and within 2755 + 5000 = 7755 mm of the car's middle, seen from above.
	int const top = -3000, bottom = 27000, carX = 1000, carZ = 2000, footprint = 2755;

	Check(!WaitsInTheMode(carX, top - 1000, carZ, carX, carZ, top, bottom, footprint),
		"#189: standing on the car at the top isn't waiting at the bottom, so the car doesn't go away from him");
	Check(!NetLiftIsWaitingAtTheFarEnd(0, carX, top - 1000, carZ, carX, carZ, top, bottom, footprint, true), "and the game's own distance doesn't count in the mode");
	Check(WaitsInTheMode(carX + 7000, bottom - 1000, carZ, carX, carZ, top, bottom, footprint), "standing beside the bottom, 7 m from the car's middle, calls it down to him");
	Check(WaitsInTheMode(carX, bottom, carZ, carX, carZ, top, bottom, footprint), "and so does standing in the shaft at the bottom");
	Check(WaitsInTheMode(carX + 7754, bottom, carZ, carX, carZ, top, bottom, footprint), "the reach is 5 m past the car's edge");
	Check(!WaitsInTheMode(carX + 7755, bottom, carZ, carX, carZ, top, bottom, footprint), "and no further");
	Check(!WaitsInTheMode(carX, bottom, carZ - 7755, carX, carZ, top, bottom, footprint), "the same along z");
	Check(WaitsInTheMode(carX + 5483, bottom, carZ + 5483, carX, carZ, top, bottom, footprint), "on the slant, it's the distance that counts: 7.754 m");
	Check(!WaitsInTheMode(carX + 5484, bottom, carZ + 5484, carX, carZ, top, bottom, footprint), "and 7.756 m is too far");
	Check(WaitsInTheMode(carX, bottom + 4999, carZ, carX, carZ, top, bottom, footprint), "up to 5 m below the far end's height counts");
	Check(!WaitsInTheMode(carX, bottom + 5000, carZ, carX, carZ, top, bottom, footprint), "but not 5 m");
	Check(WaitsInTheMode(carX, bottom - 4999, carZ, carX, carZ, top, bottom, footprint), "and up to 5 m above it");
	Check(!WaitsInTheMode(carX, bottom - 5000, carZ, carX, carZ, top, bottom, footprint), "but not 5 m, as in a room over the far end");
	Check(WaitsInTheMode(carX, top - 1000, carZ, carX, carZ, bottom, top, footprint), "with the car at the bottom, standing at the top calls it up");
	Check(!WaitsInTheMode(carX, bottom - 1000, carZ, carX, carZ, bottom, top, footprint), "and standing on it at the bottom doesn't");

	// The half of the shaft, on its own: a 6 m shaft from 0 down to 6000, so its middle is 3000, and every height here is within 5 m
	// of the far end's
	Check(WaitsInTheMode(0, 3001, 0, 0, 0, 0, 6000, footprint), "just past the middle of a short shaft, on the far end's half, counts");
	Check(!WaitsInTheMode(0, 3000, 0, 0, 0, 0, 6000, footprint), "the middle itself doesn't, as in single player");
	Check(!WaitsInTheMode(0, 2999, 0, 0, 0, 0, 6000, footprint), "nor the car's own half, though it's within 5 m of the far end's height");
	Check(!WaitsInTheMode(0, 4000, 0, 0, 0, 5000, 5000, footprint), "a car with nowhere to go has no far end, even for someone 1 m over it");

	// Numbers from a hostile level or message: nothing overflows, and nothing far away counts
	int const most = 2147483647, least = -2147483647 - 1;
	Check(!WaitsInTheMode(most, bottom, least, carX, carZ, top, bottom, footprint), "someone as far away as a number can say doesn't count");
	Check(!WaitsInTheMode(least, bottom, most, most, least, top, bottom, footprint), "nor with the car as far the other way");
	Check(WaitsInTheMode(0, most, 0, 0, 0, least, most, most), "ends at the very top and bottom of the numbers don't overflow: at the far end, he waits");
	Check(!WaitsInTheMode(0, least, 0, 0, 0, least, most, most), "and at the car's own end he doesn't");
	Check(WaitsInTheMode(carX + 146421, bottom, carZ, carX, carZ, top, bottom, most), "a car bigger than any real one counts as the biggest the footprint can be");
	Check(!WaitsInTheMode(carX + 146422, bottom, carZ, carX, carZ, top, bottom, most), "and no bigger");
	Check(WaitsInTheMode(carX + 4999, bottom, carZ, carX, carZ, top, bottom, -5), "a size below nothing counts as nothing, not as less");
	Check(!WaitsInTheMode(carX + 5000, bottom, carZ, carX, carZ, top, bottom, -5), "so the reach is 5 m from the middle then");

	// Every stock lift, with its car at either end. Round its own end, from 2 m above it to 2 m below and out to 3 m past the reach,
	// straight out along x and z and on the slant, nobody counts as waiting at the far end. Beside the far end, 4 m past the car's
	// edge, and in the shaft at the far end, someone does.
	size_t const lifts = sizeof(kStockLifts) / sizeof(kStockLifts[0]);
	int calledFromItsOwnEnd = 0, notCalledFromItsFarEnd = 0, oldRuleCalledFromItsOwnEnd = 0, widest = 0;

	for (size_t i = 0; i < lifts; i++)
	{
		StockLift const &lift = kStockLifts[i];
		int const reach = NetLiftFootprintRadius(lift.minX, lift.maxX, lift.minZ, lift.maxZ);
		bool wrong = false;

		widest = reach > widest ? reach : widest;

		// The old rule: someone over the car's middle at its own end is the shaft's length from the far end, and the old reach was
		// 5 m plus the radius of the whole shape
		if (lift.bottom - lift.top < kNetLiftCallReach + lift.radius)
		{
			oldRuleCalledFromItsOwnEnd++;
		}

		for (int carIsAtTheTop = 0; carIsAtTheTop < 2; carIsAtTheTop++)
		{
			int const nearEnd = carIsAtTheTop ? lift.top : lift.bottom;
			int const farEnd = carIsAtTheTop ? lift.bottom : lift.top;

			for (int up = -2000; up <= 2000; up += 500)
			{
				for (int out = 0; out <= reach + kNetLiftCallReach + 3000; out += 250)
				{
					int const slant = out * 7 / 10;

					if (WaitsInTheMode(out, nearEnd + up, 0, 0, 0, nearEnd, farEnd, reach) || WaitsInTheMode(0, nearEnd + up, -out, 0, 0, nearEnd, farEnd, reach) ||
						WaitsInTheMode(-slant, nearEnd + up, slant, 0, 0, nearEnd, farEnd, reach))
					{
						calledFromItsOwnEnd++;
						wrong = true;
					}
				}
			}

			if (!WaitsInTheMode(reach + 4000, farEnd, 0, 0, 0, nearEnd, farEnd, reach) || !WaitsInTheMode(0, farEnd, 0, 0, 0, nearEnd, farEnd, reach))
			{
				notCalledFromItsFarEnd++;
				wrong = true;
			}
		}

		if (wrong)
		{
			printf("  (%s's %s is called wrongly)\n", lift.level, lift.name);
		}

		if (lift.top >= lift.bottom)
		{
			printf("  (%s's %s has no travel)\n", lift.level, lift.name);
		}
	}

	Check(lifts == 74, "the table holds all 74 platform lifts of the single-player levels");
	Check(oldRuleCalledFromItsOwnEnd == 36, "the old rule called 36 of them from the end the car was at: the bug is in the table");
	Check(calledFromItsOwnEnd == 0, "now no stock lift is called from round the end its car is at, whichever end that is");
	Check(notCalledFromItsFarEnd == 0, "and every one is called by someone beside its far end, or standing in its shaft there");
	Check(widest == 6906, "the widest stock car, Temple's powlift, reaches 6.906 m from its middle: the size the old test meant");

	// The game's own lines: the rule is asked in both places, the shape's whole radius is gone, and the host's player calls a car
	// in the mode as single player does, while single player's own call is as it was
	std::string const lift = ReadSourceFile("3dc/avp/bh_platform_lift.cpp");

	Check(!lift.empty(), "SOURCE: the lift's file can be read");
	Check(Has(lift, "/* remote player */\n\t\t\t\tLOCALASSERT(sbPtr->DynPtr);\n\t\t\t\tif(WaitingAtTheFarEnd(&(sbPtr->DynPtr->Position),&(otherTerminal),nearEndHeight,footprint)) return 1;"),
		"SOURCE: a joiner waits at the far end by the rule");
	Check(Has(lift, "/* local player */\n\t\t\tLOCALASSERT(sbPtr->DynPtr);\n\t\t\tif(WaitingAtTheFarEnd(&(sbPtr->DynPtr->Position),&(otherTerminal),nearEndHeight,footprint)) return 1;"),
		"SOURCE: and so does the host's own player");
	Check(Has(lift, "return NetLiftIsWaitingAtTheFarEnd(VectorDistance(where, farEnd), where->vx, where->vy, where->vz, farEnd->vx, farEnd->vz, nearEndHeight, farEnd->vy,"),
		"SOURCE: with the game's own distance, for outside the mode");
	Check(!Has(lift, "shaperadius"), "SOURCE: the radius of the car's whole shape calls nothing");
	Check(Has(lift, "NetLiftFootprintRadius(liftShape->shapeminx, liftShape->shapemaxx, liftShape->shapeminz, liftShape->shapemaxz)"), "SOURCE: the car's footprint does");
	Check(Has(lift, "if(ActivatedByPlayer(dynPtr) || HostSeesTheCarFromTheOtherHalf(sbPtr))"), "SOURCE: the host's player calls a car he sees from the other half");
	Check(Has(lift, "if (!SameAsSinglePlayer() || !sbPtr->SBdptr || !Player || !Player->ObStrategyBlock || !Player->ObStrategyBlock->DynPtr)\n\t{\n\t\treturn 0;\n\t}\n\n\treturn PlayerIsNearOtherTerminal(sbPtr);"),
		"SOURCE: only in the mode, and only when it's drawn on his screen");
	Check(Has(lift, "if(ActivatedByPlayer(dynPtr) || PlayerIsNearOtherTerminal(sbPtr))"), "SOURCE: single player's own call is as it was");
}

// Whether the host has heard from everyone else on the loading screen
static void TestEveryoneElseIsReady()
{
	puts("waiting for everyone on the loading screen");

	bool const noneHere[3] = { false, false, false };
	bool const twoHere[3] = { true, true, false };
	bool const noneReady[3] = { false, false, false };
	bool const firstReady[3] = { true, false, false };
	bool const secondReady[3] = { false, true, false };

	Check(NetEveryoneElseIsReady(noneHere, noneReady, 3, 0), "with nobody else in the game, there's nobody to wait for");
	Check(!NetEveryoneElseIsReady(twoHere, noneReady, 3, 0), "another player who hasn't said so is waited for");
	Check(NetEveryoneElseIsReady(twoHere, secondReady, 3, 0), "and not once they have");
	Check(NetEveryoneElseIsReady(twoHere, firstReady, 3, 1), "your own place doesn't count");
	Check(!NetEveryoneElseIsReady(twoHere, firstReady, 3, 0), "someone ready in your own place doesn't speak for the others");
	Check(NetEveryoneElseIsReady(twoHere, secondReady, 3, 0), "an empty place isn't waited for");
}

// A press message from another machine names something to press. The numbers here stand in for the game's kinds of object.
static void TestPressMessages()
{
	puts("what a press message can press");

	enum { kPlainSwitch = 7, kLinkSwitch = 11, kSomethingToPickUp = 3, kLift = 4, kLight = 5 };

	Check(NetPressMessageCanPress(kPlainSwitch, kPlainSwitch, kLinkSwitch), "a plain switch can be pressed");
	Check(NetPressMessageCanPress(kLinkSwitch, kPlainSwitch, kLinkSwitch), "a switch with a link group can be pressed");
	Check(!NetPressMessageCanPress(kSomethingToPickUp, kPlainSwitch, kLinkSwitch), "something to pick up can't");
	Check(!NetPressMessageCanPress(kLift, kPlainSwitch, kLinkSwitch), "a lift can't");
	Check(!NetPressMessageCanPress(kLight, kPlainSwitch, kLinkSwitch), "a light can't");
}

static bool FitsAmmunitionTable(int number)
{
	return NetNumberFitsTable(number, MAX_NO_OF_AMMO_TEMPLATES);
}

static void TestAmmunitionNumbers()
{
	puts("ammunition numbers");

	Check(!FitsAmmunitionTable(-1024), "the lowest number a message can hold is refused");
	Check(!FitsAmmunitionTable(AMMO_NONE), "the number for no ammunition is refused");
	Check(FitsAmmunitionTable(0), "the first place in the table is accepted");
	Check(FitsAmmunitionTable(MAX_NO_OF_AMMO_TEMPLATES - 1), "the last place in the table is accepted");
	Check(!FitsAmmunitionTable(MAX_NO_OF_AMMO_TEMPLATES), "one past the last place is refused");
	Check(!FitsAmmunitionTable(AMMO_FLECHETTE_POSTMAX), "a number after the table is refused");
	Check(!FitsAmmunitionTable(AMMO_FALLING_POSTMAX), "another number after the table is refused");
	Check(!FitsAmmunitionTable(AMMO_FIREDAMAGE_POSTMAX), "the last number after the table is refused");
	Check(!FitsAmmunitionTable(1023), "the highest number a message can hold is refused");

	// Every number the field can hold, as it comes out of the field, so that the sign is the one the game sees.
	int accepted = 0;
	int lowest = 0;
	int highest = 0;

	for (int bits = 0; bits < 2048; bits++)
	{
		AmmunitionField field;
		field.ammo_id = (short)bits;

		int const number = field.ammo_id;

		lowest = (number < lowest) ? number : lowest;
		highest = (number > highest) ? number : highest;

		if (FitsAmmunitionTable(number))
		{
			accepted++;
		}
	}

	Check(lowest == -1024 && highest == 1023, "the field really holds -1024 to 1023");
	Check(accepted == MAX_NO_OF_AMMO_TEMPLATES, "of all those numbers, exactly the table's places are accepted");

	Check(!NetNumberFitsTable(0, 0), "nothing fits a table with no places");
}

// Trouble that goes on all level is in the log once when it starts, and once with how many there were when the level is over (NetLevelCount)
static void TestTroubleAllLevel()
{
	NetLevelCount level = {};

	Check(NetLevelCount_One(level), "the first in a level is the one the log hears of");
	Check(!NetLevelCount_One(level) && !NetLevelCount_One(level), "and not the ones after it");
	Check(NetLevelCount_LevelIsOver(level) == 3, "the level's end says how many there were");
	Check(NetLevelCount_LevelIsOver(level) == 0, "and starts the count again, so a level with none says none");
	Check(NetLevelCount_One(level), "and the first in the next level is heard of again");
}

static void TestTheLogCantBeFlooded()
{
	puts("a flood of bad messages");

	unsigned long long count = 0;
	bool first[17];

	for (int drop = 1; drop <= 16; drop++)
	{
		first[drop] = LogThisDrop(count);
	}

	Check(first[1] && first[2] && first[3] && first[4] && first[5], "the first five are logged");
	Check(!first[6] && !first[7], "the sixth and seventh aren't");
	Check(first[8], "the eighth is");
	Check(!first[9] && !first[15], "the ones between aren't");
	Check(first[16], "the sixteenth is");

	count = 0;
	int logged = 0;

	for (int drop = 0; drop < 10000000; drop++)
	{
		if (LogThisDrop(count))
		{
			logged++;
		}
	}

	Check(logged > 5 && logged < 30, "ten million of them add only a few lines");
}

// A small made-up table, so that these say something about the walking and nothing about the game's own messages. tests/net_start_test.cpp checks the game's table against its source.
enum { kPartFixed, kPartEmpty, kPartHosts, kPartVaries, kPartBig, kPartKinds };

static const NetPartRule kTestRules[kPartKinds] =
{
	{ kPartFixed, 3, false, false },
	{ kPartEmpty, 0, false, false },
	{ kPartHosts, 2, false, true },
	{ kPartVaries, 1, true, false },
	{ kPartBig, 60000, false, false }
};

// A part whose length varies says so in its first byte. 0 means it doesn't fit, as for the real ones.
static size_t TestLength(unsigned char, const unsigned char *part, size_t)
{
	return part[0];
}

static NetPartsAnswer Walk(const unsigned char *parts, size_t size, bool fromTheHost = false)
{
	return NetCheckParts(parts, size, kTestRules, kPartKinds, fromTheHost, TestLength);
}

static void TestMessageParts()
{
	puts("the parts of a message");

	unsigned char const fixedOnly[] = { kPartFixed, 1, 2, 3 };

	Check(Walk(fixedOnly, 0) == kNetPartsGood, "nothing at all is fine: a message can have no parts");
	Check(Walk(fixedOnly, sizeof(fixedOnly)) == kNetPartsGood, "a part with all its bytes is good");
	Check(Walk(fixedOnly, sizeof(fixedOnly) - 1) == kNetPartsTooShort, "one byte short is too short");
	Check(Walk(fixedOnly, 1) == kNetPartsTooShort, "and so is the first byte alone");

	unsigned char const twoParts[] = { kPartFixed, 1, 2, 3, kPartEmpty, kPartFixed, 4, 5, 6 };

	Check(Walk(twoParts, sizeof(twoParts)) == kNetPartsGood, "several parts are walked one after another");
	Check(Walk(twoParts, sizeof(twoParts) - 1) == kNetPartsTooShort, "and a short one at the end is found");

	unsigned char const unknownNext[] = { kPartEmpty, kPartKinds };
	unsigned char const unknownFar[] = { 255 };

	Check(Walk(unknownNext, sizeof(unknownNext)) == kNetPartsUnknownKind, "the first kind past the table is unknown");
	Check(Walk(unknownFar, sizeof(unknownFar)) == kNetPartsUnknownKind, "and so is the last a byte can say");

	unsigned char const lastKind[] = { kPartBig };

	Check(Walk(lastKind, sizeof(lastKind)) == kNetPartsTooShort, "the last kind in the table is known");

	unsigned char const fromHost[] = { kPartHosts, 1, 2 };

	Check(Walk(fromHost, sizeof(fromHost), true) == kNetPartsGood, "only the host may say this, and the host can");
	Check(Walk(fromHost, sizeof(fromHost), false) == kNetPartsOnlyFromTheHost, "and nobody else");

	unsigned char const hiddenAfterOthers[] = { kPartFixed, 1, 2, 3, kPartHosts, 1, 2 };

	Check(Walk(hiddenAfterOthers, sizeof(hiddenAfterOthers)) == kNetPartsOnlyFromTheHost, "and it's no better hidden behind something else");

	unsigned char const variesFits[] = { kPartVaries, 3, 9, 9 };

	Check(Walk(variesFits, sizeof(variesFits)) == kNetPartsGood, "a part whose length varies is as long as it says");
	Check(Walk(variesFits, sizeof(variesFits) - 1) == kNetPartsTooShort, "and has to fit");

	unsigned char const variesThenMore[] = { kPartVaries, 2, 9, kPartEmpty };

	Check(Walk(variesThenMore, sizeof(variesThenMore)) == kNetPartsGood, "and the next part starts where it ends");

	unsigned char const variesNothing[] = { kPartVaries, 0 };
	unsigned char const variesMissing[] = { kPartVaries };
	unsigned char const variesFar[] = { kPartVaries, 200, 1 };

	unsigned char const variesThenWhatLooksFine[] = { kPartVaries, 0, 1, 2, 3 };

	Check(Walk(variesNothing, sizeof(variesNothing)) == kNetPartsTooShort, "saying it doesn't fit is taken at its word");
	Check(Walk(variesThenWhatLooksFine, sizeof(variesThenWhatLooksFine)) == kNetPartsTooShort, "even when what's after it would make a good message on its own");
	Check(Walk(variesMissing, sizeof(variesMissing)) == kNetPartsTooShort, "with no bytes at all it isn't even asked");
	Check(Walk(variesFar, sizeof(variesFar)) == kNetPartsTooShort, "and it can't claim more than there is");
}

static void TestChatLines()
{
	puts("chat lines");

	unsigned char line[kNetMostChatLetters + 8];

	memset(line, 'x', sizeof(line));
	line[3] = 0;
	Check(NetChatPartLength(line, sizeof(line)) == 4, "a chat line is as long as its first zero");

	line[1] = 0;
	Check(NetChatPartLength(line, sizeof(line)) == 2, "an empty one has the byte saying who it's for, and the zero");

	line[1] = 'x';
	line[3] = 'x';
	Check(NetChatPartLength(line, kNetMostChatLetters) == 0, "with no zero in what's there, it doesn't fit, however long it could have been");

	line[5] = 0;
	Check(NetChatPartLength(line, 5) == 0, "and a zero just past the end of the message doesn't count, because that isn't part of it");
	Check(NetChatPartLength(line, 6) == 6, "but one just inside it does");
	line[5] = 'x';

	line[kNetMostChatLetters + 1] = 0;
	Check(NetChatPartLength(line, sizeof(line)) == kNetMostChatLetters + 2, "the longest there can be ends at the end of its letters");

	line[kNetMostChatLetters + 1] = 'x';
	line[kNetMostChatLetters + 2] = 0;
	Check(NetChatPartLength(line, sizeof(line)) == 0, "and one letter more doesn't fit");

	line[0] = 0;
	Check(NetChatPartLength(line, 1) == 0, "and a line that stops after the first byte has no zero to find");
}

static void TestNames()
{
	puts("player names");

	char from[13];
	char to[13];

	memset(from, 'n', sizeof(from));
	memset(to, 0, sizeof(to));
	NetCopyPlayerName(to, from, sizeof(to));
	Check(to[sizeof(to) - 1] == 0, "a name that never ends gets an end");
	Check(memcmp(to, from, sizeof(to) - 1) == 0, "and keeps everything before it");
}

static void TestAnswersInASecond()
{
	unsigned int started = 0;
	int used = 0;
	int allowed = 0;

	for (int i = 0; i < 25; i++)
	{
		allowed += NetThisSecondHasRoom(100, &started, &used, 10) ? 1 : 0;
	}

	Check(allowed == 10, "only so many answers in a second");
	Check(!NetThisSecondHasRoom(999, &started, &used, 10), "and still none just before the second is up");
	Check(NetThisSecondHasRoom(1100, &started, &used, 10), "and then there's room again");
	Check(started == 1100 && used == 1, "from a new second");
	started = 0xFFFFFFF0u;
	used = 10;
	Check(!NetThisSecondHasRoom(0xFFFFFFF8u, &started, &used, 10), "the clock's numbers near the top still work");
	Check(NetThisSecondHasRoom(1000u, &started, &used, 10), "and when they wrap round to the start");
}

// Where a campaign goes next, as the host says. The place is used to look up a level in a list.
static void TestCampaignPlaces()
{
	Check(NetCampaignMoveIsGood(1, 3, 1, 2, 11), "going on to the next level is good");
	Check(NetCampaignMoveIsGood(1, 11, 1, 10, 11), "and so is the place just past the last, which ends the campaign");
	Check(!NetCampaignMoveIsGood(1, 12, 1, 10, 11), "but nothing further");
	Check(!NetCampaignMoveIsGood(1, 2, 1, 2, 11), "the same place again is refused, or one message could reload a level for ever");
	Check(!NetCampaignMoveIsGood(1, 1, 1, 2, 11), "and so is going back");
	Check(!NetCampaignMoveIsGood(2, 3, 1, 2, 11), "and another campaign's");
	Check(!NetCampaignMoveIsGood(0, 3, 0, 2, 11), "and it means nothing in a game with no campaign");
	Check(NetCampaignDifficultyIsGood(0), "the easiest difficulty is good");
	Check(NetCampaignDifficultyIsGood(1), "and the middle");
	Check(NetCampaignDifficultyIsGood(2), "and the hardest on offer");
	Check(!NetCampaignDifficultyIsGood(3), "and the next isn't");
	Check(!NetCampaignDifficultyIsGood(255), "nor anything a byte can hold above that");
	Check(!NetCampaignDifficultyIsGood(-1), "nor below");
	Check(NetCampaignPlaceIsGood(0, 0, 0), "no campaign, at place 0, is what most games say");
	Check(!NetCampaignPlaceIsGood(0, 1, 0), "no campaign at any other place isn't");
	Check(NetCampaignPlaceIsGood(1, 0, 11), "the first level of one is good");
	Check(NetCampaignPlaceIsGood(1, 10, 11), "and the last");
	Check(!NetCampaignPlaceIsGood(1, 11, 11), "a joiner can't be told the place past the last");
	Check(!NetCampaignPlaceIsGood(1, -1, 11), "or one before the first");
	Check(!NetCampaignPlaceIsGood(9, 0, 0), "and a campaign with no levels has no places");
}

static void TestDamageMultiples()
{
	Check(NetDamageMultipleIsGood(65536), "a hit counting once is good");
	Check(NetDamageMultipleIsGood(0), "and so is none at all");
	Check(NetDamageMultipleIsGood(kNetMostDamageMultiple), "and so is the most");
	Check(!NetDamageMultipleIsGood(kNetMostDamageMultiple + 1), "but not more");
	Check(!NetDamageMultipleIsGood(-1), "and no negative number, which would go round the damage loop about four billion times");
	Check(!NetDamageMultipleIsGood(-2147483647 - 1), "however large");
}

static size_t LengthForTheBudgetTest(unsigned char, const unsigned char *part, size_t)
{
	return 1 + (size_t)part[0];
}

static void TestRoomForAmounts()
{
	unsigned int started = 1000;
	int used = 0;

	Check(NetThisSecondHasRoomFor(1000, &started, &used, 100, 60), "an amount that fits has room");
	Check(!NetThisSecondHasRoomFor(1500, &started, &used, 100, 41), "one that wouldn't doesn't");
	Check(used == 60, "and what's refused isn't counted");
	Check(NetThisSecondHasRoomFor(1600, &started, &used, 100, 40), "so what still fits does");
	Check(!NetThisSecondHasRoomFor(1700, &started, &used, 100, 1), "up to the limit exactly");
	Check(NetThisSecondHasRoomFor(2000, &started, &used, 100, 100), "and the next second it starts again");
	Check(!NetThisSecondHasRoomFor(3000, &started, &used, 100, 2000000000), "a huge amount can't overflow the sum");
}

static void TestWhatAClientMaySpend()
{
	// Kind 0 is one byte long with no limit, kind 1 is one byte long and at most 3 a second, kind 2 varies and at most 1 a second
	static const NetPartRule rules[] =
	{
		{ 0, 1, false, false, 0 },
		{ 1, 1, false, false, 3 },
		{ 2, 1, true, false, 1 }
	};
	static const size_t ruleCount = sizeof(rules) / sizeof(rules[0]);

	NetPartsSpent spent;

	memset(&spent, 0, sizeof(spent));

	unsigned char const threeLimited[] = { 1, 9, 1, 9, 1, 9 };
	unsigned char const oneLimited[] = { 1, 9 };
	unsigned char const manyFree[] = { 0, 9, 0, 9, 0, 9, 0, 9, 0, 9, 0, 9 };
	unsigned char const twoLimited[] = { 1, 9, 1, 9 };

	Check(NetPartsFitTheBudget(threeLimited, sizeof(threeLimited), rules, ruleCount, LengthForTheBudgetTest, spent, 5000), "a client can spend all of its budget");
	Check(!NetPartsFitTheBudget(oneLimited, sizeof(oneLimited), rules, ruleCount, LengthForTheBudgetTest, spent, 5100), "but not one more in that second");
	Check(NetPartsFitTheBudget(manyFree, sizeof(manyFree), rules, ruleCount, LengthForTheBudgetTest, spent, 5200), "and a kind with no limit isn't touched by that");
	Check(NetPartsFitTheBudget(oneLimited, sizeof(oneLimited), rules, ruleCount, LengthForTheBudgetTest, spent, 6000), "the next second it has its budget back");

	// A message that doesn't fit spends nothing
	unsigned char const tooMany[] = { 1, 9, 1, 9, 1, 9 };

	Check(!NetPartsFitTheBudget(tooMany, sizeof(tooMany), rules, ruleCount, LengthForTheBudgetTest, spent, 6100), "one message can't go over either, whatever's in it");
	Check(NetPartsFitTheBudget(twoLimited, sizeof(twoLimited), rules, ruleCount, LengthForTheBudgetTest, spent, 6200), "and a refused message spends nothing");

	// A part whose length varies is walked by its real length, so what's inside it isn't read as parts
	NetPartsSpent spentOnChat;

	memset(&spentOnChat, 0, sizeof(spentOnChat));

	unsigned char const chatHoldingWhatLooksLikeParts[] = { 2, 3, 1, 1, 1 };

	Check(NetPartsFitTheBudget(chatHoldingWhatLooksLikeParts, sizeof(chatHoldingWhatLooksLikeParts), rules, ruleCount, LengthForTheBudgetTest, spentOnChat, 100), "a part's inside isn't counted as parts");
	Check(!NetPartsFitTheBudget(chatHoldingWhatLooksLikeParts, sizeof(chatHoldingWhatLooksLikeParts), rules, ruleCount, LengthForTheBudgetTest, spentOnChat, 200), "and the part itself is");

	unsigned char const unknown[] = { 200 };

	Check(!NetPartsFitTheBudget(unknown, sizeof(unknown), rules, ruleCount, LengthForTheBudgetTest, spentOnChat, 300), "a kind that doesn't exist isn't paid for, it's refused");

	Check(!NetPoolIsShort(899, 1000, 100), "with more than what's kept back left, a pool isn't short");
	Check(NetPoolIsShort(900, 1000, 100), "with only that left, it is");
}

static void TestSoundAddresses()
{
	int block[4];

	Check(SoundAddressIsInside(&block[0], block, sizeof(block)), "the first number of a block is inside it");
	Check(SoundAddressIsInside(&block[3], block, sizeof(block)), "and so is the last");
	Check(!SoundAddressIsInside(&block[3] + 1, block, sizeof(block)), "one past the end isn't");
	Check(!SoundAddressIsInside(&block[1], &block[2], sizeof(int)), "one before the start isn't");
	Check(!SoundAddressIsInside(NULL, block, sizeof(block)), "and a sound with no address has nothing to forget");
}

static void TestCountingRefusals()
{
	NetRefusalCounts counts;

	memset(&counts, 0, sizeof(counts));

	char const flood[] = "flood";
	char const other[] = "other";
	// The same words at another address: the game may hold one reason's text twice
	char floodAgain[sizeof(flood)];

	memcpy(floodAgain, flood, sizeof(flood));

	Check(NetCountRefusal(counts, flood) == 1, "a reason's first refusal is number 1");
	Check(NetCountRefusal(counts, floodAgain) == 2, "the same words count as the same reason");
	Check(NetCountRefusal(counts, other) == 1, "another reason has its own count");

	for (int i = 0; i < 1000; i++)
	{
		NetCountRefusal(counts, flood);
	}

	Check(NetCountRefusal(counts, other) == 2, "and a flood of the first doesn't touch it");

	Check(NetLogThisRefusal(1) && NetLogThisRefusal(kRefusalsLoggedInFull), "the first ones all go in the log");
	Check(!NetLogThisRefusal(kRefusalsLoggedInFull + 1), "then they stop");
	Check(NetLogThisRefusal(64) && !NetLogThisRefusal(65), "except now and then");

	NetRefusalCounts full;
	char names[kMostRefusalReasons + 5][8];

	memset(&full, 0, sizeof(full));

	for (int i = 0; i < kMostRefusalReasons + 5; i++)
	{
		names[i][0] = (char)('A' + i / 26);
		names[i][1] = (char)('A' + i % 26);
		names[i][2] = 0;
		NetCountRefusal(full, names[i]);
	}

	Check(full.counts[kMostRefusalReasons - 1] == 6, "more reasons than places share the last one, and nothing is written past the end");

	Check(NetLevelIsOnItsList(0, 13) && NetLevelIsOnItsList(12, 13), "levels on the list are good");
	Check(!NetLevelIsOnItsList(13, 13), "one past the end isn't");
	Check(!NetLevelIsOnItsList(-1, 13), "and nor is one before the start");
}

static void TestWhatGoesOnTheScreen()
{
	char text[] = "one\ntwo\tthree\x7f four";

	NetCleanText(text, sizeof(text));
	Check(strcmp(text, "one two three  four") == 0, "control characters become spaces");

	char noEnd[4] = { 'a', 1, 'b', 2 };

	NetCleanText(noEnd, sizeof(noEnd));
	Check(noEnd[1] == ' ' && noEnd[3] == ' ', "text with no end is only read within its room");

	char high[] = { (char)0xC0, 'x', 0 };

	NetCleanText(high, sizeof(high));
	Check(high[0] == (char)0xC0, "a letter past plain English stays");

	char from[13];
	char to[13];

	memset(from, 3, sizeof(from));
	NetCopyPlayerName(to, from, sizeof(to));
	Check(to[0] == ' ' && to[12] == 0, "a name is cleaned as it's copied");

	auto const justOne = [](const char *candidate) { return strcmp(candidate, "BOB") == 0; };
	char name[13] = "BOB";

	Check(NetMakeNameUnique(name, sizeof(name), justOne, 9) && strcmp(name, "BOB2") == 0, "the second BOB gets a number");

	char other[13] = "ANN";

	Check(NetMakeNameUnique(other, sizeof(other), justOne, 9) && strcmp(other, "ANN") == 0, "a name nobody has stays as it is");

	auto const manyTaken = [](const char *candidate) { return strcmp(candidate, "BOB") == 0 || strcmp(candidate, "BOB2") == 0 || strcmp(candidate, "BOB3") == 0; };
	char third[13] = "BOB";

	Check(NetMakeNameUnique(third, sizeof(third), manyTaken, 9) && strcmp(third, "BOB4") == 0, "it takes the first number that's free");

	char full[13] = "ABCDEFGHIJKL";
	auto const fullTaken = [](const char *candidate) { return strcmp(candidate, "ABCDEFGHIJKL") == 0; };

	Check(NetMakeNameUnique(full, sizeof(full), fullTaken, 9) && strcmp(full, "ABCDEFGHIJK2") == 0, "a name that fills its room is cut to fit the number");

	auto const everything = [](const char *) { return true; };
	char none[13] = "BOB";

	Check(!NetMakeNameUnique(none, sizeof(none), everything, 9), "it says so if no number was free");
}

int main()
{
	TestCampaignPlaces();
	TestWhatGoesOnTheScreen();
	TestCountingRefusals();
	TestSoundAddresses();
	TestWhatAClientMaySpend();
	TestRoomForAmounts();
	TestPressMessages();
	TestEveryoneElseIsReady();
	TestLiftCallReach();
	TestLiftSync();
	TestBreakableHits();
	TestOtherSpeciesAsFriends();
	TestDamageMultiples();
	TestAnswersInASecond();
	TestAmmunitionNumbers();
	TestTheLogCantBeFlooded();
	TestTroubleAllLevel();
	TestMessageParts();
	TestChatLines();
	TestNames();

	if (checksFailed)
	{
		printf("The network checks test FAILED: %d checks\n", checksFailed);
		return 1;
	}

	puts("The network checks test passed");
	return 0;
}
