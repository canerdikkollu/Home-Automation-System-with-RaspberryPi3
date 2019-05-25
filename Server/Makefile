CC=gcc
LINK=gcc
CFLAGS=-c -Wall -I.
OBJECT_FILES= gpioDIO.o keypad.o server.o

.SUFFIXES: .c .o

.c .o:
	$(CC) $^ -o $@ $<

appServer: $(OBJECT_FILES)
	$(LINK) $(OBJECT_FILES) -o $@ -lpthread -lm

keypad.o : keypad.c keypad.h gpioDIO.h
gpioDIO.o: gpioDIO.c gpioDIO.h gpioTypes.h
server.o: server.c gpioDIO.h keypad.h config.h
