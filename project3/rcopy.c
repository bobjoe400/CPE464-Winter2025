// Client side - UDP Code				    
// By Hugh Smith	4/1/2017		

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "gethostbyname.h"
#include "networks.h"
#include "safeUtil.h"
#include "pollLib.h"

#include "packet.h"
#include "window.h"

#define SERVER_NAME_MAX 1024

typedef struct{
	char fromFileName[FILENAME_MAX_LEN + 1];
	char toFileName[FILENAME_MAX_LEN + 1];
	
	uint32_t windowSize;
	uint16_t bufferSize;

	float errorRate;

	char serverName[SERVER_NAME_MAX + 1];
	uint16_t serverPort;

	int socketNum;
	struct sockaddr_in6* server;
	int serverAddrLen;
}rcopySettings_t;

enum rcopyState{
	STATE_SEND_FILENAME = 0,
	STATE_WAIT_FOR_FILENAME_ACK,
	STATE_RECEIVE_FIRST_DATA,
	STATE_RECEIVE_DATA_TIMEOUT,
	STATE_RECEIVE_DATA,
	STATE_BAD_DATA,
	STATE_PROCESS_DATA,
	STATE_LAST_DATA,
	STATE_KILL,

	NUM_STATES,
};

static rcopySettings_t settings;

bool 
receiveAndValidateData(
	Packet_t* packetPtr,
	uint16_t expectedSize
){	
	bool retVal = true;
	char buffer[expectedSize];

	// call safeRecvFrom
	int dataLen = safeRecvfrom(settings.socketNum, buffer, expectedSize, 0, (struct sockaddr*) settings.server, &settings.serverAddrLen);

	if(dataLen < expectedSize){
		// Short Packet Received
	#ifdef DEBUG_ON
		printf("Error: Less bytes received than expected!\n");
	#endif // DEBUG_ON
		retVal = false;
	}

	// Copy data from buffer into packet struct
	memcpy(packetPtr, buffer, dataLen);

	if(!isValidPacket(packetPtr, dataLen)){
		// Invalid Packet Received
	#ifdef DEBUG_ON
		printf("Error: Malformed packet receieved!\n");
	#endif // DEBUG_ON
		retVal = false;
	}

	retVal = true;
	
#ifdef DEBUG_ON
	char * ipString = NULL;
	ipString = ipAddressToString(settings.server);

	printf("Server with ip: %s and port %d said it received:\n", ipString, ntohs(settings.server->sin6_port));

	// print out bytes received
	for(int i=0; i < dataLen; i++){
		printf("%02x ", buffer[i]);
	}

	printf("\n");
#endif // DEBUG_ON

	return retVal;
}

void
sendFileName(
	void
){
	Packet_t packet;
	int fileNameLen = strlen(settings.fromFileName);
#ifdef DEBUG_ON
	printf("Info: Sending filename: %s", settings.fromFileName);
#endif // DEBUG_ON

	buildFileNamePacket(&packet, 0, settings.windowSize, settings.bufferSize, (uint8_t*) settings.fromFileName, fileNameLen);

	int packetSize = FILENAME_PACKET_SSIZE(fileNameLen);

	safeSendto(settings.socketNum, (uint8_t*) &packet, packetSize, 0, (struct sockaddr*) settings.server, settings.serverAddrLen);
}

int 
waitForFileNameAck(
	Packet_t* packetPtr
){
	if(pollCall(1) < 0){
		// Timeout
	#ifdef DEBUG_ON
		printf("Timeout: Filename response timed out! Resending filename...\n");
	#endif // DEBUG_ON

		return STATE_SEND_FILENAME;
	} else {
		if(!receiveAndValidateData(packetPtr, FILENAME_RESP_PACKET_SSIZE)){
		#ifdef DEBUG_ON
			printf("Error: Bad data received! Resending filename...\n");
		#endif // DEBUG_ON
			return STATE_SEND_FILENAME;
		}

		if(packetPtr->header.flag != FLAG_TYPE_FILENAME_RESP){
			// Incorrect Packet Received
		#ifdef DEBUG_ON
			printf("Error: Didn't recieve a filename response packet! Resending filename...\n");
		#endif // DEBUG_ON
			return STATE_SEND_FILENAME;
		}

		if(packetPtr->payload.fileNameResponse.response != true){
			// Bad Filename
		#ifdef DEBUG_ON
			printf("Error: Bad filename! Gracefully Exiting...\n");
		#endif //DEBUG_ON
			return STATE_KILL;
		}

		return STATE_RECEIVE_FIRST_DATA;
	}
}

int
recvData(
	Packet_t* packetPtr,
	bool firstPacket
){
	if(pollCall(10) < 0){
		// Timeout
		if (firstPacket) {
		#ifdef DEBUG_ON
			printf("Timeout: Timeout receiving first file data! Resending filename...\n");
		#endif // DEBUG_ON
			return STATE_SEND_FILENAME;
		} else {
		#ifdef DEBUG_ON
			printf("Timeout: Timeout receiving data!\n");
		#endif // DEBUG_ON
			return STATE_RECEIVE_DATA_TIMEOUT;
		}
	} else {
		if(!receiveAndValidateData(packetPtr, DATA_PACKET_SSIZE(settings.bufferSize))){
			// Bad data received
			return STATE_BAD_DATA;
		} else {
			return STATE_PROCESS_DATA;
		}
	}
}

void 
stateMachine(
	void
){
	static int nextState = -1;
	static int state = STATE_SEND_FILENAME;
	static int timeout = 0;

	static Packet_t currPacket;

	while(1){
		if(timeout >= TIMEOUT_MAX){
		#ifdef DEBUG_ON
			printf("Timeout: Timeout maximum (%i) reached! Gracefully Exiting...", TIMEOUT_MAX);
		#endif // DEBUG_ON
			state = STATE_KILL;
		}
		
		// Clear packet buffer if we aren't processing the data
		if(state != STATE_PROCESS_DATA){
			memset(&currPacket, 0, PACKET_MAX_SSIZE);
		}

		switch (state)
		{
		case STATE_SEND_FILENAME:
		{
			sendFileName();

			nextState = STATE_WAIT_FOR_FILENAME_ACK;
			break;
		}
		case STATE_WAIT_FOR_FILENAME_ACK:
		{	
			nextState = waitForFileNameAck(&currPacket);
			
			if(nextState == STATE_SEND_FILENAME){
				// Timeout
				timeout++;
			}
		}
		case STATE_RECEIVE_FIRST_DATA:
		{	
			nextState = recvData(&currPacket, true);
			break;
		}
		case STATE_RECEIVE_DATA_TIMEOUT:
		{
			timeout++;
			nextState = STATE_RECEIVE_DATA;
		}
		case STATE_RECEIVE_DATA:
		{
			nextState = recvData(&currPacket, false);
			break;
		}
		case STATE_BAD_DATA:
		{	
			nextState = STATE_RECEIVE_DATA;
			break;
		}
		case STATE_PROCESS_DATA:
		{	
			nextState = STATE_RECEIVE_DATA;
			break;
		}
		case STATE_LAST_DATA:
		{
			break;
		}
		case STATE_KILL:
		{
			return;
		}

		default:
			// Shouldn't happen
		#ifdef DEBUG_ON
			printf("Error: Invalid state reached! Exiting...\n");
		#endif
			exit(1);
		}

		state = nextState;
	}
}

int 
checkArgs(
	int argc, 
	char *argv[], 
	rcopySettings_t *settings
){
    // Expecting 7 arguments plus the program name.
    if (argc != 8) {
        fprintf(stderr, "Usage: %s from-filename to-filename window-size buffer-size error-rate remote-machine remote-port\n", argv[0]);
        return -1;
    }

    // Copy fromFilename (ensure null termination)
    strncpy((char *)settings->fromFileName, argv[1], FILENAME_MAX_LEN);
    settings->fromFileName[FILENAME_MAX_LEN] = '\0';

    // Copy toFilename (ensure null termination)
    strncpy((char *)settings->toFileName, argv[2], FILENAME_MAX_LEN);
    settings->toFileName[FILENAME_MAX_LEN] = '\0';

    char *endptr;
    long value;

    // Parse and validate windowSize (must be > 0 and fit in uint32_t)
    value = strtol(argv[3], &endptr, 10);
    if (*endptr != '\0' || value <= 0 || value > WINDOW_SIZE_MAX) {
        fprintf(stderr, "Invalid window-size: %s\n", argv[3]);
        return -1;
    }
    settings->windowSize = (uint32_t)value;

    // Parse and validate bufferSize (must be > 0 and fit in uint16_t)
    value = strtol(argv[4], &endptr, 10);
    if (*endptr != '\0' || value <= 0 || value > UINT16_MAX) {
        fprintf(stderr, "Invalid buffer-size: %s\n", argv[4]);
        return -1;
    }
    settings->bufferSize = (uint16_t)value;

    // Parse and validate errorRate (float between 0 and 1 inclusive)
    float rate = strtof(argv[5], &endptr);
    if (*endptr != '\0' || rate < 0.0f || rate > 1.0f) {
        fprintf(stderr, "Invalid error-rate: %s\n", argv[5]);
        return -1;
    }
    settings->errorRate = rate;

    // Copy serverName (ensure null termination)
    strncpy((char *)settings->serverName, argv[6], SERVER_NAME_MAX);
    settings->serverName[SERVER_NAME_MAX] = '\0';

    // Parse and validate serverPort (must be in range 1 to 65535)
    value = strtol(argv[7], &endptr, 10);
    if (*endptr != '\0' || value <= 0 || value > 65535) {
        fprintf(stderr, "Invalid server-port: %s\n", argv[7]);
        return -1;
    }
    settings->serverPort = (uint16_t)value;

    // All arguments validated and stored successfully.
    return 0;
}

int 
main(
	int argc, 
	char *argv[]
){	
	if(checkArgs(argc, argv, &settings) != 0){
		exit(1);
	}
	
	struct sockaddr_in6 server;		// Supports 4 and 6 but requires IPv6 struct

	settings.socketNum = setupUdpClientToServer(&server, (char*) settings.serverName, settings.serverPort);

	settings.server = &server;
	settings.serverAddrLen = sizeof(struct sockaddr_in6);

	setupPollSet();
	addToPollSet(settings.socketNum);
	
	stateMachine();

	removeFromPollSet(settings.socketNum);
	
	close(settings.socketNum);

	exit(0);
}
