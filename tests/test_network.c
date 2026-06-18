/**
 * test_network.c - Network configuration module tests
 */
#include "test_framework.h"
#include "network_config.h"
#include <string.h>
#include <stdio.h>

void test_ip_from_string(void) {
    TEST_BEGIN("net_ip_from_string_conversions");
    uint32_t ip;

    ip = net_ip_from_string("192.168.0.100");
    ASSERT_NE(0, (long)ip);

    char buf[32];
    net_ip_to_string(ip, buf, sizeof(buf));
    ASSERT_STR_EQ("192.168.0.100", buf);

    ip = net_ip_from_string("10.0.0.1");
    net_ip_to_string(ip, buf, sizeof(buf));
    ASSERT_STR_EQ("10.0.0.1", buf);

    ip = net_ip_from_string("0.0.0.0");
    ASSERT_EQ(0, (long)ip);

    TEST_PASS();
}

void test_ip_validation(void) {
    TEST_BEGIN("net_ip_is_valid_checks");
    ASSERT_FALSE(net_ip_is_valid(0));           /* 0.0.0.0 invalid */
    ASSERT_TRUE(net_ip_is_valid(net_ip_from_string("192.168.1.1")));
    ASSERT_TRUE(net_ip_is_valid(net_ip_from_string("10.0.0.1")));
    TEST_PASS();
}

void test_if_get_mac(void) {
    TEST_BEGIN("net_if_get_mac_interface");
    uint8_t mac[6];
    /* On macOS with lo0 or on Linux with lo, this may or may not work */
    int ret = net_if_get_mac("lo0", mac);
    /* Just verify it doesn't crash - result depends on platform */
    (void)ret;
    TEST_PASS();
}

void test_if_get_ip(void) {
    TEST_BEGIN("net_if_get_ip_interface");
    uint32_t ip;
    int ret = net_if_get_ip("lo0", &ip);
    (void)ret;
    (void)ip;
    TEST_PASS();
}

void test_tcp_tuning_read(void) {
    TEST_BEGIN("net_tcp_tuning_read_current");
    tcp_tuning_config_t tuning;
    int ret = net_tcp_tuning_read_current(&tuning);
    /* On macOS this returns -1 since /proc/sys doesn't exist */
#ifdef PLATFORM_LINUX
    ASSERT_EQ(0, ret);
    ASSERT_TRUE(tuning.tcp_timestamps);
#else
    ASSERT_EQ(-1, ret);
#endif
    TEST_PASS();
}

void test_if_config_aggregate(void) {
    TEST_BEGIN("net_if_get_config_aggregate");
    net_if_config_t cfg;
    int ret = net_if_get_config("lo0", &cfg);
    /* Platform dependent - just verify no crash */
    (void)ret;
    (void)cfg;
    TEST_PASS();
}

void test_set_up_down(void) {
    TEST_BEGIN("net_if_set_up_down_no_crash");
    /* These will fail without root, but should not crash */
    int ret = net_if_set_up("eth_test_nonexist");
    ASSERT_EQ(-1, ret);
    ret = net_if_set_down("eth_test_nonexist");
    ASSERT_EQ(-1, ret);
    TEST_PASS();
}

void run_network_tests(void) {
    TEST_SUITE_BEGIN("Network Configuration Module");
    test_ip_from_string();
    test_ip_validation();
    test_if_get_mac();
    test_if_get_ip();
    test_tcp_tuning_read();
    test_if_config_aggregate();
    test_set_up_down();
    TEST_SUITE_END();
}
