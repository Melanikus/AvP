// The test of the co-op campaign lobby's rules. Written 2026-11-07 by Melanikus for the AvP port. Credit: Melanikus and Bobblen (the co-op campaign
// system follows Bobblen's avp-forever, rewritten).
//
// WHAT IT TESTS
//
// 3dc/coop_lobby.h and the walk over a message's parts in 3dc/net_checks.h:
// - the host's choice, each number good and bad, each bad one refused for its own reason;
// - a choice's number, which only goes forward, and stops at the start;
// - a joiner's answer about the map;
// - the short lists of what the lobby takes, against every kind of message there is, in both directions. The kinds are read from the
//   game's own header, so a kind added later is refused until someone decides otherwise and changes the list here too.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this with the settings in tests/coop_lobby_test.args, and the program gives back a non-zero result if any
// check failed. It needs no game files. tests/takeouts/coop_lobby_test.json proves each check can fail.

// Modified 2026-11-08 by Melanikus for the AvP port:
//   its Between is the shared one in source_slice.h, so a marker that isn't there is a failed check of its own (SOURCE-SLICE-1, #128)
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "net_checks.h"
#include "coop_lobby.h"
#include "source_slice.h"

static int checksFailed = 0;

static void Check(bool good, const std::string &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// A good choice of the Predator's campaign, or of a custom map
static NETMESSAGE_COOPCAMPAIGNCHOICE StockChoice()
{
	NETMESSAGE_COOPCAMPAIGNCHOICE choice;

	memset(&choice, 0, sizeof(choice));
	choice.number = 5;
	choice.campaign = 2;
	choice.place = 10;
	choice.species = 1;
	choice.difficulty = 2;
	choice.keepKit = 1;
	choice.friendlyFireOff = 1;
	choice.started = 1;
	choice.rollNumber = -12345;

	return choice;
}

static NETMESSAGE_COOPCAMPAIGNCHOICE CustomChoice()
{
	NETMESSAGE_COOPCAMPAIGNCHOICE choice = StockChoice();

	choice.campaign = kCoopChoiceCustomCampaign;
	choice.place = 0;
	choice.species = 2;
	strcpy(choice.mapName, "My_map (2)");
	choice.mapSize = 12345;
	choice.mapSum[3] = 9;

	return choice;
}

static const int kLevelsInAStockCampaign = 11;

static const char *Why(NETMESSAGE_COOPCAMPAIGNCHOICE const &choice)
{
	return CoopChoice_WhyRefused(choice, choice.campaign == kCoopChoiceCustomCampaign ? 1 : kLevelsInAStockCampaign);
}

static bool Refused(NETMESSAGE_COOPCAMPAIGNCHOICE const &choice, const char *reason)
{
	return Why(choice) == reason;
}

static void TestTheChoice()
{
	puts("The host's choice:");

	Check(Why(StockChoice()) == 0, "a good choice of a stock campaign is taken, with each number at its most");
	Check(Why(CustomChoice()) == 0, "and so is a good choice of a custom map");

	for (int campaign = 1; campaign <= 3; campaign++)
	{
		NETMESSAGE_COOPCAMPAIGNCHOICE choice = StockChoice();

		choice.campaign = (unsigned char)campaign;
		choice.species = (unsigned char)(campaign - 1);
		Check(Why(choice) == 0, "stock campaign " + std::to_string(campaign) + " is a campaign");
	}

	NETMESSAGE_COOPCAMPAIGNCHOICE choice = StockChoice();

	choice.campaign = 0;
	Check(Refused(choice, kCoopRefusedCampaign), "no campaign at all isn't a campaign");
	choice.campaign = 5;
	Check(Refused(choice, kCoopRefusedCampaign), "and neither is the number after the last");

	choice = StockChoice();
	choice.place = kLevelsInAStockCampaign;
	Check(Refused(choice, kCoopRefusedPlace), "the level after a campaign's last isn't one of its levels");
	choice.place = kLevelsInAStockCampaign - 1;
	Check(Why(choice) == 0, "and its last is");

	choice = CustomChoice();
	choice.place = 1;
	Check(Refused(choice, kCoopRefusedPlace), "a custom map has one level");
	Check(CoopChoice_WhyRefused(StockChoice(), 0) == kCoopRefusedPlace, "a campaign with no levels has no level to choose");

	choice = CustomChoice();
	choice.species = 3;
	Check(Refused(choice, kCoopRefusedSpecies), "a species past the last isn't one");

	choice = StockChoice();
	choice.species = 0;
	Check(Refused(choice, kCoopRefusedCampaignSpecies), "a stock campaign plays its own species");
	choice = CustomChoice();
	choice.species = 0;
	Check(Why(choice) == 0, "and a custom map can be played as any");

	choice = StockChoice();
	choice.difficulty = 3;
	Check(Refused(choice, kCoopRefusedDifficulty), "a difficulty past the last isn't one");

	choice = StockChoice();
	choice.keepKit = 2;
	Check(Refused(choice, kCoopRefusedKeepKit), "the kit rule is yes or no");

	choice = StockChoice();
	choice.friendlyFireOff = 2;
	Check(Refused(choice, kCoopRefusedFriendlyFire), "friendly fire is yes or no");

	choice = StockChoice();
	choice.started = 2;
	Check(Refused(choice, kCoopRefusedStarted), "started is yes or no");

	puts("A custom map's name:");

	choice = CustomChoice();
	memset(choice.mapName, 'a', sizeof(choice.mapName));
	Check(Refused(choice, kCoopRefusedNameHasNoEnd), "a name that fills its field with no end is refused");

	choice = CustomChoice();
	memset(choice.mapName, 0, sizeof(choice.mapName));
	memset(choice.mapName, 'a', sizeof(choice.mapName) - 1);
	Check(Why(choice) == 0, "the longest name there can be is taken");

	char const *const badNames[] = { "", "..", "..\\Maps", "a/b", "C:\\x", "a.rif", " a", "a ", "CON", "%s" };

	for (char const *const name : badNames)
	{
		choice = CustomChoice();
		memset(choice.mapName, 0, sizeof(choice.mapName));
		snprintf(choice.mapName, sizeof(choice.mapName), "%s", name);
		Check(Refused(choice, kCoopRefusedName), std::string("the name \"") + name + "\" breaks the rule");
	}

	puts("A stock campaign carries no map:");

	choice = StockChoice();
	choice.mapName[0] = 'a';
	Check(Refused(choice, kCoopRefusedStockHasAMap), "not a name");
	choice = StockChoice();
	choice.mapName[sizeof(choice.mapName) - 1] = 'a';
	Check(Refused(choice, kCoopRefusedStockHasAMap), "nor anything hidden after an end marker");
	choice = StockChoice();
	choice.mapSize = 1;
	Check(Refused(choice, kCoopRefusedStockHasAMap), "not a size");
	choice = StockChoice();
	choice.mapSum[kCoopMapSumBytes - 1] = 1;
	Check(Refused(choice, kCoopRefusedStockHasAMap), "not a sum");
}

static void TestTheNumbers()
{
	puts("A choice's number:");

	Check(CoopChoice_Step(0, false, 0, false) == kCoopChoiceTakeIt, "the first after joining is taken, even number 0");
	Check(CoopChoice_Step(7, false, 0, false) == kCoopChoiceTakeIt, "whatever number that is");
	Check(CoopChoice_Step(8, true, 7, false) == kCoopChoiceTakeIt, "a higher number is taken");
	Check(CoopChoice_Step(7, true, 7, false) == kCoopChoiceIsARepeat, "the same number is a repeat");
	Check(CoopChoice_Step(6, true, 7, false) == kCoopChoiceIsOlder, "a lower number is older");
	Check(CoopChoice_Step(7, true, 7, true) == kCoopChoiceIsARepeat, "after the start, a repeat of that same choice is still only a repeat");
	Check(CoopChoice_Step(8, true, 7, true) == kCoopChoiceIsAfterTheStart, "and a higher number isn't taken");
	Check(CoopChoice_Step(6, true, 7, true) == kCoopChoiceIsAfterTheStart, "and nor is a lower one");
}

static void TestTheMapAnswer()
{
	puts("A joiner's answer about the map:");

	unsigned char mine[kCoopMapSumBytes] = { 1, 2, 3 };
	unsigned char same[kCoopMapSumBytes] = { 1, 2, 3 };
	unsigned char other[kCoopMapSumBytes] = { 1, 2, 3 };

	other[kCoopMapSumBytes - 1] = 1;

	Check(CoopMap_Answer(true, 100, mine, 100, same) == kCoopMapIsHere, "the same size and sum: it's here");
	Check(CoopMap_Answer(false, 100, mine, 100, same) == kCoopMapIsMissing, "no such map on this machine: missing");
	Check(CoopMap_Answer(true, 100, mine, 101, same) == kCoopMapIsDifferent, "another size: different");
	Check(CoopMap_Answer(true, 100, mine, 100, other) == kCoopMapIsDifferent, "another sum, down to the last byte: different");

	NETMESSAGE_COOPMAPANSWER answer;

	answer.number = 4;
	answer.answer = kCoopMapIsDifferent;
	Check(CoopAnswer_WhyRefused(answer, true, true, 4) == 0, "the host takes an answer to the choice it's on");
	Check(CoopAnswer_WhyRefused(answer, true, true, 9) == 0, "and an honest answer to an earlier one");
	Check(CoopAnswer_WhyRefused(answer, false, true, 4) == kCoopRefusedAnswerNoLobby, "not with no lobby open");
	Check(CoopAnswer_WhyRefused(answer, true, false, 4) == kCoopRefusedAnswerStranger, "not from someone who isn't in the game");
	Check(CoopAnswer_WhyRefused(answer, true, true, 3) == kCoopRefusedAnswerFuture, "not to a choice the host hasn't made");

	answer.answer = 3;
	Check(CoopAnswer_WhyRefused(answer, true, true, 4) == kCoopRefusedAnswerKind, "and not an answer that isn't one of the three");
}

// ---- Walking a message's parts. Kinds 0 to 3, with 1 byte each, and kind 3's length said by its own first byte.

static size_t LengthOfTestPart(unsigned char, const unsigned char *part, size_t)
{
	return part[0];
}

static NetPartRule const kRules[] = {
	{ 0, 1, false, false, 0 },
	{ 1, 1, false, false, 0 },
	{ 2, 0, false, false, 0 },
	{ 3, 1, true, false, 0 }
};

static bool AllOnTheList(std::vector<unsigned char> const &parts, std::vector<unsigned char> const &list, size_t *partCount = 0)
{
	size_t count = 0;
	size_t onTheList = 0;
	bool const walked = NetCountPartsOnTheList(parts.data(), parts.size(), kRules, 4, LengthOfTestPart, list.data(), list.size(), &count, &onTheList);

	if (partCount)
	{
		*partCount = count;
	}

	return walked && onTheList == count;
}

static void TestTheWalk()
{
	puts("Walking the parts of a message:");

	std::vector<unsigned char> const list = { 0, 2 };
	size_t parts = 0;

	Check(AllOnTheList({ 0, 9 }, list), "one part on the list passes");
	Check(!AllOnTheList({ 1, 9 }, list), "one part not on the list doesn't");
	Check(AllOnTheList({ 0, 9, 2, 0, 9 }, list, &parts) && parts == 3, "three parts on the list pass");
	Check(!AllOnTheList({ 0, 9, 2, 1, 9 }, list), "one bad part at the end of three refuses the message");
	Check(!AllOnTheList({ 1, 9, 2, 0, 9 }, list), "and one at the start");
	Check(AllOnTheList({}, list, &parts) && parts == 0, "a message of no parts has nothing to refuse");
	Check(!AllOnTheList({ 1, 2 }, {}), "an empty list takes nothing");
	Check(!AllOnTheList({ 0, 0, 3, 3, 9, 9, 1, 0 }, { 0, 3 }), "a part whose length varies is stepped over properly, to the bad one after it");
	Check(AllOnTheList({ 0, 1, 3, 3, 1, 1 }, { 0, 3 }), "and what's inside it isn't taken for another part");
	Check(AllOnTheList({ 0, 9 }, { 0, 0 }), "a kind named twice on the list still counts its part once");

	size_t count = 0;
	size_t onTheList = 0;
	unsigned char const list2[] = { 0 };
	unsigned char const unknownKind[] = { 4, 0 };
	unsigned char const tooShort[] = { 0 };
	unsigned char const tooLong[] = { 3, 200 };
	unsigned char const noLength[] = { 3, 0 };

	Check(!NetCountPartsOnTheList(unknownKind, sizeof(unknownKind), kRules, 4, LengthOfTestPart, list2, 1, &count, &onTheList), "a kind that doesn't exist can't be walked");
	Check(!NetCountPartsOnTheList(tooShort, sizeof(tooShort), kRules, 4, LengthOfTestPart, list2, 1, &count, &onTheList), "nor a part cut short");
	Check(!NetCountPartsOnTheList(tooLong, sizeof(tooLong), kRules, 4, LengthOfTestPart, list2, 1, &count, &onTheList), "nor one saying it's longer than the message");
	Check(!NetCountPartsOnTheList(noLength, sizeof(noLength), kRules, 4, LengthOfTestPart, list2, 1, &count, &onTheList), "nor one saying it has no length");

	// A table with a row past the count it's given: without the check on the kind, that row would be read, and the part would pass
	NetPartRule const rulesWithOneMore[] = { kRules[0], kRules[1], kRules[2], kRules[3], { 4, 1, false, false, 0 } };

	Check(!NetCountPartsOnTheList(unknownKind, sizeof(unknownKind), rulesWithOneMore, 4, LengthOfTestPart, list2, 1, &count, &onTheList), "a kind past the count it's given isn't walked, even when the table has a row there");
}

static void TestTheRemovedList()
{
	puts("The players a host has removed:");

	CoopRemovedList list;

	memset(&list, 0, sizeof(list));

	Check(!CoopRemoved_Has(list, 7), "nobody is on an empty list");

	CoopRemoved_Add(list, 7);

	Check(CoopRemoved_Has(list, 7), "an address removed is on it");
	Check(!CoopRemoved_Has(list, 8), "and another address isn't");

	CoopRemoved_Add(list, 7);
	Check(list.count == 1, "the same address twice is one place");

	for (unsigned int address = 100; address < 100 + kCoopMostRemoved; address++)
	{
		CoopRemoved_Add(list, address);
	}

	Check(list.count == kCoopMostRemoved, "it never holds more than it has room for");
	Check(!CoopRemoved_Has(list, 7), "when it's full the oldest goes");
	Check(CoopRemoved_Has(list, 100) && CoopRemoved_Has(list, 100 + kCoopMostRemoved - 1), "and the rest stay");

	CoopRemoved_Forget(list);
	Check(!CoopRemoved_Has(list, 100), "forgetting clears it");

	CoopRemoved_Add(list, 9);
	Check(CoopRemoved_Has(list, 9) && list.count == 1, "and it works from the start again");
}

static void TestTheStates()
{
	puts("A game's lobby state, from a search:");

	Check(CoopLobby_StateIsReal(kCoopLobbyNone) && CoopLobby_StateIsReal(kCoopLobbyOpen) && CoopLobby_StateIsReal(kCoopLobbyUnderWay), "the three states are real");
	Check(!CoopLobby_StateIsReal(3) && !CoopLobby_StateIsReal(-1) && !CoopLobby_StateIsReal(255), "and nothing else is");
}

// ---- The game's own files

static std::string ReadSourceFile(const std::string &name)
{
	std::ifstream file(name, std::ios::binary);
	std::stringstream all;

	all << file.rdbuf();

	std::string const text = all.str();
	std::string withoutReturns;

	for (size_t i = 0; i < text.size(); i++)
	{
		if (text[i] != '\r')
		{
			withoutReturns += text[i];
		}
	}

	if (withoutReturns.empty())
	{
		printf("  FAILED  the test couldn't read %s\n", name.c_str());
		checksFailed++;
	}

	return withoutReturns;
}

static bool Has(const std::string &text, const std::string &what)
{
	return text.find(what) != std::string::npos;
}

// The text from one marker up to the next (source_slice.h). A marker that isn't there is a failed check of its own, named by the marker.
static std::string Between(const std::string &text, const std::string &from, const std::string &to)
{
	return SourceSlice(text, from, to, kSliceEndAfterTheStart, [](const std::string &why) { Check(false, why.c_str()); });
}

// Every NetMT_ name in this text, in order
static std::vector<std::string> KindsIn(const std::string &text)
{
	std::vector<std::string> kinds;
	std::string const start = "NetMT_";
	size_t at = 0;

	while ((at = text.find(start, at)) != std::string::npos)
	{
		size_t end = at + start.size();

		while (end < text.size() && (isalnum((unsigned char)text[end]) || text[end] == '_'))
		{
			end++;
		}

		kinds.push_back(text.substr(at, end - at));
		at = end;
	}

	return kinds;
}

// A list against what the test says it has to be, and every kind there is against that list, through the walk the game uses
static void CheckAList(std::vector<std::string> const &allKinds, const std::string &pldnet, const std::string &name, std::vector<std::string> expected, const std::string &who)
{
	std::vector<std::string> onTheList = KindsIn(Between(pldnet, "static const unsigned char " + name + "[] = {", "};"));

	Check(onTheList == expected, who + ": the list is exactly the kinds this test names. Adding one has to be decided here too");

	std::vector<NetPartRule> rules(allKinds.size());
	std::vector<unsigned char> list;

	for (size_t kind = 0; kind < allKinds.size(); kind++)
	{
		rules[kind].kind = (unsigned char)kind;

		if (std::find(onTheList.begin(), onTheList.end(), allKinds[kind]) != onTheList.end())
		{
			list.push_back((unsigned char)kind);
		}
	}

	Check(list.size() == expected.size(), who + ": each kind on the list is a kind there is");

	int refused = 0;
	int taken = 0;

	for (size_t kind = 0; kind < allKinds.size(); kind++)
	{
		unsigned char const part[] = { (unsigned char)kind };
		size_t count = 0;
		size_t onIt = 0;
		bool const walked = NetCountPartsOnTheList(part, 1, rules.data(), rules.size(), 0, list.data(), list.size(), &count, &onIt);

		if (walked && count == 1 && onIt == 1)
		{
			taken++;
		}
		else if (walked && count == 1 && onIt == 0)
		{
			refused++;
		}
	}

	Check(taken == (int)expected.size(), who + ": those are taken");
	Check(refused == (int)allKinds.size() - (int)expected.size(), who + ": and each of the other " + std::to_string(allKinds.size() - expected.size()) + " kinds is refused");
}

static void TestTheLists()
{
	puts("What the lobby takes, against every kind of message there is:");

	std::string const header = ReadSourceFile("3dc/avp/win95/pldnet.h");
	std::string const pldnet = ReadSourceFile("3dc/avp/win95/pldnet.cpp");
	std::vector<std::string> const allKinds = KindsIn(Between(header, "typedef enum", "} NETMESSAGETYPE;"));

	Check(allKinds.size() > 50, "the kinds were read from the game's header");
	Check((int)KindsIn(Between(pldnet, "static constexpr NetPartRule kNetPartRules[] =", "};")).size() == (int)allKinds.size(), "and the game's table has a row for each");

	CheckAList(allKinds, pldnet, "kCoopLobbyFromAJoiner", { "NetMT_PlayerDescription", "NetMT_PlayerLeaving", "NetMT_CoopMapAnswer" }, "from a joiner");
	CheckAList(allKinds, pldnet, "kCoopLobbyFromTheHost", { "NetMT_GameDescription", "NetMT_PlayerLeaving", "NetMT_CoopCampaignChoice" }, "from the host");
}

static void TestTheGameUsesIt()
{
	puts("The game's own files:");

	std::string const pldnet = ReadSourceFile("3dc/avp/win95/pldnet.cpp");
	std::string const networking = ReadSourceFile("3dc/networking.cpp");
	std::string const isGood = Between(pldnet, "bool Net_GameMessageIsGood(const uint8_t *message, size_t size, bool fromTheHost)\n{", "\n}\n");

	Check(Has(isGood, "case kNetPartsGood:\n\t\t\treturn Net_CoopLobbyTakesThis(message, size);"), "everything that arrives goes past the lobby's lists, once it's shown to be well made");
	Check(Has(pldnet, "{ NetMT_CoopCampaignChoice, sizeof(NETMESSAGE_COOPCAMPAIGNCHOICE), false, true },"), "only a host can say what's chosen");
	Check(Has(pldnet, "{ NetMT_CoopMapAnswer, sizeof(NETMESSAGE_COOPMAPANSWER), false, false, kMostMapAnswersEachSecond }"), "and a joiner's answers have a limit a second");
	Check(Has(pldnet, "static_assert(kCoopChoiceCustomCampaign == kCampaignCustom"), "the build stops if the campaign numbers ever differ");

	std::string const hears = Between(networking, "static bool HostHearsFromClient(ENetPeer *from, const uint8_t *data, size_t size)\n{", "\n}\n");

	Check(hears.find("Net_GameMessageIsGood") < hears.find("SendToClients(packet, from)"), "nothing is passed on before it's been checked");
	Check(Has(hears, "// Nothing goes from one joiner to another in the lobby, and an answer about the map is only ever for the host\n\tif (Net_CoopLobbyState() != kCoopLobbyNone || Net_MessageIsForTheHostAlone(data, size))\n\t{\n\t\treturn true;\n\t}\n"), "and nothing is passed on in the lobby, nor is an answer ever");
	Check(hears.find("Net_CoopLobbyState() != kCoopLobbyNone") < hears.find("SendToClients(packet, from)"), "that comes before the passing on");
	Check(Has(hears, "// In the lobby only what the lobby takes counts as being heard from"), "a joiner sending only what's refused still counts as silent");
	Check(Has(hears, "if (connection <= kMostClients && Net_CoopLobbyState() == kCoopLobbyNone)\n\t{\n\t\tlastHeardFrom[connection] = enet_time_get();"), "before a message is checked, it keeps a seat only outside the lobby");

	std::string const forget = Between(networking, "static void ForgetConnectionsNotLetIn()\n{", "\n}\n");

	Check(Has(forget, "Net_CoopLobbyState() == kCoopLobbyOpen ? kMostMillisecondsSilentInTheLobby : kMostMillisecondsSilent"), "the host lets go of a silent joiner sooner in the lobby");

	// Modified 2026-11-10 by Melanikus for the AvP port (the owner's ruling for beta.2: "Everything over nyc3"): a game is found only on a
	// server's online list, so its lobby state is checked where a listed game is kept (AddRelaySession); the search's Net_AddSession is gone
	std::string const addSession = Between(networking, "static bool AddRelaySession(", "\n}\n");

	Check(Has(addSession, "!CoopLobby_StateIsReal(coopLobby))") && !Has(networking, "static void Net_AddSession("), "a listed game's lobby state is checked");
	Check(addSession.find("CoopLobby_StateIsReal") < addSession.find("SESSION_DESC &entry = SessionData[NumberOfSessionsFound];"), "before anything of it is kept");
	Check(Has(addSession, "if (localIndex < 0 && coopLobby == kCoopLobbyNone)"), "a lobby game stays on the list even if this machine lacks the map");
	Check(Has(networking, "Net_CoopLobbyJoined(SessionData[sessionNumber].coopLobby);"), "a joiner is in the lobby from the moment it's connected");

	std::string const choice = Between(pldnet, "static void ProcessNetMsg_CoopCampaignChoice(NETMESSAGE_COOPCAMPAIGNCHOICE *messagePtr)\n{", "\n}\n");

	Check(Has(choice, "AvP.Network != I_Peer"), "only a joiner takes a choice");
	Check(choice.find("CoopChoice_Step") < choice.find("CoopChoice_WhyRefused"), "an old number costs nothing more");
	Check(Has(choice, "int const levels = messagePtr->campaign == kCoopChoiceCustomCampaign ? 1 : Campaign_LevelCount(messagePtr->campaign);"), "each number in it is checked against the real campaign");
	Check(Has(choice, "CoopChoice_WhyRefused(*messagePtr, levels)"), "by the rule in coop_lobby.h");
	Check(choice.find("CoopChoice_WhyRefused") < choice.find("coopChoice = *messagePtr;"), "and nothing of it is kept until all of it has been checked");
	Check(Has(choice, "if (why)\n\t{\n\t\tNet_NoteRefused(why);\n\t\treturn;\n\t}"), "a refused choice is dropped, all of it, once it's counted");
	Check(Has(choice, "netGameData.coopCustomMap[sizeof(netGameData.coopCustomMap) - 1] = 0;"), "the name ends inside the game's copy, whatever came before");
	Check(Has(pldnet, "CustomMaps_SizeAndSum(CustomMaps_Named(coopChoice.mapName, sizeof(coopChoice.mapName)), &size, sum)"), "a joiner only looks the name up among the maps it listed itself");

	std::string const answer = Between(pldnet, "static void ProcessNetMsg_CoopMapAnswer(NETMESSAGE_COOPMAPANSWER *messagePtr, NetID senderId)\n{", "\n}\n");

	Check(Has(answer, "CoopAnswer_WhyRefused(*messagePtr, coopLobbyState == kCoopLobbyOpen, place != NET_IDNOTINPLAYERLIST, coopChoice.number)"), "the host checks each answer against what it knows");
	Check(answer.find("CoopAnswer_WhyRefused") < answer.find("coopMapAnswers[place] = "), "before writing anything down with that place");
	Check(Has(answer, "if (messagePtr->number == coopChoice.number)"), "and an answer to an earlier choice isn't counted");

	std::string const says = Between(pldnet, "void Net_CoopHostSays(void)\n{", "\n}\n");

	Check(Has(says, "now.number = coopChoice.number + 1;"), "each change of a row raises the choice's number");
	Check(Has(says, "Coop_ForgetTheAnswers();"), "and what anyone said about the last one no longer counts");

	std::string const gate = Between(pldnet, "static bool Net_CoopLobbyTakesThis(const uint8_t *message, size_t size)\n{", "\n}\n");

	Check(Has(gate, "onTheList == partCount"), "every part of a message has to be on the list");
	Check(Has(gate, "Net_NoteRefused(coopLobbyState == kCoopLobbyUnderWay ? kCoopRefusedPartBeforeJoining : kCoopRefusedPartInLobby);"), "each refusal is named");
	Check(Has(gate, "Net_NoteRefused(kCoopRefusedAnswerNoLobby);"), "and an answer in a game with no lobby is refused");
	Check(Has(gate, "if (AvP.Network == I_Host && !coopHostHasStarted && Net_MessageIsForTheHostAlone(message, size))"), "but not one that crossed the host's Start, which is honest");
	Check(Has(answer, "if (AvP.Network != I_Host || coopHostHasStarted)"), "and the host does nothing with that one");

	std::string const join = Between(pldnet, "void InitAVPNetGameForJoin(void)\n{", "\n}\n");

	Check(Has(join, "Coop_ForgetTheLobby();"), "a join forgets the last game's lobby");
	Check(Has(join, "netGameData.coopCampaign = kCampaignNone;") && Has(join, "netGameData.coopCampaignLevel = 0;"), "and its place, so a join starts clean");

	std::string const description = Between(pldnet, "static void ProcessNetMsg_GameDescription(NETMESSAGE_GAMEDESCRIPTION *messagePtr)\n{", "static void ProcessNetMsg_PlayerDescription(");

	Check(Has(description, "if (coopLobbyState == kCoopLobbyNone)\n\t\t{\n\t\t\tif (Coop_LevelChangeIsDue())"), "in the lobby a joiner's campaign doesn't come from the game description");
	Check(Has(description, "if (coopLobbyState == kCoopLobbyNone)\n\t\t{\n\t\t\tnetGameData.disableFriendlyFire = messagePtr->disableFriendlyFire;"), "and nor does friendly fire");

	std::string const environment = ReadSourceFile("3dc/avp/win95/frontend/env_info.cpp");

	Check(Has(environment, "return netGameData.coopCustomMap[0] != 0 ? 1 : 0;"), "a custom map only counts as a campaign once a lobby has chosen one");
	Check(Has(environment, "if (campaign == kCampaignCustom || place < 0 || place >= Campaign_LevelCount(campaign))"), "and is never looked up on the lists of the game's own levels");
}

// The text in quotes that starts at this place
static std::string QuotedAt(const std::string &text, size_t at)
{
	std::string words;

	for (size_t i = at; i < text.size() && text[i] != '"'; i++)
	{
		if (text[i] == '\\')
		{
			i++;
		}

		words += text[i];
	}

	return words;
}

// Every reason the game can give for refusing something, from all of its source. A new file is counted without anyone changing this.
static std::set<std::string> RefusalReasons()
{
	std::set<std::string> reasons;

	for (std::filesystem::directory_entry const &entry : std::filesystem::recursive_directory_iterator("3dc"))
	{
		std::string const extension = entry.path().extension().string();

		if (!entry.is_regular_file() || (extension != ".cpp" && extension != ".h" && extension != ".c" && extension != ".hpp"))
		{
			continue;
		}

		std::ifstream file(entry.path(), std::ios::binary);
		std::stringstream all;

		all << file.rdbuf();

		std::string const text = all.str();

		for (std::string const &before : { std::string("Net_NoteRefused(\""), std::string(" = \"") })
		{
			bool const isAName = before[0] == ' ';

			for (size_t at = text.find(before); at != std::string::npos; at = text.find(before, at + 1))
			{
				// Of the things set to words, only the lobby's named reasons count
				size_t const lineStart = text.rfind('\n', at);

				if (!isAName || text.compare(lineStart == std::string::npos ? 0 : lineStart + 1, 37, "static const char *const kCoopRefused", 0, 37) == 0)
				{
					reasons.insert(QuotedAt(text, at + before.size()));
				}
			}
		}
	}

	return reasons;
}

static void TestTheRefusalTable()
{
	puts("The table of refusal reasons:");

	std::set<std::string> const reasons = RefusalReasons();

	Check(reasons.size() > 60, "the reasons were read from the source: " + std::to_string(reasons.size()) + " of them");
	Check(reasons.count(kCoopRefusedPartInLobby) == 1, "the lobby's named reasons are among them");
	Check(reasons.count("a part only a host may send") == 1, "and so are the ones written out where they're used");
	Check((int)reasons.size() < kMostRefusalReasons, "each has a place of its own, so nobody can choose which share the last one");
}

static std::string Names(int count, const char *const *names, size_t room = 120, size_t most = kCoopRowMostLetters)
{
	char line[200];

	memset(line, 'x', sizeof(line));
	CoopNames_Line(line, room, names, count, 13, "and %d more", most);

	Check(memchr(line, 0, room) != NULL, "the row ends within its room");
	Check(room >= sizeof(line) || line[room] == 'x', "and nothing was written past it");

	return line;
}

static void TestTheNames()
{
	puts("A row of names:");

	const char *const few[] = { "Ripley", "Hicks" };
	const char *const many[] = { "AAAAAAAAAAAA", "BBBBBBBBBBBB", "CCCCCCCCCCCC", "DDDDDDDDDDDD", "EEEEEEEEEEEE", "FFFFFFFFFFFF", "GGGGGGGGGGGG" };

	Check(Names(2, few) == "Ripley, Hicks", "names that fit are all there, with a comma between");
	Check(Names(1, few) == "Ripley", "one name has no comma");
	Check(Names(0, few).empty(), "no names is an empty row");

	std::string const seven = Names(7, many);

	Check(seven == "AAAAAAAAAAAA, BBBBBBBBBBBB and 5 more", "seven long names stop in time to say how many more: " + seven);
	Check(seven.size() <= kCoopRowMostLetters, "and the row is no longer than the most letters");

	std::string const exact = Names(2, few, 120, 13);

	Check(exact == "Ripley, Hicks", "names exactly as long as the most letters all fit");
	Check(Names(2, few, 120, 12) == "Ripley and 1 more" || Names(2, few, 120, 12) == "and 2 more", "one letter less and they don't: " + Names(2, few, 120, 12));
	Check(Names(7, many, 120, 5) == "and 7 more", "no room for any name says only how many");
	Check(Names(7, many, 16, 200) == "AAAAAAAAAAAA, B", "a room smaller than the row cuts it short, with its end");

	char noEnd[20];

	memset(noEnd, 'N', sizeof(noEnd));
	noEnd[sizeof(noEnd) - 1] = 0;

	const char *const unended[] = { noEnd };

	Check(Names(1, unended) == "NNNNNNNNNNNNN", "a name with no end stops at the bytes a name has");

	// A name comes from another machine, so it's never read as a format: what looks like one prints as itself
	const char *const formats[] = { "%d%x%%", "%s%n%s%n", "%p%c%99d" };

	Check(Names(1, formats) == "%d%x%%", "a name that looks like a format prints as plain text");
	Check(Names(3, formats) == "%d%x%%, %s%n%s%n, %p%c%99d", "and so do names with %s and %n in them");
	Check(Names(3, formats, 120, 17) == "%d%x%% and 2 more", "and the words for the rest count only the names left out");

	// The words for the rest come from language.txt, which anyone can change, so they're never read as a format either. Each call
	// starts from a room full of x, so a missing end would show.
	char words[64];

	memset(words, 'x', sizeof(words));
	Check(CoopNames_MoreWords(words, sizeof(words), "and %d more", 12) == 12 && std::string(words) == " and 12 more", "the number goes where the words' %d is, digits in order");
	memset(words, 'x', sizeof(words));
	Check(CoopNames_MoreWords(words, sizeof(words), "%s %n", 5) == 8 && std::string(words) == " %s %n 5", "words with a %s and a %n and no %d come out as they are, with the number after them");
	memset(words, 'x', sizeof(words));
	Check(CoopNames_MoreWords(words, sizeof(words), "%s and %d more %n", 3) == 17 && std::string(words) == " %s and 3 more %n", "with one %d among them, only the %d is replaced");
	memset(words, 'x', sizeof(words));
	Check(CoopNames_MoreWords(words, sizeof(words), "%d of %d", 4) == 11 && std::string(words) == " %d of %d 4", "with two, neither is replaced, and the number comes after them");
	memset(words, 'x', sizeof(words));
	Check(CoopNames_MoreWords(words, sizeof(words), "", 9) == 3 && std::string(words) == "  9", "empty words still say the number");
	memset(words, 'x', sizeof(words));
	Check(CoopNames_MoreWords(words, 6, "and %d more", 12) == 12 && std::string(words) == " and " && words[6] == 'x', "a room too small for them cuts them short with an end, writes nothing past it, and still says how long they'd be");

	const char *const someNames[] = { "Ripley", "Hicks", "Hudson" };
	char line[64];

	CoopNames_Line(line, sizeof(line), someNames, 3, 13, "%s%n%s", 15);
	Check(std::string(line) == "Ripley %s%n%s 2", "a row whose words have a %s and a %n in them still comes out");
}

static void TestTheLayout()
{
	puts("Where a page's rows go:");

	int const centre = 280;
	int const rowHeight = 19;
	int const episodePictureEnds = 178;
	int const tallestPictureEnds = 206;

	CoopPageLayout layout = CoopPage_Layout(9 * rowHeight, centre, episodePictureEnds);

	Check(layout.rowsTop == centre - (9 * rowHeight) / 2 && layout.showPicture, "rows clear of the picture stay in the middle, and the picture shows");

	layout = CoopPage_Layout(13 * rowHeight, centre, episodePictureEnds);

	Check(layout.rowsTop == episodePictureEnds + kCoopRowsUnderPicture && layout.showPicture, "four players' rows would be on the picture, so they move under it");

	layout = CoopPage_Layout(14 * rowHeight, centre, episodePictureEnds);

	Check(!layout.showPicture, "five players' rows would reach the help line under it, so the picture goes");
	Check(layout.rowsTop == centre - (14 * rowHeight) / 2, "and they stay in the middle");

	layout = CoopPage_Layout(17 * rowHeight, centre, tallestPictureEnds);

	Check(!layout.showPicture && layout.rowsTop == centre - (17 * rowHeight) / 2, "a full game with the tallest picture is rows alone");
	Check(layout.rowsTop + 17 * rowHeight <= kCoopRowsMostLow + rowHeight / 2, "and those rows still end above the help line");

	layout = CoopPage_Layout(12 * rowHeight, centre, tallestPictureEnds);

	Check(layout.showPicture && layout.rowsTop == tallestPictureEnds + kCoopRowsUnderPicture, "the tallest picture keeps its place while the rows fit under it");

	layout = CoopPage_Layout(10 * rowHeight, centre, 0);

	Check(!layout.showPicture && layout.rowsTop == centre - (10 * rowHeight) / 2, "no picture is the middle of the page");

	int const justFits = kCoopRowsMostLow - (episodePictureEnds + kCoopRowsUnderPicture);

	Check(CoopPage_Layout(justFits, centre, episodePictureEnds).showPicture, "rows that end exactly at the lowest place still fit");
	Check(!CoopPage_Layout(justFits + 1, centre, episodePictureEnds).showPicture, "and one unit more doesn't");

	int const justClear = (centre - (episodePictureEnds + kCoopRowsUnderPicture)) * 2;

	Check(CoopPage_Layout(justClear, centre, episodePictureEnds).rowsTop == centre - justClear / 2, "rows just clear of the picture don't move");

	// Rows that would start in the gap under the picture, one unit below its bottom, move down to leave the whole gap
	int const inTheGap = (centre - (episodePictureEnds + 1)) * 2;

	Check(CoopPage_Layout(inTheGap, centre, episodePictureEnds).rowsTop == episodePictureEnds + kCoopRowsUnderPicture, "rows that would start in the gap under the picture move down under it");
}

static void TestThePages()
{
	puts("The two pages:");

	std::string const data = ReadSourceFile("3dc/avp/win95/frontend/menu_data.cpp");
	std::string const menus = ReadSourceFile("3dc/avp/win95/frontend/menus.cpp");
	std::string const pages = ReadSourceFile("3dc/avp/win95/frontend/coop_menu.cpp");
	std::string const hostPage = Between(data, "AVPMENU_ELEMENT AvPMenu_CoopHost[] =", "};");
	std::string const joinPage = Between(data, "AVPMENU_ELEMENT AvPMenu_CoopJoin[] =", "};");

	Check(Has(hostPage, "{AVPMENU_ELEMENT_COOP_START,"), "the host's page has a Start row");
	Check(!Has(joinPage, "AVPMENU_ELEMENT_COOP_START"), "and the joiner's hasn't");
	Check(!Has(joinPage, "{AVPMENU_ELEMENT_TEXTSLIDER,") && !Has(joinPage, "{AVPMENU_ELEMENT_TEXTSLIDER_POINTER,") && !Has(joinPage, "{AVPMENU_ELEMENT_TEXTFIELD,"), "a joiner can change nothing on its page");

	for (char const *const row : { "{TEXTSTRING_CAMPAIGN_LEVEL}", "{TEXTSTRING_COOP_START_AT}", "{TEXTSTRING_CUSTOMMAPS_SPECIES}" })
	{
		Check(Has(hostPage, row) && Has(joinPage, row), std::string("both pages have the row ") + row + ", which the code looks for by name");
	}

	// Modified 2026-11-10 by Melanikus for the AvP port: the Multiplayer page's rows are written as it opens (menu_data.cpp,
	// MenuData_SetMultiplayerRows), so the row is a constant, and the page writes it there
	Check(Has(data, "static AVPMENU_ELEMENT const CoopRow = {AVPMENU_ELEMENT_GOTOMENU,\t{TEXTSTRING_COOP_TITLE},\t{AVPMENU_COOP_HOST},") &&
		Has(data, "\tAvPMenu_Multiplayer[row++] = CoopRow;\n"), "the multiplayer page leads to the host's page");
	Check(Has(menus, "SetupNewMenu(Net_CoopLobbyState() != kCoopLobbyNone ? AVPMENU_COOP_JOIN : AVPMENU_MULTIPLAYER_CONFIG_JOIN);"), "a joiner lands on the lobby page only for a lobby game");

	std::string const hostFrame = Between(pages, "static CoopPageAnswer HostEveryFrame(void)\n{", "\n}\n");

	Check(Has(hostFrame, "HostSaysWhatTheGameIs(kCoopLobbyOpen);"), "what a search is told follows the rows, every frame");
	Check(Has(hostFrame, "Net_CoopHostSays();"), "and so does what joiners are told");

	std::string const hostStarts = Between(pages, "bool CoopMenu_HostStarts(void)\n{", "\n}\n");

	Check(Has(hostStarts, "if (netGameData.coopCampaign == kCampaignCustom && !CustomMaps_SizeAndSum(CoopRowPlace, &size, sum))"), "the host can't start a custom map whose file has gone");
	Check(Has(hostStarts, "HostSaysWhatTheGameIs(kCoopLobbyUnderWay);"), "a game that has started says so to anyone searching");
	Check(hostStarts.find("Campaign_ChooseRollNumber();") < hostStarts.find("Net_CoopHostStarts();"), "the one number is chosen before the host's last word, which carries it");

	Check(Has(pages, "int const mostCampaignRow = CustomMaps_Count() > 0 ? 3 : 2;"), "a custom map is only offered if there is one");
	Check(Has(menus, "CoopPageLayout const layout = CoopPage_Layout(AvPMenus.MenuHeight, MENU_CENTREY,"), "a co-op page's rows go where the rule says");
	Check(Has(menus, "y = layout.rowsTop;"), "and not simply in the middle");
	Check(Has(pages, "_snprintf(removeRowText[rows], sizeof(removeRowText[rows]) - 1, \"%s %s\", GetTextString(TEXTSTRING_COOP_REMOVE), netGameData.playerData[place].name);"), "a name on a Remove row is printed as text, never as a format");
	Check(Has(pages, "CoopNames_Line(CoopRowNoMap, sizeof(CoopRowNoMap), names, count, NET_PLAYERNAMELENGTH, GetTextString(TEXTSTRING_COOP_AND_MORE), kCoopRowMostLetters);"), "the host's row of names stops before the edge of the screen");
	Check(Has(menus, "coopPicture = layout.showPicture ? picture : MISSING_TEXTURE;"), "and the picture only shows if the rule says so");

	std::string const joinerFrame = Between(pages, "static CoopPageAnswer JoinerEveryFrame(void)\n{", "\n}\n");

	Check(Has(joinerFrame, "if (Net_CoopMyMapAnswer() == kCoopMapIsHere)\n\t\t{\n\t\t\tJoinerGoesIntoTheLoad();"), "a joiner only goes if it has the map");
	Check(Has(joinerFrame, "\t\treturn JoinerIsOut();\n\t}\n\n\tNet_CoopJoinerSays();"), "one who hasn't is out of the game");

	std::string const isOut = Between(pages, "static CoopPageAnswer JoinerIsOut(void)\n{", "\n}\n");

	Check(Has(isOut, "CoopMenu_PageLeft();"), "and leaves it, so as not to hold a seat");
	Check(isOut.find("netGameData.coopCustomMap") < isOut.find("CoopMenu_PageLeft();"), "keeping the map's name first, because leaving wipes it");
	Check(Has(menus, "case kCoopPageJoinerIsOut:\n\t\t\t\tSetupNewMenu(AVPMENU_COOP_OUT);"), "the rows that were wiped aren't left on the screen");

	std::string const outPage = Between(data, "AVPMENU_ELEMENT AvPMenu_CoopOut[] =", "};");
	std::string const outOpens = Between(pages, "bool CoopMenu_OutPageOpens(void)\n{", "\n}\n");

	Check(!Has(outPage, "netGameData"), "the page for a joiner who's out reads nothing the end of the game wipes");
	Check(Has(outPage, "{AVPMENU_ELEMENT_GOTOMENU,"), "its sentence is a row of its own, in the middle of the page, so it has the room it needs");
	Check(Has(outOpens, "if (Net_WasRemovedByTheHost())\n\t{\n\t\tlastWord = kCoopLastWordRemoved;"), "it says which happened, asking first if the player was removed");
	Check(Has(pages, "{kCoopLastWordLeftBehind, TEXTSTRING_COOP_LEFT_BEHIND, TEXTSTRING_COOP_MAP_LINE_HELP, TEXTSTRING_COOP_MAP_LINE},"), "a player left behind is told where a map goes, and which map");
	Check(Has(pages, "{kCoopLastWordRemoved, TEXTSTRING_COOP_REMOVED, TEXTSTRING_BLANK, TEXTSTRING_BLANK},"), "a removed player, who may never have heard a name, gets only the sentence");
	Check(Has(joinerFrame, "if (netGameData.myGameState == NGS_Error_HostLost)"), "a joiner whose host has gone isn't left waiting");
	Check(Has(pages, "if (CoopRowPlace < 0 || CoopRowPlace >= (int)placeNames.size())\n\t{\n\t\tCoopRowPlace = 0;\n\t}\n\n\tif (CoopRowSpecies < 0"), "a joiner's row of names can't be read past its end");

	std::string const goes = Between(pages, "static void JoinerGoesIntoTheLoad(void)\n{", "\n}\n");

	Check(goes.find("Net_CoopLobbyEnds();") < goes.find("Net_CoopTakeTheCampaign();"), "a joiner leaves the lobby before anything of the game runs");
	Check(Has(pages, "void CoopMenu_PageLeft(void)\n{\n\tif (AvP.Network != I_No_Network)\n\t{\n\t\tEndAVPNetGame();"), "leaving a page closes or leaves the game");
	Check(Has(menus, "case AVPMENU_COOP_HOST:\n\t\t\t\tcase AVPMENU_COOP_JOIN:\n\t\t\t\t{\n\t\t\t\t\tCoopMenu_PageLeft();"), "and Escape on either page does that");
	Check(Has(menus, "SetupNewMenu((enum AVPMENU_ID)CoopMenu_PageAfterACustomMap());"), "a custom map played with others goes back to the multiplayer page");

	std::string const networking = ReadSourceFile("3dc/networking.cpp");
	std::string const pldnet = ReadSourceFile("3dc/avp/win95/pldnet.cpp");
	std::string const remove = Between(networking, "void Net_RemoveThisPlayer(NetID id)\n{", "\n}\n");
	std::string const letIn = Between(networking, "static void LetClientIn(ENetPeer *peer, const uint8_t *details)\n{", "\n}\n");

	Check(!Has(joinPage, "AVPMENU_ELEMENT_COOP_REMOVE"), "a joiner's page has no row to remove anyone");
	Check(Has(menus, "case AVPMENU_ELEMENT_COOP_REMOVE:\n\t\t{\n\t\t\tif (interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT)\n\t\t\t{\n\t\t\t\tCoopMenu_HostRemoves(elementPtr->MaxValue);"), "pressing a row removes that player");
	Check(!Has(pldnet, "Net_RemoveThisPlayer"), "nothing a message does can reach the removing: no message handler asks for it");
	Check(Has(remove, "if (AvP.Network != I_Host || !host || id == 0 || id == AvPNetID)"), "only a host can remove anyone, and never itself");
	Check(Has(remove, "CoopRemoved_Add(removedPlayers, peer->address.host);"), "the address the connection really came from goes on the list");
	Check(remove.find("peer->data = NULL;") < remove.find("enet_peer_disconnect(peer, kCoopRemovedByTheHost);"), "they're out before the goodbye, so nothing more from them counts");
	Check(Has(remove, "netSession.nPlayers--;") && Has(remove, "Net_PlayerHasGone(id);"), "and their seat is free");
	Check(letIn.find("CoopRemoved_Has(removedPlayers, peer->address.host)") < letIn.find("Net_GetNextPlayerID()"), "someone removed is refused before they get a player number");
	Check(Has(letIn, "Net_NoteRefused(kCoopRefusedRemoved);"), "with a named reason");
	Check(Has(Between(networking, "void Net_Disconnect()\n{", "\n}\n"), "CoopRemoved_Forget(removedPlayers);"), "the list goes when the game closes");
	Check(Has(joinerFrame, "if (Net_WasRemovedByTheHost())\n\t{\n\t\treturn JoinerIsOut();"), "a removed player is out of the game");
	Check(Has(menus, "SetupNewMenu(Net_WasRemovedByTheHost() ? AVPMENU_COOP_OUT : AVPMENU_MULTIPLAYER);"), "and is told so if they try to come back");
	Check(joinerFrame.find("Net_WasRemovedByTheHost") < joinerFrame.find("NGS_Error_HostLost"), "before the page would go back for a host that has gone");
}

int main()
{
	TestTheChoice();
	TestTheNumbers();
	TestTheMapAnswer();
	TestTheWalk();
	TestTheStates();
	TestTheRemovedList();
	TestTheLists();
	TestTheGameUsesIt();
	TestTheRefusalTable();
	TestTheLayout();
	TestTheNames();
	TestThePages();

	puts(checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
