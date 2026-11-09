// net_start_test's stand-in for 3dc/net_relay_v2.h: a scripted session with a relay and a scripted picker, with the real one's calls.
// Written 2026-11-09 by Melanikus for the AvP port (protocol version 2's draft, step 7).
//
// WHY A STAND-IN. net_start_test tries networking.cpp's wiring of the channel to the relay (v2-step7-design.md): keys before anything,
// every version 1 message sealed, only opened messages read, the relay's datagrams sorted by their first four bytes, and what ends the
// channel. The real session (handshake A, then sealed control) needs a relay's private keys to answer, and there's no C++ relay side, so
// its own test is net_v2a_test (Primary's, against the relay's own vectors). This one only stands in for it here. Primary's conditions
// (2026-11-09):
// - it lives only in net_start_test's stubs folder, never in the game's project (this folder is searched before 3dc, by this test only);
// - it records what the wiring hands it: Start's arguments (the player's pointers among them), each Seal, and each Hear with its port;
// - the test can tell it to go Ready, to fail (7), or to give TakeOpened messages.
//
// ITS DATAGRAMS. Everything it sends starts 'AVPR', as the real one's do, then one byte saying what it stands for: 'H' a handshake datagram,
// 'S' a sealed message (then the message's 64 bytes, plain, so the test can read them), 'C' the picker's cookie ask. A datagram from the
// relay starting 'AVPR' 'S' is a sealed message the stand-in "opens" (the 64 bytes after it), and 'AVPR' 'c' is the cookie round's answer.
#ifndef NET_RELAY_V2_H
#define NET_RELAY_V2_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>

#include "net_crypto.h"
#include "net_relay.h"
#include "net_v2_fragments.h"
#include "net_handshake_a.h"
#include "net_sealed_control.h"

size_t const kNetRelayV2MostSends = 8;
size_t const kNetRelayV2MostOpened = 8;
int const kNetRelayV2NoAnswer = 7;
int const kNetRelayV2BadCookie = 5;
size_t const kNetRelayV2StandInBytes = 5 + kNetSealedControlMessageBytes;   // 'AVPR', the kind, and a message

struct NetRelayV2Send
{
	uint16_t toPort;
	size_t size;
	uint8_t bytes[kNetV2DatagramBytes];
};

struct NetRelayV2Opened
{
	uint16_t fromPort;
	uint8_t message[kNetSealedControlMessageBytes];
};

struct NetRelayV2Drops
{
	uint32_t notFromControl;
	uint32_t notOurs;
	uint32_t refused;
	uint32_t full;
};

// What the wiring handed the stand-in, for the test to read: one record for every session and picker there is (networking.cpp has one of
// each), and the test's own wishes for them
struct NetRelayV2StandIn
{
	// Start's arguments
	int starts;
	std::string region;
	NetRelayStaticKeys keys;
	const void *playerP384;
	const uint8_t *playerStatics;
	const uint8_t *playerKemPrivate;
	uint16_t controlPort;
	bool drawsGiven;
	// Each Seal and each Hear
	std::vector<std::vector<uint8_t>> sealed;
	std::vector<uint16_t> sealedTo;
	std::vector<uint16_t> heardFrom;
	int wipes;
	// The test's wishes
	bool startRefused;
	bool goReady;
	int failWith;
	// The picker's. Unless pickerSilent, the relay answers the cookie round at once, as if its answer had come.
	int pickerStarts;
	uint16_t pickerPort;
	bool pickerAnswered;
	bool pickerSilent;
	uint32_t pickerMs;
};

inline NetRelayV2StandIn netRelayV2StandIn = {};

class NetRelayV2Session
{
public:
	NetRelayV2Session() { Clear(); }

	bool Start(const char *region, const NetRelayStaticKeys *relayKeys, const NetHandshakeAPlayer *me, uint16_t controlPort,
		const uint8_t id[kNetV2IdBytes], NetHandshakeADraws *draws, uint32_t now)
	{
		(void)now;
		NetRelayV2StandIn &s = netRelayV2StandIn;
		s.starts++;
		s.region = region ? region : "";
		memset(&s.keys, 0, sizeof(s.keys));

		if (relayKeys)
		{
			s.keys = *relayKeys;
		}

		s.playerP384 = me ? me->p384 : NULL;
		s.playerStatics = me ? me->statics : NULL;
		s.playerKemPrivate = me ? me->kemPrivate : NULL;
		s.controlPort = controlPort;
		s.drawsGiven = draws != NULL && id != NULL;
		NetHandshakeA_WipeDraws(draws);
		Clear();

		if (s.startRefused || !me || !me->p384 || !me->statics || !me->kemPrivate)
		{
			return false;
		}

		live = true;
		port = controlPort;
		Queue(controlPort, 'H', NULL);
		return true;
	}

	void Hear(uint16_t fromPort, const uint8_t *data, size_t size, uint32_t now)
	{
		(void)now;
		netRelayV2StandIn.heardFrom.push_back(fromPort);

		if (ready && data && size == kNetRelayV2StandInBytes && data[4] == 'S' && openedCount < kNetRelayV2MostOpened)
		{
			opened[openedCount].fromPort = fromPort;
			memcpy(opened[openedCount].message, data + 5, kNetSealedControlMessageBytes);
			openedCount++;
		}
	}

	void Tick(uint32_t now)
	{
		(void)now;

		if (live && netRelayV2StandIn.failWith != 0)
		{
			why = netRelayV2StandIn.failWith;
			failed = true;
			ready = false;
			live = false;
		}
		else if (live && netRelayV2StandIn.goReady)
		{
			ready = true;
		}
	}

	bool Seal(const uint8_t message[kNetSealedControlMessageBytes], uint16_t toPort, uint32_t now)
	{
		(void)now;

		if (!ready)
		{
			return false;
		}

		netRelayV2StandIn.sealed.push_back(std::vector<uint8_t>(message, message + kNetSealedControlMessageBytes));
		netRelayV2StandIn.sealedTo.push_back(toPort);
		Queue(toPort, 'S', message);
		return true;
	}

	size_t TakeSends(NetRelayV2Send *out, size_t most)
	{
		size_t const count = sendCount < most ? sendCount : most;
		memcpy(out, sends, sizeof(sends[0]) * count);
		sendCount = 0;
		return count;
	}

	size_t TakeOpened(NetRelayV2Opened *out, size_t most)
	{
		size_t const count = openedCount < most ? openedCount : most;
		memcpy(out, opened, sizeof(opened[0]) * count);
		openedCount = 0;
		return count;
	}

	bool Ready() const { return ready; }
	bool Failed() const { return failed; }
	int Why() const { return why; }
	const NetRelayV2Drops &Drops() const { return drops; }

	void Wipe()
	{
		netRelayV2StandIn.wipes++;
		Clear();
	}

private:
	void Clear()
	{
		live = ready = failed = false;
		why = 0;
		port = 0;
		sendCount = openedCount = 0;
		memset(&drops, 0, sizeof(drops));
	}

	void Queue(uint16_t toPort, char kind, const uint8_t *message)
	{
		if (sendCount == kNetRelayV2MostSends)
		{
			return;
		}

		NetRelayV2Send &send = sends[sendCount++];
		memset(&send, 0, sizeof(send));
		send.toPort = toPort;
		send.size = kNetRelayV2StandInBytes;
		memcpy(send.bytes, "AVPR", 4);
		send.bytes[4] = static_cast<uint8_t>(kind);

		if (message)
		{
			memcpy(send.bytes + 5, message, kNetSealedControlMessageBytes);
		}
	}

	bool live, ready, failed;
	int why;
	uint16_t port;
	NetRelayV2Send sends[kNetRelayV2MostSends];
	size_t sendCount;
	NetRelayV2Opened opened[kNetRelayV2MostOpened];
	size_t openedCount;
	NetRelayV2Drops drops;
};

struct NetRelayV2PickerSend
{
	uint8_t relay;
	uint16_t port;
	uint8_t bytes[kNetV2DatagramBytes];
};

size_t const kNetRelayV2MostPickerSends = kNetRelayMostRelays;

class NetRelayV2Picker
{
public:
	NetRelayV2Picker() { Reset(); }

	void Reset()
	{
		started = asked = false;
		startedAt = 0;
	}

	bool Start(size_t relayCount, const uint16_t *ports, const uint8_t *ids, uint32_t now)
	{
		Reset();

		if (relayCount != 1 || !ports || !ids)
		{
			return false;
		}

		netRelayV2StandIn.pickerStarts++;
		netRelayV2StandIn.pickerPort = ports[0];
		netRelayV2StandIn.pickerAnswered = !netRelayV2StandIn.pickerSilent;
		port = ports[0];
		started = true;
		startedAt = now;
		return true;
	}

	bool Hear(size_t relay, uint16_t fromPort, const uint8_t *data, size_t size, uint32_t now)
	{
		(void)now;
		bool const answer = started && relay == 0 && fromPort == port && data && size >= 5 && data[4] == 'c';
		netRelayV2StandIn.pickerAnswered = netRelayV2StandIn.pickerAnswered || answer;
		return answer;
	}

	void Tick(uint32_t now) { (void)now; }

	// Done once the relay has answered, or 3.5 s on, as the real one gives up
	bool Done(uint32_t now) const { return started && (netRelayV2StandIn.pickerAnswered || now - startedAt >= 3500); }
	int Best() const { return started && netRelayV2StandIn.pickerAnswered ? 0 : -1; }
	uint32_t BestMs() const { return netRelayV2StandIn.pickerMs; }

	bool RelayMs(size_t relay, uint32_t *ms) const
	{
		*ms = netRelayV2StandIn.pickerMs;
		return relay == 0 && netRelayV2StandIn.pickerAnswered;
	}

	size_t TakeSends(NetRelayV2PickerSend *out, size_t most)
	{
		if (!started || asked || most == 0)
		{
			return 0;
		}

		asked = true;
		memset(&out[0], 0, sizeof(out[0]));
		out[0].relay = 0;
		out[0].port = port;
		memcpy(out[0].bytes, "AVPR", 4);
		out[0].bytes[4] = 'C';
		return 1;
	}

	uint32_t NotOurs() const { return 0; }
	uint32_t SendsDropped() const { return 0; }

private:
	bool started, asked;
	uint32_t startedAt;
	uint16_t port;
};

#endif
