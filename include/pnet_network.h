/**
 * pnet_network.h - Profinet Network Configuration and Optimization
 *
 * Based on document chapter 4: Network configuration and performance optimization
 * Provides network parameter configuration, real-time performance monitoring,
 * and optimization interfaces.
 */

#ifndef PNET_NETWORK_H
#define PNET_NETWORK_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Network constants */
#define PNET_DEFAULT_PORT          102
#define PNET_DCP_PORT              102
#define PNET_MAX_INTERFACES        8
#define PNET_MAX_ROUTES            32
#define PNET_INTERFACE_NAME_LEN    32

/* Network interface flags */
#define PNET_IF_UP                 0x01
#define PNET_IF_RUNNING            0x02
#define PNET_IF_PROMISC            0x04
#define PNET_IF_MULTICAST          0x08

/* Network interface configuration */
typedef struct {
    char     name[PNET_INTERFACE_NAME_LEN];
    uint32_t ip_addr;
    uint32_t subnet_mask;
    uint32_t gateway;
    uint32_t broadcast;
    uint8_t  mac_addr[6];
    uint16_t mtu;
    uint32_t flags;
    bool     configured;
} pnet_interface_t;

/* Static route entry */
typedef struct {
    uint32_t destination;
    uint32_t netmask;
    uint32_t gateway;
    char     interface[PNET_INTERFACE_NAME_LEN];
    int      metric;
    bool     active;
} pnet_route_entry_t;

/* Network topology types */
typedef enum {
    PNET_TOPOLOGY_STAR = 0,
    PNET_TOPOLOGY_RING,
    PNET_TOPOLOGY_LINE,
    PNET_TOPOLOGY_BUS,
    PNET_TOPOLOGY_TREE
} pnet_topology_t;

/* QoS priority levels */
typedef enum {
    PNET_QOS_BEST_EFFORT = 0,
    PNET_QOS_BACKGROUND,
    PNET_QOS_STANDARD,
    PNET_QOS_LOW_LATENCY,
    PNET_QOS_VIDEO,
    PNET_QOS_VOICE,
    PNET_QOS_NETWORK_CONTROL,
    PNET_QOS_CRITICAL
} pnet_qos_priority_t;

/* TCP/IP tuning parameters */
typedef struct {
    bool     tcp_timestamps;
    bool     tcp_window_scaling;
    bool     tcp_sack;
    uint32_t tcp_rmem_min;
    uint32_t tcp_rmem_default;
    uint32_t tcp_rmem_max;
    uint32_t tcp_wmem_min;
    uint32_t tcp_wmem_default;
    uint32_t tcp_wmem_max;
} pnet_tcp_tuning_t;

/* Network performance metrics */
typedef struct {
    double   bandwidth_mbps;
    double   latency_us;
    double   jitter_us;
    double   packet_loss_rate;
    uint64_t packets_sent;
    uint64_t packets_received;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint64_t errors_rx;
    uint64_t errors_tx;
    uint64_t dropped_rx;
    uint64_t dropped_tx;
} pnet_network_stats_t;

/* Network configuration profile (full deployment config) */
typedef struct {
    pnet_interface_t  interfaces[PNET_MAX_INTERFACES];
    int               interface_count;
    pnet_route_entry_t routes[PNET_MAX_ROUTES];
    int               route_count;
    pnet_topology_t   topology;
    pnet_tcp_tuning_t tcp_tuning;
    bool              redundancy_enabled;
    bool              vlan_enabled;
    uint16_t          vlan_id;
} pnet_network_config_t;

/* Interface management */
int  pnet_interface_init(pnet_interface_t *iface, const char *name);
int  pnet_interface_configure(pnet_interface_t *iface, uint32_t ip, uint32_t mask, uint32_t gw);
int  pnet_interface_set_mac(pnet_interface_t *iface, const uint8_t mac[6]);
int  pnet_interface_up(pnet_interface_t *iface);
int  pnet_interface_down(pnet_interface_t *iface);
int  pnet_interface_get_stats(const pnet_interface_t *iface, pnet_network_stats_t *stats);
bool pnet_interface_is_up(const pnet_interface_t *iface);

/* Route management */
int  pnet_route_add(pnet_network_config_t *config, const pnet_route_entry_t *route);
int  pnet_route_remove(pnet_network_config_t *config, uint32_t destination, uint32_t netmask);
int  pnet_route_flush(pnet_network_config_t *config);
const pnet_route_entry_t* pnet_route_lookup(const pnet_network_config_t *config, uint32_t dest_ip);

/* Network configuration */
int  pnet_network_init(pnet_network_config_t *config);
int  pnet_network_apply(pnet_network_config_t *config);
int  pnet_network_validate(const pnet_network_config_t *config);
int  pnet_network_generate_script(const pnet_network_config_t *config, char *buffer, size_t buf_len);

/* TCP/IP tuning */
int  pnet_tcp_tuning_init(pnet_tcp_tuning_t *tuning);
int  pnet_tcp_tuning_apply(const pnet_tcp_tuning_t *tuning);
int  pnet_tcp_tuning_generate_sysctl(const pnet_tcp_tuning_t *tuning, char *buffer, size_t buf_len);

/* Performance monitoring */
int  pnet_network_get_stats(const char *interface_name, pnet_network_stats_t *stats);
int  pnet_network_monitor_start(const char *interface_name);
int  pnet_network_monitor_stop(void);
double pnet_network_calculate_jitter(const double *latencies, int count);

/* Utility functions */
uint32_t pnet_ip_from_str(const char *ip_str);
void     pnet_ip_to_str(uint32_t ip, char *buffer);
bool     pnet_ip_is_private(uint32_t ip);
bool     pnet_ip_in_subnet(uint32_t ip, uint32_t subnet, uint32_t mask);

#endif /* PNET_NETWORK_H */
