#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>

#include "packet.h"
#include "checksum.h"
#include "window.h"

Packet_t*
buildPacketHeader(
    Packet_t* packetPtr,
    SeqNum_t seqNum,
    uint8_t flag
){
    memset(packetPtr, 0, PACKET_MAX_SSIZE);

    packetPtr->header.seqNum = htonl(seqNum);
    packetPtr->header.cksum = 0;
    packetPtr->header.flag = flag;

    return packetPtr;
}

Packet_t*
buildRrPacket(
    Packet_t* packetPtr,
    SeqNum_t seqNum,
    SeqNum_t rrSeqNum
){
    buildPacketHeader(packetPtr, seqNum, FLAG_TYPE_RR);

    packetPtr->payload.rr.seqNum = htonl(rrSeqNum);

    packetPtr->header.cksum = in_cksum((uint16_t*) packetPtr, RR_PACKET_SSIZE);

    return packetPtr;
}

Packet_t*
buildSrejPacket(
    Packet_t* packetPtr,
    SeqNum_t seqNum,
    SeqNum_t srejSeqNum
){
    buildPacketHeader(packetPtr, seqNum, FLAG_TYPE_SREJ);

    packetPtr->payload.srej.seqNum = htonl(srejSeqNum);

    packetPtr->header.cksum = in_cksum((uint16_t*) packetPtr, SREJ_PACKET_SSIZE);

    return packetPtr;
}

Packet_t*
buildDataPacket(
    Packet_t* packetPtr,
    SeqNum_t seqNum,
    uint8_t* dataPtr,
    uint16_t dataSize
){
    buildPacketHeader(packetPtr, seqNum, FLAG_TYPE_DATA);

    // Populate packet
    memcpy(&packetPtr->payload, dataPtr, dataSize);

    // Calculate checksum
    packetPtr->header.cksum = in_cksum((uint16_t*) packetPtr, DATA_PACKET_SSIZE(dataSize));

    return packetPtr;
}

Packet_t*
buildFileNameRespPacket(
    Packet_t* packetPtr,
    SeqNum_t seqNum,
    bool response
){
    buildPacketHeader(packetPtr, seqNum, FLAG_TYPE_FILENAME_RESP);

    //Populate packet
    packetPtr->payload.fileNameResponse.response = response;

    // Calculate checksum
    packetPtr->header.cksum = in_cksum((uint16_t*) packetPtr, FILENAME_RESP_PACKET_SSIZE);

    return packetPtr;
}

Packet_t*
buildFileNamePacket(
    Packet_t* packetPtr,
    SeqNum_t seqNum,
    uint32_t windowSize,
    uint16_t bufferSize,
    uint8_t* fileNamePtr,
    uint8_t fileNameSize
){
    buildPacketHeader(packetPtr, seqNum, FLAG_TYPE_FILENAME);

    packetPtr->payload.fileName.bufferSize = bufferSize;
    packetPtr->payload.fileName.windowSize = windowSize;

    memcpy(&packetPtr->payload.fileName.fileName, fileNamePtr, fileNameSize);

    packetPtr->header.cksum = in_cksum((uint16_t*) packetPtr, FILENAME_PACKET_SSIZE(fileNameSize));

    return packetPtr;
}

bool
isValidPacket(
    Packet_t* packetPtr,
    uint16_t packetSize
){
    if(in_cksum((uint16_t*) packetPtr, packetSize) == 0) {
        return true;
    }
    return false;
}
