/**
 * test_framework.h - Minimal C Test Framework
 *
 * A lightweight test framework for p-net driver testing.
 * Provides assertion macros, test registration, and test reporting.
 */

#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Test result tracking */
typedef struct {
    int  total;
    int  passed;
    int  failed;
    int  skipped;
    char current_suite[64];
    char current_test[128];
} test_result_t;

/* Global test result tracker - defined in test_main.c */
extern test_result_t g_test_result;

/* Color codes for terminal output */
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_RESET   "\033[0m"

/* Test suite macros */
#define TEST_SUITE_BEGIN(name) \
    do { \
        strncpy(g_test_result.current_suite, name, sizeof(g_test_result.current_suite) - 1); \
        printf("\n" COLOR_BLUE "=== Test Suite: %s ===" COLOR_RESET "\n", name); \
    } while (0)

#define TEST_SUITE_END() \
    do { \
        printf(COLOR_BLUE "=== End Suite: %s ===\n" COLOR_RESET, g_test_result.current_suite); \
    } while (0)

/* Test case macros */
#define TEST_BEGIN(name) \
    do { \
        strncpy(g_test_result.current_test, name, sizeof(g_test_result.current_test) - 1); \
        g_test_result.total++; \
        printf("  [TEST] %s ... ", name); \
    } while (0)

#define TEST_PASS() \
    do { \
        g_test_result.passed++; \
        printf(COLOR_GREEN "PASS" COLOR_RESET "\n"); \
        return; \
    } while (0)

#define TEST_FAIL(msg) \
    do { \
        g_test_result.failed++; \
        printf(COLOR_RED "FAIL" COLOR_RESET " - %s\n", msg); \
        return; \
    } while (0)

#define TEST_SKIP(msg) \
    do { \
        g_test_result.skipped++; \
        printf(COLOR_YELLOW "SKIP" COLOR_RESET " - %s\n", msg); \
        return; \
    } while (0)

/* Assertion macros */
#define ASSERT_TRUE(expr) \
    do { \
        if (!(expr)) { \
            char _msg[256]; \
            snprintf(_msg, sizeof(_msg), "ASSERT_TRUE failed: %s (line %d)", #expr, __LINE__); \
            TEST_FAIL(_msg); \
        } \
    } while (0)

#define ASSERT_FALSE(expr) \
    do { \
        if ((expr)) { \
            char _msg[256]; \
            snprintf(_msg, sizeof(_msg), "ASSERT_FALSE failed: %s (line %d)", #expr, __LINE__); \
            TEST_FAIL(_msg); \
        } \
    } while (0)

#define ASSERT_EQ(expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            char _msg[256]; \
            snprintf(_msg, sizeof(_msg), "ASSERT_EQ failed: expected %ld, got %ld (line %d)", \
                     (long)(expected), (long)(actual), __LINE__); \
            TEST_FAIL(_msg); \
        } \
    } while (0)

#define ASSERT_NE(val1, val2) \
    do { \
        if ((val1) == (val2)) { \
            char _msg[256]; \
            snprintf(_msg, sizeof(_msg), "ASSERT_NE failed: %ld == %ld (line %d)", \
                     (long)(val1), (long)(val2), __LINE__); \
            TEST_FAIL(_msg); \
        } \
    } while (0)

#define ASSERT_STR_EQ(expected, actual) \
    do { \
        if (strcmp((expected), (actual)) != 0) { \
            char _msg[256]; \
            snprintf(_msg, sizeof(_msg), "ASSERT_STR_EQ failed: expected \"%s\", got \"%s\" (line %d)", \
                     (expected), (actual), __LINE__); \
            TEST_FAIL(_msg); \
        } \
    } while (0)

#define ASSERT_NOT_NULL(ptr) \
    do { \
        if ((ptr) == NULL) { \
            char _msg[256]; \
            snprintf(_msg, sizeof(_msg), "ASSERT_NOT_NULL failed: %s is NULL (line %d)", #ptr, __LINE__); \
            TEST_FAIL(_msg); \
        } \
    } while (0)

#define ASSERT_NULL(ptr) \
    do { \
        if ((ptr) != NULL) { \
            char _msg[256]; \
            snprintf(_msg, sizeof(_msg), "ASSERT_NULL failed: %s is not NULL (line %d)", #ptr, __LINE__); \
            TEST_FAIL(_msg); \
        } \
    } while (0)

#define ASSERT_GE(val1, val2) \
    do { \
        if ((val1) < (val2)) { \
            char _msg[256]; \
            snprintf(_msg, sizeof(_msg), "ASSERT_GE failed: %ld < %ld (line %d)", \
                     (long)(val1), (long)(val2), __LINE__); \
            TEST_FAIL(_msg); \
        } \
    } while (0)

#define ASSERT_LE(val1, val2) \
    do { \
        if ((val1) > (val2)) { \
            char _msg[256]; \
            snprintf(_msg, sizeof(_msg), "ASSERT_LE failed: %ld > %ld (line %d)", \
                     (long)(val1), (long)(val2), __LINE__); \
            TEST_FAIL(_msg); \
        } \
    } while (0)

#define ASSERT_GT(val1, val2) \
    do { \
        if ((val1) <= (val2)) { \
            char _msg[256]; \
            snprintf(_msg, sizeof(_msg), "ASSERT_GT failed: %ld <= %ld (line %d)", \
                     (long)(val1), (long)(val2), __LINE__); \
            TEST_FAIL(_msg); \
        } \
    } while (0)

/* Test report */
static inline void test_print_summary(void) {
    printf("\n" COLOR_BLUE "========================================" COLOR_RESET "\n");
    printf(COLOR_BLUE "  Test Summary" COLOR_RESET "\n");
    printf(COLOR_BLUE "========================================" COLOR_RESET "\n");
    printf("  Total:   %d\n", g_test_result.total);
    printf("  " COLOR_GREEN "Passed:  %d" COLOR_RESET "\n", g_test_result.passed);
    if (g_test_result.failed > 0)
        printf("  " COLOR_RED "Failed:  %d" COLOR_RESET "\n", g_test_result.failed);
    else
        printf("  Failed:  %d\n", g_test_result.failed);
    if (g_test_result.skipped > 0)
        printf("  " COLOR_YELLOW "Skipped: %d" COLOR_RESET "\n", g_test_result.skipped);
    else
        printf("  Skipped: %d\n", g_test_result.skipped);
    printf(COLOR_BLUE "========================================" COLOR_RESET "\n");

    if (g_test_result.failed == 0) {
        printf(COLOR_GREEN "  ALL TESTS PASSED!" COLOR_RESET "\n");
    } else {
        printf(COLOR_RED "  SOME TESTS FAILED!" COLOR_RESET "\n");
    }
    printf(COLOR_BLUE "========================================\n" COLOR_RESET "\n");
}

static inline int test_exit_code(void) {
    return g_test_result.failed > 0 ? 1 : 0;
}

static inline void test_reset(void) {
    memset(&g_test_result, 0, sizeof(g_test_result));
}

#endif /* TEST_FRAMEWORK_H */
