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
#ifndef UTILS_MEM_H
#define UTILS_MEM_H

#include <stdbool.h>
#include <stddef.h>

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

char *trim_whitespace(char *str);
void escape_shell_arg(const char *src, char *dest, size_t dest_size);

#endif /* UTILS_MEM_H */
