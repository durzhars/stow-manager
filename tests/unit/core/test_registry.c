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
#include "core/registry.h"

void test_registry_parsing(void)
{
    char tmp_dir[PATH_MAX];
    ASSERT(create_test_tmp_dir(tmp_dir, sizeof(tmp_dir), "reg") != NULL,
           "Should create temporary test directory");

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

    cleanup_test_dir(tmp_dir);
}

void test_get_all_packages_skips_dot_dirs(void)
{
    char tmp_dir[PATH_MAX];
    ASSERT(create_test_tmp_dir(tmp_dir, sizeof(tmp_dir), "get_pkgs") != NULL,
           "Should create temporary dotfiles directory");

    char pkg1[PATH_MAX];
    snprintf(pkg1, sizeof(pkg1), "%s/hyprland", tmp_dir);
    mkdir(pkg1, 0755);

    char pkg2[PATH_MAX];
    snprintf(pkg2, sizeof(pkg2), "%s/nvim", tmp_dir);
    mkdir(pkg2, 0755);

    StringArray pkgs;
    str_array_init(&pkgs);
    get_all_packages(tmp_dir, &pkgs);

    ASSERT(!str_array_contains(&pkgs, "."), "get_all_packages must not include '.'");
    ASSERT(!str_array_contains(&pkgs, ".."), "get_all_packages must not include '..'");
    ASSERT(str_array_contains(&pkgs, "hyprland"), "get_all_packages should include 'hyprland'");
    ASSERT(str_array_contains(&pkgs, "nvim"), "get_all_packages should include 'nvim'");

    str_array_free(&pkgs);

    cleanup_test_dir(tmp_dir);
}
