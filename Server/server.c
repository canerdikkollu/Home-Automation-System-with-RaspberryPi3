#include<stdio.h>
#include<stdbool.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>
#include<math.h>

/* UART Kutuphaneleri */
#include<unistd.h>
#include<fcntl.h>
#include<termios.h>

/* Socket Kutuphaneleri */
#include<sys/socket.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<netdb.h>
#include<errno.h>

/* Thread ve Signal Kutuphaneleri */
#include<signal.h>
#include<pthread.h>
#include<semaphore.h>

#include"gpioDIO.h" /* GPIO okuma ve yazma islemleri */
#include"keypad.h"  /* Keypad veri okuma islemleri */
#include"config.h"  /* Sistem ilk atamalari */

/* Sensor tanimlamalari */
int sensorPins[NUMBERSENSOR]={SENSOR1};
int sensorValues[NUMBERSENSOR];
char *sensorName[NUMBERSENSOR]={"Hareket"};

/* Socket tanimlamalari */
int connectionCount=0;
int connectedDeviceFD[MAX_CONNECTION];
int serverSocketFD;
struct sockaddr_in serverAddress;
struct sockaddr_in connectedDeviceList[MAX_CONNECTION];

/* UART tanimlamalari */
int uartFD=-1;

/* THREAD tanimlamalari */
pthread_t threadIDKeypadListen;
pthread_t threadIDSensorValuesRead;
pthread_t threadIDSocketConnection;
pthread_t threadIDUARTRead;
pthread_t threadIDUARTWrite;
pthread_t threadIDSocketMessageManage;
pthread_t threadIDRelayAutoMode;

/* Semaphore ve mutex tanimlamalari */
sem_t semConnDeviceFD;
pthread_mutex_t mutex;

/* Fonksiyon Prototipleri  */
void keypadInputOperations(char*);
void showInfoMessage(char*, int);
void requestManageUARTorSocket(char*, int*);
void messageParseUARTorSocket(char*, char*, int*);

/* SIGINT(CTRL^C) sinyali icin handler fonksiyonu  */
void signalHandler(int sigNo){
	if( sigNo == SIGINT ){
        	printf("[INFO] - Server is shutting down..\n");
		pthread_cancel(threadIDSocketConnection);
        	pthread_cancel(threadIDSensorValuesRead);
	        pthread_cancel(threadIDKeypadListen);
        	pthread_cancel(threadIDUARTRead);
	        pthread_cancel(threadIDUARTWrite);

		for(int i=0;i<connectionCount;i++)
			close(connectedDeviceFD[i]);
		close(serverSocketFD);
		close(uartFD);
	}
}

/* THREAD FONKSIYONLARI */

// Keypad inputlari dinlenir ve islemler yapilir.
void *threadKeypadListen(void *parameter){
	char keypadInput[11];
	while(1){
		memset(&keypadInput[0],'\0',sizeof(keypadInput));
		sleep(1);
		// Keypadden max 11 karakter okunur.
		readInputKeypad(keypadInput,11);
		keypadInputOperations(keypadInput);
	}
}

// Sensor durumunu 2 saniye aralikla okur.
void *threadSensorValuesRead(void *parameter){
	while(1){
		// 1000000=1sn.
		usleep((int)2*1000000);
		for(int i=0;i<NUMBERSENSOR;i++){
			int sensorValue=GPIORead(sensorPins[i]);
			usleep((int)1000000);
			if(sensorValue!=-1){
				sensorValues[i]=sensorValue;
				printf("[INFO] - Pin Number: [%d] -> Value: [%d] \n",sensorPins[i],sensorValues[i]);
			}
		}
	}
}

// UART mesajini okur.
void *threadUARTRead(){
	while(1){
		if(uartFD!=-1){
			char rxBuffer[256];
			int rxLength=read(uartFD, (void*)rxBuffer, 255);
			if(rxLength>0){
				rxBuffer[rxLength]='\0';
				printf("[UART] - [%i] bytes received -> Message: [%s] \n", rxLength, rxBuffer);
				requestManageUARTorSocket(rxBuffer, &uartFD);
			}
		}
		sleep(1);
	}
}

// UART mesajini yazar.
void *threadUARTWrite(){
	while(1){
		char rsBuffer[20];
		if(uartFD!=-1){
			int count=write(uartFD, &rsBuffer[0], strlen(rsBuffer));
			if(count<0)
				printf("[ERROR] - UART TX error\n");
		}
		sleep(1);
	}
}

// Otomatik modda sensor durumu degisimine gore role durumunu degistirir.
void *threadRelayAutoMode(void* parameter){
	char *message=(char*)parameter;
	int sensorID=atoi(strtok(message," "));
	int sensorStatus=atoi(strtok(message," "));
	while(1){
		if(sensorValues[sensorID]==sensorStatus)
			GPIOWrite(RELAY,0);
		else
			GPIOWrite(RELAY,1);
	}

}

// Socket isteklerini kabul eder.
void *threadSocketConnection(void *parameter){
	while(1){
		if(connectionCount<MAX_CONNECTION){
			struct sockaddr_in clientSocket;
			socklen_t addrSize=sizeof(clientSocket);
			connectedDeviceFD[connectionCount]=accept(serverSocketFD, (struct sockaddr*)&clientSocket, &addrSize);
			if(connectedDeviceFD[connectionCount]==-1){
				printf("[ERROR] - Socket accept [%s]\n",strerror(errno));
				continue;
			}
			// Socket bloklanmasini onler.
			fcntl(connectedDeviceFD[connectionCount],F_SETFL,O_NONBLOCK);
			printf("[SERVER] - New connection FD:[%d], Address:[%s - %d]\n", connectedDeviceFD[connectionCount],inet_ntoa(clientSocket.sin_addr), clientSocket.sin_port);
			connectedDeviceList[connectionCount]=clientSocket;
			connectionCount++;
		}
	}
}

// Socket mesajlarini UART baglantisinda oldugu gibi parse eder ve ilgili islemi yapar.
void *threadSocketMessageManage(void *parameter){
	char message[20];
	int messageLength;
	while(1){
		for(int i=0;i<connectionCount;i++){
			messageLength=read(connectedDeviceFD[i], message, sizeof(message));
			if(messageLength!=-1){ // Okuma islemi basarili ise
				message[messageLength]='\0';
				printf("[MESSAGE] - [%s], From: [%s / %d]\n",message, inet_ntoa(connectedDeviceList[i].sin_addr), (connectedDeviceList[i].sin_port));
				requestManageUARTorSocket(message, &connectedDeviceFD[i]);
			}
		}
	}


}

/* END - THREAD FONKSIYONLARI */

int main(int argc, char **argv){

	if(argc!=2){
		printf("[ERROR] - False Arguments!! (USAGE: [App Name] [Port Number]\n) ");
		return -1;
	}
	// Arguman olarak alinan port numarasina gore socker ayarlamalari yapilir.
	int serverPortNo=atoi(argv[1]);

	// Adres olusturuluyor.
	serverAddress.sin_family=AF_INET;
	serverAddress.sin_port=htons(serverPortNo);
	serverAddress.sin_addr.s_addr=htonl(INADDR_ANY);

	// Sunucu olarak baglanti isteklerini dinleyecek soket yaratiliyor.
	serverSocketFD=socket(PF_INET, SOCK_STREAM, 0);
	if(serverSocketFD==-1){
		printf("[ERROR] on socket creation [%s]\n",strerror(errno));
		return -1;
	}

	// Sokete olusturulan adres ve ayarlar baglaniyor.
	if(bind(serverSocketFD, (struct sockaddr *) &serverAddress, sizeof(struct sockaddr))==-1){
		printf("[ERROR] on socket bind [%s]\n",strerror(errno));
		return -1;
	}

	// Baglanti istekleri icin dinlemeye baslaniyor.
	if(listen(serverSocketFD, MAX_CONNECTION)){
		printf("[ERROR] on socket listen [%s]\n",strerror(errno));
		return -1;
	}
	printf("Server socket created for [%d] connections. \n", MAX_CONNECTION);

	// SIGINT sinyali handler
	struct sigaction new_action, old_action;
	new_action.sa_handler=signalHandler;
	new_action.sa_flags=SA_NODEFER | SA_ONSTACK;
	sigaction(SIGINT, &new_action, &old_action);

	sem_init(&semConnDeviceFD,0,0);
	pthread_mutex_init(&mutex, 0);

	// Surekli olarak keypadi dinleyecek thread olustuluyor.
	pthread_create(&threadIDKeypadListen, NULL, threadKeypadListen, NULL);

	// Sensor degerlerinini okuyacak thread olusturuluyor.
	pthread_create(&threadIDSensorValuesRead, NULL, threadSensorValuesRead, NULL);

	// UART iletisimi icin ilk atamalar yapiliyor.
	uartFD=open("/dev/serial0", O_RDWR | O_NOCTTY | O_NDELAY );
	if(uartFD==-1)
		printf("[ERROR] - Unable to open UART!\n");
	struct termios options;
	tcgetattr(uartFD,&options);
	options.c_cflag=B9600 | CS8 | CLOCAL | CREAD;
	options.c_iflag=IGNPAR;
	options.c_oflag=0;
	options.c_lflag=0;
	tcflush(uartFD,TCIFLUSH);
	tcsetattr(uartFD,TCSANOW,&options);

	// UART ilk atamalari yapildiktan sonra okuma ve yazma islemleri icin threadler olusturuluyor.
	pthread_create(&threadIDUARTRead, NULL, threadUARTRead, NULL);
	sleep(1);
	pthread_create(&threadIDUARTWrite, NULL, threadUARTWrite, NULL);

	// Socket uzerinden gelen istekler icin thread olusturuluyor.
	pthread_create(&threadIDSocketConnection, NULL, threadSocketConnection, NULL);

	// Socket mesajlarini yoneten thread olusturuluyor.
	pthread_create(&threadIDSocketMessageManage, NULL, threadSocketMessageManage, NULL);

	// Sistemin kapanmasini engellemek icin thread bekleniyor.
	pthread_join(threadIDKeypadListen, NULL);

	// Tum dosya tanimlayicilar kapatiliyor.
	for(int i=0;i<connectionCount;i++)
		close(connectedDeviceFD[i]);
	close(serverSocketFD);
	close(uartFD);

	pthread_mutex_destroy(&mutex);
	sem_destroy(&semConnDeviceFD);
	return 0;
}

/* YAPILAN ISLEM FONKSIYONLARI */

// Keypad verisi parse edildikten sonra ilgili islemi gerceklestiren fonksiyondur.
void keypadInputOperations(char *keypadInput){
	char currPsw[5],newPsw[5],star[2],sharp[2],relayStat[2];

	// Parse islemi yapildiysa
	if(keypadInputParse(keypadInput,currPsw,star,newPsw,sharp,relayStat)){
		// Parola dogru girildiyse
		if(strcmp(currPsw, DEFAULT_PASSWORD)==0 && strcmp(star,"*")==0){
			// XXXX*YYYY# -> parola degisimi
			if(strlen(keypadInput)==10){
				strcpy(DEFAULT_PASSWORD, newPsw);
				printf("[INFO] - Password has changed.\n\n");
			}
			// XXXX*Y# -> role durum degisimi
			else if(strlen(keypadInput)==7){
				int ret=GPIOWrite(RELAY, atoi(relayStat));
				if(ret){
					if(atoi(relayStat)==0)
						printf("[INFO] - Relay Status [OFF]\n");
					else if(atoi(relayStat)==1)
						printf("[INFO] - Relay Status [ON]\n");
				}
			}
		}
		else
			printf("[WARNING] - Wrong Password! \n\n");
	}
}

void messageParseUARTorSocket(char *response, char *command, int *argument) {

    char *sensorToken;
    sensorToken = strtok(response, ":");

    if(!strncmp(sensorToken,"autoMode",8))
        strcpy(command,sensorToken);
    else
        sscanf(sensorToken, "%s", command);

    if (!strcmp(command, "sensorType") || !strcmp(command, "relayChange"))
        sscanf(response, "%s %d", command, argument);
}

void showInfoMessage(char *response, int connDevFD){
	if(write(connDevFD, response, strlen(response))!=-1)
		printf("[SUCCESS] - Writing: [%s] -> File Descriptor: [%d]\n\n",response, connDevFD);
	else
		printf("[ERROR] - Writing: [%s] -> File Descriptor: [%d]\n\n",response, connDevFD);
}

// UART veya Socket isteklerini yoneten fonksiyondur.
void requestManageUARTorSocket(char *rxBuffer, int *connectedDevFD){
	// Kritik bolge baslangici
	pthread_mutex_lock(&mutex);
	char command[10];
	int argument=-1;

	messageParseUARTorSocket(rxBuffer, command,&argument);

	printf("[INFO] - Request: [%s] -> Argument: [%d]\n", command,argument);

	char rsBuffer[20];
	if(strcmp(command,"sensorCount")==0){
		sprintf(rsBuffer, "sensorCount %d:", NUMBERSENSOR);
		showInfoMessage(rsBuffer, *connectedDevFD);
	}
	else if(strcmp(command,"sensorType")==0){
		sprintf(rsBuffer,"sensorType %s:", sensorName[argument]);
		showInfoMessage(rsBuffer, *connectedDevFD);
	}
	else if(strcmp(command,"sensorState")==0){
		int stateVal=0;
		for(int i=0;i<NUMBERSENSOR;i++)
			stateVal+=sensorValues[i]*pow(2,i);

		sprintf(rsBuffer,"sensorState %d:", stateVal);
		showInfoMessage(rsBuffer, *connectedDevFD);
	}
	else if(strcmp(command,"sensorList")==0){
		char rsBuffer[256];
		rsBuffer[0]='\0';

		for(int i=0;i<NUMBERSENSOR;i++){
			strcat(rsBuffer,sensorName[i]);
			if(i==NUMBERSENSOR-1)
				break;
			strcat(rsBuffer,",");
		}
		strcat(rsBuffer,":");
		printf("[INFO] - Sensor List: [%s]\n", rsBuffer);
		showInfoMessage(rsBuffer, *connectedDevFD);
	}
	else if(strcmp(command,"relayChange")==0){
		int ret=GPIOWrite(RELAY, argument);
		if(ret<0)
			sprintf(rsBuffer,"relayChange %s:", "Error");
		else
			sprintf(rsBuffer,"relayChange %s:", "OK");
		showInfoMessage(rsBuffer, *connectedDevFD);
	}
	else if(strcmp(command,"relayState")==0){
		sprintf(rsBuffer,"relayState %d:", GPIORead(RELAY));
		showInfoMessage(rsBuffer, *connectedDevFD);
	}
	else if(strncmp(command,"autoMode",8)==0){
		char *message,*tempCommand;
		tempCommand=strdup(command);
		message=strtok(tempCommand," ");

		while(message!=NULL){
			message=strtok(NULL," ");
			if(strcmp(message,"on")==0){
				printf("[INFO] - Automode ON! \n");
				//Automode islemini gerceklestirecek thread yaratiliyor.
				pthread_create(&threadIDRelayAutoMode, NULL, threadRelayAutoMode, (void*) message);
				break;
			} else {
				printf("[INFO] - Automode OFF! \n");
				pthread_cancel(threadIDRelayAutoMode);
				sprintf(rsBuffer,"autoMode %s:", "OK");
				showInfoMessage(rsBuffer, *connectedDevFD);
				break;
			}

		}
	}
	else if(strcmp(command,"close")==0){
		for(int i = 0 ; i < connectionCount; i++) {
			if (connectedDeviceFD[i] == *connectedDevFD) {
        			connectionCount--;
		 	        for (int j = i; j < connectionCount; j++) {
                			connectedDeviceFD[j] = connectedDeviceFD[j + 1];
		                	connectedDeviceList[j] = connectedDeviceList[j + 1];
            			}
				break;
			}
		}
		if(close(*connectedDevFD) < 0)
			printf("\n[ERROR] - Cannot Closed FD[%d]\n", *connectedDevFD);
		sem_post(&semConnDeviceFD);
   		sem_wait(&semConnDeviceFD);
	}

	pthread_mutex_unlock(&mutex);
}
/* END- ISLEM FONKSIYONLARI */
