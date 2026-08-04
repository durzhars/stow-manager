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

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "utils.h"

bool file_exists(const char *path)
{
    struct stat st;
    return (lstat(path, &st) == 0);
}

FILE *open_resource_file(const char *filename)
{
    if (!filename || *filename == '\0') {
        return NULL;
    }

    StringArray search_paths;
    str_array_init(&search_paths);

    char data_home[PATH_MAX];
    if (get_xdg_data_home(data_home, sizeof(data_home))) {
        char p[PATH_MAX * 2];
        snprintf(p, sizeof(p), "%s/stow-manager/%s", data_home, filename);
        str_array_append(&search_paths, p);
    }

    char config_home[PATH_MAX];
    if (get_xdg_config_home(config_home, sizeof(config_home))) {
        char p[PATH_MAX * 2];
        snprintf(p, sizeof(p), "%s/stow-manager/%s", config_home, filename);
        str_array_append(&search_paths, p);
    }

    StringArray data_dirs;
    str_array_init(&data_dirs);
    get_xdg_data_dirs(&data_dirs);
    for (size_t i = 0; i < data_dirs.count; i++) {
        char p[PATH_MAX * 2];
        snprintf(p, sizeof(p), "%s/stow-manager/%s", data_dirs.items[i], filename);
        str_array_append(&search_paths, p);
    }
    str_array_free(&data_dirs);

#ifdef DATADIR
    char p3[PATH_MAX * 2];
    snprintf(p3, sizeof(p3), "%s/stow-manager/%s", STR(DATADIR), filename);
    str_array_append(&search_paths, p3);
#endif

    char p_res[PATH_MAX * 2];
    snprintf(p_res, sizeof(p_res), "resources/%s", filename);
    str_array_append(&search_paths, p_res);

    FILE *fp = NULL;
    for (size_t i = 0; i < search_paths.count; i++) {
        if (file_exists(search_paths.items[i])) {
            fp = fopen(search_paths.items[i], "r");
            if (fp) {
                break;
            }
        }
    }

    str_array_free(&search_paths);
    return fp;
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
    if (!path || *path == '\0') {
        return NULL;
    }

    char target[PATH_MAX * 2];
    ssize_t len = readlink(path, target, sizeof(target) - 1);
    if (len == -1) {
        return NULL;
    }
    target[len] = '\0';

    // 2 bytes of extra room for '/' and '\0'
    char abs_target[PATH_MAX * 2 + 2];
    const char *query_path = target;

    if (target[0] != '/') {
        const char *last_slash = strrchr(path, '/');
        int formatted_len = 0;
        if (last_slash) {
            size_t parent_len = (size_t)(last_slash - path);
            if (parent_len >= PATH_MAX) {
                return NULL;
            }
            formatted_len =
                snprintf(abs_target, sizeof(abs_target), "%.*s/%s", (int)parent_len, path, target);
        } else {
            formatted_len = snprintf(abs_target, sizeof(abs_target), "./%s", target);
        }

        if (formatted_len < 0 || (size_t)formatted_len >= sizeof(abs_target)) {
            return NULL;
        }
        query_path = abs_target;
    }

    // POSIX 2008 / GNU realpath(path, NULL) allocates the exact buffer required
    char *real_res = realpath(query_path, NULL);
    if (real_res) {
        return real_res;
    }

    // Fallback for non-existent targets: try resolving parent directory if possible
    char norm_path[PATH_MAX * 2];
    int norm_ret = snprintf(norm_path, sizeof(norm_path), "%s", query_path);
    if (norm_ret < 0 || (size_t)norm_ret >= sizeof(norm_path)) {
        return NULL;
    }
    collapse_path(norm_path);

    char *last_slash = strrchr(norm_path, '/');
    if (last_slash && last_slash != norm_path) {
        *last_slash = '\0';
        char *parent_real = realpath(norm_path, NULL);
        if (parent_real) {
            char full_buf[PATH_MAX * 2];
            int ret = snprintf(full_buf, sizeof(full_buf), "%s/%s", parent_real, last_slash + 1);
            free(parent_real);
            if (ret < 0 || (size_t)ret >= sizeof(full_buf)) {
                return NULL;
            }
            return safe_strdup(full_buf);
        }
        *last_slash = '/';
    }

    return safe_strdup(norm_path);
}

bool is_symlink_pointing_to(const char *symlink_path,
                            const char *pkg_file_path,
                            const char *real_pkg_file_path)
{
    if (!symlink_path || !pkg_file_path) {
        return false;
    }

    char raw_link[PATH_MAX];
    ssize_t len = readlink(symlink_path, raw_link, sizeof(raw_link) - 1);
    if (len == -1) {
        return false;
    }
    raw_link[len] = '\0';

    char one_level_target[PATH_MAX];
    if (raw_link[0] == '/') {
        memcpy(one_level_target, raw_link, (size_t)len + 1);
    } else {
        const char *last_slash = strrchr(symlink_path, '/');
        if (last_slash) {
            size_t parent_len = (size_t)(last_slash - symlink_path);
            if (parent_len + 1 + (size_t)len >= sizeof(one_level_target)) {
                return false;
            }
            memcpy(one_level_target, symlink_path, parent_len);
            one_level_target[parent_len] = '/';
            memcpy(one_level_target + parent_len + 1, raw_link, (size_t)len + 1);
        } else {
            if ((size_t)len + 3 >= sizeof(one_level_target)) {
                return false;
            }
            one_level_target[0] = '.';
            one_level_target[1] = '/';
            memcpy(one_level_target + 2, raw_link, (size_t)len + 1);
        }
    }
    collapse_path(one_level_target);
    normalize_path(one_level_target);

    char norm_pkg_file[PATH_MAX];
    size_t pkg_len = strlen(pkg_file_path);
    if (pkg_len < sizeof(norm_pkg_file)) {
        memcpy(norm_pkg_file, pkg_file_path, pkg_len + 1);
        normalize_path(norm_pkg_file);
        if (strcmp(one_level_target, norm_pkg_file) == 0) {
            return true;
        }
    }

    if (real_pkg_file_path && *real_pkg_file_path != '\0') {
        if (strcmp(one_level_target, real_pkg_file_path) == 0) {
            return true;
        }
        char norm_real_pkg_file[PATH_MAX];
        size_t real_len = strlen(real_pkg_file_path);
        if (real_len < sizeof(norm_real_pkg_file)) {
            memcpy(norm_real_pkg_file, real_pkg_file_path, real_len + 1);
            normalize_path(norm_real_pkg_file);
            if (strcmp(one_level_target, norm_real_pkg_file) == 0) {
                return true;
            }
        }
    }

    char resolved_target[PATH_MAX];
    char resolved_pkg_file[PATH_MAX];
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
    if (!path || *path == '\0') {
        errno = EINVAL;
        return -1;
    }

    char tmp[PATH_MAX * 2];
    size_t len = strlen(path);
    if (len >= sizeof(tmp)) {
        errno = ENAMETOOLONG;
        return -1;
    }
    memcpy(tmp, path, len + 1);

    if (len > 1 && tmp[len - 1] == '/') {
        tmp[--len] = '\0';
    }

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';

            if (mkdir(tmp, mode) != 0) {
                if (errno == EEXIST) {
                    struct stat st;
                    if (stat(tmp, &st) != 0 || !S_ISDIR(st.st_mode)) {
                        errno = ENOTDIR;
                        return -1;
                    }
                } else {
                    return -1; // EACCES, EROFS, etc.
                }
            }
            *p = '/';
        }
    }

    if (mkdir(tmp, mode) != 0) {
        if (errno == EEXIST) {
            struct stat st;
            if (stat(tmp, &st) != 0 || !S_ISDIR(st.st_mode)) {
                errno = ENOTDIR;
                return -1;
            }
            return 0;
        }
        return -1;
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
    char path[PATH_MAX * 2];
    size_t dotfiles_len = strlen(dotfiles_dir);

    if (dotfiles_len < sizeof(path) - 1) {
        memcpy(path, dotfiles_dir, dotfiles_len);
        if (dotfiles_len > 0 && path[dotfiles_len - 1] != '/') {
            path[dotfiles_len++] = '/';
        }
    }

    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        if (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'))) {
            continue;
        }

        if (!is_path_ignored(name, &ignore_patterns)) {
            // Leverage d_type to avoid unnecessary stat calls when available
            if (entry->d_type == DT_DIR) {
                str_array_append(packages, name);
            } else if (entry->d_type == DT_UNKNOWN || entry->d_type == DT_LNK) {
                size_t name_len = strlen(name);
                if (dotfiles_len + name_len < sizeof(path)) {
                    memcpy(path + dotfiles_len, name, name_len + 1);
                    if (is_dir(path) && !is_symlink(path)) {
                        str_array_append(packages, name);
                    }
                }
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

    char path[PATH_MAX * 2];
    size_t dir_len = strlen(dir_path);
    if (dir_len < sizeof(path) - 1) {
        memcpy(path, dir_path, dir_len);
        if (dir_len > 0 && path[dir_len - 1] != '/') {
            path[dir_len++] = '/';
        }
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        if (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'))) {
            continue;
        }

        size_t name_len = strlen(name);
        if (dir_len + name_len >= sizeof(path)) {
            continue;
        }
        memcpy(path + dir_len, name, name_len + 1);

        // Fast path via d_type
        if (entry->d_type == DT_LNK) {
            cb(path, user_data);
        } else if (entry->d_type == DT_DIR) {
            walk_dir_symlinks(path, current_depth + 1, max_depth, cb, user_data);
        } else if (entry->d_type == DT_UNKNOWN) {
            if (is_symlink(path)) {
                cb(path, user_data);
            } else if (is_dir(path)) {
                walk_dir_symlinks(path, current_depth + 1, max_depth, cb, user_data);
            }
        }
    }

    closedir(dir);
}

typedef struct {
    const char *base_dir;
    char full_buf[PATH_MAX * 2];
    char rel_buf[PATH_MAX * 2];
    WalkFileCallback cb;
    void *user_data;
} WalkFilesState;

static void walk_dir_files_recursive(WalkFilesState *state)
{
    DIR *dir = opendir(state->full_buf);
    if (!dir) {
        return;
    }

    size_t base_full_len = strlen(state->full_buf);
    size_t base_rel_len = strlen(state->rel_buf);

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        if (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'))) {
            continue;
        }

        size_t name_len = strlen(name);

        char *full_p = state->full_buf + base_full_len;
        if (base_full_len > 0 && state->full_buf[base_full_len - 1] != '/') {
            *full_p++ = '/';
        }
        if ((size_t)(full_p - state->full_buf) + name_len < sizeof(state->full_buf)) {
            memcpy(full_p, name, name_len + 1);
        }

        char *rel_p = state->rel_buf + base_rel_len;
        if (base_rel_len > 0 && state->rel_buf[base_rel_len - 1] != '/') {
            *rel_p++ = '/';
        }
        if ((size_t)(rel_p - state->rel_buf) + name_len < sizeof(state->rel_buf)) {
            memcpy(rel_p, name, name_len + 1);
        }

        if (entry->d_type == DT_DIR) {
            walk_dir_files_recursive(state);
        } else if (entry->d_type == DT_REG || entry->d_type == DT_LNK) {
            state->cb(state->full_buf, state->rel_buf, state->user_data);
        } else {
            if (is_dir(state->full_buf) && !is_symlink(state->full_buf)) {
                walk_dir_files_recursive(state);
            } else {
                state->cb(state->full_buf, state->rel_buf, state->user_data);
            }
        }

        state->full_buf[base_full_len] = '\0';
        state->rel_buf[base_rel_len] = '\0';
    }

    closedir(dir);
}

void walk_dir_files(const char *base_dir,
                    const char *current_dir,
                    WalkFileCallback cb,
                    void *user_data)
{
    WalkFilesState state;
    state.base_dir = base_dir;
    state.cb = cb;
    state.user_data = user_data;

    if (current_dir && *current_dir != '\0') {
        join_path(state.full_buf, sizeof(state.full_buf), base_dir, current_dir);
        snprintf(state.rel_buf, sizeof(state.rel_buf), "%s", current_dir);
    } else {
        snprintf(state.full_buf, sizeof(state.full_buf), "%s", base_dir);
        state.rel_buf[0] = '\0';
    }

    walk_dir_files_recursive(&state);
}

void cleanup_temp_dir_contents(const char *dir_path)
{
    if (!dir_path || *dir_path == '\0') {
        return;
    }

    DIR *dir = opendir(dir_path);
    if (!dir) {
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        if (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'))) {
            continue;
        }

        char child_path[PATH_MAX * 2];
        join_path(child_path, sizeof(child_path), dir_path, name);

        if (is_dir(child_path) && !is_symlink(child_path)) {
            cleanup_temp_dir_contents(child_path);
            rmdir(child_path);
        } else {
            unlink(child_path);
        }
    }

    closedir(dir);
}
