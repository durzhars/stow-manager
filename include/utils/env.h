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
#ifndef UTILS_ENV_H
#define UTILS_ENV_H

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/mem.h"

typedef enum {
    PATH_VALID = 0,
    ERR_PATH_EMPTY,
    ERR_NOT_ABSOLUTE,
    ERR_NOT_A_DIRECTORY,
    ERR_NOT_OWNED_BY_USER,
    ERR_WORLD_WRITABLE,
    ERR_INSUFFICIENT_PERMS
} PathSanityResult;

PathSanityResult verify_path_sanity(const char *path);
PathSanityResult verify_home_path_sanity(const char *path);
const char *path_sanity_strerror(PathSanityResult res, const char *path);
bool get_user_home_dir(char *buf, size_t buf_size);

typedef enum { XDG_CONFIG = 0, XDG_DATA, XDG_CACHE, XDG_STATE } XdgDirType;
bool get_xdg_dir(XdgDirType type, char *buf, size_t buf_size);
bool get_xdg_config_home(char *buf, size_t buf_size);
bool get_xdg_data_home(char *buf, size_t buf_size);
bool get_xdg_cache_home(char *buf, size_t buf_size);
bool get_xdg_state_home(char *buf, size_t buf_size);
void get_xdg_data_dirs(StringArray *dirs);
void get_xdg_config_dirs(StringArray *dirs);

typedef struct {
    char home_dir[PATH_MAX];
    char target_dir[PATH_MAX];
    char xdg_config_home[PATH_MAX];
    char xdg_data_home[PATH_MAX];
    char xdg_cache_home[PATH_MAX];
    char xdg_state_home[PATH_MAX];
    bool is_home_validated;
    bool is_target_override;
} AppEnvironment;

void app_env_init(AppEnvironment *env);
bool app_env_resolve(AppEnvironment *env, const char *cli_target_override);

void expand_env_vars(const char *src, char *out, size_t out_size);
void get_distro_id(char *buf, size_t buf_size);

#endif /* UTILS_ENV_H */
