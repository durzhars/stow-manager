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
#define _POSIX_C_SOURCE 200809L

#include "cli.h"

#include <stdio.h>
#include <string.h>

#include "config.h"
#include "help.h"

static int parse_string_opt(const char *arg,
                            const char *next_arg,
                            const char *short_flag,
                            const char *long_flag,
                            const char **dest,
                            int *i,
                            int argc)
{
    if (strcmp(arg, short_flag) == 0 || strcmp(arg, long_flag) == 0) {
        if (*i + 1 < argc) {
            *dest = next_arg;
            (*i)++;
            return 1;
        }
        log_error("Option '%s' requires a directory path argument", arg);
        return -1;
    }

    size_t len = strlen(long_flag);
    if (strncmp(arg, long_flag, len) == 0 && arg[len] == '=') {
        *dest = arg + len + 1;
        return 1;
    }

    return 0;
}

static bool
parse_bool_opt(const char *arg, const char *short_flag, const char *long_flag, bool *dest)
{
    if (strcmp(arg, short_flag) == 0 || strcmp(arg, long_flag) == 0) {
        *dest = true;
        return true;
    }
    return false;
}

static void
persist_dir_override(const char *val, void (*setter_fn)(const char *), const char *label)
{
    if (val && *val != '\0') {
        setter_fn(val);
        log_info("Saved %s directory override to config: %s", label, val);
    }
}

int parse_cli_options(int argc, char **argv, CliOptions *opts, StringArray *args)
{
    if (!opts || !args) {
        return 1;
    }

    memset(opts, 0, sizeof(CliOptions));

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        const char *next_arg = (i + 1 < argc) ? argv[i + 1] : NULL;

        // 1. Path Options (-d / --dotfiles-dir and -t / --target-dir)
        int res_d = parse_string_opt(
            arg, next_arg, "-d", "--dotfiles-dir", &opts->cli_dotfiles_dir, &i, argc);
        if (res_d < 0)
            return 1;
        if (res_d > 0)
            continue;

        int res_t =
            parse_string_opt(arg, next_arg, "-t", "--target-dir", &opts->cli_target_dir, &i, argc);
        if (res_t < 0)
            return 1;
        if (res_t > 0)
            continue;

        // 2. Boolean Flags (-y, -n, -s, -h)
        if (parse_bool_opt(arg, "-y", "--install", &opts->auto_install))
            continue;
        if (parse_bool_opt(arg, "-n", "--dry-run", &opts->dry_run))
            continue;
        if (parse_bool_opt(arg, "-s", "--save", &opts->save_flag))
            continue;

        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            show_help();
            return -1;
        }

        // 3. Positional Subcommands / Package Names
        str_array_append(args, arg);
    }

    if (args->count == 0) {
        show_help();
        return -1;
    }

    // Persist CLI directory overrides when -s / --save is passed
    if (opts->save_flag) {
        persist_dir_override(opts->cli_target_dir, config_set_target_dir, "target");
        persist_dir_override(opts->cli_dotfiles_dir, config_set_dotfiles_dir, "dotfiles");
    }

    return 0;
}
