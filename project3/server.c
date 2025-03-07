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

#include "gethostbyname.h"
#include "networks.h"
#include "safeUtil.h"
#include "pollLib.h"

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
	int socketNum;
	struct sockaddr_in6* client;
	int clientAddrlen;
}ClientSettings_t;

enum {
	STATE_WAIT_FILENAME = 0,
	STATE_PROCESS_FILENAME,
	STATE_SEND_RECEIVE_DATA,
	STATE_LAST_DATA,
	STATE_KILL,

	NUM_STATES
};

static ServerSettings_t settings = {0};
static SeqNum_t seqNum = 0;

bool 
receiveAndValidateData(
	Packet_t* packetPtr,
	uint16_t expectedSize,
	ClientSettings_t* client,
	bool validateSize
){	
	bool retVal = true;
	char buffer[expectedSize];

	// call safeRecvFrom
	int dataLen = safeRecvfrom(settings.socketNum, buffer, expectedSize, 0, (struct sockaddr*) client->client, &client->clientAddrlen);

	if(validateSize && dataLen < expectedSize){
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
	ipString = ipAddressToString(client->client);

	printf("Server with ip: %s and port %d said it received:\n", ipString, ntohs(client->client->sin6_port));

	// print out bytes received
	for(int i=0; i < dataLen; i++){
		printf("%02x ", buffer[i]);
	}

	printf("\n");
#endif // DEBUG_ON

	return retVal;
}

void
processClient(
	int socketNum
){
	int dataLen = 0;
	char buffer[PAYLOAD_MAX + 1];
	struct sockaddr_in6 client;
	int clientAddrLen = sizeof(client);

	buffer[0] = '\0';
	while (buffer[0] != '.')
	{
		dataLen = safeRecvfrom(socketNum, buffer, PAYLOAD_MAX, 0, (struct sockaddr *) &client, &clientAddrLen);

		Packet_t packet;
		memset(&packet, 0, sizeof(Packet_t));
		memcpy(&packet, buffer, dataLen);

		printf("Received message from client with ");
		printIPInfo(&client);
		printf(" Len: %d\nData", dataLen);

		for(int i=0; i < dataLen; i++){
			printf("%02x ", buffer[i]);
		}

		printf("\n");

		// just for fun send back to client number of bytes received
		safeSendto(socketNum, buffer, dataLen, 0, (struct sockaddr *) &client, clientAddrLen);

	}
}

int
waitFileName(
	Packet_t* packetPtr,
	ClientSettings_t* client
){	
	if(settings.socketNum == pollCall(POLL_WAIT_FOREVER)){
		if(!receiveAndValidateData(packetPtr, FILENAME_MAX_SSIZE, client, false)){
		#ifdef DEBUG_ON
			printf("Error: Bad filename packet received! Throwing out...\n");
		#endif // DEBUG_ON
			return STATE_WAIT_FILENAME;
		}

		return STATE_PROCESS_FILENAME;
	} else {
	#ifdef DEBUG_ON
		printf("Error: Data recieved on not main socket while waiting for filename! (This shouldn't happen)\n");
	#endif // DEBUG_ON
		return STATE_KILL;
	}
}

int
processFileName(
	Packet_t* packetPtr,
	ClientSettings_t* client,
	FILE* filePtr
){
	int retVal = STATE_SEND_RECEIVE_DATA;
	bool goodFile = true;

	if((filePtr = fopen((char*) packetPtr->payload.fileName.fileName, "r")) == NULL){
		// Bad filename
	#ifdef DEBUG_ON
		printf("Bad filename received! Sending response");
	#endif // DEBUG_ON
		goodFile = false;

		retVal = STATE_WAIT_FILENAME;
	}

	Packet_t respPacket;
	buildFileNameRespPacket(packetPtr, seqNum++, goodFile);

	safeSendto(settings.socketNum, (uint8_t*) &respPacket, FILENAME_RESP_PACKET_SSIZE, 0, (struct sockaddr*) client->client, client->clientAddrlen);

	return retVal;
}

void
stateMachine(
	void
){
	static int state = STATE_WAIT_FILENAME;
	static int nextState = -1;
	
	static ClientSettings_t client;
	static Packet_t currPacket;
	static FILE* filePtr;

	static struct sockaddr_in6 clientAddr;

	while(1){

		if(state == STATE_WAIT_FILENAME){
			client.clientAddrlen = sizeof(clientAddr);
			client.client = &clientAddr;
			client.socketNum = -1;
			filePtr = NULL;
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
			nextState = processFileName(&currPacket, &client, filePtr);
			break;
		}
		case STATE_SEND_RECEIVE_DATA: 
		{
			
			break;
		}
		case STATE_LAST_DATA: 
		{
			break;
		}
		case STATE_KILL:
			break;
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

	printf("Server PID: %i\n", getpid());
	settings.socketNum = udpServerSetup(settings.port);

	setupPollSet();
	addToPollSet(settings.socketNum);

	stateMachine();

	close(settings.socketNum);

	return 0;
}

