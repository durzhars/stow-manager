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

#include "utils/logger.h"
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#define COLOR_RED "\033[0;31m"
#define COLOR_GREEN "\033[0;32m"
#define COLOR_YELLOW "\033[1;33m"
#define COLOR_BLUE "\033[0;34m"
#define COLOR_CYAN "\033[0;36m"
#define COLOR_BOLD "\033[1m"
#define COLOR_RESET "\033[0m"

static LogLevel g_log_level = LOG_LEVEL_INFO;
static FILE *g_log_file = NULL;

void logger_init(LogLevel level, const char *log_file_path)
{
    g_log_level = level;
    if (log_file_path) {
        g_log_file = fopen(log_file_path, "a");
    }
}

void logger_set_level(LogLevel level)
{
    g_log_level = level;
}

void logger_close(void)
{
    if (g_log_file) {
        fclose(g_log_file);
        g_log_file = NULL;
    }
}

static void
log_write(LogLevel level, const char *prefix_color, const char *tag, const char *fmt, va_list args)
{
    if (level < g_log_level) {
        return;
    }

    va_list args_copy;
    va_copy(args_copy, args);

    bool is_tty = isatty(fileno(level >= LOG_LEVEL_ERROR ? stderr : stdout)) != 0;
    FILE *out = (level >= LOG_LEVEL_ERROR) ? stderr : stdout;

    if (is_tty) {
        fprintf(out, "%s%s[%s]%s ", prefix_color, COLOR_BOLD, tag, COLOR_RESET);
    } else {
        fprintf(out, "[%s] ", tag);
    }

    vfprintf(out, fmt, args);
    fprintf(out, "\n");

    if (g_log_file) {
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        char time_buf[64];
        if (t) {
            strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", t);
        } else {
            snprintf(time_buf, sizeof(time_buf), "unknown");
        }

        fprintf(g_log_file, "[%s] [%s] ", time_buf, tag);
        vfprintf(g_log_file, fmt, args_copy);
        fprintf(g_log_file, "\n");
        fflush(g_log_file);
    }

    va_end(args_copy);
}

void log_debug(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_write(LOG_LEVEL_DEBUG, COLOR_CYAN, "DEBUG", fmt, args);
    va_end(args);
}

void log_info(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_write(LOG_LEVEL_INFO, COLOR_BLUE, "INFO", fmt, args);
    va_end(args);
}

void log_success(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_write(LOG_LEVEL_SUCCESS, COLOR_GREEN, "SUCCESS", fmt, args);
    va_end(args);
}

void log_warn(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_write(LOG_LEVEL_WARN, COLOR_YELLOW, "WARNING", fmt, args);
    va_end(args);
}

void log_error(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_write(LOG_LEVEL_ERROR, COLOR_RED, "ERROR", fmt, args);
    va_end(args);
}
