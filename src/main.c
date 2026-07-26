#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <getopt.h>
#include "stow.h"
#include "scanner.h"

static void show_help(const char *prog_name) {
    printf("%sModular ANSI C Dotfiles Framework Manager%s\n", COLOR_BOLD, COLOR_RESET);
    printf("Usage: %s [options] <command> [arguments]\n\n", prog_name);
    printf("Options:\n");
    printf("  %s-y, --install%s                  Auto-confirm installation of missing dependencies/plugins\n", COLOR_CYAN, COLOR_RESET);
    printf("  %s-h, --help%s                     Show this help menu\n\n", COLOR_CYAN, COLOR_RESET);
    printf("Dependency Management Commands (Artisan-style):\n");
    printf("  %sdeps:add%s <pkg> <dep> [--opt]   Add a dependency/conflict to package manifest\n", COLOR_CYAN, COLOR_RESET);
    printf("  %sdeps:remove%s <pkg> <dep>        Remove a dependency from package manifest\n", COLOR_CYAN, COLOR_RESET);
    printf("  %sdeps:show%s <pkg>               Display package manifest contents\n", COLOR_CYAN, COLOR_RESET);
    printf("  %smake:package%s <name>            Scaffold a new Stow package directory & manifest\n\n", COLOR_CYAN, COLOR_RESET);
    printf("Package & Stow Operations:\n");
    printf("  %scheck%s [pkg]                    Detect missing dependencies & optional plugins\n", COLOR_CYAN, COLOR_RESET);
    printf("  %sscan%s [pkg]                     Recursively scan package files to auto-detect dependencies\n", COLOR_CYAN, COLOR_RESET);
    printf("  %slist%s                           List all packages and stowed status\n", COLOR_CYAN, COLOR_RESET);
    printf("  %sstow%s <pkg>                     Stow a package with auto conflict resolution\n", COLOR_CYAN, COLOR_RESET);
    printf("  %sunstow%s <pkg>                   Unstow a package\n", COLOR_CYAN, COLOR_RESET);
    printf("  %srestow%s <pkg>                   Restow a package\n", COLOR_CYAN, COLOR_RESET);
    printf("  %sfix-conflicts%s                  Unfold directory symlinks & resolve conflicts\n", COLOR_CYAN, COLOR_RESET);
    printf("  %sall%s                            Stow all packages\n", COLOR_CYAN, COLOR_RESET);
    printf("  %shelp%s                           Show this help menu\n", COLOR_CYAN, COLOR_RESET);
}

int main(int argc, char **argv) {
    bool auto_install = false;

    static struct option long_options[] = {
        {"install", no_argument, 0, 'y'},
        {"help",    no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    int option_index = 0;
    while ((opt = getopt_long(argc, argv, "yh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'y':
                auto_install = true;
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
    char dotfiles_dir[PATH_MAX];
    char target_dir[PATH_MAX];

    get_dotfiles_dir(dotfiles_dir, sizeof(dotfiles_dir));
    get_target_dir(target_dir, sizeof(target_dir));

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
        check_package_dependencies(dotfiles_dir, pkg, auto_install);
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
        stow_package(dotfiles_dir, target_dir, argv[optind + 1], auto_install);
    } else if (strcmp(cmd, "unstow") == 0) {
        if (optind + 1 >= argc) {
            log_error("Please specify a package name to unstow!");
            return 1;
        }
        unstow_package(dotfiles_dir, target_dir, argv[optind + 1]);
    } else if (strcmp(cmd, "restow") == 0) {
        if (optind + 1 >= argc) {
            log_error("Please specify a package name to restow!");
            return 1;
        }
        restow_package(dotfiles_dir, target_dir, argv[optind + 1], auto_install);
    } else if (strcmp(cmd, "fix-conflicts") == 0) {
        unfold_directory_symlinks(target_dir, dotfiles_dir);
    } else if (strcmp(cmd, "all") == 0) {
        stow_all_packages(dotfiles_dir, target_dir, auto_install);
    } else if (strcmp(cmd, "help") == 0) {
        show_help(argv[0]);
    } else {
        // Fallback: Check if target argument is a directory in dotfiles_dir
        char pkg_dir[PATH_MAX * 2];
        snprintf(pkg_dir, sizeof(pkg_dir), "%s/%s", dotfiles_dir, cmd);
        if (is_dir(pkg_dir)) {
            stow_package(dotfiles_dir, target_dir, cmd, auto_install);
        } else {
            log_error("Unknown command or package '%s'", cmd);
            show_help(argv[0]);
            return 1;
        }
    }

    return 0;
}
