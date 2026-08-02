#!/usr/bin/env bash
# tests/feature/test_ignore_cmd.sh
# Feature test suite for file filtering & .stowignore operations

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/test_helpers.sh"

echo -e "${COLOR_CYAN}${COLOR_BOLD}=== Feature Tests: Ignore Rules & File Filtering ===${COLOR_RESET}\n"

setup_sandbox

# [Test 1] Ignore Init (repo root & per-package)
echo -e "${COLOR_BOLD}[Test 1] Ignore Init ('ignore init')${COLOR_RESET}"
mkdir -p "$MOCK_DOTFILES/ignpkg"

assert_success "$STOW_BIN ignore init" "Global .stowignore initialization"
assert_path_exists "$MOCK_DOTFILES/.stowignore" "Global .stowignore file created"
assert_file_contains "$MOCK_DOTFILES/.stowignore" "Global .stowignore for dotfiles repository" "Global header written"

assert_success "$STOW_BIN ignore init ignpkg" "Package .stowignore initialization"
assert_path_exists "$MOCK_DOTFILES/ignpkg/.stowignore" "Package .stowignore file created"
assert_file_contains "$MOCK_DOTFILES/ignpkg/.stowignore" "for package 'ignpkg'" "Package header written"

# [Test 2] Ignore Add (package & global -g flag)
echo -e "\n${COLOR_BOLD}[Test 2] Ignore Add ('ignore add')${COLOR_RESET}"
assert_success "$STOW_BIN ignore add ignpkg '*.log' 'temp_cache/'" "Add patterns to package .stowignore"
assert_file_contains "$MOCK_DOTFILES/ignpkg/.stowignore" "*.log" "Package .stowignore contains *.log"
assert_file_contains "$MOCK_DOTFILES/ignpkg/.stowignore" "temp_cache/" "Package .stowignore contains temp_cache/"

assert_success "$STOW_BIN ignore add -g '*.bak'" "Add pattern to global .stowignore using -g"
assert_file_contains "$MOCK_DOTFILES/.stowignore" "*.bak" "Global .stowignore contains *.bak"

# Duplicate addition prevention
assert_success "$STOW_BIN ignore add ignpkg '*.log'" "Re-add duplicate pattern *.log"
dup_count=$(grep -c '^\*\.log$' "$MOCK_DOTFILES/ignpkg/.stowignore" || true)
if [ "$dup_count" -eq 1 ]; then
    echo -e "  ${COLOR_GREEN}✓${COLOR_RESET} Duplicate pattern *.log was not added twice"
    TESTS_PASSED=$((TESTS_PASSED + 1))
else
    echo -e "  ${COLOR_RED}✗${COLOR_RESET} Duplicate pattern *.log was added $dup_count times"
    TESTS_FAILED=$((TESTS_FAILED + 1))
fi
TESTS_RUN=$((TESTS_RUN + 1))

# [Test 3] Ignore Show ('ignore show')
echo -e "\n${COLOR_BOLD}[Test 3] Ignore Show ('ignore show')${COLOR_RESET}"
assert_success "$STOW_BIN ignore show ignpkg" "ignore show ignpkg succeeded"
assert_file_contains "$LAST_CMD_OUTPUT" "*.log" "ignore show displays *.log pattern"

assert_success "$STOW_BIN ignore show" "ignore show global succeeded"
assert_file_contains "$LAST_CMD_OUTPUT" "*.bak" "ignore show global displays *.bak pattern"

# [Test 4] Ignore Remove ('ignore remove')
echo -e "\n${COLOR_BOLD}[Test 4] Ignore Remove ('ignore remove')${COLOR_RESET}"
assert_success "$STOW_BIN ignore remove ignpkg '*.log'" "Remove *.log pattern from ignpkg .stowignore"
assert_file_not_contains "$MOCK_DOTFILES/ignpkg/.stowignore" "*.log" "Package .stowignore no longer contains *.log"
assert_file_contains "$MOCK_DOTFILES/ignpkg/.stowignore" "temp_cache/" "Package .stowignore still contains temp_cache/"

# [Test 5] Ignore Clear ('ignore clear')
echo -e "\n${COLOR_BOLD}[Test 5] Ignore Clear ('ignore clear')${COLOR_RESET}"
assert_success "$STOW_BIN ignore clear ignpkg" "Clear package .stowignore"
assert_path_not_exists "$MOCK_DOTFILES/ignpkg/.stowignore" "Package .stowignore file deleted"

assert_success "$STOW_BIN ignore clear" "Clear global .stowignore"
assert_path_not_exists "$MOCK_DOTFILES/.stowignore" "Global .stowignore file deleted"

# [Test 6] End-to-End Stow Integration with Ignore Rules
echo -e "\n${COLOR_BOLD}[Test 6] End-to-End Stow Integration with Ignore Rules${COLOR_RESET}"
mkdir -p "$MOCK_DOTFILES/apppkg"
echo "config_val=1" > "$MOCK_DOTFILES/apppkg/app.conf"
echo "secret_key=xyz" > "$MOCK_DOTFILES/apppkg/secret.key"

assert_success "$STOW_BIN ignore add apppkg 'secret.key'" "Ignore secret.key pattern for apppkg"
assert_success "$STOW_BIN stow apppkg" "Stow apppkg package"
assert_symlink_exists "$MOCK_HOME/app.conf" "$MOCK_DOTFILES/apppkg/app.conf" "app.conf is stowed as symlink"
assert_path_not_exists "$MOCK_HOME/secret.key" "secret.key is ignored and not stowed"

print_summary
