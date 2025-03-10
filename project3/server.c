/* Server side - UDP Code				    */
/* By Hugh Smith	4/1/2017	*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "checksum.h"
#include "gethostbyname.h"
#include "networks.h"
#include "safeUtil.h"
#include "pollLib.h"
#include "cpe464.h"

#include "packet.h"
#include "window.h"

#define MAX_ARGS 3
#define MIN_ARGS 2

typedef struct{
	float errorRate;
	uint16_t port;

	int socketNum;
}ServerSettings_t;

typedef struct{
	uint32_t windowSize;
	uint16_t bufferSize;

	FILE* file;

	int socketNum;
	struct sockaddr_in6* client;
	int clientAddrlen;
}ClientSettings_t;

enum MainServerStates_e{
	STATE_WAIT_FILENAME = 0,
	STATE_PROCESS_FILENAME,
	STATE_SEND_RECEIVE_DATA,
	STATE_LAST_DATA,
	STATE_KILL,

	NUM_MAIN_STATES
};

static ServerSettings_t settings = {0};
static SeqNum_t seqNum = 0;

bool 
receiveAndValidateData(
	Packet_t* packetPtr,
	uint16_t* dataSize,
	uint16_t expectedSize,
	ClientSettings_t* client,
	bool validateSize,
	bool serverSocket
){	
	bool retVal = true;
	char buffer[expectedSize];

	int socketNum = (serverSocket) ? settings.socketNum : client->socketNum;

	// call safeRecvFrom
	int dataLen = safeRecvfrom(socketNum, buffer, expectedSize, 0, (struct sockaddr*) client->client, &client->clientAddrlen);

	// Copy data from buffer into packet struct
	memcpy(packetPtr, buffer, dataLen);

	if(validateSize && dataLen < expectedSize){
		// Short Packet Received
	#ifdef __DEBUG_ON
		printf("Error: Less bytes received than expected!\n");
	#endif // __DEBUG_ON
		retVal = false;
	}

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
	
// #ifdef __DEBUG_ON
// 	char * ipString = NULL;
// 	ipString = ipAddressToString(client->client);

// 	printf("Info: Received from client with IP: %s and port %d:\n", ipString, ntohs(client->client->sin6_port));

// 	// print out bytes received
// 	for(int i=0; i < dataLen; i++){
// 		printf("%02x ", buffer[i]);
// 	}

// 	printf("\n");
// #endif // __DEBUG_ON

	return retVal;
}

int
waitFileName(
	Packet_t* packetPtr,
	ClientSettings_t* client
){	
	if(settings.socketNum == pollCall(POLL_FOREVER)){
		if(!receiveAndValidateData(packetPtr, NULL, FILENAME_MAX_SSIZE, client, false, true)){
		#ifdef __DEBUG_ON
			printf("Error: Bad filename packet received! Throwing out...\n");
		#endif // __DEBUG_ON
			return STATE_WAIT_FILENAME;
		}
	#ifdef __DEBUG_ON
		printf("Info: Filename received! Processing filename...\n");
	#endif // __DEBUG_ON

		return STATE_PROCESS_FILENAME;
	} else {
	#ifdef __DEBUG_ON
		printf("Error: Data recieved on not main socket while waiting for filename! (This shouldn't happen)\n");
	#endif // __DEBUG_ON
		return STATE_KILL;
	}
}

int
processFileName(
	Packet_t* packetPtr,
	ClientSettings_t* client
){
	int retVal = STATE_SEND_RECEIVE_DATA;
	bool goodFile = true;

	if((client->file = fopen((char*) packetPtr->payload.fileName.fileName, "r")) == NULL){
		// Bad filename
	#ifdef __DEBUG_ON
		printf("Error: Bad filename received! Sending response...\n");
	#endif // __DEBUG_ONf
		goodFile = false;

		retVal = STATE_WAIT_FILENAME;
	}
	
	if(goodFile){
		if((client->socketNum = socket(AF_INET6, SOCK_DGRAM, 0)) < 0){
			perror("processFileName: socket() call");
		}
	
		client->windowSize = ntohl(packetPtr->payload.fileName.windowSize);
		client->bufferSize = ntohs(packetPtr->payload.fileName.bufferSize);
	#ifdef __DEBUG_ON
		printf("Info: Client window size received as: %i buffer size received as: %i\n", client->windowSize, client->bufferSize);
	#endif // __DEBUG_ON
	}

	Packet_t respPacket;
	buildFileNameRespPacket(&respPacket, seqNum++, goodFile);

	safeSendto(client->socketNum, (uint8_t*) &respPacket, FILENAME_RESP_PACKET_SSIZE, 0, (struct sockaddr*) client->client, client->clientAddrlen);

	return retVal;
}

int
processRrSrej(
	Packet_t* packetPtr,
	ClientSettings_t* client
){
	memset(packetPtr, 0, PACKET_MAX_SSIZE);

	uint16_t dataSize;

	if(!receiveAndValidateData(packetPtr, &dataSize, RR_PACKET_SSIZE, client, true, false)){
	#ifdef __DEBUG_ON
		printf("Error: Invalid RR/SREJ packet recieved! Throwing out...\n");
	#endif // __DEBUG_ON

		return -1;
	}

	switch (packetPtr->header.flag)
	{
	case FLAG_TYPE_RR:
	{
	#ifdef __DEBUG_ON
		printf("Info: Received RR# %i. Removing from window...\n", ntohl(packetPtr->payload.rr.seqNum));
	#endif // __DEBUG_ON

		removePacket(ntohl(packetPtr->payload.rr.seqNum));
		return FLAG_TYPE_RR;
	}
	case FLAG_TYPE_SREJ:
	{
		Packet_t srejDataPacket;
		getPacket(&srejDataPacket, &dataSize, ntohl(packetPtr->payload.srej.seqNum));

		srejDataPacket.header.cksum = 0;
		srejDataPacket.header.flag = FLAG_TYPE_SREJ_DATA;

		srejDataPacket.header.cksum = in_cksum((uint16_t*) &srejDataPacket, dataSize);

		safeSendto(client->socketNum, (uint8_t*) &srejDataPacket, dataSize, 0, (struct sockaddr*) client->client, client->clientAddrlen);
		return FLAG_TYPE_SREJ;
	}
	case FLAG_TYPE_EOF_ACK:
	{
		return FLAG_TYPE_EOF_ACK;
	}
	
	default:
	#ifdef __DEBUG_ON
		printf("Error: Recieved packet not SREJ or RR Type! Throwing out...\n");
	#endif // __DEBUG_ON

		return -1;
	}
}

void
readFromDiskAndSend(
	Packet_t* packetPtr,
	ClientSettings_t* client,
	uint8_t* data,
	uint16_t* dataSize,
	bool* atEof
){
	memset(data, 0, client->bufferSize);
	memset(packetPtr, 0, PACKET_MAX_SSIZE);

	uint16_t dataLen = (uint16_t) fread(data, sizeof(char), client->bufferSize, client->file);

	if(dataLen < client->bufferSize){
		if(feof(client->file)){
		#ifdef __DEBUG_ON
			printf("Info: End of file reached! Sending last bit of data...\n");
		#endif // __DEBUG_ON

			*atEof = true;
		}

		if(ferror(client->file)){
			perror("sendAndRecieveData: Error reading file. Exiting...");
			exit(1);
		}
	}
	
	*dataSize = DATA_PACKET_SSIZE(dataLen);

#ifdef __DEBUG_ON
printf("Info: Sending data %i\n", seqNum);
#endif // __DEBUG_ON

	buildDataPacket(packetPtr, seqNum++, data, dataLen);

	if(*atEof){
		packetPtr->header.cksum = 0;
		packetPtr->header.flag = FLAG_TYPE_EOF;

		packetPtr->header.cksum = in_cksum((uint16_t*) packetPtr, *dataSize);
	}

	safeSendto(client->socketNum, (uint8_t*) packetPtr, *dataSize, 0, (struct sockaddr*) client->client, client->clientAddrlen);

	addPacket(packetPtr, *dataSize);
}

int
sendAndReceiveData(
	Packet_t* packetPtr,
	ClientSettings_t* client
){
	windowInit(client->windowSize, client->bufferSize);

	bool atEof = false;

	uint8_t data[client->bufferSize];
	uint16_t dataSize = 0;

	removeFromPollSet(settings.socketNum);
	addToPollSet(client->socketNum);

	static int timeout = 0;

	while(!atEof){
		#ifdef __DEBUG_ON
			printf("\nInfo: -------------------\n");
			printf("Info: --- Window Open ---\n");
			printf("Info: -------------------\n\n");
		#endif // __DEBUG_ON

		while(isWindowOpen()){

			readFromDiskAndSend(packetPtr, client, data, &dataSize, &atEof);

			if(atEof){
				return STATE_LAST_DATA;
			}

		#ifdef __DEBUG_ON
			printf("Info: Checking for RR's or SREJ's\n");
		#endif // __DEBUG_ON

			//Handle RR's and SREJ's
			while(pollCall(POLL_NO_BLOCK) > 0){
				processRrSrej(packetPtr, client);
			}
		}

		if(atEof){
			return STATE_LAST_DATA;
		}

		#ifdef __DEBUG_ON
			printf("\nInfo: --------------------\n");
			printf("Info: --- Window Closed ---\n");
			printf("Info: ---------------------\n\n");
		#endif // __DEBUG_ON

		while(!isWindowOpen()){
		#ifdef __DEBUG_ON
			printf("Info: Waiting on RR/SREJs...\n");
		#endif // __DEBUG_ON

			if(timeout > TIMEOUT_MAX){
			#ifdef __DEBUG_ON
				printf("Timeout: Timed out waiting for client response! Ending session...\n");
			#endif // __DEBUG_ON
				
				return STATE_KILL;
			}

			if(pollCall(1) < 0){
			#ifdef __DEBUG_ON
				printf("Timeout: Timeout waiting for RR/SREJs. Sending lowest packet...\n");
			#endif // __DEBUG_ON

				timeout++;

				memset(packetPtr, 0, PACKET_MAX_SSIZE);

				getLowestPacket(packetPtr, &dataSize);

				packetPtr->header.cksum = 0;
				packetPtr->header.flag = FLAG_TYPE_TIMEOUT_DATA;

				packetPtr->header.cksum = in_cksum((uint16_t*) packetPtr, dataSize);

				safeSendto(client->socketNum, (uint8_t*) packetPtr, dataSize, 0, (struct sockaddr*) client->client, client->clientAddrlen);
			} else {
				timeout = 0;

				processRrSrej(packetPtr, client);
			}
		}
	}

#ifdef __DEBUG_ON
	printf("Error: Sending data failed!\n");
#endif // __DEBUG_ON

	return STATE_KILL;
}

void
lastData(
	ClientSettings_t* client
){
#ifdef __DEBUG_ON
	printf("\nInfo: ------------------------------------\n");
	printf("Info: Entering last data teardown state...\n");
	printf("Info: ------------------------------------\n\n");
#endif // __DEBUG_ON

	Packet_t currPacket;
	uint16_t dataSize;
	int timeout = 0;

	do{
		memset(&currPacket, 0, sizeof(Packet_t));

		if(pollCall(1) < 0){
		#ifdef __DEBUG_ON
			printf("Timeout: Timeout waiting for RR/SREJs. Sending lowest packet...\n");
		#endif // __DEBUG_ON

			timeout++;
			
			memset(&currPacket, 0, PACKET_MAX_SSIZE);

			getLowestPacket(&currPacket, &dataSize);

			currPacket.header.cksum = 0;
			currPacket.header.flag = FLAG_TYPE_TIMEOUT_DATA;

			currPacket.header.cksum = in_cksum((uint16_t*) &currPacket, dataSize);

			safeSendto(client->socketNum, (uint8_t*) &currPacket, dataSize, 0, (struct sockaddr*) client->client, client->clientAddrlen);
		} else {			
			if (processRrSrej(&currPacket, client) == FLAG_TYPE_EOF_ACK){
			#ifdef __DEBUG_ON
				printf("Info: EOF ack recievied! Closing file...\n");
			#endif // __DEBUG_ON
				fclose(client->file);
				return;
			}
		}
	}while(timeout < TIMEOUT_MAX);

#ifdef __DEBUG_ON
	printf("Timeout: Timed out while waiting for EOF ack! Closing file...\n");
#endif // __DEBUG_ON
	fclose(client->file);
}

void
stateMachine(
	void
){
	static int state = STATE_WAIT_FILENAME;
	static int nextState = -1;
	
	static ClientSettings_t client;
	static Packet_t currPacket;

	static struct sockaddr_in6 clientAddr;

	while(1){

		if(state == STATE_WAIT_FILENAME){
			client.clientAddrlen = sizeof(clientAddr);
			client.client = &clientAddr;
			client.socketNum = -1;
			client.file = NULL;
		}

		if(
			state == STATE_WAIT_FILENAME ||
			state == STATE_SEND_RECEIVE_DATA
		){
			// Reset the currently received packet
			memset(&currPacket, 0, PACKET_MAX_SSIZE);
		}

		switch (state)
		{
		case STATE_WAIT_FILENAME:
		{
			nextState = waitFileName(&currPacket, &client);
			break;
		}
		case STATE_PROCESS_FILENAME: 
		{
			fork()
			//sendErr_init(settings.errorRate, DROP_ON, FLIP_ON, __DEBUG_ON, RSEED_ON);

			nextState = processFileName(&currPacket, &client);
			break;
		}
		case STATE_SEND_RECEIVE_DATA: 
		{
			nextState = sendAndReceiveData(&currPacket, &client);
			break;
		}
		case STATE_LAST_DATA: 
		{
			lastData(&client);

		#ifdef __DEBUG_ON
			printf("Info: Exiting Gracefully...\n");
		#endif // __DEBUG_ON

			close(client.socketNum);
			windowDestroy();

			nextState = STATE_KILL;
			break;
		}
		case STATE_KILL:
			return;

		default:
			break;
		}

		state = nextState;
	}
}

int
checkArgs(
	int argc,
	char* argv[],
	ServerSettings_t* settings
){
    // Expecting 1 to 2 arguments plus the program name.
    if (argc > MAX_ARGS || argc < MIN_ARGS) {
        fprintf(stderr, "Usage: %s error-rate [optional-port-number]\n", argv[0]);
        return -1;
    }

	char *endptr;
	long value;

    // Parse and validate errorRate (float between 0 and 1 inclusive)
    float rate = strtof(argv[1], &endptr);
    if (*endptr != '\0' || rate < 0.0f || rate > 1.0f) {
        fprintf(stderr, "Invalid error-rate: %s\n", argv[5]);
        return -1;
    }
    settings->errorRate = rate;

	if(argc == 2){
		settings->port = 0;
		return 0;
	}

    // Parse and validate serverPort (must be in range 1 to 65535)
    value = strtol(argv[2], &endptr, 10);
    if (*endptr != '\0' || value <= 0 || value > 65535) {
        fprintf(stderr, "Invalid optional-port-numbert: %s\n", argv[7]);
        return -1;
    }
    settings->port = (uint16_t)value;

    // All arguments validated and stored successfully.
    return 0;
}

int
main(
	int argc,
	char *argv[]
){
	if(checkArgs(argc, argv, &settings) < 0){
		exit(1);
	}

#ifdef __DEBUG_ON
	printf("Server PID: %i\n", getpid());
#endif // __DEBUG_ON

	settings.socketNum = udpServerSetup(settings.port);

	sendErr_init(settings.errorRate, DROP_ON, FLIP_ON, ERR_LIB_DEBUG, RSEED_ON);

	setupPollSet();
	addToPollSet(settings.socketNum);

	stateMachine();

	close(settings.socketNum);

	return 0;
}

