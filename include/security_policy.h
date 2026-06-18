/**
 * security_policy.h - Profinet Security Policy Management
 *
 * Manages firewall rules, ACL, and IPSec via Linux nftables/iptables.
 */

#ifndef SECURITY_POLICY_H
#define SECURITY_POLICY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define MAX_FW_RULES 64

typedef enum {
    SEC_ACTION_ACCEPT = 0,
    SEC_ACTION_DROP,
    SEC_ACTION_REJECT
} sec_action_t;

typedef enum {
    SEC_CHAIN_INPUT = 0,
    SEC_CHAIN_OUTPUT,
    SEC_CHAIN_FORWARD
} sec_chain_t;

typedef struct {
    sec_chain_t  chain;
    uint32_t     src_ip;
    uint32_t     src_mask;
    uint32_t     dst_port;
    uint8_t      protocol;   /* 6=TCP, 17=UDP, 0=all */
    sec_action_t action;
    int          priority;
    bool         active;
    char         comment[64];
} sec_rule_t;

typedef struct {
    sec_rule_t rules[MAX_FW_RULES];
    int        rule_count;
    bool       enabled;
} sec_policy_t;

/* Policy lifecycle */
int  sec_policy_init(sec_policy_t *policy);
int  sec_policy_add_rule(sec_policy_t *policy, const sec_rule_t *rule);
int  sec_policy_remove_rule(sec_policy_t *policy, int index);
int  sec_policy_flush(sec_policy_t *policy);

/* Apply to system (generates and executes iptables/nftables commands) */
int  sec_policy_apply(const sec_policy_t *policy);
int  sec_policy_generate_script(const sec_policy_t *policy, char *buf, size_t len);

/* IPSec tunnel management */
int  sec_ipsec_create_tunnel(const char *name, uint32_t local_ip,
                            uint32_t remote_ip, const char *psk);
int  sec_ipsec_remove_tunnel(const char *name);

/* Audit */
int  sec_policy_audit(const sec_policy_t *policy, char *report, size_t len);

#endif /* SECURITY_POLICY_H */
