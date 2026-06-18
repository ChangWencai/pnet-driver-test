/**
 * test_deployment.c - Chapter 6: Practical Application and Deployment Tests
 *
 * Tests for Profinet practical deployment including:
 * - Deployment scenario creation and validation
 * - Full system integration test (driver + network + security)
 * - Deployment simulation and report generation
 * - Industrial application scenario tests
 */

#include "test_framework.h"
#include "pnet_driver.h"
#include "pnet_protocol.h"
#include "pnet_network.h"
#include "pnet_security.h"
#include <string.h>

/* ===== Deployment Management Tests ===== */

void test_deployment_create(void) {
    TEST_BEGIN("deployment_create_scenario");

    pnet_deployment_t deploy;
    int ret = pnet_deployment_create(&deploy, "factory-line-1");
    ASSERT_EQ(0, ret);
    ASSERT_STR_EQ("factory-line-1", deploy.scenario_name);
    ASSERT_EQ(PNET_TOPOLOGY_STAR, deploy.topology);
    ASSERT_EQ(1000, deploy.expected_cycle_time_us);

    TEST_PASS();
}

void test_deployment_validate(void) {
    TEST_BEGIN("deployment_validate_config");

    pnet_deployment_t deploy;
    pnet_deployment_create(&deploy, "test-deploy");

    /* Empty deployment should fail validation */
    int ret = pnet_deployment_validate(&deploy);
    ASSERT_EQ(-1, ret);

    /* Add a device */
    deploy.device_count = 1;
    deploy.devices[0].vendor_id = 0x0001;
    deploy.devices[0].device_id = 0x0001;
    deploy.devices[0].type = PNET_DEVICE_TYPE_IO_DEVICE;
    strncpy(deploy.devices[0].device_name, "Robot-Arm-01", sizeof(deploy.devices[0].device_name) - 1);

    ret = pnet_deployment_validate(&deploy);
    ASSERT_EQ(0, ret);

    TEST_PASS();
}

void test_deployment_simulate(void) {
    TEST_BEGIN("deployment_simulation");

    pnet_deployment_t deploy;
    pnet_deployment_create(&deploy, "simulation-test");
    deploy.device_count = 3;
    deploy.use_redundancy = true;
    deploy.use_irt = true;

    int ret = pnet_deployment_simulate(&deploy);
    ASSERT_EQ(0, ret);

    TEST_PASS();
}

void test_deployment_report(void) {
    TEST_BEGIN("deployment_generate_report");

    pnet_deployment_t deploy;
    pnet_deployment_create(&deploy, "auto-assembly");
    strncpy(deploy.description, "Automated assembly line with 10 robots",
            sizeof(deploy.description) - 1);
    deploy.device_count = 10;
    deploy.use_redundancy = true;
    deploy.use_irt = true;
    deploy.expected_cycle_time_us = 250;

    char report[2048];
    int len = pnet_deployment_generate_report(&deploy, report, sizeof(report));
    ASSERT_GT(len, 0);
    ASSERT_NOT_NULL(strstr(report, "auto-assembly"));
    ASSERT_NOT_NULL(strstr(report, "Redundancy:      Enabled"));
    ASSERT_NOT_NULL(strstr(report, "IRT:             Enabled"));
    ASSERT_NOT_NULL(strstr(report, "250 us"));

    TEST_PASS();
}

/* ===== Integration Test: Full Deployment Pipeline ===== */

void test_full_deployment_pipeline(void) {
    TEST_BEGIN("integration_full_deployment_pipeline");

    /* Step 1: Install check */
    pnet_install_check_t check;
    pnet_install_check(&check);
    ASSERT_TRUE(check.system_compatible);

    /* Step 2: Initialize driver with default config */
    pnet_driver_t driver;
    pnet_driver_config_t config;
    pnet_config_default(&config);
    config.enable_security = true;
    config.enable_monitoring = true;

    int ret = pnet_driver_init(&driver, &config);
    ASSERT_EQ(0, ret);

    /* Step 3: Configure network */
    pnet_network_init(&config.network);
    pnet_interface_init(&config.network.interfaces[0], "eth0");
    pnet_interface_configure(&config.network.interfaces[0],
                             pnet_ip_from_str("192.168.0.10"),
                             pnet_ip_from_str("255.255.255.0"),
                             pnet_ip_from_str("192.168.0.1"));
    config.network.interface_count = 1;

    /* Step 4: Start service (this initializes security subsystems) */
    ret = pnet_driver_start(&driver);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(PNET_SERVICE_RUNNING, pnet_driver_get_state(&driver));

    /* Step 5: Configure security (after driver start, since start calls init) */
    pnet_fw_rule_t fw_rule = {
        .chain = PNET_CHAIN_INPUT,
        .src_ip = pnet_ip_from_str("192.168.0.0"),
        .src_mask = pnet_ip_from_str("255.255.255.0"),
        .port = 102,
        .protocol = PNET_PROTO_TCP,
        .action = PNET_ACL_ACCEPT,
        .active = true
    };
    pnet_firewall_add_rule(&fw_rule);

    /* Step 6: Add devices */
    pnet_device_info_t dev_info = {
        .vendor_id = 0x0001,
        .device_id = 0x0001,
        .type = PNET_DEVICE_TYPE_IO_DEVICE,
        .ip_addr = pnet_ip_from_str("192.168.0.101")
    };
    strncpy(dev_info.device_name, "Sensor-001", sizeof(dev_info.device_name) - 1);
    int idx = pnet_driver_add_device(&driver, &dev_info);
    ASSERT_GE(idx, 0);

    /* Step 7: Establish communication */
    pnet_cr_descriptor_t cr;
    pnet_cr_create(&cr, PNET_CR_TYPE_IO_DATA, PNET_RT_CLASS_RT);
    pnet_cr_activate(&cr);

    pnet_io_data_t io_data;
    memset(&io_data, 0, sizeof(io_data));
    io_data.input_data[0] = 42;
    io_data.input_len = 1;

    ret = pnet_cyclic_send(&cr, &io_data);
    ASSERT_EQ(0, ret);

    /* Step 8: Security audit */
    pnet_security_audit_t audit;
    pnet_security_audit(&audit);
    ASSERT_TRUE(audit.firewall_enabled);

    /* Step 9: Shutdown */
    pnet_cr_destroy(&cr);
    ret = pnet_driver_shutdown(&driver);
    ASSERT_EQ(0, ret);

    TEST_PASS();
}

/* ===== Industrial Application Scenario Tests ===== */

void test_scenario_robotic_assembly(void) {
    TEST_BEGIN("scenario_robotic_assembly_line");

    /* Scenario: Robot assembly with 6-axis robots and sensors */
    pnet_driver_t driver;
    pnet_config_default(&driver.config);
    pnet_driver_init(&driver, &driver.config);
    pnet_driver_start(&driver);

    /* Add robot controller */
    pnet_device_info_t robot = {
        .vendor_id = 0x0001,
        .device_id = 0x1001,
        .type = PNET_DEVICE_TYPE_IO_CONTROLLER,
        .ip_addr = pnet_ip_from_str("192.168.0.10")
    };
    strncpy(robot.device_name, "Robot-Controller-01", sizeof(robot.device_name) - 1);
    pnet_driver_add_device(&driver, &robot);

    /* Add I/O modules */
    for (int i = 0; i < 4; i++) {
        pnet_device_info_t io_module = {
            .vendor_id = 0x0002,
            .device_id = (uint32_t)(0x2000 + i),
            .type = PNET_DEVICE_TYPE_IO_DEVICE,
            .ip_addr = pnet_ip_from_str("192.168.0.20") + (uint32_t)i
        };
        snprintf(io_module.device_name, sizeof(io_module.device_name), "IO-Module-%03d", i + 1);
        pnet_driver_add_device(&driver, &io_module);
    }

    ASSERT_EQ(5, driver.device_count);

    /* Simulate cyclic data exchange for robot control */
    pnet_cr_descriptor_t cr;
    pnet_cr_create(&cr, PNET_CR_TYPE_IO_DATA, PNET_RT_CLASS_RT);
    pnet_cr_activate(&cr);

    for (int cycle = 0; cycle < 100; cycle++) {
        pnet_io_data_t data;
        memset(&data, 0, sizeof(data));
        /* Simulate joint angle data (6 axes) */
        for (int axis = 0; axis < 6; axis++) {
            data.input_data[axis * 4] = (uint8_t)(cycle + axis);
        }
        data.input_len = 24;

        int ret = pnet_cyclic_send(&cr, &data);
        ASSERT_EQ(0, ret);
    }

    pnet_protocol_stats_t stats;
    pnet_protocol_get_stats(&stats);
    ASSERT_GE((int)stats.frames_sent, 100);

    pnet_cr_destroy(&cr);
    pnet_driver_shutdown(&driver);
    TEST_PASS();
}

void test_scenario_redundant_network(void) {
    TEST_BEGIN("scenario_redundant_network_deployment");

    /* Scenario: Redundant Profinet network with dual paths */
    pnet_deployment_t deploy;
    pnet_deployment_create(&deploy, "redundant-network");
    strncpy(deploy.description, "Dual-path redundant Profinet network",
            sizeof(deploy.description) - 1);

    deploy.topology = PNET_TOPOLOGY_RING;
    deploy.use_redundancy = true;
    deploy.device_count = 6;

    /* Primary controller */
    deploy.devices[0].type = PNET_DEVICE_TYPE_IO_CONTROLLER;
    strncpy(deploy.devices[0].device_name, "Primary-PLC", sizeof(deploy.devices[0].device_name) - 1);
    deploy.devices[0].ip_addr = pnet_ip_from_str("192.168.0.1");

    /* Backup controller */
    deploy.devices[1].type = PNET_DEVICE_TYPE_IO_CONTROLLER;
    strncpy(deploy.devices[1].device_name, "Backup-PLC", sizeof(deploy.devices[1].device_name) - 1);
    deploy.devices[1].ip_addr = pnet_ip_from_str("192.168.0.2");

    /* IO devices */
    for (int i = 2; i < 6; i++) {
        deploy.devices[i].type = PNET_DEVICE_TYPE_IO_DEVICE;
        snprintf(deploy.devices[i].device_name, sizeof(deploy.devices[i].device_name),
                 "Sensor-%03d", i - 1);
    }

    int ret = pnet_deployment_validate(&deploy);
    ASSERT_EQ(0, ret);

    ret = pnet_deployment_simulate(&deploy);
    ASSERT_EQ(0, ret);

    char report[2048];
    int len = pnet_deployment_generate_report(&deploy, report, sizeof(report));
    ASSERT_GT(len, 0);
    ASSERT_NOT_NULL(strstr(report, "Redundancy:      Enabled"));

    TEST_PASS();
}

void test_scenario_high_speed_irt(void) {
    TEST_BEGIN("scenario_irt_high_speed_motion_control");

    /* Scenario: IRT class for high-speed motion control (< 250us cycle) */
    pnet_protocol_init();

    pnet_cr_descriptor_t cr;
    pnet_cr_create(&cr, PNET_CR_TYPE_IO_DATA, PNET_RT_CLASS_IRT);
    cr.cycle_time_us = 250;  /* 250us cycle time */
    pnet_cr_activate(&cr);

    ASSERT_EQ(PNET_RT_CLASS_IRT, cr.rt_class);
    ASSERT_EQ(250, (int)cr.cycle_time_us);

    /* Simulate high-speed cyclic exchange */
    for (int i = 0; i < 1000; i++) {
        pnet_io_data_t data;
        memset(&data, 0, sizeof(data));
        data.input_data[0] = (uint8_t)(i & 0xFF);
        data.input_len = 1;

        int ret = pnet_cyclic_send(&cr, &data);
        ASSERT_EQ(0, ret);
    }

    pnet_protocol_stats_t stats;
    pnet_protocol_get_stats(&stats);
    ASSERT_GE((int)stats.frames_sent, 1000);

    pnet_cr_destroy(&cr);
    pnet_protocol_shutdown();
    TEST_PASS();
}

void test_network_script_full_deployment(void) {
    TEST_BEGIN("full_deployment_network_script_generation");

    /* Generate a complete network configuration script */
    pnet_network_config_t config;
    pnet_network_init(&config);

    /* Primary interface */
    pnet_interface_init(&config.interfaces[0], "eth0");
    pnet_interface_configure(&config.interfaces[0],
                             pnet_ip_from_str("192.168.0.10"),
                             pnet_ip_from_str("255.255.255.0"),
                             pnet_ip_from_str("192.168.0.1"));

    /* Secondary interface (redundancy) */
    pnet_interface_init(&config.interfaces[1], "eth1");
    pnet_interface_configure(&config.interfaces[1],
                             pnet_ip_from_str("192.168.1.10"),
                             pnet_ip_from_str("255.255.255.0"),
                             pnet_ip_from_str("192.168.1.1"));
    config.interface_count = 2;

    /* Routes */
    pnet_route_entry_t route = {
        .destination = pnet_ip_from_str("10.0.0.0"),
        .netmask = pnet_ip_from_str("255.0.0.0"),
        .gateway = pnet_ip_from_str("192.168.0.1"),
        .active = true
    };
    strncpy(route.interface, "eth0", PNET_INTERFACE_NAME_LEN - 1);
    pnet_route_add(&config, &route);

    /* TCP tuning */
    pnet_tcp_tuning_init(&config.tcp_tuning);

    char script[4096];
    int net_len = pnet_network_generate_script(&config, script, sizeof(script));
    ASSERT_GT(net_len, 0);

    char sysctl[1024];
    int tcp_len = pnet_tcp_tuning_generate_sysctl(&config.tcp_tuning, sysctl, sizeof(sysctl));
    ASSERT_GT(tcp_len, 0);

    /* Verify both scripts are valid */
    ASSERT_NOT_NULL(strstr(script, "eth0"));
    ASSERT_NOT_NULL(strstr(script, "eth1"));
    ASSERT_NOT_NULL(strstr(sysctl, "tcp_window_scaling"));

    TEST_PASS();
}

/* ===== Run all chapter 6 tests ===== */
void run_chapter6_tests(void) {
    TEST_SUITE_BEGIN("Chapter 6: Practical Applications & Deployment");

    test_deployment_create();
    test_deployment_validate();
    test_deployment_simulate();
    test_deployment_report();

    test_full_deployment_pipeline();

    test_scenario_robotic_assembly();
    test_scenario_redundant_network();
    test_scenario_high_speed_irt();
    test_network_script_full_deployment();

    TEST_SUITE_END();
}
