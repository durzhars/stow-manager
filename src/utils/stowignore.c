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

#include "utils.h"
#include <fnmatch.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void parse_stowignore(const char *dir_path, StringArray *ignore_patterns)
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

void parse_stowignore_raw(const char *dir_path, StringArray *raw_ignores)
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
        if (!str_array_contains(raw_ignores, trimmed)) {
            str_array_append(raw_ignores, trimmed);
        }
    }

    free(linebuf);
    fclose(fp);
}

void get_default_stowignore(StringArray *ignore_patterns)
{
    const char *filename = "stowignore.default";

    StringArray search_paths;
    str_array_init(&search_paths);

    char data_home[PATH_MAX];
    get_xdg_data_home(data_home, sizeof(data_home));
    char p1[PATH_MAX * 2];
    snprintf(p1, sizeof(p1), "%s/stow-manager/%s", data_home, filename);
    str_array_append(&search_paths, p1);

    char config_home[PATH_MAX];
    get_xdg_config_home(config_home, sizeof(config_home));
    char p2[PATH_MAX * 2];
    snprintf(p2, sizeof(p2), "%s/stow-manager/%s", config_home, filename);
    str_array_append(&search_paths, p2);

    StringArray data_dirs;
    str_array_init(&data_dirs);
    get_xdg_data_dirs(&data_dirs);
    for (size_t i = 0; i < data_dirs.count; i++) {
        char path[PATH_MAX * 2];
        snprintf(path, sizeof(path), "%s/stow-manager/%s", data_dirs.items[i], filename);
        str_array_append(&search_paths, path);
    }
    str_array_free(&data_dirs);

#ifdef DATADIR
    char p3[PATH_MAX * 2];
    snprintf(p3, sizeof(p3), "%s/stow-manager/%s", STR(DATADIR), filename);
    str_array_append(&search_paths, p3);
#endif

    char p_res[PATH_MAX];
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

    str_array_free(&search_paths);
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

    if (!raw_ignores) {
        return false;
    }

    for (size_t i = 0; i < raw_ignores->count; i++) {
        const char *pat = raw_ignores->items[i];
        if (!pat || *pat == '\0') {
            continue;
        }

        size_t plen = strlen(pat);
        if (strchr(pat, '/') != NULL) {
            if (fnmatch(pat, rel_path, FNM_PATHNAME) == 0) {
                return true;
            }
            if (pat[plen - 1] == '/') {
                if (plen < PATH_MAX) {
                    char dir_pat[PATH_MAX];
                    memcpy(dir_pat, pat, plen - 1);
                    dir_pat[plen - 1] = '\0';
                    if (fnmatch(dir_pat, rel_path, FNM_PATHNAME) == 0 ||
                        strncmp(rel_path, pat, plen) == 0 || strstr(rel_path, pat) != NULL) {
                        return true;
                    }
                }
            }
        } else {
            if (fnmatch(pat, base, 0) == 0 || fnmatch(pat, rel_path, 0) == 0) {
                return true;
            }
        }
    }

    return false;
}
