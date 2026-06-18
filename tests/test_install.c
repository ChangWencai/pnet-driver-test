/**
 * test_install.c - Chapter 3: Driver Installation Tests
 *
 * Tests for p-net driver installation process including:
 * - System compatibility check
 * - Dependency verification
 * - Service management (start/stop/status)
 * - Installation report generation
 */

#include "test_framework.h"
#include "pnet_driver.h"
#include <string.h>

void test_install_check_system(void) {
    TEST_BEGIN("install_check_system_compatibility");

    pnet_install_check_t check;
    int ret = pnet_install_check(&check);
    ASSERT_EQ(0, ret);
    ASSERT_TRUE(check.system_compatible);
    ASSERT_TRUE(strlen(check.kernel_version) > 0);
    ASSERT_TRUE(strlen(check.os_name) > 0);

    /* On macOS/Linux, we should detect a valid kernel */
    printf("    (kernel=%s, os=%s)\n", check.kernel_version, check.os_name);

    TEST_PASS();
}

void test_install_check_dependencies(void) {
    TEST_BEGIN("install_check_build_dependencies");

    pnet_install_check_t check;
    memset(&check, 0, sizeof(check));

    int ret = pnet_install_check_deps(&check);
    ASSERT_EQ(0, ret);

    printf("    (deps_met=%s", check.dependencies_met ? "true" : "false");
    if (!check.dependencies_met) {
        printf(", missing=%d", check.missing_deps_count);
    }
    printf(")\n");

    /* At least some tools should be available */
    TEST_PASS();
}

void test_install_verify_service(void) {
    TEST_BEGIN("install_verify_service_status");

    pnet_install_check_t check;
    memset(&check, 0, sizeof(check));

    int ret = pnet_install_verify_service(&check);
    ASSERT_EQ(0, ret);
    ASSERT_TRUE(check.source_available);

    TEST_PASS();
}

void test_install_generate_report(void) {
    TEST_BEGIN("install_generate_full_report");

    pnet_install_check_t check;
    pnet_install_check(&check);
    pnet_install_check_deps(&check);
    pnet_install_verify_service(&check);

    char report[2048];
    int len = pnet_install_generate_report(&check, report, sizeof(report));
    ASSERT_GT(len, 0);

    /* Verify report contains key sections */
    ASSERT_NOT_NULL(strstr(report, "Installation Check Report"));
    ASSERT_NOT_NULL(strstr(report, "Kernel:"));
    ASSERT_NOT_NULL(strstr(report, "Compatible:"));

    printf("    (report length=%d)\n", len);

    TEST_PASS();
}

void test_driver_config_default(void) {
    TEST_BEGIN("driver_config_default_values");

    pnet_driver_config_t config;
    int ret = pnet_config_default(&config);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(2, config.log_level);
    ASSERT_TRUE(config.enable_security);
    ASSERT_TRUE(config.enable_monitoring);
    ASSERT_FALSE(config.enable_redundancy);
    ASSERT_EQ(PNET_MAX_DEVICES, config.max_devices);
    ASSERT_EQ(64, config.max_connections);
    ASSERT_TRUE(strlen(config.config_path) > 0);
    ASSERT_TRUE(strlen(config.log_path) > 0);

    TEST_PASS();
}

void test_driver_config_load(void) {
    TEST_BEGIN("driver_config_load_from_path");

    pnet_driver_config_t config;
    int ret = pnet_config_load(&config, "/tmp/pnet_test.conf");
    ASSERT_EQ(0, ret);
    ASSERT_STR_EQ("/tmp/pnet_test.conf", config.config_path);

    TEST_PASS();
}

void test_service_lifecycle(void) {
    TEST_BEGIN("driver_service_start_stop_lifecycle");

    pnet_driver_t driver;
    pnet_driver_config_t config;
    pnet_config_default(&config);

    int ret = pnet_driver_init(&driver, &config);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(PNET_SERVICE_STOPPED, pnet_driver_get_state(&driver));

    /* Start service */
    ret = pnet_driver_start(&driver);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(PNET_SERVICE_RUNNING, pnet_driver_get_state(&driver));

    /* Double start should be safe */
    ret = pnet_driver_start(&driver);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(PNET_SERVICE_RUNNING, pnet_driver_get_state(&driver));

    /* Stop service */
    ret = pnet_driver_stop(&driver);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(PNET_SERVICE_STOPPED, pnet_driver_get_state(&driver));

    /* Shutdown */
    ret = pnet_driver_shutdown(&driver);
    ASSERT_EQ(0, ret);
    ASSERT_FALSE(driver.initialized);

    TEST_PASS();
}

void test_service_state_strings(void) {
    TEST_BEGIN("service_state_string_conversions");

    ASSERT_STR_EQ("STOPPED", pnet_service_state_str(PNET_SERVICE_STOPPED));
    ASSERT_STR_EQ("STARTING", pnet_service_state_str(PNET_SERVICE_STARTING));
    ASSERT_STR_EQ("RUNNING", pnet_service_state_str(PNET_SERVICE_RUNNING));
    ASSERT_STR_EQ("STOPPING", pnet_service_state_str(PNET_SERVICE_STOPPING));
    ASSERT_STR_EQ("FAILED", pnet_service_state_str(PNET_SERVICE_FAILED));

    TEST_PASS();
}

void test_device_management_via_driver(void) {
    TEST_BEGIN("driver_add_remove_list_devices");

    pnet_driver_t driver;
    pnet_driver_init(&driver, NULL);
    pnet_driver_start(&driver);

    /* Add devices */
    pnet_device_info_t info1 = {
        .vendor_id = 0x0001,
        .device_id = 0x0001,
        .type = PNET_DEVICE_TYPE_IO_DEVICE
    };
    strncpy(info1.device_name, "Sensor-001", sizeof(info1.device_name) - 1);

    pnet_device_info_t info2 = {
        .vendor_id = 0x0001,
        .device_id = 0x0002,
        .type = PNET_DEVICE_TYPE_IO_CONTROLLER
    };
    strncpy(info2.device_name, "PLC-001", sizeof(info2.device_name) - 1);

    int idx1 = pnet_driver_add_device(&driver, &info1);
    ASSERT_GE(idx1, 0);

    int idx2 = pnet_driver_add_device(&driver, &info2);
    ASSERT_GE(idx2, 0);
    ASSERT_EQ(2, driver.device_count);

    /* List devices */
    char list_buf[1024];
    int len = pnet_driver_list_devices(&driver, list_buf, sizeof(list_buf));
    ASSERT_GT(len, 0);
    ASSERT_NOT_NULL(strstr(list_buf, "Sensor-001"));
    ASSERT_NOT_NULL(strstr(list_buf, "PLC-001"));

    /* Get device */
    pnet_device_t *dev;
    int ret = pnet_driver_get_device(&driver, 0, &dev);
    ASSERT_EQ(0, ret);
    ASSERT_NOT_NULL(dev);

    /* Remove device */
    ret = pnet_driver_remove_device(&driver, 0);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(1, driver.device_count);

    pnet_driver_shutdown(&driver);
    TEST_PASS();
}

/* ===== Run all chapter 3 tests ===== */
void run_chapter3_tests(void) {
    TEST_SUITE_BEGIN("Chapter 3: Driver Installation & Service Management");

    test_install_check_system();
    test_install_check_dependencies();
    test_install_verify_service();
    test_install_generate_report();
    test_driver_config_default();
    test_driver_config_load();
    test_service_lifecycle();
    test_service_state_strings();
    test_device_management_via_driver();

    TEST_SUITE_END();
}
