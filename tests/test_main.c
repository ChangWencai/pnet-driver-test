/**
 * test_main.c - Main Test Runner
 *
 * Runs all test suites covering chapters 2-6 of the
 * "p-net Driver Installation: Profinet Protocol Deployment on Linux" document.
 */

#include "test_framework.h"

/* Define the global test result tracker */
test_result_t g_test_result = {0, 0, 0, 0, "", ""};

/* External test suite runners */
extern void run_chapter2_tests(void);
extern void run_chapter3_tests(void);
extern void run_chapter4_tests(void);
extern void run_chapter5_tests(void);
extern void run_chapter6_tests(void);

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    test_reset();

    printf("\n");
    printf("========================================\n");
    printf("  p-net Driver Test Suite\n");
    printf("  Profinet Protocol on Linux\n");
    printf("  Based on CSDN Document: 2sasavii7e\n");
    printf("========================================\n");

    /* Run all test suites */
    run_chapter2_tests();  /* Driver Architecture & Protocol Core */
    run_chapter3_tests();  /* Installation & Service Management */
    run_chapter4_tests();  /* Network Configuration & Optimization */
    run_chapter5_tests();  /* Security Mechanisms & Hardening */
    run_chapter6_tests();  /* Practical Applications & Deployment */

    /* Print summary */
    test_print_summary();

    return test_exit_code();
}
