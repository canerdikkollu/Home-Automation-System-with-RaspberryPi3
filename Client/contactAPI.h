#ifndef CONTACTAPI_H
#define CONTACTAPI_H

#include<stdio.h>
#include<stdlib.h>
#include<stdbool.h>
#include<string.h>
#include<errno.h>
#include<unistd.h>
#include<termios.h>
#include<fcntl.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<netdb.h>

#include"config.h"

typedef struct {
	int connFD;
	char ip[15];
	int port;
}ConnectionInfoStruct;

typedef struct {
	char message[30];
	char command[15];
	char argument[15];
}ClientMessageStruct;

bool connectUART(ConnectionInfoStruct*);
bool connectSocket(ConnectionInfoStruct*);

void messageParse(ClientMessageStruct*, int, char*);
void receiveMessage(ConnectionInfoStruct*, ClientMessageStruct*);

void parseServerResponse(char*, ClientMessageStruct*);
void displayServerResponse(ClientMessageStruct);

void showSensorList(ConnectionInfoStruct*);


#endif
