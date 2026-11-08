// Makes sure the multiplayer version goes up whenever what two machines have to agree on changes.
// Written 2026-11-06 by Melanikus for the AvP port.
//
// Two copies of the game only make sense of each other if every message is the same size in both, and if
// the same number on a level list means the same level. Each copy says its version number, kMultiplayerVersion,
// and a game with another number is refused (networking.cpp). But nothing used to make anyone change that number,
// so a build that changed a message and forgot would read the other's messages wrongly.
//
// So: below is one number worked out from the size of every message, and from how long the level lists are, and
// that number is written down next to the version it belongs to. If anything changes, the build stops here.
//
// WHAT TO DO WHEN THE BUILD STOPS HERE:
//   1. Raise kMultiplayerVersion in 3dc/networking.h by one. (Then also the check of it in tests/net_start_test.cpp.)
//   2. Put that version in kVersionTheseSizesBelongTo below.
//   3. The error message shows MessageSizesHaveChanged<12345,false>. Put that number in kSizesAtThatVersion below.
//
// Each size is multiplied by its place in the list, so that one message growing while another shrinks by the
// same amount still changes the answer.
//
// Only pldnet.cpp includes this, just after its table of messages, kNetPartRules, because this counts that table's rows.

#ifndef _net_version_check_h_
#define _net_version_check_h_ 1

static const int kVersionTheseSizesBelongTo = 112;
static const unsigned int kSizesAtThatVersion = 55128;

static const unsigned int kSizesNow =
	1 * sizeof(NETMESSAGEHEADER) +
	2 * sizeof(NETMESSAGE_GAMEDESCRIPTION) +
	3 * sizeof(NETMESSAGE_PLAYERDESCRIPTION) +
	4 * sizeof(NETMESSAGE_PLAYERSTATE) +
	5 * sizeof(NETMESSAGE_PLAYERSTATE_MINIMAL) +
	6 * sizeof(NETMESSAGE_PLAYERSTATE_MEDIUM) +
	7 * sizeof(NETMESSAGE_FRAMETIMER) +
	8 * sizeof(NETMESSAGE_PLAYERKILLED) +
	9 * sizeof(NETMESSAGE_CORPSEDEATHANIM) +
	10 * sizeof(NETMESSAGE_ALLGAMESCORES) +
	11 * sizeof(NETMESSAGE_SPECIESSCORES) +
	12 * sizeof(NETMESSAGE_PLAYERSCORES) +
	13 * sizeof(NETMESSAGE_SCORECHANGE) +
	14 * sizeof(NETMESSAGE_LOCALRICOCHET) +
	15 * sizeof(NETMESSAGE_LOBSTATE) +
	16 * sizeof(NETMESSAGE_LOBDAMAGED_HEADER) +
	17 * sizeof(NETMESSAGE_GHOSTHIERARCHYDAMAGED_HEADER) +
	18 * sizeof(NETMESSAGE_INANIMATEDAMAGED_HEADER) +
	19 * sizeof(NETMESSAGE_DAMAGE_PROFILE) +
	20 * sizeof(NETMESSAGE_DAMAGE_MULTIPLE) +
	21 * sizeof(NETMESSAGE_DAMAGE_SECTION) +
	22 * sizeof(NETMESSAGE_DAMAGE_DELTA) +
	23 * sizeof(NETMESSAGE_DAMAGE_DIRECTION) +
	24 * sizeof(NETMESSAGE_LEVELEVENT) +
	25 * sizeof(NETMESSAGE_COOPNEXTLEVEL) +
	26 * sizeof(NETMESSAGE_LOBDESTROYED_REQUEST) +
	27 * sizeof(NETMESSAGE_LOBDESTROYED) +
	28 * sizeof(NETMESSAGE_OBJECTPICKEDUP) +
	29 * sizeof(NETMESSAGE_INANIMATEDAMAGED) +
	30 * sizeof(NETMESSAGE_INANIMATEDESTROYED) +
	31 * sizeof(NETMESSAGE_LOSREQUESTBINARYSWITCH) +
	32 * sizeof(NETMESSAGE_PLATFORMLIFTSTATE) +
	33 * sizeof(NETMESSAGE_REQUESTPLATFORMLIFTACTIVATE) +
	34 * sizeof(NETMESSAGE_AGUNSTATE) +
	35 * sizeof(NETMESSAGE_MAKEDECAL) +
	36 * sizeof(NETMESSAGE_MAKEEXPLOSION) +
	37 * sizeof(NETMESSAGE_MAKEFLECHETTEEXPLOSION) +
	38 * sizeof(NETMESSAGE_MAKEPLASMAEXPLOSION) +
	39 * sizeof(NETMESSAGE_PREDATORSIGHTS) +
	40 * sizeof(NETMESSAGE_LOBONFIRE) +
	41 * sizeof(NETMESSAGE_ALIENAISTATE) +
	42 * sizeof(NETMESSAGE_ALIENSEQUENCECHANGE) +
	43 * sizeof(NETMESSAGE_ALIENAIKILLED) +
	44 * sizeof(NETMESSAGE_FARALIENPOSITION) +
	45 * sizeof(NETMESSAGE_GIBBING) +
	46 * sizeof(NETMESSAGE_SPOTALIENSOUND) +
	47 * sizeof(NETMESSAGE_CREATEWEAPON) +
	48 * sizeof(NETMESSAGE_FRAGMENTALOBJECTSSTATUS) +
	49 * sizeof(NETMESSAGE_STRATEGYSYNCH) +
	50 * sizeof(NETMESSAGE_PLAYERID) +
	51 * sizeof(NETMESSAGE_LMS_RESTART) +
	52 * sizeof(NETMESSAGE_RESTARTGAME) +
	53 * sizeof(NETMESSAGE_LMS_RESTARTTIMER) +
	54 * sizeof(NETMESSAGE_SPOTOTHERSOUND) +
	55 * sizeof(SessionDescription) +
	56 * sizeof(PlayerDetails) +
	57 * sizeof(MessageHeader) +
	58 * (sizeof(kNetPartRules) / sizeof(kNetPartRules[0])) + // how many kinds of message there are: each has a row in that table, or it's refused
	62 * sizeof(NETMESSAGE_NPCSTATE) +
	63 * sizeof(NETMESSAGE_NPCXENODELTAS) +
	64 * sizeof(NETMESSAGE_NPCKILLED) +
	65 * sizeof(NETMESSAGE_NPCSOUND) +
	66 * sizeof(NETMESSAGE_NPCTHROWNOBJECT) +
	67 * sizeof(NETMESSAGE_LIFTSTATE) +
	68 * sizeof(NETMESSAGE_COOPCAMPAIGNCHOICE) +
	69 * sizeof(NETMESSAGE_COOPMAPANSWER) +
	59 * MAX_NO_OF_MULTIPLAYER_EPISODES +
	60 * MAX_NO_OF_COOPERATIVE_EPISODES +
	61 * kLevelListCampaignLevels;

// If this stops the build, the steps are at the top of this file. The number in the error is the new one.
template <unsigned int theSizesNow, bool sizesAreAsWrittenDown>
struct MessageSizesHaveChanged
{
	static_assert(sizesAreAsWrittenDown, "A network message or a level list has changed. See the top of net_version_check.h for what to do. The number in the name MessageSizesHaveChanged<number,false> is the new one.");
};

static_assert(sizeof(MessageSizesHaveChanged<kSizesNow, kSizesNow == kSizesAtThatVersion>) > 0, "This only makes the compiler look at the check above.");

static_assert(kMultiplayerVersion == kVersionTheseSizesBelongTo, "kMultiplayerVersion has changed. See the top of net_version_check.h for what to do.");

#endif
