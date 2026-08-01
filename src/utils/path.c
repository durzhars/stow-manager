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
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void normalize_path(char *path)
{
    if (!path || *path == '\0') {
        return;
    }
    char *p = path;
    while (*p) {
        if (*p == '/' && *(p + 1) == '/') {
            char *q = p + 1;
            while (*q == '/') {
                q++;
            }
            memmove(p + 1, q, strlen(q) + 1);
        }
        p++;
    }
    size_t len = strlen(path);
    if (len > 1 && path[len - 1] == '/') {
        path[len - 1] = '\0';
    }
}

void collapse_path(char *path)
{
    if (!path || *path == '\0') {
        return;
    }
    char temp[PATH_MAX * 2];
    char *out = temp;
    const char *in = path;

    while (*in) {
        if (in[0] == '/' && in[1] == '.' && in[2] == '.' && (in[3] == '/' || in[3] == '\0')) {
            if (out > temp) {
                out--;
                while (out > temp && *out != '/') {
                    out--;
                }
            }
            in += 3;
        } else if (in[0] == '/' && in[1] == '.' && (in[2] == '/' || in[2] == '\0')) {
            in += 2;
        } else {
            *out++ = *in++;
        }
    }
    *out = '\0';
    if (temp[0] == '\0') {
        memmove(path, "/", 2);
    } else {
        memmove(path, temp, strlen(temp) + 1);
    }
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
        const char *home = getenv("HOME");
        if (home && strlen(home) > 0) {
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
    if (!buf || buf_size == 0) {
        return false;
    }
    const char *home = getenv("HOME");
    if (home && strlen(home) > 0) {
        snprintf(buf, buf_size, "%s", home);
        return true;
    }
    buf[0] = '\0';
    return false;
}
