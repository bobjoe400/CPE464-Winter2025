#include "windowBuffer.h"
#include "stdlib.h"
#include "string.h"

static Window_t window;

void windowInit(uint32_t windowSize, uint16_t bufferSize)
{
	window.windowSize = windowSize;
	window.bufferSize = bufferSize;

	window.windowState.lower = 0;
	window.windowState.current = 0;
	window.windowState.upper = windowSize - 1;
	
	window.elements = (WindowElement_t*) malloc(WINDOW_SSIZE(window));

	memset(window.elements, 0, WINDOW_SSIZE(window));
}

uint32_t getWindowSize()
{
	return window.windowSize;
}

bool isWindowOpen()
{
	return (window.windowState.current == window.windowState.upper);
}

bool addPacket(Packet_t* packetPtr)
{	
	if(!isWindowOpen()){
		return false;
	}

	memcpy(&window.elements[WINDOW_INDEX(packetPtr, window)], packetPtr, WINDOW_ELEMENT_SSIZE(window));
	window.elements[WINDOW_INDEX(packetPtr, window)].valid = false;

	return true;
}

Packet_t* getPacket(Packet_t* packetPtr, SeqNum_t seqNum)
{
	memcpy(packetPtr, &window.elements[seqNum % window.windowSize].packet, WINDOW_ELEMENT_SSIZE(window));

	return packetPtr;
}

Packet_t* getNextPacket(Packet_t* nextPacketPtr)
{
	memcpy(nextPacketPtr, &window.elements[WINDOW_NEXT_PACKET_INDEX(window)].packet, WINDOW_ELEMENT_SSIZE(window));

	window.windowState.current++;

	return nextPacketPtr;
}

void removePacket(SeqNum_t seqNum)
{	
	for(int i = window.windowState.lower; i < seqNum; i++){
		window.elements[i % window.windowSize].valid = true;
	}

	window.windowState.lower = seqNum;
	window.windowState.upper += seqNum + window.windowSize;
}

uint32_t getInvalidPackets(InvalidPacket_t* invalidPacketArray){
	uint32_t invalidPacketArrayIdx = 0;

	invalidPacketArray = malloc(sizeof(InvalidPacket_t));

	for(int i = 0; i < window.windowSize; i++){
		WindowElement_t* currElement = &window.elements[i];

		if (currElement->valid == false){
			if (invalidPacketArrayIdx > 0) {
				invalidPacketArray = realloc(invalidPacketArray, sizeof(InvalidPacket_t) *  (invalidPacketArrayIdx + 1));
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