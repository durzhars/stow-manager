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
void test_expand_env_vars(void);
void test_degraded_env_path_resolution(void);
void test_is_path_prefix(void);
void test_mkdir_p(void);
void test_mkdir_p_file_collision(void);
void test_join_path(void);
void test_symlink_helpers(void);
void test_is_executable_in_path(void);
void test_get_all_packages_skips_dot_dirs(void);
void test_default_stowignore(void);
void test_is_symlink_pointing_to(void);
void test_walk_dir_files_and_cleanup(void);
void test_path_sanity_strerror(void);
void test_temp_path_registration(void);
void test_path_parsing_traversal_and_naming_edge_cases(void);
void test_str_set(void);
void test_perf_timer(void);

/* Prototypes from test_manifest.c */
void test_manifest_load_save(void);
void test_manifest_add_and_remove_dep(void);
void test_manifest_malformed_file(void);
void test_manifest_edit_dep(void);
void test_package_remove(void);
void test_manifest_set_target(void);

/* Prototypes from test_config.c */
void test_config_system(void);
void test_config_add_remove_dotfiles_dir(void);
void test_get_active_dotfiles_dir_cascade(void);
void test_config_sanity_checks(void);
void test_config_save_disclaimer(void);

/* Prototypes from test_stow.c */
void test_stowignore(void);
void test_dry_run_stow(void);
void test_symlink_health_check(void);
void test_handle_mutual_exclusions(void);
void test_unfold_directory_symlinks(void);
void test_package_stow_status(void);
void test_native_stow_broken_symlink_conflict(void);
void test_native_unstow_edited_user_file(void);
void test_native_unstow_recursive_directory_cleanup(void);
void test_native_stow_ignore_patterns(void);
void test_dynamic_package_conflicts(void);
void test_restow_package(void);

/* Prototypes from test_scanner.c */
void test_scan_package(void);

/* Prototypes from test_registry.c */
void test_registry_parsing(void);

/* Prototypes from test_ignore.c */
void test_ignore_init_and_clear(void);
void test_ignore_add_and_remove_patterns(void);
void test_ignore_show(void);

/* Prototypes from test_cli.c */
void test_parse_cli_options_flags(void);
void test_parse_cli_options_directory_overrides(void);
void test_parse_cli_options_errors_and_help(void);

/* Prototypes from test_dispatch.c */
void test_dispatch_command_routes(void);

/* Prototypes from test_checker.c */
void test_check_package_dependencies(void);

int main(void)
{
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
    RUN_TEST(test_expand_env_vars);
    RUN_TEST(test_degraded_env_path_resolution);
    RUN_TEST(test_is_path_prefix);
    RUN_TEST(test_mkdir_p);
    RUN_TEST(test_join_path);
    RUN_TEST(test_symlink_helpers);
    RUN_TEST(test_is_executable_in_path);
    RUN_TEST(test_get_all_packages_skips_dot_dirs);
    RUN_TEST(test_default_stowignore);
    RUN_TEST(test_is_symlink_pointing_to);
    RUN_TEST(test_mkdir_p_file_collision);
    RUN_TEST(test_walk_dir_files_and_cleanup);
    RUN_TEST(test_path_sanity_strerror);
    RUN_TEST(test_temp_path_registration);
    RUN_TEST(test_path_parsing_traversal_and_naming_edge_cases);
    RUN_TEST(test_str_set);
    RUN_TEST(test_perf_timer);

    // test_manifest.c
    RUN_TEST(test_manifest_load_save);
    RUN_TEST(test_manifest_add_and_remove_dep);
    RUN_TEST(test_manifest_malformed_file);
    RUN_TEST(test_manifest_edit_dep);
    RUN_TEST(test_package_remove);
    RUN_TEST(test_manifest_set_target);

    // test_config.c
    RUN_TEST(test_config_system);
    RUN_TEST(test_config_add_remove_dotfiles_dir);
    RUN_TEST(test_get_active_dotfiles_dir_cascade);
    RUN_TEST(test_config_sanity_checks);
    RUN_TEST(test_config_save_disclaimer);

    // test_stow.c
    RUN_TEST(test_stowignore);
    RUN_TEST(test_dry_run_stow);
    RUN_TEST(test_symlink_health_check);
    RUN_TEST(test_handle_mutual_exclusions);
    RUN_TEST(test_unfold_directory_symlinks);
    RUN_TEST(test_package_stow_status);
    RUN_TEST(test_native_stow_broken_symlink_conflict);
    RUN_TEST(test_native_unstow_edited_user_file);
    RUN_TEST(test_native_unstow_recursive_directory_cleanup);
    RUN_TEST(test_native_stow_ignore_patterns);
    RUN_TEST(test_dynamic_package_conflicts);
    RUN_TEST(test_restow_package);

    // test_scanner.c
    RUN_TEST(test_scan_package);

    // test_registry.c
    RUN_TEST(test_registry_parsing);

    // test_ignore.c
    RUN_TEST(test_ignore_init_and_clear);
    RUN_TEST(test_ignore_add_and_remove_patterns);
    RUN_TEST(test_ignore_show);

    // test_cli.c
    RUN_TEST(test_parse_cli_options_flags);
    RUN_TEST(test_parse_cli_options_directory_overrides);
    RUN_TEST(test_parse_cli_options_errors_and_help);

    // test_dispatch.c
    RUN_TEST(test_dispatch_command_routes);

    // test_checker.c
    RUN_TEST(test_check_package_dependencies);

    printf("\n=== Test Results: %d Passed, %d Failed ===\n\n",
           g_tests_run - g_tests_failed,
           g_tests_failed);

    return g_tests_failed == 0 ? 0 : 1;
}
