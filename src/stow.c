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

#include <errno.h>
#include <fnmatch.h>
#include <time.h>

static void get_timestamp_str(char* buf, size_t size) {
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    if (t) {
        strftime(buf, size, "%Y%m%d_%H%M%S", t);
    } else {
        snprintf(buf, size, "unknown");
    }
}

// Resolves symlink_path and checks if it points into dotfiles_dir.
// If so, extracts the owner package name into owner_pkg_buf.
static bool get_symlink_owner_package(const char* symlink_path,
                                      const char* dotfiles_dir,
                                      char* owner_pkg_buf, size_t buf_size) {
    char real_dotfiles[PATH_MAX];
    if (realpath(dotfiles_dir, real_dotfiles) == NULL) {
        snprintf(real_dotfiles, sizeof(real_dotfiles), "%s", dotfiles_dir);
    }

    char real_target[PATH_MAX];
    if (realpath(symlink_path, real_target) == NULL) {
        return false;
    }

    if (is_path_prefix(real_target, real_dotfiles)) {
        size_t prefix_len = strlen(real_dotfiles);
        const char* rel = real_target + prefix_len;
        while (*rel == '/') {
            rel++;
        }

        const char* slash = strchr(rel, '/');
        if (slash) {
            size_t pkg_len = (size_t)(slash - rel);
            if (pkg_len > 0 && pkg_len < buf_size) {
                strncpy(owner_pkg_buf, rel, pkg_len);
                owner_pkg_buf[pkg_len] = '\0';
                return true;
            }
        } else if (strlen(rel) > 0 && strlen(rel) < buf_size) {
            snprintf(owner_pkg_buf, buf_size, "%s", rel);
            return true;
        }
    }
    return false;
}

typedef struct {
    const char* dotfiles_dir;
    bool dry_run;
    int unfolded_count;
} UnfoldContext;

static void unfold_symlink_cb(const char* symlink_path, void* user_data) {
    UnfoldContext* ctx = (UnfoldContext*)user_data;
    if (!is_dir(symlink_path)) {
        return;
    }

    char* target = read_symlink_target(symlink_path);
    if (!target) {
        return;
    }

    if (is_path_prefix(target, ctx->dotfiles_dir)) {
        if (ctx->dry_run) {
            log_warn("[DRY-RUN] Would unfold directory symlink: %s -> %s",
                     symlink_path, target);
        } else {
            log_warn("Unfolding directory symlink: %s -> %s", symlink_path,
                     target);

            // Inherit permissions from target directory
            struct stat target_st;
            mode_t target_mode = 0755;
            if (stat(target, &target_st) == 0) {
                target_mode = target_st.st_mode & 0777;
            }

            char tmp_dir[PATH_MAX * 4];
            snprintf(tmp_dir, sizeof(tmp_dir), "%s.unfold_tmp_%d", symlink_path,
                     (int)getpid());

            register_temp_path(tmp_dir);

            if (mkdir_p(tmp_dir, target_mode) == 0) {
                DIR* tdir = opendir(target);
                bool copy_success = true;

                if (tdir) {
                    struct dirent* entry;
                    while ((entry = readdir(tdir)) != NULL) {
                        if (strcmp(entry->d_name, ".") == 0 ||
                            strcmp(entry->d_name, "..") == 0) {
                            continue;
                        }
                        char child_src[PATH_MAX * 2];
                        char child_dst[PATH_MAX * 2];
                        join_path(child_src, sizeof(child_src), target,
                                  entry->d_name);
                        join_path(child_dst, sizeof(child_dst), tmp_dir,
                                  entry->d_name);

                        if (symlink(child_src, child_dst) != 0) {
                            log_error("Failed to symlink unfolded child '%s'",
                                      child_dst);
                            copy_success = false;
                            break;
                        }
                    }
                    closedir(tdir);

                    if (copy_success) {
                        // Atomic swap: replace symlink without prior unlinking
                        if (rename(tmp_dir, symlink_path) != 0) {
                            unlink(symlink_path);
                            if (rename(tmp_dir, symlink_path) != 0) {
                                log_error(
                                    "Failed to rename unfolded directory "
                                    "'%s': %s",
                                    tmp_dir, strerror(errno));
                            }
                        }
                    } else {
                        cleanup_temp_dir_contents(tmp_dir);
                        rmdir(tmp_dir);
                    }
                } else {
                    rmdir(tmp_dir);
                }
            }
            unregister_temp_path(tmp_dir);
        }
        ctx->unfolded_count++;
    }

    free(target);
}

void unfold_directory_symlinks(const char* target_dir, const char* dotfiles_dir,
                               bool dry_run) {
    if (dry_run) {
        log_info(
            "[DRY-RUN] Scanning for directory symlinks that cause Stow folding "
            "conflicts...");
    } else {
        log_info(
            "Scanning for directory symlinks that cause Stow folding "
            "conflicts...");
    }
    UnfoldContext ctx = {dotfiles_dir, dry_run, 0};
    walk_dir_symlinks(target_dir, 1, 6, unfold_symlink_cb, &ctx);
    if (ctx.unfolded_count == 0) {
        log_info("No directory symlinks required unfolding.");
    }
}

typedef struct {
    const char* target_dir;
    const char* dotfiles_dir;
    const char* pkg_name;
    const char* pkg_dir;
    const char* real_pkg_dir;
    const StringArray* raw_ignores;
    bool dry_run;
    size_t new_links;
    size_t replaced_links;
    size_t backups;
    size_t unchanged;
} ConflictContext;

static void prepare_conflict_cb(const char* file_path, const char* rel_path,
                                void* user_data) {
    (void)file_path;
    ConflictContext* ctx = (ConflictContext*)user_data;

    if (is_path_ignored(rel_path, ctx->raw_ignores)) {
        return;
    }

    char target_path[PATH_MAX * 2];
    join_path(target_path, sizeof(target_path), ctx->target_dir, rel_path);

    char pkg_file_path[PATH_MAX * 2];
    join_path(pkg_file_path, sizeof(pkg_file_path), ctx->pkg_dir, rel_path);

    char real_pkg_file_path[PATH_MAX * 2];
    join_path(real_pkg_file_path, sizeof(real_pkg_file_path), ctx->real_pkg_dir,
              rel_path);

    if (is_symlink(target_path)) {
        if (is_symlink_pointing_to(target_path, pkg_file_path,
                                   real_pkg_file_path)) {
            ctx->unchanged++;
        } else {
            char owner_pkg[256];
            if (get_symlink_owner_package(target_path, ctx->dotfiles_dir,
                                          owner_pkg, sizeof(owner_pkg))) {
                if (ctx->dry_run) {
                    log_warn(
                        "[DRY-RUN] Conflict! Target '%s' is stowed by "
                        "package '%s'. Would replace with '%s'.",
                        rel_path, owner_pkg, ctx->pkg_name);
                } else {
                    log_warn(
                        "Conflict! Target '%s' is stowed by package '%s'. "
                        "Replacing with '%s'...",
                        rel_path, owner_pkg, ctx->pkg_name);
                }
            } else {
                if (ctx->dry_run) {
                    log_info("[DRY-RUN] Would replace symlink: %s -> %s",
                             target_path, pkg_file_path);
                }
            }
            ctx->replaced_links++;
        }
    } else if (file_exists(target_path)) {
        char ts[64];
        get_timestamp_str(ts, sizeof(ts));
        char backup_path[PATH_MAX * 3];
        snprintf(backup_path, sizeof(backup_path), "%s.stow_backup_%s",
                 target_path, ts);

        if (ctx->dry_run) {
            log_warn("[DRY-RUN] Conflict! Would backup file: %s -> %s",
                     target_path, backup_path);
        }
        ctx->backups++;
    } else {
        if (ctx->dry_run) {
            log_info("[DRY-RUN] Would create symlink: %s -> %s", target_path,
                     pkg_file_path);
        }
        ctx->new_links++;
    }
}

typedef struct {
    const char* target_dir;
    const char* pkg_dir;
    const StringArray* raw_ignores;
    int errors;
    size_t created_count;
} NativeStowContext;

static void native_stow_cb(const char* file_path, const char* rel_path,
                           void* user_data) {
    (void)file_path;
    NativeStowContext* ctx = (NativeStowContext*)user_data;

    if (is_path_ignored(rel_path, ctx->raw_ignores)) {
        return;
    }

    char target_path[PATH_MAX * 2];
    join_path(target_path, sizeof(target_path), ctx->target_dir, rel_path);

    char pkg_file_path[PATH_MAX * 2];
    join_path(pkg_file_path, sizeof(pkg_file_path), ctx->pkg_dir, rel_path);

    char real_pkg_file_path[PATH_MAX * 2];
    if (realpath(pkg_file_path, real_pkg_file_path) == NULL) {
        snprintf(real_pkg_file_path, sizeof(real_pkg_file_path), "%s",
                 pkg_file_path);
    }

    if (is_symlink(target_path)) {
        if (is_symlink_pointing_to(target_path, pkg_file_path,
                                   real_pkg_file_path)) {
            return;
        }
        unlink(target_path);
    } else if (file_exists(target_path)) {
        char ts[64];
        get_timestamp_str(ts, sizeof(ts));
        char backup_path[PATH_MAX * 3];
        snprintf(backup_path, sizeof(backup_path), "%s.stow_backup_%s",
                 target_path, ts);

        char test_path[PATH_MAX * 4];
        snprintf(test_path, sizeof(test_path), "%s", backup_path);
        unsigned int counter = 1;
        while (file_exists(test_path)) {
            snprintf(test_path, sizeof(test_path), "%s.%u", backup_path,
                     counter++);
        }

        log_warn("Conflict! Backing up file: %s -> %s", target_path, test_path);
        if (rename(target_path, test_path) != 0) {
            log_error("Failed to backup conflicting file: %s: %s", target_path,
                      strerror(errno));
            ctx->errors++;
            return;
        }
    }

    char parent_dir[PATH_MAX * 2];
    snprintf(parent_dir, sizeof(parent_dir), "%s", target_path);
    char* last_slash = strrchr(parent_dir, '/');
    if (last_slash) {
        *last_slash = '\0';
        mkdir_p(parent_dir, 0755);
    }

    if (symlink(pkg_file_path, target_path) == 0) {
        log_info("LINK: %s => %s", rel_path, pkg_file_path);
        ctx->created_count++;
    } else {
        log_error("Failed to create symlink: %s -> %s: %s", target_path,
                  pkg_file_path, strerror(errno));
        ctx->errors++;
    }
}

typedef struct {
    const char* target_dir;
    const char* pkg_dir;
    const char* real_pkg_dir;
    const StringArray* raw_ignores;
    bool dry_run;
    int errors;
    size_t unlinked_count;
} NativeUnstowContext;

static void native_unstow_cb(const char* file_path, const char* rel_path,
                             void* user_data) {
    (void)file_path;
    NativeUnstowContext* ctx = (NativeUnstowContext*)user_data;

    if (is_path_ignored(rel_path, ctx->raw_ignores)) {
        return;
    }

    char target_path[PATH_MAX * 2];
    join_path(target_path, sizeof(target_path), ctx->target_dir, rel_path);

    char pkg_file_path[PATH_MAX * 2];
    join_path(pkg_file_path, sizeof(pkg_file_path), ctx->pkg_dir, rel_path);

    if (is_symlink_pointing_to(target_path, pkg_file_path, ctx->real_pkg_dir)) {
        if (ctx->dry_run) {
            log_info("[DRY-RUN] Would unlink symlink: %s", target_path);
            ctx->unlinked_count++;
        } else {
            if (unlink(target_path) == 0) {
                log_info("UNLINK: %s", rel_path);
                ctx->unlinked_count++;

                char parent[PATH_MAX * 2];
                snprintf(parent, sizeof(parent), "%s", target_path);
                char* last_slash = strrchr(parent, '/');
                if (last_slash) {
                    *last_slash = '\0';
                }
                while (strlen(parent) > strlen(ctx->target_dir) &&
                       is_path_prefix(parent, ctx->target_dir)) {
                    if (rmdir(parent) != 0) {
                        break;
                    }
                    last_slash = strrchr(parent, '/');
                    if (last_slash) {
                        *last_slash = '\0';
                    } else {
                        break;
                    }
                }
            } else {
                log_error("Failed to unlink symlink: %s: %s", target_path,
                          strerror(errno));
                ctx->errors++;
            }
        }
    }
}

void prepare_target_conflicts(const char* target_dir, const char* dotfiles_dir,
                              const char* pkg_name, bool dry_run) {
    char pkg_dir[PATH_MAX * 2];
    join_path(pkg_dir, sizeof(pkg_dir), dotfiles_dir, pkg_name);

    char real_pkg_dir[PATH_MAX * 2];
    if (realpath(pkg_dir, real_pkg_dir) == NULL) {
        snprintf(real_pkg_dir, sizeof(real_pkg_dir), "%s", pkg_dir);
    }

    if (dry_run) {
        log_info(
            "[DRY-RUN] Previewing target paths & conflicts for package '%s'...",
            pkg_name);
    }

    StringArray raw_ignores;
    str_array_init(&raw_ignores);
    get_default_stowignore(&raw_ignores);
    parse_stowignore_raw(dotfiles_dir, &raw_ignores);
    parse_stowignore_raw(pkg_dir, &raw_ignores);

    ConflictContext ctx = {target_dir,
                           dotfiles_dir,
                           pkg_name,
                           pkg_dir,
                           real_pkg_dir,
                           &raw_ignores,
                           dry_run,
                           0,
                           0,
                           0,
                           0};
    walk_dir_files(pkg_dir, "", prepare_conflict_cb, &ctx);

    if (dry_run) {
        log_info(
            "[DRY-RUN] Summary for '%s': %zu new symlink(s), %zu replaced, %zu "
            "backed up, %zu "
            "unchanged.",
            pkg_name, ctx.new_links, ctx.replaced_links, ctx.backups,
            ctx.unchanged);
    }

    str_array_free(&raw_ignores);
}

typedef struct {
    const char* target_dir;
    const char* pkg_dir;
    const char* real_pkg_dir;
    const StringArray* raw_ignores;
    size_t total_files;
    size_t stowed_files;
} CheckStowedStatsContext;

static void check_stowed_stats_cb(const char* file_path, const char* rel_path,
                                  void* user_data) {
    (void)file_path;
    CheckStowedStatsContext* ctx = (CheckStowedStatsContext*)user_data;

    if (is_path_ignored(rel_path, ctx->raw_ignores)) {
        return;
    }

    ctx->total_files++;

    char target_path[PATH_MAX * 2];
    join_path(target_path, sizeof(target_path), ctx->target_dir, rel_path);

    char pkg_file_path[PATH_MAX * 2];
    join_path(pkg_file_path, sizeof(pkg_file_path), ctx->pkg_dir, rel_path);

    char real_pkg_file_path[PATH_MAX * 2];
    join_path(real_pkg_file_path, sizeof(real_pkg_file_path), ctx->real_pkg_dir,
              rel_path);

    if (is_symlink(target_path)) {
        if (is_symlink_pointing_to(target_path, pkg_file_path,
                                   real_pkg_file_path)) {
            ctx->stowed_files++;
        }
    }
}

StowStatus get_package_stow_status(const char* target_dir,
                                   const char* dotfiles_dir,
                                   const char* pkg_name) {
    char pkg_dir[PATH_MAX * 2];
    join_path(pkg_dir, sizeof(pkg_dir), dotfiles_dir, pkg_name);

    if (!is_dir(pkg_dir)) {
        return STOW_STATUS_UNSTOWED;
    }

    char real_pkg_dir[PATH_MAX * 2];
    if (realpath(pkg_dir, real_pkg_dir) == NULL) {
        snprintf(real_pkg_dir, sizeof(real_pkg_dir), "%s", pkg_dir);
    }

    StringArray raw_ignores;
    str_array_init(&raw_ignores);
    get_default_stowignore(&raw_ignores);
    parse_stowignore_raw(dotfiles_dir, &raw_ignores);
    parse_stowignore_raw(pkg_dir, &raw_ignores);

    CheckStowedStatsContext ctx = {target_dir,   pkg_dir, real_pkg_dir,
                                   &raw_ignores, 0,       0};
    walk_dir_files(pkg_dir, "", check_stowed_stats_cb, &ctx);
    str_array_free(&raw_ignores);

    if (ctx.total_files == 0) {
        return STOW_STATUS_UNSTOWED;
    }
    if (ctx.stowed_files == ctx.total_files) {
        return STOW_STATUS_STOWED;
    }
    if (ctx.stowed_files > 0) {
        return STOW_STATUS_PARTIAL;
    }
    return STOW_STATUS_UNSTOWED;
}

bool is_package_stowed(const char* target_dir, const char* dotfiles_dir,
                       const char* pkg_name) {
    StowStatus status =
        get_package_stow_status(target_dir, dotfiles_dir, pkg_name);
    return (bool)(status == STOW_STATUS_STOWED ||
                  status == STOW_STATUS_PARTIAL);
}

void handle_mutual_exclusions(const char* target_dir, const char* dotfiles_dir,
                              const char* pkg_name, bool dry_run) {
    PackageManifest manifest;
    manifest_init(&manifest, pkg_name);
    manifest_load(&manifest, dotfiles_dir);

    for (size_t i = 0; i < manifest.conflicts.count; i++) {
        const char* conflict_pkg = manifest.conflicts.items[i];
        if (is_package_stowed(target_dir, dotfiles_dir, conflict_pkg)) {
            if (dry_run) {
                log_warn(
                    "[DRY-RUN] Would unstow manifest-conflicting package '%s' "
                    "before stowing '%s'.",
                    conflict_pkg, pkg_name);
            } else {
                log_warn(
                    "Unstowing manifest-conflicting package '%s' before "
                    "stowing '%s'...",
                    conflict_pkg, pkg_name);
                unstow_package(dotfiles_dir, target_dir, conflict_pkg, dry_run);
            }
        }
    }

    manifest_free(&manifest);
}

typedef struct {
    const char* target_dir;
    const char* dotfiles_dir;
    const char* current_pkg;
    const StringArray* raw_ignores;
    StringArray conflicting_pkgs;
} DetectConflictsContext;

static void detect_conflicts_cb(const char* file_path, const char* rel_path,
                                void* user_data) {
    (void)file_path;
    DetectConflictsContext* ctx = (DetectConflictsContext*)user_data;

    if (is_path_ignored(rel_path, ctx->raw_ignores)) {
        return;
    }

    char target_path[PATH_MAX * 2];
    join_path(target_path, sizeof(target_path), ctx->target_dir, rel_path);

    if (is_symlink(target_path)) {
        char owner_pkg[256];
        if (get_symlink_owner_package(target_path, ctx->dotfiles_dir, owner_pkg,
                                      sizeof(owner_pkg))) {
            if (strcmp(owner_pkg, ctx->current_pkg) != 0) {
                if (!str_array_contains(&ctx->conflicting_pkgs, owner_pkg)) {
                    str_array_append(&ctx->conflicting_pkgs, owner_pkg);
                }
            }
        }
    }
}

// Scans target paths for symlinks belonging to other packages and unstows them
void handle_dynamic_package_conflicts(const char* target_dir,
                                      const char* dotfiles_dir,
                                      const char* pkg_name, bool dry_run) {
    char pkg_dir[PATH_MAX * 2];
    join_path(pkg_dir, sizeof(pkg_dir), dotfiles_dir, pkg_name);

    if (!is_dir(pkg_dir)) {
        return;
    }

    StringArray raw_ignores;
    str_array_init(&raw_ignores);
    get_default_stowignore(&raw_ignores);
    parse_stowignore_raw(dotfiles_dir, &raw_ignores);
    parse_stowignore_raw(pkg_dir, &raw_ignores);

    DetectConflictsContext ctx;
    ctx.target_dir = target_dir;
    ctx.dotfiles_dir = dotfiles_dir;
    ctx.current_pkg = pkg_name;
    ctx.raw_ignores = &raw_ignores;
    str_array_init(&ctx.conflicting_pkgs);

    walk_dir_files(pkg_dir, "", detect_conflicts_cb, &ctx);

    for (size_t i = 0; i < ctx.conflicting_pkgs.count; i++) {
        const char* conflict_pkg = ctx.conflicting_pkgs.items[i];
        if (dry_run) {
            log_warn(
                "[DRY-RUN] Package conflict detected! Package '%s' collides "
                "with stowed package '%s'. Would unstow '%s' before stowing "
                "'%s'.",
                pkg_name, conflict_pkg, conflict_pkg, pkg_name);
        } else {
            log_warn(
                "Package conflict detected! Package '%s' collides with "
                "stowed package '%s'. Unstowing '%s' before stowing '%s'...",
                pkg_name, conflict_pkg, conflict_pkg, pkg_name);
            unstow_package(dotfiles_dir, target_dir, conflict_pkg, false);
        }
    }

    str_array_free(&ctx.conflicting_pkgs);
    str_array_free(&raw_ignores);
}

int stow_package(const char* dotfiles_dir, const char* target_dir,
                 const char* pkg_name, bool auto_install, bool dry_run) {
    if (dry_run) {
        log_info("[DRY-RUN] Previewing stow operation for package '%s'...",
                 pkg_name);
    } else {
        log_info("Stowing package '%s'...", pkg_name);
    }

    check_package_dependencies(dotfiles_dir, pkg_name, auto_install, dry_run);
    handle_mutual_exclusions(target_dir, dotfiles_dir, pkg_name, dry_run);
    handle_dynamic_package_conflicts(target_dir, dotfiles_dir, pkg_name,
                                     dry_run);
    unfold_directory_symlinks(target_dir, dotfiles_dir, dry_run);
    prepare_target_conflicts(target_dir, dotfiles_dir, pkg_name, dry_run);

    if (dry_run) {
        log_success(
            "[DRY-RUN] Dry run / Diff complete for package '%s'. No changes "
            "were made to disk.",
            pkg_name);
        return 0;
    }

    char pkg_dir[PATH_MAX * 2];
    join_path(pkg_dir, sizeof(pkg_dir), dotfiles_dir, pkg_name);

    if (!is_dir(pkg_dir)) {
        log_error("Package directory does not exist: %s", pkg_dir);
        return -1;
    }

    StringArray raw_ignores;
    str_array_init(&raw_ignores);
    get_default_stowignore(&raw_ignores);
    parse_stowignore_raw(dotfiles_dir, &raw_ignores);
    parse_stowignore_raw(pkg_dir, &raw_ignores);

    NativeStowContext ctx = {target_dir, pkg_dir, &raw_ignores, 0, 0};
    walk_dir_files(pkg_dir, "", native_stow_cb, &ctx);
    str_array_free(&raw_ignores);

    if (ctx.errors == 0) {
        log_success("Successfully stowed package '%s'!", pkg_name);
        return 0;
    } else {
        log_error("Failed to stow package '%s'!", pkg_name);
        return -1;
    }
}

int unstow_package(const char* dotfiles_dir, const char* target_dir,
                   const char* pkg_name, bool dry_run) {
    if (dry_run) {
        log_info("[DRY-RUN] Previewing unstow operation for package '%s'...",
                 pkg_name);
    } else {
        log_info("Unstowing package '%s'...", pkg_name);
    }

    char pkg_dir[PATH_MAX * 2];
    join_path(pkg_dir, sizeof(pkg_dir), dotfiles_dir, pkg_name);

    if (!is_dir(pkg_dir)) {
        log_error("Package directory does not exist: %s", pkg_dir);
        return -1;
    }

    char real_pkg_dir[PATH_MAX * 2];
    if (realpath(pkg_dir, real_pkg_dir) == NULL) {
        snprintf(real_pkg_dir, sizeof(real_pkg_dir), "%s", pkg_dir);
    }

    StringArray raw_ignores;
    str_array_init(&raw_ignores);
    get_default_stowignore(&raw_ignores);
    parse_stowignore_raw(dotfiles_dir, &raw_ignores);
    parse_stowignore_raw(pkg_dir, &raw_ignores);

    NativeUnstowContext ctx = {
        target_dir, pkg_dir, real_pkg_dir, &raw_ignores, dry_run, 0, 0};
    walk_dir_files(pkg_dir, "", native_unstow_cb, &ctx);
    str_array_free(&raw_ignores);

    if (dry_run) {
        log_success(
            "[DRY-RUN] Dry run / Diff complete for package '%s'. No changes "
            "were made to disk.",
            pkg_name);
        return 0;
    }

    if (ctx.errors == 0) {
        log_success("Successfully unstowed package '%s'!", pkg_name);
        return 0;
    } else {
        log_error("Failed to unstow package '%s'!", pkg_name);
        return -1;
    }
}

int restow_package(const char* dotfiles_dir, const char* target_dir,
                   const char* pkg_name, bool auto_install, bool dry_run) {
    if (dry_run) {
        log_info("[DRY-RUN] Restowing package '%s'...", pkg_name);
    } else {
        log_info("Restowing package '%s'...", pkg_name);
    }

    check_package_dependencies(dotfiles_dir, pkg_name, auto_install, dry_run);
    handle_mutual_exclusions(target_dir, dotfiles_dir, pkg_name, dry_run);
    handle_dynamic_package_conflicts(target_dir, dotfiles_dir, pkg_name,
                                     dry_run);
    unfold_directory_symlinks(target_dir, dotfiles_dir, dry_run);

    if (dry_run) {
        unstow_package(dotfiles_dir, target_dir, pkg_name, true);
        prepare_target_conflicts(target_dir, dotfiles_dir, pkg_name, true);
        log_success(
            "[DRY-RUN] Dry run / Diff complete for package '%s'. No changes "
            "were made to disk.",
            pkg_name);
        return 0;
    }

    unstow_package(dotfiles_dir, target_dir, pkg_name, false);
    prepare_target_conflicts(target_dir, dotfiles_dir, pkg_name, dry_run);

    char pkg_dir[PATH_MAX * 2];
    join_path(pkg_dir, sizeof(pkg_dir), dotfiles_dir, pkg_name);

    if (!is_dir(pkg_dir)) {
        log_error("Package directory does not exist: %s", pkg_dir);
        return -1;
    }

    StringArray raw_ignores;
    str_array_init(&raw_ignores);
    get_default_stowignore(&raw_ignores);
    parse_stowignore_raw(dotfiles_dir, &raw_ignores);
    parse_stowignore_raw(pkg_dir, &raw_ignores);

    NativeStowContext ctx = {target_dir, pkg_dir, &raw_ignores, 0, 0};
    walk_dir_files(pkg_dir, "", native_stow_cb, &ctx);
    str_array_free(&raw_ignores);

    if (ctx.errors == 0) {
        log_success("Successfully restowed package '%s'!", pkg_name);
        return 0;
    } else {
        log_error("Failed to restow package '%s'!", pkg_name);
        return -1;
    }
}

void stow_all_packages(const char* dotfiles_dir, const char* target_dir,
                       bool auto_install, bool dry_run) {
    StringArray packages;
    str_array_init(&packages);
    get_all_packages(dotfiles_dir, &packages);

    if (dry_run) {
        log_info(
            "[DRY-RUN] Previewing stow operation for ALL packages (%zu "
            "found)...",
            packages.count);
    } else {
        log_info("Stowing ALL packages (%zu found)...", packages.count);
    }

    for (size_t i = 0; i < packages.count; i++) {
        stow_package(dotfiles_dir, target_dir, packages.items[i], auto_install,
                     dry_run);
    }

    str_array_free(&packages);
}

void list_packages_status(const char* dotfiles_dir, const char* target_dir) {
    StringArray packages;
    str_array_init(&packages);
    get_all_packages(dotfiles_dir, &packages);

    printf("\n%s%s=== Stow Packages Status ===%s\n\n", COLOR_CYAN, COLOR_BOLD,
           COLOR_RESET);

    for (size_t i = 0; i < packages.count; i++) {
        const char* pkg = packages.items[i];
        StowStatus status =
            get_package_stow_status(target_dir, dotfiles_dir, pkg);
        if (status == STOW_STATUS_STOWED) {
            printf("  %s[STOWED]%s   %s\n", COLOR_GREEN, COLOR_RESET, pkg);
        } else if (status == STOW_STATUS_PARTIAL) {
            printf("  %s[PARTIAL]%s  %s\n", COLOR_YELLOW, COLOR_RESET, pkg);
        } else {
            printf("  %s[UNSTOWED]%s %s\n", COLOR_RED, COLOR_RESET, pkg);
        }
    }
    printf("\n");

    str_array_free(&packages);
}
