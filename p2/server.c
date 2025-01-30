/******************************************************************************
* myServer.c
* 
* Writen by Prof. Smith, updated Jan 2023
* Use at your own risk.  
*
*****************************************************************************/

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
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdint.h>

#include "networks.h"
#include "safeUtil.h"
#include "lab2.h"
#include "pollLib.h"

#define MAXBUF 1024
#define DEBUG_FLAG 1
#define BLOCKING_POLL -1

int recvFromClient(int clientSocket);
int checkArgs(int argc, char *argv[]);
void serverControl(int serverSocket);

int main(int argc, char *argv[])
{
	int mainServerSocket = 0;   //socket descriptor for the server socket
	int portNumber = 0;
	
	portNumber = checkArgs(argc, argv);
	
	//create the server socket
	mainServerSocket = tcpServerSetup(portNumber);

	setupPollSet();

	serverControl(mainServerSocket);

	close(mainServerSocket);

	return 0;
}

int recvFromClient(int clientSocket)
{
	uint8_t dataBuffer[MAXBUF];
	int messageLen = 0;
	
	//now get the data from the client_socket
	if ((messageLen = recvPDU(clientSocket, dataBuffer, MAXBUF)) < 0){
		if(errno == ECONNRESET){
			return 0;
		}
		
		perror("recv call");
		exit(-1);
	}

	if (messageLen == 0){
		printf("Connection closed by other side\n");
		return -1;
	}
	
	printf("Message received, length: %d Data: %s\n", messageLen, dataBuffer);

	sendPDU(clientSocket, dataBuffer, messageLen);
	return 0;
}

int checkArgs(int argc, char *argv[])
{
	// Checks args and returns port number
	int portNumber = 0;

	if (argc > 2)
	{
		fprintf(stderr, "Usage %s [optional port number]\n", argv[0]);
		exit(-1);
	}
	
	if (argc == 2)
	{
		portNumber = atoi(argv[1]);
	}
	
	return portNumber;
}

void processClient(int clientSocket){
	if(recvFromClient(clientSocket) < 0){
		removeFromPollSet(clientSocket);
	}
}

void addNewSocket(int serverSocket){
	// wait for client to connect
	int newSocket = tcpAccept(serverSocket, DEBUG_FLAG);

	addToPollSet(newSocket);
}

void serverControl(int serverSocket){

	addToPollSet(serverSocket);
	int currSocket;

	while(1){
		currSocket = pollCall(BLOCKING_POLL);

		if(currSocket < 0){
			printf("pollCall() returned -1 (shouldn't happen)");
			exit(-1);
		}else if (currSocket == serverSocket){
			addNewSocket(serverSocket);	
		}else{
			processClient(currSocket);
		}
	}
}
