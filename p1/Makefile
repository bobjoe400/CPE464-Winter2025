# Example makefile for CPE464 Trace program
#
# Just links in pcap library

CC = gcc
LIBS = -lpcap
CFLAGS = -g -Wall -pedantic -std=gnu99
#CGLAGS = 

all:  trace

trace: trace.c checksum.c utility.c
	$(CC) $(CFLAGS) -o $@ trace.c checksum.c utility.c $(LIBS)

clean:
	rm -f trace
