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
#include "stow.h"
#include "scanner.h"
#include "config.h"

static void show_help(const char *prog_name) {
    printf("%sDotfiles Stow Manager (stow-manager)%s\n", COLOR_BOLD, COLOR_RESET);
    printf("Usage: %s [options] <command> [arguments]\n\n", prog_name);
    printf("Options:\n");
    printf("  %s-d, --dotfiles-dir%s <path>        Set dotfiles repository directory for current command\n", COLOR_CYAN, COLOR_RESET);
    printf("  %s-t, --target-dir%s <path>          Set target home directory for current command\n", COLOR_CYAN, COLOR_RESET);
    printf("  %s-y, --install%s                  Auto-confirm installation of missing dependencies/plugins\n", COLOR_CYAN, COLOR_RESET);
    printf("  %s-n, --dry-run%s                  Dry-run mode (preview changes without modifying disk)\n", COLOR_CYAN, COLOR_RESET);
    printf("  %s-h, --help%s                     Show this help menu\n\n", COLOR_CYAN, COLOR_RESET);
    printf("Configuration Commands:\n");
    printf("  %sconfig show%s                      Display current configuration settings & paths\n", COLOR_CYAN, COLOR_RESET);
    printf("  %sconfig set%s [dotfiles|target] <path> Set primary dotfiles/target directory in config file\n", COLOR_CYAN, COLOR_RESET);
    printf("  %sconfig add%s <path>                Add an additional dotfiles repository directory\n", COLOR_CYAN, COLOR_RESET);
    printf("  %sconfig remove%s <path>             Remove a dotfiles repository directory from config\n\n", COLOR_CYAN, COLOR_RESET);
    printf("Dependency Management Commands:\n");
    printf("  %sdeps:add%s <pkg> <dep> [--opt]   Add a dependency/conflict to package manifest\n", COLOR_CYAN, COLOR_RESET);
    printf("  %sdeps:remove%s <pkg> <dep>        Remove a dependency from package manifest\n", COLOR_CYAN, COLOR_RESET);
    printf("  %sdeps:show%s <pkg>               Display package manifest contents\n", COLOR_CYAN, COLOR_RESET);
    printf("  %smake:package%s <name>            Scaffold a new Stow package directory & manifest\n\n", COLOR_CYAN, COLOR_RESET);
    printf("Package & Stow Operations:\n");
    printf("  %scheck%s [pkg]                    Detect missing dependencies, plugins & broken symlinks\n", COLOR_CYAN, COLOR_RESET);
    printf("  %scheck-symlinks%s                 Scan for broken repo symlinks & unmanaged target symlinks\n", COLOR_CYAN, COLOR_RESET);
    printf("  %sdiff%s [pkg]                     Preview symlink creations, conflict backups, and missing deps\n", COLOR_CYAN, COLOR_RESET);
    printf("  %sscan%s [pkg]                     Recursively scan package files to auto-detect dependencies\n", COLOR_CYAN, COLOR_RESET);
    printf("  %slist%s                           List all packages and stowed status\n", COLOR_CYAN, COLOR_RESET);
    printf("  %sstow%s <pkg>                     Stow a package with auto conflict resolution\n", COLOR_CYAN, COLOR_RESET);
    printf("  %sunstow%s <pkg>                   Unstow a package\n", COLOR_CYAN, COLOR_RESET);
    printf("  %srestow%s <pkg>                   Restow a package\n", COLOR_CYAN, COLOR_RESET);
    printf("  %sfix-conflicts%s                  Unfold directory symlinks & resolve conflicts\n", COLOR_CYAN, COLOR_RESET);
    printf("  %sall%s                            Stow all packages\n", COLOR_CYAN, COLOR_RESET);
    printf("  %shelp%s                           Show this help menu\n", COLOR_CYAN, COLOR_RESET);
}

static void handle_config_command(int argc, char **argv, int optind) {
    const char *cmd = argv[optind];
    int remaining = argc - optind - 1;

    if (strcmp(cmd, "config:show") == 0 || strcmp(cmd, "config:list") == 0 || strcmp(cmd, "config:get") == 0) {
        config_show();
        return;
    }

    if (strcmp(cmd, "config:target") == 0) {
        if (remaining < 1) {
            log_error("Usage: %s config target <path>", argv[0]);
            return;
        }
        config_set_target_dir(argv[optind + 1]);
        return;
    }

    if (strcmp(cmd, "config:add") == 0) {
        if (remaining < 1) {
            log_error("Usage: %s config add <path>", argv[0]);
            return;
        }
        config_add_dotfiles_dir(argv[optind + 1]);
        return;
    }

    if (strcmp(cmd, "config:remove") == 0 || strcmp(cmd, "config:rm") == 0) {
        if (remaining < 1) {
            log_error("Usage: %s config remove <path>", argv[0]);
            return;
        }
        config_remove_dotfiles_dir(argv[optind + 1]);
        return;
    }

    if (strcmp(cmd, "config:set") == 0 || strcmp(cmd, "config") == 0) {
        if (remaining == 0) {
            config_show();
            return;
        }

        const char *sub = argv[optind + 1];

        if (strcmp(sub, "show") == 0 || strcmp(sub, "list") == 0 || strcmp(sub, "get") == 0) {
            config_show();
            return;
        }

        if (strcmp(sub, "set") == 0) {
            if (remaining < 2) {
                log_error("Usage: %s config set [dotfiles|target] <path>", argv[0]);
                return;
            }
            const char *key_or_path = argv[optind + 2];
            if (strcmp(key_or_path, "target") == 0) {
                if (remaining < 3) {
                    log_error("Usage: %s config set target <path>", argv[0]);
                    return;
                }
                config_set_target_dir(argv[optind + 3]);
            } else if (strcmp(key_or_path, "dotfiles") == 0 || strcmp(key_or_path, "repo") == 0) {
                if (remaining < 3) {
                    log_error("Usage: %s config set dotfiles <path>", argv[0]);
                    return;
                }
                config_set_dotfiles_dir(argv[optind + 3]);
            } else {
                config_set_dotfiles_dir(key_or_path);
            }
            return;
        }

        if (strcmp(sub, "add") == 0) {
            if (remaining < 2) {
                log_error("Usage: %s config add <path>", argv[0]);
                return;
            }
            const char *key_or_path = argv[optind + 2];
            if (strcmp(key_or_path, "dotfiles") == 0 || strcmp(key_or_path, "repo") == 0) {
                if (remaining < 3) {
                    log_error("Usage: %s config add <path>", argv[0]);
                    return;
                }
                config_add_dotfiles_dir(argv[optind + 3]);
            } else {
                config_add_dotfiles_dir(key_or_path);
            }
            return;
        }

        if (strcmp(sub, "remove") == 0 || strcmp(sub, "rm") == 0) {
            if (remaining < 2) {
                log_error("Usage: %s config remove <path>", argv[0]);
                return;
            }
            const char *key_or_path = argv[optind + 2];
            if (strcmp(key_or_path, "dotfiles") == 0 || strcmp(key_or_path, "repo") == 0) {
                if (remaining < 3) {
                    log_error("Usage: %s config remove <path>", argv[0]);
                    return;
                }
                config_remove_dotfiles_dir(argv[optind + 3]);
            } else {
                config_remove_dotfiles_dir(key_or_path);
            }
            return;
        }

        if (strcmp(sub, "target") == 0) {
            if (remaining < 2) {
                log_error("Usage: %s config target <path>", argv[0]);
                return;
            }
            config_set_target_dir(argv[optind + 2]);
            return;
        }

        if (strcmp(sub, "dotfiles") == 0 || strcmp(sub, "repo") == 0) {
            if (remaining < 2) {
                log_error("Usage: %s config dotfiles <path>", argv[0]);
                return;
            }
            config_set_dotfiles_dir(argv[optind + 2]);
            return;
        }

        char abs_sub[PATH_MAX * 2];
        expand_tilde_path(sub, abs_sub, sizeof(abs_sub));
        normalize_path(abs_sub);
        if (is_dir(abs_sub)) {
            config_set_dotfiles_dir(abs_sub);
        } else {
            log_error("Unknown config subcommand or non-existent path '%s'", sub);
            log_info("Valid commands: config show, config set dotfiles <path>, config target <path>, config add <path>, config remove <path>");
        }
        return;
    }
}

int main(int argc, char **argv) {
    setup_signal_handlers();

    bool auto_install = false;
    bool dry_run = false;
    const char *cli_dotfiles_dir = NULL;
    const char *cli_target_dir = NULL;

    static struct option long_options[] = {
        {"dotfiles-dir", required_argument, 0, 'd'},
        {"target-dir",   required_argument, 0, 't'},
        {"install",      no_argument,       0, 'y'},
        {"dry-run",      no_argument,       0, 'n'},
        {"help",         no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    int option_index = 0;
    while ((opt = getopt_long(argc, argv, "d:t:ynh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'd':
                cli_dotfiles_dir = optarg;
                break;
            case 't':
                cli_target_dir = optarg;
                break;
            case 'y':
                auto_install = true;
                break;
            case 'n':
                dry_run = true;
                break;
            case 'h':
                show_help(argv[0]);
                return 0;
            default:
                break;
        }
    }

    if (optind >= argc) {
        show_help(argv[0]);
        return 0;
    }

    const char *cmd = argv[optind];

    if (strncmp(cmd, "config", 6) == 0) {
        handle_config_command(argc, argv, optind);
        return 0;
    }

    char dotfiles_dir[PATH_MAX * 2];
    char target_dir[PATH_MAX * 2];

    get_active_dotfiles_dir(cli_dotfiles_dir, dotfiles_dir, sizeof(dotfiles_dir));
    get_active_target_dir(cli_target_dir, target_dir, sizeof(target_dir));

    if (strcmp(cmd, "deps:add") == 0) {
        if (optind + 2 >= argc) {
            log_error("Usage: %s deps:add <package> <dependency> [--required|--optional|--conflict]", argv[0]);
            return 1;
        }
        const char *pkg = argv[optind + 1];
        const char *dep = argv[optind + 2];
        const char *type = (optind + 3 < argc) ? argv[optind + 3] : "--optional";
        manifest_add_dep(dotfiles_dir, pkg, dep, type);
    } else if (strcmp(cmd, "deps:remove") == 0 || strcmp(cmd, "deps:rm") == 0) {
        if (optind + 2 >= argc) {
            log_error("Usage: %s deps:remove <package> <dependency>", argv[0]);
            return 1;
        }
        manifest_remove_dep(dotfiles_dir, argv[optind + 1], argv[optind + 2]);
    } else if (strcmp(cmd, "deps:show") == 0 || strcmp(cmd, "deps:list") == 0) {
        if (optind + 1 >= argc) {
            log_error("Usage: %s deps:show <package>", argv[0]);
            return 1;
        }
        manifest_show(dotfiles_dir, argv[optind + 1]);
    } else if (strcmp(cmd, "make:package") == 0 || strcmp(cmd, "make:pkg") == 0) {
        if (optind + 1 >= argc) {
            log_error("Usage: %s make:package <package_name>", argv[0]);
            return 1;
        }
        const char *pkg = argv[optind + 1];
        PackageManifest manifest;
        manifest_init(&manifest, pkg);
        manifest_save(&manifest, dotfiles_dir);
        log_success("Created package directory & manifest for '%s'.", pkg);
        manifest_free(&manifest);
    } else if (strcmp(cmd, "check") == 0) {
        const char *pkg = (optind + 1 < argc) ? argv[optind + 1] : NULL;
        check_package_dependencies(dotfiles_dir, pkg, auto_install, dry_run);
        check_symlink_health(dotfiles_dir, target_dir);
    } else if (strcmp(cmd, "check-symlinks") == 0) {
        check_symlink_health(dotfiles_dir, target_dir);
    } else if (strcmp(cmd, "diff") == 0) {
        const char *pkg = (optind + 1 < argc) ? argv[optind + 1] : NULL;
        if (pkg) {
            stow_package(dotfiles_dir, target_dir, pkg, auto_install, true);
        } else {
            stow_all_packages(dotfiles_dir, target_dir, auto_install, true);
        }
    } else if (strcmp(cmd, "scan") == 0) {
        if (optind + 1 < argc) {
            scan_package(dotfiles_dir, argv[optind + 1]);
        } else {
            StringArray pkgs;
            str_array_init(&pkgs);
            get_all_packages(dotfiles_dir, &pkgs);
            for (size_t i = 0; i < pkgs.count; i++) {
                scan_package(dotfiles_dir, pkgs.items[i]);
            }
            str_array_free(&pkgs);
        }
    } else if (strcmp(cmd, "list") == 0) {
        list_packages_status(dotfiles_dir, target_dir);
    } else if (strcmp(cmd, "stow") == 0) {
        if (optind + 1 >= argc) {
            log_error("Please specify a package name to stow!");
            return 1;
        }
        stow_package(dotfiles_dir, target_dir, argv[optind + 1], auto_install, dry_run);
    } else if (strcmp(cmd, "unstow") == 0) {
        if (optind + 1 >= argc) {
            log_error("Please specify a package name to unstow!");
            return 1;
        }
        unstow_package(dotfiles_dir, target_dir, argv[optind + 1], dry_run);
    } else if (strcmp(cmd, "restow") == 0) {
        if (optind + 1 >= argc) {
            log_error("Please specify a package name to restow!");
            return 1;
        }
        restow_package(dotfiles_dir, target_dir, argv[optind + 1], auto_install, dry_run);
    } else if (strcmp(cmd, "fix-conflicts") == 0) {
        unfold_directory_symlinks(target_dir, dotfiles_dir, dry_run);
    } else if (strcmp(cmd, "all") == 0) {
        stow_all_packages(dotfiles_dir, target_dir, auto_install, dry_run);
    } else if (strcmp(cmd, "help") == 0) {
        show_help(argv[0]);
    } else {
        char pkg_dir[PATH_MAX * 2];
        join_path(pkg_dir, sizeof(pkg_dir), dotfiles_dir, cmd);
        if (is_dir(pkg_dir)) {
            stow_package(dotfiles_dir, target_dir, cmd, auto_install, dry_run);
        } else {
            log_error("Unknown command or package '%s'", cmd);
            show_help(argv[0]);
            return 1;
        }
    }

    return 0;
}
