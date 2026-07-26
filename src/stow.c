#define _POSIX_C_SOURCE 200809L
#include "stow.h"
#include <time.h>

static void get_timestamp_str(char *buf, size_t size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    if (t) {
        strftime(buf, size, "%Y%m%d_%H%M%S", t);
    } else {
        snprintf(buf, size, "unknown");
    }
}

typedef struct {
    const char *dotfiles_dir;
    bool dry_run;
    int unfolded_count;
} UnfoldContext;

static void unfold_symlink_cb(const char *symlink_path, void *user_data) {
    UnfoldContext *ctx = (UnfoldContext *)user_data;
    if (is_dir(symlink_path)) {
        char *target = read_symlink_target(symlink_path);
        if (target && is_path_prefix(target, ctx->dotfiles_dir)) {
            if (ctx->dry_run) {
                log_warn("[DRY-RUN] Would unfold directory symlink: %s -> %s", symlink_path, target);
            } else {
                log_warn("Unfolding directory symlink: %s -> %s", symlink_path, target);
                char tmp_dir[PATH_MAX * 4];
                snprintf(tmp_dir, sizeof(tmp_dir), "%s.unfold_tmp_%d", symlink_path, (int)getpid());
                register_temp_path(tmp_dir);
                if (mkdir_p(tmp_dir, 0755) == 0) {
                    DIR *tdir = opendir(target);
                    if (tdir) {
                        struct dirent *entry;
                        while ((entry = readdir(tdir)) != NULL) {
                            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
                            char child_src[PATH_MAX * 2], child_dst[PATH_MAX * 2];
                            join_path(child_src, sizeof(child_src), target, entry->d_name);
                            join_path(child_dst, sizeof(child_dst), tmp_dir, entry->d_name);
                            symlink(child_src, child_dst);
                        }
                        closedir(tdir);

                        unlink(symlink_path);
                        if (rename(tmp_dir, symlink_path) != 0) {
                            log_error("Failed to atomic rename unfolded directory '%s'", tmp_dir);
                        }
                    } else {
                        rmdir(tmp_dir);
                    }
                }
                unregister_temp_path(tmp_dir);
            }
            ctx->unfolded_count++;
        }
        if (target) free(target);
    }
}

void unfold_directory_symlinks(const char *target_dir, const char *dotfiles_dir, bool dry_run) {
    if (dry_run) {
        log_info("[DRY-RUN] Scanning for directory symlinks that cause Stow folding conflicts...");
    } else {
        log_info("Scanning for directory symlinks that cause Stow folding conflicts...");
    }

    UnfoldContext ctx = { dotfiles_dir, dry_run, 0 };
    walk_dir_symlinks(target_dir, 1, 6, unfold_symlink_cb, &ctx);

    if (ctx.unfolded_count > 0) {
        if (dry_run) {
            log_info("[DRY-RUN] Found %d directory symlinks that would be unfolded into real directories.", ctx.unfolded_count);
        } else {
            log_success("Successfully unfolded %d directory symlinks into real directories!", ctx.unfolded_count);
        }
    } else {
        log_info("No directory symlinks required unfolding.");
    }
}

typedef struct {
    const char *target_dir;
    const char *pkg_dir;
    const char *timestamp;
    bool dry_run;
    size_t create_count;
    size_t replace_count;
    size_t backup_count;
    size_t unchanged_count;
} PrepareConflictContext;

static void prepare_conflict_cb(const char *file_path, const char *rel_path, void *user_data) {
    PrepareConflictContext *ctx = (PrepareConflictContext *)user_data;
    char target_path[PATH_MAX * 2];
    join_path(target_path, sizeof(target_path), ctx->target_dir, rel_path);

    char parent_dir[PATH_MAX * 2];
    snprintf(parent_dir, sizeof(parent_dir), "%s", target_path);
    char *last_slash = strrchr(parent_dir, '/');
    if (last_slash) {
        *last_slash = '\0';
        if (!is_dir(parent_dir)) {
            if (ctx->dry_run) {
                log_info("[DRY-RUN] Would create directory: %s", parent_dir);
            } else {
                mkdir_p(parent_dir, 0755);
            }
        }
    }

    if (is_symlink(target_path)) {
        char *existing_target = read_symlink_target(target_path);
        char real_file_path[PATH_MAX * 2];
        if (realpath(file_path, real_file_path) == NULL) {
            snprintf(real_file_path, sizeof(real_file_path), "%s", file_path);
        }

        if (existing_target && (strcmp(existing_target, file_path) == 0 || strcmp(existing_target, real_file_path) == 0)) {
            ctx->unchanged_count++;
        } else {
            if (ctx->dry_run) {
                log_info("[DRY-RUN] Would replace symlink: %s -> %s", target_path, file_path);
            } else {
                log_info("Removing existing symlink: %s", target_path);
                unlink(target_path);
            }
            ctx->replace_count++;
        }
        if (existing_target) free(existing_target);
    } else if (file_exists(target_path)) {
        char backup_path[PATH_MAX * 4];
        snprintf(backup_path, sizeof(backup_path), "%s.bak.%s", target_path, ctx->timestamp);

        char test_path[PATH_MAX * 4];
        snprintf(test_path, sizeof(test_path), "%s", backup_path);
        unsigned int counter = 1;
        while (file_exists(test_path) || is_symlink(test_path)) {
            snprintf(test_path, sizeof(test_path), "%s.%u", backup_path, counter++);
        }

        if (ctx->dry_run) {
            log_warn("[DRY-RUN] Would backup unmanaged file: %s -> %s", target_path, test_path);
        } else {
            log_warn("Backing up unmanaged file conflict: %s -> %s", target_path, test_path);
            rename(target_path, test_path);
        }
        ctx->backup_count++;
    } else {
        if (ctx->dry_run) {
            log_info("[DRY-RUN] Would create symlink: %s -> %s", target_path, file_path);
        }
        ctx->create_count++;
    }
}

void prepare_target_conflicts(const char *target_dir, const char *dotfiles_dir, const char *pkg_name, bool dry_run) {
    char pkg_dir[PATH_MAX * 2];
    join_path(pkg_dir, sizeof(pkg_dir), dotfiles_dir, pkg_name);

    if (!is_dir(pkg_dir)) return;

    if (dry_run) {
        log_info("[DRY-RUN] Previewing target paths & conflicts for package '%s'...", pkg_name);
    } else {
        log_info("Preparing target paths and resolving conflicts for package '%s'...", pkg_name);
    }

    char timestamp[64];
    get_timestamp_str(timestamp, sizeof(timestamp));

    PrepareConflictContext ctx = { target_dir, pkg_dir, timestamp, dry_run, 0, 0, 0, 0 };
    walk_dir_files(pkg_dir, "", prepare_conflict_cb, &ctx);

    if (dry_run) {
        log_info("[DRY-RUN] Summary for '%s': %zu new symlink(s), %zu replaced, %zu backed up, %zu unchanged.",
                 pkg_name, ctx.create_count, ctx.replace_count, ctx.backup_count, ctx.unchanged_count);
    }
}

typedef struct {
    const char *pkg_dir;
    const char *target_dir;
    bool is_stowed;
} CheckStowedContext;

static void check_stowed_cb(const char *file_path, const char *rel_path, void *user_data) {
    CheckStowedContext *ctx = (CheckStowedContext *)user_data;
    if (ctx->is_stowed) return;

    char target_path[PATH_MAX * 2];
    join_path(target_path, sizeof(target_path), ctx->target_dir, rel_path);

    if (is_symlink(target_path)) {
        char *link_dest = read_symlink_target(target_path);
        char real_file_path[PATH_MAX * 2];
        if (realpath(file_path, real_file_path) == NULL) {
            snprintf(real_file_path, sizeof(real_file_path), "%s", file_path);
        }

        if (link_dest && (strcmp(link_dest, file_path) == 0 || strcmp(link_dest, real_file_path) == 0 || is_path_prefix(link_dest, ctx->pkg_dir))) {
            ctx->is_stowed = true;
        }
        if (link_dest) free(link_dest);
    }
}

bool is_package_stowed(const char *target_dir, const char *dotfiles_dir, const char *pkg_name) {
    char pkg_dir[PATH_MAX * 2];
    join_path(pkg_dir, sizeof(pkg_dir), dotfiles_dir, pkg_name);

    if (!is_dir(pkg_dir)) return false;

    CheckStowedContext ctx = { pkg_dir, target_dir, false };
    walk_dir_files(pkg_dir, "", check_stowed_cb, &ctx);
    return ctx.is_stowed;
}

void handle_mutual_exclusions(const char *target_dir, const char *dotfiles_dir, const char *pkg_name, bool dry_run) {
    PackageManifest manifest;
    manifest_init(&manifest, pkg_name);
    manifest_load(&manifest, dotfiles_dir);

    for (size_t i = 0; i < manifest.conflicts.count; i++) {
        const char *conflicting_pkg = manifest.conflicts.items[i];
        if (is_package_stowed(target_dir, dotfiles_dir, conflicting_pkg)) {
            if (dry_run) {
                log_warn("[DRY-RUN] Package '%s' conflicts with currently stowed package '%s'. Would unstow '%s' first.",
                         pkg_name, conflicting_pkg, conflicting_pkg);
            } else {
                log_warn("Package '%s' conflicts with currently stowed package '%s'.", pkg_name, conflicting_pkg);
                log_info("Auto-unstowing conflicting package '%s' first...", conflicting_pkg);
                unstow_package(dotfiles_dir, target_dir, conflicting_pkg, false);
            }
        }
    }

    manifest_free(&manifest);
}

int stow_package(const char *dotfiles_dir, const char *target_dir, const char *pkg_name, bool auto_install, bool dry_run) {
    if (dry_run) {
        log_info("[DRY-RUN] Previewing stow operation for package '%s'...", pkg_name);
    } else {
        log_info("Stowing package '%s'...", pkg_name);
    }

    check_package_dependencies(dotfiles_dir, pkg_name, auto_install, dry_run);
    handle_mutual_exclusions(target_dir, dotfiles_dir, pkg_name, dry_run);
    unfold_directory_symlinks(target_dir, dotfiles_dir, dry_run);
    prepare_target_conflicts(target_dir, dotfiles_dir, pkg_name, dry_run);

    if (dry_run) {
        log_info("[DRY-RUN] Would execute Stow command: stow -d \"%s\" -t \"%s\" --no-folding --ignore='\\.stowdeps' -v -R \"%s\"",
                 dotfiles_dir, target_dir, pkg_name);
        log_success("[DRY-RUN] Dry run / Diff complete for package '%s'. No changes were made to disk.", pkg_name);
        return 0;
    }

    char escaped_dotfiles[PATH_MAX * 2], escaped_target[PATH_MAX * 2], escaped_pkg[PATH_MAX * 2];
    escape_shell_arg(dotfiles_dir, escaped_dotfiles, sizeof(escaped_dotfiles));
    escape_shell_arg(target_dir, escaped_target, sizeof(escaped_target));
    escape_shell_arg(pkg_name, escaped_pkg, sizeof(escaped_pkg));

    char stow_cmd[PATH_MAX * 6 + 128];
    snprintf(stow_cmd, sizeof(stow_cmd), "stow -d %s -t %s --no-folding --ignore='\\.stowdeps' -v -R %s",
             escaped_dotfiles, escaped_target, escaped_pkg);

    int res = run_system_cmd(stow_cmd);
    if (res == 0) {
        log_success("Successfully stowed package '%s'!", pkg_name);
    } else {
        log_error("Failed to stow package '%s'.", pkg_name);
    }
    return res;
}

int unstow_package(const char *dotfiles_dir, const char *target_dir, const char *pkg_name, bool dry_run) {
    if (dry_run) {
        log_info("[DRY-RUN] Previewing unstow operation for package '%s'...", pkg_name);
    } else {
        log_info("Unstowing package '%s'...", pkg_name);
    }

    unfold_directory_symlinks(target_dir, dotfiles_dir, dry_run);

    if (dry_run) {
        log_info("[DRY-RUN] Would execute Stow command: stow -d \"%s\" -t \"%s\" --no-folding --ignore='\\.stowdeps' -v -D \"%s\"",
                 dotfiles_dir, target_dir, pkg_name);
        log_success("[DRY-RUN] Dry run complete for package '%s'. No changes were made to disk.", pkg_name);
        return 0;
    }

    char escaped_dotfiles[PATH_MAX * 2], escaped_target[PATH_MAX * 2], escaped_pkg[PATH_MAX * 2];
    escape_shell_arg(dotfiles_dir, escaped_dotfiles, sizeof(escaped_dotfiles));
    escape_shell_arg(target_dir, escaped_target, sizeof(escaped_target));
    escape_shell_arg(pkg_name, escaped_pkg, sizeof(escaped_pkg));

    char stow_cmd[PATH_MAX * 6 + 128];
    snprintf(stow_cmd, sizeof(stow_cmd), "stow -d %s -t %s --no-folding --ignore='\\.stowdeps' -v -D %s",
             escaped_dotfiles, escaped_target, escaped_pkg);

    int res = run_system_cmd(stow_cmd);
    if (res == 0) {
        log_success("Successfully unstowed package '%s'!", pkg_name);
    } else {
        log_error("Failed to unstow package '%s'.", pkg_name);
    }
    return res;
}

int restow_package(const char *dotfiles_dir, const char *target_dir, const char *pkg_name, bool auto_install, bool dry_run) {
    if (dry_run) {
        log_info("[DRY-RUN] Previewing restow operation for package '%s'...", pkg_name);
    } else {
        log_info("Restowing package '%s'...", pkg_name);
    }
    return stow_package(dotfiles_dir, target_dir, pkg_name, auto_install, dry_run);
}

void stow_all_packages(const char *dotfiles_dir, const char *target_dir, bool auto_install, bool dry_run) {
    StringArray pkgs;
    str_array_init(&pkgs);
    get_all_packages(dotfiles_dir, &pkgs);

    for (size_t i = 0; i < pkgs.count; i++) {
        stow_package(dotfiles_dir, target_dir, pkgs.items[i], auto_install, dry_run);
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
