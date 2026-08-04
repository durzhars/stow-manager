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

#include "../include/cmd_dispatch.h"
#include "../include/config.h"
#include "test_framework.h"

void test_dispatch_command_routes(void)
{
    char tmp_dotfiles[PATH_MAX];
    char tmp_target[PATH_MAX];
    ASSERT(create_test_tmp_dir(tmp_dotfiles, sizeof(tmp_dotfiles), "disp_dot") != NULL,
           "Should create temporary dotfiles directory");
    ASSERT(create_test_tmp_dir(tmp_target, sizeof(tmp_target), "disp_tgt") != NULL,
           "Should create temporary target directory");

    char pkg_dir[PATH_MAX];
    snprintf(pkg_dir, sizeof(pkg_dir), "%s/mypkg", tmp_dotfiles);
    ASSERT(mkdir(pkg_dir, 0755) == 0, "Should create package mypkg");

    char file1[PATH_MAX];
    snprintf(file1, sizeof(file1), "%s/.configfile", pkg_dir);
    FILE *fp = fopen(file1, "w");
    if (fp) {
        fprintf(fp, "content\n");
        fclose(fp);
    }

    CliOptions opts = {.auto_install = false,
                       .dry_run = true, // Use dry_run for safe execution testing
                       .save_flag = false,
                       .cli_dotfiles_dir = tmp_dotfiles,
                       .cli_target_dir = tmp_target};

    StringArray args;

    // 1. Dispatch 'stow mypkg'
    str_array_init(&args);
    str_array_append(&args, "stow");
    str_array_append(&args, "mypkg");
    int res1 = dispatch_command(&args, &opts);
    ASSERT(res1 == 0, "dispatch_command 'stow mypkg' should return 0 success");
    str_array_free(&args);

    // 2. Dispatch 'diff mypkg'
    str_array_init(&args);
    str_array_append(&args, "diff");
    str_array_append(&args, "mypkg");
    int res2 = dispatch_command(&args, &opts);
    ASSERT(res2 == 0, "dispatch_command 'diff mypkg' should return 0 success");
    str_array_free(&args);

    // 3. Dispatch 'pkg:list' (alias route)
    str_array_init(&args);
    str_array_append(&args, "pkg:list");
    int res3 = dispatch_command(&args, &opts);
    ASSERT(res3 == 0, "dispatch_command 'pkg:list' should return 0 success");
    str_array_free(&args);

    // 4. Dispatch implicit stow (passing 'mypkg' directly)
    str_array_init(&args);
    str_array_append(&args, "mypkg");
    int res4 = dispatch_command(&args, &opts);
    ASSERT(res4 == 0, "dispatch_command implicit package stow should return 0 success");
    str_array_free(&args);

    // 5. Dispatch unknown command
    str_array_init(&args);
    str_array_append(&args, "unknown_action_xyz_123");
    int res5 = dispatch_command(&args, &opts);
    ASSERT(res5 == 1, "dispatch_command with unknown command should return 1 error");
    str_array_free(&args);

    // 6. Insufficient arguments (e.g., 'stow' without package name)
    str_array_init(&args);
    str_array_append(&args, "stow");
    int res6 = dispatch_command(&args, &opts);
    ASSERT(res6 == 1, "dispatch_command 'stow' without pkg should fail with 1 error");
    str_array_free(&args);

    // 7. Dispatch zero arguments
    str_array_init(&args);
    int res7 = dispatch_command(&args, &opts);
    ASSERT(res7 == 1, "dispatch_command zero arguments should fail with 1 error");
    str_array_free(&args);

    cleanup_test_dir(tmp_dotfiles);
    cleanup_test_dir(tmp_target);
}
