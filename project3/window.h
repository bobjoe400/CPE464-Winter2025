#ifndef WINDOWBUFFER_H
#define WINDOWBUFFER_H

#include <stdbool.h>
#include <arpa/inet.h>

#include "packet.h"

#define WINDOW_SIZE_MAX_EXP 30
#define WINDOW_SIZE_MAX 1 << WINDOW_SIZE_MAX_EXP

#pragma pack(push, 1)
typedef struct{
	bool valid;
	SeqNum_t seqNum;
} InvalidPacket_t;

typedef struct {
	bool valid;
	Packet_t* packet;
} WindowElement_t;

typedef struct {
	uint32_t lower;
	uint32_t current;
	uint32_t upper;
} WindowState_t;

typedef struct {
	uint32_t windowSize;
	uint16_t bufferSize;
	WindowState_t windowState;
	WindowElement_t* elements;
} Window_t;

#pragma pack(pop)

#define WINDOW_ELEMENT_SSIZE(x) (sizeof(WindowElement_t))
#define WINDOW_ELEMENT_PACKET_SSIZE(x) (PACKET_MAX_SIZE - PAYLOAD_MAX + x.bufferSize)
#define WINDOW_SSIZE(x) (WINDOW_ELEMENT_SSIZE(x) * x.windowSize)

#define WINDOW_INDEX(x, y) (ntohl(x->header.seqNum) % y.windowSize)
#define WINDOW_CURRENT_PACKET_INDEX(x) (x.windowState.current % x.windowSize)
#define WINDOW_LOWEST_PACKET_INDEX(x) (x.windowState.lower % x.windowSize)

#define WINDOW_ELEMENT_PACKET(x, y) (x.elements[y].packet)

void windowInit(uint32_t windowSize, uint16_t bufferSize);
void windowDestroy(void);

uint32_t getWindowSize(void);
uint16_t getPacketSize(void);
bool isWindowOpen(void);

bool addPacket(Packet_t* packetPtr);

Packet_t* getPacket(Packet_t* packetPtr, SeqNum_t seqNum);
Packet_t* getLowestPacket(Packet_t* lowestPacketPtr);

void removePacket(SeqNum_t seqNum);

InvalidPacket_t* getInvalidPackets(InvalidPacket_t* invalidPacketArray, uint32_t* numPackets);

#endif // WINDOWBUFFER_H