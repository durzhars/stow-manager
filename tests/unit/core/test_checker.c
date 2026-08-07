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
#include "core/checker.h"
#include "core/manifest.h"

void test_check_package_dependencies(void)
{
    char tmp_dotfiles[PATH_MAX];
    ASSERT(create_test_tmp_dir(tmp_dotfiles, sizeof(tmp_dotfiles), "chk_dep") != NULL,
           "Should create temporary dotfiles directory");

    char pkg_dir[PATH_MAX];
    int snp_res = snprintf(pkg_dir, sizeof(pkg_dir), "%s/chkpkg", tmp_dotfiles);
    ASSERT(snp_res > 0 && (size_t)snp_res < sizeof(pkg_dir),
           "snprintf should successfully format pkg_dir without truncation");
    ASSERT(mkdir(pkg_dir, 0755) == 0, "Should create chkpkg directory");

    // Add manifest with valid tool 'sh' and non-existent tool 'nonexistent_tool_xyz_99'
    manifest_add_dep(tmp_dotfiles, "chkpkg", "sh", "--required");
    manifest_add_dep(tmp_dotfiles, "chkpkg", "nonexistent_tool_xyz_99", "--optional");

    // Run dependency check in dry_run mode
    check_package_dependencies(tmp_dotfiles, "chkpkg", false, true);

    // Run dependency check for all packages in dry_run mode
    check_package_dependencies(tmp_dotfiles, "all", false, true);

    cleanup_test_dir(tmp_dotfiles);
}
