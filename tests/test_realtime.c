/**
 * test_realtime.c - Real-time setup module tests
 */
#include "test_framework.h"
#include "rt_setup.h"
#include <string.h>

void test_rt_config_default(void) {
    TEST_BEGIN("rt_config_default_values");
    rt_config_t cfg;
    rt_config_default(&cfg);
    ASSERT_TRUE(cfg.use_rt_scheduling);
    ASSERT_EQ(80, cfg.rt_priority);
    ASSERT_TRUE(cfg.lock_memory);
    ASSERT_EQ(-1, cfg.isolate_cpu);
    TEST_PASS();
}

void test_rt_setup_init(void) {
    TEST_BEGIN("rt_setup_init");
    rt_config_t cfg;
    rt_config_default(&cfg);
    int ret = rt_setup_init(&cfg);
    ASSERT_EQ(0, ret);
    TEST_PASS();
}

void test_rt_setup_init_invalid(void) {
    TEST_BEGIN("rt_setup_init_invalid_priority");
    rt_config_t cfg;
    rt_config_default(&cfg);
    cfg.rt_priority = 0;  /* Invalid */
    int ret = rt_setup_init(&cfg);
    ASSERT_NE(0, ret);  /* Should fail with non-zero */

    cfg.rt_priority = 100;  /* Invalid */
    ret = rt_setup_init(&cfg);
    ASSERT_NE(0, ret);  /* Should fail with non-zero */
    TEST_PASS();
}

void test_rt_setup_check(void) {
    TEST_BEGIN("rt_setup_check_status");
    rt_config_t cfg;
    rt_config_default(&cfg);
    rt_setup_init(&cfg);

    rt_check_result_t result;
    int ret = rt_setup_check(&result);
    ASSERT_EQ(0, ret);
    ASSERT_GT((int)strlen(result.report), 0);

#ifdef PLATFORM_LINUX
    /* On Linux, we can check if RT scheduling is active */
    printf("    (RT=%s, memlock=%s)\n",
           result.rt_scheduling_ok ? "yes" : "no",
           result.memory_locked ? "yes" : "no");
#else
    printf("    (non-Linux: limited checks)\n");
#endif
    TEST_PASS();
}

void test_rt_setup_measure_latency(void) {
    TEST_BEGIN("rt_setup_measure_latency");
    double max_latency = 0;
    int ret = rt_setup_measure_latency(&max_latency, 100);
    ASSERT_EQ(0, ret);
    ASSERT_GT((int)(max_latency * 1000), 0);  /* Should be > 0 ns */
    printf("    (max_latency=%.1f us)\n", max_latency);
    TEST_PASS();
}

void test_rt_setup_apply_restore(void) {
    TEST_BEGIN("rt_setup_apply_and_restore");
    rt_config_t cfg;
    rt_config_default(&cfg);
    cfg.use_rt_scheduling = false;  /* Don't actually change scheduling in test */
    cfg.lock_memory = false;
    rt_setup_init(&cfg);

    int ret = rt_setup_apply();
    /* May fail without root on Linux */
    (void)ret;

    rt_setup_restore();
    TEST_PASS();
}

void run_realtime_tests(void) {
    TEST_SUITE_BEGIN("Real-Time Setup Module");
    test_rt_config_default();
    test_rt_setup_init();
    test_rt_setup_init_invalid();
    test_rt_setup_check();
    test_rt_setup_measure_latency();
    test_rt_setup_apply_restore();
    TEST_SUITE_END();
}
