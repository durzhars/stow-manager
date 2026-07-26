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

#include "test_framework.h"

int g_tests_run = 0;
int g_tests_failed = 0;

/* Prototypes from test_utils.c */
void test_trim_whitespace(void);
void test_string_array(void);
void test_xdg_paths(void);
void test_safe_allocators(void);
void test_normalize_path(void);
void test_collapse_path(void);
void test_escape_shell_arg(void);
void test_expand_tilde_path(void);
void test_is_path_prefix(void);
void test_mkdir_p(void);

/* Prototypes from test_manifest.c */
void test_manifest_load_save(void);
void test_manifest_add_and_remove_dep(void);
void test_manifest_malformed_file(void);
void test_manifest_edit_dep(void);
void test_package_remove(void);

/* Prototypes from test_config.c */
void test_config_system(void);
void test_config_add_remove_dotfiles_dir(void);
void test_get_active_dotfiles_dir_cascade(void);

/* Prototypes from test_stow.c */
void test_stowignore(void);
void test_dry_run_stow(void);
void test_symlink_health_check(void);
void test_handle_mutual_exclusions(void);
void test_unfold_directory_symlinks(void);
void test_package_stow_status(void);

/* Prototypes from test_registry.c */
void test_registry_parsing(void);

int main(void) {
    printf("\n=== Running Dotfiles Stow Manager C Unit Tests ===\n\n");

    // test_utils.c
    RUN_TEST(test_trim_whitespace);
    RUN_TEST(test_string_array);
    RUN_TEST(test_xdg_paths);
    RUN_TEST(test_safe_allocators);
    RUN_TEST(test_normalize_path);
    RUN_TEST(test_collapse_path);
    RUN_TEST(test_escape_shell_arg);
    RUN_TEST(test_expand_tilde_path);
    RUN_TEST(test_is_path_prefix);
    RUN_TEST(test_mkdir_p);

    // test_manifest.c
    RUN_TEST(test_manifest_load_save);
    RUN_TEST(test_manifest_add_and_remove_dep);
    RUN_TEST(test_manifest_malformed_file);
    RUN_TEST(test_manifest_edit_dep);
    RUN_TEST(test_package_remove);

    // test_config.c
    RUN_TEST(test_config_system);
    RUN_TEST(test_config_add_remove_dotfiles_dir);
    RUN_TEST(test_get_active_dotfiles_dir_cascade);

    // test_stow.c
    RUN_TEST(test_stowignore);
    RUN_TEST(test_dry_run_stow);
    RUN_TEST(test_symlink_health_check);
    RUN_TEST(test_handle_mutual_exclusions);
    RUN_TEST(test_unfold_directory_symlinks);
    RUN_TEST(test_package_stow_status);

    // test_registry.c
    RUN_TEST(test_registry_parsing);

    printf("\n=== Test Results: %d Passed, %d Failed ===\n\n",
           g_tests_run - g_tests_failed, g_tests_failed);

    return g_tests_failed == 0 ? 0 : 1;
}
