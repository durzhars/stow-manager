#!/usr/bin/env bash
# tests/feature/test_deps_cmd.sh
# Feature test suite for stow-manager dependency management commands.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/test_helpers.sh"

echo -e "${COLOR_CYAN}${COLOR_BOLD}=== Feature Tests: Dependency Management ===${COLOR_RESET}\n"

# 1. make:package <name>
echo -e "${COLOR_BOLD}[Test 1] Scaffold package (make:package)${COLOR_RESET}"
setup_sandbox

assert_success "$STOW_BIN make:package editor" "stow-manager make:package editor succeeded"
assert_path_exists "$STOW_DOTFILES_DIR/editor" "Scaffolded package directory created"
MANIFEST_FILE="$STOW_DOTFILES_DIR/editor/.stowdeps"
assert_path_exists "$MANIFEST_FILE" ".stowdeps manifest file created"
assert_file_contains "$MANIFEST_FILE" "Package Dependency Manifest for 'editor'" ".stowdeps header initialized"

# 2. deps:add [--required | --optional | --conflict]
echo -e "\n${COLOR_BOLD}[Test 2] Add dependencies & conflicts (deps:add)${COLOR_RESET}"
setup_sandbox
mkdir -p "$STOW_DOTFILES_DIR/terminal"

assert_success "$STOW_BIN deps:add terminal bash --required" "deps:add terminal bash --required succeeded"
MANIFEST_FILE="$STOW_DOTFILES_DIR/terminal/.stowdeps"
assert_file_contains "$MANIFEST_FILE" 'REQUIRED="bash"' ".stowdeps manifest updated with REQUIRED=\"bash\""

assert_success "$STOW_BIN deps:add terminal fzf --optional" "deps:add terminal fzf --optional succeeded"
assert_file_contains "$MANIFEST_FILE" 'OPTIONAL="fzf"' ".stowdeps manifest updated with OPTIONAL=\"fzf\""

assert_success "$STOW_BIN deps:add terminal zsh --conflict" "deps:add terminal zsh --conflict succeeded"
assert_file_contains "$MANIFEST_FILE" 'CONFLICTS="zsh"' ".stowdeps manifest updated with CONFLICTS=\"zsh\""

# 3. deps:show <pkg>
echo -e "\n${COLOR_BOLD}[Test 3] Display package manifest (deps:show)${COLOR_RESET}"
assert_success "$STOW_BIN deps:show terminal" "stow-manager deps:show terminal succeeded"
assert_file_contains "$LAST_CMD_OUTPUT" "Manifest [.stowdeps] for 'terminal'" "deps:show outputs manifest header"
assert_file_contains "$LAST_CMD_OUTPUT" 'REQUIRED="bash"' "deps:show outputs REQUIRED entries"
assert_file_contains "$LAST_CMD_OUTPUT" 'OPTIONAL="fzf"' "deps:show outputs OPTIONAL entries"
assert_file_contains "$LAST_CMD_OUTPUT" 'CONFLICTS="zsh"' "deps:show outputs CONFLICTS entries"

# 4. deps:remove <pkg> <dep>
echo -e "\n${COLOR_BOLD}[Test 4] Remove dependency (deps:remove)${COLOR_RESET}"
assert_success "$STOW_BIN deps:remove terminal bash" "stow-manager deps:remove terminal bash succeeded"
assert_file_contains "$MANIFEST_FILE" 'REQUIRED=""' ".stowdeps manifest updated and REQUIRED is cleared"
assert_file_not_contains "$MANIFEST_FILE" 'REQUIRED="bash"' ".stowdeps manifest no longer contains bash"
assert_file_contains "$MANIFEST_FILE" 'OPTIONAL="fzf"' ".stowdeps manifest retains OPTIONAL entries"

print_summary
