// The test of the game socket's traffic counts (3dc/net_traffic.h). Written 2026-11-09 by Melanikus for the AvP port.
//
// WHAT IT TESTS: the counts start at 0 from ENet's totals as they are; each look adds only what's new, even when ENet's 32-bit totals
// wrap round; what goes past ENet is counted too; nothing is counted before the start; and the figures for the last second come once a
// window has run a second, as an average over a window that ran long, and never overflow.
#include <stdint.h>
#include <stdio.h>
#include <string>

#include "net_traffic.h"

static int checksFailed = 0;

static void Check(bool good, const std::string &what)
{
	printf("%s  %s\n", good ? "ok    " : "FAILED", what.c_str());
	checksFailed += good ? 0 : 1;
}

int main()
{
	printf("The game socket's traffic counts\n");
	NetTraffic t;

	// Before the start, nothing is counted (all 0, as the game's own is before its first host)
	memset(&t, 0, sizeof(t));
	NetTraffic_Sent(&t, 64);
	NetTraffic_Received(&t, 64);
	NetTraffic_FromENet(&t, 5, 5, 500, 500);
	Check(t.packetsIn == 0 && t.packetsOut == 0 && t.bytesIn == 0 && t.bytesOut == 0, "before the start nothing is counted");

	// The start: ENet's totals as they are are the zero
	NetTraffic_Start(&t, 10, 20, 1000, 2000, 5000);
	Check(t.started && t.packetsIn == 0 && t.packetsOut == 0 && t.bytesIn == 0 && t.bytesOut == 0 && t.packetsInLastSecond == 0,
		"a new host starts every count at 0, whatever ENet's totals are");

	NetTraffic_FromENet(&t, 13, 24, 1300, 2400);
	Check(t.packetsIn == 3 && t.packetsOut == 4 && t.bytesIn == 300 && t.bytesOut == 400, "a look adds what ENet counted since the start");
	NetTraffic_FromENet(&t, 13, 24, 1300, 2400);
	Check(t.packetsIn == 3 && t.packetsOut == 4 && t.bytesIn == 300 && t.bytesOut == 400, "and a look with nothing new adds nothing");

	NetTraffic_Sent(&t, 64);
	NetTraffic_Received(&t, 64);
	NetTraffic_Received(&t, 64);
	Check(t.packetsOut == 5 && t.bytesOut == 464 && t.packetsIn == 5 && t.bytesIn == 428, "what goes past ENet is counted too, one packet each");

	// ENet's totals wrap round at 2^32: the look still adds what's new, and the counts go past 32 bits
	NetTraffic w;
	NetTraffic_Start(&w, 0xFFFFFFF0u, 0, 0xFFFFFF00u, 0, 0);
	NetTraffic_FromENet(&w, 0x10u, 0, 0x100u, 0);
	Check(w.packetsIn == 0x20 && w.bytesIn == 0x200, "ENet's totals wrapping round past 2^32 still add what's new");

	for (int i = 0; i < 5; i++)
	{
		NetTraffic_FromENet(&w, w.enetPacketsIn, 0, w.enetBytesIn + 0xF0000000u, 0);
	}

	Check(w.bytesIn > 0xFFFFFFFFull, "and the counts here go past 32 bits without wrapping");

	// The last second: nothing until a window has run a second
	NetTraffic s;
	NetTraffic_Start(&s, 0, 0, 0, 0, 1000);
	NetTraffic_FromENet(&s, 30, 20, 3000, 2000);
	NetTraffic_Tick(&s, 1999);
	Check(s.packetsInLastSecond == 0 && s.packetsOutLastSecond == 0, "no figures before the first window has run a second");
	NetTraffic_Tick(&s, 2000);
	Check(s.packetsInLastSecond == 30 && s.packetsOutLastSecond == 20 && s.bytesInLastSecond == 3000 && s.bytesOutLastSecond == 2000,
		"after a second, the window's packets and bytes, each way");
	NetTraffic_FromENet(&s, 40, 20, 4000, 2000);
	NetTraffic_Tick(&s, 2500);
	Check(s.packetsInLastSecond == 30, "the figures stay until the next window has run a second");
	NetTraffic_Tick(&s, 3000);
	Check(s.packetsInLastSecond == 10 && s.packetsOutLastSecond == 0 && s.bytesInLastSecond == 1000, "and then they're the new window's");

	// A window that ran long (a level loading): an average for one second, not the whole burst
	NetTraffic_FromENet(&s, 80, 20, 8000, 2000);
	NetTraffic_Tick(&s, 7000);
	Check(s.packetsInLastSecond == 10 && s.bytesInLastSecond == 1000, "a window that ran 4 seconds gives a quarter of what came in it");

	// The clock wrapping round at 2^32 ms (49 days) doesn't stop the windows: 0xFFFFFE18 is 1000 ms before 0x200, across the wrap
	NetTraffic c;
	NetTraffic_Start(&c, 0, 0, 0, 0, 0xFFFFFE18u);
	NetTraffic_FromENet(&c, 7, 0, 700, 0);
	NetTraffic_Tick(&c, 0x00000200u);
	Check(c.packetsInLastSecond == 7, "the clock wrapping round still closes a window after its second");

	// The figure for a second never overflows. 18446744073709552 is the smallest count whose sum (count x 1000) is past 64 bits: unguarded,
	// it would wrap round to 384 and give 0.
	Check(NetTraffic_PerSecond(0, 0) == 0 && NetTraffic_PerSecond(UINT64_MAX, 1000) == 0xFFFFFFFFu && NetTraffic_PerSecond(5000000000ull, 1000) == 0xFFFFFFFFu,
		"a figure for one second is at most what 32 bits hold, and a window of no time gives 0");
	Check(NetTraffic_PerSecond(18446744073709552ull, 1000) == 0xFFFFFFFFu, "a count too big to work out in 64 bits is the most there is, not a wrapped-round small one");

	// A new host starts again
	NetTraffic_Start(&t, 0, 0, 0, 0, 9000);
	Check(t.packetsIn == 0 && t.bytesOut == 0 && t.packetsInLastSecond == 0, "a new host's counts start again from 0");

	if (checksFailed > 0)
	{
		printf("%d check%s FAILED\n", checksFailed, checksFailed == 1 ? "" : "s");
		return 1;
	}

	printf("Every check passed\n");
	return 0;
}
