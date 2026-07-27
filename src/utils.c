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
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>

volatile sig_atomic_t g_interrupted = 0;

#define MAX_SIGNAL_TEMP_PATHS 64
static char g_signal_temp_paths[MAX_SIGNAL_TEMP_PATHS][PATH_MAX];
static volatile sig_atomic_t g_signal_temp_count = 0;

static StringArray g_temp_paths = {NULL, 0, 0};

void *safe_malloc(size_t size)
{
    if (size == 0)
        return NULL;
    void *ptr = malloc(size);
    if (!ptr) {
        log_error("Out of memory! Failed to allocate %zu bytes", size);
        exit(EXIT_FAILURE);
    }
    return ptr;
}

void *safe_calloc(size_t num, size_t size)
{
    if (num == 0 || size == 0)
        return NULL;
    void *ptr = calloc(num, size);
    if (!ptr) {
        log_error("Out of memory! Failed to allocate %zu x %zu bytes", num, size);
        exit(EXIT_FAILURE);
    }
    return ptr;
}

void *safe_realloc(void *ptr, size_t size)
{
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    void *new_ptr = realloc(ptr, size);
    if (!new_ptr) {
        log_error("Out of memory! Failed to reallocate %zu bytes", size);
        exit(EXIT_FAILURE);
    }
    return new_ptr;
}

char *safe_strdup(const char *s)
{
    if (!s)
        return NULL;
    char *dup = strdup(s);
    if (!dup) {
        log_error("Out of memory! Failed to duplicate string");
        exit(EXIT_FAILURE);
    }
    return dup;
}

void get_xdg_config_home(char *buf, size_t buf_size)
{
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

void get_xdg_data_home(char *buf, size_t buf_size)
{
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

void get_xdg_cache_home(char *buf, size_t buf_size)
{
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

void get_xdg_state_home(char *buf, size_t buf_size)
{
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
            str_array_append(dirs, token);
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
            str_array_append(dirs, token);
        }
        token = strtok_r(NULL, ":", &saveptr);
    }
    free(copy);
}

void register_temp_path(const char *path)
{
    if (!path)
        return;
    if (!str_array_contains(&g_temp_paths, path)) {
        str_array_append(&g_temp_paths, path);
    }

    if (g_signal_temp_count < MAX_SIGNAL_TEMP_PATHS) {
        snprintf(
            g_signal_temp_paths[g_signal_temp_count], sizeof(g_signal_temp_paths[0]), "%s", path);
        g_signal_temp_count++;
    }
}

void unregister_temp_path(const char *path)
{
    if (!path)
        return;
    StringArray new_paths;
    str_array_init(&new_paths);
    for (size_t i = 0; i < g_temp_paths.count; i++) {
        if (strcmp(g_temp_paths.items[i], path) != 0) {
            str_array_append(&new_paths, g_temp_paths.items[i]);
        }
    }
    str_array_free(&g_temp_paths);
    g_temp_paths = new_paths;

    for (sig_atomic_t i = 0; i < g_signal_temp_count; i++) {
        if (strcmp(g_signal_temp_paths[i], path) == 0) {
            g_signal_temp_paths[i][0] = '\0';
        }
    }
}

void cleanup_temp_paths(void)
{
    for (size_t i = 0; i < g_temp_paths.count; i++) {
        const char *p = g_temp_paths.items[i];
        if (is_dir(p)) {
            char escaped[PATH_MAX * 3];
            escape_shell_arg(p, escaped, sizeof(escaped));
            char rm_cmd[PATH_MAX * 3 + 32];
            snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf %s", escaped);
            (void)system(rm_cmd);
        } else if (file_exists(p) || is_symlink(p)) {
            unlink(p);
        }
    }
    str_array_free(&g_temp_paths);
}

void cleanup_temp_paths_signal_safe(void)
{
    for (sig_atomic_t i = 0; i < g_signal_temp_count; i++) {
        const char *p = g_signal_temp_paths[i];
        if (p && p[0] != '\0') {
            (void)unlink(p);
            (void)rmdir(p);
        }
    }
}

static void handle_signal_interrupt(int sig)
{
    g_interrupted = sig;
    const char msg[] =
        "\n[WARNING] Operation interrupted by signal (Ctrl+C). Cleaning up temporary files...\n";
    (void)write(STDERR_FILENO, msg, sizeof(msg) - 1);
    cleanup_temp_paths_signal_safe();
    _exit(128 + sig);
}

void setup_signal_handlers(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal_interrupt;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

char *trim_whitespace(char *str)
{
    if (!str) {
        return NULL;
    }
    while (isspace((unsigned char)*str)) {
        str++;
    }
    if (*str == '\0') {
        return str;
    }

    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        end--;
    }
    end[1] = '\0';

    if (end > str && ((*str == '"' && end[0] == '"') || (*str == '\'' && end[0] == '\''))) {
        str++;
        end[0] = '\0';
    }
    return str;
}

bool file_exists(const char *path)
{
    struct stat st;
    return (stat(path, &st) == 0);
}

bool is_dir(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    return false;
}

bool is_symlink(const char *path)
{
    struct stat st;
    if (lstat(path, &st) == 0) {
        return S_ISLNK(st.st_mode);
    }
    return false;
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

void normalize_path(char *path)
{
    if (!path || *path == '\0') {
        return;
    }
    char *p = path;
    while (*p) {
        if (*p == '/' && *(p + 1) == '/') {
            char *q = p + 1;
            while (*q == '/') {
                q++;
            }
            memmove(p + 1, q, strlen(q) + 1);
        }
        p++;
    }
    size_t len = strlen(path);
    if (len > 1 && path[len - 1] == '/') {
        path[len - 1] = '\0';
    }
}

void collapse_path(char *path)
{
    if (!path || *path == '\0') {
        return;
    }
    char temp[PATH_MAX * 2];
    char *out = temp;
    const char *in = path;

    while (*in) {
        if (in[0] == '/' && in[1] == '.' && in[2] == '.' && (in[3] == '/' || in[3] == '\0')) {
            if (out > temp) {
                out--;
                while (out > temp && *out != '/') {
                    out--;
                }
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
        memmove(path, "/", 2);
    } else {
        memmove(path, temp, strlen(temp) + 1);
    }
}

char *read_symlink_target(const char *path)
{
    char resolved[PATH_MAX * 2];
    if (realpath(path, resolved)) {
        return safe_strdup(resolved);
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
                return safe_strdup(real_abs);
            }
            normalize_path(abs_target);
            return safe_strdup(abs_target);
        }
        return safe_strdup(target);
    }
    return NULL;
}

bool is_symlink_pointing_to(const char *symlink_path,
                            const char *pkg_file_path,
                            const char *real_pkg_file_path)
{
    if (!symlink_path || !is_symlink(symlink_path) || !pkg_file_path) {
        return false;
    }

    char raw_link[PATH_MAX * 2];
    ssize_t len = readlink(symlink_path, raw_link, sizeof(raw_link) - 1);
    if (len == -1) {
        return false;
    }
    raw_link[len] = '\0';

    char one_level_target[PATH_MAX * 2];
    if (raw_link[0] == '/') {
        snprintf(one_level_target, sizeof(one_level_target), "%s", raw_link);
    } else {
        char parent_dir[PATH_MAX * 2];
        snprintf(parent_dir, sizeof(parent_dir), "%s", symlink_path);
        char *last_slash = strrchr(parent_dir, '/');
        if (last_slash) {
            *last_slash = '\0';
        } else {
            snprintf(parent_dir, sizeof(parent_dir), ".");
        }
        join_path(one_level_target, sizeof(one_level_target), parent_dir, raw_link);
    }
    collapse_path(one_level_target);
    normalize_path(one_level_target);

    char norm_pkg_file[PATH_MAX * 2];
    snprintf(norm_pkg_file, sizeof(norm_pkg_file), "%s", pkg_file_path);
    normalize_path(norm_pkg_file);

    if (strcmp(one_level_target, norm_pkg_file) == 0) {
        return true;
    }

    if (real_pkg_file_path && strlen(real_pkg_file_path) > 0) {
        char norm_real_pkg_file[PATH_MAX * 2];
        snprintf(norm_real_pkg_file, sizeof(norm_real_pkg_file), "%s", real_pkg_file_path);
        normalize_path(norm_real_pkg_file);
        if (strcmp(one_level_target, norm_real_pkg_file) == 0) {
            return true;
        }
    }

    char resolved_target[PATH_MAX * 2];
    char resolved_pkg_file[PATH_MAX * 2];
    if (realpath(symlink_path, resolved_target) != NULL &&
        realpath(pkg_file_path, resolved_pkg_file) != NULL) {
        if (strcmp(resolved_target, resolved_pkg_file) == 0) {
            return true;
        }
    }

    return false;
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

void join_path(char *out, size_t out_size, const char *dir, const char *rel)
{
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

int mkdir_p(const char *path, mode_t mode)
{
    char tmp[PATH_MAX * 2];
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);
    if (len == 0) {
        return -1;
    }
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
    }

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (!is_dir(tmp)) {
                if (mkdir(tmp, mode) != 0 && errno != EEXIST) {
                    return -1;
                }
            }
            *p = '/';
        }
    }
    if (!is_dir(tmp)) {
        if (mkdir(tmp, mode) != 0 && errno != EEXIST) {
            return -1;
        }
    }
    return 0;
}

bool is_path_prefix(const char *path, const char *prefix)
{
    if (!path || !prefix) {
        return false;
    }
    char norm_path[PATH_MAX * 2];
    char norm_prefix[PATH_MAX * 2];
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

void escape_shell_arg(const char *src, char *dest, size_t dest_size)
{
    if (!src || !dest || dest_size == 0) {
        return;
    }
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
    if (d < dest_size) {
        dest[d++] = '\'';
    }
    dest[d < dest_size ? d : dest_size - 1] = '\0';
}

void expand_tilde_path(const char *path, char *out, size_t out_size)
{
    if (!path || !out || out_size == 0) {
        return;
    }
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

void str_array_init(StringArray *arr)
{
    arr->items = NULL;
    arr->count = 0;
    arr->capacity = 0;
}

void str_array_append(StringArray *arr, const char *str)
{
    if (!str || strlen(str) == 0) {
        return;
    }
    if (arr->count >= arr->capacity) {
        size_t new_cap = (arr->capacity == 0) ? 8 : arr->capacity * 2;
        char **new_items = (char **)safe_realloc((void *)arr->items, new_cap * sizeof(char *));
        arr->items = new_items;
        arr->capacity = new_cap;
    }
    arr->items[arr->count++] = safe_strdup(str);
}

bool str_array_contains(const StringArray *arr, const char *str)
{
    if (!arr || !str) {
        return false;
    }
    for (size_t i = 0; i < arr->count; i++) {
        if (strcmp(arr->items[i], str) == 0) {
            return true;
        }
    }
    return false;
}

void str_array_free(StringArray *arr)
{
    if (!arr) {
        return;
    }
    for (size_t i = 0; i < arr->count; i++) {
        free(arr->items[i]);
    }
    free((void *)arr->items);
    arr->items = NULL;
    arr->count = 0;
    arr->capacity = 0;
}

void get_dotfiles_dir(char *buf, size_t buf_size)
{
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd))) {
        snprintf(buf, buf_size, "%s", cwd);
    } else {
        snprintf(buf, buf_size, ".");
    }
}

void get_target_dir(char *buf, size_t buf_size)
{
    const char *home = getenv("HOME");
    if (home) {
        snprintf(buf, buf_size, "%s", home);
    } else {
        snprintf(buf, buf_size, "/tmp");
    }
}

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
    snprintf(p3, sizeof(p3), "%s/stow-manager/%s", DATADIR, filename);
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
            ".stowdeps", ".stowignore", ".git",       ".gitignore", ".gitattributes",
            ".gitmodules", ".DS_Store",  ".cvsignore", "CVS",        ".svn",
            ".hg",       ".hgignore",   ".hgtags",    "_darcs",     "README*",
            "LICENSE*",  "COPYING*",    "*~",         "#*#",        ".#*"
        };
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
    if (!rel_path || strlen(rel_path) == 0) {
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
        if (!pat || strlen(pat) == 0) {
            continue;
        }

        if (strchr(pat, '/') != NULL) {
            if (fnmatch(pat, rel_path, FNM_PATHNAME) == 0) {
                return true;
            }
            size_t plen = strlen(pat);
            if (pat[plen - 1] == '/') {
                char dir_pat[PATH_MAX];
                snprintf(dir_pat, sizeof(dir_pat), "%.*s", (int)(plen - 1), pat);
                if (fnmatch(dir_pat, rel_path, FNM_PATHNAME) == 0 ||
                    strncmp(rel_path, pat, plen) == 0 || strstr(rel_path, pat) != NULL) {
                    return true;
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

void get_all_packages(const char *dotfiles_dir, StringArray *packages)
{
    DIR *dir = opendir(dotfiles_dir);
    if (!dir) {
        return;
    }

    StringArray ignore_patterns;
    str_array_init(&ignore_patterns);
    get_default_stowignore(&ignore_patterns);
    parse_stowignore_raw(dotfiles_dir, &ignore_patterns);

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
            continue;
        }
        if (!is_path_ignored(name, &ignore_patterns)) {
            char path[PATH_MAX * 2];
            join_path(path, sizeof(path), dotfiles_dir, name);
            if (is_dir(path) && !is_symlink(path)) {
                str_array_append(packages, name);
            }
        }
    }

    str_array_free(&ignore_patterns);
    closedir(dir);
}

void walk_dir_symlinks(const char *dir_path,
                       int current_depth,
                       int max_depth,
                       WalkSymlinkCallback cb,
                       void *user_data)
{
    if (current_depth > max_depth) {
        return;
    }

    DIR *dir = opendir(dir_path);
    if (!dir) {
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char path[PATH_MAX * 2];
        join_path(path, sizeof(path), dir_path, entry->d_name);

        if (is_symlink(path)) {
            cb(path, user_data);
        } else if (is_dir(path) && !is_symlink(path)) {
            walk_dir_symlinks(path, current_depth + 1, max_depth, cb, user_data);
        }
    }
    closedir(dir);
}

void walk_dir_files(const char *base_dir,
                    const char *current_dir,
                    WalkFileCallback cb,
                    void *user_data)
{
    char dir_path[PATH_MAX * 2];
    if (current_dir && strlen(current_dir) > 0) {
        join_path(dir_path, sizeof(dir_path), base_dir, current_dir);
    } else {
        snprintf(dir_path, sizeof(dir_path), "%s", base_dir);
    }

    DIR *dir = opendir(dir_path);
    if (!dir) {
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[PATH_MAX * 2];
        join_path(full_path, sizeof(full_path), dir_path, entry->d_name);

        char rel_path[PATH_MAX * 2];
        if (current_dir && strlen(current_dir) > 0) {
            snprintf(rel_path, sizeof(rel_path), "%s/%s", current_dir, entry->d_name);
        } else {
            snprintf(rel_path, sizeof(rel_path), "%s", entry->d_name);
        }

        if (is_dir(full_path) && !is_symlink(full_path)) {
            walk_dir_files(base_dir, rel_path, cb, user_data);
        } else {
            cb(full_path, rel_path, user_data);
        }
    }
    closedir(dir);
}

int run_system_cmd(const char *cmd)
{
    return system(cmd);
}
