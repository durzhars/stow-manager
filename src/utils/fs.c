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
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "utils.h"
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

bool file_exists(const char *path)
{
    struct stat st;
    return (stat(path, &st) == 0);
}

bool is_dir(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    return false;
}

bool is_symlink(const char *path)
{
    struct stat st;
    if (lstat(path, &st) == 0) {
        return S_ISLNK(st.st_mode);
    }
    return false;
}

char *read_symlink_target(const char *path)
{
    char resolved[PATH_MAX * 2];
    if (realpath(path, resolved)) {
        return safe_strdup(resolved);
    }

    char target[PATH_MAX * 2];
    ssize_t len = readlink(path, target, sizeof(target) - 1);
    if (len != -1) {
        target[len] = '\0';
        if (target[0] != '/') {
            char parent_dir[PATH_MAX * 2];
            snprintf(parent_dir, sizeof(parent_dir), "%s", path);
            char *last_slash = strrchr(parent_dir, '/');
            if (last_slash) {
                *last_slash = '\0';
            } else {
                snprintf(parent_dir, sizeof(parent_dir), ".");
            }

            char abs_target[PATH_MAX * 2];
            join_path(abs_target, sizeof(abs_target), parent_dir, target);

            char real_abs[PATH_MAX * 2];
            if (realpath(abs_target, real_abs) != NULL) {
                return safe_strdup(real_abs);
            }
            normalize_path(abs_target);
            return safe_strdup(abs_target);
        }
        return safe_strdup(target);
    }
    return NULL;
}

bool is_symlink_pointing_to(const char *symlink_path,
                            const char *pkg_file_path,
                            const char *real_pkg_file_path)
{
    if (!symlink_path || !is_symlink(symlink_path) || !pkg_file_path) {
        return false;
    }

    char raw_link[PATH_MAX * 2];
    ssize_t len = readlink(symlink_path, raw_link, sizeof(raw_link) - 1);
    if (len == -1) {
        return false;
    }
    raw_link[len] = '\0';

    char one_level_target[PATH_MAX * 2];
    if (raw_link[0] == '/') {
        snprintf(one_level_target, sizeof(one_level_target), "%s", raw_link);
    } else {
        char parent_dir[PATH_MAX * 2];
        snprintf(parent_dir, sizeof(parent_dir), "%s", symlink_path);
        char *last_slash = strrchr(parent_dir, '/');
        if (last_slash) {
            *last_slash = '\0';
        } else {
            snprintf(parent_dir, sizeof(parent_dir), ".");
        }
        join_path(one_level_target, sizeof(one_level_target), parent_dir, raw_link);
    }
    collapse_path(one_level_target);
    normalize_path(one_level_target);

    char norm_pkg_file[PATH_MAX * 2];
    snprintf(norm_pkg_file, sizeof(norm_pkg_file), "%s", pkg_file_path);
    normalize_path(norm_pkg_file);

    if (strcmp(one_level_target, norm_pkg_file) == 0) {
        return true;
    }

    if (real_pkg_file_path && strlen(real_pkg_file_path) > 0) {
        char norm_real_pkg_file[PATH_MAX * 2];
        snprintf(norm_real_pkg_file, sizeof(norm_real_pkg_file), "%s", real_pkg_file_path);
        normalize_path(norm_real_pkg_file);
        if (strcmp(one_level_target, norm_real_pkg_file) == 0) {
            return true;
        }
    }

    char resolved_target[PATH_MAX * 2];
    char resolved_pkg_file[PATH_MAX * 2];
    if (realpath(symlink_path, resolved_target) != NULL &&
        realpath(pkg_file_path, resolved_pkg_file) != NULL) {
        if (strcmp(resolved_target, resolved_pkg_file) == 0) {
            return true;
        }
    }

    return false;
}

int mkdir_p(const char *path, mode_t mode)
{
    char tmp[PATH_MAX * 2];
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);
    if (len == 0) {
        return -1;
    }
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
    }

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (!is_dir(tmp)) {
                if (mkdir(tmp, mode) != 0 && errno != EEXIST) {
                    return -1;
                }
            }
            *p = '/';
        }
    }
    if (!is_dir(tmp)) {
        if (mkdir(tmp, mode) != 0 && errno != EEXIST) {
            return -1;
        }
    }
    return 0;
}

void get_all_packages(const char *dotfiles_dir, StringArray *packages)
{
    DIR *dir = opendir(dotfiles_dir);
    if (!dir) {
        return;
    }

    StringArray ignore_patterns;
    str_array_init(&ignore_patterns);
    get_default_stowignore(&ignore_patterns);
    parse_stowignore_raw(dotfiles_dir, &ignore_patterns);

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
            continue;
        }
        if (!is_path_ignored(name, &ignore_patterns)) {
            char path[PATH_MAX * 2];
            join_path(path, sizeof(path), dotfiles_dir, name);
            if (is_dir(path) && !is_symlink(path)) {
                str_array_append(packages, name);
            }
        }
    }

    str_array_free(&ignore_patterns);
    closedir(dir);
}

void walk_dir_symlinks(const char *dir_path,
                       int current_depth,
                       int max_depth,
                       WalkSymlinkCallback cb,
                       void *user_data)
{
    if (current_depth > max_depth) {
        return;
    }

    DIR *dir = opendir(dir_path);
    if (!dir) {
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char path[PATH_MAX * 2];
        join_path(path, sizeof(path), dir_path, entry->d_name);

        if (is_symlink(path)) {
            cb(path, user_data);
        } else if (is_dir(path) && !is_symlink(path)) {
            walk_dir_symlinks(path, current_depth + 1, max_depth, cb, user_data);
        }
    }
    closedir(dir);
}

void walk_dir_files(const char *base_dir,
                    const char *current_dir,
                    WalkFileCallback cb,
                    void *user_data)
{
    char dir_path[PATH_MAX * 2];
    if (current_dir && strlen(current_dir) > 0) {
        join_path(dir_path, sizeof(dir_path), base_dir, current_dir);
    } else {
        snprintf(dir_path, sizeof(dir_path), "%s", base_dir);
    }

    DIR *dir = opendir(dir_path);
    if (!dir) {
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[PATH_MAX * 2];
        join_path(full_path, sizeof(full_path), dir_path, entry->d_name);

        char rel_path[PATH_MAX * 2];
        if (current_dir && strlen(current_dir) > 0) {
            snprintf(rel_path, sizeof(rel_path), "%s/%s", current_dir, entry->d_name);
        } else {
            snprintf(rel_path, sizeof(rel_path), "%s", entry->d_name);
        }

        if (is_dir(full_path) && !is_symlink(full_path)) {
            walk_dir_files(base_dir, rel_path, cb, user_data);
        } else {
            cb(full_path, rel_path, user_data);
        }
    }
    closedir(dir);
}
