/*
 * Dotfiles Stow Manager (stow-manager)
 * Copyright (C) 2026 durzhars
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#define _GNU_SOURCE
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "../test_framework.h"
#include "core/stowignore.h"
#include "utils/defs.h"
#include "utils/env.h"
#include "utils/fs.h"
#include "utils/logger.h"
#include "utils/mem.h"
#include "utils/path.h"
#include "utils/signal.h"
#include "utils/str.h"
#include "utils/timer.h"
#include <errno.h>

void test_trim_whitespace(void)
{
    ASSERT(trim_whitespace(NULL) == NULL, "trim_whitespace(NULL) should return NULL");

    char s1[] = "  hello world  ";
    ASSERT_STR_EQ(trim_whitespace(s1), "hello world");

    char s2[] = "\"quoted string\"";
    ASSERT_STR_EQ(trim_whitespace(s2), "quoted string");

    char s2_single[] = "'single quoted'";
    ASSERT_STR_EQ(trim_whitespace(s2_single), "single quoted");

    char s3[] = "\t\n  ";
    ASSERT_STR_EQ(trim_whitespace(s3), "");
}

void test_string_array(void)
{
    StringArray arr;
    str_array_init(&arr);

    ASSERT(arr.count == 0, "Array count should initially be 0");
    str_array_append(&arr, "item1");
    str_array_append(&arr, "item2");

    ASSERT(arr.count == 2, "Array count should be 2");
    ASSERT(str_array_contains(&arr, "item1"), "Array should contain 'item1'");
    ASSERT(str_array_contains(&arr, "item2"), "Array should contain 'item2'");
    ASSERT(!str_array_contains(&arr, "item3"), "Array should not contain 'item3'");

    str_array_free(&arr);
    ASSERT(arr.count == 0, "Array count should be 0 after free");
}

void test_xdg_paths(void)
{
    const char *orig_home = getenv("HOME");
    const char *orig_xdg_config = getenv("XDG_CONFIG_HOME");
    const char *orig_xdg_data = getenv("XDG_DATA_HOME");

    char cfg_home[PATH_MAX];
    char data_home[PATH_MAX];

    // 1. Top precedence: Explicit XDG environment variables
    setenv("XDG_CONFIG_HOME", "/custom/xdg_config", 1);
    setenv("XDG_DATA_HOME", "/custom/xdg_data", 1);
    unsetenv("HOME");

    ASSERT(get_xdg_config_home(cfg_home, sizeof(cfg_home)),
           "get_xdg_config_home should resolve explicit XDG_CONFIG_HOME");
    ASSERT_STR_EQ(cfg_home, "/custom/xdg_config");

    ASSERT(get_xdg_data_home(data_home, sizeof(data_home)),
           "get_xdg_data_home should resolve explicit XDG_DATA_HOME");
    ASSERT_STR_EQ(data_home, "/custom/xdg_data");

    // 2. Second precedence: Fallback to validated $HOME when XDG variables are
    // unset
    unsetenv("XDG_CONFIG_HOME");
    unsetenv("XDG_DATA_HOME");

    char mock_home[PATH_MAX];
    ASSERT(create_test_tmp_dir(mock_home, sizeof(mock_home), "test_xdghome") != NULL,
           "create_test_tmp_dir should create temporary mock HOME directory");
    setenv("HOME", mock_home, 1);

    char expected_cfg[PATH_MAX];
    char expected_data[PATH_MAX];
    snprintf(expected_cfg, sizeof(expected_cfg), "%s/.config", mock_home);
    snprintf(expected_data, sizeof(expected_data), "%s/.local/share", mock_home);

    ASSERT(get_xdg_config_home(cfg_home, sizeof(cfg_home)),
           "get_xdg_config_home should fallback to $HOME/.config when "
           "XDG_CONFIG_HOME is unset");
    ASSERT_STR_EQ(cfg_home, expected_cfg);

    ASSERT(get_xdg_data_home(data_home, sizeof(data_home)),
           "get_xdg_data_home should fallback to $HOME/.local/share when "
           "XDG_DATA_HOME is unset");
    ASSERT_STR_EQ(data_home, expected_data);

    cleanup_test_dir(mock_home);

    // 3. Slow-path: Unset $HOME calls getpwuid_r(getuid())
    unsetenv("HOME");
    ASSERT(get_xdg_config_home(cfg_home, sizeof(cfg_home)),
           "get_xdg_config_home should succeed via getpwuid_r slow path when "
           "HOME is unset");

    // 4. Default Data Dirs
    StringArray data_dirs;
    str_array_init(&data_dirs);
    get_xdg_data_dirs(&data_dirs);
    ASSERT(data_dirs.count > 0, "XDG_DATA_DIRS should yield at least 1 default directory");
    str_array_free(&data_dirs);

    // Restore original environment
    if (orig_home) {
        setenv("HOME", orig_home, 1);
    } else {
        unsetenv("HOME");
    }
    if (orig_xdg_config) {
        setenv("XDG_CONFIG_HOME", orig_xdg_config, 1);
    } else {
        unsetenv("XDG_CONFIG_HOME");
    }
    if (orig_xdg_data) {
        setenv("XDG_DATA_HOME", orig_xdg_data, 1);
    } else {
        unsetenv("XDG_DATA_HOME");
    }
}

void test_safe_allocators(void)
{
    char *ptr = safe_malloc(100);
    ASSERT(ptr != NULL, "safe_malloc should return non-null pointer");
    strcpy(ptr, "testing safe_malloc");
    ASSERT_STR_EQ(ptr, "testing safe_malloc");

    char *dup = safe_strdup(ptr);
    ASSERT(dup != NULL, "safe_strdup should return non-null pointer");
    ASSERT_STR_EQ(dup, "testing safe_malloc");

    free(ptr);
    free(dup);
}

void test_normalize_path(void)
{
    // NULL & Empty string handling
    normalize_path(NULL);

    char p0[PATH_MAX] = "";
    normalize_path(p0);
    ASSERT_STR_EQ(p0, "");

    // Collapsing duplicate slashes
    char p1[PATH_MAX] = "///home//user///.config";
    normalize_path(p1);
    ASSERT_STR_EQ(p1, "/home/user/.config");

    // Stripping trailing slashes on non-root paths
    char p2[PATH_MAX] = "/var/log/";
    normalize_path(p2);
    ASSERT_STR_EQ(p2, "/var/log");

    // Preserving single root slash
    char p3[PATH_MAX] = "/";
    normalize_path(p3);
    ASSERT_STR_EQ(p3, "/");

    // Collapsing multiple root slashes to a single root slash
    char p4[PATH_MAX] = "///";
    normalize_path(p4);
    ASSERT_STR_EQ(p4, "/");
}

void test_collapse_path(void)
{
    // NULL & Empty string handling
    collapse_path(NULL);

    char p0[PATH_MAX] = "";
    collapse_path(p0);
    ASSERT_STR_EQ(p0, "");

    // Resolving .. relative path segments
    char p1[PATH_MAX] = "/a/b/../c";
    collapse_path(p1);
    ASSERT_STR_EQ(p1, "/a/c");

    // Resolving . relative path segments
    char p2[PATH_MAX] = "/a/b/./c";
    collapse_path(p2);
    ASSERT_STR_EQ(p2, "/a/b/c");

    // Multiple relative segments
    char p3[PATH_MAX] = "/a/b/c/../../d";
    collapse_path(p3);
    ASSERT_STR_EQ(p3, "/a/d");

    // Complex sequence with dot and dot-dot
    char p4[PATH_MAX] = "/a/./b/../c";
    collapse_path(p4);
    ASSERT_STR_EQ(p4, "/a/c");

    // Absolute path cannot collapse past root
    char p5[PATH_MAX] = "/a/../../b";
    collapse_path(p5);
    ASSERT_STR_EQ(p5, "/b");

    // Relative path with leading dot-dot
    char p6[PATH_MAX] = "a/b/../../../c";
    collapse_path(p6);
    ASSERT_STR_EQ(p6, "../c");
}

void test_escape_shell_arg(void)
{
    char dest[256];

    // Simple single-word argument wrapping
    escape_shell_arg("foo", dest, sizeof(dest));
    ASSERT_STR_EQ(dest, "'foo'");

    // Escaping embedded single quotes
    escape_shell_arg("it's", dest, sizeof(dest));
    ASSERT_STR_EQ(dest, "'it'\\''s'");

    // Empty string handling
    escape_shell_arg("", dest, sizeof(dest));
    ASSERT_STR_EQ(dest, "''");

    // Buffer boundary limits (Must fail closed to empty string state on overflow)
    char small_buf[5] = "XXXX";
    escape_shell_arg("hello", small_buf, sizeof(small_buf));
    ASSERT(small_buf[0] == '\0',
           "escape_shell_arg must fail closed to empty string on buffer overflow");
}

void test_expand_tilde_path(void)
{
    char out[PATH_MAX];
    const char *orig_home = getenv("HOME");

    char mock_home[PATH_MAX];
    ASSERT(create_test_tmp_dir(mock_home, sizeof(mock_home), "test_tilde") != NULL,
           "create_test_tmp_dir should create temporary mock HOME directory");

    // 1. With a valid HOME directory
    setenv("HOME", mock_home, 1);

    char expected_config[PATH_MAX];
    snprintf(expected_config, sizeof(expected_config), "%s/config", mock_home);

    expand_tilde_path("~/config", out, sizeof(out));
    ASSERT_STR_EQ(out, expected_config);

    expand_tilde_path("~", out, sizeof(out));
    ASSERT_STR_EQ(out, mock_home);

    cleanup_test_dir(mock_home);

    // 2. Non-tilde paths remaining untouched
    expand_tilde_path("/var/log", out, sizeof(out));
    ASSERT_STR_EQ(out, "/var/log");

    expand_tilde_path("relative/path", out, sizeof(out));
    ASSERT_STR_EQ(out, "relative/path");

    // Restore original HOME
    if (orig_home) {
        setenv("HOME", orig_home, 1);
    } else {
        unsetenv("HOME");
    }
}

void test_expand_env_vars(void)
{
    char out[PATH_MAX];
    const char *orig_home = getenv("HOME");

    char mock_home[PATH_MAX];
    ASSERT(create_test_tmp_dir(mock_home, sizeof(mock_home), "test_env") != NULL,
           "create_test_tmp_dir should create temporary mock HOME directory");

    setenv("TEST_STOW_VAR1", "/custom/path", 1);
    setenv("TEST_STOW_VAR2", "my_app", 1);
    setenv("HOME", mock_home, 1);

    // 1. POSIX $VAR syntax
    expand_env_vars("$TEST_STOW_VAR1/sub", out, sizeof(out));
    ASSERT_STR_EQ(out, "/custom/path/sub");

    // 2. POSIX ${VAR} syntax
    expand_env_vars("${TEST_STOW_VAR2}/config", out, sizeof(out));
    ASSERT_STR_EQ(out, "my_app/config");

    // 3. Combination of tilde + env var in expand_tilde_path
    char expected_combo[PATH_MAX];
    snprintf(expected_combo, sizeof(expected_combo), "%s/my_app", mock_home);

    expand_tilde_path("~/$TEST_STOW_VAR2", out, sizeof(out));
    ASSERT_STR_EQ(out, expected_combo);

    // 5. Fail-closed buffer overflow check
    char small_env_buf[5] = "XXXX";
    expand_env_vars("$TEST_STOW_VAR1/sub", small_env_buf, sizeof(small_env_buf));
    ASSERT(small_env_buf[0] == '\0',
           "expand_env_vars must fail closed to empty string on buffer overflow");

    cleanup_test_dir(mock_home);
    unsetenv("TEST_STOW_VAR1");
    unsetenv("TEST_STOW_VAR2");

    if (orig_home) {
        setenv("HOME", orig_home, 1);
    } else {
        unsetenv("HOME");
    }
}

void test_degraded_env_path_resolution(void)
{
    // Save original environment
    const char *orig_home = getenv("HOME");
    const char *orig_xdg_cfg = getenv("XDG_CONFIG_HOME");
    const char *orig_xdg_data = getenv("XDG_DATA_HOME");
    const char *orig_xdg_cache = getenv("XDG_CACHE_HOME");
    const char *orig_xdg_state = getenv("XDG_STATE_HOME");
    const char *orig_xdg_data_dirs = getenv("XDG_DATA_DIRS");
    const char *orig_xdg_config_dirs = getenv("XDG_CONFIG_DIRS");

    char home_backup[PATH_MAX] = {0};
    char xdg_cfg_backup[PATH_MAX] = {0};
    char xdg_data_backup[PATH_MAX] = {0};
    char xdg_cache_backup[PATH_MAX] = {0};
    char xdg_state_backup[PATH_MAX] = {0};
    char xdg_data_dirs_backup[PATH_MAX] = {0};
    char xdg_config_dirs_backup[PATH_MAX] = {0};

    if (orig_home) {
        snprintf(home_backup, sizeof(home_backup), "%s", orig_home);
    }
    if (orig_xdg_cfg) {
        snprintf(xdg_cfg_backup, sizeof(xdg_cfg_backup), "%s", orig_xdg_cfg);
    }
    if (orig_xdg_data) {
        snprintf(xdg_data_backup, sizeof(xdg_data_backup), "%s", orig_xdg_data);
    }
    if (orig_xdg_cache) {
        snprintf(xdg_cache_backup, sizeof(xdg_cache_backup), "%s", orig_xdg_cache);
    }
    if (orig_xdg_state) {
        snprintf(xdg_state_backup, sizeof(xdg_state_backup), "%s", orig_xdg_state);
    }
    if (orig_xdg_data_dirs) {
        snprintf(xdg_data_dirs_backup, sizeof(xdg_data_dirs_backup), "%s", orig_xdg_data_dirs);
    }
    if (orig_xdg_config_dirs) {
        snprintf(
            xdg_config_dirs_backup, sizeof(xdg_config_dirs_backup), "%s", orig_xdg_config_dirs);
    }

    // --- Scenario 1: Invalid / Missing $HOME and Unset XDG Variables ---
    setenv("HOME", "/dev/null", 1);
    unsetenv("XDG_CONFIG_HOME");
    unsetenv("XDG_DATA_HOME");
    unsetenv("XDG_CACHE_HOME");
    unsetenv("XDG_STATE_HOME");
    unsetenv("XDG_DATA_DIRS");
    unsetenv("XDG_CONFIG_DIRS");

    char buf[PATH_MAX];

    // Invalid getenv("HOME") triggers getpwuid_r slow path fallback
    ASSERT(get_user_home_dir(buf, sizeof(buf)),
           "get_user_home_dir should fallback to getpwuid_r slow path when "
           "HOME is invalid");
    ASSERT(get_xdg_config_home(buf, sizeof(buf)),
           "get_xdg_config_home should succeed via getpwuid_r slow path when "
           "HOME is invalid");

    expand_tilde_path("~/dotfiles", buf, sizeof(buf));
    ASSERT(buf[0] == '/',
           "expand_tilde_path should resolve ~ using getpwuid_r slow path when "
           "HOME environment "
           "variable is invalid");

    StringArray dirs;
    str_array_init(&dirs);
    get_xdg_config_dirs(&dirs);
    ASSERT(dirs.count == 1, "Should have 1 default config dir");
    ASSERT_STR_EQ(dirs.items[0], "/etc/xdg");
    str_array_free(&dirs);

    str_array_init(&dirs);
    get_xdg_data_dirs(&dirs);
    ASSERT(dirs.count == 2, "Should have 2 default data dirs");
    ASSERT_STR_EQ(dirs.items[0], "/usr/local/share");
    ASSERT_STR_EQ(dirs.items[1], "/usr/share");
    str_array_free(&dirs);

    // --- Scenario 2: Empty $HOME Variable (Bypasses Fast Path to getpwuid_r
    // Slow Path) ---
    setenv("HOME", "", 1);

    ASSERT(get_user_home_dir(buf, sizeof(buf)),
           "get_user_home_dir should succeed via getpwuid_r slow path when "
           "HOME is empty");
    ASSERT(get_xdg_config_home(buf, sizeof(buf)),
           "get_xdg_config_home should succeed via getpwuid_r slow path when "
           "HOME is empty");

    // --- Scenario 3: Malformed XDG_DATA_DIRS with empty segments & trailing
    // colons ---
    setenv("XDG_DATA_DIRS", ":::/custom/share1::/custom/share2:", 1);

    str_array_init(&dirs);
    get_xdg_data_dirs(&dirs);
    ASSERT(dirs.count == 2, "Should parse only non-empty paths from malformed XDG_DATA_DIRS");
    ASSERT_STR_EQ(dirs.items[0], "/custom/share1");
    ASSERT_STR_EQ(dirs.items[1], "/custom/share2");
    str_array_free(&dirs);

    // --- Scenario 4: Nested environment variable expansion in XDG variables
    // ---
    setenv("CUSTOM_BASE", "/opt/stow", 1);
    setenv("XDG_CONFIG_HOME", "$CUSTOM_BASE/config", 1);

    get_xdg_config_home(buf, sizeof(buf));
    ASSERT_STR_EQ(buf, "/opt/stow/config");

    unsetenv("CUSTOM_BASE");

    // --- Scenario 5: Path Sanity Kernel Checks ---
    ASSERT(verify_path_sanity("") == ERR_PATH_EMPTY, "Empty path should return ERR_PATH_EMPTY");
    ASSERT(verify_path_sanity("relative/path") == ERR_NOT_ABSOLUTE,
           "Relative path should return ERR_NOT_ABSOLUTE");
    ASSERT(verify_path_sanity("/dev/null") == ERR_NOT_A_DIRECTORY,
           "/dev/null file should return ERR_NOT_A_DIRECTORY");

    PathSanityResult tmp_res = verify_path_sanity("/tmp");
    ASSERT(tmp_res == ERR_WORLD_WRITABLE || tmp_res == ERR_NOT_OWNED_BY_USER ||
               tmp_res == ERR_INSUFFICIENT_PERMS || tmp_res == PATH_VALID,
           "/tmp should fail sanity or return valid on systems");

    // --- Scenario 6: AppEnvironment Resolution Pipeline ---
    AppEnvironment app_env;
    setenv("HOME", "/dev/null", 1);
    ASSERT(app_env_resolve(&app_env, NULL, NULL),
           "app_env_resolve should recover valid user home via getpwuid_r when "
           "getenv(HOME) is "
           "invalid");
    ASSERT(app_env.is_home_validated, "is_home_validated should be true via getpwuid_r recovery");

    ASSERT(app_env_resolve(&app_env, "/custom/target", NULL),
           "app_env_resolve should succeed when CLI target override is "
           "provided despite invalid HOME");
    ASSERT(app_env.is_target_override, "is_target_override should be true");
    ASSERT_STR_EQ(app_env.target_dir, "/custom/target");

    // Restore original environment
    if (orig_home) {
        setenv("HOME", home_backup, 1);
    } else {
        unsetenv("HOME");
    }
    if (orig_xdg_cfg) {
        setenv("XDG_CONFIG_HOME", xdg_cfg_backup, 1);
    } else {
        unsetenv("XDG_CONFIG_HOME");
    }
    if (orig_xdg_data) {
        setenv("XDG_DATA_HOME", xdg_data_backup, 1);
    } else {
        unsetenv("XDG_DATA_HOME");
    }
    if (orig_xdg_cache) {
        setenv("XDG_CACHE_HOME", xdg_cache_backup, 1);
    } else {
        unsetenv("XDG_CACHE_HOME");
    }
    if (orig_xdg_state) {
        setenv("XDG_STATE_HOME", xdg_state_backup, 1);
    } else {
        unsetenv("XDG_STATE_HOME");
    }
    if (orig_xdg_data_dirs) {
        setenv("XDG_DATA_DIRS", xdg_data_dirs_backup, 1);
    } else {
        unsetenv("XDG_DATA_DIRS");
    }
    if (orig_xdg_config_dirs) {
        setenv("XDG_CONFIG_DIRS", xdg_config_dirs_backup, 1);
    } else {
        unsetenv("XDG_CONFIG_DIRS");
    }
}

void test_is_path_prefix(void)
{
    // True prefix matches
    ASSERT(is_path_prefix("/home/user/dotfiles/bash", "/home/user/dotfiles"),
           "/home/user/dotfiles should be a prefix of /home/user/dotfiles/bash");

    ASSERT(is_path_prefix("/home/user/dotfiles", "/home/user/dotfiles"),
           "Identical path should be a valid prefix of itself");

    // False positive boundaries
    ASSERT(!is_path_prefix("/home/user/dotfiles-other", "/home/user/dotfiles"),
           "/home/user/dotfiles should NOT be a prefix of "
           "/home/user/dotfiles-other");

    ASSERT(!is_path_prefix("/etc/passwd", "/home/user"),
           "/home/user should NOT be a prefix of /etc/passwd");
}

void test_mkdir_p(void)
{
    char tmp_dir[PATH_MAX];
    ASSERT(create_test_tmp_dir(tmp_dir, sizeof(tmp_dir), "mkdir") != NULL,
           "Should create temporary directory for mkdir_p test");

    // Recursively create deeply nested directory
    char deep_path[PATH_MAX];
    snprintf(deep_path, sizeof(deep_path), "%s/a/b/c/d", tmp_dir);
    int res = mkdir_p(deep_path, 0755);
    ASSERT(res == 0, "mkdir_p should return 0 on success");
    ASSERT(is_dir(deep_path), "Deeply nested directory should exist");

    // Handle existing directory paths gracefully without returning an error
    int res_existing = mkdir_p(deep_path, 0755);
    ASSERT(res_existing == 0, "mkdir_p on existing directory should return 0");

    cleanup_test_dir(tmp_dir);
}

void test_join_path(void)
{
    char out[PATH_MAX];

    // Standard successful joining
    int r1 = join_path(out, sizeof(out), "/home/user/dotfiles", "hyprland");
    ASSERT(r1 == 0, "join_path should return 0 on success");
    ASSERT_STR_EQ(out, "/home/user/dotfiles/hyprland");

    int r2 = join_path(out, sizeof(out), "/home/user/dotfiles/", "hyprland");
    ASSERT(r2 == 0, "join_path should return 0 on success");
    ASSERT_STR_EQ(out, "/home/user/dotfiles/hyprland");

    int r3 = join_path(out, sizeof(out), "", "hyprland");
    ASSERT(r3 == 0, "join_path should return 0 when dir is empty");
    ASSERT_STR_EQ(out, "hyprland");

    int r4 = join_path(out, sizeof(out), "/home/user", "");
    ASSERT(r4 == 0, "join_path should return 0 when rel is empty");
    ASSERT_STR_EQ(out, "/home/user");

    // NULL parameter defense
    ASSERT(join_path(NULL, 100, "dir", "rel") == -1, "join_path(NULL, ...) should return -1");
    ASSERT(errno == EINVAL, "join_path(NULL, ...) should set errno to EINVAL");

    ASSERT(join_path(out, 0, "dir", "rel") == -1, "join_path(..., out_size=0) should return -1");
    ASSERT(errno == EINVAL, "join_path(..., out_size=0) should set errno to EINVAL");

    // Defensive Fail-Closed on Buffer Overflow (NO silent truncation allowed!)
    char small_buf[10];
    small_buf[0] = 'X';
    errno = 0;
    int err_res =
        join_path(small_buf, sizeof(small_buf), "/home/user/very_long_directory_name", "subfile");
    ASSERT(err_res == -1, "join_path must fail closed (-1) when buffer is too small");
    ASSERT(errno == ENAMETOOLONG, "join_path must set errno to ENAMETOOLONG on buffer overflow");
    ASSERT(small_buf[0] == '\0',
           "join_path must fail closed by zeroing output buffer state on error");

    // Overlapping buffer safety (out == dir)
    char overlap_buf[PATH_MAX] = "/home/user";
    int r_overlap1 = join_path(overlap_buf, sizeof(overlap_buf), overlap_buf, ".config");
    ASSERT(r_overlap1 == 0, "join_path should handle out == dir overlapping buffer safely");
    ASSERT_STR_EQ(overlap_buf, "/home/user/.config");

    // Overlapping buffer safety (out == rel)
    char overlap_buf2[PATH_MAX] = "nvim";
    int r_overlap2 =
        join_path(overlap_buf2, sizeof(overlap_buf2), "/home/user/.config", overlap_buf2);
    ASSERT(r_overlap2 == 0, "join_path should handle out == rel overlapping buffer safely");
    ASSERT_STR_EQ(overlap_buf2, "/home/user/.config/nvim");
}

void test_symlink_helpers(void)
{
    char tmp_dir[PATH_MAX];
    ASSERT(create_test_tmp_dir(tmp_dir, sizeof(tmp_dir), "sym_hlp") != NULL,
           "Should create temporary directory for symlink helpers test");

    char target_file[PATH_MAX];
    snprintf(target_file, sizeof(target_file), "%s/target.txt", tmp_dir);
    FILE *fp = fopen(target_file, "w");
    if (fp) {
        fprintf(fp, "test\n");
        fclose(fp);
    }

    ASSERT(file_exists(target_file), "target_file should exist");
    ASSERT(!is_symlink(target_file), "target_file should not be a symlink");
    ASSERT(!is_dir(target_file), "target_file should not be a directory");

    char link_file[PATH_MAX];
    snprintf(link_file, sizeof(link_file), "%s/link.txt", tmp_dir);
    ASSERT(symlink(target_file, link_file) == 0, "Should create symlink");

    ASSERT(is_symlink(link_file), "link_file should be a symlink");
    char *sym_target = read_symlink_target(link_file);
    ASSERT(sym_target != NULL, "read_symlink_target should return target path");
    ASSERT_STR_EQ(sym_target, target_file);
    free(sym_target);

    // Fail-Closed & NULL Resilience
    ASSERT(read_symlink_target(NULL) == NULL, "read_symlink_target(NULL) must return NULL");
    ASSERT(read_symlink_target("") == NULL, "read_symlink_target(\"\") must return NULL");
    ASSERT(read_symlink_target("/nonexistent_file_xyz_9999") == NULL,
           "read_symlink_target on non-symlink must return NULL");

    cleanup_test_dir(tmp_dir);
}

void test_is_executable_in_path(void)
{
    // Test relative name search (standard behavior)
    ASSERT(is_executable_in_path("sh") || is_executable_in_path("bash"),
           "Should find standard shell executable in PATH");

    // Test non-existent executable
    ASSERT(!is_executable_in_path("non_existent_executable_12345"),
           "Should return false for non-existent executable");

    // Test absolute path to executable across platforms (/bin/sh or /usr/bin/sh)
    int found_abs_sh = is_executable_in_path("/bin/sh") || is_executable_in_path("/usr/bin/sh");
    ASSERT(found_abs_sh, "Should detect absolute path to sh as executable");
}



void test_default_stowignore(void)
{
    StringArray defaults;
    str_array_init(&defaults);
    get_default_stowignore(&defaults);

    ASSERT(defaults.count > 0, "Default stowignore patterns should be loaded");
    ASSERT(str_array_contains(&defaults, ".gitignore"), "Should contain .gitignore pattern");
    ASSERT(str_array_contains(&defaults, ".git"), "Should contain .git pattern");

    ASSERT(is_path_ignored(".config/nvim_lazyvim_backup/.gitignore", &defaults),
           "Subdirectory .gitignore must be ignored by default patterns");
    ASSERT(is_path_ignored("README.md", &defaults),
           "README.md must be ignored by default patterns");
    ASSERT(is_path_ignored("nested/README.md", &defaults),
           "nested/README.md must be ignored by default patterns");
    ASSERT(!is_path_ignored(".config/nvim/init.lua", &defaults),
           "Normal config file must not be ignored");

    str_array_free(&defaults);
}

void test_is_symlink_pointing_to(void)
{
    char tmp_dir[PATH_MAX];
    ASSERT(create_test_tmp_dir(tmp_dir, sizeof(tmp_dir), "sym_pt") != NULL,
           "create_test_tmp_dir should succeed");

    char real_target_file[PATH_MAX];
    snprintf(real_target_file, sizeof(real_target_file), "%s/theme.conf", tmp_dir);
    FILE *fp = fopen(real_target_file, "w");
    if (fp) {
        fprintf(fp, "color=blue\n");
        fclose(fp);
    }

    char pkg_symlink_file[PATH_MAX];
    snprintf(pkg_symlink_file, sizeof(pkg_symlink_file), "%s/current-theme.conf", tmp_dir);
    symlink("theme.conf", pkg_symlink_file);

    char outer_dir[PATH_MAX];
    snprintf(outer_dir, sizeof(outer_dir), "%s/outer", tmp_dir);
    mkdir(outer_dir, 0755);

    char outer_stow_link[PATH_MAX];
    snprintf(outer_stow_link, sizeof(outer_stow_link), "%s/current-theme.conf", outer_dir);
    symlink("../current-theme.conf", outer_stow_link);

    ASSERT(is_symlink_pointing_to(outer_stow_link, pkg_symlink_file, NULL),
           "Relative symlink to internal package symlink file must match");

    cleanup_test_dir(tmp_dir);
}

void test_mkdir_p_file_collision(void)
{
    char tmp_dir[PATH_MAX];
    ASSERT(create_test_tmp_dir(tmp_dir, sizeof(tmp_dir), "mkdir_col") != NULL,
           "Should create temporary test directory");

    // Create a regular file blocking directory creation
    char file_path[PATH_MAX];
    snprintf(file_path, sizeof(file_path), "%s/blocking_file", tmp_dir);
    FILE *fp = fopen(file_path, "w");
    if (fp) {
        fprintf(fp, "I am a file, not a directory\n");
        fclose(fp);
    }

    // Attempting mkdir_p where an intermediate element is a regular file must
    // fail
    char invalid_dir[PATH_MAX];
    snprintf(invalid_dir, sizeof(invalid_dir), "%s/blocking_file/sub_dir", tmp_dir);
    int res = mkdir_p(invalid_dir, 0755);
    ASSERT(res != 0, "mkdir_p should fail when intermediate component is a regular file");

    cleanup_test_dir(tmp_dir);
}

typedef struct {
    int file_count;
} WalkTestContext;

static void count_files_cb(const char *file_path, const char *rel_path, void *user_data)
{
    (void)file_path;
    (void)rel_path;
    WalkTestContext *ctx = (WalkTestContext *)user_data;
    ctx->file_count++;
}

void test_walk_dir_files_and_cleanup(void)
{
    char tmp_dir[PATH_MAX];
    ASSERT(create_test_tmp_dir(tmp_dir, sizeof(tmp_dir), "walk_t") != NULL,
           "Should create temporary directory for walk test");

    char sub_dir[PATH_MAX];
    snprintf(sub_dir, sizeof(sub_dir), "%s/subdir", tmp_dir);
    mkdir(sub_dir, 0755);

    char f1[PATH_MAX];
    char f2[PATH_MAX];
    snprintf(f1, sizeof(f1), "%s/file1.txt", tmp_dir);
    snprintf(f2, sizeof(f2), "%s/file2.txt", sub_dir);

    FILE *fp1 = fopen(f1, "w");
    if (fp1) {
        fprintf(fp1, "f1\n");
        fclose(fp1);
    }
    FILE *fp2 = fopen(f2, "w");
    if (fp2) {
        fprintf(fp2, "f2\n");
        fclose(fp2);
    }

    WalkTestContext ctx = {0};
    walk_dir_files(tmp_dir, NULL, count_files_cb, &ctx);
    ASSERT(ctx.file_count == 2, "walk_dir_files should count 2 regular files");

    cleanup_temp_dir_contents(tmp_dir);
    ASSERT(!file_exists(f1), "f1 should be deleted after cleanup_temp_dir_contents");
    ASSERT(!file_exists(f2), "f2 should be deleted after cleanup_temp_dir_contents");
    ASSERT(!is_dir(sub_dir), "subdir should be deleted after cleanup_temp_dir_contents");

    cleanup_test_dir(tmp_dir);
}

void test_path_sanity_strerror(void)
{
    const char *p = "/fake/path";
    char expected_uid_msg[256];

    // Pre-format the expected message that relies on the system's active UID
    snprintf(expected_uid_msg,
             sizeof(expected_uid_msg),
             "directory owner UID does not match running UID %u",
             getuid());

    ASSERT_STR_EQ(path_sanity_strerror(PATH_VALID, p), "path '/fake/path' is valid");

    ASSERT_STR_EQ(path_sanity_strerror(ERR_PATH_EMPTY, NULL), "path string is empty or NULL");

    ASSERT_STR_EQ(path_sanity_strerror(ERR_NOT_ABSOLUTE, "dotfiles"),
                  "path 'dotfiles' is not absolute (must start with '/')");

    ASSERT_STR_EQ(path_sanity_strerror(ERR_NOT_A_DIRECTORY, "/dev/null"),
                  "'/dev/null' is not a directory");

    ASSERT_STR_EQ(path_sanity_strerror(ERR_NOT_OWNED_BY_USER, p), expected_uid_msg);

    ASSERT_STR_EQ(path_sanity_strerror(ERR_WORLD_WRITABLE, p),
                  "'/fake/path' is world-writable (security violation)");

    ASSERT_STR_EQ(path_sanity_strerror(ERR_INSUFFICIENT_PERMS, p),
                  "insufficient permissions for '/fake/path' (rwx access required)");

    ASSERT_STR_EQ(path_sanity_strerror((PathSanityResult)999, p),
                  "unknown path sanity error for '/fake/path'");
}

void test_temp_path_registration(void)
{
    char tmp_file[PATH_MAX];
    snprintf(tmp_file, sizeof(tmp_file), "/tmp/stow_mgr_test_sig_%d", getpid());

    FILE *fp = fopen(tmp_file, "w");
    if (fp) {
        fprintf(fp, "temp\n");
        fclose(fp);
    }

    register_temp_path(tmp_file);
    unregister_temp_path(tmp_file);

    register_temp_path(tmp_file);
    cleanup_temp_paths();
    ASSERT(!file_exists(tmp_file), "cleanup_temp_paths should remove registered file");
}

void test_path_parsing_traversal_and_naming_edge_cases(void)
{
    char p[PATH_MAX];

    // --- Category 1: Traversal Vectors & Relative Path Parsing ---
    // Single dot and double dot traversal sequences
    snprintf(p, sizeof(p), "/a/b/..");
    collapse_path(p);
    ASSERT_STR_EQ(p, "/a");

    snprintf(p, sizeof(p), "a/b/..");
    collapse_path(p);
    ASSERT_STR_EQ(p, "a");

    snprintf(p, sizeof(p), "a/b/c/../..");
    collapse_path(p);
    ASSERT_STR_EQ(p, "a");

    snprintf(p, sizeof(p), "a/..");
    collapse_path(p);
    ASSERT_STR_EQ(p, ".");

    snprintf(p, sizeof(p), "/a/..");
    collapse_path(p);
    ASSERT_STR_EQ(p, "/");

    snprintf(p, sizeof(p), "a/b/../../c");
    collapse_path(p);
    ASSERT_STR_EQ(p, "c");

    snprintf(p, sizeof(p), "a/b/../../../c");
    collapse_path(p);
    ASSERT_STR_EQ(p, "../c");

    snprintf(p, sizeof(p), "a/b/../../../../c");
    collapse_path(p);
    ASSERT_STR_EQ(p, "../../c");

    snprintf(p, sizeof(p), "./a/./b/.././c");
    collapse_path(p);
    ASSERT_STR_EQ(p, "a/c");

    // Traversal vectors on absolute paths (cannot escape root /)
    snprintf(p, sizeof(p), "/a/b/../../..");
    collapse_path(p);
    ASSERT_STR_EQ(p, "/");

    snprintf(p, sizeof(p), "/a/../../../../etc/passwd");
    collapse_path(p);
    ASSERT_STR_EQ(p, "/etc/passwd");

    snprintf(p, sizeof(p), "/./a/../b/../c");
    collapse_path(p);
    ASSERT_STR_EQ(p, "/c");

    // --- Category 2: Multi-Dot Directory Naming & Directory Edge Cases ---
    // Multi-dot names like "..." or "...." are valid directory names in POSIX, NOT traversal
    // operators
    snprintf(p, sizeof(p), "a/.../b");
    collapse_path(p);
    ASSERT_STR_EQ(p, "a/.../b");

    snprintf(p, sizeof(p), "/a/..../b/../c");
    collapse_path(p);
    ASSERT_STR_EQ(p, "/a/..../c");

    // Directory names with spaces and special characters
    snprintf(p, sizeof(p), "/home/user/My Documents/./.config/../.local/share");
    collapse_path(p);
    ASSERT_STR_EQ(p, "/home/user/My Documents/.local/share");

    // Directory names starting with dot
    snprintf(p, sizeof(p), ".config/.stowignore");
    collapse_path(p);
    ASSERT_STR_EQ(p, ".config/.stowignore");

    // --- Category 3: Absolute Path Checking & Slash Normalization ---
    // Multiple slashes
    snprintf(p, sizeof(p), "////usr///local////bin///");
    normalize_path(p);
    ASSERT_STR_EQ(p, "/usr/local/bin");

    snprintf(p, sizeof(p), "//");
    normalize_path(p);
    ASSERT_STR_EQ(p, "/");

    // --- Category 4: Path Prefix Match Edge Cases ---
    // False positive prefix matches (same prefix substring, different component name)
    ASSERT(!is_path_prefix("/home/user/dotfiles_backup", "/home/user/dotfiles"),
           "Prefix must match directory component boundary, not partial string prefix");
    ASSERT(!is_path_prefix("/home/user/dotfiles-2/file", "/home/user/dotfiles"),
           "Prefix check must not match 'dotfiles-2' for prefix 'dotfiles'");
    ASSERT(is_path_prefix("/home/user/dotfiles/pkg/file", "/home/user/dotfiles"),
           "Prefix check should match valid subdirectory path");
    ASSERT(is_path_prefix("/home/user/dotfiles", "/home/user/dotfiles"),
           "Prefix check should match exact path");
    ASSERT(is_path_prefix("/anything", "/"), "Root '/' prefix should match any absolute path");

    // --- Category 5: join_path & Path Assembly Edge Cases ---
    char joined[PATH_MAX];
    join_path(joined, sizeof(joined), "/home/user", "../.config");
    ASSERT_STR_EQ(joined, "/home/user/../.config");

    collapse_path(joined);
    ASSERT_STR_EQ(joined, "/home/.config");

    join_path(joined, sizeof(joined), "dotfiles", "./hypr/./hyprland.conf");
    ASSERT_STR_EQ(joined, "dotfiles/./hypr/./hyprland.conf");
    collapse_path(joined);
    ASSERT_STR_EQ(joined, "dotfiles/hypr/hyprland.conf");

    // --- Category 6: Fail-Closed Buffer Bounds & Path Depth Limits ---
    // Exceeding MAX_PATH_DEPTH components (256) in collapse_path
    char deep[PATH_MAX * 2];
    size_t pos = 0;
    for (int i = 0; i < 260; i++) {
        int w = snprintf(deep + pos, sizeof(deep) - pos, "/dir%d", i);
        if (w > 0)
            pos += (size_t)w;
    }
    int res = collapse_path(deep);
    ASSERT(res == -1,
           "collapse_path should return -1 when path component depth exceeds MAX_PATH_DEPTH");
    ASSERT(errno == ENAMETOOLONG, "collapse_path must set errno to ENAMETOOLONG");
    ASSERT(deep[0] == '\0', "collapse_path must fail closed by zeroing output string state");

    // Fail-Closed join_path with small output buffers (Must return -1, set ENAMETOOLONG, and wipe
    // output buffer)
    char tiny[4] = "abc";
    errno = 0;
    int res_tiny = join_path(tiny, sizeof(tiny), "/usr/local", "bin");
    ASSERT(res_tiny == -1, "join_path must fail closed (-1) when buffer is too small");
    ASSERT(errno == ENAMETOOLONG, "join_path must set errno to ENAMETOOLONG");
    ASSERT(tiny[0] == '\0', "join_path must fail closed by setting output[0] to '\\0'");

    char single[1] = {'x'};
    errno = 0;
    int res_single = join_path(single, sizeof(single), "/path", "file");
    ASSERT(res_single == -1, "join_path with 1-byte buffer must return -1 on overflow");
    ASSERT(errno == ENAMETOOLONG, "join_path must set errno to ENAMETOOLONG");
    ASSERT(single[0] == '\0', "1-byte buffer must fail closed to empty string");
}

void test_str_set(void)
{
    StrSet set;
    str_set_init(&set);

    ASSERT(!str_set_contains(&set, "apple"), "Empty set should not contain apple");

    ASSERT(str_set_add(&set, "apple") == true, "First add of apple should succeed");
    ASSERT(str_set_contains(&set, "apple") == true, "Set should contain apple");
    ASSERT(str_set_add(&set, "apple") == false, "Duplicate add of apple should return false");

    ASSERT(str_set_add(&set, "banana") == true, "First add of banana should succeed");
    ASSERT(str_set_add(&set, "cherry") == true, "First add of cherry should succeed");

    ASSERT(str_set_contains(&set, "apple"), "Set should contain apple");
    ASSERT(str_set_contains(&set, "banana"), "Set should contain banana");
    ASSERT(str_set_contains(&set, "cherry"), "Set should contain cherry");
    ASSERT(!str_set_contains(&set, "date"), "Set should not contain date");

    str_set_free(&set);
    ASSERT(set.keys == NULL && set.count == 0, "Set should be cleaned up");
}

void test_perf_timer(void)
{
    perf_profiler_set_enabled(true);
    ASSERT(perf_profiler_is_enabled() == true, "Profiler should be enabled");

    PerfTimer timer = perf_timer_start("test_operation");
    ASSERT(timer.running == true, "Timer should be running");

    struct timespec req = {0, 5000000L}; // 5 ms
    nanosleep(&req, NULL);

    double elapsed_ms = perf_timer_stop(&timer);
    ASSERT(timer.running == false, "Timer should be stopped");
    ASSERT(elapsed_ms >= 1.0, "Elapsed ms should be >= 1.0 ms after 5 ms sleep");

    double elapsed_us = perf_timer_elapsed_us(&timer);
    ASSERT(elapsed_us >= 1000.0, "Elapsed us should be >= 1000 us");

    perf_timer_log(&timer);
    perf_profiler_set_enabled(false);
}
