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
#include <stdlib.h>
#include <string.h>

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
