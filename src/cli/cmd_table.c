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

#include "cli/cmd_routes.h"
#include <stddef.h>

// clang-format off
static const char *const ALIAS_NONE[]          = {NULL};
static const char *const ALIAS_FIX[]           = {"fix", NULL};
static const char *const ALIAS_PKG_CREATE[]    = {"package:create", "make:pkg", "pkg:create", NULL};
static const char *const ALIAS_PKG_REMOVE[]    = {"package:remove", "pkg:rm", "pkg:remove", NULL};
static const char *const ALIAS_PKG_LIST[]      = {"package:list", "pkg:show", "pkg:list", "list", NULL};
static const char *const ALIAS_DEPS_ADD[]      = {"deps:add", NULL};
static const char *const ALIAS_DEPS_EDIT[]     = {"deps:edit", "deps:set", NULL};
static const char *const ALIAS_DEPS_REMOVE[]   = {"deps:remove", "deps:rm", NULL};
static const char *const ALIAS_DEPS_SHOW[]     = {"deps:show", "deps:list", NULL};
static const char *const ALIAS_DEPS_TARGET[]   = {"deps:target", NULL};
static const char *const ALIAS_IGNORE_INIT[]   = {"ignore:init", "ignore:create", NULL};
static const char *const ALIAS_IGNORE_ADD[]    = {"ignore:add", NULL};
static const char *const ALIAS_IGNORE_REMOVE[] = {"ignore:remove", "ignore:rm", "ignore:delete", NULL};
static const char *const ALIAS_IGNORE_SHOW[]   = {"ignore:show", "ignore:list", NULL};
static const char *const ALIAS_IGNORE_CLEAR[]  = {"ignore:clear", "ignore:purge", NULL};
static const char *const ALIAS_CONFIG_SHOW[]   = {"config:show", "config:list", "config:get", NULL};
static const char *const ALIAS_CONFIG_SET[]    = {"config:set", "config:target", NULL};
static const char *const ALIAS_CONFIG_ADD[]    = {"config:add", NULL};
static const char *const ALIAS_CONFIG_REMOVE[] = {"config:remove", "config:rm", NULL};
static const char *const ALIAS_HELP[]          = {"-h", "--help", NULL};

// clang-format off
const CommandRoute ROUTE_TABLE[] = {
    /* Group          Subcommand   Aliases               MinArgs  Usage                                                              Handler */
    {"stow",           NULL,       ALIAS_NONE,           1,       "Usage: stow-manager stow <pkg...>",                               cmd_stow},
    {"unstow",         NULL,       ALIAS_NONE,           1,       "Usage: stow-manager unstow <pkg...>",                             cmd_unstow},
    {"restow",         NULL,       ALIAS_NONE,           1,       "Usage: stow-manager restow <pkg...>",                             cmd_restow},
    {"all",            NULL,       ALIAS_NONE,           0,       "Usage: stow-manager all",                                         cmd_all},
    {"diff",           NULL,       ALIAS_NONE,           0,       "Usage: stow-manager diff [pkg...]",                               cmd_diff},
    {"scan",           NULL,       ALIAS_NONE,           0,       "Usage: stow-manager scan [pkg...]",                               cmd_scan},
    {"check",          NULL,       ALIAS_NONE,           0,       "Usage: stow-manager check [pkg...]",                              cmd_check},
    {"check-symlinks", NULL,       ALIAS_NONE,           0,       "Usage: stow-manager check-symlinks",                              cmd_check_symlinks},
    {"fix-conflicts",  NULL,       ALIAS_FIX,            0,       "Usage: stow-manager fix-conflicts",                               cmd_fix_conflicts},

    {"pkg",            "create",   ALIAS_PKG_CREATE,     1,       "Usage: stow-manager pkg create <name>",                           cmd_pkg_create},
    {"pkg",            "remove",   ALIAS_PKG_REMOVE,     1,       "Usage: stow-manager pkg remove <name...>",                        cmd_pkg_remove},
    {"pkg",            "list",     ALIAS_PKG_LIST,       0,       "Usage: stow-manager pkg list",                                    cmd_pkg_list},

    {"deps",           "add",      ALIAS_DEPS_ADD,       2,       "Usage: stow-manager deps add <pkg> <dep> [--required|--optional|--conflict]", cmd_deps_add},
    {"deps",           "edit",     ALIAS_DEPS_EDIT,      3,       "Usage: stow-manager deps edit <pkg> <dep> <type>",                cmd_deps_edit},
    {"deps",           "remove",   ALIAS_DEPS_REMOVE,    2,       "Usage: stow-manager deps remove <pkg> <dep>",                     cmd_deps_remove},
    {"deps",           "show",     ALIAS_DEPS_SHOW,      1,       "Usage: stow-manager deps show <pkg>",                             cmd_deps_show},
    {"deps",           "target",   ALIAS_DEPS_TARGET,    2,       "Usage: stow-manager deps target <pkg> <path>",                    cmd_deps_target},

    {"ignore",         "init",     ALIAS_IGNORE_INIT,    0,       "Usage: stow-manager ignore init [pkg...]",                        cmd_ignore_init},
    {"ignore",         "add",      ALIAS_IGNORE_ADD,     1,       "Usage: stow-manager ignore add [pkg] <pattern...>",               cmd_ignore_add},
    {"ignore",         "remove",   ALIAS_IGNORE_REMOVE,  1,       "Usage: stow-manager ignore remove [pkg] <pattern...>",            cmd_ignore_remove},
    {"ignore",         "show",     ALIAS_IGNORE_SHOW,    0,       "Usage: stow-manager ignore show [pkg...]",                        cmd_ignore_show},
    {"ignore",         "clear",    ALIAS_IGNORE_CLEAR,   0,       "Usage: stow-manager ignore clear [pkg...]",                       cmd_ignore_clear},

    {"config",         "show",     ALIAS_CONFIG_SHOW,    0,       "Usage: stow-manager config show",                                 cmd_config_show},
    {"config",         "set",      ALIAS_CONFIG_SET,     2,       "Usage: stow-manager config set <target|dotfiles> <path>",         cmd_config_set},
    {"config",         "add",      ALIAS_CONFIG_ADD,     1,       "Usage: stow-manager config add <path>",                           cmd_config_add},
    {"config",         "remove",   ALIAS_CONFIG_REMOVE,  1,       "Usage: stow-manager config remove <path>",                        cmd_config_remove},

    {"help",           NULL,       ALIAS_HELP,           0,       "Usage: stow-manager help",                                        cmd_help},
    {NULL,             NULL,       NULL,                 0,       NULL,                                                              NULL}
};
// clang-format on
