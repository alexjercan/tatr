.PHONY: all clean install

CC = clang
CFLAGS = -Wall -Wextra -O2 -g
TARGET = tatr
SRC = tatr.c
PREFIX ?= /usr/local

all: $(TARGET)

$(TARGET): $(SRC) aids.h argparse.h
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	install -d $(PREFIX)/bin
	install -m 755 $(TARGET) $(PREFIX)/bin/$(TARGET)
