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
#include "core/scanner.h"

void test_scan_package(void)
{
    char tmp_dotfiles[PATH_MAX];
    ASSERT(create_test_tmp_dir(tmp_dotfiles, sizeof(tmp_dotfiles), "scan_pkg") != NULL,
           "Should create temporary dotfiles directory");

    char pkg_dir[PATH_MAX];
    snprintf(pkg_dir, sizeof(pkg_dir), "%s/scanpkg", tmp_dotfiles);
    ASSERT(mkdir(pkg_dir, 0755) == 0, "Should create scanpkg directory");

    char fzf_pkg_dir[PATH_MAX];
    snprintf(fzf_pkg_dir, sizeof(fzf_pkg_dir), "%s/fzf", tmp_dotfiles);
    ASSERT(mkdir(fzf_pkg_dir, 0755) == 0, "Should create fzf candidate package directory");

    char tmux_pkg_dir[PATH_MAX];
    snprintf(tmux_pkg_dir, sizeof(tmux_pkg_dir), "%s/tmux", tmp_dotfiles);
    ASSERT(mkdir(tmux_pkg_dir, 0755) == 0, "Should create tmux candidate package directory");

    char script_path[PATH_MAX];
    snprintf(script_path, sizeof(script_path), "%s/setup.sh", pkg_dir);
    FILE *fp = fopen(script_path, "w");
    ASSERT(fp != NULL, "Should open script file for writing");
    fprintf(fp, "#!/usr/bin/env bash\n");
    fprintf(fp, "# Test script\n");
    fprintf(fp, "fzf --version\n");
    fprintf(fp, "tmux new-session\n");
    fclose(fp);

    scan_package(tmp_dotfiles, "scanpkg");

    PackageManifest manifest;
    manifest_init(&manifest, "scanpkg");
    ASSERT(manifest_load(&manifest, tmp_dotfiles),
           "Generated manifest should exist and load successfully");
    ASSERT(str_array_contains(&manifest.required, "bash"),
           "Shebang should auto-detect 'bash' as required dependency");
    ASSERT(str_array_contains(&manifest.optional, "fzf"),
           "Tool invocation should auto-detect 'fzf' as optional tool");
    ASSERT(str_array_contains(&manifest.optional, "tmux"),
           "Tool invocation should auto-detect 'tmux' as optional tool");
    manifest_free(&manifest);

    cleanup_test_dir(tmp_dotfiles);
}
