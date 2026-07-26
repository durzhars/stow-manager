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
    if (*str == 0) return str;

    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';

    if ((*str == '"' && end[0] == '"') || (*str == '\'' && end[0] == '\'')) {
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

int run_system_cmd(const char *cmd) {
    return system(cmd);
}
