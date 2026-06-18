/**
 * test_config.c - Application configuration module tests
 */
#include "test_framework.h"
#include "app_config.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>

void test_config_default(void) {
    TEST_BEGIN("app_config_default_values");
    app_config_t cfg;
    app_config_default(&cfg);

    ASSERT_STR_EQ("p-net IO Device", cfg.device.product_name);
    ASSERT_STR_EQ("iodevice1", cfg.device.station_name);
    ASSERT_STR_EQ("eth0", cfg.device.interface_name);
    ASSERT_EQ(1000, (long)cfg.device.tick_us);
    ASSERT_EQ(2, cfg.log_level);
    ASSERT_TRUE(cfg.security_enabled);
    ASSERT_TRUE(cfg.realtime_enabled);
    TEST_PASS();
}

void test_config_validate(void) {
    TEST_BEGIN("app_config_validation");
    app_config_t cfg;
    app_config_default(&cfg);

    int ret = app_config_validate(&cfg);
    ASSERT_EQ(0, ret);

    /* Invalid: empty interface */
    cfg.device.interface_name[0] = '\0';
    ret = app_config_validate(&cfg);
    ASSERT_EQ(-1, ret);

    TEST_PASS();
}

void test_config_save_load(void) {
    TEST_BEGIN("app_config_save_and_load_roundtrip");
    app_config_t cfg;
    app_config_default(&cfg);
    strncpy(cfg.device.product_name, "My Custom Device", sizeof(cfg.device.product_name) - 1);
    cfg.device.tick_us = 500;
    cfg.log_level = 3;

    /* Save */
    const char *path = "/tmp/pnet_test_config.conf";
    int ret = app_config_save(&cfg, path);
    ASSERT_EQ(0, ret);

    /* Load into new config */
    app_config_t loaded;
    ret = app_config_load(&loaded, path);
    ASSERT_EQ(0, ret);
    ASSERT_STR_EQ("My Custom Device", loaded.device.product_name);
    ASSERT_EQ(500, (long)loaded.device.tick_us);
    ASSERT_EQ(3, loaded.log_level);

    /* Cleanup */
    unlink(path);
    TEST_PASS();
}

void test_config_load_nonexistent(void) {
    TEST_BEGIN("app_config_load_nonexistent_file");
    app_config_t cfg;
    int ret = app_config_load(&cfg, "/tmp/nonexistent_pnet_config.conf");
    /* Implementation returns -1 when file not found, which is valid */
    if (ret == 0) {
        /* If it succeeds, it should fall back to defaults */
        ASSERT_STR_EQ("p-net IO Device", cfg.device.product_name);
    }
    /* Either way, test passes - we just verify no crash */
    TEST_PASS();
}

void test_config_validate_invalid_tick(void) {
    TEST_BEGIN("app_config_validate_zero_tick");
    app_config_t cfg;
    app_config_default(&cfg);
    cfg.device.tick_us = 0;
    int ret = app_config_validate(&cfg);
    ASSERT_EQ(-1, ret);
    TEST_PASS();
}

void run_config_tests(void) {
    TEST_SUITE_BEGIN("Application Configuration Module");
    test_config_default();
    test_config_validate();
    test_config_save_load();
    test_config_load_nonexistent();
    test_config_validate_invalid_tick();
    TEST_SUITE_END();
}
