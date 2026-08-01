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
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

volatile sig_atomic_t g_interrupted = 0;

#define MAX_SIGNAL_TEMP_PATHS 64
static char g_signal_temp_paths[MAX_SIGNAL_TEMP_PATHS][PATH_MAX];
static volatile sig_atomic_t g_signal_temp_count = 0;

static StringArray g_temp_paths = {NULL, 0, 0};

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
