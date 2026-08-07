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

#ifndef STOW_CMD_DISPATCH_H
#define STOW_CMD_DISPATCH_H

#include <stdbool.h>

#include "cli.h"

// Unified context passed into every command handler template
typedef struct {
    const CliOptions *opts;
    const char *dotfiles_dir;
    const char *global_target_dir;
    const StringArray *args; // Positional sub-arguments for the matched command
    size_t arg_offset;       // Index where subcommand payload arguments begin
} CommandContext;

typedef int (*CommandHandler)(const CommandContext *ctx);

// Declarative Command Route Descriptor
typedef struct {
    const char *group;      // e.g. "config", "pkg", "deps", "ignore", "stow"
    const char *subcommand; // e.g. "create", "add", "show" (NULL for top-level)
    const char *const
        *aliases;      // NULL-terminated alternative names (e.g. {"package", "make:pkg", NULL})
    size_t min_args;   // Minimum required positional arguments after group/subcommand
    const char *usage; // Usage error string displayed on min_args failure
    CommandHandler handler; // Function pointer to execution logic
} CommandRoute;

// Routes positional arguments against the command table
int dispatch_command(const StringArray *args, const CliOptions *opts);

#endif /* STOW_CMD_DISPATCH_H */
