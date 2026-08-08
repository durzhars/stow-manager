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

#define MAX_PATH_DEPTH 256

#include <errno.h>

#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils/env.h"
#include "utils/mem.h"
#include "utils/path.h"

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
    size_t len = (size_t)(w - path);
    if (len > 1 && path[len - 1] == '/') {
        path[len - 1] = '\0';
    }
}

int collapse_path(char *path)
{
    if (!path || !*path) {
        errno = EINVAL;
        return -1;
    }

    int is_abs = (path[0] == '/');
    char *r = path + (is_abs ? 1 : 0);
    char *w = path + (is_abs ? 1 : 0);

    // Stack of component write-offsets
    size_t stack[MAX_PATH_DEPTH];
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
            // Do nothing. No Sorcery here.
            continue;
        }
        if (len == 2 && start[0] == '.' && start[1] == '.') {
            if (depth > 0) {
                size_t prev = stack[depth - 1];
                size_t offset = (size_t)(w - path);
                if (!is_abs && (offset - prev) == 2 && path[prev] == '.' && path[prev + 1] == '.') {
                    if (w > path && *(w - 1) != '/') {
                        *w++ = '/';
                    }
                    if (depth >= MAX_PATH_DEPTH) {
                        errno = ENAMETOOLONG;
                        path[0] = '\0';
                        return -1;
                    }
                    stack[depth++] = (size_t)(w - path);
                    *w++ = '.';
                    *w++ = '.';
                } else {
                    depth--;
                    if (depth > 0) {
                        size_t pop_start = stack[depth];
                        if (pop_start > 0 && path[pop_start - 1] == '/') {
                            w = path + pop_start - 1;
                        } else {
                            w = path + pop_start;
                        }
                    } else {
                        w = path + (is_abs ? 1 : 0);
                    }
                }
            } else if (!is_abs) {
                if (w > path && *(w - 1) != '/') {
                    *w++ = '/';
                }
                if (depth >= MAX_PATH_DEPTH) {
                    errno = ENAMETOOLONG;
                    path[0] = '\0';
                    return -1;
                }
                stack[depth++] = (size_t)(w - path);
                *w++ = '.';
                *w++ = '.';
            }
        } else if (len > 0) {
            if (w > path && *(w - 1) != '/') {
                *w++ = '/';
            }
            if (depth >= MAX_PATH_DEPTH) {
                errno = ENAMETOOLONG;
                path[0] = '\0';
                return -1;
            }
            stack[depth++] = (size_t)(w - path);
            for (size_t i = 0; i < len; i++) {
                *w++ = start[i];
            }
        }
    }
    if (w == path) {
        *w++ = is_abs ? '/' : '.';
    }
    *w = '\0';
    return 0;
}

int join_path(char *out, size_t out_size, const char *dir, const char *rel)
{
    if (!out || out_size == 0) {
        errno = EINVAL;
        return -1;
    }

    const char *d = dir ? dir : "";
    const char *r = rel ? rel : "";

    size_t dlen = strlen(d);
    size_t rlen = strlen(r);

    if (dlen == 0 && rlen == 0) {
        out[0] = '\0';
        return 0;
    }

    size_t needed;
    int has_slash = 0;
    if (dlen == 0) {
        needed = rlen;
    } else if (rlen == 0) {
        needed = dlen;
    } else {
        has_slash = (d[dlen - 1] == '/');
        needed = dlen + (has_slash ? 0 : 1) + rlen;
    }

    if (needed >= out_size) {
        out[0] = '\0';
        errno = ENAMETOOLONG;
        return -1;
    }

    // Assemble into temporary buffer to safely support overlapping buffers (out == dir or out ==
    // rel)
    char tmp[PATH_MAX];
    char *buf = tmp;
    if (needed >= sizeof(tmp)) {
        buf = (char *)safe_malloc(needed + 1);
    }

    if (dlen == 0) {
        memcpy(buf, r, rlen + 1);
    } else if (rlen == 0) {
        memcpy(buf, d, dlen + 1);
    } else {
        memcpy(buf, d, dlen);
        if (!has_slash) {
            buf[dlen] = '/';
            memcpy(buf + dlen + 1, r, rlen + 1);
        } else {
            memcpy(buf + dlen, r, rlen + 1);
        }
    }

    normalize_path(buf);
    memcpy(out, buf, strlen(buf) + 1);

    if (buf != tmp) {
        free(buf);
    }

    return 0;
}

int is_path_prefix(const char *path, const char *prefix)
{
    if (!path || !prefix) {
        return false;
    }
    char norm_path[PATH_MAX];
    char norm_prefix[PATH_MAX];
    size_t plen = strlen(path);
    size_t prlen = strlen(prefix);
    if (plen >= sizeof(norm_path) || prlen >= sizeof(norm_prefix)) {
        return false;
    }
    memcpy(norm_path, path, plen + 1);
    memcpy(norm_prefix, prefix, prlen + 1);
    normalize_path(norm_path);
    normalize_path(norm_prefix);

    size_t norm_prlen = strlen(norm_prefix);
    if (norm_prlen == 1 && norm_prefix[0] == '/') {
        return norm_path[0] == '/';
    }

    if (strncmp(norm_path, norm_prefix, norm_prlen) == 0) {
        return norm_path[norm_prlen] == '/' || norm_path[norm_prlen] == '\0';
    }
    return false;
}

void expand_tilde_path(const char *path, char *out, size_t out_size)
{
    if (!out || out_size == 0) {
        return;
    }
    if (!path) {
        out[0] = '\0';
        return;
    }
    char temp[PATH_MAX];
    bool expanded = false;
    if (path[0] == '~' && (path[1] == '/' || path[1] == '\0')) {
        char home[PATH_MAX];
        if (get_user_home_dir(home, sizeof(home))) {
            int ret = snprintf(temp, sizeof(temp), "%s%s", home, path + 1);
            if (ret >= 0 && (size_t)ret < sizeof(temp)) {
                expanded = true;
            }
        }
    }
    if (!expanded) {
        int ret = snprintf(temp, sizeof(temp), "%s", path);
        if (ret < 0 || (size_t)ret >= sizeof(temp)) {
            out[0] = '\0';
            return;
        }
    }
    expand_env_vars(temp, out, out_size);
}

void get_dotfiles_dir(char *buf, size_t buf_size)
{
    if (!buf || buf_size == 0) {
        return;
    }
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd))) {
        int ret = snprintf(buf, buf_size, "%s", cwd);
        if (ret < 0 || (size_t)ret >= buf_size) {
            buf[0] = '\0';
            return;
        }
    } else {
        if (buf_size > 1) {
            buf[0] = '.';
            buf[1] = '\0';
        } else {
            buf[0] = '\0';
        }
    }
}

bool get_target_dir(char *buf, size_t buf_size)
{
    return get_user_home_dir(buf, buf_size);
}
