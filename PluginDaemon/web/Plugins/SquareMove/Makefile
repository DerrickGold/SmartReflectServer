# CMPT 399 (Winter 2016) Assignment 2
# Syntactic Analyzer
# Derrick Gold
#
# Makefile
# Rules to build MacEwan Teeny Pascal

SHELL=/bin/bash
CC=gcc
CFLAGS= -Wall


LDLIBS=-L/usr/local/lib -lwebsockets
.PHONY: clean all

all: move

move: move.o
move.o: move.c

%.c:
	$(CC) $(LDLIBS) -o $*.c $*.l

clean:
	$(RM) move.o move
