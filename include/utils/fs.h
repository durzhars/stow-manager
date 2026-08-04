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
#ifndef UTILS_FS_H
#define UTILS_FS_H

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/stat.h>

#include "utils/mem.h"

bool file_exists(const char *path);
FILE *open_resource_file(const char *filename);
bool is_dir(const char *path);
bool is_symlink(const char *path);
bool is_executable_in_path(const char *executable);
char *read_symlink_target(const char *path);
bool is_symlink_pointing_to(const char *symlink_path,
                            const char *pkg_file_path,
                            const char *real_pkg_file_path);
int mkdir_p(const char *path, mode_t mode);

void get_all_packages(const char *dotfiles_dir, StringArray *packages);

typedef void (*WalkSymlinkCallback)(const char *symlink_path, void *user_data);
typedef void (*WalkFileCallback)(const char *file_path, const char *rel_path, void *user_data);

void cleanup_temp_dir_contents(const char *dir_path);

void walk_dir_symlinks(const char *dir_path,
                       int current_depth,
                       int max_depth,
                       WalkSymlinkCallback cb,
                       void *user_data);
void walk_dir_files(const char *base_dir,
                    const char *current_dir,
                    WalkFileCallback cb,
                    void *user_data);

int run_system_cmd(const char *cmd);

#endif /* UTILS_FS_H */
