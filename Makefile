# Makefile for k-colorability
# author: Michael Helm, 11810354

CC = gcc
DEFS = -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_POSIX_C_SOURCE=200809L
# CFLAGS = -std=c99 -pedantic -Wall -g $(DEFS) #devflags
CFLAGS = -std=c11 -O3 -DNDEBUG -march=native -flto $(DEFS) # faster
LDFLAGS =

BUILD_DIR = build
PROGRAMS = color2sat
OBJS = $(patsubst %, $(BUILD_DIR)/%.o, $(PROGRAMS))

.PHONY: all clean

all: $(PROGRAMS)

$(PROGRAMS): %: $(BUILD_DIR)/%.o
	$(CC) -o $@ $< $(LDFLAGS)

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR) $(PROGRAMS)

$(BUILD_DIR)/color2sat.o: color2sat.c
