# Makefile to build the project

# Parameters
CC = gcc
CFLAGS = -Wall

SRC = src/
INCLUDE = include/
BIN = bin/

TEST_STRING = TEST

# Targets
.PHONY: all
all: download 

download: $(SRC)/*.c
	$(CC) $(CFLAGS) -o $@ $^ -I$(INCLUDE)

.PHONY: clean
clean:
	rm -f download
