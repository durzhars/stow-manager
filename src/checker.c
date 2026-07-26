#define _GNU_SOURCE
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "checker.h"
#include "registry.h"
#include <termios.h>

static void build_install_command(const char *dotfiles_dir, const char *distro, const StringArray *pkgs, char *cmd, size_t cmd_size) {
    char pkg_list[1024] = {0};
    for (size_t i = 0; i < pkgs->count; i++) {
        char distro_pkg[256];
        registry_get_distro_pkg(dotfiles_dir, pkgs->items[i], distro, distro_pkg, sizeof(distro_pkg));
        strcat(pkg_list, distro_pkg);
        if (i + 1 < pkgs->count) strcat(pkg_list, " ");
    }

    if (strcmp(distro, "arch") == 0 || strcmp(distro, "manjaro") == 0 || strcmp(distro, "endeavouros") == 0) {
        snprintf(cmd, cmd_size, "sudo pacman -S --needed %s", pkg_list);
    } else if (strcmp(distro, "ubuntu") == 0 || strcmp(distro, "debian") == 0 || strcmp(distro, "pop") == 0 || strcmp(distro, "mint") == 0) {
        snprintf(cmd, cmd_size, "sudo apt update && sudo apt install -y %s", pkg_list);
    } else if (strcmp(distro, "fedora") == 0 || strcmp(distro, "rhel") == 0 || strcmp(distro, "centos") == 0) {
        snprintf(cmd, cmd_size, "sudo dnf install -y %s", pkg_list);
    } else if (strcmp(distro, "alpine") == 0) {
        snprintf(cmd, cmd_size, "sudo apk add %s", pkg_list);
    } else if (strcmp(distro, "macos") == 0) {
        snprintf(cmd, cmd_size, "brew install %s", pkg_list);
    } else {
        snprintf(cmd, cmd_size, "Install missing packages manually: %s", pkg_list);
    }
}

void check_package_dependencies(const char *dotfiles_dir, const char *target_pkg, bool auto_install) {
    char distro[64];
    get_distro_id(distro, sizeof(distro));

    StringArray all_pkgs;
    str_array_init(&all_pkgs);
    get_all_packages(dotfiles_dir, &all_pkgs);

    StringArray missing_req, missing_opt;
    str_array_init(&missing_req);
    str_array_init(&missing_opt);

    printf("\n%s%s=== Checking Package Dependencies & Optional Plugins ===%s\n\n", COLOR_CYAN, COLOR_BOLD, COLOR_RESET);

    for (size_t i = 0; i < all_pkgs.count; i++) {
        const char *pkg_name = all_pkgs.items[i];
        if (target_pkg && strcmp(target_pkg, "all") != 0 && strcmp(target_pkg, pkg_name) != 0) {
            continue;
        }

        PackageManifest manifest;
        manifest_init(&manifest, pkg_name);
        manifest_load(&manifest, dotfiles_dir);

        printf("%sPackage [%s]:%s\n", COLOR_BOLD, pkg_name, COLOR_RESET);

        printf("  %sRequired Dependencies:%s\n", COLOR_BOLD, COLOR_RESET);
        if (manifest.required.count > 0) {
            for (size_t r = 0; r < manifest.required.count; r++) {
                const char *tool = manifest.required.items[r];
                if (is_tool_installed_dynamic(dotfiles_dir, tool)) {
                    printf("    %s✓%s %s\n", COLOR_GREEN, COLOR_RESET, tool);
                } else {
                    printf("    %s✗%s %s %s(REQUIRED MISSING)%s\n", COLOR_RED, COLOR_RESET, tool, COLOR_RED, COLOR_RESET);
                    if (!str_array_contains(&missing_req, tool)) str_array_append(&missing_req, tool);
                }
            }
        } else {
            printf("    %s✓%s none\n", COLOR_GREEN, COLOR_RESET);
        }

        printf("  %sOptional Plugins & Tools:%s\n", COLOR_BOLD, COLOR_RESET);
        if (manifest.optional.count > 0) {
            for (size_t o = 0; o < manifest.optional.count; o++) {
                const char *tool = manifest.optional.items[o];
                if (is_tool_installed_dynamic(dotfiles_dir, tool)) {
                    printf("    %s✓%s %s\n", COLOR_GREEN, COLOR_RESET, tool);
                } else {
                    printf("    %s⚡%s %s %s(optional missing)%s\n", COLOR_YELLOW, COLOR_RESET, tool, COLOR_YELLOW, COLOR_RESET);
                    if (!str_array_contains(&missing_opt, tool)) str_array_append(&missing_opt, tool);
                }
            }
        } else {
            printf("    %s✓%s none\n", COLOR_GREEN, COLOR_RESET);
        }
        printf("\n");

        manifest_free(&manifest);
    }

    if (missing_req.count > 0) {
        log_error("Missing REQUIRED dependencies!");
        char install_cmd[1024];
        build_install_command(dotfiles_dir, distro, &missing_req, install_cmd, sizeof(install_cmd));
        printf("%sInstallation Command (%s):%s %s%s%s\n\n", COLOR_BOLD, distro, COLOR_RESET, COLOR_CYAN, install_cmd, COLOR_RESET);

        if (auto_install) {
            run_system_cmd(install_cmd);
        } else if (isatty(STDIN_FILENO)) {
            printf("Would you like to install missing REQUIRED dependencies now? [Y/n] ");
            fflush(stdout);
            char c = getchar();
            if (c == 'y' || c == 'Y' || c == '\n') {
                run_system_cmd(install_cmd);
            }
        }
    }

    if (missing_opt.count > 0) {
        log_warn("Missing OPTIONAL plugins & tools!");
        char install_cmd[1024];
        build_install_command(dotfiles_dir, distro, &missing_opt, install_cmd, sizeof(install_cmd));
        printf("%sInstallation Command (%s):%s %s%s%s\n\n", COLOR_BOLD, distro, COLOR_RESET, COLOR_CYAN, install_cmd, COLOR_RESET);

        if (auto_install) {
            run_system_cmd(install_cmd);
        } else if (isatty(STDIN_FILENO)) {
            printf("Would you like to install missing OPTIONAL plugins & tools now? [y/N] ");
            fflush(stdout);
            char c = getchar();
            if (c == 'y' || c == 'Y') {
                run_system_cmd(install_cmd);
            }
        }
    }

    if (missing_req.count == 0 && missing_opt.count == 0) {
        log_success("All required dependencies and optional plugins are installed!");
    }

    str_array_free(&missing_req);
    str_array_free(&missing_opt);
    str_array_free(&all_pkgs);
}
