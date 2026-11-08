// The one place the game makes an ENet host, with the game's own limits. Written 2026-11-07 by Melanikus for the AvP port.
//
// ENet is the library that carries the game's messages over the network. An "ENet host" is one end of that: the machine
// hosting a game makes one for others to connect to, and a joining machine makes one to connect from. ENet's own defaults
// are generous: messages of up to 32 MB, up to 255 channels a connection, and no checksum. Each of those lets someone who
// connects make the game hold or do far more than any real game needs. So the game makes both kinds here, the same way, and
// tests/enet_hostile_test.cpp makes its hosts here too, so that it tests what the game really uses.

#ifndef NET_HOST_H
#define NET_HOST_H

#include <enet/enet.h>

// The game sends everything on channel 0, so one channel is all a connection gets, and one that asks for more is given one.
// ENet keeps messages waiting on each channel separately, so every extra channel was one more place to pile them up.
const size_t kNetChannels = 1;

// One address can hold this many of the host's places. People sharing an address at home get in, and someone flooding
// from one address can't take them all.
const size_t kMostConnectionsFromOneAddress = 4;

// How much ENet may keep waiting for the game to read, from one connection. With our ENet patch 005 this counts ENet's own
// record of each message as well as its bytes, so it limits memory, not only bytes. A level load is the longest wait there
// is. ENet's own limit is 32 MB.
const size_t kMostBytesWaitingForTheGame = 8 * 1024 * 1024;

// Makes an ENet host with the game's limits.
// - address: where others reach it. A client passes NULL, and gets whatever port is free.
// - connections: how many places it has for other machines.
// - mostMessageBytes: the largest message the game sends. ENet refuses anything larger before putting it together.
// - incomingBandwidth and outgoingBandwidth: the player's settings, in bytes a second. 0 means "as much as there is".
// Gives back NULL if ENet couldn't make it.
ENetHost *Net_MakeENetHost(const ENetAddress *address, size_t connections, size_t mostMessageBytes, enet_uint32 incomingBandwidth, enet_uint32 outgoingBandwidth);

#endif
