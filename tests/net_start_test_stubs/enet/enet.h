// A stand-in for the ENet library, for tests/net_start_test.cpp. It opens nothing on the network: each call only counts what the game asked for, so that the test can say which sockets would have been open.
// Written 2026-11-06 by Melanikus for the AvP port. These are only the parts 3dc/networking.cpp uses, with the same names and meanings as the real enet.h.

#ifndef NET_START_FAKE_ENET_H
#define NET_START_FAKE_ENET_H

#include <stddef.h>
#include <stdint.h>

typedef uint32_t enet_uint32;
typedef int ENetSocket;

#define ENET_HOST_ANY 0u
#define ENET_HOST_BROADCAST 0xFFFFFFFFu
#define ENET_SOCKET_NULL (-1)

enum { ENET_SOCKET_TYPE_DATAGRAM = 2 };
enum { ENET_SOCKET_WAIT_RECEIVE = 2 };
enum { ENET_SOCKOPT_NONBLOCK = 1, ENET_SOCKOPT_BROADCAST = 2 };
enum { ENET_PACKET_FLAG_RELIABLE = 1 };
enum ENetPeerState { ENET_PEER_STATE_DISCONNECTED = 0, ENET_PEER_STATE_ACKNOWLEDGING_CONNECT = 2, ENET_PEER_STATE_CONNECTION_PENDING = 3, ENET_PEER_STATE_CONNECTED = 5 };
enum ENetEventType { ENET_EVENT_TYPE_NONE = 0, ENET_EVENT_TYPE_CONNECT = 1, ENET_EVENT_TYPE_DISCONNECT = 2, ENET_EVENT_TYPE_RECEIVE = 3 };

struct ENetAddress
{
	enet_uint32 host;
	uint16_t port;
};

struct ENetBuffer
{
	void *data;
	size_t dataLength;
};

struct ENetPacket
{
	uint8_t *data;
	size_t dataLength;
	size_t referenceCount;
};

// The parts of the real one that networking.cpp uses
struct ENetPeer
{
	void *link[2];
	ENetAddress address;
	void *data;
	ENetPeerState state;
	enet_uint32 roundTripTime;   // for a tester's mark (Net_DescribeConnections)
};

struct ENetHost
{
	size_t maximumPacketSize;
	size_t maximumWaitingData;
	enet_uint32 (*checksum)(const ENetBuffer *buffers, size_t bufferCount);
	size_t duplicatePeers;
	enet_uint32 olderConnectsRefused;   // our ENet patch 006: CONNECTs from an older version, told no
	ENetPeer *peers;
	size_t peerCount;
};

struct ENetEvent
{
	ENetEventType type;
	ENetPeer *peer;
	ENetPacket *packet;
};

int enet_initialize();
void enet_deinitialize();

ENetHost *enet_host_create(const ENetAddress *address, size_t peerCount, size_t channelLimit, enet_uint32 incomingBandwidth, enet_uint32 outgoingBandwidth);
void enet_host_destroy(ENetHost *host);
ENetPeer *enet_host_connect(ENetHost *host, const ENetAddress *address, size_t channelCount, enet_uint32 data);
int enet_host_service(ENetHost *host, ENetEvent *event, enet_uint32 timeout);
int enet_host_check_events(ENetHost *host, ENetEvent *event);
void enet_host_broadcast(ENetHost *host, uint8_t channel, ENetPacket *packet);
void enet_host_flush(ENetHost *host);

ENetPacket *enet_packet_create(const void *data, size_t dataLength, enet_uint32 flags);
void enet_packet_destroy(ENetPacket *packet);
int enet_peer_send(ENetPeer *peer, uint8_t channel, ENetPacket *packet);
void enet_peer_disconnect(ENetPeer *peer, enet_uint32 data);
void enet_peer_reset(ENetPeer *peer);
enet_uint32 enet_crc32(const ENetBuffer *buffers, size_t bufferCount);
void enet_peer_timeout(ENetPeer *peer, enet_uint32 limit, enet_uint32 minimum, enet_uint32 maximum);
enet_uint32 enet_time_get();
void enet_peer_disconnect_now(ENetPeer *peer, enet_uint32 data);

int enet_address_get_host_ip(const ENetAddress *address, char *hostName, size_t nameLength);
int enet_address_set_host_ip(ENetAddress *address, const char *hostName);

ENetSocket enet_socket_create(int type);
int enet_socket_set_option(ENetSocket socket, int option, int value);
int enet_socket_bind(ENetSocket socket, const ENetAddress *address);
void enet_socket_destroy(ENetSocket socket);
int enet_socket_wait(ENetSocket socket, enet_uint32 *condition, enet_uint32 timeout);
int enet_socket_receive(ENetSocket socket, ENetAddress *address, ENetBuffer *buffers, size_t bufferCount);
int enet_socket_send(ENetSocket socket, const ENetAddress *address, const ENetBuffer *buffers, size_t bufferCount);

#endif
