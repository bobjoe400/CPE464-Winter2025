#ifndef WINDOWBUFFER_H
#define WINDOWBUFFER_H

#include <stdbool.h>

#include "packet.h"

#define WINDOW_SIZE_MAX_EXP 30
#define WINDOW_SIZE_MAX 1 << WINDOW_SIZE_MAX_EXP

#define BUFFER_SIZE_MAX PAYLOAD_MAX

#pragma pack(push, 1)
typedef struct {
	bool valid;
	SeqNum_t seqNum;
	Packet_t packet;
} WindowElement_t;

typedef struct {
	uint32_t windowLower;
	uint32_t windowCurrent;
	uint32_t windowUpper;
} WindowState_t;

typedef struct {
	uint32_t windowSize;
	WindowState_t windowState;
	WindowElement_t* elements;
} Window_t;

#pragma pack(pop)

bool window_init(uint32_t windowSize);

bool add_packet(Packet_t* packePtr);
Packet_t* get_packet(Packet_t* packetPtr, SeqNum_t seqNum);
Packet_t* get_next_packet(Packet_t* nextPacketPtr);

bool can_send_packet();
bool can_receive_packet();

#endif // WINDOWBUFFER_H