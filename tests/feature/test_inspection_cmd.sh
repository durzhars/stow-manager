#!/usr/bin/env bash
# tests/feature/test_inspection_cmd.sh
# Feature test suite for stow-manager inspection and diagnostics commands (list, diff, check, check-symlinks, scan).

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/test_helpers.sh"

echo -e "${COLOR_CYAN}${COLOR_BOLD}=== Feature Tests: Inspection & Diagnostics ===${COLOR_RESET}\n"

# 1. Package Listing ('list')
echo -e "${COLOR_BOLD}[Test 1] List Packages Status ('list')${COLOR_RESET}"
setup_sandbox
mkdir -p "$STOW_DOTFILES_DIR/pkg_stowed"
mkdir -p "$STOW_DOTFILES_DIR/pkg_unstowed"
echo "stowed content" > "$STOW_DOTFILES_DIR/pkg_stowed/.stowed_file"
echo "unstowed content" > "$STOW_DOTFILES_DIR/pkg_unstowed/.unstowed_file"

assert_success "$STOW_BIN stow pkg_stowed" "Stowed pkg_stowed package"

assert_success "$STOW_BIN list" "stow-manager list succeeded"
assert_file_contains "$LAST_CMD_OUTPUT" "[STOWED]" "list output contains [STOWED] status"
assert_file_contains "$LAST_CMD_OUTPUT" "pkg_stowed" "list output contains pkg_stowed package name"
assert_file_contains "$LAST_CMD_OUTPUT" "[UNSTOWED]" "list output contains [UNSTOWED] status"
assert_file_contains "$LAST_CMD_OUTPUT" "pkg_unstowed" "list output contains pkg_unstowed package name"

# 2. Diff Preview ('diff [pkg]')
echo -e "\n${COLOR_BOLD}[Test 2] Diff Preview ('diff [pkg]')${COLOR_RESET}"
setup_sandbox
mkdir -p "$STOW_DOTFILES_DIR/diff_pkg"
echo "preview content" > "$STOW_DOTFILES_DIR/diff_pkg/.diff_file"

assert_success "$STOW_BIN diff diff_pkg" "stow-manager diff diff_pkg succeeded"
assert_file_contains "$LAST_CMD_OUTPUT" "[DRY-RUN]" "diff output indicates dry-run preview mode"
assert_path_not_exists "$HOME/.diff_file" "diff preview does not touch or modify disk"

# 3. Dependency Checker ('check [pkg]')
echo -e "\n${COLOR_BOLD}[Test 3] Dependency Checker ('check [pkg]')${COLOR_RESET}"
setup_sandbox
mkdir -p "$STOW_DOTFILES_DIR/pkg_missing"
MANIFEST_FILE="$STOW_DOTFILES_DIR/pkg_missing/.stowdeps"
echo 'REQUIRED="nonexistent_tool_xyz123"' > "$MANIFEST_FILE"

assert_success "$STOW_BIN -n check pkg_missing" "stow-manager -n check pkg_missing succeeded"
assert_file_contains "$LAST_CMD_OUTPUT" "nonexistent_tool_xyz123" "check output identifies missing tool"
assert_file_contains "$LAST_CMD_OUTPUT" "REQUIRED MISSING" "check output flags missing tool as REQUIRED MISSING"
assert_file_contains "$LAST_CMD_OUTPUT" "Installation Command" "check output provides installation command recommendation"

# 4. Symlink Integrity & Health ('check-symlinks')
echo -e "\n${COLOR_BOLD}[Test 4] Symlink Integrity & Health ('check-symlinks')${COLOR_RESET}"
setup_sandbox
mkdir -p "$STOW_DOTFILES_DIR/broken_pkg"
ln -s "/nonexistent_target_xyz999" "$STOW_DOTFILES_DIR/broken_pkg/broken_link"

mkdir -p "$STOW_DOTFILES_DIR/orphan_pkg"
ln -s "$STOW_DOTFILES_DIR/orphan_pkg/deleted_file" "$HOME/orphan_link"

assert_success "$STOW_BIN check-symlinks" "stow-manager check-symlinks succeeded"
assert_file_contains "$LAST_CMD_OUTPUT" "Broken symlink inside repo" "check-symlinks detects broken symlink inside dotfiles repo"
assert_file_contains "$LAST_CMD_OUTPUT" "Unmanaged / Orphan symlink" "check-symlinks detects orphan symlink in target directory"

# 5. Dependency Scanner ('scan [pkg]')
echo -e "\n${COLOR_BOLD}[Test 5] Dependency Scanner ('scan [pkg]')${COLOR_RESET}"
setup_sandbox
mkdir -p "$STOW_DOTFILES_DIR/fzf"
mkdir -p "$STOW_DOTFILES_DIR/scanner_pkg"

SCRIPT_PATH="$STOW_DOTFILES_DIR/scanner_pkg/script.sh"
cat << 'EOF' > "$SCRIPT_PATH"
#!/usr/bin/env bash
fzf --height 40%
EOF
chmod +x "$SCRIPT_PATH"

assert_success "$STOW_BIN scan scanner_pkg" "stow-manager scan scanner_pkg succeeded"
MANIFEST_FILE="$STOW_DOTFILES_DIR/scanner_pkg/.stowdeps"
assert_path_exists "$MANIFEST_FILE" "Auto-generated .stowdeps manifest created"
assert_file_contains "$MANIFEST_FILE" 'REQUIRED="bash"' ".stowdeps contains REQUIRED=\"bash\" from shebang"
assert_file_contains "$MANIFEST_FILE" 'OPTIONAL="fzf"' ".stowdeps contains OPTIONAL=\"fzf\" from tool invocation"

print_summary
