#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define TEST_GREEN "\033[0;32m"
#define TEST_RED   "\033[0;31m"
#define TEST_RESET "\033[0m"

static int g_tests_run = 0;
static int g_tests_failed = 0;

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
