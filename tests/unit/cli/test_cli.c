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
#include "cli/cli.h"

void test_parse_cli_options_flags(void)
{
    CliOptions opts;
    StringArray args;

    // 1. Short boolean flags & positional command
    char *argv1[] = {"stow-manager", "-y", "-n", "stow", "nvim"};
    str_array_init(&args);
    int res1 = parse_cli_options(5, argv1, &opts, &args);
    ASSERT(res1 == 0, "parse_cli_options should return 0 success");
    ASSERT(opts.auto_install == true, "auto_install should be true");
    ASSERT(opts.dry_run == true, "dry_run should be true");
    ASSERT(opts.save_flag == false, "save_flag should be false");
    ASSERT(args.count == 2, "Should have 2 positional args");
    ASSERT_STR_EQ(args.items[0], "stow");
    ASSERT_STR_EQ(args.items[1], "nvim");
    str_array_free(&args);

    // 2. Long boolean flags
    char *argv2[] = {"stow-manager", "--install", "--dry-run", "--save", "list"};
    str_array_init(&args);
    int res2 = parse_cli_options(5, argv2, &opts, &args);
    ASSERT(res2 == 0, "parse_cli_options should return 0 success");
    ASSERT(opts.auto_install == true, "auto_install should be true");
    ASSERT(opts.dry_run == true, "dry_run should be true");
    ASSERT(opts.save_flag == true, "save_flag should be true");
    ASSERT(args.count == 1, "Should have 1 positional arg");
    ASSERT_STR_EQ(args.items[0], "list");
    str_array_free(&args);
}

void test_parse_cli_options_directory_overrides(void)
{
    CliOptions opts;
    StringArray args;

    // 1. Separate space directory arguments (-d and -t)
    char *argv1[] = {"stow-manager", "-d", "/my/dotfiles", "-t", "/my/target", "all"};
    str_array_init(&args);
    int res1 = parse_cli_options(6, argv1, &opts, &args);
    ASSERT(res1 == 0, "parse_cli_options should return 0");
    ASSERT(opts.cli_dotfiles_dir != NULL, "cli_dotfiles_dir should not be NULL");
    ASSERT_STR_EQ(opts.cli_dotfiles_dir, "/my/dotfiles");
    ASSERT(opts.cli_target_dir != NULL, "cli_target_dir should not be NULL");
    ASSERT_STR_EQ(opts.cli_target_dir, "/my/target");
    ASSERT(args.count == 1, "Should have 1 positional arg");
    ASSERT_STR_EQ(args.items[0], "all");
    str_array_free(&args);

    // 2. Equal sign long directory arguments (--dotfiles-dir= / --target-dir=)
    char *argv2[] = {
        "stow-manager", "--dotfiles-dir=/custom/dot", "--target-dir=/custom/tgt", "diff"};
    str_array_init(&args);
    int res2 = parse_cli_options(4, argv2, &opts, &args);
    ASSERT(res2 == 0, "parse_cli_options should return 0");
    ASSERT_STR_EQ(opts.cli_dotfiles_dir, "/custom/dot");
    ASSERT_STR_EQ(opts.cli_target_dir, "/custom/tgt");
    ASSERT(args.count == 1, "Should have 1 positional arg");
    ASSERT_STR_EQ(args.items[0], "diff");
    str_array_free(&args);
}

void test_parse_cli_options_errors_and_help(void)
{
    CliOptions opts;
    StringArray args;

    // 1. Help flag (-h) returns -1
    char *argv_h[] = {"stow-manager", "-h"};
    str_array_init(&args);
    int res_h = parse_cli_options(2, argv_h, &opts, &args);
    ASSERT(res_h == -1, "parse_cli_options -h should return -1");
    str_array_free(&args);

    // 2. Missing value for directory flag (-d without next arg)
    char *argv_missing[] = {"stow-manager", "-d"};
    str_array_init(&args);
    int res_missing = parse_cli_options(2, argv_missing, &opts, &args);
    ASSERT(res_missing == 1, "parse_cli_options with missing flag value should return 1 error");
    str_array_free(&args);

    // 3. No positional arguments at all returns -1
    char *argv_none[] = {"stow-manager", "-y"};
    str_array_init(&args);
    int res_none = parse_cli_options(2, argv_none, &opts, &args);
    ASSERT(res_none == -1, "parse_cli_options with no subcommands/packages should return -1");
    str_array_free(&args);
}
