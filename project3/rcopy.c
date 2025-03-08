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

	FILE* toFile;
	
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
	STATE_SEND_FILENAME_TIMEOUT,
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

static rcopySettings_t settings = {0};
static SeqNum_t seqNum = 0;
static SeqNum_t currentRR = 0;

bool 
receiveAndValidateData(
	Packet_t* packetPtr,
	uint16_t* dataSize,
	uint16_t expectedSize
){	
	bool retVal = true;
	char buffer[expectedSize];

	// call safeRecvFrom
	int dataLen = safeRecvfrom(settings.socketNum, buffer, expectedSize, 0, (struct sockaddr*) settings.server, &settings.serverAddrLen);

	if(dataLen < expectedSize){
		// Short Packet Received
	#ifdef __DEBUG_ON
		printf("Error: Less bytes received than expected!\n");
	#endif // __DEBUG_ON
		retVal = false;
	}

	// Copy data from buffer into packet struct
	memcpy(packetPtr, buffer, dataLen);

	if(dataSize != NULL){
		*dataSize = dataLen;
	}

	if(!isValidPacket(packetPtr, dataLen)){
		// Invalid Packet Received
	#ifdef __DEBUG_ON
		printf("Error: Malformed packet receieved!\n");
	#endif // __DEBUG_ON
		retVal = false;
	}

	retVal = true;
	
#ifdef __DEBUG_ON
	char * ipString = NULL;
	ipString = ipAddressToString(settings.server);

	printf("Server with ip: %s and port %d said it received:\n", ipString, ntohs(settings.server->sin6_port));

	// print out bytes received
	for(int i=0; i < dataLen; i++){
		printf("%02x ", buffer[i]);
	}

	printf("\n");
#endif // __DEBUG_ON

	return retVal;
}

void
sendFileName(
	void
){
	Packet_t packet;
	int fileNameLen = strlen(settings.fromFileName);
#ifdef __DEBUG_ON
	printf("Info: Sending filename: %s\n", settings.fromFileName);
#endif // __DEBUG_ON

	buildFileNamePacket(&packet, seqNum, settings.windowSize, settings.bufferSize, (uint8_t*) settings.fromFileName, fileNameLen);

	int packetSize = FILENAME_PACKET_SSIZE(fileNameLen);

	safeSendto(settings.socketNum, (uint8_t*) &packet, packetSize, 0, (struct sockaddr*) settings.server, settings.serverAddrLen);
}

int 
waitForFileNameAck(
	Packet_t* packetPtr
){
	if(pollCall(1) < 0){
		// Timeout
	#ifdef __DEBUG_ON
		printf("Timeout: Filename response timed out! Resending filename...\n");
	#endif // __DEBUG_ON

		return STATE_SEND_FILENAME_TIMEOUT;
	} else {
		if(!receiveAndValidateData(packetPtr, NULL, FILENAME_RESP_PACKET_SSIZE)){
		#ifdef __DEBUG_ON
			printf("Error: Bad data received! Resending filename...\n");
		#endif // __DEBUG_ON
			return STATE_SEND_FILENAME_TIMEOUT;
		}

		if(packetPtr->header.flag != FLAG_TYPE_FILENAME_RESP){
			// Incorrect Packet Received
		#ifdef __DEBUG_ON
			printf("Error: Didn't recieve a filename response packet! Resending filename...\n");
		#endif // __DEBUG_ON
			return STATE_SEND_FILENAME_TIMEOUT;
		}

		if(packetPtr->payload.fileNameResponse.response != true){
			// Bad Filename
		#ifdef __DEBUG_ON
			printf("Error: Bad filename! Gracefully Exiting...\n");
		#endif //__DEBUG_ON
			return STATE_KILL;
		}
	#ifdef __DEBUG_ON
		printf("Info: Received filename ok! Waiting for first data...\n");
	#endif // __DEBUG_ON

		return STATE_RECEIVE_FIRST_DATA;
	}
}

int
recvData(
	Packet_t* packetPtr,
	uint16_t* dataSize,
	bool firstPacket
){
	if(pollCall(10) < 0){
		// Timeout
		if (firstPacket) {
		#ifdef __DEBUG_ON
			printf("Timeout: Timeout receiving first file data! Resending filename...\n");
		#endif // __DEBUG_ON
			return STATE_SEND_FILENAME_TIMEOUT;
		} else {
		#ifdef __DEBUG_ON
			printf("Timeout: Timeout receiving data!\n");
		#endif // __DEBUG_ON
			return STATE_RECEIVE_DATA_TIMEOUT;
		}
	} else {
		if(!receiveAndValidateData(packetPtr, dataSize, (settings.bufferSize))){
			// Bad data received
			return STATE_BAD_DATA;
		} else {
			return STATE_PROCESS_DATA;
		}
	}
}

void
sendSREJ(
	SeqNum_t srejNum
){
	Packet_t srejPacket;
	buildSrejPacket(&srejPacket, seqNum++, srejNum);

	safeSendto(settings.socketNum, (uint8_t*) &srejPacket, SREJ_PACKET_SSIZE, 0, (struct sockaddr*) settings.server, settings.serverAddrLen);
}

void
sendRR(
	void
){
	Packet_t rrPacket;
	buildRrPacket(&rrPacket, seqNum++, currentRR);

	safeSendto(settings.socketNum, (uint8_t*) &rrPacket, RR_PACKET_SSIZE, 0, (struct sockaddr*) settings.server, settings.serverAddrLen);
}

void
writeDataToDisk(
	uint8_t* data,
	uint16_t dataSize
){
	int numBytes = fwrite(data, sizeof(uint8_t), dataSize, settings.toFile);

	if(numBytes < dataSize){
		perror("writeDataToDisk: Error writing data to disk. Exiting...");
		exit(1);
	}
}

void
flushWindow(
	PacketState_t* validPackets,
	uint32_t numValidPackets
){
	Packet_t packetPtr;
	uint16_t dataSize;
	SeqNum_t currSeqNum;

	for(uint32_t i = 0; i < numValidPackets; i++){
		currSeqNum = validPackets[i].seqNum;

		getPacket(&packetPtr, &dataSize, currSeqNum);

		writeDataToDisk(packetPtr.payload.data.payload, dataSize);
	}

	currentRR = packetPtr.header.seqNum + 1;

	removePacket(currentRR);

	sendRR();
}

void
checkWindowState(
	bool* buffering
){
	PacketState_t* invalidPackets = (PacketState_t*) malloc(sizeof(PacketState_t));
	PacketState_t* validPackets = (PacketState_t*) malloc(sizeof(PacketState_t));
	uint32_t numInvalidPackets;
	uint32_t numValidPackets;

	getWindowPacketState(invalidPackets, validPackets, &numInvalidPackets, &numValidPackets);

	if(numInvalidPackets == 0){
	#ifdef __DEBUG_ON
		printf("Info: All valid packets in window! Flushing window...\n");
	#endif // __DEBUG_ON
		flushWindow(validPackets, numValidPackets);
	#ifdef __DEBUG_ON
		printf("Info: Exiting buffering mode\n\n");
	#endif // __DEBUG_ON
		*buffering = false;
	} else {
		// Still missing packets, update and send lowest SREJ
		int lowestSREJ = invalidPackets[0].seqNum;

		sendSREJ(lowestSREJ);
	}

	free(invalidPackets);
	free(validPackets);
}

int
processData(
	Packet_t* packetPtr,
	uint16_t dataSize,
	bool* buffering
){
	if(*buffering){
	#ifdef __DEBUG_ON
		printf("Info: Data Receive in buffering mode\n");
	#endif // __DEBUG_ON
		// Buffer Data
		if(!packetValidInWindow(ntohl(packetPtr->header.seqNum))){
			if ((packetPtr->header.flag == FLAG_TYPE_SREJ_DATA 	|| 
				packetPtr->header.flag == FLAG_TYPE_TIMEOUT_DATA))
			{
				// Replace packet in buffer
				if(!addPacket(packetPtr, dataSize, true)){
				#ifdef __DEBUG_ON
					printf("Error: Failure to add packet to buffer! Shouldn't happen. Exiting...\n");
				#endif // __DEBUG_ON
					exit(1);
				}
			}
		} else {
		#ifdef __DEBUG_ON
			printf("Info: Duplicate Data received! Throwing out...\n");
		#endif // __DEBUG_ON
			checkWindowState(buffering);
		}
	}else{
		if(packetPtr->header.flag != FLAG_TYPE_DATA || packetPtr->header.flag != FLAG_TYPE_EOF){
			if (packetPtr->header.seqNum == currentRR) {
			#ifdef __DEBUG_ON
				printf("Info: Regular data packet recieved! Writing to disk...\n");
			#endif // __DEBUG_ON
				writeDataToDisk(packetPtr->payload.data.payload, dataSize);

				currentRR = packetPtr->header.seqNum + 1;

				sendRR();
			} else if (packetPtr->header.seqNum > currentRR) {
			#ifdef __DEBUG_ON
				printf("Info: Greater than expected data packet received! Sending SREJ for current RR and entering buffering mode...\n");
			#endif // __DEBUG_ON
				*buffering = true;

				if(!addPacket(packetPtr, dataSize, true)){
				#ifdef __DEBUG_ON
					printf("Error: Failure to add packet to buffer! Shouldn't happen. Exiting...\n");
				#endif // __DEBUG_ON
					exit(1);
				}

				sendSREJ(currentRR);
			} else {
			#ifdef __DEBUG_ON
				printf("Info: Lower than expected data packet received! Sending current RR...\n");
			#endif // __DEBUG_ON
				sendRR();
			}
		}else{
		#ifdef __DEBUG_ON
			printf("Info: Packet isn't of type regular or eof data! Throwing out...\n");
		#endif // __DEBUG_ON
		}
	}

	if(packetPtr->header.flag == FLAG_TYPE_EOF){
		return STATE_LAST_DATA;
	}

	return STATE_RECEIVE_DATA;
}

void 
stateMachine(
	void
){
	static int nextState = -1;
	static int state = STATE_SEND_FILENAME;
	static int timeout = 0;
	static bool buffering = false;

	static Packet_t currPacket;
	static uint16_t dataSize;

	windowInit(settings.windowSize, settings.bufferSize);

	while(1){
		if(timeout >= TIMEOUT_MAX){
		#ifdef __DEBUG_ON
			printf("Timeout: Timeout maximum (%i) reached! Gracefully Exiting...", TIMEOUT_MAX);
		#endif // __DEBUG_ON
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
		case STATE_SEND_FILENAME_TIMEOUT:
		{
			// Timeout
			timeout++;

			removeFromPollSet(settings.socketNum);
			close(settings.socketNum);
			
			settings.socketNum = setupUdpClientToServer(settings.server, (char*) settings.serverName, settings.serverPort);

			addToPollSet(settings.socketNum);
			
			nextState = STATE_SEND_FILENAME;
			break;
		}
		case STATE_WAIT_FOR_FILENAME_ACK:
		{	
			nextState = waitForFileNameAck(&currPacket);
		}
		case STATE_RECEIVE_FIRST_DATA:
		{	
			nextState = recvData(&currPacket, &dataSize, true);
			break;
		}
		case STATE_RECEIVE_DATA_TIMEOUT:
		{
			timeout++;
			nextState = STATE_RECEIVE_DATA;
		}
		case STATE_RECEIVE_DATA:
		{
			nextState = recvData(&currPacket, &dataSize, false);
			break;
		}
		case STATE_BAD_DATA:
		{	
			buffering = true;

			sendSREJ(ntohl(currPacket.header.seqNum));

			nextState = STATE_RECEIVE_DATA;
			break;
		}
		case STATE_PROCESS_DATA:
		{
			// Write file and send RR for packet (if not buffering)
			nextState = processData(&currPacket, dataSize, &buffering);
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
		#ifdef __DEBUG_ON
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
    if (*endptr != '\0' || value < PAYLOAD_MIN || value > PAYLOAD_MAX) {
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

void
openToFile(
	void
){
	settings.toFile = fopen(settings.toFileName, "w");

	if(settings.toFile == NULL) {
		perror("Error opening file");

		exit(1);
	}
}

int 
main(
	int argc, 
	char *argv[]
){	
	if(checkArgs(argc, argv, &settings) != 0){
		exit(1);
	}

	openToFile();
	
	struct sockaddr_in6 server;		// Supports 4 and 6 but requires IPv6 struct

	settings.socketNum = setupUdpClientToServer(&server, (char*) settings.serverName, settings.serverPort);

	settings.server = &server;
	settings.serverAddrLen = sizeof(struct sockaddr_in6);

	//sendErr_init(settings.errorRate, DROP_ON, FLIP_ON, __DEBUG_ON, RSEED_ON);

	setupPollSet();
	addToPollSet(settings.socketNum);
	
	stateMachine();

	removeFromPollSet(settings.socketNum);
	
	close(settings.socketNum);

	exit(0);
}
