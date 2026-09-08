CC ?= gcc
PKG_CONFIG ?= pkg-config

PKGS = portaudio-2.0 ncurses panel yaml-0.1
PKG_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PKGS) 2>/dev/null)
PKG_LIBS := $(shell $(PKG_CONFIG) --libs $(PKGS) 2>/dev/null)

CPPFLAGS += -D_POSIX_C_SOURCE=200809L $(PKG_CFLAGS)
CFLAGS += -std=c11 -g -Wall -Wextra -Wpedantic
LDLIBS += $(if $(PKG_LIBS),$(PKG_LIBS),-lncurses -lpanel -lportaudio -lyaml) -lm

SOURCES = main.c minimidi.c minimidi-proj-file.c minimidi-proj.c \
	minimidi-audio.c minimidi-rb.c minimidi-tui.c minimidi-log.c
OBJS = $(SOURCES:.c=.o)
OUTPUTFILE = minimidi
TEST_OUTPUT = tests/test_refactor
TEST_SOURCES = tests/test_refactor.c minimidi.c minimidi-proj-file.c \
	minimidi-proj.c minimidi-audio.c minimidi-rb.c minimidi-log.c

.PHONY: compile clean test sanitize

compile: $(OUTPUTFILE)

$(OUTPUTFILE): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDLIBS)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(TEST_OUTPUT): $(TEST_SOURCES)
	$(CC) $(CPPFLAGS) $(CFLAGS) -I. -o $@ $(TEST_SOURCES) $(LDLIBS)

test: $(TEST_OUTPUT)
	./$(TEST_OUTPUT)

sanitize:
	$(MAKE) clean
	$(MAKE) $(TEST_OUTPUT) CFLAGS="$(CFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer" \
		LDLIBS="$(LDLIBS) -fsanitize=address,undefined"
	ASAN_OPTIONS=detect_leaks=0 ./$(TEST_OUTPUT)

clean:
	rm -f $(OUTPUTFILE) $(wildcard *.o */*.o) $(TEST_OUTPUT)
