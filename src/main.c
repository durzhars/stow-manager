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
#include <getopt.h>

#include "config.h"
#include "help.h"
#include "scanner.h"
#include "stow.h"

static void handle_config_command(const StringArray *args)
{
    const char *cmd = args->items[0];
    size_t remaining = args->count - 1;

    if (strcmp(cmd, "config:show") == 0 || strcmp(cmd, "config:list") == 0 ||
        strcmp(cmd, "config:get") == 0) {
        config_show();
        return;
    }

    if (strcmp(cmd, "config:target") == 0) {
        if (remaining < 1) {
            log_error("Usage: stow-manager config set target <path>");
            return;
        }
        config_set_target_dir(args->items[1]);
        return;
    }

    if (strcmp(cmd, "config:add") == 0) {
        if (remaining < 1) {
            log_error("Usage: stow-manager config add <path>");
            return;
        }
        config_add_dotfiles_dir(args->items[1]);
        return;
    }

    if (strcmp(cmd, "config:remove") == 0 || strcmp(cmd, "config:rm") == 0) {
        if (remaining < 1) {
            log_error("Usage: stow-manager config remove <path>");
            return;
        }
        config_remove_dotfiles_dir(args->items[1]);
        return;
    }

    if (strcmp(cmd, "config:set") == 0 || strcmp(cmd, "config") == 0) {
        if (remaining == 0) {
            config_show();
            return;
        }

        const char *sub = args->items[1];

        if (strcmp(sub, "show") == 0 || strcmp(sub, "list") == 0 || strcmp(sub, "get") == 0) {
            config_show();
            return;
        }

        if (strcmp(sub, "set") == 0) {
            if (remaining < 2) {
                log_error("Usage: stow-manager config set <target|source|dotfiles> <path>");
                return;
            }
            const char *key_or_path = args->items[2];
            if (strcmp(key_or_path, "target") == 0) {
                if (remaining < 3) {
                    log_error("Usage: stow-manager config set target <path>");
                    return;
                }
                config_set_target_dir(args->items[3]);
            } else if (strcmp(key_or_path, "dotfiles") == 0 || strcmp(key_or_path, "source") == 0 ||
                       strcmp(key_or_path, "repo") == 0) {
                if (remaining < 3) {
                    log_error("Usage: stow-manager config set dotfiles <path>");
                    return;
                }
                config_set_dotfiles_dir(args->items[3]);
            } else {
                config_set_dotfiles_dir(key_or_path);
            }
            return;
        }

        if (strcmp(sub, "add") == 0) {
            if (remaining < 2) {
                log_error("Usage: stow-manager config add <path>");
                return;
            }
            const char *key_or_path = args->items[2];
            if (strcmp(key_or_path, "dotfiles") == 0 || strcmp(key_or_path, "repo") == 0 ||
                strcmp(key_or_path, "source") == 0) {
                if (remaining < 3) {
                    log_error("Usage: stow-manager config add <path>");
                    return;
                }
                config_add_dotfiles_dir(args->items[3]);
            } else {
                config_add_dotfiles_dir(key_or_path);
            }
            return;
        }

        if (strcmp(sub, "remove") == 0 || strcmp(sub, "rm") == 0) {
            if (remaining < 2) {
                log_error("Usage: stow-manager config remove <path>");
                return;
            }
            const char *key_or_path = args->items[2];
            if (strcmp(key_or_path, "dotfiles") == 0 || strcmp(key_or_path, "repo") == 0 ||
                strcmp(key_or_path, "source") == 0) {
                if (remaining < 3) {
                    log_error("Usage: stow-manager config remove <path>");
                    return;
                }
                config_remove_dotfiles_dir(args->items[3]);
            } else {
                config_remove_dotfiles_dir(key_or_path);
            }
            return;
        }

        config_show();
        return;
    }
}

int main(int argc, char **argv)
{
    setup_signal_handlers();

    bool auto_install = false;
    bool dry_run = false;
    bool save_flag = false;
    const char *cli_dotfiles_dir = NULL;
    const char *cli_target_dir = NULL;

    StringArray args;
    str_array_init(&args);

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--dotfiles-dir") == 0) {
            if (i + 1 < argc) {
                cli_dotfiles_dir = argv[++i];
            } else {
                log_error("Option '%s' requires a directory path argument", argv[i]);
                str_array_free(&args);
                return 1;
            }
        } else if (strncmp(argv[i], "--dotfiles-dir=", 15) == 0) {
            cli_dotfiles_dir = argv[i] + 15;
        } else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--target-dir") == 0) {
            if (i + 1 < argc) {
                cli_target_dir = argv[++i];
            } else {
                log_error("Option '%s' requires a directory path argument", argv[i]);
                str_array_free(&args);
                return 1;
            }
        } else if (strncmp(argv[i], "--target-dir=", 13) == 0) {
            cli_target_dir = argv[i] + 13;
        } else if (strcmp(argv[i], "-y") == 0 || strcmp(argv[i], "--install") == 0) {
            auto_install = true;
        } else if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--dry-run") == 0) {
            dry_run = true;
        } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--save") == 0) {
            save_flag = true;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            show_help();
            str_array_free(&args);
            return 0;
        } else {
            str_array_append(&args, argv[i]);
        }
    }

    if (args.count == 0) {
        show_help();
        str_array_free(&args);
        return 0;
    }

    // Option C: Save CLI directory overrides to configuration file when -s / --save is passed
    if (save_flag) {
        if (cli_target_dir && strlen(cli_target_dir) > 0) {
            config_set_target_dir(cli_target_dir);
            log_info("Saved target directory override to config: %s", cli_target_dir);
        }
        if (cli_dotfiles_dir && strlen(cli_dotfiles_dir) > 0) {
            config_set_dotfiles_dir(cli_dotfiles_dir);
            log_info("Saved dotfiles directory override to config: %s", cli_dotfiles_dir);
        }
    }

    const char *cmd = args.items[0];

    if (strncmp(cmd, "config", 6) == 0) {
        handle_config_command(&args);
        str_array_free(&args);
        return 0;
    }

    char dotfiles_dir[PATH_MAX * 2];
    char global_target_dir[PATH_MAX * 2];

    get_active_dotfiles_dir(cli_dotfiles_dir, dotfiles_dir, sizeof(dotfiles_dir));
    get_active_target_dir(cli_target_dir, global_target_dir, sizeof(global_target_dir));

    // Handle space-separated subcommands: "pkg create", "deps add", "check symlinks", "fix
    // conflicts"
    if (strcmp(cmd, "pkg") == 0 || strcmp(cmd, "package") == 0) {
        if (args.count < 2) {
            log_error("Usage: stow-manager pkg <create|remove|list> [args...]");
            str_array_free(&args);
            return 1;
        }
        const char *sub = args.items[1];
        if (strcmp(sub, "create") == 0 || strcmp(sub, "make") == 0) {
            if (args.count < 3) {
                log_error("Usage: stow-manager pkg create <package_name>");
                str_array_free(&args);
                return 1;
            }
            const char *pkg = args.items[2];
            PackageManifest manifest;
            manifest_init(&manifest, pkg);
            manifest_save(&manifest, dotfiles_dir);
            log_success("Created package directory & manifest for '%s'.", pkg);
            manifest_free(&manifest);
            str_array_free(&args);
            return 0;
        } else if (strcmp(sub, "remove") == 0 || strcmp(sub, "rm") == 0) {
            if (args.count < 3) {
                log_error("Usage: stow-manager pkg remove <package_name...>");
                str_array_free(&args);
                return 1;
            }
            for (size_t i = 2; i < args.count; i++) {
                char target_dir[PATH_MAX * 2];
                get_active_target_dir_for_pkg(
                    cli_target_dir, dotfiles_dir, args.items[i], target_dir, sizeof(target_dir));
                package_remove(dotfiles_dir, target_dir, args.items[i], dry_run);
            }
            str_array_free(&args);
            return 0;
        } else if (strcmp(sub, "list") == 0 || strcmp(sub, "show") == 0) {
            list_packages_status(dotfiles_dir, global_target_dir);
            str_array_free(&args);
            return 0;
        }
    }

    if (strcmp(cmd, "deps") == 0) {
        if (args.count < 2) {
            log_error("Usage: stow-manager deps <add|edit|remove|show|target> [args...]");
            str_array_free(&args);
            return 1;
        }
        const char *sub = args.items[1];
        if (strcmp(sub, "add") == 0) {
            if (args.count < 4) {
                log_error("Usage: stow-manager deps add <package> <dependency> "
                          "[--required|--optional|--conflict]");
                str_array_free(&args);
                return 1;
            }
            const char *pkg = args.items[2];
            const char *dep = args.items[3];
            const char *type = (args.count > 4) ? args.items[4] : "--optional";
            manifest_add_dep(dotfiles_dir, pkg, dep, type);
            str_array_free(&args);
            return 0;
        } else if (strcmp(sub, "edit") == 0 || strcmp(sub, "set") == 0) {
            if (args.count < 5) {
                log_error("Usage: stow-manager deps edit <package> <dependency> <new_type>");
                str_array_free(&args);
                return 1;
            }
            manifest_edit_dep(dotfiles_dir, args.items[2], args.items[3], args.items[4]);
            str_array_free(&args);
            return 0;
        } else if (strcmp(sub, "remove") == 0 || strcmp(sub, "rm") == 0) {
            if (args.count < 4) {
                log_error("Usage: stow-manager deps remove <package> <dependency>");
                str_array_free(&args);
                return 1;
            }
            manifest_remove_dep(dotfiles_dir, args.items[2], args.items[3]);
            str_array_free(&args);
            return 0;
        } else if (strcmp(sub, "show") == 0 || strcmp(sub, "list") == 0) {
            if (args.count < 3) {
                log_error("Usage: stow-manager deps show <package>");
                str_array_free(&args);
                return 1;
            }
            manifest_show(dotfiles_dir, args.items[2]);
            str_array_free(&args);
            return 0;
        } else if (strcmp(sub, "target") == 0) {
            if (args.count < 4) {
                log_error("Usage: stow-manager deps target <package> <path>");
                str_array_free(&args);
                return 1;
            }
            manifest_set_target(dotfiles_dir, args.items[2], args.items[3]);
            str_array_free(&args);
            return 0;
        }
    }

    if (strcmp(cmd, "deps:add") == 0) {
        if (args.count < 3) {
            log_error("Usage: stow-manager deps add <package> <dependency> "
                      "[--required|--optional|--conflict]");
            str_array_free(&args);
            return 1;
        }
        const char *pkg = args.items[1];
        const char *dep = args.items[2];
        const char *type = (args.count > 3) ? args.items[3] : "--optional";
        manifest_add_dep(dotfiles_dir, pkg, dep, type);
    } else if (strcmp(cmd, "deps:edit") == 0 || strcmp(cmd, "deps:set") == 0) {
        if (args.count < 4) {
            log_error("Usage: stow-manager deps edit <package> <dependency> <new_type>");
            str_array_free(&args);
            return 1;
        }
        manifest_edit_dep(dotfiles_dir, args.items[1], args.items[2], args.items[3]);
    } else if (strcmp(cmd, "deps:remove") == 0 || strcmp(cmd, "deps:rm") == 0) {
        if (args.count < 3) {
            log_error("Usage: stow-manager deps remove <package> <dependency>");
            str_array_free(&args);
            return 1;
        }
        manifest_remove_dep(dotfiles_dir, args.items[1], args.items[2]);
    } else if (strcmp(cmd, "deps:show") == 0 || strcmp(cmd, "deps:list") == 0) {
        if (args.count < 2) {
            log_error("Usage: stow-manager deps show <package>");
            str_array_free(&args);
            return 1;
        }
        manifest_show(dotfiles_dir, args.items[1]);
    } else if (strcmp(cmd, "deps:target") == 0) {
        if (args.count < 3) {
            log_error("Usage: stow-manager deps target <package> <path>");
            str_array_free(&args);
            return 1;
        }
        manifest_set_target(dotfiles_dir, args.items[1], args.items[2]);
    } else if (strcmp(cmd, "pkg:create") == 0 || strcmp(cmd, "package:create") == 0 ||
               strcmp(cmd, "make:package") == 0 || strcmp(cmd, "make:pkg") == 0) {
        if (args.count < 2) {
            log_error("Usage: stow-manager pkg create <package_name>");
            str_array_free(&args);
            return 1;
        }
        const char *pkg = args.items[1];
        PackageManifest manifest;
        manifest_init(&manifest, pkg);
        manifest_save(&manifest, dotfiles_dir);
        log_success("Created package directory & manifest for '%s'.", pkg);
        manifest_free(&manifest);
    } else if (strcmp(cmd, "pkg:remove") == 0 || strcmp(cmd, "package:remove") == 0 ||
               strcmp(cmd, "remove:package") == 0 || strcmp(cmd, "pkg:rm") == 0 ||
               strcmp(cmd, "package:rm") == 0) {
        if (args.count < 2) {
            log_error("Usage: stow-manager pkg remove <package_name...>");
            str_array_free(&args);
            return 1;
        }
        for (size_t i = 1; i < args.count; i++) {
            char target_dir[PATH_MAX * 2];
            get_active_target_dir_for_pkg(
                cli_target_dir, dotfiles_dir, args.items[i], target_dir, sizeof(target_dir));
            package_remove(dotfiles_dir, target_dir, args.items[i], dry_run);
        }
    } else if (strcmp(cmd, "pkg:list") == 0 || strcmp(cmd, "package:list") == 0 ||
               strcmp(cmd, "list") == 0) {
        list_packages_status(dotfiles_dir, global_target_dir);
    } else if (strcmp(cmd, "check") == 0) {
        if (args.count > 1 && strcmp(args.items[1], "symlinks") == 0) {
            check_symlink_health(dotfiles_dir, global_target_dir);
        } else if (args.count > 1) {
            for (size_t i = 1; i < args.count; i++) {
                check_package_dependencies(dotfiles_dir, args.items[i], auto_install, dry_run);
            }
            check_symlink_health(dotfiles_dir, global_target_dir);
        } else {
            check_package_dependencies(dotfiles_dir, NULL, auto_install, dry_run);
            check_symlink_health(dotfiles_dir, global_target_dir);
        }
    } else if (strcmp(cmd, "check-symlinks") == 0) {
        check_symlink_health(dotfiles_dir, global_target_dir);
    } else if (strcmp(cmd, "diff") == 0) {
        if (args.count > 1) {
            for (size_t i = 1; i < args.count; i++) {
                char target_dir[PATH_MAX * 2];
                get_active_target_dir_for_pkg(
                    cli_target_dir, dotfiles_dir, args.items[i], target_dir, sizeof(target_dir));
                stow_package(dotfiles_dir, target_dir, args.items[i], auto_install, true);
            }
        } else {
            stow_all_packages(dotfiles_dir, global_target_dir, auto_install, true);
        }
    } else if (strcmp(cmd, "scan") == 0) {
        if (args.count > 1) {
            for (size_t i = 1; i < args.count; i++) {
                scan_package(dotfiles_dir, args.items[i]);
            }
        } else {
            StringArray pkgs;
            str_array_init(&pkgs);
            get_all_packages(dotfiles_dir, &pkgs);
            for (size_t i = 0; i < pkgs.count; i++) {
                scan_package(dotfiles_dir, pkgs.items[i]);
            }
            str_array_free(&pkgs);
        }
    } else if (strcmp(cmd, "stow") == 0) {
        if (args.count < 2) {
            log_error("Please specify at least one package name to stow!");
            str_array_free(&args);
            return 1;
        }
        for (size_t i = 1; i < args.count; i++) {
            char target_dir[PATH_MAX * 2];
            get_active_target_dir_for_pkg(
                cli_target_dir, dotfiles_dir, args.items[i], target_dir, sizeof(target_dir));
            stow_package(dotfiles_dir, target_dir, args.items[i], auto_install, dry_run);
        }
    } else if (strcmp(cmd, "unstow") == 0) {
        if (args.count < 2) {
            log_error("Please specify at least one package name to unstow!");
            str_array_free(&args);
            return 1;
        }
        for (size_t i = 1; i < args.count; i++) {
            char target_dir[PATH_MAX * 2];
            get_active_target_dir_for_pkg(
                cli_target_dir, dotfiles_dir, args.items[i], target_dir, sizeof(target_dir));
            unstow_package(dotfiles_dir, target_dir, args.items[i], dry_run);
        }
    } else if (strcmp(cmd, "restow") == 0) {
        if (args.count < 2) {
            log_error("Please specify at least one package name to restow!");
            str_array_free(&args);
            return 1;
        }
        for (size_t i = 1; i < args.count; i++) {
            char target_dir[PATH_MAX * 2];
            get_active_target_dir_for_pkg(
                cli_target_dir, dotfiles_dir, args.items[i], target_dir, sizeof(target_dir));
            restow_package(dotfiles_dir, target_dir, args.items[i], auto_install, dry_run);
        }
    } else if (strcmp(cmd, "fix") == 0 && args.count > 1 &&
               (strcmp(args.items[1], "conflicts") == 0 ||
                strcmp(args.items[1], "symlinks") == 0)) {
        unfold_directory_symlinks(global_target_dir, dotfiles_dir, dry_run);
    } else if (strcmp(cmd, "fix-conflicts") == 0) {
        unfold_directory_symlinks(global_target_dir, dotfiles_dir, dry_run);
    } else if (strcmp(cmd, "all") == 0) {
        stow_all_packages(dotfiles_dir, global_target_dir, auto_install, dry_run);
    } else if (strcmp(cmd, "help") == 0) {
        show_help();
    } else {
        bool all_valid = true;
        for (size_t i = 0; i < args.count; i++) {
            char pkg_dir[PATH_MAX * 2];
            join_path(pkg_dir, sizeof(pkg_dir), dotfiles_dir, args.items[i]);
            if (!is_dir(pkg_dir)) {
                all_valid = false;
                break;
            }
        }
        if (all_valid) {
            for (size_t i = 0; i < args.count; i++) {
                char target_dir[PATH_MAX * 2];
                get_active_target_dir_for_pkg(
                    cli_target_dir, dotfiles_dir, args.items[i], target_dir, sizeof(target_dir));
                stow_package(dotfiles_dir, target_dir, args.items[i], auto_install, dry_run);
            }
        } else {
            log_error("Unknown command: %s", cmd);
            show_help();
            str_array_free(&args);
            return 1;
        }
    }

    str_array_free(&args);
    return 0;
}
