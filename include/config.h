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

#ifndef CONFIG_H
#define CONFIG_H

#include "utils/mem.h"
#include <limits.h>

typedef struct {
    char config_file_path[PATH_MAX];
    StringArray dotfiles_dirs;
    char target_dir[PATH_MAX];
} Config;

void config_init(Config *cfg);
void config_free(Config *cfg);
void get_config_file_path(char *buf, size_t buf_size);
bool config_load(Config *cfg);
bool config_save(const Config *cfg);

void config_set_dotfiles_dir(const char *path);
void config_add_dotfiles_dir(const char *path);
void config_remove_dotfiles_dir(const char *path);
void config_set_target_dir(const char *path);
void config_show(void);

void get_active_dotfiles_dir(const char *cli_override, char *buf, size_t buf_size);
void get_active_target_dir(const char *cli_override, char *buf, size_t buf_size);
void get_active_target_dir_for_pkg(const char *cli_override,
                                   const char *dotfiles_dir,
                                   const char *pkg_name,
                                   char *buf,
                                   size_t buf_size);

#endif /* CONFIG_H */
