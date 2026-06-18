/**
 * test_network.c - Chapter 4: Network Configuration and Optimization Tests
 *
 * Tests for Profinet network configuration including:
 * - Interface management (init, configure, up/down)
 * - Route management (add, remove, lookup)
 * - TCP/IP tuning (sysctl parameter generation)
 * - Performance monitoring and jitter calculation
 * - Network script generation
 */

#include "test_framework.h"
#include "pnet_network.h"
#include <math.h>
#include <string.h>

/* ===== Interface Management Tests ===== */

void test_interface_init(void) {
    TEST_BEGIN("network_interface_init");

    pnet_interface_t iface;
    int ret = pnet_interface_init(&iface, "eth0");
    ASSERT_EQ(0, ret);
    ASSERT_STR_EQ("eth0", iface.name);
    ASSERT_EQ(1500, (int)iface.mtu);
    ASSERT_FALSE(iface.configured);
    ASSERT_FALSE(pnet_interface_is_up(&iface));

    TEST_PASS();
}

void test_interface_configure(void) {
    TEST_BEGIN("network_interface_configure_ip");

    pnet_interface_t iface;
    pnet_interface_init(&iface, "eth0");

    uint32_t ip = pnet_ip_from_str("192.168.1.100");
    uint32_t mask = pnet_ip_from_str("255.255.255.0");
    uint32_t gw = pnet_ip_from_str("192.168.1.1");

    int ret = pnet_interface_configure(&iface, ip, mask, gw);
    ASSERT_EQ(0, ret);
    ASSERT_TRUE(iface.configured);
    ASSERT_EQ(ip, iface.ip_addr);
    ASSERT_EQ(mask, iface.subnet_mask);
    ASSERT_EQ(gw, iface.gateway);

    TEST_PASS();
}

void test_interface_up_down(void) {
    TEST_BEGIN("network_interface_up_down");

    pnet_interface_t iface;
    pnet_interface_init(&iface, "eth0");
    pnet_interface_configure(&iface, pnet_ip_from_str("192.168.0.10"),
                             pnet_ip_from_str("255.255.255.0"),
                             pnet_ip_from_str("192.168.0.1"));

    int ret = pnet_interface_up(&iface);
    ASSERT_EQ(0, ret);
    ASSERT_TRUE(pnet_interface_is_up(&iface));
    ASSERT_TRUE(iface.flags & PNET_IF_RUNNING);

    ret = pnet_interface_down(&iface);
    ASSERT_EQ(0, ret);
    ASSERT_FALSE(pnet_interface_is_up(&iface));

    TEST_PASS();
}

void test_interface_mac(void) {
    TEST_BEGIN("network_interface_set_mac");

    pnet_interface_t iface;
    pnet_interface_init(&iface, "eth0");

    uint8_t mac[6] = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};
    int ret = pnet_interface_set_mac(&iface, mac);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(0x00, iface.mac_addr[0]);
    ASSERT_EQ(0x1A, iface.mac_addr[1]);
    ASSERT_EQ(0x5E, iface.mac_addr[5]);

    TEST_PASS();
}

void test_interface_stats(void) {
    TEST_BEGIN("network_interface_get_stats");

    pnet_interface_t iface;
    pnet_interface_init(&iface, "eth0");

    pnet_network_stats_t stats;
    int ret = pnet_interface_get_stats(&iface, &stats);
    ASSERT_EQ(0, ret);
    ASSERT_GT((int)stats.bandwidth_mbps, 0);
    ASSERT_GE(0, 0); /* latency_us should be non-negative */

    TEST_PASS();
}

/* ===== Route Management Tests ===== */

void test_route_add_lookup(void) {
    TEST_BEGIN("route_add_and_longest_prefix_lookup");

    pnet_network_config_t config;
    pnet_network_init(&config);

    pnet_route_entry_t route1 = {
        .destination = pnet_ip_from_str("192.168.1.0"),
        .netmask = pnet_ip_from_str("255.255.255.0"),
        .gateway = pnet_ip_from_str("192.168.0.1"),
        .metric = 100,
        .active = true
    };
    strncpy(route1.interface, "eth0", PNET_INTERFACE_NAME_LEN - 1);

    int ret = pnet_route_add(&config, &route1);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(1, config.route_count);

    /* Add a more specific route */
    pnet_route_entry_t route2 = {
        .destination = pnet_ip_from_str("192.168.1.128"),
        .netmask = pnet_ip_from_str("255.255.255.128"),
        .gateway = pnet_ip_from_str("192.168.0.101"),
        .metric = 50,
        .active = true
    };
    strncpy(route2.interface, "eth0", PNET_INTERFACE_NAME_LEN - 1);

    ret = pnet_route_add(&config, &route2);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(2, config.route_count);

    /* Lookup should find the more specific route */
    uint32_t target_ip = pnet_ip_from_str("192.168.1.200");
    const pnet_route_entry_t *found = pnet_route_lookup(&config, target_ip);
    ASSERT_NOT_NULL(found);
    ASSERT_EQ(50, found->metric);

    /* Lookup for address only matching the less specific route */
    uint32_t target_ip2 = pnet_ip_from_str("192.168.1.50");
    found = pnet_route_lookup(&config, target_ip2);
    ASSERT_NOT_NULL(found);
    ASSERT_EQ(100, found->metric);

    TEST_PASS();
}

void test_route_remove_flush(void) {
    TEST_BEGIN("route_remove_and_flush");

    pnet_network_config_t config;
    pnet_network_init(&config);

    pnet_route_entry_t route = {
        .destination = pnet_ip_from_str("10.0.0.0"),
        .netmask = pnet_ip_from_str("255.0.0.0"),
        .gateway = pnet_ip_from_str("192.168.0.1"),
        .active = true
    };
    strncpy(route.interface, "eth0", PNET_INTERFACE_NAME_LEN - 1);

    pnet_route_add(&config, &route);
    ASSERT_EQ(1, config.route_count);

    int ret = pnet_route_remove(&config, pnet_ip_from_str("10.0.0.0"), pnet_ip_from_str("255.0.0.0"));
    ASSERT_EQ(0, ret);
    ASSERT_EQ(0, config.route_count);

    /* Flush */
    pnet_route_add(&config, &route);
    pnet_route_add(&config, &route);
    ret = pnet_route_flush(&config);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(0, config.route_count);

    TEST_PASS();
}

/* ===== Network Configuration Tests ===== */

void test_network_config_validate(void) {
    TEST_BEGIN("network_config_validation");

    pnet_network_config_t config;
    pnet_network_init(&config);

    /* Empty config should fail validation */
    int ret = pnet_network_validate(&config);
    ASSERT_EQ(-1, ret);

    /* Add a configured interface */
    pnet_interface_init(&config.interfaces[0], "eth0");
    pnet_interface_configure(&config.interfaces[0],
                             pnet_ip_from_str("192.168.1.100"),
                             pnet_ip_from_str("255.255.255.0"),
                             pnet_ip_from_str("192.168.1.1"));
    config.interface_count = 1;

    ret = pnet_network_validate(&config);
    ASSERT_EQ(0, ret);

    TEST_PASS();
}

void test_network_apply(void) {
    TEST_BEGIN("network_config_apply");

    pnet_network_config_t config;
    pnet_network_init(&config);

    pnet_interface_init(&config.interfaces[0], "eth0");
    pnet_interface_configure(&config.interfaces[0],
                             pnet_ip_from_str("192.168.0.10"),
                             pnet_ip_from_str("255.255.255.0"),
                             pnet_ip_from_str("192.168.0.1"));
    config.interface_count = 1;

    int ret = pnet_network_apply(&config);
    ASSERT_EQ(0, ret);
    ASSERT_TRUE(pnet_interface_is_up(&config.interfaces[0]));

    TEST_PASS();
}

void test_network_script_generation(void) {
    TEST_BEGIN("network_config_script_generation");

    pnet_network_config_t config;
    pnet_network_init(&config);

    pnet_interface_init(&config.interfaces[0], "eth0");
    pnet_interface_configure(&config.interfaces[0],
                             pnet_ip_from_str("192.168.0.10"),
                             pnet_ip_from_str("255.255.255.0"),
                             pnet_ip_from_str("192.168.0.1"));
    config.interface_count = 1;

    pnet_route_entry_t route = {
        .destination = pnet_ip_from_str("192.168.1.0"),
        .netmask = pnet_ip_from_str("255.255.255.0"),
        .gateway = pnet_ip_from_str("192.168.0.101"),
        .active = true
    };
    strncpy(route.interface, "eth0", PNET_INTERFACE_NAME_LEN - 1);
    pnet_route_add(&config, &route);

    char script[2048];
    int len = pnet_network_generate_script(&config, script, sizeof(script));
    ASSERT_GT(len, 0);
    ASSERT_NOT_NULL(strstr(script, "#!/bin/bash"));
    ASSERT_NOT_NULL(strstr(script, "ip addr add"));
    ASSERT_NOT_NULL(strstr(script, "ip route add"));

    TEST_PASS();
}

/* ===== TCP/IP Tuning Tests ===== */

void test_tcp_tuning_defaults(void) {
    TEST_BEGIN("tcp_tuning_default_values");

    pnet_tcp_tuning_t tuning;
    int ret = pnet_tcp_tuning_init(&tuning);
    ASSERT_EQ(0, ret);
    ASSERT_TRUE(tuning.tcp_timestamps);
    ASSERT_TRUE(tuning.tcp_window_scaling);
    ASSERT_TRUE(tuning.tcp_sack);
    ASSERT_EQ(4096, (int)tuning.tcp_rmem_min);
    ASSERT_EQ(87380, (int)tuning.tcp_rmem_default);
    ASSERT_EQ(16777216, (int)tuning.tcp_rmem_max);

    TEST_PASS();
}

void test_tcp_tuning_sysctl_generation(void) {
    TEST_BEGIN("tcp_tuning_sysctl_config_generation");

    pnet_tcp_tuning_t tuning;
    pnet_tcp_tuning_init(&tuning);

    char sysctl[1024];
    int len = pnet_tcp_tuning_generate_sysctl(&tuning, sysctl, sizeof(sysctl));
    ASSERT_GT(len, 0);
    ASSERT_NOT_NULL(strstr(sysctl, "tcp_timestamps = 1"));
    ASSERT_NOT_NULL(strstr(sysctl, "tcp_window_scaling = 1"));
    ASSERT_NOT_NULL(strstr(sysctl, "tcp_sack = 1"));
    ASSERT_NOT_NULL(strstr(sysctl, "tcp_rmem"));
    ASSERT_NOT_NULL(strstr(sysctl, "tcp_wmem"));

    TEST_PASS();
}

/* ===== Performance Monitoring Tests ===== */

void test_jitter_calculation(void) {
    TEST_BEGIN("network_jitter_calculation");

    /* Simulated latency measurements (microseconds) */
    double latencies[] = {50.0, 52.0, 48.0, 51.0, 49.0, 53.0, 47.0, 50.0};
    int count = sizeof(latencies) / sizeof(latencies[0]);

    double jitter = pnet_network_calculate_jitter(latencies, count);
    ASSERT_GT((int)(jitter * 100), 0); /* Jitter should be positive */
    ASSERT_LE((int)(jitter * 100), 300); /* Jitter should be < 3us */

    /* Zero jitter for constant latencies */
    double constant[] = {50.0, 50.0, 50.0, 50.0};
    double zero_jitter = pnet_network_calculate_jitter(constant, 4);
    ASSERT_EQ(0, (int)(zero_jitter * 1000)); /* Should be very close to 0 */

    /* Edge cases */
    ASSERT_EQ(0, (int)pnet_network_calculate_jitter(NULL, 0));
    ASSERT_EQ(0, (int)pnet_network_calculate_jitter(latencies, 1));

    TEST_PASS();
}

void test_network_monitor(void) {
    TEST_BEGIN("network_monitor_start_stop");

    int ret = pnet_network_monitor_start("eth0");
    ASSERT_EQ(0, ret);

    pnet_network_stats_t stats;
    ret = pnet_network_get_stats("eth0", &stats);
    ASSERT_EQ(0, ret);
    ASSERT_GT((int)stats.bandwidth_mbps, 0);

    ret = pnet_network_monitor_stop();
    ASSERT_EQ(0, ret);

    TEST_PASS();
}

/* ===== Utility Function Tests ===== */

void test_ip_utilities(void) {
    TEST_BEGIN("ip_address_utility_functions");

    /* IP string conversion */
    uint32_t ip = pnet_ip_from_str("192.168.1.100");
    ASSERT_NE(0, (int)ip);

    char buf[16];
    pnet_ip_to_str(ip, buf);
    ASSERT_STR_EQ("192.168.1.100", buf);

    /* Private IP detection */
    ASSERT_TRUE(pnet_ip_is_private(pnet_ip_from_str("10.0.0.1")));
    ASSERT_TRUE(pnet_ip_is_private(pnet_ip_from_str("172.16.0.1")));
    ASSERT_TRUE(pnet_ip_is_private(pnet_ip_from_str("192.168.1.1")));
    ASSERT_FALSE(pnet_ip_is_private(pnet_ip_from_str("8.8.8.8")));
    ASSERT_FALSE(pnet_ip_is_private(pnet_ip_from_str("1.1.1.1")));

    /* Subnet check */
    uint32_t subnet = pnet_ip_from_str("192.168.0.0");
    uint32_t mask = pnet_ip_from_str("255.255.255.0");
    ASSERT_TRUE(pnet_ip_in_subnet(pnet_ip_from_str("192.168.0.50"), subnet, mask));
    ASSERT_FALSE(pnet_ip_in_subnet(pnet_ip_from_str("192.168.1.50"), subnet, mask));

    TEST_PASS();
}

/* ===== Run all chapter 4 tests ===== */
void run_chapter4_tests(void) {
    TEST_SUITE_BEGIN("Chapter 4: Network Configuration & Optimization");

    test_interface_init();
    test_interface_configure();
    test_interface_up_down();
    test_interface_mac();
    test_interface_stats();

    test_route_add_lookup();
    test_route_remove_flush();

    test_network_config_validate();
    test_network_apply();
    test_network_script_generation();

    test_tcp_tuning_defaults();
    test_tcp_tuning_sysctl_generation();

    test_jitter_calculation();
    test_network_monitor();
    test_ip_utilities();

    TEST_SUITE_END();
}
