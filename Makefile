CC      = gcc
CFLAGS  = -g -Wall -pedantic -I/usr/include/portaudio
LDFLAGS = -lncurses -lpanel -lportaudio -lm -lyaml

SOURCES    = $(wildcard *.c) $(wildcard */*.c)
OBJS       = $(SOURCES:.c=.o)
OUTPUTFILE = minimidi

.PHONY: compile clean

compile: $(OBJS)
	$(CC) $(CFLAGS) -o $(OUTPUTFILE) $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OUTPUTFILE) $(OBJS)