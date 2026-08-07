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
#ifndef CORE_STOWIGNORE_H
#define CORE_STOWIGNORE_H

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/str.h"

typedef struct {
    char rel_path[PATH_MAX];
    char full_path[PATH_MAX * 2];
    bool is_dir;
} PkgFileEntry;

typedef struct {
    PkgFileEntry *entries;
    size_t count;
    size_t capacity;
} PkgFileList;

void pkg_file_list_init(PkgFileList *list);
void pkg_file_list_free(PkgFileList *list);
void pkg_file_list_append(PkgFileList *list, const char *rel_path, const char *full_path, bool is_dir);
void collect_package_files(const char *pkg_dir, const StringArray *raw_ignores, PkgFileList *list);

void parse_stowignore(const char *dir_path, StringArray *ignore_patterns);
void parse_stowignore_raw(const char *dir_path, StringArray *raw_ignores);
void get_default_stowignore(StringArray *ignore_patterns);
bool is_path_ignored(const char *rel_path, const StringArray *raw_ignores);

#endif /* CORE_STOWIGNORE_H */
