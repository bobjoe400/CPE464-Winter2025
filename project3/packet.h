#ifndef PACKET_H
#define PACKET_H

#include <stdint.h>

// --- Constants ---
#define PAYLOAD_MIN 1
#define PAYLOAD_MAX 1400

#define FILENAME_MAX 100

#define FLAG_SIZE sizeof(uint8_t)

typedef uint32_t SeqNum_t;

#pragma pack(push, 1)

// --- Flags ---
typedef enum FlagTypes {
	FLAG_TYPE_RR = 5,
	FLAG_TYPE_SREJ = 6,
	FLAG_TYPE_FILENAME = 8,
	FLAG_TYPE_FILENAME_RESP = 9,
	FLAG_TYPE_EOF = 10,
	FLAG_TYPE_DATA = 16, 
	FLAG_TYPE_SREJ_DATA = 17,
	FLAG_TYPE_TIMEOUT_DATA = 18,
	FLAG_TYPE_CUSTOM_START = 31,
	// --- Custom Flags ---
} FlagTypes_e;

// --- Packet Structures ---
typedef struct {
	SeqNum_t seqNum;
} RrPacket_t;

typedef struct {
	SeqNum_t seqNum;
} SrejPacket_t;

typedef struct {
	uint8_t payload[PAYLOAD_MAX];
} DataPacket_t;

typedef struct {
	uint32_t windowSize;
	uint16_t bufferSize;
	uint8_t fileName[FILENAME_MAX];
} FileNamePacket_t;

typedef union {
	RrPacket_t rr;
	SrejPacket_t srej;
	DataPacket_t data;
	FileNamePacket_t fileName;
} PacketTypes_u;

// --- Top Level Packet ---
typedef struct {
	SeqNum_t seqNum;
	uint16_t chksum;
	FlagTypes_e flag : FLAG_SIZE;
} PacketHeader_t;

typedef struct {
	PacketHeader_t header;
	PacketTypes_u payload;
} Packet_t;

#define PACKET_MAX_SIZE sizeof(packet_t)

#pragma pack(pop)

Packet_t* build_packet(Packet_t* packetPtr, SeqNum_t seqNum, FlagTypes_e flag, uint8_t* dataPtr);
Packet_t* process_packet(Packet_t* packetPtr, uint8_t* dataPtr);

#endif //PACKET_H