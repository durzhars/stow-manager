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
#include "../include/utils.h"
#include "../include/manifest.h"
#include "../include/registry.h"
#include "../include/config.h"
#include "../include/stow.h"
#include "../include/checker.h"

static void test_trim_whitespace(void) {
    char s1[] = "  hello world  ";
    ASSERT_STR_EQ(trim_whitespace(s1), "hello world");

    char s2[] = "\"quoted string\"";
    ASSERT_STR_EQ(trim_whitespace(s2), "quoted string");

    char s3[] = "\t\n  ";
    ASSERT_STR_EQ(trim_whitespace(s3), "");
}

static void test_string_array(void) {
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

static void test_manifest_load_save(void) {
    char tmp_dir[] = "/tmp/stow_test_XXXXXX";
    ASSERT(mkdtemp(tmp_dir) != NULL, "Should create temporary test directory");

    char pkg_dir[PATH_MAX * 4];
    snprintf(pkg_dir, sizeof(pkg_dir), "%s/testpkg", tmp_dir);
    mkdir(pkg_dir, 0755);

    PackageManifest manifest;
    manifest_init(&manifest, "testpkg");
    str_array_append(&manifest.required, "bash");
    str_array_append(&manifest.required, "zsh");
    str_array_append(&manifest.optional, "fzf");
    str_array_append(&manifest.conflicts, "otherpkg");

    ASSERT(manifest_save(&manifest, tmp_dir), "Should save manifest file");
    manifest_free(&manifest);

    PackageManifest loaded;
    manifest_init(&loaded, "testpkg");
    ASSERT(manifest_load(&loaded, tmp_dir), "Should load manifest file");

    ASSERT(str_array_contains(&loaded.required, "bash"), "Loaded manifest should contain required 'bash'");
    ASSERT(str_array_contains(&loaded.required, "zsh"), "Loaded manifest should contain required 'zsh'");
    ASSERT(str_array_contains(&loaded.optional, "fzf"), "Loaded manifest should contain optional 'fzf'");
    ASSERT(str_array_contains(&loaded.conflicts, "otherpkg"), "Loaded manifest should contain conflict 'otherpkg'");

    manifest_free(&loaded);

    char cleanup_cmd[PATH_MAX * 4];
    snprintf(cleanup_cmd, sizeof(cleanup_cmd), "rm -rf \"%s\"", tmp_dir);
    (void)system(cleanup_cmd);
}

static void test_registry_parsing(void) {
    char tmp_dir[] = "/tmp/stow_reg_XXXXXX";
    ASSERT(mkdtemp(tmp_dir) != NULL, "Should create temporary test directory");

    char reg_path[PATH_MAX * 4];
    snprintf(reg_path, sizeof(reg_path), "%s/stow.registry", tmp_dir);
    FILE *fp = fopen(reg_path, "w");
    ASSERT(fp != NULL, "Should open registry file for writing");

    fprintf(fp, "tool_a = bin_a1 | bin_a2\n");
    fprintf(fp, "tool_a@ubuntu = pkg_a_ubuntu\n");
    fclose(fp);

    StringArray aliases;
    str_array_init(&aliases);
    registry_get_aliases(tmp_dir, "tool_a", &aliases);

    ASSERT(str_array_contains(&aliases, "bin_a1"), "Aliases should contain 'bin_a1'");
    ASSERT(str_array_contains(&aliases, "bin_a2"), "Aliases should contain 'bin_a2'");
    str_array_free(&aliases);

    char distro_pkg[256];
    registry_get_distro_pkg(tmp_dir, "tool_a", "ubuntu", distro_pkg, sizeof(distro_pkg));
    ASSERT_STR_EQ(distro_pkg, "pkg_a_ubuntu");

    char cleanup_cmd[PATH_MAX * 4];
    snprintf(cleanup_cmd, sizeof(cleanup_cmd), "rm -rf \"%s\"", tmp_dir);
    (void)system(cleanup_cmd);
}

static void test_dry_run_stow(void) {
    char tmp_dotfiles[] = "/tmp/stow_dry_dot_XXXXXX";
    char tmp_target[] = "/tmp/stow_dry_tgt_XXXXXX";
    ASSERT(mkdtemp(tmp_dotfiles) != NULL, "Should create temporary dotfiles directory");
    ASSERT(mkdtemp(tmp_target) != NULL, "Should create temporary target directory");

    char pkg_dir[PATH_MAX * 4];
    snprintf(pkg_dir, sizeof(pkg_dir), "%s/mypkg", tmp_dotfiles);
    mkdir(pkg_dir, 0755);

    char cfg_file[PATH_MAX * 4];
    snprintf(cfg_file, sizeof(cfg_file), "%s/.configfile", pkg_dir);
    FILE *fp = fopen(cfg_file, "w");
    if (fp) { fprintf(fp, "test content\n"); fclose(fp); }

    int res = stow_package(tmp_dotfiles, tmp_target, "mypkg", false, true);
    ASSERT(res == 0, "Dry run stow should return 0 success");

    char target_cfg[PATH_MAX * 4];
    snprintf(target_cfg, sizeof(target_cfg), "%s/.configfile", tmp_target);
    ASSERT(!file_exists(target_cfg), "Dry run stow must not modify disk or create symlinks");

    char cleanup_cmd[PATH_MAX * 4];
    snprintf(cleanup_cmd, sizeof(cleanup_cmd), "rm -rf \"%s\" \"%s\"", tmp_dotfiles, tmp_target);
    (void)system(cleanup_cmd);
}

static void test_symlink_health_check(void) {
    char tmp_dotfiles[] = "/tmp/stow_sym_dot_XXXXXX";
    char tmp_target[] = "/tmp/stow_sym_tgt_XXXXXX";
    ASSERT(mkdtemp(tmp_dotfiles) != NULL, "Should create temporary dotfiles directory");
    ASSERT(mkdtemp(tmp_target) != NULL, "Should create temporary target directory");

    char broken_link[PATH_MAX * 4];
    snprintf(broken_link, sizeof(broken_link), "%s/broken.symlink", tmp_dotfiles);
    symlink("/nonexistent/file/path", broken_link);

    char del_pkg[PATH_MAX * 4];
    snprintf(del_pkg, sizeof(del_pkg), "%s/delpkg", tmp_dotfiles);
    mkdir(del_pkg, 0755);
    char del_file[PATH_MAX * 4];
    snprintf(del_file, sizeof(del_file), "%s/.dummy", del_pkg);

    char orphan_link[PATH_MAX * 4];
    snprintf(orphan_link, sizeof(orphan_link), "%s/orphan.symlink", tmp_target);
    symlink(del_file, orphan_link);

    check_symlink_health(tmp_dotfiles, tmp_target);

    char cleanup_cmd[PATH_MAX * 4];
    snprintf(cleanup_cmd, sizeof(cleanup_cmd), "rm -rf \"%s\" \"%s\"", tmp_dotfiles, tmp_target);
    (void)system(cleanup_cmd);
}

static void test_config_system(void) {
    char tmp_dir[] = "/tmp/stow_cfg_test_XXXXXX";
    ASSERT(mkdtemp(tmp_dir) != NULL, "Should create temporary test directory");

    Config cfg;
    config_init(&cfg);
    str_array_append(&cfg.dotfiles_dirs, tmp_dir);
    config_save(&cfg);
    config_free(&cfg);

    Config loaded;
    config_init(&loaded);
    ASSERT(config_load(&loaded), "Should load config file");
    ASSERT(str_array_contains(&loaded.dotfiles_dirs, tmp_dir), "Config should contain saved dotfiles_dir");
    config_free(&loaded);

    char cleanup_cmd[PATH_MAX * 4];
    snprintf(cleanup_cmd, sizeof(cleanup_cmd), "rm -rf \"%s\"", tmp_dir);
    (void)system(cleanup_cmd);
}

int main(void) {
    printf("\n=== Running Dotfiles Stow Manager C Unit Tests ===\n\n");

    RUN_TEST(test_trim_whitespace);
    RUN_TEST(test_string_array);
    RUN_TEST(test_manifest_load_save);
    RUN_TEST(test_registry_parsing);
    RUN_TEST(test_dry_run_stow);
    RUN_TEST(test_symlink_health_check);
    RUN_TEST(test_config_system);

    printf("\n=== Test Results: %d Passed, %d Failed ===\n\n",
           g_tests_run - g_tests_failed, g_tests_failed);

    return g_tests_failed == 0 ? 0 : 1;
}
