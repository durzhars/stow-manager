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

#include "utils/mem.h"
#include "utils/logger.h"

#include <stdlib.h>
#include <string.h>

void *safe_malloc(size_t size)
{
    if (size == 0) {
        return NULL;
    }
    void *ptr = malloc(size);
    if (!ptr) {
        log_error("Out of memory! Failed to allocate %zu bytes", size);
        exit(EXIT_FAILURE);
    }
    return ptr;
}

void *safe_calloc(size_t num, size_t size)
{
    if (num == 0 || size == 0) {
        return NULL;
    }
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
    if (!s) {
        return NULL;
    }
    char *dup = strdup(s);
    if (!dup) {
        log_error("Out of memory! Failed to duplicate string");
        exit(EXIT_FAILURE);
    }
    return dup;
}
