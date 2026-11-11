// The test of the check that a player could reach a step of a level's chain. Written 2026-11-06 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// 3dc/avp/coop_reach.h: reading a level's room list, which comes from outside the code, and working out which rooms can be
// reached through open doors. Every refusal of a bad file is checked by its exact reason, so a later check can't hide a
// missing one.
//
// WHAT IT DOESN'T TEST
//
// Whether the room lists are right about the real levels, or whether the game's doors really behave as the game code says.
// The rest of the game is too tied together to compile here, so hudson_strategies.cpp has only the game building to check
// it, and the walking test running. And a route existing isn't a player walking it, which only a real walk shows.

#include <stdio.h>
#include <string>
#include <vector>

#include "coop_reach.h"

static int checksFailed = 0;

static void Check(bool passed, char const *what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what);

	if (!passed)
	{
		checksFailed++;
	}
}

// Reads a list that's meant to be fine. If it isn't, this says why, and the test fails.
static CoopRoomList ListOf(char const *text)
{
	CoopRoomList list;
	std::string whyNot;

	if (!CoopReach_ReadList(text, &list, &whyNot))
	{
		printf("  FAILED  a list in this test was refused: %s\n", whyNot.c_str());
		checksFailed++;
	}

	return list;
}

// Whether this file is refused, for exactly this reason
static void CheckRefused(std::string const &text, std::string const &reason)
{
	CoopRoomList list;
	std::string whyNot;
	bool const read = CoopReach_ReadList(text, &list, &whyNot);

	Check(!read && whyNot == reason, ("refused: " + reason).c_str());

	if (read || whyNot != reason)
	{
		printf("          it said: %s\n", read ? "fine" : whyNot.c_str());
	}
}

static int Room(CoopRoomList const &list, char const *name)
{
	return CoopReach_RoomNumber(list, name);
}

// This machine's doors, with these ones open
static std::vector<char> Doors(CoopRoomList const &list, std::vector<char const *> const &open)
{
	std::vector<char> doors(list.names.size(), 0);

	for (size_t i = 0; i < open.size(); i++)
	{
		doors[Room(list, open[i])] = 1;
	}

	return doors;
}

static std::vector<std::string> Names(std::vector<char const *> const &names)
{
	return std::vector<std::string>(names.begin(), names.end());
}

static CoopReach ReachFrom(CoopRoomList const &list, char const *start, std::vector<char const *> const &open)
{
	CoopReach reach;
	std::vector<int> const players(1, Room(list, start));

	CoopReach_Begin(list, &reach);
	CoopReach_Spread(list, Doors(list, open), players, &reach);

	return reach;
}

static std::string Says(CoopRoomList const &list, CoopReach const &reach, std::vector<char const *> const &where, std::vector<char const *> const &near = std::vector<char const *>())
{
	return CoopReach_InWords(list, CoopReach_Ask(list, reach, Names(where), Names(near)));
}

static void TestReadingAFile()
{
	puts("reading a room list");

	CoopRoomList list;
	std::string whyNot;

	Check(CoopReach_ReadList("# a note\r\n\r\ndoor D | proximity\r\njoin A | D | file  \r\nliftdoor 1 | D\r\nliftswitch 1 | A\r\nliftswitch 1 | C | near\r\njoin Room with spaces | A | sees", &list, &whyNot), "a fine file is read, with its notes, blank lines and Windows line ends");
	Check(list.names.size() == 4, "and has its 4 rooms");
	Check(list.joinCount == 2, "and its 2 joins");
	Check(list.lifts.size() == 1 && list.lifts[1].doors.size() == 1 && list.lifts[1].switches.size() == 2, "and its lift");
	Check(list.lifts[1].switches[1].onlyNear && !list.lifts[1].switches[0].onlyNear, "a lift switch can be marked as only near");
	Check(CoopReach_RoomNumber(list, "Room with spaces") == 3, "a room's name can have spaces");
	Check(CoopReach_RoomNumber(list, "room WITH spaces") == -1, "and case matters, because two rooms in a level can differ only by case");
	Check(list.joins[Room(list, "D")].size() == 1 && list.joins[Room(list, "A")].size() == 2, "a join works from both rooms");
	Check(CoopReach_RoomNumber(list, "nowhere") == -1, "a room not on the list isn't found");

	Check(CoopReach_ReadList("", &list, &whyNot) && list.names.empty(), "an empty file is an empty list");
	Check(CoopReach_ReadList("room Alone\nroom Two words\nroom Alone\n", &list, &whyNot) && list.names.size() == 2 && list.joins[0].empty(), "a room joined to nothing has a line of its own, and naming it twice is fine");
	Check(CoopReach_ReadList("join shed | SHED | file\ndoor Shed | switch\ndoor SHED | lift\n", &list, &whyNot) && list.names.size() == 3, "rooms that differ only by case are different rooms");
	Check(CoopReach_ReadList("door A | proximity\ndoor B | switch\ndoor C | lift\ndoor D | platform\n", &list, &whyNot) && list.names.size() == 4, "all four kinds of door are known");
	Check(CoopReach_ReadList("join A | B | file\njoin B | C | lift\njoin C | D | touching\njoin D | E | sees\n", &list, &whyNot) && list.names.size() == 5, "all four ways to join are known");
	Check(CoopReach_ReadList("liftdoor 256 | D\ndoor D | lift\nliftswitch 1 | A\n", &list, &whyNot), "the highest lift number is fine, and a door can be listed after its lift");
}

static void TestRefusingAFile()
{
	puts("refusing a bad room list, each for its own reason");

	CheckRefused(std::string(kCoopMostBytes + 1, ' '), "the file is too big");
	CheckRefused("join A | B | file\n" + std::string(kCoopLongestLine + 1, 'x'), "line 2: the line is too long");
	CheckRefused("wall A | B", "line 1: this file doesn't use that word");
	CheckRefused("door", "line 1: a line needs a word and what it's about");
	CheckRefused("door A", "line 1: a door line needs a room and its kind");
	CheckRefused("door A | proximity | spare", "line 1: a door line needs a room and its kind");
	CheckRefused("door A | wooden", "line 1: that isn't a kind of door");
	CheckRefused(std::string("door A\x01 | proximity"), "line 1: that can't be a room's name");
	CheckRefused(std::string("door A\xC3 | proximity"), "line 1: that can't be a room's name");
	CheckRefused("door " + std::string(kCoopLongestName + 1, 'x') + " | proximity", "line 1: that can't be a room's name");
	CheckRefused("door  | proximity", "line 1: that can't be a room's name");
	CheckRefused("door A | proximity\ndoor A | switch", "line 2: that door is already listed");
	CheckRefused("room A | B", "line 1: a room line needs one room");
	CheckRefused("lockedlift A | B", "line 1: a locked lift line needs one lift");
	CheckRefused("lockedlift A\x01", "line 1: that can't be a lift's name");
	CheckRefused("room A\x01", "line 1: that can't be a room's name");
	CheckRefused("join A | B", "line 1: a join line needs two rooms and how they join");
	CheckRefused("join A | B | file | spare", "line 1: a join line needs two rooms and how they join");
	CheckRefused("join A | B | wished", "line 1: that isn't a way for rooms to join");
	CheckRefused(std::string("join A\x7F | B | file"), "line 1: that can't be a room's name");
	CheckRefused(std::string("join A | B | file\n\x1B | file"), "line 2: this file doesn't use that word");
	CheckRefused(std::string("join A | \x1B | file"), "line 1: that can't be a room's name");
	CheckRefused("join A | A | file", "line 1: a room can't join itself");
	CheckRefused("liftdoor 1", "line 1: a lift line needs a lift's number and a room");
	CheckRefused("liftdoor 1 | A | near", "line 1: a lift line needs a lift's number and a room");
	CheckRefused("liftswitch 1 | A | far", "line 1: the only word after a lift switch's room is near");
	CheckRefused("liftswitch 1 | A | near | spare", "line 1: a lift line needs a lift's number and a room");
	CheckRefused("liftdoor one | A", "line 1: that isn't a lift's number");
	CheckRefused("liftdoor 0 | A", "line 1: that isn't a lift's number");
	CheckRefused("liftdoor  | A", "line 1: that isn't a lift's number");
	CheckRefused("liftdoor 1a | A", "line 1: that isn't a lift's number"); // without the check on each digit, this would come out as 59
	CheckRefused("liftdoor 5/ | A", "line 1: that isn't a lift's number"); // and this as 49
	CheckRefused("liftdoor 257 | A", "line 1: that isn't a lift's number");
	CheckRefused("liftdoor 99999999999 | A", "line 1: that isn't a lift's number");
	CheckRefused("liftdoor 4294967297 | A", "line 1: that isn't a lift's number"); // too big for a whole number, so it would wrap round to 1
	CheckRefused(" | A", "line 1: this file doesn't use that word");
	CheckRefused(std::string("liftswitch 1 | A\x02"), "line 1: that can't be a room's name");
	CheckRefused("liftdoor 1 | A", "lift 1 has a door that isn't listed as a door: A");

	std::string tooManyRooms;

	for (size_t i = 0; i <= kCoopMostRooms; i++)
	{
		tooManyRooms += "door R" + std::to_string(i) + " | proximity\n";
	}

	CheckRefused(tooManyRooms, "line 4097: too many rooms");

	std::string tooManyLockedLifts;

	for (size_t i = 0; i <= kCoopMostLockedLifts; i++)
	{
		tooManyLockedLifts += "lockedlift L" + std::to_string(i) + "\n";
	}

	CheckRefused(tooManyLockedLifts, "line 257: too many locked lifts");

	std::string tooManyRoomsByJoin;

	for (size_t i = 0; i < kCoopMostRooms; i++)
	{
		tooManyRoomsByJoin += "door R" + std::to_string(i) + " | proximity\n";
	}

	CheckRefused(tooManyRoomsByJoin + "room new", "line 4097: too many rooms");

	CheckRefused(tooManyRoomsByJoin + "join R0 | new | file", "line 4097: too many rooms");
	CheckRefused(tooManyRoomsByJoin + "liftswitch 1 | new", "line 4097: too many rooms");

	std::string tooManyJoins;

	for (size_t i = 0; i <= kCoopMostJoins; i++)
	{
		tooManyJoins += "join A | B | file\n";
	}

	CheckRefused(tooManyJoins, "line 65537: too many joins");
}

static void TestDoors()
{
	puts("doors");

	CoopRoomList const list = ListOf(
		"door D1 | proximity\n"
		"door D2 | switch\n"
		"join A | B | file\n"
		"join B | D1 | file\n"
		"join D1 | C | file\n"
		"join C | D2 | file\n"
		"join D2 | E | file\n"
		"join F | G | file\n");

	std::vector<char const *> const noneOpen;
	CoopReach reach = ReachFrom(list, "A", noneOpen);

	Check(Says(list, reach, { "A" }) == "a route exists through open doors", "the room a player stands in can be reached");
	Check(Says(list, reach, { "B" }) == "a route exists through open doors", "and one that joins it, with no door");
	Check(Says(list, reach, { "C" }) == "behind shut door D1", "a shut door stops it, and is named");
	Check(Says(list, reach, { "E" }) == "behind shut door D1", "and the door named is the first of two on the way");
	Check(Says(list, reach, { "D1" }) == "behind shut door D1", "a shut door's own room can't be reached either");
	Check(Says(list, reach, { "G" }) == "nothing in the room list joins it to where the players have been", "a room joined to nothing they've reached says so");
	Check(Says(list, reach, { "not on the list" }) == "nothing in the room list joins it to where the players have been", "and so does a room not on the list");
	Check(Says(list, reach, { "G", "B" }) == "a route exists through open doors", "any one of several rooms is enough");
	Check(Says(list, reach, {}) == "nothing in the room list joins it to where the players have been", "a step with no rooms at all isn't reached");

	reach = ReachFrom(list, "A", { "D1" });
	Check(Says(list, reach, { "C" }) == "a route exists through open doors", "an open door lets it through");
	Check(Says(list, reach, { "E" }) == "behind shut door D2", "and the next shut door is the one named");

	reach = ReachFrom(list, "A", { "D1", "D2" });
	Check(Says(list, reach, { "E" }) == "a route exists through open doors", "two open doors in a row");

	CoopReach_Spread(list, Doors(list, noneOpen), std::vector<int>(1, Room(list, "A")), &reach);
	Check(Says(list, reach, { "E" }) == "a route exists through open doors", "rooms stay reached after the door shuts behind them");

	reach = ReachFrom(list, "C", noneOpen);
	Check(Says(list, reach, { "C" }) == "a route exists through open doors", "a player really standing behind a shut door counts");
	Check(Says(list, reach, { "A" }) == "behind shut door D1", "and that door still stops the way back");

	CoopReach two;
	std::vector<int> players;

	players.push_back(Room(list, "A"));
	players.push_back(Room(list, "E"));
	CoopReach_Begin(list, &two);
	CoopReach_Spread(list, Doors(list, noneOpen), players, &two);
	Check(Says(list, two, { "B" }) == "a route exists through open doors" && Says(list, two, { "E" }) == "a route exists through open doors", "every player counts, not only the first");

	// A player number that isn't a room doesn't count, and doesn't break anything
	CoopReach odd;

	players.clear();
	players.push_back(-1);
	players.push_back((int)list.names.size());
	CoopReach_Begin(list, &odd);
	CoopReach_Spread(list, Doors(list, noneOpen), players, &odd);
	Check(Says(list, odd, { "A" }) != "a route exists through open doors", "a player in no known room gives no route");

	// Two machines looking at the same level, whose doors disagree
	CoopReach host = ReachFrom(list, "A", { "D1" }), joiner = ReachFrom(list, "A", noneOpen);

	Check(Says(list, host, { "C" }) != Says(list, joiner, { "C" }), "a door open on one machine and shut on the other gives different answers");

	// A shorter way behind one door is named over a longer way behind another
	// The far way's room comes first in the list, so finding the shorter way can't be down to the order of the file
	CoopRoomList const twoWays = ListOf(
		"door Far | proximity\n"
		"door Near | proximity\n"
		"join Hall | Far | file\n"
		"join Far | Beyond | file\n"
		"join Beyond | Goal | file\n"
		"join Hall | Start | file\n"
		"join Start | Near | file\n"
		"join Near | Goal | file\n");
	CoopReach ways = ReachFrom(twoWays, "Start", noneOpen);

	Check(Says(twoWays, ways, { "Goal" }) == "behind shut door Near", "the door on the shortest way is the one named");

	Check(CoopReach_IsARoom(list, 0), "the first room is a room");
	Check(CoopReach_IsARoom(list, (int)list.names.size() - 1), "and so is the last");
	Check(!CoopReach_IsARoom(list, -1), "a player in no known room isn't in a room");
	Check(!CoopReach_IsARoom(list, (int)list.names.size()), "and neither is one past the last");
}

static void TestLifts()
{
	puts("lifts");

	CoopRoomList const list = ListOf(
		"door Up | lift\n"
		"door Down | lift\n"
		"door Other | lift\n"
		"join Foyer | Down | file\n"
		"join Down | LiftLow | file\n"
		"join LiftLow | LiftHigh | lift\n"
		"join LiftHigh | Up | file\n"
		"join Up | Roof | file\n"
		"join Roof | Other | file\n"
		"join Other | Shed | file\n"
		"join Yard | Foyer | file\n"
		"liftdoor 1 | Up\n"
		"liftdoor 1 | Down\n"
		"liftswitch 1 | Foyer\n"
		"liftdoor 2 | Other\n"
		"liftswitch 2 | Shed\n");

	std::vector<char const *> const noneOpen;
	CoopReach reach = ReachFrom(list, "Yard", noneOpen);

	Check(Says(list, reach, { "Roof" }) == "a route exists through open doors", "reaching a lift's switch opens every door of that lift");
	Check(Says(list, reach, { "Shed" }) == "behind shut door Other", "and not another lift's door, whose switch is behind it");

	reach = ReachFrom(list, "Shed", noneOpen);
	Check(Says(list, reach, { "Roof" }) == "a route exists through open doors", "another lift works the same way, from the other side");

	CoopRoomList const farSwitch = ListOf(
		"door Up | lift\n"
		"join Start | Up | file\n"
		"join Up | Roof | file\n"
		"join Far | Away | file\n"
		"liftdoor 1 | Up\n"
		"liftswitch 1 | Away\n");

	reach = ReachFrom(farSwitch, "Start", noneOpen);
	Check(Says(farSwitch, reach, { "Roof" }) == "behind shut door Up", "a lift whose switch can't be reached doesn't open its door");

	CoopRoomList const nearSwitch = ListOf(
		"door Up | lift\n"
		"join Start | Up | file\n"
		"join Up | Roof | file\n"
		"liftdoor 1 | Up\n"
		"liftswitch 1 | Start | near\n");

	reach = ReachFrom(nearSwitch, "Start", noneOpen);
	Check(Says(nearSwitch, reach, { "Roof" }) == "a route exists through open doors, resting on a guess: a lift's switch that's only near a room, for door Up", "a switch that's only near a room works, and the answer says it rests on that");
	Check(reach.weakest[Room(nearSwitch, "Up")] == kCoopFairGuess, "that makes the door a guess");
	Check(reach.weakest[Room(nearSwitch, "Roof")] == kCoopFairGuess, "and everything behind it");

	reach = ReachFrom(nearSwitch, "Start", { "Up" });
	Check(Says(nearSwitch, reach, { "Roof" }) == "a route exists through open doors", "a lift door that's really open needs no guess");
}

static void TestGuesses()
{
	puts("guesses");

	CoopRoomList const list = ListOf(
		"join A | T | touching\n"
		"join T | S | sees\n"
		"join A | F | file\n");

	std::vector<char const *> const noneOpen;
	CoopReach reach = ReachFrom(list, "A", noneOpen);

	Check(Says(list, reach, { "F" }) == "a route exists through open doors", "a firm way has nothing to say about guesses");
	Check(Says(list, reach, { "T" }) == "a route exists through open doors, resting on a guess: rooms that touch, A and T", "a way through rooms that touch says so");
	Check(Says(list, reach, { "S" }) == "a route exists through open doors, resting on a guess: rooms that can see each other, T and S; rooms that touch, A and T", "and one through rooms that see each other lists every guess on it");

	// A firm way wins over a guess, whichever is found first
	CoopRoomList const both = ListOf(
		"join A | B | touching\n"
		"join A | M | file\n"
		"join M | B | file\n"
		"join B | C | file\n");

	reach = ReachFrom(both, "A", noneOpen);
	Check(Says(both, reach, { "C" }) == "a route exists through open doors", "a firm way beats a shorter guess");

	// Of several rooms it could be in, the answer goes by the firmest, wherever it comes in the list
	reach = ReachFrom(list, "A", noneOpen);
	Check(Says(list, reach, { "S", "F" }) == "a route exists through open doors", "a firm room beats a guessed one listed before it");
	Check(Says(list, reach, { "F", "S" }) == "a route exists through open doors", "and one listed after it");
	Check(Says(list, reach, { "S", "T" }) == "a route exists through open doors, resting on a guess: rooms that touch, A and T", "a better guess beats a worse one");
	Check(Says(list, reach, { "nowhere" }, { "F" }) == "a route exists through open doors, to within 2 m of it, from room F", "a near room beats nothing");

	// A guess beaten later, when a door opens
	CoopRoomList const later = ListOf(
		"door D | switch\n"
		"join A | B | touching\n"
		"join A | D | file\n"
		"join D | B | file\n");
	CoopReach grows = ReachFrom(later, "A", noneOpen);

	Check(Says(later, grows, { "B" }) == "a route exists through open doors, resting on a guess: rooms that touch, A and B", "with the door shut, it rests on the guess");

	CoopReach_Spread(later, Doors(later, { "D" }), std::vector<int>(1, Room(later, "A")), &grows);
	Check(Says(later, grows, { "B" }) == "a route exists through open doors", "and once the door opens, it no longer does");

	// A weak guess gives way to a less weak one
	CoopRoomList const weak = ListOf(
		"join A | B | sees\n"
		"join A | M | touching\n"
		"join M | B | touching\n");

	reach = ReachFrom(weak, "A", noneOpen);
	Check(Says(weak, reach, { "B" }) == "a route exists through open doors, resting on a guess: rooms that touch, M and B; rooms that touch, A and M", "two rooms that touch beat one that sees");

	// Standing in a room beats any guess about it
	CoopReach standing = ReachFrom(list, "A", noneOpen);

	CoopReach_Spread(list, Doors(list, noneOpen), std::vector<int>(1, Room(list, "T")), &standing);
	Check(Says(list, standing, { "T" }) == "a route exists through open doors", "a player really in a room needs no guess to get there");

	// Where and near
	CoopRoomList const rooms = ListOf(
		"join A | B | file\n"
		"join G | H | file\n");

	reach = ReachFrom(rooms, "A", noneOpen);
	Check(Says(rooms, reach, { "B" }, { "A" }) == "a route exists through open doors", "the room it's in counts first");
	Check(Says(rooms, reach, { "G" }, { "B" }) == "a route exists through open doors, to within 2 m of it, from room B", "a room it's only near counts next, and the answer says so");
	Check(Says(rooms, reach, { "G" }, { "H" }) == "nothing in the room list joins it to where the players have been", "not reaching either gives no route");

	CoopRoomList const doorNear = ListOf(
		"door D | switch\n"
		"join A | D | file\n"
		"join D | B | file\n");

	reach = ReachFrom(doorNear, "A", noneOpen);
	Check(Says(doorNear, reach, { "nowhere" }, { "B" }) == "behind shut door D", "a near room behind a door names the door");
}

static void TestLockedLifts()
{
	puts("lifts that start locked");

	CoopRoomList none = ListOf("join A | B | file\n");

	Check(none.lockedLifts.empty(), "a level with none has none");
	Check(CoopReach_LiftWords(none).empty(), "and nothing to say about them");

	CoopRoomList const one = ListOf("lockedlift Platlift\njoin A | B | file\n");

	Check(one.lockedLifts.size() == 1, "one is read");
	Check(one.names.size() == 2, "and it isn't a room, because it isn't one in the level file");
	Check(CoopReach_RoomNumber(one, "Platlift") == -1, "so it can't be found as a room");
	Check(CoopReach_LiftWords(one) == "depends on a lift that starts locked, so a route doesn't show a player can get there: Platlift", "the log names it");

	CoopRoomList const two = ListOf("lockedlift First\nlockedlift Second lift\n");

	Check(CoopReach_LiftWords(two) == "depends on a lift that starts locked, so a route doesn't show a player can get there: First, Second lift", "and names every one");

	// However many steps have a route, the lift is still named
	CoopRoomList const walked = ListOf("lockedlift Platlift\njoin A | B | file\n");
	CoopReach reach;

	CoopReach_Begin(walked, &reach);
	CoopReach_Spread(walked, std::vector<char>(walked.names.size(), 0), std::vector<int>(1, Room(walked, "A")), &reach);
	Check(Says(walked, reach, { "B" }) == "a route exists through open doors", "a route can exist");
	Check(!CoopReach_LiftWords(walked).empty(), "and the level still depends on the lift");
}

static void TestNames()
{
	puts("splitting names");

	std::vector<std::string> parts = CoopReach_SplitNames("one | two words | three");

	Check(parts.size() == 3 && parts[0] == "one" && parts[1] == "two words" && parts[2] == "three", "names are split at the bars, and can have spaces");

	parts = CoopReach_SplitNames("alone");
	Check(parts.size() == 1 && parts[0] == "alone", "one name is one name");
	Check(CoopReach_NameIsFine("a ~") && CoopReach_NameIsFine(std::string(kCoopLongestName, 'x')), "the longest name is fine");
}

static bool ReadWholeFile(std::string const &path, std::string *text)
{
	FILE *const file = fopen(path.c_str(), "rb");

	if (!file)
	{
		return false;
	}

	char part[4096];
	size_t got;

	text->clear();

	while ((got = fread(part, 1, sizeof(part), file)) > 0)
	{
		text->append(part, got);
	}

	fclose(file);
	return true;
}

// Whether every step of a chain file that needs a room says which, and every room it names is on the level's list. Gives back what's wrong, or nothing.
static std::string WhatsWrongWithChain(std::string const &text, CoopRoomList const &list)
{
	bool haveARoom = false;
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

		if (!line.empty() && line[line.size() - 1] == '\r')
		{
			line.erase(line.size() - 1);
		}

		if (line.empty() || line[0] == '#')
		{
			continue;
		}

		size_t const space = line.find(' ');
		std::string const word = line.substr(0, space);
		std::string const rest = space == std::string::npos ? std::string() : line.substr(space + 1);

		if (word == "where" || word == "near")
		{
			std::vector<std::string> const names = CoopReach_SplitNames(rest);

			for (size_t i = 0; i < names.size(); i++)
			{
				if (CoopReach_RoomNumber(list, names[i]) < 0)
				{
					return "the room isn't on the list: " + names[i];
				}
			}

			haveARoom = true;
			continue;
		}

		if (word == "press" || word == "walk" || word == "destroy" || word == "kill" || word == "pickup")
		{
			if (!haveARoom)
			{
				return "this step doesn't say where it is: " + line;
			}
		}
		else if (word != "missing" && word != "self-running" && word != "end")
		{
			return "this isn't a kind of line: " + line;
		}

		haveARoom = false;
	}

	return haveARoom ? "the last room line has no step after it" : "";
}

// The real files, as a run will read them. This shows they're well made and agree with each other, not that they're right about the levels.
static void TestTheRealFiles()
{
	puts("the room lists and chains in tools/hudson/chains, read from the top of the repository");

	static char const *const levels[] =
	{
		"derelict", "genshd1", "invasion", "odobenus", "sulaco", "hangar", "temple_m", "vaults_m", "nost03_m", "stat101_m", "fall_m",
		"fall", "area52", "vaults", "furyall", "caverns", "battle", "invasion_p", "escape_p", "temple_p", "breakout_p", "sulaco_p",
		"temple", "escape", "nost03", "stat101", "breakout", "invasion_a", "derelict_a", "sulaco_a", "caverns_a", "furyall_a"
	};

	for (size_t i = 0; i < sizeof(levels) / sizeof(levels[0]); i++)
	{
		std::string const path = std::string("tools/hudson/chains/") + levels[i];
		std::string rooms, chain, whyNot;
		CoopRoomList list;

		Check(ReadWholeFile(path + ".rooms", &rooms), (path + ".rooms is there").c_str());
		Check(ReadWholeFile(path + ".txt", &chain), (path + ".txt is there").c_str());

		bool const read = CoopReach_ReadList(rooms, &list, &whyNot);

		Check(read && !list.names.empty(), (std::string(levels[i]) + "'s room list reads fine, and isn't empty").c_str());

		if (!read)
		{
			printf("          it said: %s\n", whyNot.c_str());
		}

		std::string const wrong = WhatsWrongWithChain(chain, list);

		Check(wrong.empty(), (std::string(levels[i]) + "'s chain only names rooms on that list, and every step says where").c_str());

		if (!wrong.empty())
		{
			printf("          it said: %s\n", wrong.c_str());
		}
	}
}

int main()
{
	TestReadingAFile();
	TestRefusingAFile();
	TestDoors();
	TestLifts();
	TestGuesses();
	TestLockedLifts();
	TestNames();
	TestTheRealFiles();

	if (checksFailed)
	{
		printf("%d checks failed\n", checksFailed);
		return 1;
	}

	puts("The reach test passed");
	return 0;
}
