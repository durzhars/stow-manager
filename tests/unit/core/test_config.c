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
#include "core/config.h"

void test_config_system(void)
{
    char tmp_dir[PATH_MAX];
    ASSERT(create_test_tmp_dir(tmp_dir, sizeof(tmp_dir), "cfg_test") != NULL,
           "Should create temporary test directory");

    const char *old_xdg = getenv("XDG_CONFIG_HOME");
    char old_xdg_buf[PATH_MAX] = {0};
    if (old_xdg) {
        snprintf(old_xdg_buf, sizeof(old_xdg_buf), "%s", old_xdg);
    }
    setenv("XDG_CONFIG_HOME", tmp_dir, 1);

    Config cfg;
    config_init(&cfg);
    str_array_append(&cfg.dotfiles_dirs, tmp_dir);
    config_save(&cfg);
    config_free(&cfg);

    Config loaded;
    config_init(&loaded);
    ASSERT(config_load(&loaded), "Should load config file");
    ASSERT(str_array_contains(&loaded.dotfiles_dirs, tmp_dir),
           "Config should contain saved dotfiles_dir");
    config_free(&loaded);

    if (old_xdg_buf[0] != '\0') {
        setenv("XDG_CONFIG_HOME", old_xdg_buf, 1);
    } else {
        unsetenv("XDG_CONFIG_HOME");
    }

    cleanup_test_dir(tmp_dir);
}

void test_config_add_remove_dotfiles_dir(void)
{
    char tmp_dir[PATH_MAX];
    ASSERT(create_test_tmp_dir(tmp_dir, sizeof(tmp_dir), "cfg_dirs") != NULL,
           "Should create temporary directory for config dirs test");

    const char *old_xdg = getenv("XDG_CONFIG_HOME");
    char old_xdg_buf[PATH_MAX] = {0};
    if (old_xdg) {
        snprintf(old_xdg_buf, sizeof(old_xdg_buf), "%s", old_xdg);
    }
    setenv("XDG_CONFIG_HOME", tmp_dir, 1);

    char dir_a[PATH_MAX];
    char dir_b[PATH_MAX];
    snprintf(dir_a, sizeof(dir_a), "%s/dotfiles_a", tmp_dir);
    snprintf(dir_b, sizeof(dir_b), "%s/dotfiles_b", tmp_dir);
    ASSERT(mkdir(dir_a, 0755) == 0, "Should create dotfiles_a");
    ASSERT(mkdir(dir_b, 0755) == 0, "Should create dotfiles_b");

    // Add valid directory
    config_add_dotfiles_dir(dir_a);

    Config cfg;
    config_init(&cfg);
    ASSERT(config_load(&cfg), "Config should load");
    ASSERT(str_array_contains(&cfg.dotfiles_dirs, dir_a), "Config should contain dir_a");
    config_free(&cfg);

    config_add_dotfiles_dir(dir_b);
    config_init(&cfg);
    config_load(&cfg);
    ASSERT(str_array_contains(&cfg.dotfiles_dirs, dir_b), "Config should contain dir_b");

    // Duplicate addition handling
    config_add_dotfiles_dir(dir_a);
    config_free(&cfg);

    config_init(&cfg);
    config_load(&cfg);
    size_t count_a = 0;
    for (size_t i = 0; i < cfg.dotfiles_dirs.count; i++) {
        if (strcmp(cfg.dotfiles_dirs.items[i], dir_a) == 0) {
            count_a++;
        }
    }
    ASSERT(count_a == 1, "dir_a should not be duplicated in config");
    config_free(&cfg);

    // Remove path
    config_remove_dotfiles_dir(dir_a);
    config_init(&cfg);
    config_load(&cfg);
    ASSERT(!str_array_contains(&cfg.dotfiles_dirs, dir_a), "Config should no longer contain dir_a");
    ASSERT(str_array_contains(&cfg.dotfiles_dirs, dir_b), "Config should still contain dir_b");
    config_free(&cfg);

    // Remove non-registered path
    config_remove_dotfiles_dir("/nonexistent/path/for/test");
    config_init(&cfg);
    config_load(&cfg);
    ASSERT(cfg.dotfiles_dirs.count == 1,
           "Config count should remain 1 after removing non-registered path");
    config_free(&cfg);

    // Restore environment
    if (old_xdg_buf[0] != '\0') {
        setenv("XDG_CONFIG_HOME", old_xdg_buf, 1);
    } else {
        unsetenv("XDG_CONFIG_HOME");
    }

    cleanup_test_dir(tmp_dir);
}

void test_get_active_dotfiles_dir_cascade(void)
{
    char tmp_dir[PATH_MAX];
    ASSERT(create_test_tmp_dir(tmp_dir, sizeof(tmp_dir), "cfg_casc") != NULL,
           "Should create temporary directory for cascade test");

    // Save environment state
    const char *old_stow_dir = getenv("STOW_DOTFILES_DIR");
    char old_stow_buf[PATH_MAX] = {0};
    if (old_stow_dir) {
        snprintf(old_stow_buf, sizeof(old_stow_buf), "%s", old_stow_dir);
    }

    const char *old_dot_dir = getenv("DOTFILES_DIR");
    char old_dot_buf[PATH_MAX] = {0};
    if (old_dot_dir) {
        snprintf(old_dot_buf, sizeof(old_dot_buf), "%s", old_dot_dir);
    }

    const char *old_xdg = getenv("XDG_CONFIG_HOME");
    char old_xdg_buf[PATH_MAX] = {0};
    if (old_xdg) {
        snprintf(old_xdg_buf, sizeof(old_xdg_buf), "%s", old_xdg);
    }

    setenv("XDG_CONFIG_HOME", tmp_dir, 1);

    char cli_dir[PATH_MAX];
    char env_dir[PATH_MAX];
    char cfg_dir[PATH_MAX];
    snprintf(cli_dir, sizeof(cli_dir), "%s/cli_dotfiles", tmp_dir);
    snprintf(env_dir, sizeof(env_dir), "%s/env_dotfiles", tmp_dir);
    snprintf(cfg_dir, sizeof(cfg_dir), "%s/cfg_dotfiles", tmp_dir);

    ASSERT(mkdir(cli_dir, 0755) == 0, "Should create cli_dotfiles");
    ASSERT(mkdir(env_dir, 0755) == 0, "Should create env_dotfiles");
    ASSERT(mkdir(cfg_dir, 0755) == 0, "Should create cfg_dotfiles");

    // Setup config file to contain cfg_dir
    config_add_dotfiles_dir(cfg_dir);
    setenv("STOW_DOTFILES_DIR", env_dir, 1);

    char buf[PATH_MAX];

    // Priority 1: CLI override > ENV > Config
    get_active_dotfiles_dir(cli_dir, buf, sizeof(buf));
    ASSERT_STR_EQ(buf, cli_dir);

    // Priority 2: ENV > Config (CLI is NULL)
    get_active_dotfiles_dir(NULL, buf, sizeof(buf));
    ASSERT_STR_EQ(buf, env_dir);

    // Priority 3: Config file (CLI is NULL, ENV is unset)
    unsetenv("STOW_DOTFILES_DIR");
    unsetenv("DOTFILES_DIR");
    get_active_dotfiles_dir(NULL, buf, sizeof(buf));
    ASSERT_STR_EQ(buf, cfg_dir);

    // Priority 4: Fallback CWD (CLI NULL, ENV unset, Config empty)
    char cfg_file[PATH_MAX];
    snprintf(cfg_file, sizeof(cfg_file), "%s/stow-manager/config", tmp_dir);
    remove(cfg_file);

    get_active_dotfiles_dir(NULL, buf, sizeof(buf));
    ASSERT(strlen(buf) > 0, "Fallback active dotfiles dir should not be empty");

    // Restore environment
    if (old_stow_buf[0] != '\0') {
        setenv("STOW_DOTFILES_DIR", old_stow_buf, 1);
    } else {
        unsetenv("STOW_DOTFILES_DIR");
    }

    if (old_dot_buf[0] != '\0') {
        setenv("DOTFILES_DIR", old_dot_buf, 1);
    } else {
        unsetenv("DOTFILES_DIR");
    }

    if (old_xdg_buf[0] != '\0') {
        setenv("XDG_CONFIG_HOME", old_xdg_buf, 1);
    } else {
        unsetenv("XDG_CONFIG_HOME");
    }

    cleanup_test_dir(tmp_dir);
}

void test_config_sanity_checks(void)
{
    char tmp_dir[PATH_MAX];
    ASSERT(create_test_tmp_dir(tmp_dir, sizeof(tmp_dir), "cfg_san") != NULL,
           "Should create temporary test directory");

    const char *old_xdg = getenv("XDG_CONFIG_HOME");
    setenv("XDG_CONFIG_HOME", tmp_dir, 1);

    // Attempting to set an invalid/nonexistent directory should fail sanity
    // check
    config_set_target_dir("/nonexistent/invalid/target/path/xyz");

    Config cfg;
    config_init(&cfg);
    config_load(&cfg);
    ASSERT(cfg.target_dir[0] == '\0', "Target dir should not be set when sanity check fails");
    config_free(&cfg);

    if (old_xdg)
        setenv("XDG_CONFIG_HOME", old_xdg, 1);
    else
        unsetenv("XDG_CONFIG_HOME");

    cleanup_test_dir(tmp_dir);
}

void test_config_save_disclaimer(void)
{
    char tmp_dir[PATH_MAX];
    ASSERT(create_test_tmp_dir(tmp_dir, sizeof(tmp_dir), "cfg_hdr") != NULL,
           "Should create temporary test directory");

    const char *old_xdg = getenv("XDG_CONFIG_HOME");
    setenv("XDG_CONFIG_HOME", tmp_dir, 1);

    Config cfg;
    config_init(&cfg);
    str_array_append(&cfg.dotfiles_dirs, tmp_dir);
    config_save(&cfg);

    char cfg_path[PATH_MAX];
    snprintf(cfg_path, sizeof(cfg_path), "%s/stow-manager/config", tmp_dir);

    FILE *fp = fopen(cfg_path, "r");
    ASSERT(fp != NULL, "Saved config file should exist");

    char buffer[1024] = {0};
    fread(buffer, 1, sizeof(buffer) - 1, fp);
    fclose(fp);

    ASSERT(strstr(buffer, "Stow Manager Configuration (Auto-Generated)") != NULL,
           "Config file should contain auto-generated disclaimer header");

    config_free(&cfg);

    if (old_xdg)
        setenv("XDG_CONFIG_HOME", old_xdg, 1);
    else
        unsetenv("XDG_CONFIG_HOME");

    cleanup_test_dir(tmp_dir);
}
