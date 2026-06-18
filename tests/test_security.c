/**
 * test_security.c - Security policy module tests
 */
#include "test_framework.h"
#include "security_policy.h"
#include <string.h>

void test_policy_init(void) {
    TEST_BEGIN("sec_policy_init");
    sec_policy_t policy;
    int ret = sec_policy_init(&policy);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(0, policy.rule_count);
    ASSERT_FALSE(policy.enabled);
    TEST_PASS();
}

void test_policy_add_rule(void) {
    TEST_BEGIN("sec_policy_add_rules");
    sec_policy_t policy;
    sec_policy_init(&policy);

    sec_rule_t rule = {
        .chain = SEC_CHAIN_INPUT,
        .dst_port = 102,
        .protocol = 6,  /* TCP */
        .action = SEC_ACTION_ACCEPT,
        .priority = 1,
        .active = true
    };
    strncpy(rule.comment, "Allow Profinet", sizeof(rule.comment) - 1);

    int ret = sec_policy_add_rule(&policy, &rule);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(1, policy.rule_count);

    rule.action = SEC_ACTION_DROP;
    rule.priority = 99;
    strncpy(rule.comment, "Default deny", sizeof(rule.comment) - 1);
    ret = sec_policy_add_rule(&policy, &rule);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(2, policy.rule_count);
    TEST_PASS();
}

void test_policy_remove_rule(void) {
    TEST_BEGIN("sec_policy_remove_rule");
    sec_policy_t policy;
    sec_policy_init(&policy);

    sec_rule_t rule = { .action = SEC_ACTION_ACCEPT, .active = true };
    sec_policy_add_rule(&policy, &rule);
    sec_policy_add_rule(&policy, &rule);
    ASSERT_EQ(2, policy.rule_count);

    sec_policy_remove_rule(&policy, 0);
    ASSERT_EQ(1, policy.rule_count);
    TEST_PASS();
}

void test_policy_flush(void) {
    TEST_BEGIN("sec_policy_flush");
    sec_policy_t policy;
    sec_policy_init(&policy);

    sec_rule_t rule = { .action = SEC_ACTION_ACCEPT, .active = true };
    sec_policy_add_rule(&policy, &rule);
    sec_policy_add_rule(&policy, &rule);

    sec_policy_flush(&policy);
    ASSERT_EQ(0, policy.rule_count);
    TEST_PASS();
}

void test_policy_generate_script(void) {
    TEST_BEGIN("sec_policy_generate_iptables_script");
    sec_policy_t policy;
    sec_policy_init(&policy);

    sec_rule_t rule1 = {
        .chain = SEC_CHAIN_INPUT,
        .src_ip = 0xC0A80164,  /* 192.168.1.100 */
        .dst_port = 102,
        .protocol = 6,
        .action = SEC_ACTION_ACCEPT,
        .active = true
    };
    sec_policy_add_rule(&policy, &rule1);

    sec_rule_t rule2 = {
        .chain = SEC_CHAIN_INPUT,
        .dst_port = 102,
        .protocol = 6,
        .action = SEC_ACTION_DROP,
        .active = true
    };
    sec_policy_add_rule(&policy, &rule2);

    char script[4096];
    int ret = sec_policy_generate_script(&policy, script, sizeof(script));
    ASSERT_EQ(0, ret);
    ASSERT_NOT_NULL(strstr(script, "iptables"));
    ASSERT_NOT_NULL(strstr(script, "ACCEPT"));
    ASSERT_NOT_NULL(strstr(script, "DROP"));
    TEST_PASS();
}

void test_policy_audit(void) {
    TEST_BEGIN("sec_policy_audit_checks");
    sec_policy_t policy;
    sec_policy_init(&policy);

    /* Empty policy should have warnings */
    char report[1024];
    int warnings = sec_policy_audit(&policy, report, sizeof(report));
    ASSERT_GT(warnings, 0);

    /* Add a proper rule set */
    sec_rule_t rule = {
        .chain = SEC_CHAIN_INPUT,
        .dst_port = 102,
        .protocol = 6,
        .action = SEC_ACTION_ACCEPT,
        .active = true
    };
    sec_policy_add_rule(&policy, &rule);

    rule.action = SEC_ACTION_DROP;
    sec_policy_add_rule(&policy, &rule);

    warnings = sec_policy_audit(&policy, report, sizeof(report));
    ASSERT_GE(warnings, 0);
    ASSERT_GT((int)strlen(report), 0);
    TEST_PASS();
}

void test_policy_apply_mock(void) {
    TEST_BEGIN("sec_policy_apply_no_crash");
    sec_policy_t policy;
    sec_policy_init(&policy);

    sec_rule_t rule = {
        .chain = SEC_CHAIN_INPUT,
        .dst_port = 102,
        .protocol = 6,
        .action = SEC_ACTION_DROP,
        .active = true
    };
    sec_policy_add_rule(&policy, &rule);

    /* On non-Linux, this just generates a script */
    int ret = sec_policy_apply(&policy);
    /* Should succeed (or skip gracefully) */
    (void)ret;
    TEST_PASS();
}

void run_security_tests(void) {
    TEST_SUITE_BEGIN("Security Policy Module");
    test_policy_init();
    test_policy_add_rule();
    test_policy_remove_rule();
    test_policy_flush();
    test_policy_generate_script();
    test_policy_audit();
    test_policy_apply_mock();
    TEST_SUITE_END();
}
