SHELL=/bin/bash
CC=gcc
CFLAGS= -Wall


LDLIBS=-L/usr/local/lib -lwebsockets
.PHONY: clean all

all: mmcom

mmcom: mmcom.o
mmcom.o: mmcom.c

%.c:
	$(CC) $(LDLIBS) -o $*.c $*.l

clean:
	$(RM) mmcom.o mmcom
