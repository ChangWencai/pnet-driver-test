/**
 * security_policy.c - Profinet Security Policy Management
 *
 * Implements firewall rule management, IPSec tunnel configuration,
 * and security auditing for PROFINET IO-Device deployments.
 *
 * C11 standard. Linux-specific operations guarded by PLATFORM_LINUX.
 */

#include "security_policy.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/stat.h>

#ifdef PLATFORM_LINUX
#include <unistd.h>
#endif

/* ------------------------------------------------------------------ */
/* Internal helpers                                                    */
/* ------------------------------------------------------------------ */

static const char *action_to_str(sec_action_t action)
{
    switch (action) {
    case SEC_ACTION_ACCEPT: return "ACCEPT";
    case SEC_ACTION_DROP:   return "DROP";
    case SEC_ACTION_REJECT: return "REJECT";
    default:                return "DROP";
    }
}

static const char *chain_to_str(sec_chain_t chain)
{
    switch (chain) {
    case SEC_CHAIN_INPUT:   return "INPUT";
    case SEC_CHAIN_OUTPUT:  return "OUTPUT";
    case SEC_CHAIN_FORWARD: return "FORWARD";
    default:                return "INPUT";
    }
}

static const char *proto_to_str(uint8_t protocol)
{
    switch (protocol) {
    case 6:  return "tcp";
    case 17: return "udp";
    case 0:  return "all";
    default: return "all";
    }
}

/**
 * Format an IPv4 address (host byte order) into dotted-decimal string.
 */
static void ip_to_str(uint32_t ip, char *buf, size_t len)
{
    struct in_addr addr;
    addr.s_addr = htonl(ip);
    inet_ntop(AF_INET, &addr, buf, (socklen_t)len);
}

/**
 * Generate a single iptables rule line into buf.
 * Returns number of characters written (excluding NUL), or -1 on error.
 */
static int generate_rule_line(char *buf, size_t len, const sec_rule_t *rule)
{
    char src_buf[INET_ADDRSTRLEN];
    int  written;

    if (!rule->active) {
        return snprintf(buf, len, "# [inactive] rule: %s\n", rule->comment);
    }

    if (rule->src_ip != 0) {
        ip_to_str(rule->src_ip, src_buf, sizeof(src_buf));
        if (rule->src_mask != 0) {
            char mask_buf[INET_ADDRSTRLEN];
            ip_to_str(rule->src_mask, mask_buf, sizeof(mask_buf));
            written = snprintf(buf, len,
                "iptables -A %s -s %s/%s -p %s --dport %u -j %s",
                chain_to_str(rule->chain),
                src_buf, mask_buf,
                proto_to_str(rule->protocol),
                rule->dst_port,
                action_to_str(rule->action));
        } else {
            written = snprintf(buf, len,
                "iptables -A %s -s %s -p %s --dport %u -j %s",
                chain_to_str(rule->chain),
                src_buf,
                proto_to_str(rule->protocol),
                rule->dst_port,
                action_to_str(rule->action));
        }
    } else {
        written = snprintf(buf, len,
            "iptables -A %s -p %s --dport %u -j %s",
            chain_to_str(rule->chain),
            proto_to_str(rule->protocol),
            rule->dst_port,
            action_to_str(rule->action));
    }

    if (written < 0 || (size_t)written >= len) {
        return -1;
    }
    return written;
}

/* ------------------------------------------------------------------ */
/* Policy lifecycle                                                    */
/* ------------------------------------------------------------------ */

int sec_policy_init(sec_policy_t *policy)
{
    if (!policy) {
        return -EINVAL;
    }

    memset(policy, 0, sizeof(*policy));
    policy->enabled    = false;
    policy->rule_count = 0;
    return 0;
}

int sec_policy_add_rule(sec_policy_t *policy, const sec_rule_t *rule)
{
    if (!policy || !rule) {
        return -EINVAL;
    }

    if (policy->rule_count >= MAX_FW_RULES) {
        fprintf(stderr, "[security_policy] rule table full (%d/%d)\n",
                policy->rule_count, MAX_FW_RULES);
        return -ENOSPC;
    }

    policy->rules[policy->rule_count] = *rule;
    policy->rules[policy->rule_count].active = true;
    policy->rule_count++;

    return 0;
}

int sec_policy_remove_rule(sec_policy_t *policy, int index)
{
    if (!policy) {
        return -EINVAL;
    }

    if (index < 0 || index >= policy->rule_count) {
        return -ERANGE;
    }

    /* Shift remaining rules down by one position */
    for (int i = index; i < policy->rule_count - 1; i++) {
        policy->rules[i] = policy->rules[i + 1];
    }

    /* Clear the now-vacant last slot */
    memset(&policy->rules[policy->rule_count - 1], 0, sizeof(sec_rule_t));
    policy->rule_count--;

    return 0;
}

int sec_policy_flush(sec_policy_t *policy)
{
    if (!policy) {
        return -EINVAL;
    }

    memset(policy->rules, 0, sizeof(policy->rules));
    policy->rule_count = 0;

    return 0;
}

/* ------------------------------------------------------------------ */
/* Apply / script generation                                           */
/* ------------------------------------------------------------------ */

int sec_policy_apply(const sec_policy_t *policy)
{
    if (!policy) {
        return -EINVAL;
    }

    char script[4096];
    int  ret = sec_policy_generate_script(policy, script, sizeof(script));
    if (ret < 0) {
        return ret;
    }

#ifdef PLATFORM_LINUX
    /* Flush existing PROFINET chain and execute the generated script */
    fprintf(stdout, "[security_policy] applying %d rules via iptables\n",
            policy->rule_count);

    ret = system("iptables -F PNET_CHAIN 2>/dev/null");
    (void)ret; /* ignore flush errors if chain doesn't exist */

    ret = system(script);
    if (ret != 0) {
        fprintf(stderr, "[security_policy] iptables execution failed (rc=%d)\n", ret);
        return -EIO;
    }

    fprintf(stdout, "[security_policy] policy applied successfully\n");
#else
    /* Non-Linux: log the generated script only */
    fprintf(stdout,
            "[security_policy] PLATFORM_LINUX not defined - "
            "script generated but NOT executed:\n%s\n", script);
#endif

    return 0;
}

int sec_policy_generate_script(const sec_policy_t *policy, char *buf, size_t len)
{
    if (!policy || !buf || len == 0) {
        return -EINVAL;
    }

    int  offset = 0;
    int  written;

    /* Script header */
    written = snprintf(buf + offset, len - (size_t)offset,
                       "#!/bin/bash\n"
                       "# Auto-generated PROFINET security policy\n"
                       "# Rule count: %d\n\n"
                       "set -e\n\n",
                       policy->rule_count);
    if (written < 0 || (size_t)(offset + written) >= len) {
        return -ENOSPC;
    }
    offset += written;

    /* Generate a rule line for each active rule */
    for (int i = 0; i < policy->rule_count; i++) {
        char line[256];
        int  line_len = generate_rule_line(line, sizeof(line), &policy->rules[i]);
        if (line_len < 0) {
            fprintf(stderr, "[security_policy] failed to generate rule %d\n", i);
            continue;
        }

        written = snprintf(buf + offset, len - (size_t)offset,
                           "# Rule %d: %s\n%s\n\n",
                           i, policy->rules[i].comment, line);
        if (written < 0 || (size_t)(offset + written) >= len) {
            return -ENOSPC;
        }
        offset += written;
    }

    /* Footer */
    written = snprintf(buf + offset, len - (size_t)offset,
                       "echo \"PROFINET security policy applied: %d rules\"\n",
                       policy->rule_count);
    if (written < 0 || (size_t)(offset + written) >= len) {
        return -ENOSPC;
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/* IPSec tunnel management                                             */
/* ------------------------------------------------------------------ */

#define IPSEC_CONF_DIR  "/etc/ipsec.d"

int sec_ipsec_create_tunnel(const char *name, uint32_t local_ip,
                            uint32_t remote_ip, const char *psk)
{
    if (!name || !psk) {
        return -EINVAL;
    }

    char local_str[INET_ADDRSTRLEN];
    char remote_str[INET_ADDRSTRLEN];
    ip_to_str(local_ip, local_str, sizeof(local_str));
    ip_to_str(remote_ip, remote_str, sizeof(remote_str));

    /* Build config file path */
    char conf_path[256];
    snprintf(conf_path, sizeof(conf_path), "%s/%s.conf", IPSEC_CONF_DIR, name);

    FILE *fp = fopen(conf_path, "w");
    if (!fp) {
        fprintf(stderr, "[security_policy] cannot open %s: %s\n",
                conf_path, strerror(errno));
        return -EIO;
    }

    /* strongSwan / libreswan ipsec.conf connection block */
    fprintf(fp,
        "conn %s\n"
        "    authby=secret\n"
        "    left=%%defaultroute\n"
        "    leftid=%s\n"
        "    leftsubnet=%s/32\n"
        "    right=%s\n"
        "    rightsubnet=%s/32\n"
        "    ike=aes256-sha256-modp2048\n"
        "    esp=aes256-sha256\n"
        "    keyexchange=ikev2\n"
        "    auto=start\n"
        "    type=transport\n"
        "\n",
        name,
        local_str, local_str,
        remote_str, remote_str);

    fclose(fp);

    /* Write pre-shared key to ipsec.secrets */
    char secrets_path[256];
    snprintf(secrets_path, sizeof(secrets_path), "%s/%s.secrets",
             IPSEC_CONF_DIR, name);

    fp = fopen(secrets_path, "w");
    if (!fp) {
        fprintf(stderr, "[security_policy] cannot open %s: %s\n",
                secrets_path, strerror(errno));
        return -EIO;
    }

    fprintf(fp, "%s %s : PSK \"%s\"\n", local_str, remote_str, psk);
    fclose(fp);

    /* Restrict secrets file permissions to owner only */
    chmod(secrets_path, 0600);

    fprintf(stdout, "[security_policy] IPSec tunnel '%s' config written to %s\n",
            name, conf_path);

    return 0;
}

int sec_ipsec_remove_tunnel(const char *name)
{
    if (!name) {
        return -EINVAL;
    }

    char conf_path[256];
    snprintf(conf_path, sizeof(conf_path), "%s/%s.conf", IPSEC_CONF_DIR, name);

    char secrets_path[256];
    snprintf(secrets_path, sizeof(secrets_path), "%s/%s.secrets",
             IPSEC_CONF_DIR, name);

    int ret_conf   = remove(conf_path);
    int ret_secret = remove(secrets_path);

    if (ret_conf != 0 && errno != ENOENT) {
        fprintf(stderr, "[security_policy] failed to remove %s: %s\n",
                conf_path, strerror(errno));
        return -EIO;
    }

    if (ret_secret != 0 && errno != ENOENT) {
        fprintf(stderr, "[security_policy] failed to remove %s: %s\n",
                secrets_path, strerror(errno));
        return -EIO;
    }

    fprintf(stdout, "[security_policy] IPSec tunnel '%s' config removed\n", name);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Audit                                                               */
/* ------------------------------------------------------------------ */

int sec_policy_audit(const sec_policy_t *policy, char *report, size_t len)
{
    if (!policy || !report || len == 0) {
        return -EINVAL;
    }

    int  offset   = 0;
    int  written;
    int  warnings = 0;

    written = snprintf(report + offset, len - (size_t)offset,
                       "=== PROFINET Security Policy Audit Report ===\n"
                       "Total rules: %d / %d\n"
                       "Policy enabled: %s\n\n",
                       policy->rule_count, MAX_FW_RULES,
                       policy->enabled ? "yes" : "no");
    if (written < 0 || (size_t)(offset + written) >= len) {
        return -ENOSPC;
    }
    offset += written;

    /* Check: policy disabled */
    if (!policy->enabled) {
        written = snprintf(report + offset, len - (size_t)offset,
                           "[WARN] Policy is disabled - no rules are enforced\n");
        if (written > 0) { offset += written; warnings++; }
    }

    /* Check: empty rule set */
    if (policy->rule_count == 0) {
        written = snprintf(report + offset, len - (size_t)offset,
                           "[WARN] No firewall rules defined\n");
        if (written > 0) { offset += written; warnings++; }
    }

    /* Check: no default-deny rule (a catch-all DROP/REJECT on INPUT) */
    bool has_default_deny = false;
    for (int i = 0; i < policy->rule_count; i++) {
        const sec_rule_t *r = &policy->rules[i];
        if (r->chain == SEC_CHAIN_INPUT &&
            r->protocol == 0 &&
            r->dst_port == 0 &&
            r->src_ip == 0 &&
            (r->action == SEC_ACTION_DROP || r->action == SEC_ACTION_REJECT)) {
            has_default_deny = true;
            break;
        }
    }
    if (!has_default_deny) {
        written = snprintf(report + offset, len - (size_t)offset,
                           "[WARN] No default-deny rule on INPUT chain\n");
        if (written > 0) { offset += written; warnings++; }
    }

    /* Check: no IPSec-related rules (look for port 500/4500 UDP) */
    bool has_ipsec_rules = false;
    for (int i = 0; i < policy->rule_count; i++) {
        const sec_rule_t *r = &policy->rules[i];
        if (r->protocol == 17 && (r->dst_port == 500 || r->dst_port == 4500)) {
            has_ipsec_rules = true;
            break;
        }
    }
    if (!has_ipsec_rules) {
        written = snprintf(report + offset, len - (size_t)offset,
                           "[WARN] No IPSec (UDP 500/4500) rules found - "
                           "consider enabling IPSec for encrypted traffic\n");
        if (written > 0) { offset += written; warnings++; }
    }

    /* Check: inactive rules present */
    int inactive_count = 0;
    for (int i = 0; i < policy->rule_count; i++) {
        if (!policy->rules[i].active) {
            inactive_count++;
        }
    }
    if (inactive_count > 0) {
        written = snprintf(report + offset, len - (size_t)offset,
                           "[INFO] %d inactive rule(s) present\n", inactive_count);
        if (written > 0) { offset += written; }
    }

    /* Check: high rule count approaching limit */
    if (policy->rule_count > MAX_FW_RULES * 3 / 4) {
        written = snprintf(report + offset, len - (size_t)offset,
                           "[WARN] Rule table is %d%% full (%d/%d) - "
                           "consider consolidating rules\n",
                           (policy->rule_count * 100) / MAX_FW_RULES,
                           policy->rule_count, MAX_FW_RULES);
        if (written > 0) { offset += written; warnings++; }
    }

    /* Summary */
    written = snprintf(report + offset, len - (size_t)offset,
                       "\nAudit complete: %d warning(s)\n", warnings);
    if (written < 0 || (size_t)(offset + written) >= len) {
        return -ENOSPC;
    }

    return warnings;
}
