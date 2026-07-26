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
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>

static StringArray g_temp_paths = {NULL, 0, 0};

void get_xdg_config_home(char *buf, size_t buf_size) {
    const char *env = getenv("XDG_CONFIG_HOME");
    if (env && strlen(env) > 0 && env[0] == '/') {
        snprintf(buf, buf_size, "%s", env);
    } else {
        const char *home = getenv("HOME");
        if (home && strlen(home) > 0) {
            snprintf(buf, buf_size, "%s/.config", home);
        } else {
            snprintf(buf, buf_size, "/tmp");
        }
    }
}

void get_xdg_data_home(char *buf, size_t buf_size) {
    const char *env = getenv("XDG_DATA_HOME");
    if (env && strlen(env) > 0 && env[0] == '/') {
        snprintf(buf, buf_size, "%s", env);
    } else {
        const char *home = getenv("HOME");
        if (home && strlen(home) > 0) {
            snprintf(buf, buf_size, "%s/.local/share", home);
        } else {
            snprintf(buf, buf_size, "/tmp");
        }
    }
}

void get_xdg_cache_home(char *buf, size_t buf_size) {
    const char *env = getenv("XDG_CACHE_HOME");
    if (env && strlen(env) > 0 && env[0] == '/') {
        snprintf(buf, buf_size, "%s", env);
    } else {
        const char *home = getenv("HOME");
        if (home && strlen(home) > 0) {
            snprintf(buf, buf_size, "%s/.cache", home);
        } else {
            snprintf(buf, buf_size, "/tmp");
        }
    }
}

void get_xdg_state_home(char *buf, size_t buf_size) {
    const char *env = getenv("XDG_STATE_HOME");
    if (env && strlen(env) > 0 && env[0] == '/') {
        snprintf(buf, buf_size, "%s", env);
    } else {
        const char *home = getenv("HOME");
        if (home && strlen(home) > 0) {
            snprintf(buf, buf_size, "%s/.local/state", home);
        } else {
            snprintf(buf, buf_size, "/tmp");
        }
    }
}

void get_xdg_data_dirs(StringArray *dirs) {
    const char *env = getenv("XDG_DATA_DIRS");
    if (!env || strlen(env) == 0) {
        env = "/usr/local/share:/usr/share";
    }
    char *copy = strdup(env);
    if (!copy) return;

    char *saveptr = NULL;
    char *token = strtok_r(copy, ":", &saveptr);
    while (token) {
        if (strlen(token) > 0) {
            str_array_append(dirs, token);
        }
        token = strtok_r(NULL, ":", &saveptr);
    }
    free(copy);
}

void get_xdg_config_dirs(StringArray *dirs) {
    const char *env = getenv("XDG_CONFIG_DIRS");
    if (!env || strlen(env) == 0) {
        env = "/etc/xdg";
    }
    char *copy = strdup(env);
    if (!copy) return;

    char *saveptr = NULL;
    char *token = strtok_r(copy, ":", &saveptr);
    while (token) {
        if (strlen(token) > 0) {
            str_array_append(dirs, token);
        }
        token = strtok_r(NULL, ":", &saveptr);
    }
    free(copy);
}

void register_temp_path(const char *path) {
    if (!path) return;
    if (!str_array_contains(&g_temp_paths, path)) {
        str_array_append(&g_temp_paths, path);
    }
}

void unregister_temp_path(const char *path) {
    if (!path) return;
    StringArray new_paths;
    str_array_init(&new_paths);
    for (size_t i = 0; i < g_temp_paths.count; i++) {
        if (strcmp(g_temp_paths.items[i], path) != 0) {
            str_array_append(&new_paths, g_temp_paths.items[i]);
        }
    }
    str_array_free(&g_temp_paths);
    g_temp_paths = new_paths;
}

void cleanup_temp_paths(void) {
    for (size_t i = 0; i < g_temp_paths.count; i++) {
        const char *p = g_temp_paths.items[i];
        if (is_dir(p)) {
            char rm_cmd[PATH_MAX * 4];
            snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf \"%s\"", p);
            (void)system(rm_cmd);
        } else if (file_exists(p) || is_symlink(p)) {
            unlink(p);
        }
    }
    str_array_free(&g_temp_paths);
}

static void handle_signal_interrupt(int sig) {
    (void)sig;
    log_warn("\nOperation interrupted by user (SIGINT / Ctrl+C). Cleaning up temporary files...");
    cleanup_temp_paths();
    _exit(128 + sig);
}

void setup_signal_handlers(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal_interrupt;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

char *trim_whitespace(char *str) {
    if (!str) return NULL;
    while (isspace((unsigned char)*str)) str++;
    if (*str == '\0') return str;

    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';

    if (end > str && ((*str == '"' && end[0] == '"') || (*str == '\'' && end[0] == '\''))) {
        str++;
        end[0] = '\0';
    }
    return str;
}

bool file_exists(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0);
}

bool is_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    return false;
}

bool is_symlink(const char *path) {
    struct stat st;
    if (lstat(path, &st) == 0) {
        return S_ISLNK(st.st_mode);
    }
    return false;
}

bool is_executable_in_path(const char *executable) {
    if (!executable || strlen(executable) == 0) return false;

    const char *path_env = getenv("PATH");
    if (!path_env) return false;

    char *path_copy = strdup(path_env);
    if (!path_copy) return false;

    char *token = strtok(path_copy, ":");
    bool found = false;
    char full_path[PATH_MAX * 2];

    while (token) {
        snprintf(full_path, sizeof(full_path), "%s/%s", token, executable);
        if (access(full_path, X_OK) == 0) {
            found = true;
            break;
        }
        token = strtok(NULL, ":");
    }

    free(path_copy);
    return found;
}

void normalize_path(char *path) {
    if (!path || *path == '\0') return;
    char *p = path;
    while (*p) {
        if (*p == '/' && *(p + 1) == '/') {
            char *q = p + 1;
            while (*q == '/') q++;
            memmove(p + 1, q, strlen(q) + 1);
        }
        p++;
    }
    size_t len = strlen(path);
    if (len > 1 && path[len - 1] == '/') {
        path[len - 1] = '\0';
    }
}

void collapse_path(char *path) {
    if (!path || *path == '\0') return;
    char temp[PATH_MAX * 2];
    char *out = temp;
    const char *in = path;

    while (*in) {
        if (in[0] == '/' && in[1] == '.' && in[2] == '.' && (in[3] == '/' || in[3] == '\0')) {
            if (out > temp) {
                out--;
                while (out > temp && *out != '/') out--;
            }
            in += 3;
        } else if (in[0] == '/' && in[1] == '.' && (in[2] == '/' || in[2] == '\0')) {
            in += 2;
        } else {
            *out++ = *in++;
        }
    }
    *out = '\0';
    if (temp[0] == '\0') {
        strcpy(path, "/");
    } else {
        strcpy(path, temp);
    }
}

char *read_symlink_target(const char *path) {
    char resolved[PATH_MAX * 2];
    if (realpath(path, resolved)) {
        return strdup(resolved);
    }

    char target[PATH_MAX * 2];
    ssize_t len = readlink(path, target, sizeof(target) - 1);
    if (len != -1) {
        target[len] = '\0';
        if (target[0] != '/') {
            char parent_dir[PATH_MAX * 2];
            snprintf(parent_dir, sizeof(parent_dir), "%s", path);
            char *last_slash = strrchr(parent_dir, '/');
            if (last_slash) {
                *last_slash = '\0';
            } else {
                snprintf(parent_dir, sizeof(parent_dir), ".");
            }

            char abs_target[PATH_MAX * 2];
            join_path(abs_target, sizeof(abs_target), parent_dir, target);

            char real_abs[PATH_MAX * 2];
            if (realpath(abs_target, real_abs) != NULL) {
                return strdup(real_abs);
            }
            normalize_path(abs_target);
            return strdup(abs_target);
        }
        return strdup(target);
    }
    return NULL;
}

void get_distro_id(char *buf, size_t buf_size) {
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

void join_path(char *out, size_t out_size, const char *dir, const char *rel) {
    if (!dir || strlen(dir) == 0) {
        snprintf(out, out_size, "%s", rel ? rel : "");
    } else if (!rel || strlen(rel) == 0) {
        snprintf(out, out_size, "%s", dir);
    } else {
        size_t dlen = strlen(dir);
        if (dir[dlen - 1] == '/') {
            snprintf(out, out_size, "%s%s", dir, rel);
        } else {
            snprintf(out, out_size, "%s/%s", dir, rel);
        }
    }
    normalize_path(out);
}

int mkdir_p(const char *path, mode_t mode) {
    char tmp[PATH_MAX * 2];
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);
    if (len == 0) return -1;
    if (tmp[len - 1] == '/') tmp[len - 1] = '\0';

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (!is_dir(tmp)) {
                if (mkdir(tmp, mode) != 0 && errno != EEXIST) return -1;
            }
            *p = '/';
        }
    }
    if (!is_dir(tmp)) {
        if (mkdir(tmp, mode) != 0 && errno != EEXIST) return -1;
    }
    return 0;
}

bool is_path_prefix(const char *path, const char *prefix) {
    if (!path || !prefix) return false;
    char norm_path[PATH_MAX * 2], norm_prefix[PATH_MAX * 2];
    snprintf(norm_path, sizeof(norm_path), "%s", path);
    snprintf(norm_prefix, sizeof(norm_prefix), "%s", prefix);
    normalize_path(norm_path);
    normalize_path(norm_prefix);

    size_t plen = strlen(norm_prefix);
    if (strncmp(norm_path, norm_prefix, plen) == 0) {
        return norm_path[plen] == '/' || norm_path[plen] == '\0';
    }
    return false;
}

void escape_shell_arg(const char *src, char *dest, size_t dest_size) {
    if (!src || !dest || dest_size == 0) return;
    size_t d = 0;
    dest[d++] = '\'';
    for (size_t i = 0; src[i] != '\0' && d + 4 < dest_size; i++) {
        if (src[i] == '\'') {
            dest[d++] = '\'';
            dest[d++] = '\\';
            dest[d++] = '\'';
            dest[d++] = '\'';
        } else {
            dest[d++] = src[i];
        }
    }
    if (d < dest_size) dest[d++] = '\'';
    dest[d < dest_size ? d : dest_size - 1] = '\0';
}

void expand_tilde_path(const char *path, char *out, size_t out_size) {
    if (!path || !out || out_size == 0) return;
    if (path[0] == '~' && (path[1] == '/' || path[1] == '\0')) {
        const char *home = getenv("HOME");
        if (home) {
            snprintf(out, out_size, "%s%s", home, path + 1);
        } else {
            snprintf(out, out_size, "%s", path);
        }
    } else {
        snprintf(out, out_size, "%s", path);
    }
}

void str_array_init(StringArray *arr) {
    arr->items = NULL;
    arr->count = 0;
    arr->capacity = 0;
}

void str_array_append(StringArray *arr, const char *str) {
    if (!str || strlen(str) == 0) return;
    if (arr->count >= arr->capacity) {
        size_t new_cap = (arr->capacity == 0) ? 8 : arr->capacity * 2;
        char **new_items = realloc(arr->items, new_cap * sizeof(char *));
        if (!new_items) return;
        arr->items = new_items;
        arr->capacity = new_cap;
    }
    arr->items[arr->count++] = strdup(str);
}

bool str_array_contains(const StringArray *arr, const char *str) {
    if (!arr || !str) return false;
    for (size_t i = 0; i < arr->count; i++) {
        if (strcmp(arr->items[i], str) == 0) return true;
    }
    return false;
}

void str_array_free(StringArray *arr) {
    if (!arr) return;
    for (size_t i = 0; i < arr->count; i++) {
        free(arr->items[i]);
    }
    free(arr->items);
    arr->items = NULL;
    arr->count = 0;
    arr->capacity = 0;
}

void get_dotfiles_dir(char *buf, size_t buf_size) {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd))) {
        snprintf(buf, buf_size, "%s", cwd);
    } else {
        snprintf(buf, buf_size, ".");
    }
}

void get_target_dir(char *buf, size_t buf_size) {
    const char *home = getenv("HOME");
    if (home) {
        snprintf(buf, buf_size, "%s", home);
    } else {
        snprintf(buf, buf_size, "/tmp");
    }
}

void get_all_packages(const char *dotfiles_dir, StringArray *packages) {
    DIR *dir = opendir(dotfiles_dir);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0 &&
            strcmp(name, ".git") != 0 && strcmp(name, "scratch") != 0 &&
            strcmp(name, "scripts") != 0 && strcmp(name, "src") != 0 &&
            strcmp(name, "include") != 0 && strcmp(name, "build") != 0 &&
            strcmp(name, "bin") != 0 && strcmp(name, "tests") != 0) {
            char path[PATH_MAX * 2];
            snprintf(path, sizeof(path), "%s/%s", dotfiles_dir, name);
            if (is_dir(path)) {
                str_array_append(packages, name);
            }
        }
    }
    closedir(dir);
}

void walk_dir_symlinks(const char *dir_path, int current_depth, int max_depth, WalkSymlinkCallback cb, void *user_data) {
    if (current_depth > max_depth) return;

    DIR *dir = opendir(dir_path);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        char path[PATH_MAX * 2];
        join_path(path, sizeof(path), dir_path, entry->d_name);

        if (is_symlink(path)) {
            cb(path, user_data);
        } else if (is_dir(path)) {
            walk_dir_symlinks(path, current_depth + 1, max_depth, cb, user_data);
        }
    }
    closedir(dir);
}

void walk_dir_files(const char *base_dir, const char *current_dir, WalkFileCallback cb, void *user_data) {
    char dir_path[PATH_MAX * 2];
    if (current_dir && strlen(current_dir) > 0) {
        join_path(dir_path, sizeof(dir_path), base_dir, current_dir);
    } else {
        snprintf(dir_path, sizeof(dir_path), "%s", base_dir);
    }

    DIR *dir = opendir(dir_path);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        char full_path[PATH_MAX * 2];
        join_path(full_path, sizeof(full_path), dir_path, entry->d_name);

        char rel_path[PATH_MAX * 2];
        if (current_dir && strlen(current_dir) > 0) {
            snprintf(rel_path, sizeof(rel_path), "%s/%s", current_dir, entry->d_name);
        } else {
            snprintf(rel_path, sizeof(rel_path), "%s", entry->d_name);
        }

        if (is_dir(full_path)) {
            walk_dir_files(base_dir, rel_path, cb, user_data);
        } else {
            cb(full_path, rel_path, user_data);
        }
    }
    closedir(dir);
}

int run_system_cmd(const char *cmd) {
    return system(cmd);
}
