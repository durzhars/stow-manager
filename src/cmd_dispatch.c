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

#include "cmd_dispatch.h"
#include "cmd_routes.h"

#include <stdio.h>
#include <string.h>

#include "checker.h"
#include "config.h"
#include "help.h"
#include "ignore.h"
#include "manifest.h"
#include "scanner.h"
#include "stow.h"

#include "logger.h"
#include "utils/defs.h"
#include "utils/fs.h"
#include "utils/path.h"

typedef int (*PackageActionFn)(const char *dotfiles_dir,
                               const char *target_dir,
                               const char *pkg_name,
                               const CommandContext *ctx);

static int foreach_package(const CommandContext *ctx, PackageActionFn action)
{
    int status = 0;
    for (size_t i = ctx->arg_offset; i < ctx->args->count; i++) {
        const char *pkg_name = ctx->args->items[i];
        char target_dir[PATH_MAX * 2];
        get_active_target_dir_for_pkg(
            ctx->opts->cli_target_dir, ctx->dotfiles_dir, pkg_name, target_dir, sizeof(target_dir));
        int res = action(ctx->dotfiles_dir, target_dir, pkg_name, ctx);
        if (res != 0) {
            status = res;
        }
    }
    return status;
}

// Helper to disambiguate package vs. pattern arguments dynamically
static bool parse_ignore_args(const CommandContext *ctx,
                              const char **out_pkg,
                              const char *const **out_patterns,
                              size_t *out_count)
{
    *out_pkg = NULL;
    *out_patterns = NULL;
    *out_count = 0;

    if (ctx->args->count <= ctx->arg_offset) {
        return false;
    }

    size_t pattern_start = ctx->arg_offset;
    bool force_global = false;

    // Support -g / --global flag override
    const char *first_arg = ctx->args->items[pattern_start];
    if (strcmp(first_arg, "-g") == 0 || strcmp(first_arg, "--global") == 0) {
        force_global = true;
        pattern_start++;
        if (pattern_start >= ctx->args->count) {
            log_error("Option '%s' specified, but no patterns provided!", first_arg);
            return false;
        }
        first_arg = ctx->args->items[pattern_start];
    }

    if (!force_global) {
        char candidate_pkg[PATH_MAX * 2];
        join_path(candidate_pkg, sizeof(candidate_pkg), ctx->dotfiles_dir, first_arg);

        if (is_dir(candidate_pkg)) {
            if (ctx->args->count > pattern_start + 1) {
                *out_pkg = first_arg;
                pattern_start++;
            } else {
                const char *action_cmd =
                    (ctx->arg_offset >= 2) ? ctx->args->items[1] : ctx->args->items[0];

                log_error("Package '%s' specified, but no patterns provided!", first_arg);
                log_info("Hint: Use 'stow-manager %s %s %s <pattern...>' or "
                         "'stow-manager ignore -g %s'",
                         (ctx->arg_offset >= 2) ? "ignore" : "",
                         action_cmd,
                         first_arg,
                         first_arg);
                return false;
            }
        }
    }

    *out_count = ctx->args->count - pattern_start;
    if (*out_count > 0) {
        *out_patterns = (const char *const *)&ctx->args->items[pattern_start];
        return true;
    }

    return false;
}

/* Package Action Callbacks */
static int
action_stow(const char *dotfiles, const char *target, const char *pkg, const CommandContext *ctx)
{
    return stow_package(dotfiles, target, pkg, ctx->opts->auto_install, ctx->opts->dry_run);
}

static int
action_unstow(const char *dotfiles, const char *target, const char *pkg, const CommandContext *ctx)
{
    return unstow_package(dotfiles, target, pkg, ctx->opts->dry_run);
}

static int
action_restow(const char *dotfiles, const char *target, const char *pkg, const CommandContext *ctx)
{
    return restow_package(dotfiles, target, pkg, ctx->opts->auto_install, ctx->opts->dry_run);
}

static int
action_diff(const char *dotfiles, const char *target, const char *pkg, const CommandContext *ctx)
{
    return stow_package(dotfiles, target, pkg, ctx->opts->auto_install, true);
}

static int
action_remove(const char *dotfiles, const char *target, const char *pkg, const CommandContext *ctx)
{
    package_remove(dotfiles, target, pkg, ctx->opts->dry_run);
    return 0;
}

int cmd_stow(const CommandContext *ctx)
{
    return foreach_package(ctx, action_stow);
}

int cmd_unstow(const CommandContext *ctx)
{
    return foreach_package(ctx, action_unstow);
}

int cmd_restow(const CommandContext *ctx)
{
    return foreach_package(ctx, action_restow);
}

int cmd_all(const CommandContext *ctx)
{
    stow_all_packages(
        ctx->dotfiles_dir, ctx->global_target_dir, ctx->opts->auto_install, ctx->opts->dry_run);
    return 0;
}

int cmd_diff(const CommandContext *ctx)
{
    if (ctx->args->count > ctx->arg_offset) {
        return foreach_package(ctx, action_diff);
    }
    stow_all_packages(ctx->dotfiles_dir, ctx->global_target_dir, ctx->opts->auto_install, true);
    return 0;
}

int cmd_scan(const CommandContext *ctx)
{
    if (ctx->args->count > ctx->arg_offset) {
        for (size_t i = ctx->arg_offset; i < ctx->args->count; i++) {
            scan_package(ctx->dotfiles_dir, ctx->args->items[i]);
        }
    } else {
        StringArray pkgs;
        str_array_init(&pkgs);
        get_all_packages(ctx->dotfiles_dir, &pkgs);
        for (size_t i = 0; i < pkgs.count; i++) {
            scan_package(ctx->dotfiles_dir, pkgs.items[i]);
        }
        str_array_free(&pkgs);
    }
    return 0;
}

int cmd_check(const CommandContext *ctx)
{
    if (ctx->args->count > ctx->arg_offset &&
        strcmp(ctx->args->items[ctx->arg_offset], "symlinks") == 0) {
        check_symlink_health(ctx->dotfiles_dir, ctx->global_target_dir);
        return 0;
    }

    if (ctx->args->count > ctx->arg_offset) {
        for (size_t i = ctx->arg_offset; i < ctx->args->count; i++) {
            check_package_dependencies(ctx->dotfiles_dir,
                                       ctx->args->items[i],
                                       ctx->opts->auto_install,
                                       ctx->opts->dry_run);
        }
    } else {
        check_package_dependencies(
            ctx->dotfiles_dir, NULL, ctx->opts->auto_install, ctx->opts->dry_run);
    }
    check_symlink_health(ctx->dotfiles_dir, ctx->global_target_dir);
    return 0;
}

int cmd_check_symlinks(const CommandContext *ctx)
{
    check_symlink_health(ctx->dotfiles_dir, ctx->global_target_dir);
    return 0;
}

int cmd_fix_conflicts(const CommandContext *ctx)
{
    unfold_directory_symlinks(ctx->global_target_dir, ctx->dotfiles_dir, ctx->opts->dry_run);
    return 0;
}

int cmd_pkg_create(const CommandContext *ctx)
{
    const char *pkg = ctx->args->items[ctx->arg_offset];
    PackageManifest manifest;
    manifest_init(&manifest, pkg);
    manifest_save(&manifest, ctx->dotfiles_dir);
    log_success("Created package directory & manifest for '%s'.", pkg);
    manifest_free(&manifest);
    return 0;
}

int cmd_pkg_remove(const CommandContext *ctx)
{
    return foreach_package(ctx, action_remove);
}

int cmd_pkg_list(const CommandContext *ctx)
{
    list_packages_status(ctx->dotfiles_dir, ctx->global_target_dir);
    return 0;
}

int cmd_deps_add(const CommandContext *ctx)
{
    const char *pkg = ctx->args->items[ctx->arg_offset];
    const char *dep = ctx->args->items[ctx->arg_offset + 1];
    const char *type = (ctx->args->count > ctx->arg_offset + 2)
                           ? ctx->args->items[ctx->arg_offset + 2]
                           : "--optional";
    manifest_add_dep(ctx->dotfiles_dir, pkg, dep, type);
    return 0;
}

int cmd_deps_edit(const CommandContext *ctx)
{
    manifest_edit_dep(ctx->dotfiles_dir,
                      ctx->args->items[ctx->arg_offset],
                      ctx->args->items[ctx->arg_offset + 1],
                      ctx->args->items[ctx->arg_offset + 2]);
    return 0;
}

int cmd_deps_remove(const CommandContext *ctx)
{
    manifest_remove_dep(ctx->dotfiles_dir,
                        ctx->args->items[ctx->arg_offset],
                        ctx->args->items[ctx->arg_offset + 1]);
    return 0;
}

int cmd_deps_show(const CommandContext *ctx)
{
    manifest_show(ctx->dotfiles_dir, ctx->args->items[ctx->arg_offset]);
    return 0;
}

int cmd_deps_target(const CommandContext *ctx)
{
    manifest_set_target(ctx->dotfiles_dir,
                        ctx->args->items[ctx->arg_offset],
                        ctx->args->items[ctx->arg_offset + 1]);
    return 0;
}

int cmd_ignore_init(const CommandContext *ctx)
{
    size_t count = ctx->args->count - ctx->arg_offset;
    const char *const *pkgs =
        (count > 0) ? (const char *const *)&ctx->args->items[ctx->arg_offset] : NULL;
    ignore_init(ctx->dotfiles_dir, pkgs, count);
    return 0;
}

int cmd_ignore_add(const CommandContext *ctx)
{
    const char *pkg = NULL;
    const char *const *patterns = NULL;
    size_t count = 0;

    if (!parse_ignore_args(ctx, &pkg, &patterns, &count)) {
        if (!pkg)
            log_error("Usage: stow-manager ignore add [pkg] <pattern...>");
        return 1;
    }

    ignore_add_patterns(ctx->dotfiles_dir, pkg, patterns, count);
    return 0;
}

int cmd_ignore_remove(const CommandContext *ctx)
{
    const char *pkg = NULL;
    const char *const *patterns = NULL;
    size_t count = 0;

    if (!parse_ignore_args(ctx, &pkg, &patterns, &count)) {
        if (!pkg)
            log_error("Usage: stow-manager ignore remove [pkg] <pattern...>");
        return 1;
    }

    ignore_remove_patterns(ctx->dotfiles_dir, pkg, patterns, count);
    return 0;
}

int cmd_ignore_clear(const CommandContext *ctx)
{
    size_t count = ctx->args->count - ctx->arg_offset;
    const char *const *pkgs =
        (count > 0) ? (const char *const *)&ctx->args->items[ctx->arg_offset] : NULL;
    ignore_clear(ctx->dotfiles_dir, pkgs, count);
    return 0;
}

int cmd_ignore_show(const CommandContext *ctx)
{
    size_t count = ctx->args->count - ctx->arg_offset;
    const char *const *pkgs =
        (count > 0) ? (const char *const *)&ctx->args->items[ctx->arg_offset] : NULL;
    ignore_show(ctx->dotfiles_dir, pkgs, count);
    return 0;
}

int cmd_config_show(const CommandContext *ctx)
{
    (void)ctx;
    config_show();
    return 0;
}

int cmd_config_set(const CommandContext *ctx)
{
    const char *key = ctx->args->items[ctx->arg_offset];
    const char *val = ctx->args->items[ctx->arg_offset + 1];

    if (strcmp(key, "target") == 0) {
        config_set_target_dir(val);
    } else {
        config_set_dotfiles_dir(val);
    }
    return 0;
}

int cmd_config_add(const CommandContext *ctx)
{
    config_add_dotfiles_dir(ctx->args->items[ctx->arg_offset]);
    return 0;
}

int cmd_config_remove(const CommandContext *ctx)
{
    config_remove_dotfiles_dir(ctx->args->items[ctx->arg_offset]);
    return 0;
}

int cmd_help(const CommandContext *ctx)
{
    (void)ctx;
    show_help();
    return 0;
}

static void print_general_help_hint(void)
{
    log_info("Hint: Pass -h or help for information.");
}

static bool is_known_group(const char *group)
{
    if (!group) {
        return false;
    }
    for (size_t i = 0; ROUTE_TABLE[i].handler != NULL; i++) {
        if (strcmp(group, ROUTE_TABLE[i].group) == 0) {
            return true;
        }
    }
    return false;
}

static void print_group_usage_help(const char *group)
{
    log_error("Missing or invalid subcommand for command group '%s'.", group);
    printf("  %sAvailable subcommands for '%s':%s\n", COLOR_BOLD, group, COLOR_RESET);
    for (size_t i = 0; ROUTE_TABLE[i].handler != NULL; i++) {
        if (strcmp(group, ROUTE_TABLE[i].group) == 0 && ROUTE_TABLE[i].subcommand != NULL) {
            if (ROUTE_TABLE[i].usage) {
                printf("    %s%s%s\n", COLOR_CYAN, ROUTE_TABLE[i].usage, COLOR_RESET);
            } else {
                printf("    %sstow-manager %s %s%s\n",
                       COLOR_CYAN,
                       group,
                       ROUTE_TABLE[i].subcommand,
                       COLOR_RESET);
            }
        }
    }
    printf("\n");
    print_general_help_hint();
}

int dispatch_command(const StringArray *args, const CliOptions *opts)
{
    if (!args || args->count == 0) {
        log_error("Invalid arguments.");
        print_general_help_hint();
        return 1;
    }

    const char *token1 = args->items[0];
    const char *token2 = (args->count > 1) ? args->items[1] : NULL;

    for (size_t i = 0; ROUTE_TABLE[i].handler != NULL; i++) {
        const CommandRoute *route = &ROUTE_TABLE[i];
        bool matched = false;
        size_t consumed_tokens = 0;

        // Check group / subcommand space-separated matching
        if (strcmp(token1, route->group) == 0) {
            if (route->subcommand != NULL) {
                if (token2 && strcmp(token2, route->subcommand) == 0) {
                    matched = true;
                    consumed_tokens = 2;
                }
            } else {
                matched = true;
                consumed_tokens = 1;
            }
        }

        if (!matched && route->aliases) {
            for (size_t a = 0; route->aliases[a] != NULL; a++) {
                if (strcmp(token1, route->aliases[a]) == 0) {
                    matched = true;
                    consumed_tokens = 1;
                    break;
                }
            }
        }

        if (matched) {
            size_t sub_args = args->count - consumed_tokens;
            if (sub_args < route->min_args) {
                log_error("%s", route->usage ? route->usage : "Insufficient arguments!");
                return 1;
            }

            char dotfiles_dir[PATH_MAX * 2] = {0};
            char global_target_dir[PATH_MAX * 2] = {0};

            if (strcmp(route->group, "config") != 0 && strcmp(route->group, "help") != 0) {
                get_active_dotfiles_dir(opts->cli_dotfiles_dir, dotfiles_dir, sizeof(dotfiles_dir));
                get_active_target_dir(
                    opts->cli_target_dir, global_target_dir, sizeof(global_target_dir));
            }

            CommandContext ctx = {.opts = opts,
                                  .dotfiles_dir = dotfiles_dir,
                                  .global_target_dir = global_target_dir,
                                  .args = args,
                                  .arg_offset = consumed_tokens};

            return route->handler(&ctx);
        }
    }

    if (is_known_group(token1)) {
        print_group_usage_help(token1);
        return 1;
    }

    char dotfiles_dir[PATH_MAX * 2] = {0};
    char global_target_dir[PATH_MAX * 2] = {0};
    get_active_dotfiles_dir(opts->cli_dotfiles_dir, dotfiles_dir, sizeof(dotfiles_dir));
    get_active_target_dir(opts->cli_target_dir, global_target_dir, sizeof(global_target_dir));

    bool all_valid = true;
    for (size_t i = 0; i < args->count; i++) {
        char full_pkg_path[PATH_MAX * 2];
        join_path(full_pkg_path, sizeof(full_pkg_path), dotfiles_dir, args->items[i]);
        if (!is_dir(full_pkg_path)) {
            all_valid = false;
            break;
        }
    }

    if (all_valid) {
        CommandContext ctx = {.opts = opts,
                              .dotfiles_dir = dotfiles_dir,
                              .global_target_dir = global_target_dir,
                              .args = args,
                              .arg_offset = 0};
        return cmd_stow(&ctx);
    }

    log_error("Unknown command: %s", token1);
    print_general_help_hint();
    return 1;
}
