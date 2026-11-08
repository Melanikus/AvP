// The one place the game makes an ENet host, with the game's own limits. Written 2026-11-07 by Melanikus for the AvP port.
// net_host.h says why.

#include "net_host.h"

ENetHost *Net_MakeENetHost(const ENetAddress *address, size_t connections, size_t mostMessageBytes, enet_uint32 incomingBandwidth, enet_uint32 outgoingBandwidth)
{
	ENetHost *const host = enet_host_create(address, connections, kNetChannels, incomingBandwidth, outgoingBandwidth);

	if (host == NULL)
	{
		return NULL;
	}

	host->duplicatePeers = kMostConnectionsFromOneAddress;

	host->maximumPacketSize = mostMessageBytes;
	host->maximumWaitingData = kMostBytesWaitingForTheGame;

	// A packet that's been changed on the way, or forged by someone who doesn't know the connection's number, is thrown
	// away. It isn't a password, because anyone who sees the traffic can work the number out.
	host->checksum = enet_crc32;

	return host;
}
