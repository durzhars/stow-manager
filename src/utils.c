#define _GNU_SOURCE
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "utils.h"
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>

#define MAX_TEMP_PATHS 32
static char g_temp_paths[MAX_TEMP_PATHS][PATH_MAX];
static volatile sig_atomic_t g_temp_paths_count = 0;

void register_temp_path(const char *path) {
    if (!path || strlen(path) == 0) return;

    int empty_slot = -1;
    for (size_t i = 0; i < (size_t)g_temp_paths_count; i++) {
        if (g_temp_paths[i][0] != '\0' && strcmp(g_temp_paths[i], path) == 0) {
            return;
        }
        if (g_temp_paths[i][0] == '\0' && empty_slot == -1) {
            empty_slot = (int)i;
        }
    }

    if (empty_slot != -1) {
        snprintf(g_temp_paths[empty_slot], PATH_MAX, "%s", path);
    } else if ((size_t)g_temp_paths_count < MAX_TEMP_PATHS) {
        snprintf(g_temp_paths[g_temp_paths_count], PATH_MAX, "%s", path);
        g_temp_paths_count++;
    }
}

void unregister_temp_path(const char *path) {
    if (!path) return;
    for (size_t i = 0; i < (size_t)g_temp_paths_count; i++) {
        if (g_temp_paths[i][0] != '\0' && strcmp(g_temp_paths[i], path) == 0) {
            g_temp_paths[i][0] = '\0';
            break;
        }
    }
}

void cleanup_temp_paths(void) {
    for (size_t i = 0; i < (size_t)g_temp_paths_count; i++) {
        if (g_temp_paths[i][0] == '\0') continue;
        const char *p = g_temp_paths[i];
        if (is_dir(p)) {
            char rm_cmd[PATH_MAX * 2 + 32];
            snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf \"%s\"", p);
            (void)system(rm_cmd);
        } else if (file_exists(p) || is_symlink(p)) {
            unlink(p);
        }
        g_temp_paths[i][0] = '\0';
    }
    g_temp_paths_count = 0;
}

static void handle_signal_interrupt(int sig) {
    (void)sig;
    const char msg[] = "\nOperation interrupted by user (SIGINT / Ctrl+C). Cleaning up temporary files...\n";
    (void)write(STDERR_FILENO, msg, sizeof(msg) - 1);

    for (size_t i = 0; i < (size_t)g_temp_paths_count; i++) {
        if (g_temp_paths[i][0] != '\0') {
            unlink(g_temp_paths[i]);
            rmdir(g_temp_paths[i]);
        }
    }
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
    char *dup = strdup(str);
    if (!dup) return;
    arr->items[arr->count++] = dup;
}

bool str_array_contains(const StringArray *arr, const char *str) {
    if (!arr || !str || !arr->items) return false;
    for (size_t i = 0; i < arr->count; i++) {
        if (arr->items[i] && strcmp(arr->items[i], str) == 0) return true;
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

    StringArray ignored;
    str_array_init(&ignored);

    char ignore_file[PATH_MAX * 2];
    join_path(ignore_file, sizeof(ignore_file), dotfiles_dir, ".stowignore");
    FILE *fp = fopen(ignore_file, "r");
    if (fp) {
        char *linebuf = NULL;
        size_t linecap = 0;
        ssize_t linelen;
        while ((linelen = getline(&linebuf, &linecap, fp)) != -1) {
            char *trimmed = trim_whitespace(linebuf);
            if (trimmed[0] != '#' && trimmed[0] != '\0') {
                str_array_append(&ignored, trimmed);
            }
        }
        free(linebuf);
        fclose(fp);
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0 || strcmp(name, ".git") == 0) continue;
        if (!file_exists(ignore_file) && (strcmp(name, "build") == 0 || strcmp(name, "bin") == 0)) continue;
        if (str_array_contains(&ignored, name)) continue;

        char path[PATH_MAX * 2];
        join_path(path, sizeof(path), dotfiles_dir, name);
        if (is_dir(path)) {
            str_array_append(packages, name);
        }
    }
    closedir(dir);
    str_array_free(&ignored);
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
