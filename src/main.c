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

#include "cli/cli.h"
#include "cli/cmd_dispatch.h"
#include "utils/signal.h"

int main(int argc, char **argv)
{
    setup_signal_handlers();

    CliOptions opts;
    StringArray args;
    str_array_init(&args);

    int parse_res = parse_cli_options(argc, argv, &opts, &args);
    if (parse_res != 0) {
        str_array_free(&args);
        return (parse_res < 0) ? 0 : 1;
    }

    int status = dispatch_command(&args, &opts);

    str_array_free(&args);
    return status;
}
