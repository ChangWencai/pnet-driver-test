/**
 * test_security.c - Chapter 5: Security Tests
 *
 * Tests for Profinet security mechanisms including:
 * - ACL rule management and packet filtering
 * - Firewall rule management and iptables generation
 * - IPSec tunnel configuration and lifecycle
 * - Security audit and reporting
 */

#include "test_framework.h"
#include "pnet_security.h"
#include "pnet_network.h"
#include <string.h>

/* ===== ACL Tests ===== */

void test_acl_init_and_add(void) {
    TEST_BEGIN("acl_init_and_add_rules");

    int ret = pnet_acl_init();
    ASSERT_EQ(0, ret);

    /* Add allow rule for specific IPs on Profinet port */
    pnet_acl_rule_t rule1 = {
        .src_ip = pnet_ip_from_str("192.168.1.100"),
        .src_mask = pnet_ip_from_str("255.255.255.255"),
        .dst_port = 102,
        .protocol = PNET_PROTO_TCP,
        .action = PNET_ACL_ACCEPT,
        .priority = 1,
        .active = true
    };
    strncpy(rule1.description, "Allow PLC-1 on port 102", sizeof(rule1.description) - 1);

    ret = pnet_acl_add_rule(&rule1);
    ASSERT_EQ(0, ret);

    pnet_acl_rule_t rule2 = {
        .src_ip = pnet_ip_from_str("192.168.1.101"),
        .src_mask = pnet_ip_from_str("255.255.255.255"),
        .dst_port = 102,
        .protocol = PNET_PROTO_TCP,
        .action = PNET_ACL_ACCEPT,
        .priority = 2,
        .active = true
    };
    strncpy(rule2.description, "Allow PLC-2 on port 102", sizeof(rule2.description) - 1);

    ret = pnet_acl_add_rule(&rule2);
    ASSERT_EQ(0, ret);

    /* Default deny rule */
    pnet_acl_rule_t deny_rule = {
        .dst_port = 102,
        .protocol = PNET_PROTO_TCP,
        .action = PNET_ACL_DROP,
        .priority = 99,
        .active = true
    };
    strncpy(deny_rule.description, "Deny all other traffic on port 102", sizeof(deny_rule.description) - 1);

    ret = pnet_acl_add_rule(&deny_rule);
    ASSERT_EQ(0, ret);

    TEST_PASS();
}

void test_acl_check(void) {
    TEST_BEGIN("acl_check_packet_filtering");

    pnet_acl_init();

    /* Setup rules */
    pnet_acl_rule_t allow = {
        .src_ip = pnet_ip_from_str("192.168.1.100"),
        .src_mask = pnet_ip_from_str("255.255.255.255"),
        .dst_port = 102,
        .protocol = PNET_PROTO_TCP,
        .action = PNET_ACL_ACCEPT,
        .priority = 1,
        .active = true
    };
    pnet_acl_add_rule(&allow);

    pnet_acl_rule_t deny = {
        .dst_port = 102,
        .protocol = PNET_PROTO_TCP,
        .action = PNET_ACL_DROP,
        .priority = 99,
        .active = true
    };
    pnet_acl_add_rule(&deny);

    /* Allowed IP should pass */
    int result = pnet_acl_check(
        pnet_ip_from_str("192.168.1.100"),
        pnet_ip_from_str("192.168.1.1"),
        102, PNET_PROTO_TCP);
    ASSERT_EQ((int)PNET_ACL_ACCEPT, result);

    /* Unauthorized IP should be dropped */
    result = pnet_acl_check(
        pnet_ip_from_str("10.0.0.99"),
        pnet_ip_from_str("192.168.1.1"),
        102, PNET_PROTO_TCP);
    ASSERT_EQ((int)PNET_ACL_DROP, result);

    TEST_PASS();
}

void test_acl_remove_flush(void) {
    TEST_BEGIN("acl_remove_and_flush_rules");

    pnet_acl_init();

    pnet_acl_rule_t rule = {
        .dst_port = 102,
        .protocol = PNET_PROTO_TCP,
        .action = PNET_ACL_ACCEPT,
        .priority = 10,
        .active = true
    };
    pnet_acl_add_rule(&rule);

    int ret = pnet_acl_remove_rule(10);
    ASSERT_EQ(0, ret);

    /* Remove non-existent rule */
    ret = pnet_acl_remove_rule(999);
    ASSERT_EQ(-1, ret);

    /* Flush all */
    pnet_acl_add_rule(&rule);
    ret = pnet_acl_flush();
    ASSERT_EQ(0, ret);

    TEST_PASS();
}

void test_acl_list(void) {
    TEST_BEGIN("acl_list_rules_output");

    pnet_acl_init();

    pnet_acl_rule_t rule = {
        .dst_port = 102,
        .protocol = PNET_PROTO_TCP,
        .action = PNET_ACL_ACCEPT,
        .priority = 1,
        .active = true
    };
    strncpy(rule.description, "Allow Profinet", sizeof(rule.description) - 1);
    pnet_acl_add_rule(&rule);

    char buffer[1024];
    int len = pnet_acl_list(buffer, sizeof(buffer));
    ASSERT_GT(len, 0);
    ASSERT_NOT_NULL(strstr(buffer, "Allow Profinet"));

    TEST_PASS();
}

/* ===== Firewall Tests ===== */

void test_firewall_init_and_add(void) {
    TEST_BEGIN("firewall_init_and_add_rules");

    int ret = pnet_firewall_init();
    ASSERT_EQ(0, ret);

    /* Accept outbound on Profinet port */
    pnet_fw_rule_t out_rule = {
        .chain = PNET_CHAIN_OUTPUT,
        .port = 102,
        .protocol = PNET_PROTO_TCP,
        .action = PNET_ACL_ACCEPT,
        .active = true
    };
    ret = pnet_firewall_add_rule(&out_rule);
    ASSERT_EQ(0, ret);

    /* Drop inbound on Profinet port */
    pnet_fw_rule_t in_rule = {
        .chain = PNET_CHAIN_INPUT,
        .port = 102,
        .protocol = PNET_PROTO_TCP,
        .action = PNET_ACL_DROP,
        .active = true
    };
    ret = pnet_firewall_add_rule(&in_rule);
    ASSERT_EQ(0, ret);

    TEST_PASS();
}

void test_firewall_generate_iptables(void) {
    TEST_BEGIN("firewall_generate_iptables_script");

    pnet_firewall_init();

    pnet_fw_rule_t rule1 = {
        .chain = PNET_CHAIN_INPUT,
        .src_ip = pnet_ip_from_str("192.168.1.100"),
        .port = 102,
        .protocol = PNET_PROTO_TCP,
        .action = PNET_ACL_ACCEPT,
        .active = true
    };
    pnet_firewall_add_rule(&rule1);

    pnet_fw_rule_t rule2 = {
        .chain = PNET_CHAIN_INPUT,
        .port = 102,
        .protocol = PNET_PROTO_TCP,
        .action = PNET_ACL_DROP,
        .active = true
    };
    pnet_firewall_add_rule(&rule2);

    char script[2048];
    int len = pnet_firewall_generate_iptables(script, sizeof(script));
    ASSERT_GT(len, 0);
    ASSERT_NOT_NULL(strstr(script, "#!/bin/bash"));
    ASSERT_NOT_NULL(strstr(script, "iptables -A INPUT"));
    ASSERT_NOT_NULL(strstr(script, "-j ACCEPT"));
    ASSERT_NOT_NULL(strstr(script, "-j DROP"));

    TEST_PASS();
}

void test_firewall_remove_flush(void) {
    TEST_BEGIN("firewall_remove_and_flush");

    pnet_firewall_init();

    pnet_fw_rule_t rule = {
        .chain = PNET_CHAIN_INPUT,
        .port = 102,
        .protocol = PNET_PROTO_TCP,
        .action = PNET_ACL_DROP,
        .active = true
    };
    pnet_firewall_add_rule(&rule);

    int ret = pnet_firewall_remove_rule(0);
    ASSERT_EQ(0, ret);

    pnet_firewall_add_rule(&rule);
    ret = pnet_firewall_flush();
    ASSERT_EQ(0, ret);

    TEST_PASS();
}

/* ===== IPSec Tests ===== */

void test_ipsec_tunnel_lifecycle(void) {
    TEST_BEGIN("ipsec_tunnel_create_start_stop_destroy");

    pnet_ipsec_tunnel_t tunnel;
    memset(&tunnel, 0, sizeof(tunnel));
    strncpy(tunnel.name, "profinet-tunnel-1", sizeof(tunnel.name) - 1);
    tunnel.local_ip = pnet_ip_from_str("192.168.1.1");
    tunnel.remote_ip = pnet_ip_from_str("192.168.1.2");
    tunnel.mode = PNET_IPSEC_TUNNEL;
    tunnel.encrypt_algo = PNET_ENCRYPT_AES128;
    tunnel.auth_algo = PNET_AUTH_SHA1;
    memcpy(tunnel.psk, "shared-secret-key", 17);
    tunnel.psk_len = 17;
    tunnel.auto_start = true;

    /* Create */
    int ret = pnet_ipsec_create_tunnel(&tunnel);
    ASSERT_EQ(0, ret);
    ASSERT_FALSE(pnet_ipsec_is_tunnel_active("profinet-tunnel-1"));

    /* Start */
    ret = pnet_ipsec_start_tunnel("profinet-tunnel-1");
    ASSERT_EQ(0, ret);
    ASSERT_TRUE(pnet_ipsec_is_tunnel_active("profinet-tunnel-1"));

    /* Stop */
    ret = pnet_ipsec_stop_tunnel("profinet-tunnel-1");
    ASSERT_EQ(0, ret);
    ASSERT_FALSE(pnet_ipsec_is_tunnel_active("profinet-tunnel-1"));

    /* Destroy */
    ret = pnet_ipsec_destroy_tunnel("profinet-tunnel-1");
    ASSERT_EQ(0, ret);

    TEST_PASS();
}

void test_ipsec_generate_config(void) {
    TEST_BEGIN("ipsec_generate_config_file");

    pnet_ipsec_tunnel_t tunnel;
    memset(&tunnel, 0, sizeof(tunnel));
    strncpy(tunnel.name, "my-ipsec-tunnel", sizeof(tunnel.name) - 1);
    tunnel.encrypt_algo = PNET_ENCRYPT_AES128;
    tunnel.auto_start = true;

    char config[1024];
    int len = pnet_ipsec_generate_config(&tunnel, config, sizeof(config));
    ASSERT_GT(len, 0);
    ASSERT_NOT_NULL(strstr(config, "my-ipsec-tunnel"));
    ASSERT_NOT_NULL(strstr(config, "aes128"));
    ASSERT_NOT_NULL(strstr(config, "auto=start"));

    TEST_PASS();
}

/* ===== Security Audit Tests ===== */

void test_security_audit(void) {
    TEST_BEGIN("security_audit_comprehensive_check");

    /* Setup some rules */
    pnet_acl_init();
    pnet_firewall_init();

    pnet_fw_rule_t rule = {
        .chain = PNET_CHAIN_INPUT,
        .port = 102,
        .protocol = PNET_PROTO_TCP,
        .action = PNET_ACL_DROP,
        .active = true
    };
    pnet_firewall_add_rule(&rule);

    /* Run audit */
    pnet_security_audit_t audit;
    int ret = pnet_security_audit(&audit);
    ASSERT_EQ(0, ret);
    ASSERT_GE(audit.total_rules, 1);
    ASSERT_TRUE(audit.firewall_enabled);
    ASSERT_FALSE(audit.ipsec_enabled);

    TEST_PASS();
}

void test_security_report(void) {
    TEST_BEGIN("security_generate_audit_report");

    pnet_acl_init();
    pnet_firewall_init();

    pnet_fw_rule_t rule = {
        .chain = PNET_CHAIN_INPUT,
        .port = 102,
        .protocol = PNET_PROTO_TCP,
        .action = PNET_ACL_ACCEPT,
        .active = true
    };
    pnet_firewall_add_rule(&rule);

    pnet_security_audit_t audit;
    pnet_security_audit(&audit);

    char report[2048];
    int len = pnet_security_generate_report(&audit, report, sizeof(report));
    ASSERT_GT(len, 0);
    ASSERT_NOT_NULL(strstr(report, "Security Audit Report"));
    ASSERT_NOT_NULL(strstr(report, "Total rules:"));

    TEST_PASS();
}

void test_security_patch_check(void) {
    TEST_BEGIN("security_patch_check");

    char buffer[512];
    int len = pnet_security_check_patches("p-net", buffer, sizeof(buffer));
    ASSERT_GT(len, 0);
    ASSERT_NOT_NULL(strstr(buffer, "p-net"));

    TEST_PASS();
}

void test_security_utility_strings(void) {
    TEST_BEGIN("security_utility_string_conversions");

    ASSERT_STR_EQ("ACCEPT", pnet_acl_action_str(PNET_ACL_ACCEPT));
    ASSERT_STR_EQ("DROP", pnet_acl_action_str(PNET_ACL_DROP));
    ASSERT_STR_EQ("REJECT", pnet_acl_action_str(PNET_ACL_REJECT));

    ASSERT_STR_EQ("INPUT", pnet_chain_str(PNET_CHAIN_INPUT));
    ASSERT_STR_EQ("OUTPUT", pnet_chain_str(PNET_CHAIN_OUTPUT));
    ASSERT_STR_EQ("FORWARD", pnet_chain_str(PNET_CHAIN_FORWARD));

    ASSERT_STR_EQ("aes128", pnet_encrypt_algo_str(PNET_ENCRYPT_AES128));
    ASSERT_STR_EQ("aes256", pnet_encrypt_algo_str(PNET_ENCRYPT_AES256));

    TEST_PASS();
}

/* ===== Run all chapter 5 tests ===== */
void run_chapter5_tests(void) {
    TEST_SUITE_BEGIN("Chapter 5: Security Mechanisms & Hardening");

    test_acl_init_and_add();
    test_acl_check();
    test_acl_remove_flush();
    test_acl_list();

    test_firewall_init_and_add();
    test_firewall_generate_iptables();
    test_firewall_remove_flush();

    test_ipsec_tunnel_lifecycle();
    test_ipsec_generate_config();

    test_security_audit();
    test_security_report();
    test_security_patch_check();
    test_security_utility_strings();

    TEST_SUITE_END();
}
