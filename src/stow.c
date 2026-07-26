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

#define _POSIX_C_SOURCE 200809L
#include "stow.h"
#include <time.h>

void parse_stowignore(const char *dir_path, StringArray *ignore_patterns) {
    if (!dir_path) return;
    char ignore_file[PATH_MAX * 2];
    join_path(ignore_file, sizeof(ignore_file), dir_path, ".stowignore");

    FILE *fp = fopen(ignore_file, "r");
    if (!fp) return;

    char *linebuf = NULL;
    size_t linecap = 0;
    ssize_t linelen;

    while ((linelen = getline(&linebuf, &linecap, fp)) != -1) {
        (void)linelen;
        char *trimmed = trim_whitespace(linebuf);
        if (trimmed[0] == '#' || trimmed[0] == '\0') continue;

        char escaped[PATH_MAX * 2];
        size_t e = 0;
        for (size_t i = 0; trimmed[i] != '\0' && e + 2 < sizeof(escaped); i++) {
            if (trimmed[i] == '.') {
                escaped[e++] = '\\';
                escaped[e++] = '.';
            } else if (trimmed[i] == '*') {
                escaped[e++] = '.';
                escaped[e++] = '*';
            } else {
                escaped[e++] = trimmed[i];
            }
        }
        escaped[e] = '\0';

        if (strlen(escaped) > 0 && !str_array_contains(ignore_patterns, escaped)) {
            str_array_append(ignore_patterns, escaped);
        }
    }

    free(linebuf);
    fclose(fp);
}

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
    if (ctx.unfolded_count == 0) {
        log_info("No directory symlinks required unfolding.");
    }
}

typedef struct {
    const char *target_dir;
    const char *pkg_dir;
    const char *real_pkg_dir;
    bool dry_run;
    size_t new_links;
    size_t replaced_links;
    size_t backups;
    size_t unchanged;
} ConflictContext;

static void prepare_conflict_cb(const char *file_path, const char *rel_path, void *user_data) {
    (void)file_path;
    ConflictContext *ctx = (ConflictContext *)user_data;

    char target_path[PATH_MAX * 2];
    join_path(target_path, sizeof(target_path), ctx->target_dir, rel_path);

    char pkg_file_path[PATH_MAX * 2];
    join_path(pkg_file_path, sizeof(pkg_file_path), ctx->pkg_dir, rel_path);

    char real_pkg_file_path[PATH_MAX * 2];
    join_path(real_pkg_file_path, sizeof(real_pkg_file_path), ctx->real_pkg_dir, rel_path);

    if (is_symlink(target_path)) {
        char *target = read_symlink_target(target_path);
        if (target && (strcmp(target, pkg_file_path) == 0 || strcmp(target, real_pkg_file_path) == 0)) {
            ctx->unchanged++;
        } else {
            if (ctx->dry_run) {
                log_info("[DRY-RUN] Would replace symlink: %s -> %s", target_path, pkg_file_path);
            } else {
                unlink(target_path);
            }
            ctx->replaced_links++;
        }
        if (target) free(target);
    } else if (file_exists(target_path)) {
        char ts[64];
        get_timestamp_str(ts, sizeof(ts));
        char backup_path[PATH_MAX * 2];
        snprintf(backup_path, sizeof(backup_path), "%s.stow_backup_%s", target_path, ts);

        char test_path[PATH_MAX * 2];
        snprintf(test_path, sizeof(test_path), "%s", backup_path);
        unsigned int counter = 1;
        while (file_exists(test_path)) {
            snprintf(test_path, sizeof(test_path), "%s.%u", backup_path, counter++);
        }

        if (ctx->dry_run) {
            log_warn("[DRY-RUN] Conflict! Would backup file: %s -> %s", target_path, test_path);
        } else {
            log_warn("Conflict! Backing up file: %s -> %s", target_path, test_path);
            if (rename(target_path, test_path) != 0) {
                log_error("Failed to backup conflicting file: %s", target_path);
            }
        }
        ctx->backups++;
    } else {
        if (ctx->dry_run) {
            log_info("[DRY-RUN] Would create symlink: %s -> %s", target_path, pkg_file_path);
        }
        ctx->new_links++;
    }
}

void prepare_target_conflicts(const char *target_dir, const char *dotfiles_dir, const char *pkg_name, bool dry_run) {
    char pkg_dir[PATH_MAX * 2];
    join_path(pkg_dir, sizeof(pkg_dir), dotfiles_dir, pkg_name);

    char real_pkg_dir[PATH_MAX * 2];
    if (realpath(pkg_dir, real_pkg_dir) == NULL) {
        snprintf(real_pkg_dir, sizeof(real_pkg_dir), "%s", pkg_dir);
    }

    if (dry_run) {
        log_info("[DRY-RUN] Previewing target paths & conflicts for package '%s'...", pkg_name);
    }

    ConflictContext ctx = { target_dir, pkg_dir, real_pkg_dir, dry_run, 0, 0, 0, 0 };
    walk_dir_files(pkg_dir, "", prepare_conflict_cb, &ctx);

    if (dry_run) {
        log_info("[DRY-RUN] Summary for '%s': %zu new symlink(s), %zu replaced, %zu backed up, %zu unchanged.",
                 pkg_name, ctx.new_links, ctx.replaced_links, ctx.backups, ctx.unchanged);
    }
}

typedef struct {
    const char *target_dir;
    const char *pkg_dir;
    const char *real_pkg_dir;
    bool is_stowed;
} CheckStowedContext;

static void check_stowed_cb(const char *file_path, const char *rel_path, void *user_data) {
    (void)file_path;
    CheckStowedContext *ctx = (CheckStowedContext *)user_data;
    if (!ctx->is_stowed) return;

    char target_path[PATH_MAX * 2];
    join_path(target_path, sizeof(target_path), ctx->target_dir, rel_path);

    char pkg_file_path[PATH_MAX * 2];
    join_path(pkg_file_path, sizeof(pkg_file_path), ctx->pkg_dir, rel_path);

    char real_pkg_file_path[PATH_MAX * 2];
    join_path(real_pkg_file_path, sizeof(real_pkg_file_path), ctx->real_pkg_dir, rel_path);

    if (is_symlink(target_path)) {
        char *target = read_symlink_target(target_path);
        if (!target || (strcmp(target, pkg_file_path) != 0 && strcmp(target, real_pkg_file_path) != 0)) {
            ctx->is_stowed = false;
        }
        if (target) free(target);
    } else {
        ctx->is_stowed = false;
    }
}

bool is_package_stowed(const char *target_dir, const char *dotfiles_dir, const char *pkg_name) {
    char pkg_dir[PATH_MAX * 2];
    join_path(pkg_dir, sizeof(pkg_dir), dotfiles_dir, pkg_name);

    char real_pkg_dir[PATH_MAX * 2];
    if (realpath(pkg_dir, real_pkg_dir) == NULL) {
        snprintf(real_pkg_dir, sizeof(real_pkg_dir), "%s", pkg_dir);
    }

    CheckStowedContext ctx = { target_dir, pkg_dir, real_pkg_dir, true };
    walk_dir_files(pkg_dir, "", check_stowed_cb, &ctx);
    return ctx.is_stowed;
}

void handle_mutual_exclusions(const char *target_dir, const char *dotfiles_dir, const char *pkg_name, bool dry_run) {
    PackageManifest manifest;
    manifest_init(&manifest, pkg_name);
    manifest_load(&manifest, dotfiles_dir);

    for (size_t i = 0; i < manifest.conflicts.count; i++) {
        const char *conflict_pkg = manifest.conflicts.items[i];
        if (is_package_stowed(target_dir, dotfiles_dir, conflict_pkg)) {
            if (dry_run) {
                log_warn("[DRY-RUN] Would unstow conflicting package '%s' before stowing '%s'.", conflict_pkg, pkg_name);
            } else {
                log_warn("Unstowing conflicting package '%s' before stowing '%s'...", conflict_pkg, pkg_name);
                unstow_package(dotfiles_dir, target_dir, conflict_pkg, dry_run);
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

    StringArray ignore_patterns;
    str_array_init(&ignore_patterns);
    str_array_append(&ignore_patterns, "\\.stowdeps");

    parse_stowignore(dotfiles_dir, &ignore_patterns);

    char pkg_dir[PATH_MAX * 2];
    join_path(pkg_dir, sizeof(pkg_dir), dotfiles_dir, pkg_name);
    parse_stowignore(pkg_dir, &ignore_patterns);

    char ignore_args[4096] = {0};
    size_t offset = 0;
    for (size_t i = 0; i < ignore_patterns.count; i++) {
        char escaped_arg[1024];
        escape_shell_arg(ignore_patterns.items[i], escaped_arg, sizeof(escaped_arg));
        int written = snprintf(ignore_args + offset, sizeof(ignore_args) - offset, " --ignore=%s", escaped_arg);
        if (written > 0 && (size_t)written < sizeof(ignore_args) - offset) {
            offset += (size_t)written;
        }
    }
    str_array_free(&ignore_patterns);

    char escaped_dotfiles[PATH_MAX * 2], escaped_target[PATH_MAX * 2], escaped_pkg[PATH_MAX * 2];
    escape_shell_arg(dotfiles_dir, escaped_dotfiles, sizeof(escaped_dotfiles));
    escape_shell_arg(target_dir, escaped_target, sizeof(escaped_target));
    escape_shell_arg(pkg_name, escaped_pkg, sizeof(escaped_pkg));

    char stow_cmd[PATH_MAX * 8];
    snprintf(stow_cmd, sizeof(stow_cmd), "stow -d %s -t %s --no-folding%s -v -R %s",
             escaped_dotfiles, escaped_target, ignore_args, escaped_pkg);

    if (dry_run) {
        log_info("[DRY-RUN] Would execute Stow command: %s", stow_cmd);
        log_success("[DRY-RUN] Dry run / Diff complete for package '%s'. No changes were made to disk.", pkg_name);
        return 0;
    }

    log_info("Executing: %s", stow_cmd);
    int status = run_system_cmd(stow_cmd);
    if (status == 0) {
        log_success("Successfully stowed package '%s'!", pkg_name);
    } else {
        log_error("Failed to stow package '%s' (stow returned status %d)", pkg_name, status);
    }
    return status;
}

int unstow_package(const char *dotfiles_dir, const char *target_dir, const char *pkg_name, bool dry_run) {
    if (dry_run) {
        log_info("[DRY-RUN] Previewing unstow operation for package '%s'...", pkg_name);
    } else {
        log_info("Unstowing package '%s'...", pkg_name);
    }

    StringArray ignore_patterns;
    str_array_init(&ignore_patterns);
    str_array_append(&ignore_patterns, "\\.stowdeps");

    parse_stowignore(dotfiles_dir, &ignore_patterns);

    char pkg_dir[PATH_MAX * 2];
    join_path(pkg_dir, sizeof(pkg_dir), dotfiles_dir, pkg_name);
    parse_stowignore(pkg_dir, &ignore_patterns);

    char ignore_args[4096] = {0};
    size_t offset = 0;
    for (size_t i = 0; i < ignore_patterns.count; i++) {
        char escaped_arg[1024];
        escape_shell_arg(ignore_patterns.items[i], escaped_arg, sizeof(escaped_arg));
        int written = snprintf(ignore_args + offset, sizeof(ignore_args) - offset, " --ignore=%s", escaped_arg);
        if (written > 0 && (size_t)written < sizeof(ignore_args) - offset) {
            offset += (size_t)written;
        }
    }
    str_array_free(&ignore_patterns);

    char escaped_dotfiles[PATH_MAX * 2], escaped_target[PATH_MAX * 2], escaped_pkg[PATH_MAX * 2];
    escape_shell_arg(dotfiles_dir, escaped_dotfiles, sizeof(escaped_dotfiles));
    escape_shell_arg(target_dir, escaped_target, sizeof(escaped_target));
    escape_shell_arg(pkg_name, escaped_pkg, sizeof(escaped_pkg));

    char stow_cmd[PATH_MAX * 8];
    snprintf(stow_cmd, sizeof(stow_cmd), "stow -d %s -t %s%s -v -D %s",
             escaped_dotfiles, escaped_target, ignore_args, escaped_pkg);

    if (dry_run) {
        log_info("[DRY-RUN] Would execute Stow command: %s", stow_cmd);
        log_success("[DRY-RUN] Dry run / Diff complete for package '%s'. No changes were made to disk.", pkg_name);
        return 0;
    }

    log_info("Executing: %s", stow_cmd);
    int status = run_system_cmd(stow_cmd);
    if (status == 0) {
        log_success("Successfully unstowed package '%s'!", pkg_name);
    } else {
        log_error("Failed to unstow package '%s' (stow returned status %d)", pkg_name, status);
    }
    return status;
}

int restow_package(const char *dotfiles_dir, const char *target_dir, const char *pkg_name, bool auto_install, bool dry_run) {
    if (dry_run) {
        log_info("[DRY-RUN] Restowing package '%s'...", pkg_name);
    } else {
        log_info("Restowing package '%s'...", pkg_name);
    }
    unstow_package(dotfiles_dir, target_dir, pkg_name, dry_run);
    return stow_package(dotfiles_dir, target_dir, pkg_name, auto_install, dry_run);
}

void stow_all_packages(const char *dotfiles_dir, const char *target_dir, bool auto_install, bool dry_run) {
    StringArray packages;
    str_array_init(&packages);
    get_all_packages(dotfiles_dir, &packages);

    if (dry_run) {
        log_info("[DRY-RUN] Previewing stow operation for ALL packages (%zu found)...", packages.count);
    } else {
        log_info("Stowing ALL packages (%zu found)...", packages.count);
    }

    for (size_t i = 0; i < packages.count; i++) {
        stow_package(dotfiles_dir, target_dir, packages.items[i], auto_install, dry_run);
    }

    str_array_free(&packages);
}

void list_packages_status(const char *dotfiles_dir, const char *target_dir) {
    StringArray packages;
    str_array_init(&packages);
    get_all_packages(dotfiles_dir, &packages);

    printf("\n%s%s=== Stow Packages Status ===%s\n\n", COLOR_CYAN, COLOR_BOLD, COLOR_RESET);

    for (size_t i = 0; i < packages.count; i++) {
        const char *pkg = packages.items[i];
        bool stowed = is_package_stowed(target_dir, dotfiles_dir, pkg);
        if (stowed) {
            printf("  %s[STOWED]%s   %s\n", COLOR_GREEN, COLOR_RESET, pkg);
        } else {
            printf("  %s[UNSTOWED]%s %s\n", COLOR_YELLOW, COLOR_RESET, pkg);
        }
    }
    printf("\n");

    str_array_free(&packages);
}
