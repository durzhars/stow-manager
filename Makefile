# Makefile for Dotfiles Stow Manager (Modern ISO C17)

CC ?= gcc
CFLAGS ?= -Wall -Wextra -pedantic -std=c17 -O2 -Iinclude
LDFLAGS ?=

SRC_DIR = src
INC_DIR = include
BUILD_DIR = build
BIN_DIR = bin
TEST_DIR = tests

SRCS = $(SRC_DIR)/main.c \
       $(SRC_DIR)/logger.c \
       $(SRC_DIR)/utils.c \
       $(SRC_DIR)/registry.c \
       $(SRC_DIR)/manifest.c \
       $(SRC_DIR)/checker.c \
       $(SRC_DIR)/scanner.c \
       $(SRC_DIR)/stow.c

OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
TARGET = $(BIN_DIR)/stow-manager

TEST_SRCS = $(TEST_DIR)/test_runner.c
TEST_OBJS = $(BUILD_DIR)/test_runner.o \
            $(filter-out $(BUILD_DIR)/main.o,$(OBJS))
TEST_TARGET = $(BIN_DIR)/test_runner

.PHONY: all clean static install test

all: $(TARGET)

$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET) $(LDFLAGS)

static: CFLAGS += -static
static: $(TARGET)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(TEST_TARGET): $(TEST_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(TEST_OBJS) -o $(TEST_TARGET) $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/test_runner.o: $(TEST_DIR)/test_runner.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Itests -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

install: $(TARGET)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/stow-manager
