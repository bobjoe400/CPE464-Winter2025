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

#include "packet.h"
#include "window.h"

#define SERVER_NAME_MAX 1024
typedef struct{
	char fromFilename[FILENAME_MAX_LEN + 1];
	char toFilename[FILENAME_MAX_LEN + 1];
	
	uint32_t windowSize;
	uint16_t bufferSize;

	float errorRate;

	char serverName[SERVER_NAME_MAX + 1];
	uint16_t serverPort;
}rcopySettings_t;

static rcopySettings_t settings;

int 
readFromStdin(
	char * buffer
){
	char aChar = 0;
	int inputLen = 0;        
	
	// Important you don't input more characters than you have space 
	buffer[0] = '\0';
	printf("Enter data: ");
	while (inputLen < (PAYLOAD_MAX - 1) && aChar != '\n')
	{
		aChar = getchar();
		if (aChar != '\n')
		{
			buffer[inputLen] = aChar;
			inputLen++;
		}
	}
	
	// Null terminate the string
	buffer[inputLen] = '\0';
	inputLen++;
	
	return inputLen;
}

void 
talkToServer(
	int socketNum, 
	struct sockaddr_in6* server
){
	int serverAddrLen = sizeof(struct sockaddr_in6);
	char * ipString = NULL;
	char buffer[PAYLOAD_MAX+1];

	Packet_t packet;
	int fileNameLen = strlen(settings.fromFilename);

	buildFileNamePacket(&packet, 0, settings.windowSize, settings.bufferSize, (uint8_t*) settings.fromFilename, fileNameLen);

	int packetSize = FILENAME_PACKET_SIZE(fileNameLen);

	safeSendto(socketNum, (uint8_t*) &packet, packetSize, 0, (struct sockaddr*) server, serverAddrLen);

	int dataLen = safeRecvfrom(socketNum, buffer, PAYLOAD_MAX, 0, (struct sockaddr *) server, &serverAddrLen);
	
	// print out bytes received
	ipString = ipAddressToString(server);
	printf("Server with ip: %s and port %d said it received:\n", ipString, ntohs(server->sin6_port));

	for(int i=0; i < dataLen; i++){
		printf("%02x ", buffer[i]);
	}

	printf("\n");
	      
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
    strncpy((char *)settings->fromFilename, argv[1], FILENAME_MAX_LEN);
    settings->fromFilename[FILENAME_MAX_LEN] = '\0';

    // Copy toFilename (ensure null termination)
    strncpy((char *)settings->toFilename, argv[2], FILENAME_MAX_LEN);
    settings->toFilename[FILENAME_MAX_LEN] = '\0';

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
		return 1;
	}
	
	int socketNum;
	struct sockaddr_in6 server;		// Supports 4 and 6 but requires IPv6 struct

	socketNum = setupUdpClientToServer(&server, (char*) settings.serverName, settings.serverPort);
	
	talkToServer(socketNum, &server);
	
	close(socketNum);

	return 0;
}
