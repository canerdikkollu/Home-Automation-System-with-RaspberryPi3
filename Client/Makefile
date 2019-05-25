CC=gcc
LINK=gcc
CFLAGS=-c -Wall -I.
OBJECT_FILES=contactAPI.o client.o

.SUFFIXES: .c .o

.c .o:
	$(CC) $^ -o $@ $<

appClient: $(OBJECT_FILES)
	$(LINK) $(OBJECT_FILES) -o $@

#Dependances
contactAPI.o: contactAPI.c contactAPI.h config.h
client.o: client.c contactAPI.h
