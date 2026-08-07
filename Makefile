# Makefile for Dotfiles Stow Manager (ISO C17)

PREFIX ?= /usr/local
BIN_NAME ?= stow-manager
EXEC_PREFIX ?= $(PREFIX)
BINDIR ?= $(EXEC_PREFIX)/bin
DATAROOTDIR ?= $(PREFIX)/share
DATADIR ?= $(DATAROOTDIR)
SYSCONFDIR ?= $(PREFIX)/etc

SRC_DIR = src
INC_DIR = include
BUILD_DIR = build
BUILD_TEST_DIR = $(BUILD_DIR)/tests
BIN_DIR = bin
OPT_DIR = $(BUILD_DIR)/opt_records
TEST_DIR = tests
TEST_UNIT_DIR = $(TEST_DIR)/unit
TEST_FEATURE_DIR = $(TEST_DIR)/feature

# Reusable Clang optimization & diagnostic profiles
CC ?= gcc
CFLAGS ?= -Wall -Wextra -pedantic -Wconversion -Wsign-conversion \
          -Wno-overlength-strings -std=c17 -O2 -Iinclude -I$(BUILD_DIR) \
          -DDATADIR=$(DATADIR) -DSYSCONFDIR=$(SYSCONFDIR)
CLANG_OPT_FLAGS = -O3 -fomit-frame-pointer -flto=thin -fsave-optimization-record=yaml \
                  -foptimization-record-file=$(OPT_DIR)/opt.yaml \
                  -Rpass=inline -Rpass-missed=loop-vectorize \
                  -Oz -ffunction-sections -fdata-sections -fomit-frame-pointer -flto=thin
CLANG_OPT_LDFLAGS = -flto=thin -fuse-ld=lld -Wl,--gc-sections -Wl,--icf=all -Wl,-s
CLANG_SAN_FLAGS = -fsanitize=address,undefined -fno-omit-frame-pointer -g
CLANG_SIZE_FLAGS   = -Oz -ffunction-sections -fdata-sections -fomit-frame-pointer -flto=thin
CLANG_SIZE_LDFLAGS = -flto=thin -fuse-ld=lld -Wl,--gc-sections -Wl,--icf=all -Wl,-s

DEPFLAGS = -MMD -MP
LDFLAGS ?=

SRCS = $(SRC_DIR)/main.c \
       $(SRC_DIR)/cli/cli.c \
       $(SRC_DIR)/cli/cmd_dispatch.c \
       $(SRC_DIR)/cli/cmd_table.c \
       $(SRC_DIR)/cli/help.c \
       $(SRC_DIR)/core/checker.c \
       $(SRC_DIR)/core/config.c \
       $(SRC_DIR)/core/ignore.c \
       $(SRC_DIR)/core/manifest.c \
       $(SRC_DIR)/core/registry.c \
       $(SRC_DIR)/core/scanner.c \
       $(SRC_DIR)/core/stow.c \
       $(SRC_DIR)/core/stowignore.c \
       $(SRC_DIR)/utils/env.c \
       $(SRC_DIR)/utils/fs.c \
       $(SRC_DIR)/utils/logger.c \
       $(SRC_DIR)/utils/mem.c \
       $(SRC_DIR)/utils/path.c \
       $(SRC_DIR)/utils/signal.c \
       $(SRC_DIR)/utils/str.c \
       $(SRC_DIR)/utils/timer.c

OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
SRC_DEPS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/.deps/%.d,$(SRCS))
DEPS = $(SRC_DEPS) $(TEST_DEPS)
TARGET = $(BIN_DIR)/$(BIN_NAME)

TEST_SRCS = $(wildcard $(TEST_UNIT_DIR)/*.c $(TEST_UNIT_DIR)/*/*.c)
TEST_OBJS = $(patsubst $(TEST_UNIT_DIR)/%.c,$(BUILD_TEST_DIR)/%.o,$(TEST_SRCS)) \
            $(filter-out $(BUILD_DIR)/main.o,$(OBJS))
TEST_DEPS = $(patsubst $(TEST_UNIT_DIR)/%.c,$(BUILD_TEST_DIR)/.deps/%.d,$(TEST_SRCS))
TEST_TARGET = $(BIN_DIR)/test_runner

.PHONY: all clean static install test test-feature uninstall tidy format format-check build-clang-opt build-sanitize build-pgo help

all: $(TARGET)

help:
	@echo "  Build Targets:"
	@echo ""
	@echo "  make                    Build release binary using default compiler ($(CC))"
	@echo "  make test               Run unit test suite"
	@echo "  make test-feature       Run end-to-end integration feature tests"
	@echo "  make clean              Clean build and bin output directories"
	@echo ""
	@echo "  Clang Optimization & Diagnostics Targets:"
	@echo "  make build-clang-opt      Build with Clang ThinLTO, -O3, and optimization remarks"
	@echo "  make build-pgo            Build with Clang 2-stage Profile-Guided Optimization"
	@echo "  make build-sanitize       Build with Clang AddressSanitizer & UBSanitizer"
	@echo "  make tidy                 Run clang-tidy static analysis"
	@echo "  make format               Format source files with clang-format"
	@echo "  make format-check         Check source files formatting compliance with clang-format"
	@echo ""
	@echo "  Installations:"
	@echo "  make install              Install build into $(PREFIX)/$(BIN_DIR)/$(BIN_NAME)"
	@echo "  make uninstall            Uninstall binaries and data files"

$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET) $(LDFLAGS)

ALL_HDRS = $(shell find $(INC_DIR) $(TEST_UNIT_DIR) -name '*.h')

tidy:
	clang-tidy $(SRCS) $(TEST_SRCS) -- -Iinclude -Itests/unit -std=c17

format:
	clang-format -i $(SRCS) $(TEST_SRCS) $(ALL_HDRS)

format-check:
	clang-format --dry-run --Werror $(SRCS) $(TEST_SRCS) $(ALL_HDRS)

build-clang-opt:
	$(MAKE) clean
	mkdir -p $(OPT_DIR)
	$(MAKE) CC=clang CFLAGS="$(CFLAGS) $(CLANG_OPT_FLAGS)" LDFLAGS="$(LDFLAGS) $(CLANG_OPT_LDFLAGS)" $(TARGET)

build-sanitize:
	$(MAKE) clean
	$(MAKE) CC=clang CFLAGS="$(CFLAGS) $(CLANG_SAN_FLAGS)" LDFLAGS="$(LDFLAGS) $(CLANG_SAN_FLAGS)" $(TARGET)

build-pgo:
	@echo "=== Stage 1: Building instrumented binary ==="
	$(MAKE) clean
	mkdir -p $(OPT_DIR)
	$(MAKE) CC=clang CFLAGS="$(CFLAGS) $(CLANG_OPT_FLAGS) -fprofile-instr-generate" LDFLAGS="$(LDFLAGS) $(CLANG_OPT_LDFLAGS) -fprofile-instr-generate" $(TARGET)
	@echo "=== Stage 2: Collecting execution workload profile ==="
	-@bash $(TEST_FEATURE_DIR)/run_feature_tests.sh > /dev/null 2>&1
	@llvm-profdata merge -output=stow_app.profdata default.profraw 2>/dev/null || true
	@echo "=== Stage 3: Compiling PGO production binary with profile feedback ==="
	$(MAKE) clean
	mkdir -p $(OPT_DIR)
	$(MAKE) CC=clang CFLAGS="$(CFLAGS) $(CLANG_OPT_FLAGS) -fprofile-instr-use=stow_app.profdata" LDFLAGS="$(LDFLAGS) $(CLANG_OPT_LDFLAGS) -fprofile-instr-use=stow_app.profdata" $(TARGET)
	@rm -f default.profraw stow_app.profdata
	@echo "=== PGO build complete ==="

build-size:
	$(MAKE) clean
	$(MAKE) CC=clang CFLAGS="$(filter-out -O2 -O3,$(CFLAGS)) $(CLANG_SIZE_FLAGS)" LDFLAGS="$(LDFLAGS) $(CLANG_SIZE_LDFLAGS)" $(TARGET)

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

$(BUILD_DIR)/main.o: $(HELP_TXT_GEN)
$(BUILD_DIR)/cli/help.o: $(HELP_TXT_GEN)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@) $(dir $(BUILD_DIR)/.deps/$*)
	$(CC) $(CFLAGS) $(DEPFLAGS) -MF $(BUILD_DIR)/.deps/$*.d -c $< -o $@

$(BUILD_TEST_DIR)/%.o: $(TEST_UNIT_DIR)/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@) $(dir $(BUILD_TEST_DIR)/.deps/$*)
	$(CC) $(CFLAGS) $(DEPFLAGS) -MF $(BUILD_TEST_DIR)/.deps/$*.d -I$(TEST_UNIT_DIR) -c $< -o $@

-include $(DEPS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(OPT_DIR): | $(BUILD_DIR)
	mkdir -p $(OPT_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR) *.opt.yaml

install: $(TARGET)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/$(BIN_NAME)
	install -d $(DESTDIR)$(DATADIR)/$(BIN_NAME)
	install -m 644 resources/help.md $(DESTDIR)$(DATADIR)/$(BIN_NAME)/help.md
	install -m 644 resources/help.txt $(DESTDIR)$(DATADIR)/$(BIN_NAME)/help.txt
	install -m 644 resources/stowignore.default $(DESTDIR)$(DATADIR)/$(BIN_NAME)/stowignore.default
	install -m 644 resources/stowignore.template $(DESTDIR)$(DATADIR)/$(BIN_NAME)/stowignore.template
	install -m 644 resources/stowdeps.template $(DESTDIR)$(DATADIR)/$(BIN_NAME)/stowdeps.template

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(BIN_NAME)
	rm -rf $(DESTDIR)$(DATADIR)/$(BIN_NAME)
