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
#include "core/manifest.h"
#include "utils/fs.h"

void test_manifest_load_save(void)
{
    char tmp_dir[PATH_MAX];
    ASSERT(create_test_tmp_dir(tmp_dir, sizeof(tmp_dir), "man_save") != NULL,
           "Should create temporary test directory");

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

    ASSERT(str_array_contains(&loaded.required, "bash"),
           "Loaded manifest should contain required 'bash'");
    ASSERT(str_array_contains(&loaded.required, "zsh"),
           "Loaded manifest should contain required 'zsh'");
    ASSERT(str_array_contains(&loaded.optional, "fzf"),
           "Loaded manifest should contain optional 'fzf'");
    ASSERT(str_array_contains(&loaded.conflicts, "otherpkg"),
           "Loaded manifest should contain conflict 'otherpkg'");

    manifest_free(&loaded);

    cleanup_test_dir(tmp_dir);
}

void test_manifest_add_and_remove_dep(void)
{
    char tmp_dir[PATH_MAX];
    ASSERT(create_test_tmp_dir(tmp_dir, sizeof(tmp_dir), "man_dep") != NULL,
           "Should create temporary directory for manifest dep test");

    char pkg_dir[PATH_MAX];
    snprintf(pkg_dir, sizeof(pkg_dir), "%s/mypkg", tmp_dir);
    ASSERT(mkdir(pkg_dir, 0755) == 0, "Should create mypkg directory");

    // Add dependencies (--required, --optional, --conflict)
    manifest_add_dep(tmp_dir, "mypkg", "bash", "--required");
    manifest_add_dep(tmp_dir, "mypkg", "fzf", "--optional");
    manifest_add_dep(tmp_dir, "mypkg", "otherpkg", "--conflict");

    PackageManifest loaded;
    manifest_init(&loaded, "mypkg");
    ASSERT(manifest_load(&loaded, tmp_dir), "Manifest load should succeed");
    ASSERT(str_array_contains(&loaded.required, "bash"), "Should contain required 'bash'");
    ASSERT(str_array_contains(&loaded.optional, "fzf"), "Should contain optional 'fzf'");
    ASSERT(str_array_contains(&loaded.conflicts, "otherpkg"), "Should contain conflict 'otherpkg'");

    // Duplicate prevention
    manifest_add_dep(tmp_dir, "mypkg", "bash", "--required");
    manifest_free(&loaded);

    manifest_init(&loaded, "mypkg");
    ASSERT(manifest_load(&loaded, tmp_dir), "Reload manifest after duplicate addition");
    size_t bash_count = 0;
    for (size_t i = 0; i < loaded.required.count; i++) {
        if (strcmp(loaded.required.items[i], "bash") == 0) {
            bash_count++;
        }
    }
    ASSERT(bash_count == 1, "Duplicate dependency 'bash' should not be added twice");

    // Removing dependencies
    manifest_remove_dep(tmp_dir, "mypkg", "bash");
    manifest_free(&loaded);

    manifest_init(&loaded, "mypkg");
    ASSERT(manifest_load(&loaded, tmp_dir), "Reload manifest after removal");
    ASSERT(!str_array_contains(&loaded.required, "bash"), "'bash' should be removed from manifest");

    manifest_free(&loaded);

    cleanup_test_dir(tmp_dir);
}

void test_manifest_malformed_file(void)
{
    char tmp_dir[PATH_MAX];
    ASSERT(create_test_tmp_dir(tmp_dir, sizeof(tmp_dir), "man_bad") != NULL,
           "Should create temporary directory for malformed manifest test");

    char pkg_dir[PATH_MAX];
    snprintf(pkg_dir, sizeof(pkg_dir), "%s/badpkg", tmp_dir);
    ASSERT(mkdir(pkg_dir, 0755) == 0, "Should create badpkg directory");

    char stowdeps[PATH_MAX];
    snprintf(stowdeps, sizeof(stowdeps), "%s/.stowdeps", pkg_dir);
    FILE *fp = fopen(stowdeps, "w");
    ASSERT(fp != NULL, "Should open .stowdeps for writing");
    fprintf(fp, "INVALID_LINE_WITHOUT_EQUALS\n");
    fprintf(fp, "UNKNOWN_KEY=value\n");
    fprintf(fp, "REQUIRED=\n");
    fprintf(fp, "=NO_KEY\n");
    fprintf(fp, "REQUIRED=\"valid_pkg zsh\"\n");
    fclose(fp);

    PackageManifest loaded;
    manifest_init(&loaded, "badpkg");
    bool ok = manifest_load(&loaded, tmp_dir);
    ASSERT(ok, "Manifest loading should not crash on malformed input");
    ASSERT(str_array_contains(&loaded.required, "valid_pkg"),
           "Valid entry in malformed manifest should still be parsed");
    ASSERT(str_array_contains(&loaded.required, "zsh"), "Valid entry 'zsh' should be parsed");

    manifest_free(&loaded);

    cleanup_test_dir(tmp_dir);
}

void test_manifest_edit_dep(void)
{
    char tmp_dir[PATH_MAX];
    ASSERT(create_test_tmp_dir(tmp_dir, sizeof(tmp_dir), "man_edt") != NULL,
           "Should create temporary directory for manifest edit test");

    char pkg_dir[PATH_MAX];
    snprintf(pkg_dir, sizeof(pkg_dir), "%s/editpkg", tmp_dir);
    ASSERT(mkdir(pkg_dir, 0755) == 0, "Should create editpkg directory");

    manifest_add_dep(tmp_dir, "editpkg", "bash", "--optional");

    PackageManifest loaded;
    manifest_init(&loaded, "editpkg");
    ASSERT(manifest_load(&loaded, tmp_dir), "Manifest load should succeed");
    ASSERT(str_array_contains(&loaded.optional, "bash"), "Should initially be optional");
    manifest_free(&loaded);

    manifest_edit_dep(tmp_dir, "editpkg", "bash", "--required");

    manifest_init(&loaded, "editpkg");
    ASSERT(manifest_load(&loaded, tmp_dir), "Manifest reload should succeed");
    ASSERT(str_array_contains(&loaded.required, "bash"), "Should now be required");
    ASSERT(!str_array_contains(&loaded.optional, "bash"), "Should no longer be optional");
    manifest_free(&loaded);

    cleanup_test_dir(tmp_dir);
}

void test_package_remove(void)
{
    char tmp_dotfiles[PATH_MAX];
    char tmp_target[PATH_MAX];
    ASSERT(create_test_tmp_dir(tmp_dotfiles, sizeof(tmp_dotfiles), "pkg_rm_dot") != NULL,
           "Should create temporary dotfiles directory");
    ASSERT(create_test_tmp_dir(tmp_target, sizeof(tmp_target), "pkg_rm_tgt") != NULL,
           "Should create temporary target directory");

    char pkg_dir[PATH_MAX];
    snprintf(pkg_dir, sizeof(pkg_dir), "%s/rmpkg", tmp_dotfiles);
    ASSERT(mkdir(pkg_dir, 0755) == 0, "Should create rmpkg directory");

    char file1[PATH_MAX];
    snprintf(file1, sizeof(file1), "%s/.file1", pkg_dir);
    FILE *fp = fopen(file1, "w");
    if (fp) {
        fprintf(fp, "content\n");
        fclose(fp);
    }

    ASSERT(is_dir(pkg_dir), "rmpkg directory should exist");

    package_remove(tmp_dotfiles, tmp_target, "rmpkg", false);
    ASSERT(!file_exists(pkg_dir), "rmpkg directory should be deleted after package_remove");

    cleanup_test_dir(tmp_dotfiles);
    cleanup_test_dir(tmp_target);
}

void test_manifest_set_target(void)
{
    char tmp_dir[PATH_MAX];
    ASSERT(create_test_tmp_dir(tmp_dir, sizeof(tmp_dir), "man_tgt") != NULL,
           "Should create temporary test directory");

    char pkg_dir[PATH_MAX];
    snprintf(pkg_dir, sizeof(pkg_dir), "%s/tgtpkg", tmp_dir);
    ASSERT(mkdir(pkg_dir, 0755) == 0, "Should create tgtpkg directory");

    manifest_set_target(tmp_dir, "tgtpkg", "/custom/target/path");

    PackageManifest loaded;
    manifest_init(&loaded, "tgtpkg");
    ASSERT(manifest_load(&loaded, tmp_dir), "Should load manifest");
    ASSERT(loaded.target_path != NULL, "Manifest target_path should not be NULL");
    ASSERT_STR_EQ(loaded.target_path, "/custom/target/path");

    manifest_free(&loaded);

    cleanup_test_dir(tmp_dir);
}
