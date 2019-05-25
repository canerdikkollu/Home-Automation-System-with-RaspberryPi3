#include"gpioDIO.h"
#include"keypad.h"
#include<stdio.h>
#include<string.h>
#include<unistd.h>

char key[4][4]={{'1','2','3','A'},
		{'4','5','6','B'},
		{'7','8','9','C'},
		{'*','0','#','D'}};
int keyGPIOCol[4]={6,13,19,26};
int keyGPIORow[4]={16,20,21,12};

char readFromKeypad(){
    int k,i,j;
    for(i=0;i<4;i++){
        if(GPIOWrite(keyGPIOCol[i],1)==-1)
            return 'e';
        for(j=0;j<4;j++){
            if(( k=GPIORead(keyGPIORow[j]) )==-1)
                return 'e';
            if(k==1)
                return key[j][i];
        }
        if(GPIOWrite(keyGPIOCol[i],0)==-1)
            return 'e';
    }
    return 'n';
}

bool readInputKeypad(char keypadInput[],int length){

	char keypressed='n';
	char prevKeypressed = keypressed;

	sleep(1);
    	for(int index=0; keypressed != '#' && index < length; )
    	{
        	prevKeypressed = keypressed;
        	keypressed = readFromKeypad();

	        if( keypressed == 'e' || keypressed == 'n' )
		        continue;
        	if( prevKeypressed == 'n' )
                	keypadInput[index++] = keypressed;
    	}
	printf("\n[INFO] - Keypad Input: [%s] -> Input Size: [%d]\n", keypadInput, strlen(keypadInput));
	return true;
}

bool keypadInputParse(char* keypadInput, char* currPsw, char* star, char* newPsw, char *sharp, char* relayStat){

	//XXXX*YYYY# -> gelen input parola degisimi icin ise
	if(strlen(keypadInput)==10){
		sscanf(keypadInput, "%4s%1s%4s%1s", currPsw, star, newPsw, sharp);
		return true;
	}
	//XXXX*Y# -> gelen input role durumu degisimi icin ise
	else if(strlen(keypadInput)==7){
		sscanf(keypadInput, "%4s%1s%1s%1s", currPsw, star, relayStat, sharp);
		return true;
	}
	return false;
}
