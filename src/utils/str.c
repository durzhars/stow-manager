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

#include "utils/str.h"
#include "utils/mem.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

void str_array_init(StringArray *arr)
{
    if (!arr) {
        return;
    }
    arr->items = NULL;
    arr->count = 0;
    arr->capacity = 0;
}

void str_array_append(StringArray *arr, const char *str)
{
    if (!arr || !str || *str == '\0') {
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

static unsigned long str_hash_fnv1a(const char *str)
{
    unsigned long hash = 14695981039346656037UL;
    for (const unsigned char *p = (const unsigned char *)str; *p; p++) {
        hash ^= *p;
        hash *= 1099511628211UL;
    }
    return hash;
}

void str_set_init(StrSet *set)
{
    if (!set) {
        return;
    }
    set->keys = NULL;
    set->capacity = 0;
    set->count = 0;
}

static void str_set_resize(StrSet *set, size_t new_cap)
{
    char **old_keys = set->keys;
    size_t old_cap = set->capacity;

    char **new_keys = (char **)safe_calloc(new_cap, sizeof(char *));
    set->keys = new_keys;
    set->capacity = new_cap;
    set->count = 0;

    for (size_t i = 0; i < old_cap; i++) {
        if (old_keys && old_keys[i]) {
            str_set_add(set, old_keys[i]);
            free(old_keys[i]);
        }
    }
    free((void *)old_keys);
}

bool str_set_add(StrSet *set, const char *str)
{
    if (!set || !str || *str == '\0') {
        return false;
    }

    if (set->capacity == 0 || set->count * 10 >= set->capacity * 7) {
        size_t new_cap = (set->capacity == 0) ? 16 : set->capacity * 2;
        str_set_resize(set, new_cap);
    }

    unsigned long hash = str_hash_fnv1a(str);
    size_t idx = (size_t)(hash % set->capacity);

    while (set->keys[idx] != NULL) {
        if (strcmp(set->keys[idx], str) == 0) {
            return false;
        }
        idx = (idx + 1) % set->capacity;
    }

    set->keys[idx] = safe_strdup(str);
    set->count++;
    return true;
}

bool str_set_contains(const StrSet *set, const char *str)
{
    if (!set || !str || set->capacity == 0 || set->count == 0) {
        return false;
    }

    unsigned long hash = str_hash_fnv1a(str);
    size_t idx = (size_t)(hash % set->capacity);

    while (set->keys[idx] != NULL) {
        if (strcmp(set->keys[idx], str) == 0) {
            return true;
        }
        idx = (idx + 1) % set->capacity;
    }
    return false;
}

void str_set_free(StrSet *set)
{
    if (!set) {
        return;
    }
    if (set->keys) {
        for (size_t i = 0; i < set->capacity; i++) {
            if (set->keys[i]) {
                free(set->keys[i]);
            }
        }
        free((void *)set->keys);
    }
    set->keys = NULL;
    set->capacity = 0;
    set->count = 0;
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
    if (!dest || dest_size == 0) {
        return;
    }
    if (!src) {
        dest[0] = '\0';
        return;
    }

    size_t d = 0;
    if (d + 1 >= dest_size) {
        dest[0] = '\0';
        return;
    }
    dest[d++] = '\'';

    size_t i = 0;
    bool overflow = false;
    for (i = 0; src[i] != '\0'; i++) {
        if (src[i] == '\'') {
            if (d + 4 >= dest_size) {
                overflow = true;
                break;
            }
            dest[d++] = '\'';
            dest[d++] = '\\';
            dest[d++] = '\'';
            dest[d++] = '\'';
        } else {
            if (d + 1 >= dest_size) {
                overflow = true;
                break;
            }
            dest[d++] = src[i];
        }
    }

    if (overflow || src[i] != '\0' || d + 1 >= dest_size) {
        dest[0] = '\0';
        return;
    }

    dest[d++] = '\'';
    dest[d] = '\0';
}

void str_split_delim(const char *src, const char *delim, StringArray *out_arr)
{
    if (!src || !delim || !out_arr) {
        return;
    }

    char *copy = safe_strdup(src);
    char *saveptr = NULL;
    char *token = strtok_r(copy, delim, &saveptr);

    while (token) {
        if (*token != '\0') {
            str_array_append(out_arr, token);
        }
        token = strtok_r(NULL, delim, &saveptr);
    }

    free(copy);
}

static inline bool is_var_start_char(char c)
{
    return isalpha((unsigned char)c) || c == '_';
}

static inline bool is_var_body_char(char c)
{
    return isalnum((unsigned char)c) || c == '_';
}

void str_expand_vars(const char *src,
                     char *out,
                     size_t out_size,
                     StrVarResolver resolver,
                     void *ctx)
{
    if (!out || out_size == 0) {
        return;
    }
    if (!src) {
        out[0] = '\0';
        return;
    }

    size_t srclen = strlen(src);
    size_t o = 0;
    size_t i = 0;
    bool overflow = false;

    while (i < srclen && o + 1 < out_size) {
        if (src[i] == '$') {
            if (i + 1 < srclen && src[i + 1] == '{') {
                size_t j = i + 2;
                char varname[256] = {0};
                size_t vn = 0;
                while (j < srclen && src[j] != '}' && vn + 1 < sizeof(varname)) {
                    varname[vn++] = src[j++];
                }
                if (j < srclen && src[j] == '}' && vn > 0) {
                    const char *val = resolver ? resolver(varname, ctx) : NULL;
                    if (val) {
                        size_t vlen = strlen(val);
                        for (size_t k = 0; k < vlen; k++) {
                            if (o + 1 >= out_size) {
                                overflow = true;
                                break;
                            }
                            out[o++] = val[k];
                        }
                    }
                    i = j + 1;
                } else {
                    out[o++] = src[i++];
                }
            } else if (i + 1 < srclen && is_var_start_char(src[i + 1])) {
                size_t j = i + 1;
                char varname[256] = {0};
                size_t vn = 0;
                while (j < srclen && is_var_body_char(src[j]) && vn + 1 < sizeof(varname)) {
                    varname[vn++] = src[j++];
                }
                const char *val = resolver ? resolver(varname, ctx) : NULL;
                if (val) {
                    size_t vlen = strlen(val);
                    for (size_t k = 0; k < vlen; k++) {
                        if (o + 1 >= out_size) {
                            overflow = true;
                            break;
                        }
                        out[o++] = val[k];
                    }
                }
                i = j;
            } else {
                out[o++] = src[i++];
            }
        } else {
            out[o++] = src[i++];
        }
        if (overflow) {
            break;
        }
    }

    if (overflow || i < srclen) {
        out[0] = '\0';
        return;
    }

    out[o] = '\0';
}
