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

static bool is_var_start_char(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c == '_');
}

static bool is_var_body_char(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || (c == '_');
}

void expand_env_vars(const char *src, char *out, size_t out_size)
{
    if (!src || !out || out_size == 0) {
        return;
    }

    size_t srclen = strlen(src);
    size_t o = 0;
    size_t i = 0;

    while (i < srclen && o + 1 < out_size) {
        if (src[i] == '$') {
            if (i + 1 < srclen && src[i + 1] == '{') {
                size_t j = i + 2;
                char varname[256] = {0};
                size_t vn = 0;
                while (j < srclen && src[j] != '}' && vn + 1 < sizeof(varname)) {
                    varname[vn++] = src[j++];
                }
                if (j < srclen && src[j] == '}' && vn > 0) {
                    const char *val = getenv(varname);
                    if (val) {
                        size_t vlen = strlen(val);
                        for (size_t k = 0; k < vlen && o + 1 < out_size; k++) {
                            out[o++] = val[k];
                        }
                    }
                    i = j + 1;
                } else {
                    out[o++] = src[i++];
                }
            } else if (i + 1 < srclen && is_var_start_char(src[i + 1])) {
                size_t j = i + 1;
                char varname[256] = {0};
                size_t vn = 0;
                while (j < srclen && is_var_body_char(src[j]) && vn + 1 < sizeof(varname)) {
                    varname[vn++] = src[j++];
                }
                const char *val = getenv(varname);
                if (val) {
                    size_t vlen = strlen(val);
                    for (size_t k = 0; k < vlen && o + 1 < out_size; k++) {
                        out[o++] = val[k];
                    }
                }
                i = j;
            } else {
                out[o++] = src[i++];
            }
        } else {
            out[o++] = src[i++];
        }
    }
    out[o] = '\0';
}

static const char *get_user_home_dir(void)
{
    const char *home = getenv("HOME");
    return (home && strlen(home) > 0) ? home : NULL;
}

bool get_xdg_config_home(char *buf, size_t buf_size)
{
    if (!buf || buf_size == 0) {
        return false;
    }
    const char *env = getenv("XDG_CONFIG_HOME");
    if (env && strlen(env) > 0) {
        expand_env_vars(env, buf, buf_size);
        return true;
    }
    const char *home = get_user_home_dir();
    if (home) {
        snprintf(buf, buf_size, "%s/.config", home);
        return true;
    }
    buf[0] = '\0';
    return false;
}

bool get_xdg_data_home(char *buf, size_t buf_size)
{
    if (!buf || buf_size == 0) {
        return false;
    }
    const char *env = getenv("XDG_DATA_HOME");
    if (env && strlen(env) > 0) {
        expand_env_vars(env, buf, buf_size);
        return true;
    }
    const char *home = get_user_home_dir();
    if (home) {
        snprintf(buf, buf_size, "%s/.local/share", home);
        return true;
    }
    buf[0] = '\0';
    return false;
}

bool get_xdg_cache_home(char *buf, size_t buf_size)
{
    if (!buf || buf_size == 0) {
        return false;
    }
    const char *env = getenv("XDG_CACHE_HOME");
    if (env && strlen(env) > 0) {
        expand_env_vars(env, buf, buf_size);
        return true;
    }
    const char *home = get_user_home_dir();
    if (home) {
        snprintf(buf, buf_size, "%s/.cache", home);
        return true;
    }
    buf[0] = '\0';
    return false;
}

bool get_xdg_state_home(char *buf, size_t buf_size)
{
    if (!buf || buf_size == 0) {
        return false;
    }
    const char *env = getenv("XDG_STATE_HOME");
    if (env && strlen(env) > 0) {
        expand_env_vars(env, buf, buf_size);
        return true;
    }
    const char *home = get_user_home_dir();
    if (home) {
        snprintf(buf, buf_size, "%s/.local/state", home);
        return true;
    }
    buf[0] = '\0';
    return false;
}

void get_xdg_data_dirs(StringArray *dirs)
{
    const char *env = getenv("XDG_DATA_DIRS");
    if (!env || strlen(env) == 0) {
        env = "/usr/local/share:/usr/share";
    }
    char *copy = safe_strdup(env);

    char *saveptr = NULL;
    char *token = strtok_r(copy, ":", &saveptr);
    while (token) {
        if (strlen(token) > 0) {
            char expanded[PATH_MAX * 2];
            expand_env_vars(token, expanded, sizeof(expanded));
            str_array_append(dirs, expanded);
        }
        token = strtok_r(NULL, ":", &saveptr);
    }
    free(copy);
}

void get_xdg_config_dirs(StringArray *dirs)
{
    const char *env = getenv("XDG_CONFIG_DIRS");
    if (!env || strlen(env) == 0) {
        env = "/etc/xdg";
    }
    char *copy = safe_strdup(env);

    char *saveptr = NULL;
    char *token = strtok_r(copy, ":", &saveptr);
    while (token) {
        if (strlen(token) > 0) {
            char expanded[PATH_MAX * 2];
            expand_env_vars(token, expanded, sizeof(expanded));
            str_array_append(dirs, expanded);
        }
        token = strtok_r(NULL, ":", &saveptr);
    }
    free(copy);
}

void get_distro_id(char *buf, size_t buf_size)
{
    buf[0] = '\0';
    FILE *fp = fopen("/etc/os-release", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "ID=", 3) == 0) {
                char *val = line + 3;
                char *trimmed = trim_whitespace(val);
                snprintf(buf, buf_size, "%s", trimmed);
                fclose(fp);
                return;
            }
        }
        fclose(fp);
    }
    snprintf(buf, buf_size, "unknown");
}

bool is_executable_in_path(const char *executable)
{
    if (!executable || strlen(executable) == 0) {
        return false;
    }

    if (strchr(executable, '/') != NULL) {
        return access(executable, X_OK) == 0;
    }

    const char *path_env = getenv("PATH");
    if (!path_env) {
        return false;
    }

    char *path_copy = safe_strdup(path_env);
    char *saveptr = NULL;
    char *token = strtok_r(path_copy, ":", &saveptr);
    bool found = false;
    char full_path[PATH_MAX * 2];

    while (token) {
        snprintf(full_path, sizeof(full_path), "%s/%s", token, executable);
        if (access(full_path, X_OK) == 0) {
            found = true;
            break;
        }
        token = strtok_r(NULL, ":", &saveptr);
    }

    free(path_copy);
    return found;
}

int run_system_cmd(const char *cmd)
{
    return system(cmd);
}
