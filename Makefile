SOURCES = $(wildcard *.c) $(wildcard */*.c)

OBJS = $(wildcard *.o) $(wildcard */*.o)

OUTPUTFILE=main.out

NOW := $(shell date +"%c" | tr ' :' '__')

compile: main.c
	gcc -g -o $(OUTPUTFILE) -g $(SOURCES) -Wall -pedantic