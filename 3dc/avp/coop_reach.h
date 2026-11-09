// Can a player get to this room from where the players are, through doors that are open right now?
//
// WHY THIS EXISTS
//
// The co-op walking test sets off each step of a level's chain from wherever the player happens to stand. A shut door can't
// stop it, so a level can "end" that no player could ever finish. This is the check that makes it harder: before a step
// counts, its room has to be reachable from the players, through doors open at that moment, on that machine.
//
// WHAT IT PROVES, AND WHAT IT DOESN'T
//
// It says "a route exists through open doors". It never says a player walked there. It only knows which rooms join, so a
// ledge too high to climb, a door that shuts too fast or a gap too narrow are all things only a real walk can show.
//
// HOW IT THINKS
//
// A level is a set of rooms, and some rooms are doors. A room is reached once a player has stood in it, or once it joins a
// reached room and isn't a shut door. Reached rooms stay reached, because someone who got through a door before it shut is
// still through it. That list of rooms comes from tools/hudson/chains/<level>.rooms, and its README gives the format.
//
// Each join says how we know about it. The level file listing a join, or a lift linking two floors, is firm. Two rooms that
// touch, or two that can see each other, are guesses from where they are, because a window also lets you see. So this keeps,
// for each room, the weakest link on the best way there, and an answer that rests on a guess says which.
//
// A platform lift isn't a room in the level file, so it can't be on this list as a door. Some start locked, and one of those
// could be all that stops a player, where this would still say a route exists. So the list names them, and a level with any
// is said to depend on a lift: whatever its chain does, only someone really walking it can show a player gets through.
//
// That file comes from outside the code, so everything in it is checked before it's used. There's no game code in here, so
// a test can run all of it: hudson_strategies.cpp is what asks the game about its real doors and players.

#ifndef COOP_REACH_H
#define COOP_REACH_H

#include <stddef.h>
#include <map>
#include <string>
#include <vector>

enum CoopJoinKind
{
	kCoopJoinFile,      // the level file says these two rooms join
	kCoopJoinLift,      // a lift links them
	kCoopJoinTouching,  // a guess: their boxes touch
	kCoopJoinSees       // a guess: one is on the list of what the other can see
};

// How much a way to a room can be trusted. Lower is better. A way is only as good as its weakest link.
static const int kCoopFirm = 0;
static const int kCoopFairGuess = 1;
static const int kCoopWeakGuess = 2;
static const int kCoopNotReached = 3;

// Limits on the file. Far more than any level has: the biggest of the 32 stock levels has under 1,000 lines.
static const size_t kCoopMostBytes = 2 * 1024 * 1024;
static const size_t kCoopLongestLine = 400;
static const size_t kCoopLongestName = 80;
static const size_t kCoopMostRooms = 4096;
static const size_t kCoopMostJoins = 65536;
static const int kCoopMostLifts = 256;
static const size_t kCoopMostLockedLifts = 256;

struct CoopJoin
{
	int room;
	CoopJoinKind kind;
};

struct CoopLiftSwitch
{
	int room;
	bool onlyNear; // the switch isn't in this room, only within 2 m of it
};

struct CoopLift
{
	std::vector<int> doors;
	std::vector<CoopLiftSwitch> switches;
};

struct CoopRoomList
{
	std::vector<std::string> names;
	std::vector<std::string> doorKinds; // for each room: empty if it isn't a door
	std::vector<std::vector<CoopJoin> > joins;
	std::map<int, CoopLift> lifts;
	std::vector<std::string> lockedLifts; // platform lifts that start locked. They aren't rooms, so they're only names
	std::map<std::string, int> numbers; // from a name to the room's place in these lists. Names are exact, because two rooms in a level can differ only by case
	size_t joinCount;

	CoopRoomList() : joinCount(0) {}
};

// Splits "one | two | three" into its parts.
inline std::vector<std::string> CoopReach_SplitNames(std::string const &text)
{
	std::vector<std::string> parts;
	size_t from = 0;

	for (;;)
	{
		size_t const bar = text.find(" | ", from);

		if (bar == std::string::npos)
		{
			parts.push_back(text.substr(from));
			return parts;
		}

		parts.push_back(text.substr(from, bar - from));
		from = bar + 3;
	}
}

// A name goes in the log, so it has to be something that can't make a log line look like something else.
inline bool CoopReach_NameIsFine(std::string const &name)
{
	if (name.empty() || name.size() > kCoopLongestName)
	{
		return false;
	}

	for (size_t i = 0; i < name.size(); i++)
	{
		if (name[i] < ' ' || name[i] > '~')
		{
			return false;
		}
	}

	return true;
}

inline int CoopReach_RoomNumber(CoopRoomList const &list, std::string const &name)
{
	const auto found = list.numbers.find(name);

	return found == list.numbers.end() ? -1 : found->second;
}

// The room with this name, making it if it's new. -1 if there's no room for any more.
inline int CoopReach_RoomForName(CoopRoomList *list, std::string const &name)
{
	int const known = CoopReach_RoomNumber(*list, name);

	if (known >= 0)
	{
		return known;
	}

	if (list->names.size() >= kCoopMostRooms)
	{
		return -1;
	}

	int const number = (int)list->names.size();

	list->names.push_back(name);
	list->doorKinds.push_back(std::string());
	list->joins.push_back(std::vector<CoopJoin>());
	list->numbers[name] = number;

	return number;
}

// Reads a lift's number, from 1 up. False if it's anything else, such as a huge number that wouldn't fit. A blank one comes
// out as 0, which isn't a lift's number either.
inline bool CoopReach_ReadLiftNumber(std::string const &word, int *number)
{
	if (word.size() > 3)
	{
		return false;
	}

	int value = 0;

	for (size_t i = 0; i < word.size(); i++)
	{
		if (word[i] < '0' || word[i] > '9')
		{
			return false;
		}

		value = value * 10 + (word[i] - '0');
	}

	if (value < 1 || value > kCoopMostLifts)
	{
		return false;
	}

	*number = value;
	return true;
}

// Reads one line. Gives back an empty text if it's fine, or else why it isn't.
inline std::string CoopReach_ReadLine(std::string const &line, CoopRoomList *list)
{
	if (line.size() > kCoopLongestLine)
	{
		return "the line is too long";
	}

	if (line.empty() || line[0] == '#')
	{
		return std::string();
	}

	size_t const space = line.find(' ');

	if (space == std::string::npos)
	{
		return "a line needs a word and what it's about";
	}

	std::string const word = line.substr(0, space);
	std::vector<std::string> const parts = CoopReach_SplitNames(line.substr(space + 1));

	if (word == "lockedlift")
	{
		if (parts.size() != 1)
		{
			return "a locked lift line needs one lift";
		}

		if (!CoopReach_NameIsFine(parts[0]))
		{
			return "that can't be a lift's name";
		}

		if (list->lockedLifts.size() >= kCoopMostLockedLifts)
		{
			return "too many locked lifts";
		}

		list->lockedLifts.push_back(parts[0]);
		return std::string();
	}

	if (word == "room")
	{
		if (parts.size() != 1)
		{
			return "a room line needs one room";
		}

		if (!CoopReach_NameIsFine(parts[0]))
		{
			return "that can't be a room's name";
		}

		if (CoopReach_RoomForName(list, parts[0]) < 0)
		{
			return "too many rooms";
		}

		return std::string();
	}

	if (word == "door")
	{
		if (parts.size() != 2)
		{
			return "a door line needs a room and its kind";
		}

		if (parts[1] != "proximity" && parts[1] != "switch" && parts[1] != "lift" && parts[1] != "platform")
		{
			return "that isn't a kind of door";
		}

		if (!CoopReach_NameIsFine(parts[0]))
		{
			return "that can't be a room's name";
		}

		int const room = CoopReach_RoomForName(list, parts[0]);

		if (room < 0)
		{
			return "too many rooms";
		}

		if (!list->doorKinds[room].empty())
		{
			return "that door is already listed";
		}

		list->doorKinds[room] = parts[1];
		return std::string();
	}

	if (word == "join")
	{
		if (parts.size() != 3)
		{
			return "a join line needs two rooms and how they join";
		}

		CoopJoinKind kind;

		if (parts[2] == "file")
		{
			kind = kCoopJoinFile;
		}
		else if (parts[2] == "lift")
		{
			kind = kCoopJoinLift;
		}
		else if (parts[2] == "touching")
		{
			kind = kCoopJoinTouching;
		}
		else if (parts[2] == "sees")
		{
			kind = kCoopJoinSees;
		}
		else
		{
			return "that isn't a way for rooms to join";
		}

		if (!CoopReach_NameIsFine(parts[0]) || !CoopReach_NameIsFine(parts[1]))
		{
			return "that can't be a room's name";
		}

		if (list->joinCount >= kCoopMostJoins)
		{
			return "too many joins";
		}

		int const a = CoopReach_RoomForName(list, parts[0]);
		int const b = CoopReach_RoomForName(list, parts[1]);

		if (a < 0 || b < 0)
		{
			return "too many rooms";
		}

		if (a == b)
		{
			return "a room can't join itself";
		}

		CoopJoin const toB = { b, kind };
		CoopJoin const toA = { a, kind };

		// You can walk a join either way, because often only one of the two rooms lists it
		list->joins[a].push_back(toB);
		list->joins[b].push_back(toA);
		list->joinCount++;
		return std::string();
	}

	if (word == "liftdoor" || word == "liftswitch")
	{
		bool const isSwitch = word == "liftswitch";

		if (parts.size() != 2 && !(isSwitch && parts.size() == 3))
		{
			return "a lift line needs a lift's number and a room";
		}

		if (parts.size() == 3 && parts[2] != "near")
		{
			return "the only word after a lift switch's room is near";
		}

		int number = 0;

		if (!CoopReach_ReadLiftNumber(parts[0], &number))
		{
			return "that isn't a lift's number";
		}

		if (!CoopReach_NameIsFine(parts[1]))
		{
			return "that can't be a room's name";
		}

		int const room = CoopReach_RoomForName(list, parts[1]);

		if (room < 0)
		{
			return "too many rooms";
		}

		CoopLift &lift = list->lifts[number];

		if (isSwitch)
		{
			CoopLiftSwitch const liftSwitch = { room, parts.size() == 3 };
			lift.switches.push_back(liftSwitch);
		}
		else
		{
			lift.doors.push_back(room);
		}

		return std::string();
	}

	return "this file doesn't use that word";
}

// Reads the whole file. A file with anything wrong in it is refused whole, and whyNot says which line and why, because a
// list with a line skipped could say a door isn't in the way when it is.
inline bool CoopReach_ReadList(std::string const &text, CoopRoomList *list, std::string *whyNot)
{
	*list = CoopRoomList();

	if (text.size() > kCoopMostBytes)
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

		while (!line.empty() && (line[line.size() - 1] == '\r' || line[line.size() - 1] == ' '))
		{
			line.erase(line.size() - 1);
		}

		std::string const problem = CoopReach_ReadLine(line, list);

		if (!problem.empty())
		{
			*whyNot = "line " + std::to_string(lineNumber) + ": " + problem;
			return false;
		}
	}

	// A lift's door has to be a door, or the rule that a lift opens it would mean nothing
	for (std::map<int, CoopLift>::const_iterator lift = list->lifts.begin(); lift != list->lifts.end(); ++lift)
	{
		for (size_t i = 0; i < lift->second.doors.size(); i++)
		{
			int const door = lift->second.doors[i];

			if (list->doorKinds[door].empty())
			{
				*whyNot = "lift " + std::to_string(lift->first) + " has a door that isn't listed as a door: " + list->names[door];
				return false;
			}
		}
	}

	return true;
}

// What's been worked out so far on one machine. Rooms stay reached from one call to the next.
struct CoopReach
{
	std::vector<int> weakest;       // for each room: the weakest link on the best way there, or kCoopNotReached
	std::vector<int> cameFrom;      // the room before it on that way, or -1 where a player really stood
	std::vector<int> cameBy;        // the kind of join used to get in, or -1 for none
	std::vector<char> viaNearLift;  // true if it's a door that counts as open only because of a lift's switch within 2 m
};

inline void CoopReach_Begin(CoopRoomList const &list, CoopReach *reach)
{
	size_t const rooms = list.names.size();

	reach->weakest.assign(rooms, kCoopNotReached);
	reach->cameFrom.assign(rooms, -1);
	reach->cameBy.assign(rooms, -1);
	reach->viaNearLift.assign(rooms, 0);
}

// A player who isn't in any room on the list has the number -1.
inline bool CoopReach_IsARoom(CoopRoomList const &list, int room)
{
	return room >= 0 && (size_t)room < list.names.size();
}

inline int CoopReach_WeaknessOfJoin(CoopJoinKind kind)
{
	switch (kind)
	{
		case kCoopJoinTouching: return kCoopFairGuess;
		case kCoopJoinSees: return kCoopWeakGuess;
		default: return kCoopFirm;
	}
}

// Spreads out from every reached room as far as doors allow. doorIsOpen has one place for each room, and playerRooms says
// where the players are on this machine, by room number. Both come from this machine's own game, so a door open on the host
// and shut on the other machine gives different answers, and that difference is one of the bugs this exists to show.
inline void CoopReach_Spread(CoopRoomList const &list, std::vector<char> const &doorIsOpen, std::vector<int> const &playerRooms, CoopReach *reach)
{
	size_t const rooms = list.names.size();

	for (size_t i = 0; i < playerRooms.size(); i++)
	{
		int const room = playerRooms[i];

		if (CoopReach_IsARoom(list, room))
		{
			// Someone is really standing here, which beats anything a guess could say
			reach->weakest[room] = kCoopFirm;
			reach->cameFrom[room] = -1;
			reach->cameBy[room] = -1;
			reach->viaNearLift[room] = 0;
		}
	}

	// For each door, how much a working lift lets us count it as open
	std::vector<int> openedByLift(rooms, kCoopNotReached);
	std::vector<char> nearSwitchOnly(rooms, 0);

	for (;;)
	{
		std::vector<int> toDo;

		for (size_t room = 0; room < rooms; room++)
		{
			if (reach->weakest[room] < kCoopNotReached)
			{
				toDo.push_back((int)room);
			}
		}

		while (!toDo.empty())
		{
			int const from = toDo.back();
			toDo.pop_back();

			for (size_t i = 0; i < list.joins[from].size(); i++)
			{
				CoopJoin const &join = list.joins[from][i];
				int const to = join.room;
				bool const isShutDoor = !list.doorKinds[to].empty() && !doorIsOpen[to];

				// A shut door no lift opens costs kCoopNotReached, so the way through it can't beat anything
				int const costOfTheDoor = isShutDoor ? openedByLift[to] : kCoopFirm;
				int weakness = reach->weakest[from];

				if (CoopReach_WeaknessOfJoin(join.kind) > weakness)
				{
					weakness = CoopReach_WeaknessOfJoin(join.kind);
				}

				if (costOfTheDoor > weakness)
				{
					weakness = costOfTheDoor;
				}

				// Only a strictly better way replaces the one we have, which is also why this always finishes
				if (weakness < reach->weakest[to])
				{
					reach->weakest[to] = weakness;
					reach->cameFrom[to] = from;
					reach->cameBy[to] = join.kind;
					reach->viaNearLift[to] = isShutDoor && nearSwitchOnly[to];
					toDo.push_back(to);
				}
			}
		}

		// A lift works once a player can reach one of its switches, and then it opens its own doors when it arrives
		bool liftOpenedMore = false;

		for (std::map<int, CoopLift>::const_iterator lift = list.lifts.begin(); lift != list.lifts.end(); ++lift)
		{
			int best = kCoopNotReached;
			bool bestIsNear = false;

			for (size_t i = 0; i < lift->second.switches.size(); i++)
			{
				CoopLiftSwitch const &liftSwitch = lift->second.switches[i];
				int weakness = reach->weakest[liftSwitch.room];

				if (liftSwitch.onlyNear && weakness < kCoopFairGuess)
				{
					weakness = kCoopFairGuess;
				}

				if (weakness < best)
				{
					best = weakness;
					bestIsNear = liftSwitch.onlyNear;
				}
			}

			for (size_t i = 0; i < lift->second.doors.size(); i++)
			{
				int const door = lift->second.doors[i];

				if (best < openedByLift[door])
				{
					openedByLift[door] = best;
					nearSwitchOnly[door] = bestIsNear;
					liftOpenedMore = true;
				}
			}
		}

		if (!liftOpenedMore)
		{
			return;
		}
	}
}

struct CoopReachAnswer
{
	bool routeExists;
	int room;                          // the room the answer is about, or -1
	bool onlyNear;                     // the thing isn't in that room, only within 2 m of it
	int shutDoor;                      // the first shut door on the way, or -1
	std::vector<std::string> restsOn;  // every guess the route depends on, in words

	CoopReachAnswer() : routeExists(false), room(-1), onlyNear(false), shutDoor(-1) {}
};

// The best reached room of these, or -1.
inline int CoopReach_BestReached(CoopRoomList const &list, CoopReach const &reach, std::vector<std::string> const &names)
{
	int best = -1;

	for (size_t i = 0; i < names.size(); i++)
	{
		int const room = CoopReach_RoomNumber(list, names[i]);

		if (room >= 0 && reach.weakest[room] < kCoopNotReached && (best < 0 || reach.weakest[room] < reach.weakest[best]))
		{
			best = room;
		}
	}

	return best;
}

// Goes back along the way to a room and collects every guess on it.
inline std::vector<std::string> CoopReach_GuessesOnTheWay(CoopRoomList const &list, CoopReach const &reach, int room)
{
	std::vector<std::string> guesses;

	// This can't go round in a circle. A room's way goes through a room reached at least as firmly, and a room only takes a
	// new way if it's strictly better, so a way can never lead back to a room on itself.
	while (room >= 0)
	{
		int const from = reach.cameFrom[room];

		if (reach.viaNearLift[room])
		{
			guesses.push_back("a lift's switch that's only near a room, for door " + list.names[room]);
		}

		if (from >= 0 && reach.cameBy[room] == kCoopJoinTouching)
		{
			guesses.push_back("rooms that touch, " + list.names[from] + " and " + list.names[room]);
		}
		else if (from >= 0 && reach.cameBy[room] == kCoopJoinSees)
		{
			guesses.push_back("rooms that can see each other, " + list.names[from] + " and " + list.names[room]);
		}

		room = from;
	}

	return guesses;
}

// With no route, the shut door that's stopping it: the first room out of the reached ones, on the shortest way to any of
// these rooms that ignores whether doors are shut. It has to be a shut door, or spreading would have gone through it.
inline int CoopReach_FirstShutDoor(CoopRoomList const &list, CoopReach const &reach, std::vector<int> const &targets)
{
	size_t const rooms = list.names.size();
	std::vector<int> cameFrom(rooms, -2); // -2 is not seen yet, -1 is reached already
	std::vector<int> queue;

	for (size_t room = 0; room < rooms; room++)
	{
		if (reach.weakest[room] < kCoopNotReached)
		{
			cameFrom[room] = -1;
			queue.push_back((int)room);
		}
	}

	std::vector<char> isTarget(rooms, 0);

	for (size_t i = 0; i < targets.size(); i++)
	{
		isTarget[targets[i]] = 1;
	}

	for (size_t at = 0; at < queue.size(); at++)
	{
		int const room = queue[at];

		if (isTarget[room])
		{
			int door = room;

			while (cameFrom[door] >= 0 && reach.weakest[cameFrom[door]] >= kCoopNotReached)
			{
				door = cameFrom[door];
			}

			return door;
		}

		for (size_t i = 0; i < list.joins[room].size(); i++)
		{
			int const next = list.joins[room][i].room;

			if (cameFrom[next] == -2)
			{
				cameFrom[next] = room;
				queue.push_back(next);
			}
		}
	}

	return -1;
}

// Is there a route to any of these rooms? "where" are the rooms the thing is in, and "nearRooms" are rooms it's only within 2 m
// of, which count less. A name not on the list can't be reached.
inline CoopReachAnswer CoopReach_Ask(CoopRoomList const &list, CoopReach const &reach, std::vector<std::string> const &where, std::vector<std::string> const &nearRooms)
{
	CoopReachAnswer answer;
	int room = CoopReach_BestReached(list, reach, where);

	if (room < 0)
	{
		room = CoopReach_BestReached(list, reach, nearRooms);
		answer.onlyNear = room >= 0;
	}

	if (room >= 0)
	{
		answer.routeExists = true;
		answer.room = room;
		answer.restsOn = CoopReach_GuessesOnTheWay(list, reach, room);
		return answer;
	}

	std::vector<int> targets;

	for (size_t i = 0; i < where.size(); i++)
	{
		int const number = CoopReach_RoomNumber(list, where[i]);

		if (number >= 0)
		{
			targets.push_back(number);
		}
	}

	for (size_t i = 0; i < nearRooms.size(); i++)
	{
		int const number = CoopReach_RoomNumber(list, nearRooms[i]);

		if (number >= 0)
		{
			targets.push_back(number);
		}
	}

	answer.shutDoor = CoopReach_FirstShutDoor(list, reach, targets);
	return answer;
}

// The answer as the log says it. The report quotes these words, so they say what was checked and no more.
inline std::string CoopReach_InWords(CoopRoomList const &list, CoopReachAnswer const &answer)
{
	if (answer.routeExists)
	{
		std::string words = "a route exists through open doors";

		if (answer.onlyNear)
		{
			words += ", to within 2 m of it, from room " + list.names[answer.room];
		}

		for (size_t i = 0; i < answer.restsOn.size(); i++)
		{
			words += (i == 0 ? ", resting on a guess: " : "; ") + answer.restsOn[i];
		}

		return words;
	}

	if (answer.shutDoor >= 0)
	{
		return "behind shut door " + list.names[answer.shutDoor];
	}

	return "nothing in the room list joins it to where the players have been";
}

// What the log says about a level with lifts that start locked, or nothing if it has none. The report counts a level with this
// line as depending on a lift, and not as ended, whatever its chain did.
inline std::string CoopReach_LiftWords(CoopRoomList const &list)
{
	std::string words;

	for (size_t i = 0; i < list.lockedLifts.size(); i++)
	{
		words += (i == 0 ? "depends on a lift that starts locked, so a route doesn't show a player can get there: " : ", ") + list.lockedLifts[i];
	}

	return words;
}

#endif
