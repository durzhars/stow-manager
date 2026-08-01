#!/usr/bin/env bash
# tests/feature/test_degraded_env_cmd.sh
# Feature test suite for stow-manager degraded environment behavior (missing $HOME, missing XDG_CONFIG_HOME).

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/test_helpers.sh"

echo -e "${COLOR_CYAN}${COLOR_BOLD}=== Feature Tests: Degraded Environment & Fallback Warnings ===${COLOR_RESET}\n"

# 1. Config Show under missing $HOME
echo -e "${COLOR_BOLD}[Test 1] Config Show with missing $HOME${COLOR_RESET}"
setup_sandbox
mkdir -p "$STOW_DOTFILES_DIR/pkg1"
echo "config" > "$STOW_DOTFILES_DIR/pkg1/.file"

assert_success "HOME= $STOW_BIN config show" "stow-manager config show succeeded with empty HOME"
assert_file_contains "$LAST_CMD_OUTPUT" "WARNING: \$HOME is missing/unset" "config show outputs warning for missing \$HOME"

# 2. Stow package under missing $HOME warns about fallback to /tmp
echo -e "\n${COLOR_BOLD}[Test 2] Stow operation with missing $HOME triggers fallback warning${COLOR_RESET}"
setup_sandbox
mkdir -p "$STOW_DOTFILES_DIR/pkg1"
echo "config" > "$STOW_DOTFILES_DIR/pkg1/.file"

assert_success "HOME= $STOW_BIN -n stow pkg1" "stow-manager stow dry-run succeeded with empty HOME"
assert_file_contains "$LAST_CMD_OUTPUT" "HOME environment variable is missing or empty!" "stow outputs warning when falling back to /tmp"

# 3. Explicit flag override (-t) bypasses missing $HOME fallback warning
echo -e "\n${COLOR_BOLD}[Test 3] Explicit -t override bypasses missing $HOME fallback warning${COLOR_RESET}"
setup_sandbox
CUSTOM_TGT="$TEST_TMPDIR/custom_target"
mkdir -p "$CUSTOM_TGT" "$STOW_DOTFILES_DIR/pkg1"
echo "config" > "$STOW_DOTFILES_DIR/pkg1/.file"

assert_success "HOME= $STOW_BIN -t $CUSTOM_TGT stow pkg1" "stow with -t override succeeded with empty HOME"
assert_symlink_exists "$CUSTOM_TGT/.file" "$STOW_DOTFILES_DIR/pkg1/.file" "Symlink created in explicit target despite missing HOME"
assert_file_not_contains "$LAST_CMD_OUTPUT" "Falling back to target directory" "Explicit target flag suppresses HOME fallback warning"

print_summary
