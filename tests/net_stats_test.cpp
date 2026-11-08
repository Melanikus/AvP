// The test of the network's numbers in the log: the words of each line, a connection starting and stopping struggling, and where the
// network code writes them. Written 2026-11-07 by Melanikus for the AvP port.
//
// WHAT IT'S FOR: net_stats.h says what the lines are for. This tries them on made-up numbers, and reads networking.cpp to see that it
// writes them once a game is on, by connection number and never by address. Run it from the top of the repository, because that's where
// the source it reads is found from.
// Modified 2026-11-08 by Melanikus for the AvP port:
//   its Between is the shared one in source_slice.h, so a marker that isn't there is a failed check of its own (SOURCE-SLICE-1, #128)
#include "net_stats.h"

#include <string.h>
#include <fstream>
#include <sstream>
#include <string>
#include "source_slice.h"

static int checksFailed = 0;

static void Check(bool passed, const char *what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what);

	if (!passed)
	{
		checksFailed++;
	}
}

// A source file, with its line ends written \n whatever the checkout made them
static std::string ReadSource(const char *name)
{
	std::ifstream file(name, std::ios::binary);
	std::stringstream text;
	text << file.rdbuf();

	std::string out;

	for (char c : text.str())
	{
		if (c != '\r')
		{
			out += c;
		}
	}

	return out;
}

static bool Has(const std::string &text, const char *what)
{
	return text.find(what) != std::string::npos;
}

// The text from one marker up to the next (source_slice.h). A marker that isn't there is a failed check of its own, named by the marker.
static std::string Between(const std::string &text, const char *from, const char *to)
{
	return SourceSlice(text, from, to, kSliceEndAfterTheStart, [](const std::string &why) { Check(false, why.c_str()); });
}

static NetStats_Connection Numbers(unsigned int roundTrip, unsigned int lossPercent)
{
	NetStats_Connection c;
	c.roundTrip = roundTrip;
	c.roundTripVariance = 12;
	c.loss = lossPercent * kNetStatsLossScale / 100;
	c.packetsLost = 3;
	c.packetsSent = 410;
	return c;
}

static void TestTheLines()
{
	printf("The lines:\n");

	char words[200];
	NetStats_ConnectionLine(2, Numbers(180, 2), words, sizeof(words));
	Check(strcmp(words, "net: connection 2 round trip 180 ms loss 2% resends 3 (round trips vary by 12 ms; 410 sent)") == 0, "a connection's line, with the words a log reader looks for first");

	NetStats_BytesLine(61440, 0, 58320, 0, 10000, words, sizeof(words));
	Check(strcmp(words, "net: in 61440 bytes, out 58320 bytes, in the last 10 s") == 0, "the bytes in and out since the last line");

	NetStats_BytesLine(100, 0xFFFFFF00u, 5, 0, 9600, words, sizeof(words));
	Check(Has(words, "net: in 356 bytes") && Has(words, "in the last 10 s"), "and right when ENet's total has gone round");

	Check(NetStats_LossPercent(0) == 0 && NetStats_LossPercent(kNetStatsLossScale) == 100 && NetStats_LossPercent(kNetStatsLossScale / 2) == 50, "loss comes out as a percentage");
	Check(NetStats_LossPercent(0xFFFFFFFFu) == 100, "and never more than all of it");

	char cramped[24 + 8];
	memset(cramped, 'g', sizeof(cramped));
	NetStats_ConnectionLine(2, Numbers(180, 2), cramped, 24);
	bool guardsKept = true;

	for (size_t i = 24; i < sizeof(cramped); i++)
	{
		guardsKept = guardsKept && cramped[i] == 'g';
	}

	Check(guardsKept && strlen(cramped) == 23 && strncmp(cramped, "net: connection 2 round", 23) == 0, "a small space is never written past, and the line still ends");
}

static void TestStruggling()
{
	printf("Struggling and fine again:\n");

	char words[200];
	bool struggling = false;

	Check(!NetStats_Change(1, false, Numbers(180, 2), &struggling, words, sizeof(words)) && !struggling, "a good connection says nothing");
	Check(NetStats_Change(1, false, Numbers(620, 0), &struggling, words, sizeof(words)) && struggling && strcmp(words, "net: connection 1 is struggling: round trip 620 ms loss 0%") == 0, "a round trip over 500 ms says it's struggling");
	Check(NetStats_Change(1, false, Numbers(100, 14), &struggling, words, sizeof(words)) && struggling && Has(words, "loss 14%"), "and so does losing more than 10%");
	Check(!NetStats_Change(1, false, Numbers(500, 10), &struggling, words, sizeof(words)) && !struggling, "500 ms and 10% exactly aren't struggling yet");
	Check(!NetStats_Change(1, true, Numbers(480, 0), &struggling, words, sizeof(words)) && struggling, "once struggling, a round trip just under 500 ms isn't fine yet, so a line on the edge doesn't fill the log");
	Check(!NetStats_Change(1, true, Numbers(100, 9), &struggling, words, sizeof(words)) && struggling, "nor is 9% lost");
	Check(NetStats_Change(1, true, Numbers(450, 8), &struggling, words, sizeof(words)) && !struggling && strcmp(words, "net: connection 1 is fine again: round trip 450 ms loss 8%") == 0, "450 ms and 8% are fine again, and it says so");
	Check(!NetStats_Change(1, true, Numbers(900, 50), &struggling, words, sizeof(words)) && struggling, "still struggling says nothing more");
}

static void TestTheNetworkCodeUsesIt()
{
	printf("Where the network code uses it, by reading:\n");

	std::string const net = ReadSource("3dc/networking.cpp");
	std::string const service = Between(net, "void Net_ServiceNetwork()\n{", "\n}\n");
	Check(Has(service, "\t\tenet_host_service(host, NULL, 0);\n\n\t\tLogConnectionNumbers();"), "the numbers are looked at every time the network is served, on a host and a client alike");

	std::string const logging = Between(net, "static void LogConnectionNumbers()\n{", "\nstatic void LogOlderVersionsToldNo()");
	Check(Has(logging, "if (now - netStatsLastLook < 1000)"), "about once a second");
	Check(Has(logging, "bool const linesDue = now - netStatsLastLines >= kNetStatsEveryMilliseconds;"), "with the full lines every 10 seconds");
	Check(Has(logging, "NetStats_ConnectionLine((int)i + 1, numbers, words, sizeof(words));") && Has(logging, "NetStats_Change((int)i + 1, netStatsStruggling[i], numbers, &struggling, words, sizeof(words))"), "each connection named by its number, from 1");
	Check(Has(logging, "if (host != netStatsHost)"), "and a new game's host starts the numbers again");
	Check(!logging.empty() && !Has(logging, "address") && !Has(logging, "enet_address"), "and never by its address");
	Check(Has(logging, "LogString(words);") && !Has(logging, "NewOnScreenMessage"), "the lines go in the log, which leaves out names and addresses anyway, and not on the screen");
	Check(Has(net, "static_assert(kNetStatsLossScale == ENET_PEER_PACKET_LOSS_SCALE,"), "and the build stops if ENet's share of loss isn't net_stats.h's");
}

int main()
{
	setvbuf(stdout, NULL, _IONBF, 0);

	TestTheLines();
	TestStruggling();
	TestTheNetworkCodeUsesIt();

	if (checksFailed)
	{
		printf("SOME CHECKS FAILED: %d\n", checksFailed);
		return 1;
	}

	puts("All checks passed");
	return 0;
}
