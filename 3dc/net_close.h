// Closing a network game's connections without losing what was said last. Written 2026-11-07 by Melanikus for the AvP port.
//
// When a machine leaves a network game, whatever it said last is often still on its way: a player's "I'm leaving", or a host's
// "that was the last level". There are two quick ways to close, and both lose it:
// - Destroying the connections at once (enet_host_destroy) throws away anything still waiting to be sent. The other end only
//   finds out that this machine has gone when nothing has come from it for a long while.
// - Closing at once (enet_peer_disconnect_now) sends the goodbye straight away. If the other end then reads the last message and
//   the goodbye together, which it does whenever one of its frames was slow, ENet throws the last message away before the game is
//   given it: its handling of a goodbye (enet_protocol_handle_disconnect) empties the connection's queues first.
// So this asks ENet to close each connection only once everything waiting for it has been sent and the other end has said it got
// it (enet_peer_disconnect_later), and then waits for that, but not for long, because a machine that has gone already will never
// answer. tests/net_close_test.cpp runs both quick ways and this one over real ENet on this machine, and shows which keep the last
// message.

#ifndef NET_CLOSE_H
#define NET_CLOSE_H

#include <enet/enet.h>

// Closes every open connection of this ENet host once what's waiting for it has been sent and heard, and waits up to
// mostMilliseconds for them to finish closing. Anything that arrives meanwhile is thrown away, because this machine is leaving.
// Gives back how many were still closing when it stopped waiting: 0 means every one closed properly. It doesn't destroy the
// host; the caller does that next.
int Net_CloseConnectionsAfterWhatsWaiting(ENetHost *host, unsigned int mostMilliseconds);

#endif
