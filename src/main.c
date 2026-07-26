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

static const char *EMBEDDED_HELP =
"# Dotfiles Stow Manager (`stow-manager`)\n\n"
"**Usage**: `stow-manager [options] <command> [arguments]`\n\n"
"High-performance ISO C17 dotfiles framework & Stow package manager. Auto-resolves package dependencies, mutual exclusion conflicts, directory symlink folding collisions, and multi-repository setups.\n\n"
"## Global Options\n\n"
"- **`-d, --dotfiles-dir`** `<path>` : Set dotfiles repository directory for current command (e.g. `-d ~/dotfiles`)\n"
"- **`-t, --target-dir`** `<path>`   : Set target home directory for current command (e.g. `-t ~/`)\n"
"- **`-y, --install`**             : Auto-confirm installation of missing required dependencies & optional plugins\n"
"- **`-n, --dry-run`**             : Dry-run mode (preview disk changes, symlink creations & backups without modifying disk)\n"
"- **`-h, --help`**                : Display this comprehensive help manual\n\n"
"## Configuration Commands (`config:*`)\n\n"
"- **`config show`**                        : Display active configuration, dotfiles repositories & target directory\n"
"- **`config set [dotfiles|target]`** `<path>` : Set primary dotfiles repository or target home directory\n"
"- **`config add`** `<path>`                  : Add an additional dotfiles repository directory (multi-repo mode)\n"
"- **`config remove`** `<path>`               : Remove a dotfiles repository directory from config\n\n"
"## Package Management Commands (`pkg:*`)\n\n"
"- **`pkg:create`** `<name>`             : Scaffold a new Stow package directory & initialize `.stowdeps` manifest (alias: `make:package`)\n"
"- **`pkg:remove`** `<name ...>`         : Safely unstow and remove one or multiple Stow package directories (alias: `remove:package`)\n"
"- **`pkg:list`**                       : List all packages with status: `[STOWED]`, `[PARTIAL]`, or `[UNSTOWED]` (alias: `list`)\n\n"
"## Dependency & Manifest Commands (`deps:*`)\n\n"
"- **`deps:add`** `<pkg> <dep> [--type]`  : Add dependency/conflict to `.stowdeps` (`--required`, `--optional`, `--conflict`)\n"
"- **`deps:edit`** `<pkg> <dep> <type>`   : Edit existing dependency classification (`--required`, `--optional`, `--conflict`)\n"
"- **`deps:remove`** `<pkg> <dep>`        : Remove a dependency or conflict entry from package `.stowdeps` \n"
"- **`deps:show`** `<pkg>`               : Display raw `.stowdeps` manifest contents for a package\n"
"- **`scan`** `[pkg ...]`                 : Recursively scan package scripts/configs to auto-detect required tools & plugins\n\n"
"## Stow & Deployment Commands\n\n"
"- **`stow`** `<pkg ...>`                 : Stow one or multiple packages with automatic conflict & dependency resolution\n"
"- **`unstow`** `<pkg ...>`               : Unstow one or multiple packages from target directory\n"
"- **`restow`** `<pkg ...>`               : Restow (unstow & stow) one or multiple packages\n"
"- **`all`**                            : Stow all packages present in dotfiles repository\n"
"- **`diff`** `[pkg ...]`                 : Preview symlink creations, conflict backups, and missing dependencies (dry-run)\n"
"- **`fix-conflicts`**                  : Unfold directory symlinks in target into real directories to resolve folding collisions\n"
"- **`check`** `[pkg ...]`                : Verify required/optional tools, plugins, and symlink integrity for packages\n"
"- **`check-symlinks`**                 : Scan repository & target home for broken symlinks and unmanaged orphan symlinks\n"
"- **`help`**                           : Display this help manual\n\n"
"## Workflow Examples\n\n"
"- **Scaffold & Configure Package**:\n"
"  `stow-manager pkg:create hyprland`\n"
"  `stow-manager deps:add hyprland waybar --required`\n"
"  `stow-manager deps:add hyprland rofi --optional`\n\n"
"- **Stow Package with Auto-Install**:\n"
"  `stow-manager -y stow hyprland`\n\n"
"- **Preview Stow Changes (Dry-Run)**:\n"
"  `stow-manager -n stow terminal`\n\n"
"- **Unstow & Delete Package**:\n"
"  `stow-manager pkg:remove hyprland`\n";

static void render_markdown_line(char *line, bool is_tty) {
    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\n') {
        line[len - 1] = '\0';
    }

    if (!is_tty) {
        printf("%s\n", line);
        return;
    }

    if (strncmp(line, "# ", 2) == 0) {
        printf("\n%s%s%s%s\n", COLOR_CYAN, COLOR_BOLD, line + 2, COLOR_RESET);
    } else if (strncmp(line, "## ", 3) == 0) {
        printf("\n%s%s=== %s ===%s\n", COLOR_CYAN, COLOR_BOLD, line + 3, COLOR_RESET);
    } else {
        char *p = line;
        bool in_bold = false;
        bool in_code = false;

        while (*p) {
            if (strncmp(p, "**", 2) == 0) {
                if (in_bold) {
                    printf("%s", COLOR_RESET);
                    in_bold = false;
                } else {
                    printf("%s%s", COLOR_BOLD, COLOR_CYAN);
                    in_bold = true;
                }
                p += 2;
            } else if (*p == '`') {
                if (in_code) {
                    printf("%s", COLOR_RESET);
                    in_code = false;
                } else {
                    printf("%s", COLOR_CYAN);
                    in_code = true;
                }
                p++;
            } else {
                putchar(*p);
                p++;
            }
        }
        if (in_bold || in_code) {
            printf("%s", COLOR_RESET);
        }
        printf("\n");
    }
}

static void show_help(void) {
    StringArray search_paths;
    str_array_init(&search_paths);

    char data_home[PATH_MAX];
    get_xdg_data_home(data_home, sizeof(data_home));
    char p1[PATH_MAX * 2];
    snprintf(p1, sizeof(p1), "%s/stow-manager/help.md", data_home);
    str_array_append(&search_paths, p1);

    char config_home[PATH_MAX];
    get_xdg_config_home(config_home, sizeof(config_home));
    char p2[PATH_MAX * 2];
    snprintf(p2, sizeof(p2), "%s/stow-manager/help.md", config_home);
    str_array_append(&search_paths, p2);

    StringArray data_dirs;
    str_array_init(&data_dirs);
    get_xdg_data_dirs(&data_dirs);
    for (size_t i = 0; i < data_dirs.count; i++) {
        char path[PATH_MAX * 2];
        snprintf(path, sizeof(path), "%s/stow-manager/help.md", data_dirs.items[i]);
        str_array_append(&search_paths, path);
    }
    str_array_free(&data_dirs);

#ifdef DATADIR
    char p3[PATH_MAX * 2];
    snprintf(p3, sizeof(p3), "%s/stow-manager/help.md", DATADIR);
    str_array_append(&search_paths, p3);
#endif

    str_array_append(&search_paths, "resources/help.md");

    FILE *fp = NULL;
    for (size_t i = 0; i < search_paths.count; i++) {
        if (file_exists(search_paths.items[i])) {
            fp = fopen(search_paths.items[i], "r");
            if (fp) break;
        }
    }

    bool is_tty = isatty(STDOUT_FILENO) != 0;

    if (fp) {
        char line[1024];
        while (fgets(line, sizeof(line), fp)) {
            render_markdown_line(line, is_tty);
        }
        fclose(fp);
    } else {
        char *copy = strdup(EMBEDDED_HELP);
        if (copy) {
            char *saveptr = NULL;
            char *token = strtok_r(copy, "\n", &saveptr);
            while (token) {
                render_markdown_line(token, is_tty);
                token = strtok_r(NULL, "\n", &saveptr);
            }
            free(copy);
        }
    }

    str_array_free(&search_paths);
}

static void handle_config_command(const StringArray *args) {
    const char *cmd = args->items[0];
    size_t remaining = args->count - 1;

    if (strcmp(cmd, "config:show") == 0 || strcmp(cmd, "config:list") == 0 || strcmp(cmd, "config:get") == 0) {
        config_show();
        return;
    }

    if (strcmp(cmd, "config:target") == 0) {
        if (remaining < 1) {
            log_error("Usage: stow-manager config target <path>");
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
                log_error("Usage: stow-manager config set [dotfiles|target] <path>");
                return;
            }
            const char *key_or_path = args->items[2];
            if (strcmp(key_or_path, "target") == 0) {
                if (remaining < 3) {
                    log_error("Usage: stow-manager config set target <path>");
                    return;
                }
                config_set_target_dir(args->items[3]);
            } else if (strcmp(key_or_path, "dotfiles") == 0 || strcmp(key_or_path, "repo") == 0) {
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
            if (strcmp(key_or_path, "dotfiles") == 0 || strcmp(key_or_path, "repo") == 0) {
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
            if (strcmp(key_or_path, "dotfiles") == 0 || strcmp(key_or_path, "repo") == 0) {
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

        if (strcmp(sub, "target") == 0) {
            if (remaining < 2) {
                log_error("Usage: stow-manager config target <path>");
                return;
            }
            config_set_target_dir(args->items[2]);
            return;
        }

        if (strcmp(sub, "dotfiles") == 0 || strcmp(sub, "repo") == 0) {
            if (remaining < 2) {
                log_error("Usage: stow-manager config dotfiles <path>");
                return;
            }
            config_set_dotfiles_dir(args->items[2]);
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

    const char *cmd = args.items[0];

    if (strncmp(cmd, "config", 6) == 0) {
        handle_config_command(&args);
        str_array_free(&args);
        return 0;
    }

    char dotfiles_dir[PATH_MAX * 2];
    char target_dir[PATH_MAX * 2];

    get_active_dotfiles_dir(cli_dotfiles_dir, dotfiles_dir, sizeof(dotfiles_dir));
    get_active_target_dir(cli_target_dir, target_dir, sizeof(target_dir));

    if (strcmp(cmd, "deps:add") == 0) {
        if (args.count < 3) {
            log_error("Usage: stow-manager deps:add <package> <dependency> [--required|--optional|--conflict]");
            str_array_free(&args);
            return 1;
        }
        const char *pkg = args.items[1];
        const char *dep = args.items[2];
        const char *type = (args.count > 3) ? args.items[3] : "--optional";
        manifest_add_dep(dotfiles_dir, pkg, dep, type);
    } else if (strcmp(cmd, "deps:edit") == 0 || strcmp(cmd, "deps:set") == 0) {
        if (args.count < 4) {
            log_error("Usage: stow-manager deps:edit <package> <dependency> <new_type>");
            log_info("Available types: --required, --optional, --conflict (or required, optional, conflict)");
            str_array_free(&args);
            return 1;
        }
        manifest_edit_dep(dotfiles_dir, args.items[1], args.items[2], args.items[3]);
    } else if (strcmp(cmd, "deps:remove") == 0 || strcmp(cmd, "deps:rm") == 0) {
        if (args.count < 3) {
            log_error("Usage: stow-manager deps:remove <package> <dependency>");
            str_array_free(&args);
            return 1;
        }
        manifest_remove_dep(dotfiles_dir, args.items[1], args.items[2]);
    } else if (strcmp(cmd, "deps:show") == 0 || strcmp(cmd, "deps:list") == 0) {
        if (args.count < 2) {
            log_error("Usage: stow-manager deps:show <package>");
            str_array_free(&args);
            return 1;
        }
        manifest_show(dotfiles_dir, args.items[1]);
    } else if (strcmp(cmd, "pkg:create") == 0 || strcmp(cmd, "package:create") == 0 || strcmp(cmd, "make:package") == 0 || strcmp(cmd, "make:pkg") == 0) {
        if (args.count < 2) {
            log_error("Usage: stow-manager pkg:create <package_name>");
            str_array_free(&args);
            return 1;
        }
        const char *pkg = args.items[1];
        PackageManifest manifest;
        manifest_init(&manifest, pkg);
        manifest_save(&manifest, dotfiles_dir);
        log_success("Created package directory & manifest for '%s'.", pkg);
        manifest_free(&manifest);
    } else if (strcmp(cmd, "pkg:remove") == 0 || strcmp(cmd, "package:remove") == 0 || strcmp(cmd, "remove:package") == 0 || strcmp(cmd, "pkg:rm") == 0 || strcmp(cmd, "package:rm") == 0) {
        if (args.count < 2) {
            log_error("Usage: stow-manager pkg:remove <package_name>");
            str_array_free(&args);
            return 1;
        }
        for (size_t i = 1; i < args.count; i++) {
            package_remove(dotfiles_dir, target_dir, args.items[i], dry_run);
        }
    } else if (strcmp(cmd, "pkg:list") == 0 || strcmp(cmd, "package:list") == 0 || strcmp(cmd, "list") == 0) {
        list_packages_status(dotfiles_dir, target_dir);
    } else if (strcmp(cmd, "check") == 0) {
        if (args.count > 1) {
            for (size_t i = 1; i < args.count; i++) {
                check_package_dependencies(dotfiles_dir, args.items[i], auto_install, dry_run);
            }
        } else {
            check_package_dependencies(dotfiles_dir, NULL, auto_install, dry_run);
        }
        check_symlink_health(dotfiles_dir, target_dir);
    } else if (strcmp(cmd, "check-symlinks") == 0) {
        check_symlink_health(dotfiles_dir, target_dir);
    } else if (strcmp(cmd, "diff") == 0) {
        if (args.count > 1) {
            for (size_t i = 1; i < args.count; i++) {
                stow_package(dotfiles_dir, target_dir, args.items[i], auto_install, true);
            }
        } else {
            stow_all_packages(dotfiles_dir, target_dir, auto_install, true);
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
    } else if (strcmp(cmd, "list") == 0) {
        list_packages_status(dotfiles_dir, target_dir);
    } else if (strcmp(cmd, "stow") == 0) {
        if (args.count < 2) {
            log_error("Please specify at least one package name to stow!");
            str_array_free(&args);
            return 1;
        }
        for (size_t i = 1; i < args.count; i++) {
            stow_package(dotfiles_dir, target_dir, args.items[i], auto_install, dry_run);
        }
    } else if (strcmp(cmd, "unstow") == 0) {
        if (args.count < 2) {
            log_error("Please specify at least one package name to unstow!");
            str_array_free(&args);
            return 1;
        }
        for (size_t i = 1; i < args.count; i++) {
            unstow_package(dotfiles_dir, target_dir, args.items[i], dry_run);
        }
    } else if (strcmp(cmd, "restow") == 0) {
        if (args.count < 2) {
            log_error("Please specify at least one package name to restow!");
            str_array_free(&args);
            return 1;
        }
        for (size_t i = 1; i < args.count; i++) {
            restow_package(dotfiles_dir, target_dir, args.items[i], auto_install, dry_run);
        }
    } else if (strcmp(cmd, "fix-conflicts") == 0) {
        unfold_directory_symlinks(target_dir, dotfiles_dir, dry_run);
    } else if (strcmp(cmd, "all") == 0) {
        stow_all_packages(dotfiles_dir, target_dir, auto_install, dry_run);
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
                stow_package(dotfiles_dir, target_dir, args.items[i], auto_install, dry_run);
            }
        } else {
            log_error("Unknown command or package '%s'", cmd);
            show_help();
            str_array_free(&args);
            return 1;
        }
    }

    str_array_free(&args);
    return 0;
}
