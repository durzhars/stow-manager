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
#ifndef STR
#define XSTR(s) #s
#define STR(s) XSTR(s)
#endif

#include <dirent.h>
#include <fnmatch.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/stowignore.h"
#include "utils/fs.h"
#include "utils/mem.h"
#include "utils/path.h"
#include "utils/timer.h"

void pkg_file_list_init(PkgFileList *list)
{
    if (!list)
        return;
    list->entries = NULL;
    list->count = 0;
    list->capacity = 0;
}

void pkg_file_list_free(PkgFileList *list)
{
    if (!list)
        return;
    free(list->entries);
    list->entries = NULL;
    list->count = 0;
    list->capacity = 0;
}

void pkg_file_list_append(PkgFileList *list,
                          const char *rel_path,
                          const char *full_path,
                          bool is_dir)
{
    if (!list || !rel_path || !full_path)
        return;
    if (list->count >= list->capacity) {
        size_t new_cap = list->capacity == 0 ? 32 : list->capacity * 2;
        PkgFileEntry *new_entries = safe_realloc(list->entries, new_cap * sizeof(PkgFileEntry));
        list->entries = new_entries;
        list->capacity = new_cap;
    }
    PkgFileEntry *entry = &list->entries[list->count++];
    size_t rel_len = strlen(rel_path);
    if (rel_len < sizeof(entry->rel_path)) {
        memcpy(entry->rel_path, rel_path, rel_len + 1);
    } else {
        snprintf(entry->rel_path, sizeof(entry->rel_path), "%s", rel_path);
    }

    size_t full_len = strlen(full_path);
    if (full_len < sizeof(entry->full_path)) {
        memcpy(entry->full_path, full_path, full_len + 1);
    } else {
        snprintf(entry->full_path, sizeof(entry->full_path), "%s", full_path);
    }
    entry->is_dir = is_dir;
}

typedef struct {
    const StringArray *raw_ignores;
    PkgFileList *list;
    char full_buf[PATH_MAX * 2];
    char rel_buf[PATH_MAX * 2];
} CollectState;

static void
collect_package_files_recursive(CollectState *state, size_t base_full_len, size_t base_rel_len)
{
    DIR *dir = opendir(state->full_buf);
    if (!dir) {
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        if (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'))) {
            continue;
        }

        // Avoid strlen(name) if using dirent d_namlen (BSD/macOS) or track offset during copy
        size_t name_len = strlen(name);

        char *full_p = state->full_buf + base_full_len;
        if (base_full_len > 0 && state->full_buf[base_full_len - 1] != '/') {
            *full_p++ = '/';
        }
        if ((size_t)(full_p - state->full_buf) + name_len >= sizeof(state->full_buf)) {
            continue;
        }
        memcpy(full_p, name, name_len + 1);
        size_t next_full_len = (size_t)(full_p - state->full_buf) + name_len;

        char *rel_p = state->rel_buf + base_rel_len;
        if (base_rel_len > 0 && state->rel_buf[base_rel_len - 1] != '/') {
            *rel_p++ = '/';
        }
        if ((size_t)(rel_p - state->rel_buf) + name_len >= sizeof(state->rel_buf)) {
            state->full_buf[base_full_len] = '\0';
            continue;
        }
        memcpy(rel_p, name, name_len + 1);
        size_t next_rel_len = (size_t)(rel_p - state->rel_buf) + name_len;

        if (state->raw_ignores && is_path_ignored(state->rel_buf, state->raw_ignores)) {
            state->full_buf[base_full_len] = '\0';
            state->rel_buf[base_rel_len] = '\0';
            continue;
        }

        int entry_is_dir = (entry->d_type == DT_DIR);
        if (entry->d_type == DT_UNKNOWN) {
            entry_is_dir = is_dir(state->full_buf) && !is_symlink(state->full_buf);
        }

        if (entry_is_dir) {
            collect_package_files_recursive(state, next_full_len, next_rel_len);
        } else {
            pkg_file_list_append(state->list, state->rel_buf, state->full_buf, false);
        }

        state->full_buf[base_full_len] = '\0';
        state->rel_buf[base_rel_len] = '\0';
    }

    closedir(dir);
}

void collect_package_files(const char *pkg_dir, const StringArray *raw_ignores, PkgFileList *list)
{
    const char *pkg_name = strrchr(pkg_dir, '/');
    pkg_name = pkg_name ? pkg_name + 1 : pkg_dir;
    char timer_label[256];
    snprintf(timer_label, sizeof(timer_label), "collect_package_files (%s)", pkg_name);
    PerfTimer t = perf_timer_start(timer_label);
    pkg_file_list_init(list);

    size_t len = strlen(pkg_dir);
    if (len >= sizeof(((CollectState *)0)->full_buf)) {
        // Path exceeds buffer capacity; handle error gracefully instead of truncating
        perf_timer_log(&t);
        return;
    }

    CollectState state;
    state.raw_ignores = raw_ignores;
    state.list = list;

    memcpy(state.full_buf, pkg_dir, len + 1);

    // Normalize trailing slash if user passed e.g. "path/to/pkg/"
    if (len > 0 && state.full_buf[len - 1] == '/') {
        len--;
        state.full_buf[len] = '\0';
    }

    state.rel_buf[0] = '\0';

    // Pass base_full_len and base_rel_len (0) directly down the stack
    collect_package_files_recursive(&state, len, 0);

    perf_timer_log(&t);
}

typedef void (*StowignoreLineCallback)(const char *line, void *user_data);

static void read_stowignore_file(const char *dir_path, StowignoreLineCallback cb, void *user_data)
{
    if (!dir_path) {
        return;
    }
    char ignore_file[PATH_MAX * 2];
    join_path(ignore_file, sizeof(ignore_file), dir_path, ".stowignore");

    FILE *fp = fopen(ignore_file, "r");
    if (!fp) {
        return;
    }

    char *linebuf = NULL;
    size_t linecap = 0;
    ssize_t linelen;

    while ((linelen = getline(&linebuf, &linecap, fp)) != -1) {
        (void)linelen;
        char *trimmed = trim_whitespace(linebuf);
        if (trimmed[0] == '#' || trimmed[0] == '\0') {
            continue;
        }

        /* Strip inline comment if present */
        char *hash = strchr(trimmed, '#');
        if (hash) {
            *hash = '\0';
            trimmed = trim_whitespace(trimmed);
        }

        if (trimmed[0] != '\0') {
            cb(trimmed, user_data);
        }
    }

    free(linebuf);
    fclose(fp);
}

static void stowignore_cb(const char *line, void *user_data)
{
    StringArray *ignore_patterns = (StringArray *)user_data;
    char escaped[PATH_MAX * 2];
    size_t e = 0;
    for (size_t i = 0; line[i] != '\0' && e + 2 < sizeof(escaped); i++) {
        if (line[i] == '.') {
            escaped[e++] = '\\';
            escaped[e++] = '.';
        } else if (line[i] == '*') {
            escaped[e++] = '.';
            escaped[e++] = '*';
        } else {
            escaped[e++] = line[i];
        }
    }
    escaped[e] = '\0';

    if (e > 0 && !str_array_contains(ignore_patterns, escaped)) {
        str_array_append(ignore_patterns, escaped);
    }
}

void parse_stowignore(const char *dir_path, StringArray *ignore_patterns)
{
    read_stowignore_file(dir_path, stowignore_cb, ignore_patterns);
}

static void raw_stowignore_cb(const char *line, void *user_data)
{
    StringArray *raw_ignores = (StringArray *)user_data;
    if (!str_array_contains(raw_ignores, line)) {
        str_array_append(raw_ignores, line);
    }
}

void parse_stowignore_raw(const char *dir_path, StringArray *raw_ignores)
{
    read_stowignore_file(dir_path, raw_stowignore_cb, raw_ignores);
}

void get_default_stowignore(StringArray *ignore_patterns)
{
    FILE *fp = open_resource_file("stowignore.default");
    if (fp) {
        char *linebuf = NULL;
        size_t linecap = 0;
        ssize_t linelen;
        while ((linelen = getline(&linebuf, &linecap, fp)) != -1) {
            (void)linelen;
            char *trimmed = trim_whitespace(linebuf);
            if (trimmed[0] == '#' || trimmed[0] == '\0') {
                continue;
            }
            if (!str_array_contains(ignore_patterns, trimmed)) {
                str_array_append(ignore_patterns, trimmed);
            }
        }
        free(linebuf);
        fclose(fp);
    } else {
        /* Embedded fallback if resource registry is not found */
        static const char *default_ignores[] = {
            ".stowdeps",   ".stowignore", ".git",       ".gitignore", ".gitattributes",
            ".gitmodules", ".DS_Store",   ".cvsignore", "CVS",        ".svn",
            ".hg",         ".hgignore",   ".hgtags",    "_darcs",     "README*",
            "LICENSE*",    "COPYING*",    "*~",         "#*#",        ".#*"};
        size_t num_defaults = sizeof(default_ignores) / sizeof(default_ignores[0]);
        for (size_t i = 0; i < num_defaults; i++) {
            if (!str_array_contains(ignore_patterns, default_ignores[i])) {
                str_array_append(ignore_patterns, default_ignores[i]);
            }
        }
    }
}

bool is_path_ignored(const char *rel_path, const StringArray *raw_ignores)
{
    if (!rel_path || *rel_path == '\0') {
        return false;
    }

    if (strcmp(rel_path, ".") == 0 || strcmp(rel_path, "..") == 0) {
        return true;
    }

    const char *base = strrchr(rel_path, '/');
    base = base ? base + 1 : rel_path;

    if (strcmp(base, ".") == 0 || strcmp(base, "..") == 0) {
        return true;
    }

    if (!raw_ignores || raw_ignores->count == 0) {
        return false;
    }

    for (size_t i = 0; i < raw_ignores->count; i++) {
        const char *pat = raw_ignores->items[i];
        if (!pat || *pat == '\0') {
            continue;
        }

        bool has_glob = (strpbrk(pat, "*?[") != NULL);

        if (strchr(pat, '/') != NULL) {
            if (!has_glob) {
                if (strcmp(pat, rel_path) == 0) {
                    return true;
                }
            } else if (fnmatch(pat, rel_path, FNM_PATHNAME) == 0) {
                return true;
            }

            size_t plen = strlen(pat);
            if (pat[plen - 1] == '/') {
                if (plen < PATH_MAX) {
                    char dir_pat[PATH_MAX];
                    memcpy(dir_pat, pat, plen - 1);
                    dir_pat[plen - 1] = '\0';

                    if (!has_glob) {
                        if (strcmp(dir_pat, rel_path) == 0 || strncmp(rel_path, pat, plen) == 0) {
                            return true;
                        }
                    } else {
                        if (fnmatch(dir_pat, rel_path, FNM_PATHNAME) == 0 ||
                            strncmp(rel_path, pat, plen) == 0 || strstr(rel_path, pat) != NULL) {
                            return true;
                        }
                    }
                }
            }
        } else {
            if (!has_glob) {
                if (strcmp(pat, base) == 0 || strcmp(pat, rel_path) == 0) {
                    return true;
                }
            } else {
                if (fnmatch(pat, base, 0) == 0 || fnmatch(pat, rel_path, 0) == 0) {
                    return true;
                }
            }
        }
    }

    return false;
}
