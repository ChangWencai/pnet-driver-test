/**
 * test_main.c - Test runner for production architecture
 */
#include "test_framework.h"

test_result_t g_test_result = {0, 0, 0, 0, "", ""};

extern void run_io_device_tests(void);
extern void run_network_tests(void);
extern void run_security_tests(void);
extern void run_realtime_tests(void);
extern void run_config_tests(void);

int main(void) {
    test_reset();
    printf("\n========================================\n");
    printf("  p-net Manager Test Suite\n");
    printf("  Production Architecture\n");
    printf("========================================\n");

    run_io_device_tests();
    run_network_tests();
    run_security_tests();
    run_realtime_tests();
    run_config_tests();

    test_print_summary();
    return test_exit_code();
}
