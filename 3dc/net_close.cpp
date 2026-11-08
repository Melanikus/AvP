// Closing a network game's connections without losing what was said last. Written 2026-11-07 by Melanikus for the AvP port.
// net_close.h says why this is needed.

#include "net_close.h"

#include <windows.h>

// A connection part of the way through closing: told to close once its messages have been heard, saying goodbye and waiting for
// the other end to answer, or closed and waiting for ENet to report it
static bool IsStillClosing(ENetPeer const *peer)
{
	switch (peer->state)
	{
		case ENET_PEER_STATE_DISCONNECT_LATER:
		case ENET_PEER_STATE_DISCONNECTING:
		case ENET_PEER_STATE_ACKNOWLEDGING_DISCONNECT:
		case ENET_PEER_STATE_ZOMBIE:
			return true;
		default:
			return false;
	}
}

int Net_CloseConnectionsAfterWhatsWaiting(ENetHost *host, unsigned int mostMilliseconds)
{
	if (host == NULL)
	{
		return 0;
	}

	// Only an open connection is asked. One still being made, or already closed, has nothing to say, and destroying the host ends it.
	for (ENetPeer *peer = host->peers; peer < &host->peers[host->peerCount]; ++peer)
	{
		if (peer->state == ENET_PEER_STATE_CONNECTED)
		{
			enet_peer_disconnect_later(peer, 0);
		}
	}

	// GetTickCount goes back to 0 every 49 days. Taking one reading from a later one still gives the time between them, even then.
	DWORD const startedAt = GetTickCount();

	for (;;)
	{
		int closing = 0;

		for (ENetPeer *peer = host->peers; peer < &host->peers[host->peerCount]; ++peer)
		{
			if (IsStillClosing(peer))
			{
				closing++;
			}
		}

		if (closing == 0 || GetTickCount() - startedAt >= mostMilliseconds)
		{
			return closing;
		}

		// This is where ENet sends what's waiting and takes in the other ends' answers. It waits up to 5 ms for one.
		ENetEvent event;

		if (enet_host_service(host, &event, 5) < 0)
		{
			// The network itself has failed on this machine, so nothing more can be sent or heard
			return closing;
		}

		if (event.type == ENET_EVENT_TYPE_RECEIVE)
		{
			enet_packet_destroy(event.packet);
		}
	}
}
