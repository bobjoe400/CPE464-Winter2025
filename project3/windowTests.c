#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#include "packet.h"
#include "window.h"

int main(void) {
    // --- Initialize the window ---
    uint32_t winSize = 3;        // small window size for testing
    uint16_t bufSize = 128;      // example buffer size
    windowInit(winSize, bufSize);
    printf("Window initialized with size: %u, buffer size: %u\n", winSize, bufSize);


    // --- Build and test a Data Packet ---
    Packet_t dataPkt;
    uint8_t testData[] = "Test data payload";
    uint16_t dataSize = strlen((char*)testData) + 1; // include null terminator
    buildDataPacket(&dataPkt, 0, testData, dataSize);
    
    if(isValidPacket(&dataPkt, DATA_PACKET_SIZE(dataSize))) {
        printf("Data packet built and is valid.\n");
    } else {
        printf("Data packet built but is invalid.\n");
    }
    
    if(addPacket(&dataPkt)) {
        printf("Data packet added to window buffer.\n");
    } else {
        printf("Failed to add data packet to window buffer.\n");
    }

    // --- Build and test a RR Packet ---
    Packet_t rrPkt;
    buildRrPacket(&rrPkt, 1, 0);
    if(isValidPacket(&rrPkt, RR_PACKET_SIZE)) {
        printf("RR packet built and is valid.\n");
    } else {
        printf("RR packet built but is invalid.\n");
    }
    
    if(addPacket(&rrPkt)) {
        printf("RR packet added to window buffer.\n");
    } else {
        printf("Failed to add RR packet to window buffer.\n");
    }
    
    // --- Build and test a SREJ Packet ---
    Packet_t srejPkt;
    buildSrejPacket(&srejPkt, 2, 1);
    if(isValidPacket(&srejPkt, SREJ_PACKET_SIZE)) {
        printf("SREJ packet built and is valid.\n");
    } else {
        printf("SREJ packet built but is invalid.\n");
    }
    
    if(addPacket(&srejPkt)) {
        printf("SREJ packet added to window buffer.\n");
    } else {
        printf("Failed to add SREJ packet to window buffer.\n");
    }
    
    // --- Retrieve the lowest packet from the window ---
    Packet_t lowestPkt;
    getLowestPacket(&lowestPkt);
    printf("Lowest packet retrieved from window buffer: SeqNum = %u\n", ntohl(lowestPkt.header.seqNum));

    // --- Remove packets from the window ---
    removePacket(2);  // remove packets with sequence numbers less than 2
    printf("Removed packets with sequence numbers lower than 2.\n");

    // --- Get and print the count of invalid packets ---
    InvalidPacket_t* invalidPacketArray = NULL;

    uint32_t invalidCount = getInvalidPackets(invalidPacketArray);
    printf("Number of invalid packets in window: %u\n", invalidCount);
    
    // --- Build and test a FileName Packet ---
    Packet_t filePkt;
    uint8_t fileName[] = "example.txt";
    uint8_t fileNameSize = strlen((char*)fileName) + 1;
    buildFileNamePacket(&filePkt, 3, bufSize, fileName, fileNameSize);
    printf("Filename packet built with buffer size: %u and window size: %u\n",
           filePkt.payload.fileName.bufferSize,
           filePkt.payload.fileName.windowSize);

    return 0;
}
