/******************************************************************************
* myClient.c
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

int sendToServer(int socketNum, uint8_t* sendBuf, int sendLen);
void checkArgs(int argc, char * argv[]);
void clientControl(int socketNum);

int main(int argc, char * argv[])
{
	int socketNum = 0;         //socket descriptor
	
	checkArgs(argc, argv);

	/* set up the TCP Client socket  */
	socketNum = tcpClientSetup(argv[1], argv[2], DEBUG_FLAG);
	
	setupPollSet();

	clientControl(socketNum);
	
	close(socketNum);
	
	return 0;
}

int sendToServer(int socketNum, uint8_t* sendBuf, int sendLen)
{
	int sent = 0;            //actual amount of data sent/* get the data and send it   */
	
	sent =  sendPDU(socketNum, sendBuf, sendLen);
	if (sent < 0)
	{
		perror("send call");
		exit(-1);
	}

	printf("Amount of data sent is: %d\n", sent);
	return 0;
}

void checkArgs(int argc, char * argv[])
{
	/* check command line arguments  */
	if (argc != 3)
	{
		printf("usage: %s host-name port-number \n", argv[0]);
		exit(1);
	}
}

int processStdin(uint8_t* buffer){
	char aChar = 0;
	int inputLen = 0;        
	
	// Important you don't input more characters than you have space 
	buffer[0] = '\0';
	while (inputLen < (MAXBUF - 1) && aChar != '\n')
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
	
	printf("read: %s string len: %d (including null)\n", buffer, inputLen);

	return inputLen;
}

void processMsgFromServer(int sockNum){
	uint8_t recvBuf[MAXBUF];
	int messageLen = recvPDU(sockNum, recvBuf, MAXBUF);

	if(messageLen == 0){
		printf("\nServer terminated\n");
		exit(0);
	}

	printf("\nMessage received, length: %d Data: %s\n", messageLen, recvBuf);
}

void clientControl(int socketNum){
	addToPollSet(STDIN_FILENO);
	addToPollSet(socketNum);

	while(1){
		printf("Enter data: ");
		fflush(stdout);
		int currSock = pollCall(BLOCKING_POLL);
		if(currSock < 0){
			printf("pollCall() returned -1 (shouldn't happen)");
			exit(-1);
		}else if(currSock == STDIN_FILENO){
			uint8_t sendBuf[MAXBUF];
			int sendLen = processStdin(sendBuf);
			sendToServer(socketNum, sendBuf, sendLen);
		}else{
			processMsgFromServer(currSock);
		}
	}
}