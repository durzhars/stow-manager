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
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils/fs.h"
#include "utils/mem.h"
#include "utils/signal.h"

volatile sig_atomic_t g_interrupted = 0;

#define MAX_SIGNAL_TEMP_PATHS 64
static char g_signal_temp_paths[MAX_SIGNAL_TEMP_PATHS][PATH_MAX];

static StringArray g_temp_paths = {NULL, 0, 0};

void register_temp_path(const char *path)
{
    if (!path || *path == '\0') {
        return;
    }

    if (!str_array_contains(&g_temp_paths, path)) {
        str_array_append(&g_temp_paths, path);
    }

    for (int i = 0; i < MAX_SIGNAL_TEMP_PATHS; i++) {
        if (g_signal_temp_paths[i][0] == '\0') {
            size_t len = strlen(path);
            if (len >= PATH_MAX) {
                len = PATH_MAX - 1;
            }
            memcpy(&g_signal_temp_paths[i][1], path + 1, len);
            g_signal_temp_paths[i][len] = '\0';
// Compiler memory barrier: enforce string completion before publishing index 0
#if defined(__GNUC__) || defined(__clang__)
            __asm__ __volatile__("" ::: "memory");
#endif
            g_signal_temp_paths[i][0] = path[0];
            return;
        }
    }
}

void unregister_temp_path(const char *path)
{
    if (!path || *path == '\0') {
        return;
    }

    size_t w = 0;
    for (size_t r = 0; r < g_temp_paths.count; r++) {
        if (strcmp(g_temp_paths.items[r], path) == 0) {
            free(g_temp_paths.items[r]);
        } else {
            g_temp_paths.items[w++] = g_temp_paths.items[r];
        }
    }
    g_temp_paths.count = w;

    for (int i = 0; i < MAX_SIGNAL_TEMP_PATHS; i++) {
        if (strcmp(g_signal_temp_paths[i], path) == 0) {
            g_signal_temp_paths[i][0] = '\0';
        }
    }
}

void cleanup_temp_paths(void)
{
    for (size_t i = 0; i < g_temp_paths.count; i++) {
        const char *p = g_temp_paths.items[i];
        if (!p || *p == '\0') {
            continue;
        }

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
    // unlink the directory first
    for (int i = 0; i < MAX_SIGNAL_TEMP_PATHS; i++) {
        const char *p = g_signal_temp_paths[i];
        if (p && p[0] != '\0') {
            (void)unlink(p);
        }
    }

    // and remove the directory
    for (int i = 0; i < MAX_SIGNAL_TEMP_PATHS; i++) {
        const char *p = g_signal_temp_paths[i];
        if (p && p[0] != '\0') {
            (void)rmdir(p);
        }
    }
}

static void handle_signal_interrupt(int sig)
{
    g_interrupted = sig;
    const char msg[] = "\n[WARNING] Operation interrupted by signal (Ctrl+C). Cleaning up "
                       "temporary files...\n";
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
