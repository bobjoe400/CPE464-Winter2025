#ifndef LAB2_H
#define LAB2_H
#include <stdint.h>

int sendPDU(int clientSocket, uint8_t* dataBuffer, int lengthOfData);
int recvPDU(int socketNumber, uint8_t* dataBuffer, int bufferSize);

#endif