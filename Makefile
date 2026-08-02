# Makefile for Dotfiles Stow Manager (ISO C17)

PREFIX ?= /usr/local
EXEC_PREFIX ?= $(PREFIX)
BINDIR ?= $(EXEC_PREFIX)/bin
DATAROOTDIR ?= $(PREFIX)/share
DATADIR ?= $(DATAROOTDIR)
SYSCONFDIR ?= $(PREFIX)/etc

CC ?= gcc
CFLAGS ?= -Wall -Wextra -pedantic -Wconversion -Wsign-conversion -Wno-overlength-strings -std=c17 -O2 -Iinclude -DDATADIR=\"$(DATADIR)\" -DSYSCONFDIR=\"$(SYSCONFDIR)\"
LDFLAGS ?=

SRC_DIR = src
INC_DIR = include
BUILD_DIR = build
BIN_DIR = bin
TEST_DIR = tests
TEST_UNIT_DIR = $(TEST_DIR)/unit
TEST_FEATURE_DIR = $(TEST_DIR)/feature

SRCS = $(SRC_DIR)/main.c \
	   $(SRC_DIR)/cli.c \
	   $(SRC_DIR)/cmd_dispatch.c \
	   $(SRC_DIR)/ignore.c \
       $(SRC_DIR)/help.c \
       $(SRC_DIR)/logger.c \
       $(SRC_DIR)/utils/mem.c \
       $(SRC_DIR)/utils/path.c \
       $(SRC_DIR)/utils/fs.c \
       $(SRC_DIR)/utils/env.c \
       $(SRC_DIR)/utils/signal.c \
       $(SRC_DIR)/utils/stowignore.c \
       $(SRC_DIR)/config.c \
       $(SRC_DIR)/registry.c \
       $(SRC_DIR)/manifest.c \
       $(SRC_DIR)/checker.c \
       $(SRC_DIR)/scanner.c \
       $(SRC_DIR)/stow.c

OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
TARGET = $(BIN_DIR)/stow-manager

TEST_SRCS = $(wildcard $(TEST_UNIT_DIR)/*.c)
TEST_OBJS = $(patsubst $(TEST_UNIT_DIR)/%.c,$(BUILD_DIR)/%.o,$(TEST_SRCS)) \
            $(filter-out $(BUILD_DIR)/main.o,$(OBJS))
TEST_TARGET = $(BIN_DIR)/test_runner

.PHONY: all clean static install test test-feature uninstall tidy format format-check build-clang-opt build-sanitize

all: $(TARGET)

$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET) $(LDFLAGS)

tidy:
	clang-tidy $(SRCS) $(TEST_SRCS) -- -Iinclude -Itests/unit -std=c17

format:
	clang-format -i $(SRCS) $(TEST_SRCS) include/*.h tests/unit/*.h

format-check:
	clang-format --dry-run --Werror $(SRCS) $(TEST_SRCS) include/*.h tests/unit/*.h

# Compile using Clang with Thin LTO and optimization remarks
build-clang-opt: CC = clang
build-clang-opt: CFLAGS += -flto=thin -fsave-optimization-record -Rpass=inline -Rpass-missed=loop-vectorize
build-clang-opt: LDFLAGS += -flto=thin -fuse-ld=lld
build-clang-opt: clean $(TARGET)

# Compile using Clang with sanitizers enabled (ASan + UBSan)
build-sanitize: CC = clang
build-sanitize: CFLAGS += -fsanitize=address,undefined -fno-omit-frame-pointer -g
build-sanitize: LDFLAGS += -fsanitize=address,undefined
build-sanitize: clean $(TARGET)

static: CFLAGS += -static
static: $(TARGET)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

test-feature: $(TARGET)
	bash $(TEST_FEATURE_DIR)/run_feature_tests.sh

$(TEST_TARGET): $(TEST_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(TEST_OBJS) -o $(TEST_TARGET) $(LDFLAGS)

HELP_TXT_GEN = $(BUILD_DIR)/help_text_plain.h

$(HELP_TXT_GEN): resources/help.txt | $(BUILD_DIR)
	@echo "Generating embedded plain text help string from resources/help.txt..."
	@echo "static const char *EMBEDDED_HELP_TXT =" > $@
	@sed -e 's/\\/\\\\/g' -e 's/"/\\"/g' -e 's/^/"/' -e 's/$$/\\n"/' $< >> $@
	@echo ";" >> $@

$(BUILD_DIR)/main.o: $(SRC_DIR)/main.c $(HELP_TXT_GEN) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(BUILD_DIR) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(TEST_UNIT_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(TEST_UNIT_DIR) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

install: $(TARGET)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/stow-manager
	install -d $(DESTDIR)$(DATADIR)/stow-manager
	install -m 644 resources/help.md $(DESTDIR)$(DATADIR)/stow-manager/help.md
	install -m 644 resources/help.txt $(DESTDIR)$(DATADIR)/stow-manager/help.txt
	install -m 644 resources/stowignore.default $(DESTDIR)$(DATADIR)/stow-manager/stowignore.default

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/stow-manager
	rm -rf $(DESTDIR)$(DATADIR)/stow-manager
