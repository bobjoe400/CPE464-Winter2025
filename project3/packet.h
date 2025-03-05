#ifndef PACKET_H
#define PACKET_H

#include <stdint.h>
#include <stdbool.h>

// --- Constants ---
#define PAYLOAD_MIN 1
#define PAYLOAD_MAX 1400

#define FILENAME_MAX_LEN 100

#define FLAG_SIZE 8

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

	FLAG_TYPE_MAX
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
	uint8_t fileName[FILENAME_MAX_LEN];
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
	uint16_t cksum;
	uint8_t flag : FLAG_SIZE;
} PacketHeader_t;

typedef struct {
	PacketHeader_t header;
	PacketTypes_u payload;
} Packet_t;

#pragma pack(pop)

// --- Packet Sizes ---
#define PACKET_MAX_SIZE sizeof(Packet_t)

#define PACKET_HEADER_SIZE sizeof(PacketHeader_t)

#define RR_PACKET_SIZE (PACKET_HEADER_SIZE + sizeof(RrPacket_t))
#define SREJ_PACKET_SIZE (PACKET_HEADER_SIZE + sizeof(SrejPacket_t))
#define DATA_PACKET_SIZE(x) (PACKET_HEADER_SIZE + x)
#define FILENAME_PACKET_SIZE(x) (PACKET_HEADER_SIZE + sizeof(FileNamePacket_t) - FILENAME_MAX_LEN + x)

Packet_t* buildRrPacket(Packet_t* packetPtr, SeqNum_t seqNum, SeqNum_t rrSeqNum);
Packet_t* buildSrejPacket(Packet_t* packetPtr, SeqNum_t seqNum, SeqNum_t srejSeqNum);
Packet_t* buildDataPacket(Packet_t* packetPtr, SeqNum_t seqNum, uint8_t* dataPtr, uint16_t dataSize);
Packet_t* buildFileNamePacket(Packet_t* packetPtr, SeqNum_t seqNum, uint16_t bufferSize, uint8_t* fileNamePtr, uint8_t fileNameSize);

bool isValidPacket(Packet_t* packetPtr, uint16_t packetSize);

#endif //PACKET_H