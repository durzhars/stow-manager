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

#ifndef STOW_CLI_H
#define STOW_CLI_H

#include <stdbool.h>

#include "utils/mem.h"

typedef struct {
    bool auto_install;
    bool dry_run;
    bool save_flag;
    const char *cli_dotfiles_dir;
    const char *cli_target_dir;
} CliOptions;

int parse_cli_options(int argc, char **argv, CliOptions *opts, StringArray *positional_args);

#endif /* STOW_CLI_H */
