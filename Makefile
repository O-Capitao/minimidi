CC ?= gcc
PKG_CONFIG ?= pkg-config

PKGS = portaudio-2.0 ncursesw panelw yaml-0.1
PKG_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PKGS) 2>/dev/null)
PKG_LIBS := $(shell $(PKG_CONFIG) --libs $(PKGS) 2>/dev/null)

CPPFLAGS += -Isrc -D_POSIX_C_SOURCE=200809L $(PKG_CFLAGS)
CFLAGS += -std=c11 -g -Wall -Wextra -Wpedantic
DEPFLAGS = -MMD -MP
LDLIBS += $(if $(PKG_LIBS),$(PKG_LIBS),-lncursesw -lpanelw -lportaudio -lyaml) -lm

LIB_SOURCES = src/midi/minimidi.c src/project/minimidi-proj-file.c \
	src/project/minimidi-proj.c src/audio/minimidi-audio.c \
	src/common/minimidi-rb.c src/common/minimidi-log.c
SOURCES = src/main.c $(LIB_SOURCES) src/tui/minimidi-tui.c
OBJS = $(SOURCES:.c=.o)
DEPS = $(OBJS:.o=.d)
OUTPUTFILE = minimidi
TEST_OUTPUT = tests/test_suite
TEST_SOURCES = tests/test_suite.c $(LIB_SOURCES)
HEADERS = $(wildcard src/*/*.h)

.PHONY: compile clean test sanitize

compile: $(OUTPUTFILE)

$(OUTPUTFILE): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDLIBS)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

-include $(DEPS)

$(TEST_OUTPUT): $(TEST_SOURCES) $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(TEST_SOURCES) $(LDLIBS)

test: $(TEST_OUTPUT)
	./$(TEST_OUTPUT)

sanitize:
	$(MAKE) clean
	$(MAKE) $(TEST_OUTPUT) CFLAGS="$(CFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer" \
		LDLIBS="$(LDLIBS) -fsanitize=address,undefined"
	ASAN_OPTIONS=detect_leaks=0 ./$(TEST_OUTPUT)

clean:
	rm -f $(OUTPUTFILE) $(OBJS) $(DEPS) \
		$(wildcard *.o */*.o) $(wildcard *.d */*.d) \
		tests/test_suite tests/test_refactor
