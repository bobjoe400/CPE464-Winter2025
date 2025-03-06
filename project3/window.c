#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "window.h"
#include "packet.h"

Window_t window;

void windowInit(uint32_t windowSize, uint16_t bufferSize)
{
	window.windowSize = windowSize;
	window.bufferSize = bufferSize;

	window.windowState.lower = SEQ_NUM_START;
	window.windowState.current = SEQ_NUM_START;
	window.windowState.upper = window.windowState.lower + windowSize;
	
	window.elements = (WindowElement_t*) malloc(WINDOW_SSIZE(window));

	for(uint32_t i = 0; i < windowSize; i++){
		window.elements[i % windowSize].valid = false;
		
		WINDOW_ELEMENT_PACKET(window, i % windowSize) = (Packet_t*) malloc(WINDOW_ELEMENT_PACKET_SSIZE(window));
		memset(WINDOW_ELEMENT_PACKET(window, i % windowSize), 0, WINDOW_ELEMENT_PACKET_SSIZE(window));
	}
}

void windowDestroy(){
	for(uint32_t i = 0; i < window.windowSize; i++){
		free(window.elements[i].packet);
	}
	free(window.elements);
}

uint32_t getWindowSize()
{
	return window.windowSize;
}

uint16_t getPacketSize()
{
	return WINDOW_ELEMENT_PACKET_SSIZE(window);
}

bool isWindowOpen()
{
	return (window.windowState.current != window.windowState.upper);
}

bool addPacket(Packet_t* packetPtr)
{	
	if(!isWindowOpen()){
		return false;
	}

	window.elements[WINDOW_INDEX(packetPtr, window)].valid = false;

	memcpy(WINDOW_ELEMENT_PACKET(window, WINDOW_INDEX(packetPtr, window)), packetPtr, WINDOW_ELEMENT_PACKET_SSIZE(window));

	window.windowState.current++;

	return true;
}

Packet_t* getPacket(Packet_t* packetPtr, SeqNum_t seqNum)
{
	memcpy(packetPtr, WINDOW_ELEMENT_PACKET(window, seqNum % window.windowSize), WINDOW_ELEMENT_PACKET_SSIZE(window));

	return packetPtr;
}

Packet_t* getLowestPacket(Packet_t* lowestPacketPtr)
{
	memcpy(lowestPacketPtr, WINDOW_ELEMENT_PACKET(window, WINDOW_LOWEST_PACKET_INDEX(window)), WINDOW_ELEMENT_PACKET_SSIZE(window));

	return lowestPacketPtr;
}

void removePacket(SeqNum_t seqNum)
{	
	for(uint32_t i = window.windowState.lower; i < seqNum; i++){
		window.elements[i % window.windowSize].valid = true;
	}

	window.windowState.lower = seqNum;
	window.windowState.upper = seqNum + window.windowSize;
}

InvalidPacket_t* getInvalidPackets(InvalidPacket_t* invalidPacketArray, uint32_t* numPackets){
	uint32_t invalidPacketArrayIdx = 0;

	invalidPacketArray = (InvalidPacket_t*) malloc(sizeof(InvalidPacket_t));

	for(uint32_t i = window.windowState.lower; i <= window.windowState.current; i++){
		WindowElement_t currElement = window.elements[i % window.windowSize];

		if (currElement.valid == false){
			if (invalidPacketArrayIdx > 0) {
				invalidPacketArray = (InvalidPacket_t*) realloc(invalidPacketArray, sizeof(InvalidPacket_t) *  (invalidPacketArrayIdx + 1));
			}
			
			invalidPacketArray[invalidPacketArrayIdx].seqNum = ntohl(currElement.packet->header.seqNum);
			invalidPacketArray[invalidPacketArrayIdx].valid = false;

			invalidPacketArrayIdx++;
		}
	}

	if (invalidPacketArrayIdx == 0){
		free(invalidPacketArray);
		return NULL;
	}

	*numPackets = invalidPacketArrayIdx;
	return invalidPacketArray;
}