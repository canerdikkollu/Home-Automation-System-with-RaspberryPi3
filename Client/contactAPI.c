#include"contactAPI.h"

// UART baglantisi icin ayarlamalar yapiliyor.
bool connectUART(ConnectionInfoStruct* connInfoStruct){
	connInfoStruct->connFD = open("/dev/serial0", O_RDWR | O_NOCTTY | O_NDELAY);
	if(connInfoStruct->connFD==-1)
		return false;
	struct termios options;
	tcgetattr(connInfoStruct->connFD, &options);
	options.c_cflag = B9600 | CS8 | CLOCAL | CREAD;
	options.c_iflag = IGNPAR;
	options.c_oflag = 0;
	options.c_lflag = 0;
	tcflush(connInfoStruct->connFD, TCIFLUSH);
	tcsetattr(connInfoStruct->connFD, TCSANOW, &options);
	return true;
}

// Socket baglantisi icin ayarlamalar yapiliyor.
bool connectSocket(ConnectionInfoStruct* connInfoStruct){
	// Adres olusturuluyor.
	struct sockaddr_in clientAddr;
	clientAddr.sin_family = AF_INET;
	clientAddr.sin_port = htons(connInfoStruct->port);
	if(inet_aton(connInfoStruct->ip, &clientAddr.sin_addr)==0)
		return false;
	// Sucu olarak baglanti kuracak soket olusturuluyor.
	connInfoStruct->connFD = socket(PF_INET, SOCK_STREAM, 0);

	if (setsockopt(connInfoStruct->connFD, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int)) < 0){
		printf("Error on setsockopt creation [%s] \n", strerror(errno));
		return false;
	}

	if(connInfoStruct->connFD == -1){
		printf("[ERROR] - On socket creation [%s] \n", strerror(errno));
		return false;
	}

	if(connect(connInfoStruct->connFD, (const struct sockaddr*)&clientAddr,sizeof(struct sockaddr))==-1){
		printf("[ERROR] - On socket connect [%s] \n", strerror(errno));
		return false;
	}
	return true;
}

void messageParse(ClientMessageStruct* clMsgStruct, int argument, char* input){
        if(!strcmp(input,"sensorList") || !strcmp(input,"sensorCount") || !strcmp(input,"sensorState") ||
	   !strcmp(input,"relayState") || !strcmp(input,"close") ){

		sprintf(clMsgStruct->message, "%s:", input);
                strcpy(clMsgStruct->command, input);
                strcpy(clMsgStruct->argument, "-1");
        }
	// Argumani -1 olmayan sorgulamalar icin
	else if(!strcmp(input,"relayChange")  ||  !strcmp(input,"sensorType") ){
		sprintf(clMsgStruct->message, "%s %d:", input, argument);
                strcpy(clMsgStruct->command, input);
                sprintf(clMsgStruct->argument, "%d", argument);
	}
}

void showSensorList(ConnectionInfoStruct *cInfoStruct){
        ClientMessageStruct clMsgStruct;
        messageParse(&clMsgStruct, -1, "sensorList");
        int rxLength=write(cInfoStruct->connFD, clMsgStruct.message, strlen(clMsgStruct.message));
        if(rxLength<0)
                printf("[ERROR] - Could not send request!\n ");

        while(1){
                if(cInfoStruct->connFD!=-1){
                        char receivedBuffer[256];
                        int receivedLength;
                        receivedLength=read(cInfoStruct->connFD, (void*)receivedBuffer, 255);
                        if(receivedLength>0){
                                receivedBuffer[receivedLength]='\0';
                                char *sensorType, *cpyMessage, *messageToken;
                                cpyMessage = strdup(receivedBuffer);
                                messageToken = strtok(cpyMessage, ":"); // : karakteri ayristiriliyor.
                                sensorType = strtok(messageToken, ",");
                                int index=0;
                                printf("All Sensors\n");
                                while(sensorType!=NULL){
                                        printf("[%d] - %s\n", index, sensorType);
                                        sensorType = strtok(NULL,",");
					index++;
                                }
				break;
                        }
                }
                sleep(1);
        }
}

void parseServerResponse(char* input, ClientMessageStruct* parseMsg){

	strcpy(parseMsg->message, input);
	char *messageToken, *dupInput, *argument, *command;
	dupInput=strdup(input);
	messageToken=strtok(dupInput,":");

	while(messageToken!=NULL){
		command=strtok(messageToken," ");
		strcpy(parseMsg->command, command);

		argument=strtok(NULL," ");
		if(argument!=NULL)
			strcpy(parseMsg->argument, argument);
		messageToken=strtok(NULL,":");
	}

	displayServerResponse(*parseMsg);
}

// Parse islemi sonrasi mesaj ekrana yazilir.
void displayServerResponse(ClientMessageStruct responseMsg){

	if(strcmp(responseMsg.command, "sensorCount")==0)
		printf("Sensor Count: [%s]\n\n", responseMsg.argument);
	else if(strcmp(responseMsg.command, "sensorState")==0){
		for(int i=0;i<NUMBERSENSOR;i++)
			printf("Sensor: [%d] -> Value: [%s]\n\n", i+1 ,(atoi(responseMsg.argument) & (1 >> i)) ? "ON" : "OFF");
	}
	else if(strcmp(responseMsg.command, "relayState")==0)
		printf("Relay Status: [%s]\n\n", (!strcmp(responseMsg.argument,"1")) ? "OFF" : "ON");
	else if(strcmp(responseMsg.command, "relayChange")==0)
		printf("[%s] -> Relay Status Change.\n\n",responseMsg.argument);
}

void receiveMessage(ConnectionInfoStruct *cInfoStruct, ClientMessageStruct* clMsgStruct){
	int step=0;
	while(step<4){
		if(cInfoStruct->connFD!=-1){
			char rxBuffer[256];
			int rxLength;
			rxLength=read( cInfoStruct->connFD, (void*)rxBuffer, 255 );
			if(rxLength>0){
				rxBuffer[rxLength]='\0';
				printf("[INFO] - Message: [%s], Length: [%d]\n", rxBuffer, rxLength);

				// Alinan mesaj parse ediliyor.
				parseServerResponse(rxBuffer,clMsgStruct);

				break;
			}

		}
		sleep(1);
		step++;
	}
	if(step>4)
		printf("[WARNING] - Failed to retrieve messages from server! \n");
}
