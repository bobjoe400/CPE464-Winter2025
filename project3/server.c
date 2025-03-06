/* Server side - UDP Code				    */
/* By Hugh Smith	4/1/2017	*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "gethostbyname.h"
#include "networks.h"
#include "safeUtil.h"

#include "packet.h"
#include "window.h"

#define MAX_ARGS 3
#define MIN_ARGS 2

typedef struct{
	float errorRate;
	uint16_t port;
}serverSettings_t;

static serverSettings_t settings;

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
		safeSendto(socketNum, buffer, dataLen, 0, (struct sockaddr *) & client, clientAddrLen);

	}
}

int
checkArgs(
	int argc,
	char* argv[],
	serverSettings_t* settings
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
        fprintf(stderr, "Invalid errorRate: %s\n", argv[5]);
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
        fprintf(stderr, "Invalid serverPort: %s\n", argv[7]);
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

	int socketNum = 0;

	socketNum = udpServerSetup(settings.port);

	processClient(socketNum);

	close(socketNum);

	return 0;
}

