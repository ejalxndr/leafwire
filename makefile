CC       ?= cc
PKGS      = hidapi-hidraw x11 xext

CPPFLAGS += -D_GNU_SOURCE -Iinclude -Isrc
CFLAGS   ?= -std=c99 -O2 -g -Wall -Wextra -Wconversion -Wshadow -Wstrict-prototypes
CFLAGS   += $(shell pkg-config --cflags $(PKGS))
LDLIBS   += $(shell pkg-config --libs $(PKGS)) -lm

ifdef DEBUG
CFLAGS   := $(filter-out -O2,$(CFLAGS)) -O0 -fsanitize=address,undefined
LDFLAGS  += -fsanitize=address,undefined
endif

BUILD     = build
PREFIX   ?= /usr/local
BINDIR    = $(PREFIX)/bin
UNITDIR  ?= /etc/systemd/system

LIB_SRC   = src/nl_status.c src/color.c src/hid.c src/capture_x11.c \
            src/zone.c src/anim.c src/ipc.c
LIB_OBJ   = $(LIB_SRC:src/%.c=$(BUILD)/%.o)
LIB       = $(BUILD)/libnlctl.a

DAEMON    = $(BUILD)/nlctld
CLIENT    = $(BUILD)/nlctl
SELFTEST  = $(BUILD)/nl-selftest

TEST_SRC  = $(wildcard tests/test_*.c)
TEST_BIN  = $(TEST_SRC:tests/%.c=$(BUILD)/tests/%)

.PHONY: all clean test install

all: $(DAEMON) $(CLIENT) $(SELFTEST)

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(LIB): $(LIB_OBJ)
	$(AR) rcs $@ $^

$(DAEMON): $(BUILD)/nlctld.o $(LIB)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(CLIENT): $(BUILD)/nlctl.o $(LIB)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(SELFTEST): $(BUILD)/nl-selftest.o $(LIB)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILD)/tests/%: tests/%.c $(LIB) | $(BUILD)/tests
	$(CC) $(CPPFLAGS) $(CFLAGS) -Itests $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

test: $(TEST_BIN) $(CLIENT) $(DAEMON)
	@fail=0; for t in $(TEST_BIN); do \
		echo "== $$t"; $$t || fail=1; \
	done; \
	echo "== tests/cli_smoke.sh"; \
	BUILD=$(BUILD) sh tests/cli_smoke.sh || fail=1; \
	exit $$fail

$(BUILD) $(BUILD)/tests:
	mkdir -p $@

clean:
	rm -rf $(BUILD)

install: all
	install -d $(DESTDIR)$(BINDIR)
	install -m 0755 $(DAEMON) $(CLIENT) $(DESTDIR)$(BINDIR)/
	install -d $(DESTDIR)$(UNITDIR)
	install -m 0644 assets/nlctld.service $(DESTDIR)$(UNITDIR)/
