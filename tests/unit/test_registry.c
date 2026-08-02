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

#include "../include/registry.h"
#include "test_framework.h"

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
