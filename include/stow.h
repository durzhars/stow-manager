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

#ifndef STOW_ENGINE_H
#define STOW_ENGINE_H

#include "checker.h"

typedef enum {
    STOW_STATUS_UNSTOWED = 0,
    STOW_STATUS_PARTIAL,
    STOW_STATUS_STOWED
} StowStatus;

void parse_stowignore(const char *dir_path, StringArray *ignore_patterns);
void parse_stowignore_raw(const char *dir_path, StringArray *raw_ignores);
bool is_path_ignored(const char *rel_path, const StringArray *raw_ignores);

void unfold_directory_symlinks(const char *target_dir, const char *dotfiles_dir, bool dry_run);
void prepare_target_conflicts(const char *target_dir, const char *dotfiles_dir, const char *pkg_name, bool dry_run);

StowStatus get_package_stow_status(const char *target_dir, const char *dotfiles_dir, const char *pkg_name);
bool is_package_stowed(const char *target_dir, const char *dotfiles_dir, const char *pkg_name);
void handle_mutual_exclusions(const char *target_dir, const char *dotfiles_dir, const char *pkg_name, bool dry_run);

int stow_package(const char *dotfiles_dir, const char *target_dir, const char *pkg_name, bool auto_install, bool dry_run);
int unstow_package(const char *dotfiles_dir, const char *target_dir, const char *pkg_name, bool dry_run);
int restow_package(const char *dotfiles_dir, const char *target_dir, const char *pkg_name, bool auto_install, bool dry_run);

void stow_all_packages(const char *dotfiles_dir, const char *target_dir, bool auto_install, bool dry_run);
void list_packages_status(const char *dotfiles_dir, const char *target_dir);

#endif /* STOW_ENGINE_H */
