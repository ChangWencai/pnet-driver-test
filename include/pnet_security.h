/**
 * pnet_security.h - Profinet Security Mechanisms
 *
 * Based on document chapter 5: Security considerations and hardening
 * Provides ACL, IPSec, firewall, and security update management interfaces.
 */

#ifndef PNET_SECURITY_H
#define PNET_SECURITY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Security constants */
#define PNET_MAX_ACL_RULES        64
#define PNET_MAX_IPSEC_TUNNELS    8
#define PNET_MAX_FIREWALL_RULES   128
#define PNET_MAX_CERT_SIZE        4096
#define PNET_MAX_KEY_SIZE         256

/* ACL action types */
typedef enum {
    PNET_ACL_ACCEPT = 0,
    PNET_ACL_DROP,
    PNET_ACL_REJECT,
    PNET_ACL_LOG
} pnet_acl_action_t;

/* Firewall chain types */
typedef enum {
    PNET_CHAIN_INPUT = 0,
    PNET_CHAIN_OUTPUT,
    PNET_CHAIN_FORWARD
} pnet_chain_t;

/* Protocol types for rules */
typedef enum {
    PNET_PROTO_TCP = 6,
    PNET_PROTO_UDP = 17,
    PNET_PROTO_ICMP = 1,
    PNET_PROTO_ALL = 0
} pnet_proto_t;

/* IPSec encryption algorithms */
typedef enum {
    PNET_ENCRYPT_AES128 = 0,
    PNET_ENCRYPT_AES256,
    PNET_ENCRYPT_3DES,
    PNET_ENCRYPT_DES
} pnet_encrypt_algo_t;

/* IPSec authentication algorithms */
typedef enum {
    PNET_AUTH_SHA1 = 0,
    PNET_AUTH_SHA256,
    PNET_AUTH_MD5
} pnet_auth_algo_t;

/* IPSec mode */
typedef enum {
    PNET_IPSEC_TRANSPORT = 0,
    PNET_IPSEC_TUNNEL
} pnet_ipsec_mode_t;

/* ACL rule */
typedef struct {
    uint32_t         src_ip;
    uint32_t         src_mask;
    uint32_t         dst_ip;
    uint32_t         dst_mask;
    uint16_t         src_port;
    uint16_t         dst_port;
    pnet_proto_t     protocol;
    pnet_acl_action_t action;
    int              priority;
    bool             active;
    char             description[128];
} pnet_acl_rule_t;

/* Firewall rule */
typedef struct {
    pnet_chain_t     chain;
    uint32_t         src_ip;
    uint32_t         src_mask;
    uint32_t         dst_ip;
    uint32_t         dst_mask;
    uint16_t         port;
    pnet_proto_t     protocol;
    pnet_acl_action_t action;
    int              rule_number;
    bool             active;
    uint64_t         hit_count;
} pnet_fw_rule_t;

/* IPSec tunnel configuration */
typedef struct {
    char               name[64];
    uint32_t           local_ip;
    uint32_t           remote_ip;
    uint32_t           local_subnet;
    uint32_t           remote_subnet;
    pnet_ipsec_mode_t  mode;
    pnet_encrypt_algo_t encrypt_algo;
    pnet_auth_algo_t   auth_algo;
    uint8_t            psk[PNET_MAX_KEY_SIZE];
    size_t             psk_len;
    bool               auto_start;
    bool               active;
} pnet_ipsec_tunnel_t;

/* Security audit result */
typedef struct {
    int  total_rules;
    int  active_rules;
    int  vulnerabilities_found;
    int  warnings;
    bool ipsec_enabled;
    bool firewall_enabled;
    bool ids_enabled;
    char report[1024];
} pnet_security_audit_t;

/* ACL management */
int  pnet_acl_init(void);
int  pnet_acl_add_rule(pnet_acl_rule_t *rule);
int  pnet_acl_remove_rule(int priority);
int  pnet_acl_flush(void);
int  pnet_acl_check(uint32_t src_ip, uint32_t dst_ip, uint16_t port, pnet_proto_t proto);
int  pnet_acl_list(char *buffer, size_t buf_len);

/* Firewall management */
int  pnet_firewall_init(void);
int  pnet_firewall_add_rule(pnet_fw_rule_t *rule);
int  pnet_firewall_remove_rule(int rule_number);
int  pnet_firewall_flush(void);
int  pnet_firewall_generate_iptables(char *buffer, size_t buf_len);
int  pnet_firewall_apply(const pnet_fw_rule_t *rules, int count);

/* IPSec management */
int  pnet_ipsec_create_tunnel(pnet_ipsec_tunnel_t *tunnel);
int  pnet_ipsec_destroy_tunnel(const char *name);
int  pnet_ipsec_start_tunnel(const char *name);
int  pnet_ipsec_stop_tunnel(const char *name);
int  pnet_ipsec_generate_config(const pnet_ipsec_tunnel_t *tunnel, char *buffer, size_t buf_len);
bool pnet_ipsec_is_tunnel_active(const char *name);

/* Security audit */
int  pnet_security_audit(pnet_security_audit_t *result);
int  pnet_security_check_patches(const char *package_name, char *buffer, size_t buf_len);
int  pnet_security_generate_report(const pnet_security_audit_t *audit, char *buffer, size_t buf_len);

/* Utility functions */
const char* pnet_acl_action_str(pnet_acl_action_t action);
const char* pnet_chain_str(pnet_chain_t chain);
const char* pnet_encrypt_algo_str(pnet_encrypt_algo_t algo);

#endif /* PNET_SECURITY_H */
