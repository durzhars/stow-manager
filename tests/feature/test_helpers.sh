#!/usr/bin/env bash
# tests/feature/test_helpers.sh
# Shared helper functions, assertion utilities, and sandboxing for feature tests.

set -u

# Color formatting for terminal output
if [ -t 1 ] || [ "${FORCE_COLOR:-0}" = "1" ]; then
    COLOR_RED='\033[0;31m'
    COLOR_GREEN='\033[0;32m'
    COLOR_YELLOW='\033[1;33m'
    COLOR_CYAN='\033[0;36m'
    COLOR_BOLD='\033[1m'
    COLOR_RESET='\033[0m'
else
    COLOR_RED=''
    COLOR_GREEN=''
    COLOR_YELLOW=''
    COLOR_CYAN=''
    COLOR_BOLD=''
    COLOR_RESET=''
fi

# Counters for test assertions
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Locate compiled stow-manager binary
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
STOW_BIN="${STOW_BIN:-$PROJECT_ROOT/bin/stow-manager}"

if [ ! -x "$STOW_BIN" ]; then
    echo -e "${COLOR_RED}Error: Binary '$STOW_BIN' not found or not executable.${COLOR_RESET}" >&2
    echo -e "${COLOR_YELLOW}Please build the project first using 'make'.${COLOR_RESET}" >&2
    exit 1
fi

# Sandbox setup & cleanup
TEST_TMPDIR=""
MOCK_HOME=""
MOCK_CONFIG=""
MOCK_DOTFILES=""
LAST_CMD_OUTPUT=""

setup_sandbox() {
    cleanup_sandbox
    TEST_TMPDIR=$(mktemp -d)
    MOCK_HOME="$TEST_TMPDIR/mock_home"
    MOCK_CONFIG="$MOCK_HOME/.config"
    MOCK_DOTFILES="$TEST_TMPDIR/mock_dotfiles"
    LAST_CMD_OUTPUT="$TEST_TMPDIR/last_cmd.out"

    mkdir -p "$MOCK_HOME" "$MOCK_CONFIG" "$MOCK_DOTFILES"

    export HOME="$MOCK_HOME"
    export XDG_CONFIG_HOME="$MOCK_CONFIG"
    export STOW_DOTFILES_DIR="$MOCK_DOTFILES"
}

cleanup_sandbox() {
    if [ -n "${TEST_TMPDIR:-}" ] && [ -d "${TEST_TMPDIR}" ]; then
        rm -rf "$TEST_TMPDIR"
        TEST_TMPDIR=""
    fi
}

trap cleanup_sandbox EXIT

# Assertions

assert_success() {
    local cmd="$1"
    local desc="${2:-Command expected to succeed: $cmd}"
    TESTS_RUN=$((TESTS_RUN + 1))

    if [ -z "${TEST_TMPDIR:-}" ]; then
        LAST_CMD_OUTPUT="$(mktemp)"
    else
        LAST_CMD_OUTPUT="$TEST_TMPDIR/last_cmd.out"
    fi

    eval "$cmd" > "$LAST_CMD_OUTPUT" 2>&1
    local exit_code=$?

    if [ $exit_code -eq 0 ]; then
        echo -e "  ${COLOR_GREEN}✓${COLOR_RESET} $desc"
        TESTS_PASSED=$((TESTS_PASSED + 1))
        return 0
    else
        echo -e "  ${COLOR_RED}✗${COLOR_RESET} $desc (Exit code: $exit_code)"
        if [ -f "$LAST_CMD_OUTPUT" ]; then
            echo -e "    ${COLOR_RED}Command output:${COLOR_RESET}"
            sed 's/^/      /' "$LAST_CMD_OUTPUT"
        fi
        TESTS_FAILED=$((TESTS_FAILED + 1))
        return 1
    fi
}

assert_failure() {
    local cmd="$1"
    local desc="${2:-Command expected to fail: $cmd}"
    TESTS_RUN=$((TESTS_RUN + 1))

    if [ -z "${TEST_TMPDIR:-}" ]; then
        LAST_CMD_OUTPUT="$(mktemp)"
    else
        LAST_CMD_OUTPUT="$TEST_TMPDIR/last_cmd.out"
    fi

    eval "$cmd" > "$LAST_CMD_OUTPUT" 2>&1
    local exit_code=$?

    if [ $exit_code -ne 0 ]; then
        echo -e "  ${COLOR_GREEN}✓${COLOR_RESET} $desc (Exit code: $exit_code)"
        TESTS_PASSED=$((TESTS_PASSED + 1))
        return 0
    else
        echo -e "  ${COLOR_RED}✗${COLOR_RESET} $desc (Expected non-zero exit code, got 0)"
        if [ -f "$LAST_CMD_OUTPUT" ]; then
            echo -e "    ${COLOR_RED}Command output:${COLOR_RESET}"
            sed 's/^/      /' "$LAST_CMD_OUTPUT"
        fi
        TESTS_FAILED=$((TESTS_FAILED + 1))
        return 1
    fi
}

resolve_canonical_path() {
    local target="$1"
    if command -v realpath >/dev/null 2>&1; then
        realpath "$target" 2>/dev/null || true
    elif readlink -f "$target" >/dev/null 2>&1; then
        readlink -f "$target" 2>/dev/null || true
    else
        readlink "$target" 2>/dev/null || true
    fi
}

assert_symlink_exists() {
    local target_path="$1"
    local expected_source="$2"
    local desc="${3:-Symlink $target_path -> $expected_source exists}"
    TESTS_RUN=$((TESTS_RUN + 1))

    if [ ! -L "$target_path" ]; then
        echo -e "  ${COLOR_RED}✗${COLOR_RESET} $desc ('$target_path' is not a symlink)"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        return 1
    fi

    local actual_resolved
    local expected_resolved
    actual_resolved=$(resolve_canonical_path "$target_path")
    expected_resolved=$(resolve_canonical_path "$expected_source")

    local raw_target
    raw_target=$(readlink "$target_path" 2>/dev/null || true)

    if [ "$actual_resolved" = "$expected_resolved" ] || [ "$raw_target" = "$expected_source" ]; then
        echo -e "  ${COLOR_GREEN}✓${COLOR_RESET} $desc"
        TESTS_PASSED=$((TESTS_PASSED + 1))
        return 0
    else
        echo -e "  ${COLOR_RED}✗${COLOR_RESET} $desc (Points to '$raw_target' / '$actual_resolved', expected '$expected_source')"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        return 1
    fi
}

assert_file_contains() {
    local file_path="$1"
    local search_text="$2"
    local desc="${3:-File '$file_path' contains '$search_text'}"
    TESTS_RUN=$((TESTS_RUN + 1))

    if [ ! -f "$file_path" ]; then
        echo -e "  ${COLOR_RED}✗${COLOR_RESET} $desc (File '$file_path' does not exist)"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        return 1
    fi

    if grep -q -F -- "$search_text" "$file_path"; then
        echo -e "  ${COLOR_GREEN}✓${COLOR_RESET} $desc"
        TESTS_PASSED=$((TESTS_PASSED + 1))
        return 0
    else
        echo -e "  ${COLOR_RED}✗${COLOR_RESET} $desc (Pattern '$search_text' not found in '$file_path')"
        echo -e "    ${COLOR_RED}File content:${COLOR_RESET}"
        sed 's/^/      /' "$file_path"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        return 1
    fi
}

assert_file_not_contains() {
    local file_path="$1"
    local search_text="$2"
    local desc="${3:-File '$file_path' does not contain '$search_text'}"
    TESTS_RUN=$((TESTS_RUN + 1))

    if [ ! -f "$file_path" ]; then
        echo -e "  ${COLOR_GREEN}✓${COLOR_RESET} $desc (File does not exist)"
        TESTS_PASSED=$((TESTS_PASSED + 1))
        return 0
    fi

    if grep -q -F -- "$search_text" "$file_path"; then
        echo -e "  ${COLOR_RED}✗${COLOR_RESET} $desc (Pattern '$search_text' was found in '$file_path')"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        return 1
    else
        echo -e "  ${COLOR_GREEN}✓${COLOR_RESET} $desc"
        TESTS_PASSED=$((TESTS_PASSED + 1))
        return 0
    fi
}

assert_path_exists() {
    local path="$1"
    local desc="${2:-Path '$path' exists}"
    TESTS_RUN=$((TESTS_RUN + 1))

    if [ -e "$path" ] || [ -L "$path" ]; then
        echo -e "  ${COLOR_GREEN}✓${COLOR_RESET} $desc"
        TESTS_PASSED=$((TESTS_PASSED + 1))
        return 0
    else
        echo -e "  ${COLOR_RED}✗${COLOR_RESET} $desc (Path '$path' does not exist)"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        return 1
    fi
}

assert_path_not_exists() {
    local path="$1"
    local desc="${2:-Path '$path' does not exist}"
    TESTS_RUN=$((TESTS_RUN + 1))

    if [ ! -e "$path" ] && [ ! -L "$path" ]; then
        echo -e "  ${COLOR_GREEN}✓${COLOR_RESET} $desc"
        TESTS_PASSED=$((TESTS_PASSED + 1))
        return 0
    else
        echo -e "  ${COLOR_RED}✗${COLOR_RESET} $desc (Path '$path' still exists)"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        return 1
    fi
}

assert_directory_not_symlink() {
    local dir_path="$1"
    local desc="${2:-Path '$dir_path' is a genuine directory and not a symlink}"
    TESTS_RUN=$((TESTS_RUN + 1))

    if [ -d "$dir_path" ] && [ ! -L "$dir_path" ]; then
        echo -e "  ${COLOR_GREEN}✓${COLOR_RESET} $desc"
        TESTS_PASSED=$((TESTS_PASSED + 1))
        return 0
    else
        echo -e "  ${COLOR_RED}✗${COLOR_RESET} $desc ('$dir_path' is not a directory or is still a symlink)"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        return 1
    fi
}

print_summary() {
    echo ""
    echo -e "${COLOR_BOLD}Test Summary:${COLOR_RESET}"
    echo -e "  Total Assertions: $TESTS_RUN"
    echo -e "  ${COLOR_GREEN}Passed:           $TESTS_PASSED${COLOR_RESET}"
    if [ $TESTS_FAILED -gt 0 ]; then
        echo -e "  ${COLOR_RED}Failed:           $TESTS_FAILED${COLOR_RESET}"
        return 1
    else
        echo -e "  Failed:           0"
        return 0
    fi
}
