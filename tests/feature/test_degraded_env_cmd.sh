#!/usr/bin/env bash
# tests/feature/test_degraded_env_cmd.sh
# Feature test suite for stow-manager degraded environment behavior (missing $HOME).

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/test_helpers.sh"

echo -e "${COLOR_CYAN}${COLOR_BOLD}=== Feature Tests: Degraded Environment & Clean Fatal Exits ===${COLOR_RESET}\n"

# 1. Config Show under missing $HOME
echo -e "${COLOR_BOLD}[Test 1] Config Show with missing $HOME${COLOR_RESET}"
setup_sandbox
mkdir -p "$STOW_DOTFILES_DIR/pkg1"
echo "config" > "$STOW_DOTFILES_DIR/pkg1/.file"

assert_success "HOME= $STOW_BIN config show" "stow-manager config show succeeded with empty HOME"
assert_file_contains "$LAST_CMD_OUTPUT" "\$HOME environment variable is not set" "config show indicates \$HOME is not set"

# 2. Stow package under missing $HOME triggers clean fatal exit (no /tmp usage)
echo -e "\n${COLOR_BOLD}[Test 2] Stow operation with missing $HOME triggers clean fatal exit${COLOR_RESET}"
setup_sandbox
mkdir -p "$STOW_DOTFILES_DIR/pkg1"
echo "config" > "$STOW_DOTFILES_DIR/pkg1/.file"

assert_failure "HOME= $STOW_BIN -n stow pkg1" "stow-manager stow fails cleanly when \$HOME is missing"
assert_file_contains "$LAST_CMD_OUTPUT" "Fatal: \$HOME environment variable is not set" "stow outputs fatal error explaining missing \$HOME"

# 3. Explicit flag override (-t) bypasses missing $HOME requirement
echo -e "\n${COLOR_BOLD}[Test 3] Explicit -t override succeeds despite missing $HOME${COLOR_RESET}"
setup_sandbox
CUSTOM_TGT="$TEST_TMPDIR/custom_target"
mkdir -p "$CUSTOM_TGT" "$STOW_DOTFILES_DIR/pkg1"
echo "config" > "$STOW_DOTFILES_DIR/pkg1/.file"

assert_success "HOME= $STOW_BIN -t $CUSTOM_TGT stow pkg1" "stow with -t override succeeded with empty HOME"
assert_symlink_exists "$CUSTOM_TGT/.file" "$STOW_DOTFILES_DIR/pkg1/.file" "Symlink created in explicit target despite missing HOME"

print_summary
