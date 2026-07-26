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

#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define TEST_GREEN "\033[0;32m"
#define TEST_RED   "\033[0;31m"
#define TEST_RESET "\033[0m"

extern int g_tests_run;
extern int g_tests_failed;

#define ASSERT(expr, msg) do { \
    if (!(expr)) { \
        printf("    %s[FAIL]%s %s (line %d): %s\n", TEST_RED, TEST_RESET, __func__, __LINE__, msg); \
        g_tests_failed++; \
        return; \
    } \
} while (0)

#define ASSERT_STR_EQ(actual, expected) do { \
    const char *a = (actual); \
    const char *e = (expected); \
    if (!a || !e || strcmp(a, e) != 0) { \
        printf("    %s[FAIL]%s %s (line %d): Expected '%s', got '%s'\n", TEST_RED, TEST_RESET, __func__, __LINE__, e, a ? a : "NULL"); \
        g_tests_failed++; \
        return; \
    } \
} while (0)

#define RUN_TEST(test_func) do { \
    g_tests_run++; \
    int before_failures = g_tests_failed; \
    test_func(); \
    if (g_tests_failed == before_failures) { \
        printf("  %s[PASS]%s %s\n", TEST_GREEN, TEST_RESET, #test_func); \
    } \
} while (0)

#endif /* TEST_FRAMEWORK_H */
