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

#include "checksum.h"
#include "gethostbyname.h"
#include "networks.h"
#include "safeUtil.h"
#include "pollLib.h"
#include "cpe464.h"

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
	STATE_RECEIVE_DATA,
	STATE_RECEIVE_DATA_TIMEOUT,
	STATE_BAD_DATA,
	STATE_BUFFER_DATA,
	STATE_PROCESS_DATA,
	STATE_LAST_DATA,
	STATE_KILL,

	NUM_STATES,
};

static rcopySettings_t settings = 
	{
		{0},
		{0},
		0,
		0,
		0,
		0,
		{0},
		0
	};

static SeqNum_t seqNum = 0;
static SeqNum_t expected = SEQ_NUM_START;
static SeqNum_t highest = SEQ_NUM_START;

static bool wroteLastData = false;

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

// #ifdef __DEBUG_ON
// 	char * ipString = NULL;
// 	ipString = ipAddressToString(settings.server);

// 	printf("Server with ip: %s and port %d said it received:\n", ipString, ntohs(settings.server->sin6_port));

// 	// print out bytes received
// 	for(int i=0; i < dataLen; i++){
// 		printf("%02x ", buffer[i]);
// 	}

// 	printf("\n");
// #endif // __DEBUG_ON

	return retVal;
}

void print_address(const struct sockaddr *addr, socklen_t addrlen, const char *label) {
    char ip_str[INET6_ADDRSTRLEN];
    uint16_t port;

    if (addr->sa_family == AF_INET) {
        struct sockaddr_in *s = (struct sockaddr_in *)addr;
        inet_ntop(AF_INET, &s->sin_addr, ip_str, sizeof(ip_str));
        port = ntohs(s->sin_port);
        printf("%s (IPv4): %s:%d\n", label, ip_str, port);
    } else if (addr->sa_family == AF_INET6) {
        struct sockaddr_in6 *s = (struct sockaddr_in6 *)addr;
        inet_ntop(AF_INET6, &s->sin6_addr, ip_str, sizeof(ip_str));
        port = ntohs(s->sin6_port);
        printf("%s (IPv6): [%s]:%d\n", label, ip_str, port);
    } else {
        printf("%s: Unknown address family.\n", label);
    }
}

void print_socket_info(int sockfd) {
    struct sockaddr_storage local_addr;
    socklen_t addrlen = sizeof(local_addr);

    // Get local socket address.
    if (getsockname(sockfd, (struct sockaddr *)&local_addr, &addrlen) < 0) {
        perror("getsockname");
        return;
    }
    print_address((struct sockaddr *)&local_addr, addrlen, "Local Address");

    // Attempt to get peer address. This will succeed only if the socket is connected.
    addrlen = sizeof(local_addr);
    if (getpeername(sockfd, (struct sockaddr *)&local_addr, &addrlen) == 0) {
        print_address((struct sockaddr *)&local_addr, addrlen, "Peer Address");
    } else {
        perror("getpeername (socket might not be connected)");
    }
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

	print_socket_info(settings.socketNum);
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
			printf("Error: file %s not found.\n", settings.fromFileName);
			return STATE_KILL;
		}
	#ifdef __DEBUG_ON
		printf("Info: Received filename ok! Waiting for first data...\n");
	#endif // __DEBUG_ON

		highest++;
		return STATE_RECEIVE_FIRST_DATA;
	}
}

int
recvData(
	Packet_t* packetPtr,
	uint16_t* dataSize,
	bool firstPacket,
	bool buffering
){
#ifdef __DEBUG_ON
	printf("Info: Expected SeqNum: %i\n", expected);
#endif // __DEBUG_ON
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
		if(!receiveAndValidateData(packetPtr, dataSize, DATA_PACKET_SSIZE(settings.bufferSize))){
			// Bad data received
			return STATE_BAD_DATA;
		}
	#ifdef __DEBUG_ON
		printf("Info: Good data received! Processing data...\n");
	#endif // __DEBUG_ON

		if(buffering){
			return STATE_BUFFER_DATA;
		}

		return STATE_PROCESS_DATA;
	}
}

void
sendSREJ(
	SeqNum_t srejNum
){
	Packet_t srejPacket;
	buildSrejPacket(&srejPacket, seqNum++, srejNum);

#ifdef __DEBUG_ON
	printf("Info: Sending SREJ: %i\n", srejNum);
#endif // __DEBUG_ON

	safeSendto(settings.socketNum, (uint8_t*) &srejPacket, SREJ_PACKET_SSIZE, 0, (struct sockaddr*) settings.server, settings.serverAddrLen);
}

void
sendRR(
	void
){
	Packet_t rrPacket;
	buildRrPacket(&rrPacket, seqNum++, expected);

#ifdef __DEBUG_ON
	printf("Info: Sending RR: %i...\n", expected);
#endif // __DEBUG_ON

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
	Packet_t packet;
	uint16_t dataSize;
	SeqNum_t currSeqNum;

	for(uint32_t i = 0; i < numValidPackets; i++){
		memset(&packet, 0, PACKET_MAX_SSIZE);

		currSeqNum = validPackets[i].seqNum;

		getPacket(&packet, &dataSize, currSeqNum);

	#ifdef __DEBUG_ON
		printf("Info: Writing data %i to disk.\n", currSeqNum);
	#endif // __DEBUG_ON

		if(packet.header.flag == FLAG_TYPE_EOF){
			wroteLastData = true;
		}

		writeDataToDisk(packet.payload.data.payload, dataSize - sizeof(PacketHeader_t));

		expected++;
	}

	removePacket(expected);
}

void
checkWindowState(
	bool lastData
){
	PacketState_t* validPackets = (PacketState_t*) malloc(sizeof(PacketState_t));
	uint32_t numValidPackets;

	inorderValidPackets(&validPackets, &numValidPackets);

	if(numValidPackets > 0){
	#ifdef __DEBUG_ON
		printf("Info: %i valid in-order packets in window! Flushing window...\n", numValidPackets);
	#endif // __DEBUG_ON

		flushWindow(validPackets, numValidPackets);
	
	} else {
	#ifdef __DEBUG_ON
		printf("Info: Out of order data still in buffer.\n");
	#endif // __DEBUG_ON
	}

	free(validPackets);
}

int 
processDataBuffering(
	Packet_t* packetPtr,
	uint16_t dataSize,
	bool* buffering
){
	if(
		packetPtr->header.flag != FLAG_TYPE_SREJ_DATA &&
		packetPtr->header.flag != FLAG_TYPE_TIMEOUT_DATA &&
		packetPtr->header.flag != FLAG_TYPE_DATA &&
		packetPtr->header.flag != FLAG_TYPE_EOF
	){
	#ifdef __DEBUG_ON
		printf("Error: Packet isn't a data packet! Throwing out...\n");
	#endif // __DEBUG_ON
		return STATE_RECEIVE_DATA;
	}

	if(!packetValidInWindow(ntohl(packetPtr->header.seqNum))){
		if( ntohl(packetPtr->header.seqNum) == expected ){
		#ifdef __DEBUG_ON
			printf("Info: Replacement data received (SeqNum %i)! Replacing in window...\n", ntohl(packetPtr->header.seqNum));
		#endif // __DEBUG_ON
			replacePacket(packetPtr, dataSize);

			checkWindowState(false);

			if(expected < highest){
				sendSREJ(expected);
				sendRR();
			} else {
			#ifdef __DEBUG_ON
				printf("\nInfo: ---------------------------\n");
				printf("Info: --- Exiting buffer mode ---\n");
				printf("Info: ---------------------------\n\n");
			#endif // __DEBUG_ON

				*buffering = false;

				sendRR();
			}

		} else if(ntohl(packetPtr->header.seqNum) > expected) {
			#ifdef __DEBUG_ON
			printf("Error: Greater than expected (%i) data packet received (%i)! Buffering data...\n", expected, ntohl(packetPtr->header.seqNum));
		#endif // __DEBUG_ON

			if(!addPacket(packetPtr, dataSize)){
			#ifdef __DEBUG_ON
				printf("Error: Failure to add packet to buffer! Shouldn't happen. Exiting...\n");
			#endif // __DEBUG_ON

				exit(1);
			}

			highest = ntohl(packetPtr->header.seqNum);
		} else {
		#ifdef __DEBUG_ON
			printf("Info: Received lower (%i) than expected (%i) when buffering! Sending lowest SREJ and RR\n", ntohl(packetPtr->header.seqNum), expected);
		#endif // __DEBUG_ON

			sendSREJ(expected);
			sendRR();
		}
	} else{
	#ifdef __DEBUG_ON
		printf("Info: Duplicate data (SeqNum: %i) received! Throwing out...\n", ntohl(packetPtr->header.seqNum));
	#endif // __DEBUG_ON
		
		sendSREJ(expected);
		sendRR();
	}

	if(packetPtr->header.flag == FLAG_TYPE_EOF){
		return STATE_LAST_DATA;
	} else {
		return STATE_RECEIVE_DATA;
	}
}

int
processData(
	Packet_t* packetPtr,
	uint16_t dataSize,
	bool* buffering
){
	if(
		packetPtr->header.flag != FLAG_TYPE_SREJ_DATA &&
		packetPtr->header.flag != FLAG_TYPE_TIMEOUT_DATA &&
		packetPtr->header.flag != FLAG_TYPE_DATA &&
		packetPtr->header.flag != FLAG_TYPE_EOF
	){
	#ifdef __DEBUG_ON
		printf("Error: Packet isn't a data packet! Throwing out...\n");
	#endif // __DEBUG_ON
		return STATE_RECEIVE_DATA;
	}

	if (ntohl(packetPtr->header.seqNum) == expected) {
	#ifdef __DEBUG_ON
		printf("Info: Regular data packet recieved! Writing to disk...\n");
	#endif // __DEBUG_ON

	#ifdef __DEBUG_ON
		printf("Info: Writing data %i to disk.\n", expected);
	#endif // __DEBUG_ON

		writeDataToDisk(packetPtr->payload.data.payload, dataSize - sizeof(PacketHeader_t));

		if(packetPtr->header.flag == FLAG_TYPE_EOF){
			wroteLastData = true;
		}

	#ifdef __DEBUG_ON
		printf("Info: Good packet received! Moving up window...\n");
	#endif // __DEBUG_ON
	
		highest = expected;

		expected++;

		removePacket(expected);

		sendRR();

	} else if (ntohl(packetPtr->header.seqNum) > expected) {
	#ifdef __DEBUG_ON
		printf("Error: Greater than expected data packet received! Sending SREJ for current RR...\n");
	#endif // __DEBUG_ON
		
		sendSREJ(expected);

		highest = ntohl(packetPtr->header.seqNum);

		*buffering = true;

	#ifdef __DEBUG_ON
		printf("\nInfo: ----------------------------\n");
		printf("Info: --- Entering buffer mode ---\n");
		printf("Info: ----------------------------\n\n");
	#endif // __DEBUG_ON

		if(!addPacket(packetPtr, dataSize)){
		#ifdef __DEBUG_ON
			printf("Error: Failure to add packet to buffer! Shouldn't happen. Exiting...\n");
		#endif // __DEBUG_ON

			exit(1);
		}
	} else {
	#ifdef __DEBUG_ON
		printf("Error: Lower than expected data packet received! Sending current RR...\n");
	#endif // __DEBUG_ON

		sendRR();
	}

	if(packetPtr->header.flag == FLAG_TYPE_EOF){
		return STATE_LAST_DATA;
	} else {
		return STATE_RECEIVE_DATA;
	}
}

void
lastData(
	bool buffering
){
#ifdef __DEBUG_ON
	printf("\nInfo: ------------------------------------\n");
	printf("Info: Entering last data teardown state...\n");
	printf("Info: ------------------------------------\n\n");
#endif // __DEBUG_ON

	Packet_t packet;
	int timeout = 0;
	uint16_t dataSize = 0;

	do{
		if(wroteLastData){
		#ifdef __DEBUG_ON
			printf("Info: All data written to disk! Sending Ack and closing file...\n");
		#endif // __DEBUG_ON

			buildRrPacket(&packet, seqNum++, expected);

			packet.header.cksum = 0;
			packet.header.flag = FLAG_TYPE_EOF_ACK;

			packet.header.cksum = in_cksum((uint16_t*) &packet, RR_PACKET_SSIZE);

			safeSendto(settings.socketNum, (uint8_t*) &packet, RR_PACKET_SSIZE, 0, (struct sockaddr*) settings.server, settings.serverAddrLen);
			
			fclose(settings.toFile);
			return;
		}

		if(pollCall(10) < 0){
		#ifdef __DEBUG_ON
			printf("Timeout: Timedout while receiving last data packets! Trying again...\n");
		#endif // __DEBUG_ON

			timeout++;
		}else{
			timeout = 0;

			if(!receiveAndValidateData(&packet, &dataSize, DATA_PACKET_SSIZE(settings.bufferSize))){
			#ifdef __DEBUG_ON
				printf("Error: Bad data received! Sending SREJ...\n");
			#endif // __DEBUG_ON

				sendSREJ(ntohl(packet.header.seqNum));
			} else if (buffering) {
				processDataBuffering(&packet, dataSize, &buffering);
			} else {
				processData(&packet, dataSize, &buffering);
			}
		}

	}while(timeout < TIMEOUT_MAX);
#ifdef __DEBUG_ON
	printf("Timeout: Timeout maximum (%i) reached while receiving last data packets!\n", TIMEOUT_MAX);
#endif // __DEBUG_ON
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
			printf("Timeout: Timeout maximum (%i) reached! Gracefully Exiting...\n", TIMEOUT_MAX);
		#endif // __DEBUG_ON

			state = STATE_KILL;
		}
		
		// Clear packet buffer if we aren't processing the data
		if(state == STATE_RECEIVE_DATA){
			memset(&currPacket, 0, PACKET_MAX_SSIZE);
		}

		switch (state)
		{
		case STATE_SEND_FILENAME:
		{
			seqNum = 0;
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
			break;
		}
		case STATE_RECEIVE_FIRST_DATA:
		{	
			seqNum++;
			nextState = recvData(&currPacket, &dataSize, true, false);
			break;
		}
		case STATE_RECEIVE_DATA:
		{
			nextState = recvData(&currPacket, &dataSize, false, buffering);
			break;
		}
		case STATE_RECEIVE_DATA_TIMEOUT:
		{
			timeout++;

			nextState = STATE_RECEIVE_DATA;
			break;
		}
		case STATE_BAD_DATA:
		{	
			nextState = STATE_RECEIVE_DATA;
			break;
		}
		case STATE_PROCESS_DATA:
		{
			// Write file and send RR for packet (if not buffering)
			nextState = processData(&currPacket, dataSize, &buffering);

			break;
		}
		case STATE_BUFFER_DATA:
		{	
			nextState = processDataBuffering(&currPacket, dataSize, &buffering);

			break;
		}
		case STATE_LAST_DATA:
		{
			lastData(buffering);

			nextState = STATE_KILL;	
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

	sendErr_init(settings.errorRate, DROP_ON, FLIP_ON, ERR_LIB_DEBUG, RSEED_ON);

	setupPollSet();
	addToPollSet(settings.socketNum);
	
	stateMachine();

	removeFromPollSet(settings.socketNum);
	
	close(settings.socketNum);

	exit(0);
}
