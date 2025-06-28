# CFLAGS = -std=c++17 -I. -I/opt/homebrew/Cellar/boost/1.80.0/include/boost \
# 	-I/opt/homebrew/include \
# 	`pkg-config --cflags-only-I portaudio-2.0 sndfile fftw3f`

LDFLAGS = -lncurses

SOURCES = $(wildcard *.c) $(wildcard */*.c)

OBJS = $(wildcard *.o) $(wildcard */*.o)

OUTPUTFILE=minimidi.a

NOW := $(shell date +"%c" | tr ' :' '__')

compile: main.c
	gcc -g -o $(OUTPUTFILE) -g $(SOURCES) $(LDFLAGS) -Wall -pedantic

clean:
	rm -f $(OUTPUTFILE) $(OBJS)