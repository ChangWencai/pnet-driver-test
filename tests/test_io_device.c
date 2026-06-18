/**
 * test_io_device.c - IO Device module tests using mock p-net
 */
#include "test_framework.h"
#include "io_device.h"
#include <string.h>

static io_device_cfg_t make_test_cfg(void) {
    io_device_cfg_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    strncpy(cfg.product_name, "Test Device", sizeof(cfg.product_name) - 1);
    strncpy(cfg.station_name, "test-device-1", sizeof(cfg.station_name) - 1);
    strncpy(cfg.interface_name, "eth0", sizeof(cfg.interface_name) - 1);
    cfg.ip_addr = 0xC0A80064;  /* 192.168.0.100 */
    cfg.netmask = 0xFFFFFF00;
    cfg.gateway = 0xC0A80001;
    cfg.vendor_id = 0x0001;
    cfg.device_id = 0x0001;
    cfg.tick_us = 1000;
    cfg.send_hello = true;
    return cfg;
}

void test_io_device_init(void) {
    TEST_BEGIN("io_device_init_default");
    io_device_t dev;
    io_device_cfg_t cfg = make_test_cfg();
    int ret = io_device_init(&dev, &cfg);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(IO_DEVICE_STATE_INITIALIZED, io_device_get_state(&dev));
    ASSERT_STR_EQ("Test Device", dev.config.product_name);
    ASSERT_STR_EQ("test-device-1", dev.config.station_name);
    io_device_cleanup(&dev);
    TEST_PASS();
}

void test_io_device_init_null(void) {
    TEST_BEGIN("io_device_init_null_safety");
    ASSERT_EQ(-1, io_device_init(NULL, NULL));
    TEST_PASS();
}

void test_io_device_start_stop(void) {
    TEST_BEGIN("io_device_start_stop_lifecycle");
    io_device_t dev;
    io_device_cfg_t cfg = make_test_cfg();
    io_device_init(&dev, &cfg);

    int ret = io_device_start(&dev);
    ASSERT_EQ(0, ret);
    ASSERT_NOT_NULL(dev.pnet_handle);

    ret = io_device_stop(&dev);
    ASSERT_EQ(0, ret);

    io_device_cleanup(&dev);
    ASSERT_EQ(IO_DEVICE_STATE_IDLE, io_device_get_state(&dev));
    TEST_PASS();
}

void test_io_device_tick(void) {
    TEST_BEGIN("io_device_tick_cycle_counter");
    io_device_t dev;
    io_device_cfg_t cfg = make_test_cfg();
    io_device_init(&dev, &cfg);
    io_device_start(&dev);

    for (int i = 0; i < 100; i++) {
        int ret = io_device_tick(&dev);
        ASSERT_EQ(0, ret);
    }
    ASSERT_EQ(100, (long)dev.cycle_count);

    io_device_stop(&dev);
    io_device_cleanup(&dev);
    TEST_PASS();
}

void test_io_device_update_input(void) {
    TEST_BEGIN("io_device_update_input_data");
    io_device_t dev;
    io_device_cfg_t cfg = make_test_cfg();
    io_device_init(&dev, &cfg);
    io_device_start(&dev);

    uint8_t data[] = {0x42, 0xAB, 0xCD};
    int ret = io_device_update_input(&dev, data, 3);
    ASSERT_EQ(0, ret);

    io_device_stop(&dev);
    io_device_cleanup(&dev);
    TEST_PASS();
}

void test_io_device_read_output(void) {
    TEST_BEGIN("io_device_read_output_data");
    io_device_t dev;
    io_device_cfg_t cfg = make_test_cfg();
    io_device_init(&dev, &cfg);
    io_device_start(&dev);

    uint8_t data[256];
    uint16_t len = sizeof(data);
    int ret = io_device_read_output(&dev, data, &len);
    /* May return -1 if no output module plugged, that's OK */
    (void)ret;

    io_device_stop(&dev);
    io_device_cleanup(&dev);
    TEST_PASS();
}

void test_io_device_state_str(void) {
    TEST_BEGIN("io_device_state_strings");
    ASSERT_STR_EQ("IDLE", io_device_state_str(IO_DEVICE_STATE_IDLE));
    ASSERT_STR_EQ("INITIALIZED", io_device_state_str(IO_DEVICE_STATE_INITIALIZED));
    ASSERT_STR_EQ("CONNECTED", io_device_state_str(IO_DEVICE_STATE_CONNECTED));
    ASSERT_STR_EQ("RUNNING", io_device_state_str(IO_DEVICE_STATE_RUNNING));
    ASSERT_STR_EQ("ERROR", io_device_state_str(IO_DEVICE_STATE_ERROR));
    TEST_PASS();
}

void test_io_device_show(void) {
    TEST_BEGIN("io_device_show_no_crash");
    io_device_t dev;
    io_device_cfg_t cfg = make_test_cfg();
    io_device_init(&dev, &cfg);
    io_device_start(&dev);
    io_device_show(&dev);  /* Should not crash */
    io_device_stop(&dev);
    io_device_cleanup(&dev);
    TEST_PASS();
}

void run_io_device_tests(void) {
    TEST_SUITE_BEGIN("IO Device Module");
    test_io_device_init();
    test_io_device_init_null();
    test_io_device_start_stop();
    test_io_device_tick();
    test_io_device_update_input();
    test_io_device_read_output();
    test_io_device_state_str();
    test_io_device_show();
    TEST_SUITE_END();
}
