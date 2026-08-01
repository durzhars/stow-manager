#!/usr/bin/env bash
# tests/feature/test_stow_cmd.sh
# Feature test suite for stow-manager stow, unstow, restow, all, fix-conflicts, dry-run, and flag overrides.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/test_helpers.sh"

echo -e "${COLOR_CYAN}${COLOR_BOLD}=== Feature Tests: Stow Engine Operations ===${COLOR_RESET}\n"

# 1. Single and Multiple Package Operations (stow, restow, unstow)
echo -e "${COLOR_BOLD}[Test 1] Stow, Restow, and Unstow Single & Multiple Packages${COLOR_RESET}"
setup_sandbox
mkdir -p "$STOW_DOTFILES_DIR/pkg1/.config/app1"
mkdir -p "$STOW_DOTFILES_DIR/pkg2/.config/app2"
echo "app1 config" > "$STOW_DOTFILES_DIR/pkg1/.config/app1/app1.conf"
echo "app2 config" > "$STOW_DOTFILES_DIR/pkg2/.config/app2/app2.conf"

assert_success "$STOW_BIN stow pkg1 pkg2" "stow-manager stow pkg1 pkg2 succeeded"
assert_symlink_exists "$HOME/.config/app1/app1.conf" "$STOW_DOTFILES_DIR/pkg1/.config/app1/app1.conf" "Symlink created for pkg1"
assert_symlink_exists "$HOME/.config/app2/app2.conf" "$STOW_DOTFILES_DIR/pkg2/.config/app2/app2.conf" "Symlink created for pkg2"

assert_success "$STOW_BIN restow pkg1 pkg2" "stow-manager restow pkg1 pkg2 succeeded"
assert_symlink_exists "$HOME/.config/app1/app1.conf" "$STOW_DOTFILES_DIR/pkg1/.config/app1/app1.conf" "Symlink maintained after restow for pkg1"
assert_symlink_exists "$HOME/.config/app2/app2.conf" "$STOW_DOTFILES_DIR/pkg2/.config/app2/app2.conf" "Symlink maintained after restow for pkg2"

assert_success "$STOW_BIN unstow pkg1 pkg2" "stow-manager unstow pkg1 pkg2 succeeded"
assert_path_not_exists "$HOME/.config/app1/app1.conf" "Symlink removed for pkg1 after unstow"
assert_path_not_exists "$HOME/.config/app2/app2.conf" "Symlink removed for pkg2 after unstow"

# 2. Stow All Packages ('all')
echo -e "\n${COLOR_BOLD}[Test 2] Stow All Packages ('all')${COLOR_RESET}"
setup_sandbox
mkdir -p "$STOW_DOTFILES_DIR/shell/.config/zsh"
mkdir -p "$STOW_DOTFILES_DIR/editor/.config/nvim"
echo "export FOO=bar" > "$STOW_DOTFILES_DIR/shell/.config/zsh/.zshrc"
echo "set number" > "$STOW_DOTFILES_DIR/editor/.config/nvim/init.vim"

assert_success "$STOW_BIN all" "stow-manager all succeeded"
assert_symlink_exists "$HOME/.config/zsh/.zshrc" "$STOW_DOTFILES_DIR/shell/.config/zsh/.zshrc" "Symlink created for shell package"
assert_symlink_exists "$HOME/.config/nvim/init.vim" "$STOW_DOTFILES_DIR/editor/.config/nvim/init.vim" "Symlink created for editor package"

# 3. Unfold Directory Symlinks ('fix-conflicts')
echo -e "\n${COLOR_BOLD}[Test 3] Unfold Directory Symlinks ('fix-conflicts')${COLOR_RESET}"
setup_sandbox
mkdir -p "$STOW_DOTFILES_DIR/terminal/.config/kitty"
echo "font_size 12" > "$STOW_DOTFILES_DIR/terminal/.config/kitty/kitty.conf"
echo "theme dark" > "$STOW_DOTFILES_DIR/terminal/.config/kitty/theme.conf"

mkdir -p "$HOME/.config"
ln -s "$STOW_DOTFILES_DIR/terminal/.config/kitty" "$HOME/.config/kitty"
assert_symlink_exists "$HOME/.config/kitty" "$STOW_DOTFILES_DIR/terminal/.config/kitty" "Initial directory symlink created in target"

assert_success "$STOW_BIN fix-conflicts" "stow-manager fix-conflicts succeeded"
assert_directory_not_symlink "$HOME/.config/kitty" "Directory symlink unfolded into genuine directory"
assert_symlink_exists "$HOME/.config/kitty/kitty.conf" "$STOW_DOTFILES_DIR/terminal/.config/kitty/kitty.conf" "Individual symlink created for kitty.conf"
assert_symlink_exists "$HOME/.config/kitty/theme.conf" "$STOW_DOTFILES_DIR/terminal/.config/kitty/theme.conf" "Individual symlink created for theme.conf"

# 4. Mutual Exclusion Handling
echo -e "\n${COLOR_BOLD}[Test 4] Mutual Exclusion Handling${COLOR_RESET}"
setup_sandbox
mkdir -p "$STOW_DOTFILES_DIR/pkgA"
mkdir -p "$STOW_DOTFILES_DIR/pkgB"
echo "pkgA config" > "$STOW_DOTFILES_DIR/pkgA/.pkgA_cfg"
echo "pkgB config" > "$STOW_DOTFILES_DIR/pkgB/.pkgB_cfg"

assert_success "$STOW_BIN deps:add pkgB pkgA --conflict" "Registered mutual exclusion conflict: pkgB conflicts with pkgA"

assert_success "$STOW_BIN stow pkgA" "Stowed pkgA initially"
assert_symlink_exists "$HOME/.pkgA_cfg" "$STOW_DOTFILES_DIR/pkgA/.pkgA_cfg" "pkgA symlink active"

assert_success "$STOW_BIN stow pkgB" "Stowing pkgB triggers automatic unstowing of pkgA"
assert_path_not_exists "$HOME/.pkgA_cfg" "pkgA automatically unstowed due to conflict"
assert_symlink_exists "$HOME/.pkgB_cfg" "$STOW_DOTFILES_DIR/pkgB/.pkgB_cfg" "pkgB symlink now active"

# 5. Conflict Backup
echo -e "\n${COLOR_BOLD}[Test 5] Conflict Backup${COLOR_RESET}"
setup_sandbox
mkdir -p "$STOW_DOTFILES_DIR/terminal/.config/kitty"
echo "font_size 12" > "$STOW_DOTFILES_DIR/terminal/.config/kitty/kitty.conf"

mkdir -p "$HOME/.config/kitty"
echo "original local config file" > "$HOME/.config/kitty/kitty.conf"

assert_success "$STOW_BIN stow terminal" "stow-manager stow terminal resolved conflict"
assert_symlink_exists "$HOME/.config/kitty/kitty.conf" "$STOW_DOTFILES_DIR/terminal/.config/kitty/kitty.conf" "Symlink placed over conflicting file"

BACKUP_FILE=$(ls "$HOME/.config/kitty"/kitty.conf.stow_backup_* 2>/dev/null | head -n 1)
if [ -n "$BACKUP_FILE" ] && [ -f "$BACKUP_FILE" ]; then
    assert_file_contains "$BACKUP_FILE" "original local config file" "Original file preserved in .stow_backup_* file"
else
    TESTS_RUN=$((TESTS_RUN + 1))
    TESTS_FAILED=$((TESTS_FAILED + 1))
    echo -e "  ${COLOR_RED}✗${COLOR_RESET} Conflict backup file (.stow_backup_*) was not created"
fi

# 6. Global Dry-Run Mode (-n / --dry-run)
echo -e "\n${COLOR_BOLD}[Test 6] Global Dry-Run Mode (-n / --dry-run)${COLOR_RESET}"
setup_sandbox
mkdir -p "$STOW_DOTFILES_DIR/pkg1"
mkdir -p "$STOW_DOTFILES_DIR/pkg2"
echo "file 1" > "$STOW_DOTFILES_DIR/pkg1/.file1"
echo "file 2" > "$STOW_DOTFILES_DIR/pkg2/.file2"

assert_success "$STOW_BIN -n stow pkg1" "stow dry-run (-n) succeeded"
assert_path_not_exists "$HOME/.file1" "No disk changes made during stow dry-run"

assert_success "$STOW_BIN --dry-run all" "all dry-run (--dry-run) succeeded"
assert_path_not_exists "$HOME/.file1" "No disk changes made during all dry-run"
assert_path_not_exists "$HOME/.file2" "No disk changes made during all dry-run"

assert_success "$STOW_BIN stow pkg1" "Stowed pkg1 for unstow/restow dry-run test"
assert_symlink_exists "$HOME/.file1" "$STOW_DOTFILES_DIR/pkg1/.file1" "pkg1 symlink created"

assert_success "$STOW_BIN -n unstow pkg1" "unstow dry-run (-n) succeeded"
assert_symlink_exists "$HOME/.file1" "$STOW_DOTFILES_DIR/pkg1/.file1" "Symlink remains untouched after unstow dry-run"

assert_success "$STOW_BIN --dry-run restow pkg1" "restow dry-run (--dry-run) succeeded"
assert_symlink_exists "$HOME/.file1" "$STOW_DOTFILES_DIR/pkg1/.file1" "Symlink remains active after restow dry-run"

mkdir -p "$STOW_DOTFILES_DIR/terminal/.config/kitty"
echo "font_size 14" > "$STOW_DOTFILES_DIR/terminal/.config/kitty/kitty.conf"
mkdir -p "$HOME/.config"
ln -s "$STOW_DOTFILES_DIR/terminal/.config/kitty" "$HOME/.config/kitty"

assert_success "$STOW_BIN -n fix-conflicts" "fix-conflicts dry-run (-n) succeeded"
assert_symlink_exists "$HOME/.config/kitty" "$STOW_DOTFILES_DIR/terminal/.config/kitty" "Target directory remains symlink after fix-conflicts dry-run"

# 7. Flag Overrides (-d, --dotfiles-dir & -t, --target-dir)
echo -e "\n${COLOR_BOLD}[Test 7] Flag Overrides (-d, --dotfiles-dir & -t, --target-dir)${COLOR_RESET}"
setup_sandbox
CUSTOM_REPO="$TEST_TMPDIR/custom_repo"
CUSTOM_TARGET="$TEST_TMPDIR/custom_target"
mkdir -p "$CUSTOM_REPO/custom_pkg" "$CUSTOM_TARGET"
echo "custom content" > "$CUSTOM_REPO/custom_pkg/.custom_config"

assert_success "$STOW_BIN -d $CUSTOM_REPO -t $CUSTOM_TARGET stow custom_pkg" "stow with short flags -d and -t succeeded"
assert_symlink_exists "$CUSTOM_TARGET/.custom_config" "$CUSTOM_REPO/custom_pkg/.custom_config" "Symlink created in custom target from custom repo"
assert_path_not_exists "$HOME/.custom_config" "Default HOME untouched when -t override is provided"

assert_success "$STOW_BIN --dotfiles-dir=$CUSTOM_REPO --target-dir=$CUSTOM_TARGET unstow custom_pkg" "unstow with long flags --dotfiles-dir and --target-dir succeeded"
assert_path_not_exists "$CUSTOM_TARGET/.custom_config" "Symlink removed from custom target after unstow"

# 8. Help Menu Flags (-h, --help, help)
echo -e "\n${COLOR_BOLD}[Test 8] Help Menu Flags (-h, --help, help)${COLOR_RESET}"
setup_sandbox
assert_success "$STOW_BIN -h" "stow-manager -h succeeded"
assert_file_contains "$LAST_CMD_OUTPUT" "stow-manager" "-h output contains binary name"

assert_success "$STOW_BIN --help" "stow-manager --help succeeded"
assert_file_contains "$LAST_CMD_OUTPUT" "stow-manager" "--help output contains binary name"

assert_success "$STOW_BIN help" "stow-manager help succeeded"
assert_file_contains "$LAST_CMD_OUTPUT" "stow-manager" "help subcommand output contains binary name"

print_summary
