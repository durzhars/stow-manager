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

#ifndef STOW_CMD_ROUTES_H
#define STOW_CMD_ROUTES_H

#include "cmd_dispatch.h"

/* Command Handler Prototypes */
int cmd_stow(const CommandContext *ctx);
int cmd_unstow(const CommandContext *ctx);
int cmd_restow(const CommandContext *ctx);
int cmd_all(const CommandContext *ctx);
int cmd_diff(const CommandContext *ctx);
int cmd_scan(const CommandContext *ctx);
int cmd_check(const CommandContext *ctx);
int cmd_check_symlinks(const CommandContext *ctx);
int cmd_fix_conflicts(const CommandContext *ctx);

int cmd_pkg_create(const CommandContext *ctx);
int cmd_pkg_remove(const CommandContext *ctx);
int cmd_pkg_list(const CommandContext *ctx);

int cmd_deps_add(const CommandContext *ctx);
int cmd_deps_edit(const CommandContext *ctx);
int cmd_deps_remove(const CommandContext *ctx);
int cmd_deps_show(const CommandContext *ctx);
int cmd_deps_target(const CommandContext *ctx);

int cmd_ignore_init(const CommandContext *ctx);
int cmd_ignore_add(const CommandContext *ctx);
int cmd_ignore_remove(const CommandContext *ctx);
int cmd_ignore_show(const CommandContext *ctx);
int cmd_ignore_clear(const CommandContext *ctx);

int cmd_config_show(const CommandContext *ctx);
int cmd_config_set(const CommandContext *ctx);
int cmd_config_add(const CommandContext *ctx);
int cmd_config_remove(const CommandContext *ctx);

int cmd_help(const CommandContext *ctx);

/* Command Routing Table Registry */
extern const CommandRoute ROUTE_TABLE[];

#endif /* STOW_CMD_ROUTES_H */
