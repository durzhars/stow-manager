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

#include <limits.h>
#include <pwd.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "utils.h"

typedef struct {
    const char *env_var;
    const char *default_rel;
} XdgMapping;

static bool is_var_start_char(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c == '_');
}

static bool is_var_body_char(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || (c == '_');
}

static const XdgMapping XDG_TABLE[] = {
    [XDG_CONFIG] = {"XDG_CONFIG_HOME", ".config"},
    [XDG_DATA] = {"XDG_DATA_HOME", ".local/share"},
    [XDG_CACHE] = {"XDG_CACHE_HOME", ".cache"},
    [XDG_STATE] = {"XDG_STATE_HOME", ".local/state"},
};

static bool
resolve_xdg_path(const char *env_var, const char *default_rel, char *buf, size_t buf_size)
{
    if (!buf || buf_size == 0) {
        return false;
    }

    const char *env = getenv(env_var);
    if (env && strlen(env) > 0) {
        char expanded[PATH_MAX * 2];
        expand_env_vars(env, expanded, sizeof(expanded));
        if (expanded[0] == '/') {
            snprintf(buf, buf_size, "%s", expanded);
            return true;
        }
    }

    char home[PATH_MAX];

    if (get_user_home_dir(home, sizeof(home))) {
        join_path(buf, buf_size, home, default_rel);
        return true;
    }

    return false;
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

PathSanityResult verify_path_sanity(const char *path)
{
    if (!path || *path == '\0') {
        return ERR_PATH_EMPTY;
    }

    if (path[0] != '/') {
        return ERR_NOT_ABSOLUTE;
    }

    struct stat st;

    if (stat(path, &st) != 0) {
        return ERR_INSUFFICIENT_PERMS;
    }

    if (!S_ISDIR(st.st_mode)) {
        return ERR_NOT_A_DIRECTORY;
    }

    if (st.st_uid != getuid()) {
        return ERR_NOT_OWNED_BY_USER;
    }

    if (st.st_mode & S_IWOTH) {
        return ERR_WORLD_WRITABLE;
    }

    if (access(path, R_OK | W_OK | X_OK) != 0) {
        return ERR_INSUFFICIENT_PERMS;
    }

    return PATH_VALID;
}

bool get_user_home_dir(char *buf, size_t buf_size)
{
    if (!buf || buf_size == 0) {
        return false;
    }

    const char *home = getenv("HOME");
    if (home && verify_path_sanity(home) == PATH_VALID) {
        snprintf(buf, buf_size, "%s", home);
        return true;
    }

    struct passwd pwd;
    struct passwd *result = NULL;
    char pwbuf[1024];

    if (getpwuid_r(getuid(), &pwd, pwbuf, sizeof(pwbuf), &result) == 0 && result != NULL) {
        if (pwd.pw_dir && verify_path_sanity(pwd.pw_dir) == PATH_VALID) {
            snprintf(buf, buf_size, "%s", pwd.pw_dir);
            return true;
        }
    }

    buf[0] = '\0';
    return false;
}

const char *path_sanity_strerror(PathSanityResult res)
{
    switch (res) {
    case PATH_VALID:
        return "path is valid";
    case ERR_PATH_EMPTY:
        return "path string is empty or NULL";
    case ERR_NOT_ABSOLUTE:
        return "path is not an absolute path (must start with '/')";
    case ERR_NOT_A_DIRECTORY:
        return "path is not a directory (e.g. /dev/null or regular file)";
    case ERR_NOT_OWNED_BY_USER:
        return "directory owner UID does not match running process UID";
    case ERR_WORLD_WRITABLE:
        return "directory is world-writable (security violation, e.g. 1777 /tmp)";
    case ERR_INSUFFICIENT_PERMS:
        return "insufficient permissions (requires read, write, and search/execute access)";
    default:
        return "unknown path sanity error";
    }
}

PathSanityResult verify_home_path_sanity(const char *path)
{
    return verify_path_sanity(path);
}

bool get_xdg_config_home(char *buf, size_t buf_size)
{
    return get_xdg_dir(XDG_CONFIG, buf, buf_size);
}

bool get_xdg_data_home(char *buf, size_t buf_size)
{
    return get_xdg_dir(XDG_DATA, buf, buf_size);
}

bool get_xdg_cache_home(char *buf, size_t buf_size)
{
    return get_xdg_dir(XDG_CACHE, buf, buf_size);
}

bool get_xdg_state_home(char *buf, size_t buf_size)
{
    return get_xdg_dir(XDG_STATE, buf, buf_size);
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

void app_env_init(AppEnvironment *env)
{
    if (!env) {
        return;
    }
    memset(env->home_dir, 0, sizeof(env->home_dir));
    memset(env->target_dir, 0, sizeof(env->target_dir));
    memset(env->xdg_config_home, 0, sizeof(env->xdg_config_home));
    memset(env->xdg_data_home, 0, sizeof(env->xdg_data_home));
    memset(env->xdg_cache_home, 0, sizeof(env->xdg_cache_home));
    memset(env->xdg_state_home, 0, sizeof(env->xdg_state_home));
    env->is_home_validated = false;
    env->is_target_override = false;
}

bool app_env_resolve(AppEnvironment *env, const char *cli_target_override)
{
    if (!env) {
        return false;
    }
    app_env_init(env);

    // Phase 1 & 5: CLI Target Override
    if (cli_target_override && strlen(cli_target_override) > 0) {
        expand_tilde_path(cli_target_override, env->target_dir, sizeof(env->target_dir));
        normalize_path(env->target_dir);
        env->is_target_override = true;
    }

    // Phase 2 & 3: Resolve & Validate Raw $HOME Candidate
    env->is_home_validated = get_user_home_dir(env->home_dir, sizeof(env->home_dir));

    if (!env->is_home_validated) {
        if (!env->is_target_override) {
            const char *raw_home = getenv("HOME");
            PathSanityResult reason = verify_home_path_sanity(raw_home);
            log_error("Fatal: Invalid or missing $HOME directory (%s). Exiting.",
                      path_sanity_strerror(reason));
            return false;
        }
    } else if (!env->is_target_override) {
        snprintf(env->target_dir, sizeof(env->target_dir), "%s", env->home_dir);
    }

    // Phase 4: Derive XDG Base Directories
    get_xdg_config_home(env->xdg_config_home, sizeof(env->xdg_config_home));
    get_xdg_data_home(env->xdg_data_home, sizeof(env->xdg_data_home));
    get_xdg_cache_home(env->xdg_cache_home, sizeof(env->xdg_cache_home));
    get_xdg_state_home(env->xdg_state_home, sizeof(env->xdg_state_home));

    return true;
}

bool get_xdg_dir(XdgDirType type, char *buf, size_t buf_size)
{
    if (type < XDG_CONFIG || type > XDG_STATE) {
        return false;
    }

    const XdgMapping *spec = &XDG_TABLE[type];
    return resolve_xdg_path(spec->env_var, spec->default_rel, buf, buf_size);
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
