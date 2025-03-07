# Makefile for CPE464 tcp and udp test code
# updated by Hugh Smith - April 2023

CC= gcc
CFLAGS= -g -Wall -std=gnu99
LIBS = 

OBJS = networks.o gethostbyname.o pollLib.o safeUtil.o window.o packet.o

#uncomment next two lines if you're using sendtoErr() library
LIBS += libcpe464.2.21.a -lstdc++ -ldl
CFLAGS += -D__LIBCPE464_

all: rcopy server

rcopy: rcopy.c $(OBJS)
	$(CC) $(CFLAGS) -o rcopy rcopy.c $(OBJS) $(LIBS)

server: server.c $(OBJS)
	$(CC) $(CFLAGS) -o server server.c $(OBJS) $(LIBS)

.c.o:
	gcc -c $(CFLAGS) $< -o $@

cleano:
	rm -f *.o

clean:
	rm -f server rcopy *.o

# Target-specific variable assignment:
debug: CFLAGS += -DDEBUG_ON
debug: all
