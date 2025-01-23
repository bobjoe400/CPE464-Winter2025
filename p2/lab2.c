#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>

#include "lab2.h"

#define PDU_HDR_LEN 2

void print_packet_hex(uint8_t* buff, int len){
    for(int i = 0; i < len; i++){
        printf("%02x ", buff++[0]);
    }

    printf("\n");
}

int sendPDU(int clientSocket, uint8_t* dataBuffer, int lengthOfData){
    int pdu_len = lengthOfData + PDU_HDR_LEN;

    uint8_t app_pdu[pdu_len + 1];
    uint16_t hdr_pdu_len = htons(pdu_len);

    memset(app_pdu, 0, pdu_len + 1);
    memcpy(app_pdu, &hdr_pdu_len, PDU_HDR_LEN);
    memcpy(&app_pdu[PDU_HDR_LEN], dataBuffer, lengthOfData);

    int bytes_sent= send(clientSocket, app_pdu, pdu_len, 0) ;

    if(bytes_sent < 0){
        printf("Error sending packet to socket: %d\n", clientSocket);
        printf("Packet:\n"); print_packet_hex(app_pdu, pdu_len);

        perror("lab2::sendPDU()");
        exit(-1);
    }

    return bytes_sent - PDU_HDR_LEN;
}

int recvPDU(int socketNumber, uint8_t* dataBuffer, int bufferSize){
    uint16_t pdu_len = 0;
    uint16_t payload_len;

    int bytes_received = recv(socketNumber, &pdu_len, PDU_HDR_LEN, MSG_WAITALL);

    if(bytes_received < 0){
        printf("Error reciving packet from socket: %d\n", socketNumber);
        
        perror("lab2::recvPDU()");
        exit(-1);
    }else if (bytes_received == 0){
        printf("Socket %d closed\n", socketNumber);
        return 0;
    }

    pdu_len = ntohs(pdu_len);
    
    if (pdu_len - PDU_HDR_LEN > bufferSize){
        printf("dataBuffer too small! buff size: %d pdu len: %d\n", bufferSize, pdu_len);
        exit(-1);
    }

    payload_len = pdu_len - PDU_HDR_LEN;

    bytes_received = recv(socketNumber, dataBuffer, payload_len, MSG_WAITALL);

    if(bytes_received < 0){
        printf("Error reciving packet from socket: %d\n", socketNumber);
        
        perror("lab2::recvPDU()");
        exit(-1);
    }else if (bytes_received == 0){
        printf("Socket %d closed\n", socketNumber);
        return 0;
    }

    return bytes_received;
}