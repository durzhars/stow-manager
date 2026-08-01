#!/usr/bin/env bash
# tests/feature/test_config_cmd.sh
# Feature test suite for stow-manager configuration commands.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/test_helpers.sh"

echo -e "${COLOR_CYAN}${COLOR_BOLD}=== Feature Tests: Config Operations ===${COLOR_RESET}\n"

# 1. Config Show
echo -e "${COLOR_BOLD}[Test 1] Config Show${COLOR_RESET}"
setup_sandbox
assert_success "$STOW_BIN config show" "stow-manager config show succeeded"
assert_file_contains "$LAST_CMD_OUTPUT" "Stow Manager Configuration" "config show displays configuration header"
assert_file_contains "$LAST_CMD_OUTPUT" "Target Directory:" "config show outputs target home path section"

# 2. Config Set [target|dotfiles]
echo -e "\n${COLOR_BOLD}[Test 2] Config Set (target & dotfiles)${COLOR_RESET}"
setup_sandbox
CUSTOM_TARGET="$TEST_TMPDIR/custom_target"
CUSTOM_DOTFILES="$TEST_TMPDIR/custom_dotfiles"
mkdir -p "$CUSTOM_TARGET" "$CUSTOM_DOTFILES"

assert_success "$STOW_BIN config set target $CUSTOM_TARGET" "stow-manager config set target succeeded"
assert_success "$STOW_BIN config set dotfiles $CUSTOM_DOTFILES" "stow-manager config set dotfiles succeeded"

CONFIG_FILE="$XDG_CONFIG_HOME/stow-manager/config"
assert_file_contains "$CONFIG_FILE" "TARGET_DIR=$CUSTOM_TARGET" "config file updated with TARGET_DIR"
assert_file_contains "$CONFIG_FILE" "$CUSTOM_DOTFILES" "config file updated with DOTFILES_DIRS"

assert_success "$STOW_BIN config show" "stow-manager config show succeeded with updated settings"
assert_file_contains "$LAST_CMD_OUTPUT" "$CUSTOM_TARGET" "config show outputs configured target path"
assert_file_contains "$LAST_CMD_OUTPUT" "$CUSTOM_DOTFILES" "config show outputs configured dotfiles repository path"

# 3. Config Add & Remove (Multi-Repository Search Paths)
echo -e "\n${COLOR_BOLD}[Test 3] Config Add & Remove (Multi-Repository Paths)${COLOR_RESET}"
setup_sandbox
REPO_PRIMARY="$TEST_TMPDIR/primary_repo"
REPO_SECONDARY="$TEST_TMPDIR/secondary_repo"
mkdir -p "$REPO_PRIMARY" "$REPO_SECONDARY"

assert_success "$STOW_BIN config set dotfiles $REPO_PRIMARY" "Set primary repository path succeeded"
assert_success "$STOW_BIN config add $REPO_SECONDARY" "stow-manager config add secondary repository succeeded"

CONFIG_FILE="$XDG_CONFIG_HOME/stow-manager/config"
assert_file_contains "$CONFIG_FILE" "$REPO_PRIMARY:$REPO_SECONDARY" "Config file contains colon-separated multi-repository paths"

assert_success "$STOW_BIN config show" "stow-manager config show displays multi-repository configuration"
assert_file_contains "$LAST_CMD_OUTPUT" "$REPO_PRIMARY" "config show contains primary repository path"
assert_file_contains "$LAST_CMD_OUTPUT" "$REPO_SECONDARY" "config show contains secondary repository path"

assert_success "$STOW_BIN config remove $REPO_SECONDARY" "stow-manager config remove succeeded"
assert_file_not_contains "$CONFIG_FILE" "$REPO_SECONDARY" "Config file no longer contains removed repository path"

print_summary
