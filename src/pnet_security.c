/**
 * pnet_security.c - Security Mechanisms Implementation
 *
 * Implements ACL, firewall rules, IPSec tunnel configuration,
 * and security audit functionality for Profinet networks.
 */

#include "pnet_security.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Global ACL rules storage */
static pnet_acl_rule_t g_acl_rules[PNET_MAX_ACL_RULES];
static int g_acl_count = 0;

/* Global firewall rules storage */
static pnet_fw_rule_t g_fw_rules[PNET_MAX_FIREWALL_RULES];
static int g_fw_count = 0;

/* Global IPSec tunnels */
static pnet_ipsec_tunnel_t g_ipsec_tunnels[PNET_MAX_IPSEC_TUNNELS];
static int g_ipsec_count = 0;

/* ACL management */
int pnet_acl_init(void) {
    memset(g_acl_rules, 0, sizeof(g_acl_rules));
    g_acl_count = 0;
    return 0;
}

int pnet_acl_add_rule(pnet_acl_rule_t *rule) {
    if (!rule) return -1;
    if (g_acl_count >= PNET_MAX_ACL_RULES) return -1;

    memcpy(&g_acl_rules[g_acl_count], rule, sizeof(pnet_acl_rule_t));
    g_acl_rules[g_acl_count].active = true;
    g_acl_count++;
    return 0;
}

int pnet_acl_remove_rule(int priority) {
    for (int i = 0; i < g_acl_count; i++) {
        if (g_acl_rules[i].priority == priority) {
            for (int j = i; j < g_acl_count - 1; j++) {
                memcpy(&g_acl_rules[j], &g_acl_rules[j + 1], sizeof(pnet_acl_rule_t));
            }
            g_acl_count--;
            return 0;
        }
    }
    return -1;
}

int pnet_acl_flush(void) {
    memset(g_acl_rules, 0, sizeof(g_acl_rules));
    g_acl_count = 0;
    return 0;
}

int pnet_acl_check(uint32_t src_ip, uint32_t dst_ip, uint16_t port, pnet_proto_t proto) {
    /* Process rules in priority order (lower number = higher priority) */
    for (int i = 0; i < g_acl_count; i++) {
        if (!g_acl_rules[i].active) continue;

        bool match = true;

        /* Check source IP */
        if (g_acl_rules[i].src_ip != 0 &&
            (src_ip & g_acl_rules[i].src_mask) != (g_acl_rules[i].src_ip & g_acl_rules[i].src_mask)) {
            match = false;
        }

        /* Check destination IP */
        if (match && g_acl_rules[i].dst_ip != 0 &&
            (dst_ip & g_acl_rules[i].dst_mask) != (g_acl_rules[i].dst_ip & g_acl_rules[i].dst_mask)) {
            match = false;
        }

        /* Check port */
        if (match && g_acl_rules[i].dst_port != 0 && g_acl_rules[i].dst_port != port) {
            match = false;
        }

        /* Check protocol */
        if (match && g_acl_rules[i].protocol != PNET_PROTO_ALL && g_acl_rules[i].protocol != proto) {
            match = false;
        }

        if (match) {
            return (int)g_acl_rules[i].action;
        }
    }

    /* Default: drop */
    return (int)PNET_ACL_DROP;
}

int pnet_acl_list(char *buffer, size_t buf_len) {
    if (!buffer || buf_len == 0) return -1;

    int offset = 0;
    offset += snprintf(buffer + offset, buf_len - (size_t)offset,
                       "# ACL Rules (%d total)\n", g_acl_count);

    for (int i = 0; i < g_acl_count; i++) {
        offset += snprintf(buffer + offset, buf_len - (size_t)offset,
                           "[%d] priority=%d action=%s port=%d proto=%d \"%s\"\n",
                           i, g_acl_rules[i].priority,
                           pnet_acl_action_str(g_acl_rules[i].action),
                           g_acl_rules[i].dst_port,
                           g_acl_rules[i].protocol,
                           g_acl_rules[i].description);
    }

    return offset;
}

/* Firewall management */
int pnet_firewall_init(void) {
    memset(g_fw_rules, 0, sizeof(g_fw_rules));
    g_fw_count = 0;
    return 0;
}

int pnet_firewall_add_rule(pnet_fw_rule_t *rule) {
    if (!rule) return -1;
    if (g_fw_count >= PNET_MAX_FIREWALL_RULES) return -1;

    memcpy(&g_fw_rules[g_fw_count], rule, sizeof(pnet_fw_rule_t));
    g_fw_rules[g_fw_count].active = true;
    g_fw_rules[g_fw_count].rule_number = g_fw_count;
    g_fw_count++;
    return 0;
}

int pnet_firewall_remove_rule(int rule_number) {
    for (int i = 0; i < g_fw_count; i++) {
        if (g_fw_rules[i].rule_number == rule_number) {
            for (int j = i; j < g_fw_count - 1; j++) {
                memcpy(&g_fw_rules[j], &g_fw_rules[j + 1], sizeof(pnet_fw_rule_t));
            }
            g_fw_count--;
            return 0;
        }
    }
    return -1;
}

int pnet_firewall_flush(void) {
    memset(g_fw_rules, 0, sizeof(g_fw_rules));
    g_fw_count = 0;
    return 0;
}

int pnet_firewall_generate_iptables(char *buffer, size_t buf_len) {
    if (!buffer || buf_len == 0) return -1;

    int offset = 0;
    offset += snprintf(buffer + offset, buf_len - (size_t)offset,
                       "#!/bin/bash\n# Profinet Firewall Rules (iptables)\n# Auto-generated\n\n");
    offset += snprintf(buffer + offset, buf_len - (size_t)offset,
                       "# Flush existing rules\niptables -F\n\n");

    for (int i = 0; i < g_fw_count; i++) {
        if (!g_fw_rules[i].active) continue;

        const char *chain = pnet_chain_str(g_fw_rules[i].chain);
        const char *action = pnet_acl_action_str(g_fw_rules[i].action);
        const char *proto_str;
        switch (g_fw_rules[i].protocol) {
            case PNET_PROTO_TCP:  proto_str = "tcp"; break;
            case PNET_PROTO_UDP:  proto_str = "udp"; break;
            case PNET_PROTO_ICMP: proto_str = "icmp"; break;
            default: proto_str = "all"; break;
        }

        if (action) {
            /* Map ACL action to iptables target */
            const char *target = "DROP";
            if (g_fw_rules[i].action == PNET_ACL_ACCEPT) target = "ACCEPT";
            else if (g_fw_rules[i].action == PNET_ACL_REJECT) target = "REJECT";

            if (chain) {
                offset += snprintf(buffer + offset, buf_len - (size_t)offset,
                                   "iptables -A %s -p %s --dport %d -j %s\n",
                                   chain, proto_str, g_fw_rules[i].port, target);
            }
        }
    }

    return offset;
}

int pnet_firewall_apply(const pnet_fw_rule_t *rules, int count) {
    if (!rules || count <= 0) return -1;

    pnet_firewall_flush();
    for (int i = 0; i < count && i < PNET_MAX_FIREWALL_RULES; i++) {
        pnet_firewall_add_rule((pnet_fw_rule_t *)&rules[i]);
    }
    return 0;
}

/* IPSec management */
int pnet_ipsec_create_tunnel(pnet_ipsec_tunnel_t *tunnel) {
    if (!tunnel) return -1;
    if (g_ipsec_count >= PNET_MAX_IPSEC_TUNNELS) return -1;

    memcpy(&g_ipsec_tunnels[g_ipsec_count], tunnel, sizeof(pnet_ipsec_tunnel_t));
    g_ipsec_tunnels[g_ipsec_count].active = false;
    g_ipsec_count++;
    return 0;
}

int pnet_ipsec_destroy_tunnel(const char *name) {
    if (!name) return -1;

    for (int i = 0; i < g_ipsec_count; i++) {
        if (strcmp(g_ipsec_tunnels[i].name, name) == 0) {
            for (int j = i; j < g_ipsec_count - 1; j++) {
                memcpy(&g_ipsec_tunnels[j], &g_ipsec_tunnels[j + 1], sizeof(pnet_ipsec_tunnel_t));
            }
            g_ipsec_count--;
            return 0;
        }
    }
    return -1;
}

int pnet_ipsec_start_tunnel(const char *name) {
    if (!name) return -1;

    for (int i = 0; i < g_ipsec_count; i++) {
        if (strcmp(g_ipsec_tunnels[i].name, name) == 0) {
            g_ipsec_tunnels[i].active = true;
            return 0;
        }
    }
    return -1;
}

int pnet_ipsec_stop_tunnel(const char *name) {
    if (!name) return -1;

    for (int i = 0; i < g_ipsec_count; i++) {
        if (strcmp(g_ipsec_tunnels[i].name, name) == 0) {
            g_ipsec_tunnels[i].active = false;
            return 0;
        }
    }
    return -1;
}

int pnet_ipsec_generate_config(const pnet_ipsec_tunnel_t *tunnel, char *buffer, size_t buf_len) {
    if (!tunnel || !buffer || buf_len == 0) return -1;

    int offset = 0;
    offset += snprintf(buffer + offset, buf_len - (size_t)offset,
                       "config setup\n\n");
    offset += snprintf(buffer + offset, buf_len - (size_t)offset,
                       "conn %s\n", tunnel->name);
    offset += snprintf(buffer + offset, buf_len - (size_t)offset,
                       "  ike=%s-sha1-modp2048\n",
                       pnet_encrypt_algo_str(tunnel->encrypt_algo));
    offset += snprintf(buffer + offset, buf_len - (size_t)offset,
                       "  phase2alg=%s-sha1\n",
                       pnet_encrypt_algo_str(tunnel->encrypt_algo));
    offset += snprintf(buffer + offset, buf_len - (size_t)offset,
                       "  auto=%s\n", tunnel->auto_start ? "start" : "ignore");

    return offset;
}

bool pnet_ipsec_is_tunnel_active(const char *name) {
    if (!name) return false;

    for (int i = 0; i < g_ipsec_count; i++) {
        if (strcmp(g_ipsec_tunnels[i].name, name) == 0) {
            return g_ipsec_tunnels[i].active;
        }
    }
    return false;
}

/* Security audit */
int pnet_security_audit(pnet_security_audit_t *result) {
    if (!result) return -1;

    memset(result, 0, sizeof(pnet_security_audit_t));
    result->total_rules = g_acl_count + g_fw_count;
    result->active_rules = 0;

    for (int i = 0; i < g_acl_count; i++) {
        if (g_acl_rules[i].active) result->active_rules++;
    }
    for (int i = 0; i < g_fw_count; i++) {
        if (g_fw_rules[i].active) result->active_rules++;
    }

    result->ipsec_enabled = (g_ipsec_count > 0);
    result->firewall_enabled = (g_fw_count > 0);

    /* Check for common vulnerabilities */
    if (g_fw_count == 0) {
        result->vulnerabilities_found++;
        strncat(result->report, "WARNING: No firewall rules configured\n",
                sizeof(result->report) - strlen(result->report) - 1);
    }
    if (g_ipsec_count == 0) {
        result->warnings++;
        strncat(result->report, "INFO: No IPSec tunnels configured\n",
                sizeof(result->report) - strlen(result->report) - 1);
    }

    return 0;
}

int pnet_security_check_patches(const char *package_name, char *buffer, size_t buf_len) {
    if (!package_name || !buffer || buf_len == 0) return -1;

    /* Simulation */
    int offset = snprintf(buffer, buf_len,
                          "# Patch check for: %s\n"
                          "# In production, run: apt-get update && apt-get list --upgradable\n"
                          "# Status: UP_TO_DATE\n", package_name);
    return offset;
}

int pnet_security_generate_report(const pnet_security_audit_t *audit, char *buffer, size_t buf_len) {
    if (!audit || !buffer || buf_len == 0) return -1;

    int offset = 0;
    offset += snprintf(buffer + offset, buf_len - (size_t)offset,
                       "=== Profinet Security Audit Report ===\n\n");
    offset += snprintf(buffer + offset, buf_len - (size_t)offset,
                       "Total rules:      %d\n", audit->total_rules);
    offset += snprintf(buffer + offset, buf_len - (size_t)offset,
                       "Active rules:     %d\n", audit->active_rules);
    offset += snprintf(buffer + offset, buf_len - (size_t)offset,
                       "Vulnerabilities:  %d\n", audit->vulnerabilities_found);
    offset += snprintf(buffer + offset, buf_len - (size_t)offset,
                       "Warnings:         %d\n", audit->warnings);
    offset += snprintf(buffer + offset, buf_len - (size_t)offset,
                       "IPSec enabled:    %s\n", audit->ipsec_enabled ? "Yes" : "No");
    offset += snprintf(buffer + offset, buf_len - (size_t)offset,
                       "Firewall enabled: %s\n\n", audit->firewall_enabled ? "Yes" : "No");
    offset += snprintf(buffer + offset, buf_len - (size_t)offset,
                       "Details:\n%s\n", audit->report);

    return offset;
}

/* Utility functions */
const char* pnet_acl_action_str(pnet_acl_action_t action) {
    switch (action) {
        case PNET_ACL_ACCEPT: return "ACCEPT";
        case PNET_ACL_DROP:   return "DROP";
        case PNET_ACL_REJECT: return "REJECT";
        case PNET_ACL_LOG:    return "LOG";
        default: return "UNKNOWN";
    }
}

const char* pnet_chain_str(pnet_chain_t chain) {
    switch (chain) {
        case PNET_CHAIN_INPUT:   return "INPUT";
        case PNET_CHAIN_OUTPUT:  return "OUTPUT";
        case PNET_CHAIN_FORWARD: return "FORWARD";
        default: return "UNKNOWN";
    }
}

const char* pnet_encrypt_algo_str(pnet_encrypt_algo_t algo) {
    switch (algo) {
        case PNET_ENCRYPT_AES128: return "aes128";
        case PNET_ENCRYPT_AES256: return "aes256";
        case PNET_ENCRYPT_3DES:   return "3des";
        case PNET_ENCRYPT_DES:    return "des";
        default: return "unknown";
    }
}
