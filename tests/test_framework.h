/**
 * test_framework.h - Minimal C Test Framework
 */
#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
    int  total;
    int  passed;
    int  failed;
    int  skipped;
    char current_suite[64];
    char current_test[128];
} test_result_t;

extern test_result_t g_test_result;

#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_RESET   "\033[0m"

#define TEST_SUITE_BEGIN(name) \
    do { \
        strncpy(g_test_result.current_suite, name, sizeof(g_test_result.current_suite) - 1); \
        printf("\n" COLOR_BLUE "=== %s ===" COLOR_RESET "\n", name); \
    } while (0)

#define TEST_SUITE_END() \
    do { \
        printf(COLOR_BLUE "=== End: %s ===\n" COLOR_RESET, g_test_result.current_suite); \
    } while (0)

#define TEST_BEGIN(name) \
    do { \
        strncpy(g_test_result.current_test, name, sizeof(g_test_result.current_test) - 1); \
        g_test_result.total++; \
        printf("  [TEST] %s ... ", name); \
    } while (0)

#define TEST_PASS()     do { g_test_result.passed++; printf(COLOR_GREEN "PASS" COLOR_RESET "\n"); return; } while (0)
#define TEST_FAIL(msg)  do { g_test_result.failed++; printf(COLOR_RED "FAIL" COLOR_RESET " - %s\n", msg); return; } while (0)
#define TEST_SKIP(msg)  do { g_test_result.skipped++; printf(COLOR_YELLOW "SKIP" COLOR_RESET " - %s\n", msg); return; } while (0)

#define ASSERT_TRUE(expr) \
    do { if (!(expr)) { char _m[256]; snprintf(_m, sizeof(_m), "ASSERT_TRUE(%s) line %d", #expr, __LINE__); TEST_FAIL(_m); } } while (0)
#define ASSERT_FALSE(expr) \
    do { if ((expr)) { char _m[256]; snprintf(_m, sizeof(_m), "ASSERT_FALSE(%s) line %d", #expr, __LINE__); TEST_FAIL(_m); } } while (0)
#define ASSERT_EQ(a, b) \
    do { if ((long)(a) != (long)(b)) { char _m[256]; snprintf(_m, sizeof(_m), "ASSERT_EQ: %ld != %ld line %d", (long)(a), (long)(b), __LINE__); TEST_FAIL(_m); } } while (0)
#define ASSERT_NE(a, b) \
    do { if ((long)(a) == (long)(b)) { char _m[256]; snprintf(_m, sizeof(_m), "ASSERT_NE: %ld == %ld line %d", (long)(a), (long)(b), __LINE__); TEST_FAIL(_m); } } while (0)
#define ASSERT_STR_EQ(a, b) \
    do { if (strcmp((a),(b)) != 0) { char _m[256]; snprintf(_m, sizeof(_m), "ASSERT_STR_EQ: \"%s\" != \"%s\" line %d", (a), (b), __LINE__); TEST_FAIL(_m); } } while (0)
#define ASSERT_NOT_NULL(p) \
    do { if ((p) == NULL) { char _m[256]; snprintf(_m, sizeof(_m), "ASSERT_NOT_NULL(%s) line %d", #p, __LINE__); TEST_FAIL(_m); } } while (0)
#define ASSERT_NULL(p) \
    do { if ((p) != NULL) { char _m[256]; snprintf(_m, sizeof(_m), "ASSERT_NULL(%s) line %d", #p, __LINE__); TEST_FAIL(_m); } } while (0)
#define ASSERT_GE(a, b) \
    do { if ((long)(a) < (long)(b)) { char _m[256]; snprintf(_m, sizeof(_m), "ASSERT_GE: %ld < %ld line %d", (long)(a), (long)(b), __LINE__); TEST_FAIL(_m); } } while (0)
#define ASSERT_GT(a, b) \
    do { if ((long)(a) <= (long)(b)) { char _m[256]; snprintf(_m, sizeof(_m), "ASSERT_GT: %ld <= %ld line %d", (long)(a), (long)(b), __LINE__); TEST_FAIL(_m); } } while (0)
#define ASSERT_LE(a, b) \
    do { if ((long)(a) > (long)(b)) { char _m[256]; snprintf(_m, sizeof(_m), "ASSERT_LE: %ld > %ld line %d", (long)(a), (long)(b), __LINE__); TEST_FAIL(_m); } } while (0)

static inline void test_print_summary(void) {
    printf("\n" COLOR_BLUE "========================================" COLOR_RESET "\n");
    printf("  Total:   %d\n", g_test_result.total);
    printf("  " COLOR_GREEN "Passed:  %d" COLOR_RESET "\n", g_test_result.passed);
    printf("  Failed:  %d\n", g_test_result.failed);
    if (g_test_result.skipped > 0)
        printf("  " COLOR_YELLOW "Skipped: %d" COLOR_RESET "\n", g_test_result.skipped);
    printf(COLOR_BLUE "========================================" COLOR_RESET "\n");
    if (g_test_result.failed == 0)
        printf(COLOR_GREEN "  ALL TESTS PASSED!" COLOR_RESET "\n");
    else
        printf(COLOR_RED "  SOME TESTS FAILED!" COLOR_RESET "\n");
    printf(COLOR_BLUE "========================================\n" COLOR_RESET "\n");
}

static inline int test_exit_code(void) { return g_test_result.failed > 0 ? 1 : 0; }
static inline void test_reset(void) { memset(&g_test_result, 0, sizeof(g_test_result)); }

#endif
