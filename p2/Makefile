# Makefile for CPE464 tcp test code
# written by Hugh Smith - April 2019

CC= gcc
CFLAGS= -g -Wall -std=gnu99
LIBS = 

OBJS = networks.o gethostbyname.o pollLib.o safeUtil.o lab2.o

all: cclient server

cclient: cclient.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $? $(LIBS)

server: server.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $? $(LIBS)

.c.o:
	gcc -c $(CFLAGS) $< -o $@ $(LIBS)

cleano:
	rm -f *.o

clean:
	rm -f cclient server *.o




