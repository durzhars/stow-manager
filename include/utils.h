#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <limits.h>
#include "logger.h"

#define COLOR_RED     "\033[0;31m"
#define COLOR_GREEN   "\033[0;32m"
#define COLOR_YELLOW  "\033[1;33m"
#define COLOR_BLUE    "\033[0;34m"
#define COLOR_CYAN    "\033[0;36m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_RESET   "\033[0m"

char *trim_whitespace(char *str);
bool file_exists(const char *path);
bool is_dir(const char *path);
bool is_symlink(const char *path);
bool is_executable_in_path(const char *executable);
char *read_symlink_target(const char *path);
void get_distro_id(char *buf, size_t buf_size);

typedef struct {
    char **items;
    size_t count;
    size_t capacity;
} StringArray;

void str_array_init(StringArray *arr);
void str_array_append(StringArray *arr, const char *str);
bool str_array_contains(const StringArray *arr, const char *str);
void str_array_free(StringArray *arr);

void get_dotfiles_dir(char *buf, size_t buf_size);
void get_target_dir(char *buf, size_t buf_size);
void get_all_packages(const char *dotfiles_dir, StringArray *packages);

int run_system_cmd(const char *cmd);

#endif /* UTILS_H */
