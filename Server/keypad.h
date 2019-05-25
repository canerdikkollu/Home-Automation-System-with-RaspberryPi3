#ifndef KEYPAD_H
#define KEYPAD_H
#include <stdbool.h>

char readFromKeypad();
bool readInputKeypad(char[],int);
bool keypadInputParse(char *, char *, char *, char *, char *, char *);

#endif
