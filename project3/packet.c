#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>

#include "packet.h"
#include "checksum.h"
#include "window.h"

Packet_t* buildRrPacket(Packet_t* packetPtr, SeqNum_t seqNum, SeqNum_t rrSeqNum)
{
	packetPtr->header.seqNum = htonl(seqNum);
	packetPtr->header.cksum = 0;
	packetPtr->header.flag = FLAG_TYPE_RR;

	packetPtr->payload.rr.seqNum = htonl(rrSeqNum);
	
	packetPtr->header.cksum = in_cksum((uint16_t*) packetPtr, RR_PACKET_SIZE);

	return packetPtr;
}

Packet_t* buildSrejPacket(Packet_t* packetPtr, SeqNum_t seqNum, SeqNum_t srejSeqNum)
{
	packetPtr->header.seqNum = htonl(seqNum);
	packetPtr->header.cksum = 0;
	packetPtr->header.flag = FLAG_TYPE_RR;

	packetPtr->payload.srej.seqNum = htonl(srejSeqNum);
	
	packetPtr->header.cksum = in_cksum((uint16_t*) packetPtr, SREJ_PACKET_SIZE);

	return packetPtr;
}

Packet_t* buildDataPacket(Packet_t* packetPtr, SeqNum_t seqNum, uint8_t* dataPtr, uint16_t dataSize)
{
	// Populate packet
	packetPtr->header.seqNum = htonl(seqNum);
	packetPtr->header.cksum = 0;
	packetPtr->header.flag = FLAG_TYPE_DATA;
	memcpy(&packetPtr->payload, dataPtr, dataSize);

	// Calculate checksum
	packetPtr->header.cksum = in_cksum((uint16_t*) packetPtr, DATA_PACKET_SIZE(dataSize));

	return packetPtr;
}

Packet_t* buildFileNamePacket(Packet_t* packetPtr, SeqNum_t seqNum, uint16_t bufferSize, uint8_t* fileNamePtr, uint8_t fileNameSize)
{	
	packetPtr->header.seqNum = htonl(seqNum);
	packetPtr->header.cksum = 0;
	packetPtr->header.flag = FLAG_TYPE_FILENAME;

	packetPtr->payload.fileName.bufferSize = bufferSize;
	packetPtr->payload.fileName.windowSize = getWindowSize();

	memcpy(&packetPtr->payload.fileName.fileName, fileNamePtr, fileNameSize);

	return packetPtr;
}

bool isValidPacket(Packet_t* packetPtr, uint16_t packetSize)
{
	if(in_cksum((uint16_t*) packetPtr, packetSize) == 0) {
		return true;
	}

	return false;
}
