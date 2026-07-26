#define _POSIX_C_SOURCE 200809L
#include "stow.h"
#include <time.h>

static void get_timestamp_str(char *buf, size_t size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buf, size, "%Y%m%d_%H%M%S", t);
}

void unfold_directory_symlinks(const char *target_dir, const char *dotfiles_dir) {
    log_info("Scanning for directory symlinks that cause Stow folding conflicts...");
    int unfolded_count = 0;

    // Scan target directory up to depth 6 for directory symlinks targeting dotfiles_dir
    char find_cmd[PATH_MAX + 128];
    snprintf(find_cmd, sizeof(find_cmd), "find \"%s\" -maxdepth 6 -type l 2>/dev/null", target_dir);

    FILE *fp = popen(find_cmd, "r");
    if (fp) {
        char link_path[PATH_MAX];
        while (fgets(link_path, sizeof(link_path), fp)) {
            char *trimmed = trim_whitespace(link_path);
            if (is_dir(trimmed)) {
                char *target = read_symlink_target(trimmed);
                if (target && strncmp(target, dotfiles_dir, strlen(dotfiles_dir)) == 0) {
                    log_warn("Unfolding directory symlink: %s -> %s", trimmed, target);
                    unlink(trimmed);
                    mkdir(trimmed, 0755);
                    unfolded_count++;
                }
                if (target) free(target);
            }
        }
        pclose(fp);
    }

    if (unfolded_count > 0) {
        log_success("Successfully unfolded %d directory symlinks into real directories!", unfolded_count);
    } else {
        log_info("No directory symlinks required unfolding.");
    }
}

void prepare_target_conflicts(const char *target_dir, const char *dotfiles_dir, const char *pkg_name) {
    char pkg_dir[PATH_MAX];
    snprintf(pkg_dir, sizeof(pkg_dir), "%s/%s", dotfiles_dir, pkg_name);

    if (!is_dir(pkg_dir)) return;

    log_info("Preparing target paths and resolving conflicts for package '%s'...", pkg_name);

    char timestamp[32];
    get_timestamp_str(timestamp, sizeof(timestamp));

    char find_cmd[PATH_MAX + 128];
    snprintf(find_cmd, sizeof(find_cmd), "find \"%s\" ! -type d -a ! -name '.stowdeps' 2>/dev/null", pkg_dir);

    FILE *fp = popen(find_cmd, "r");
    if (fp) {
        char file_path[PATH_MAX];
        size_t prefix_len = strlen(pkg_dir) + 1;

        while (fgets(file_path, sizeof(file_path), fp)) {
            char *trimmed = trim_whitespace(file_path);
            if (strlen(trimmed) <= prefix_len) continue;

            const char *relative_path = trimmed + prefix_len;
            char target_path[PATH_MAX];
            snprintf(target_path, sizeof(target_path), "%s/%s", target_dir, relative_path);

            // Create parent directory
            char parent_dir[PATH_MAX];
            snprintf(parent_dir, sizeof(parent_dir), "%s", target_path);
            char *last_slash = strrchr(parent_dir, '/');
            if (last_slash) {
                *last_slash = '\0';
                char mkdir_cmd[PATH_MAX + 32];
                snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", parent_dir);
                run_system_cmd(mkdir_cmd);
            }

            if (is_symlink(target_path)) {
                log_info("Removing existing symlink: %s", target_path);
                unlink(target_path);
            } else if (file_exists(target_path)) {
                char backup_path[PATH_MAX + 64];
                snprintf(backup_path, sizeof(backup_path), "%s.bak.%s", target_path, timestamp);
                log_warn("Backing up unmanaged file conflict: %s -> %s", target_path, backup_path);
                rename(target_path, backup_path);
            }
        }
        pclose(fp);
    }
}

bool is_package_stowed(const char *target_dir, const char *dotfiles_dir, const char *pkg_name) {
    char pkg_dir[PATH_MAX];
    snprintf(pkg_dir, sizeof(pkg_dir), "%s/%s", dotfiles_dir, pkg_name);

    if (!is_dir(pkg_dir)) return false;

    char find_cmd[PATH_MAX + 128];
    snprintf(find_cmd, sizeof(find_cmd), "find \"%s\" ! -type d -a ! -name '.stowdeps' 2>/dev/null", pkg_dir);

    bool stowed = false;
    FILE *fp = popen(find_cmd, "r");
    if (fp) {
        char file_path[PATH_MAX];
        size_t prefix_len = strlen(pkg_dir) + 1;

        while (fgets(file_path, sizeof(file_path), fp)) {
            char *trimmed = trim_whitespace(file_path);
            if (strlen(trimmed) <= prefix_len) continue;

            const char *relative_path = trimmed + prefix_len;
            char target_path[PATH_MAX];
            snprintf(target_path, sizeof(target_path), "%s/%s", target_dir, relative_path);

            if (is_symlink(target_path)) {
                char *link_dest = read_symlink_target(target_path);
                if (link_dest && strncmp(link_dest, pkg_dir, strlen(pkg_dir)) == 0) {
                    free(link_dest);
                    stowed = true;
                    break;
                }
                if (link_dest) free(link_dest);
            }
        }
        pclose(fp);
    }

    return stowed;
}

void handle_mutual_exclusions(const char *target_dir, const char *dotfiles_dir, const char *pkg_name) {
    PackageManifest manifest;
    manifest_init(&manifest, pkg_name);
    manifest_load(&manifest, dotfiles_dir);

    for (size_t i = 0; i < manifest.conflicts.count; i++) {
        const char *conflicting_pkg = manifest.conflicts.items[i];
        if (is_package_stowed(target_dir, dotfiles_dir, conflicting_pkg)) {
            log_warn("Package '%s' conflicts with currently stowed package '%s'.", pkg_name, conflicting_pkg);
            log_info("Auto-unstowing conflicting package '%s' first...", conflicting_pkg);
            unstow_package(dotfiles_dir, target_dir, conflicting_pkg);
        }
    }

    manifest_free(&manifest);
}

int stow_package(const char *dotfiles_dir, const char *target_dir, const char *pkg_name, bool auto_install) {
    log_info("Stowing package '%s'...", pkg_name);

    check_package_dependencies(dotfiles_dir, pkg_name, auto_install);
    handle_mutual_exclusions(target_dir, dotfiles_dir, pkg_name);
    unfold_directory_symlinks(target_dir, dotfiles_dir);
    prepare_target_conflicts(target_dir, dotfiles_dir, pkg_name);

    char stow_cmd[PATH_MAX * 2 + 128];
    snprintf(stow_cmd, sizeof(stow_cmd), "stow -d \"%s\" -t \"%s\" --no-folding --ignore='\\.stowdeps' -v -R \"%s\"",
             dotfiles_dir, target_dir, pkg_name);

    int res = run_system_cmd(stow_cmd);
    if (res == 0) {
        log_success("Successfully stowed package '%s'!", pkg_name);
    } else {
        log_error("Failed to stow package '%s'.", pkg_name);
    }
    return res;
}

int unstow_package(const char *dotfiles_dir, const char *target_dir, const char *pkg_name) {
    log_info("Unstowing package '%s'...", pkg_name);

    unfold_directory_symlinks(target_dir, dotfiles_dir);

    char stow_cmd[PATH_MAX * 2 + 128];
    snprintf(stow_cmd, sizeof(stow_cmd), "stow -d \"%s\" -t \"%s\" --no-folding --ignore='\\.stowdeps' -v -D \"%s\"",
             dotfiles_dir, target_dir, pkg_name);

    int res = run_system_cmd(stow_cmd);
    if (res == 0) {
        log_success("Successfully unstowed package '%s'!", pkg_name);
    } else {
        log_error("Failed to unstow package '%s'.", pkg_name);
    }
    return res;
}

int restow_package(const char *dotfiles_dir, const char *target_dir, const char *pkg_name, bool auto_install) {
    log_info("Restowing package '%s'...", pkg_name);
    return stow_package(dotfiles_dir, target_dir, pkg_name, auto_install);
}

void stow_all_packages(const char *dotfiles_dir, const char *target_dir, bool auto_install) {
    StringArray pkgs;
    str_array_init(&pkgs);
    get_all_packages(dotfiles_dir, &pkgs);

    for (size_t i = 0; i < pkgs.count; i++) {
        stow_package(dotfiles_dir, target_dir, pkgs.items[i], auto_install);
    }

    str_array_free(&pkgs);
}

void list_packages_status(const char *dotfiles_dir, const char *target_dir) {
    printf("\n%s%s=== Available Dotfiles Packages ===%s\n\n", COLOR_CYAN, COLOR_BOLD, COLOR_RESET);

    StringArray pkgs;
    str_array_init(&pkgs);
    get_all_packages(dotfiles_dir, &pkgs);

    for (size_t i = 0; i < pkgs.count; i++) {
        const char *pkg_name = pkgs.items[i];
        if (is_package_stowed(target_dir, dotfiles_dir, pkg_name)) {
            printf("  %s●%s %s%s%s %s(stowed)%s\n", COLOR_GREEN, COLOR_RESET, COLOR_BOLD, pkg_name, COLOR_RESET, COLOR_GREEN, COLOR_RESET);
        } else {
            printf("  %s○%s %s (not stowed)\n", COLOR_RED, COLOR_RESET, pkg_name);
        }
    }
    printf("\n");

    str_array_free(&pkgs);
}
