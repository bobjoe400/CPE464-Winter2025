#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "window.h"
#include "packet.h"

static Window_t window;

void windowInit(uint32_t windowSize, uint16_t bufferSize)
{
	window.windowSize = windowSize;
	window.bufferSize = bufferSize;

	window.windowState.lower = SEQ_NUM_START;
	window.windowState.current = SEQ_NUM_START;
	window.windowState.upper = window.windowState.lower + windowSize;

	size_t elemSize = WINDOW_ELEMENT_SSIZE(window);
    size_t packetSize = WINDOW_ELEMENT_PACKET_SSIZE(window);
    printf("Allocated per element: %zu bytes, packet portion: %zu bytes\n", elemSize, packetSize);
	
	window.elements = (WindowElement_t*) malloc(WINDOW_SSIZE(window));

	memset(window.elements, 0, WINDOW_SSIZE(window));
}

uint32_t getWindowSize()
{
	return window.windowSize;
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

	memcpy(WINDOW_ELEMENT_PACKET_INDEX(window, WINDOW_INDEX(packetPtr, window)), packetPtr, WINDOW_ELEMENT_PACKET_SSIZE(window));
	window.elements[WINDOW_INDEX(packetPtr, window)].valid = false;

	window.windowState.current++;

	return true;
}

Packet_t* getPacket(Packet_t* packetPtr, SeqNum_t seqNum)
{
	memcpy(packetPtr, WINDOW_ELEMENT_PACKET_INDEX(window, seqNum % window.windowSize), WINDOW_ELEMENT_PACKET_SSIZE(window));

	return packetPtr;
}

Packet_t* getLowestPacket(Packet_t* lowestPacketPtr)
{
	memcpy(lowestPacketPtr, WINDOW_ELEMENT_PACKET_INDEX(window, WINDOW_LOWEST_PACKET_INDEX(window)), WINDOW_ELEMENT_PACKET_SSIZE(window));

	return lowestPacketPtr;
}

void removePacket(SeqNum_t seqNum)
{	
	for(uint32_t i = window.windowState.lower; i < seqNum; i++){
		window.elements[i % window.windowSize].valid = true;
		memset(WINDOW_ELEMENT_PACKET_INDEX(window, i % window.windowSize), 0, WINDOW_ELEMENT_PACKET_SSIZE(window));
	}

	window.windowState.lower = seqNum;
	window.windowState.upper = seqNum + window.windowSize;
}

uint32_t getInvalidPackets(InvalidPacket_t* invalidPacketArray){
	uint32_t invalidPacketArrayIdx = 0;

	invalidPacketArray = (InvalidPacket_t*) malloc(sizeof(InvalidPacket_t));

	for(uint32_t i = window.windowState.lower; i <= window.windowState.current; i++){
		WindowElement_t* currElement = &window.elements[i];

		if (currElement->valid == false){
			if (invalidPacketArrayIdx > 0) {
				invalidPacketArray = (InvalidPacket_t*) realloc(invalidPacketArray, sizeof(InvalidPacket_t) *  (invalidPacketArrayIdx + 1));
			}
			
			invalidPacketArray[invalidPacketArrayIdx].seqNum = currElement->packet.header.seqNum;
			invalidPacketArray[invalidPacketArrayIdx].valid = false;

			invalidPacketArrayIdx++;
		}
	}

	if (invalidPacketArrayIdx == 0){
		free(invalidPacketArray);
	}

	return invalidPacketArrayIdx;
}