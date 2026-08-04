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
#ifndef UTILS_PATH_H
#define UTILS_PATH_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

void normalize_path(char *path);
int collapse_path(char *path);
int join_path(char *out, size_t out_size, const char *dir, const char *rel);
int is_path_prefix(const char *path, const char *prefix);
void expand_tilde_path(const char *path, char *out, size_t out_size);

void get_dotfiles_dir(char *buf, size_t buf_size);
bool get_target_dir(char *buf, size_t buf_size);

#endif /* UTILS_PATH_H */
