#include<signal.h>
#include<string.h>
#include <ctype.h>
#include"contactAPI.h"

ConnectionInfoStruct connInfoStruct;
ClientMessageStruct clientMessageStruct;

/* Fonksiyon Prototipleri */
void signalHandler(int);
void showConnectionTypeMenu();
void showOperationMenu();
void manageOperationChoice(int);

int main(){

	// SIGINT sinyali icin ayarlamalar yapiliyor.
	struct sigaction new_action, old_action;
	new_action.sa_handler=signalHandler;
	new_action.sa_flags=SA_NODEFER | SA_ONSTACK;
	sigaction(SIGINT, &new_action, &old_action);

	int choice=0;
	showConnectionTypeMenu();
	scanf("%d", &choice);
	while ((getchar()) != '\n');
	system("clear");

	// UART secildiyse
	if(choice==1){
		if(!connectUART(&connInfoStruct))
			printf("[ERROR] - Cannot connect UART! \n");
		while(1){
			showOperationMenu();
			scanf("%d", &choice);
			while ((getchar()) != '\n');
			system("clear");
			manageOperationChoice(choice);
			int rxLength=write(connInfoStruct.connFD, clientMessageStruct.message, strlen(clientMessageStruct.message));
			if(rxLength<0){
				printf("[ERROR] - Could not sent request! \n ");
				continue;
			}
			receiveMessage(&connInfoStruct, &clientMessageStruct);
		}
	}

	// SOCKET secildiyse
	else if(choice==2){
		printf("Server IP: ");
		scanf("%[^\n]%*c", connInfoStruct.ip);
		printf("Port: ");
		scanf("%d", & connInfoStruct.port);
		while ((getchar()) != '\n');
		// Soket baglantisi basariliysa
		system("clear");
		if(connectSocket(&connInfoStruct)){
			while(1){
				showOperationMenu();
				scanf("%d", &choice);
				if(choice==1) continue;
				while ((getchar()) != '\n');
				system("clear");
				printf("[INFO] - Connection Success. [%s / %d]\n\n",  connInfoStruct.ip,  connInfoStruct.port);
				manageOperationChoice(choice);
				int rxLength=write(connInfoStruct.connFD, clientMessageStruct.message, strlen(clientMessageStruct.message));
				if(rxLength<0){
					printf("[ERROR] - Could not sent request! \n ");
					continue;
				}
				receiveMessage(&connInfoStruct, &clientMessageStruct);
			}
		}
	}
	else
		exit(0);

	return 0;
}

void signalHandler(int signalNo){
	if(signalNo==SIGINT){
		printf("The application is shutting down.\n\n");
		messageParse(&clientMessageStruct, -1, "close");
		/*int rxLength=*/write(connInfoStruct.connFD, clientMessageStruct.message, strlen(clientMessageStruct.message));
		//if(rxLength<0)
		//	printf("[WARNING] - Write Error [%s]\n",strerror(errno));
		sleep(2);
		close(connInfoStruct.connFD);
		exit(0);
	}

}

void showConnectionTypeMenu(){
	printf("+----------------------------+\n");
	printf("|   HOME AUTOMATION SYSTEM   |\n");
	printf("+----------------------------+\n");
	printf("| Select Connection Type     |\n");
	printf("| 1. UART                    |\n");
	printf("| 2. SOCKET                  |\n");
	printf("+----------------------------+\n>");
}

void showOperationMenu(){
	printf("+----------------------------+\n");
	printf("|       OPERATION MENU       |\n");
	printf("+----------------------------+\n");
	printf("| 1. Show All Sensors        |\n");
	printf("| 2. Total Sensor Count      |\n");
	printf("| 3. Sensor Values           |\n");
	printf("| 4. Show Relay Status       |\n");
	printf("| 5. Change Relay Status     |\n");
	printf("| 6. Automode                |\n");
	printf("| 7. Close                   |\n");
	printf("+----------------------------+\n>");
}

void manageOperationChoice(int choice){
	if(choice==1)
		showSensorList(&connInfoStruct);
	else if(choice==2)
		messageParse(&clientMessageStruct, -1, "sensorCount");
	else if(choice==3)
		messageParse(&clientMessageStruct, -1, "sensorState");
	else if(choice==4)
		messageParse(&clientMessageStruct, -1, "relayState");
	else if(choice==5){
		int relayStatus=-1;
		printf("Set Relay Status [1->ON, 0->OFF]\n>");
		scanf("%d",&relayStatus);
		while(getchar()!='\n');
		messageParse(&clientMessageStruct, (1-relayStatus), "relayChange");
	}
	else if(choice==6){
		showSensorList(&connInfoStruct);
		char selectSensorID[5];
		char selectSensorValue[5];

		printf("Sensor ID: ");
		scanf("%[^\n]%*c", selectSensorID);

		printf("Which sensor value open the relay? (1 or 0): ");
		scanf("%[^\n]%*c", selectSensorValue);

		strcat(selectSensorID, selectSensorValue);
		sprintf(clientMessageStruct.message, "autoMode on %d %d:", atoi(selectSensorID), atoi(selectSensorValue));

		/*int rxLength=*/
		write(connInfoStruct.connFD, clientMessageStruct.message, strlen(clientMessageStruct.message));
		//if(rxLength<0)
		//	printf("[WARNING] - Write Error [%s]\n",strerror(errno));

		char ex;
		printf("Press 'E' to exit.:");
		scanf("%[^\n]%*c", &ex);
		if(tolower(ex)=='e')
			sprintf(clientMessageStruct.message, "autoMode off:");
		system("clear");
	}
	else if(choice==7){
		messageParse(&clientMessageStruct, -1, "close");
		/*int rxLength=*/
		write(connInfoStruct.connFD, clientMessageStruct.message, strlen(clientMessageStruct.message));
		//if(rxLength<0)
		//	printf("[WARNING] - Write Error [%s]\n",strerror(errno));
		int status=close(connInfoStruct.connFD);
		if(status<0)
			printf("[WARNING] - Close Error [%s]", strerror(errno));
		else{
			printf("The application is shutting down.\n\n");
			exit(0);
		}
	}
}
