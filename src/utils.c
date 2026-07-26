#define _GNU_SOURCE
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "utils.h"
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>

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
    char full_path[PATH_MAX];

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

char *read_symlink_target(const char *path) {
    char resolved[PATH_MAX];
    if (realpath(path, resolved)) {
        return strdup(resolved);
    }

    char target[PATH_MAX];
    ssize_t len = readlink(path, target, sizeof(target) - 1);
    if (len != -1) {
        target[len] = '\0';
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

void normalize_path(char *path) {
    if (!path) return;
    size_t len = strlen(path);
    while (len > 1 && path[len - 1] == '/') {
        path[--len] = '\0';
    }
}

void join_path(char *out, size_t out_size, const char *dir, const char *rel) {
    if (!dir || strlen(dir) == 0) {
        snprintf(out, out_size, "%s", rel ? rel : "");
        normalize_path(out);
        return;
    }
    if (!rel || strlen(rel) == 0) {
        snprintf(out, out_size, "%s", dir);
        normalize_path(out);
        return;
    }

    char clean_dir[PATH_MAX];
    snprintf(clean_dir, sizeof(clean_dir), "%s", dir);
    normalize_path(clean_dir);

    while (*rel == '/') rel++;

    snprintf(out, out_size, "%s/%s", clean_dir, rel);
}

int mkdir_p(const char *path, mode_t mode) {
    if (!path || strlen(path) == 0) return -1;

    char temp[PATH_MAX];
    snprintf(temp, sizeof(temp), "%s", path);
    normalize_path(temp);
    size_t len = strlen(temp);

    if (len == 0) return 0;

    char *p = temp;
    if (*p == '/') p++;

    for (; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (strlen(temp) > 0 && !is_dir(temp)) {
                if (mkdir(temp, mode) != 0 && errno != EEXIST) {
                    return -1;
                }
            }
            *p = '/';
        }
    }

    if (strlen(temp) > 0 && !is_dir(temp)) {
        if (mkdir(temp, mode) != 0 && errno != EEXIST) {
            return -1;
        }
    }

    return 0;
}

bool is_path_prefix(const char *path, const char *prefix) {
    if (!path || !prefix) return false;
    size_t len = strlen(prefix);
    if (strncmp(path, prefix, len) != 0) return false;
    return (path[len] == '/' || path[len] == '\0' || (len > 0 && prefix[len - 1] == '/'));
}

void escape_shell_arg(const char *src, char *dest, size_t dest_size) {
    if (!src || !dest || dest_size == 0) return;
    size_t di = 0;
    for (size_t si = 0; src[si] != '\0' && di + 2 < dest_size; si++) {
        char c = src[si];
        if (c == '"' || c == '$' || c == '`' || c == '\\' || c == '!') {
            dest[di++] = '\\';
        }
        dest[di++] = c;
    }
    dest[di] = '\0';
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
        if (!new_items) {
            log_error("Failed to allocate memory in str_array_append");
            return;
        }
        arr->items = new_items;
        arr->capacity = new_cap;
    }
    char *copy = strdup(str);
    if (!copy) {
        log_error("Failed to copy string in str_array_append");
        return;
    }
    arr->items[arr->count++] = copy;
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
    normalize_path(buf);
}

void get_target_dir(char *buf, size_t buf_size) {
    const char *home = getenv("HOME");
    if (home) {
        snprintf(buf, buf_size, "%s", home);
    } else {
        snprintf(buf, buf_size, "/tmp");
    }
    normalize_path(buf);
}

void get_all_packages(const char *dotfiles_dir, StringArray *packages) {
    DIR *dir = opendir(dotfiles_dir);
    if (!dir) return;

    StringArray ignored;
    str_array_init(&ignored);

    char ignore_file[PATH_MAX];
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
        if (name[0] == '.') continue;
        if (strcmp(name, "build") == 0) continue;
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
    if (!dir_path || current_depth > max_depth) return;

    DIR *dir = opendir(dir_path);
    if (!dir) return;

    struct dirent *entry;
    char path[PATH_MAX * 2];

    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0 ||
            strcmp(name, ".git") == 0 || strcmp(name, ".cache") == 0 ||
            strcmp(name, "node_modules") == 0 || strcmp(name, "build") == 0 ||
            strcmp(name, "bin") == 0) continue;

        snprintf(path, sizeof(path), "%s/%s", dir_path, name);

        if (is_symlink(path)) {
            if (cb) cb(path, user_data);
        } else if (is_dir(path) && current_depth < max_depth) {
            walk_dir_symlinks(path, current_depth + 1, max_depth, cb, user_data);
        }
    }

    closedir(dir);
}

void walk_dir_files(const char *base_dir, const char *current_dir, WalkFileCallback cb, void *user_data) {
    if (!base_dir || !current_dir) return;

    DIR *dir = opendir(current_dir);
    if (!dir) return;

    struct dirent *entry;
    char path[PATH_MAX * 2];
    size_t prefix_len = strlen(base_dir);

    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0 ||
            strcmp(name, ".stowdeps") == 0) continue;

        snprintf(path, sizeof(path), "%s/%s", current_dir, name);

        const char *rel_path = path + prefix_len;
        if (*rel_path == '/') rel_path++;

        if (is_symlink(path) || !is_dir(path)) {
            if (cb) cb(path, rel_path, user_data);
        } else if (is_dir(path)) {
            walk_dir_files(base_dir, path, cb, user_data);
        }
    }

    closedir(dir);
}

int run_system_cmd(const char *cmd) {
    return system(cmd);
}
