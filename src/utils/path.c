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
#define MAX_PATH_DEPTH (PATH_MAX / 2)

#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils.h"

void normalize_path(char *path)
{
    if (!path || *path == '\0') {
        return;
    }
    char *r = path;
    char *w = path;
    while (*r) {
        *w++ = *r;
        if (*r == '/') {
            while (*(r + 1) == '/') {
                r++;
            }
        }
        r++;
    }
    *w = '\0';
    size_t len = w - path;
    if (len > 1 && path[len - 1] == '/') {
        path[len - 1] = '\0';
    }
}

void collapse_path(char *path)
{
    if (!path || !*path) {
        return;
    }

    bool is_abs = (path[0] == '/');
    char *r = path + (is_abs ? 1 : 0);
    char *w = path + (is_abs ? 1 : 0);

    // Stack of component write-offsets
    size_t stack[PATH_MAX / 2];
    size_t depth = 0;

    // Sorcery below. Don't touch
    while (*r) {
        while (*r == '/') {
            r++;
        }
        if (!*r) {
            break;
        }
        const char *start = r;
        while (*r && *r != '/') {
            r++;
        }
        size_t len = (size_t)(r - start);
        if (len == 1 && start[0] == '.') {
            // Do nothing. No Sorcery here. Bohoo.
            continue;
        }
        if (len == 2 && start[0] == '.' && start[1] == '.') {
            if (depth > 0) {
                size_t prev = stack[depth - 1];
                if (!is_abs && (size_t)(w - path - prev) == 2 && path[prev] == '.' &&
                    path[prev + 1] == '.') {
                    depth++;
                    if (w > path && *(w - 1) != '/') {
                        *w++ = '/';
                    }
                    if (depth < MAX_PATH_DEPTH) {
                        stack[depth++] = (size_t)(w - path);
                    }
                    *w++ = '.';
                    *w++ = '.';
                } else {
                    depth--;
                    w = path + stack[depth];
                }
            } else if (!is_abs) {
                if (w > path && *(w - 1) != '/') {
                    *w++ = '/';
                }
                if (depth < MAX_PATH_DEPTH) {
                    stack[depth++] = (size_t)(w - path);
                }
                *w++ = '.';
                *w++ = '.';
            }
        } else if (len > 0) {
            if (w > path && *(w - 1) != '/') {
                *w++ = '/';
            }
            if (depth < MAX_PATH_DEPTH) {
                stack[depth++] = (size_t)(w - path);
            }
            for (size_t i = 0; i < len; i++) {
                *w++ = start[i];
            }
        }
    }
    if (w == path) {
        *w++ = is_abs ? '/' : '.';
    }
    *w = '\0';
}

void join_path(char *out, size_t out_size, const char *dir, const char *rel)
{
    if (!dir || strlen(dir) == 0) {
        snprintf(out, out_size, "%s", rel ? rel : "");
    } else if (!rel || strlen(rel) == 0) {
        snprintf(out, out_size, "%s", dir);
    } else {
        size_t dlen = strlen(dir);
        if (dir[dlen - 1] == '/') {
            snprintf(out, out_size, "%s%s", dir, rel);
        } else {
            snprintf(out, out_size, "%s/%s", dir, rel);
        }
    }
    normalize_path(out);
}

bool is_path_prefix(const char *path, const char *prefix)
{
    if (!path || !prefix) {
        return false;
    }
    char norm_path[PATH_MAX * 2];
    char norm_prefix[PATH_MAX * 2];
    snprintf(norm_path, sizeof(norm_path), "%s", path);
    snprintf(norm_prefix, sizeof(norm_prefix), "%s", prefix);
    normalize_path(norm_path);
    normalize_path(norm_prefix);

    size_t plen = strlen(norm_prefix);
    if (plen == 1 && norm_prefix[0] == '/') {
        return norm_path[0] == '/';
    }

    if (strncmp(norm_path, norm_prefix, plen) == 0) {
        return norm_path[plen] == '/' || norm_path[plen] == '\0';
    }
    return false;
}

void expand_tilde_path(const char *path, char *out, size_t out_size)
{
    if (!path || !out || out_size == 0) {
        return;
    }
    char temp[PATH_MAX * 2];
    if (path[0] == '~' && (path[1] == '/' || path[1] == '\0')) {
        char home[PATH_MAX];
        if (get_user_home_dir(home, sizeof(home))) {
            snprintf(temp, sizeof(temp), "%s%s", home, path + 1);
        } else {
            snprintf(temp, sizeof(temp), "%s", path);
        }
    } else {
        snprintf(temp, sizeof(temp), "%s", path);
    }
    expand_env_vars(temp, out, out_size);
}

void get_dotfiles_dir(char *buf, size_t buf_size)
{
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd))) {
        snprintf(buf, buf_size, "%s", cwd);
    } else {
        snprintf(buf, buf_size, ".");
    }
}

bool get_target_dir(char *buf, size_t buf_size)
{
    return get_user_home_dir(buf, buf_size);
}
