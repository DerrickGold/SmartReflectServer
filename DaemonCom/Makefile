# CMPT 399 (Winter 2016) Assignment 2
# Syntactic Analyzer
# Derrick Gold
#
# Makefile
# Rules to build MacEwan Teeny Pascal

SHELL=/bin/bash
CC=gcc
CFLAGS= -Wall -march=armv7-a -mfloat-abi=hard -mfpu=vfpv3-d16


LDLIBS=-L/usr/local/lib -lwebsockets
.PHONY: clean all

all: mmcom

mmcom: mmcom.o
mmcom.o: mmcom.c

%.c:
	$(CC) $(LDLIBS) -o $*.c $*.l

clean:
	$(RM) mmcom.o mmcom
