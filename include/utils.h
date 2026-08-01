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

#ifndef UTILS_H
#define UTILS_H

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include "logger.h"
#include <dirent.h>
#include <fnmatch.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define COLOR_RED "\033[0;31m"
#define COLOR_GREEN "\033[0;32m"
#define COLOR_YELLOW "\033[1;33m"
#define COLOR_BLUE "\033[0;34m"
#define COLOR_CYAN "\033[0;36m"
#define COLOR_WHITE "\033[1;37m"
#define COLOR_BOLD "\033[1m"
#define COLOR_RESET "\033[0m"

// Async Signal Interrupt Flag
extern volatile sig_atomic_t g_interrupted;

// Safe Memory Allocation Validators
void *safe_malloc(size_t size);
void *safe_calloc(size_t num, size_t size);
void *safe_realloc(void *ptr, size_t size);
char *safe_strdup(const char *s);

typedef struct {
    char **items;
    size_t count;
    size_t capacity;
} StringArray;

void str_array_init(StringArray *arr);
void str_array_append(StringArray *arr, const char *str);
bool str_array_contains(const StringArray *arr, const char *str);
void str_array_free(StringArray *arr);

// XDG Base Directory Specification Helpers
bool get_xdg_config_home(char *buf, size_t buf_size);
bool get_xdg_data_home(char *buf, size_t buf_size);
bool get_xdg_cache_home(char *buf, size_t buf_size);
bool get_xdg_state_home(char *buf, size_t buf_size);
void get_xdg_data_dirs(StringArray *dirs);
void get_xdg_config_dirs(StringArray *dirs);

char *trim_whitespace(char *str);
bool file_exists(const char *path);
bool is_dir(const char *path);
bool is_symlink(const char *path);
bool is_executable_in_path(const char *executable);
char *read_symlink_target(const char *path);
bool is_symlink_pointing_to(const char *symlink_path,
                            const char *pkg_file_path,
                            const char *real_pkg_file_path);
void get_distro_id(char *buf, size_t buf_size);
void normalize_path(char *path);
void collapse_path(char *path);
void join_path(char *out, size_t out_size, const char *dir, const char *rel);
int mkdir_p(const char *path, mode_t mode);
bool is_path_prefix(const char *path, const char *prefix);
void escape_shell_arg(const char *src, char *dest, size_t dest_size);
void expand_env_vars(const char *src, char *out, size_t out_size);
void expand_tilde_path(const char *path, char *out, size_t out_size);

// Signal Handling & Temp Path Registration
void setup_signal_handlers(void);
void register_temp_path(const char *path);
void unregister_temp_path(const char *path);
void cleanup_temp_paths(void);
void cleanup_temp_paths_signal_safe(void);

void get_dotfiles_dir(char *buf, size_t buf_size);
bool get_target_dir(char *buf, size_t buf_size);
void get_all_packages(const char *dotfiles_dir, StringArray *packages);

void parse_stowignore(const char *dir_path, StringArray *ignore_patterns);
void parse_stowignore_raw(const char *dir_path, StringArray *raw_ignores);
void get_default_stowignore(StringArray *ignore_patterns);
bool is_path_ignored(const char *rel_path, const StringArray *raw_ignores);

typedef void (*WalkSymlinkCallback)(const char *symlink_path, void *user_data);
typedef void (*WalkFileCallback)(const char *file_path, const char *rel_path, void *user_data);

void walk_dir_symlinks(const char *dir_path,
                       int current_depth,
                       int max_depth,
                       WalkSymlinkCallback cb,
                       void *user_data);
void walk_dir_files(const char *base_dir,
                    const char *current_dir,
                    WalkFileCallback cb,
                    void *user_data);

int run_system_cmd(const char *cmd);

#endif /* UTILS_H */
