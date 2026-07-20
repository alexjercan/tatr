.PHONY: all clean install guard

CC = clang
CFLAGS = -Wall -Wextra -O2 -g
TARGET = tatr
SRC = tatr.c
PREFIX ?= /usr/local

all: $(TARGET)

# Build guard: this project's canonical toolchain (clang + valgrind) comes from
# the nix dev shell. A bare login shell has only gcc, and the recurring
# build/test lessons all trace back to building outside the shell. Fail fast and
# point the way. Passes when inside `nix develop` (IN_NIX_SHELL) or a nix build
# sandbox (NIX_BUILD_TOP, e.g. `nix flake check`); an explicit
# TATR_ALLOW_BARE_BUILD=1 opts out for callers that provision the toolchain
# themselves (CI installs clang/gcc via apt and sets it).
guard:
	@if [ -z "$$IN_NIX_SHELL" ] && [ -z "$$NIX_BUILD_TOP" ] && [ -z "$$TATR_ALLOW_BARE_BUILD" ]; then \
		printf '%s\n' \
			'error: building outside the nix dev shell.' \
			'  run:  nix develop -c make' \
			'  (set TATR_ALLOW_BARE_BUILD=1 to override when the toolchain is provisioned another way)' 1>&2; \
		exit 1; \
	fi

$(TARGET): $(SRC) aids.h argparse.h | guard
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	install -d $(PREFIX)/bin
	install -m 755 $(TARGET) $(PREFIX)/bin/$(TARGET)
