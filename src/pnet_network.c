/**
 * pnet_network.c - Network Configuration and Optimization Implementation
 *
 * Implements network parameter configuration, route management,
 * TCP/IP tuning, and performance monitoring for Profinet networks.
 */

#include "pnet_network.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <arpa/inet.h>

int pnet_interface_init(pnet_interface_t *iface, const char *name) {
    if (!iface || !name) return -1;

    memset(iface, 0, sizeof(pnet_interface_t));
    strncpy(iface->name, name, PNET_INTERFACE_NAME_LEN - 1);
    iface->mtu = 1500;  /* Default MTU */
    iface->configured = false;
    return 0;
}

int pnet_interface_configure(pnet_interface_t *iface, uint32_t ip, uint32_t mask, uint32_t gw) {
    if (!iface) return -1;

    iface->ip_addr = ip;
    iface->subnet_mask = mask;
    iface->gateway = gw;
    iface->broadcast = (ip & mask) | (~mask);
    iface->configured = true;
    return 0;
}

int pnet_interface_set_mac(pnet_interface_t *iface, const uint8_t mac[6]) {
    if (!iface || !mac) return -1;
    memcpy(iface->mac_addr, mac, 6);
    return 0;
}

int pnet_interface_up(pnet_interface_t *iface) {
    if (!iface) return -1;
    iface->flags |= (PNET_IF_UP | PNET_IF_RUNNING);
    return 0;
}

int pnet_interface_down(pnet_interface_t *iface) {
    if (!iface) return -1;
    iface->flags &= ~(PNET_IF_UP | PNET_IF_RUNNING);
    return 0;
}

int pnet_interface_get_stats(const pnet_interface_t *iface, pnet_network_stats_t *stats) {
    if (!iface || !stats) return -1;

    /* Simulation: return dummy stats */
    memset(stats, 0, sizeof(pnet_network_stats_t));
    stats->bandwidth_mbps = 100.0;
    stats->latency_us = 50.0;
    stats->jitter_us = 2.5;
    stats->packet_loss_rate = 0.0001;
    return 0;
}

bool pnet_interface_is_up(const pnet_interface_t *iface) {
    if (!iface) return false;
    return (iface->flags & PNET_IF_UP) != 0;
}

/* Route management */
int pnet_route_add(pnet_network_config_t *config, const pnet_route_entry_t *route) {
    if (!config || !route) return -1;
    if (config->route_count >= PNET_MAX_ROUTES) return -1;

    memcpy(&config->routes[config->route_count], route, sizeof(pnet_route_entry_t));
    config->routes[config->route_count].active = true;
    config->route_count++;
    return 0;
}

int pnet_route_remove(pnet_network_config_t *config, uint32_t destination, uint32_t netmask) {
    if (!config) return -1;

    for (int i = 0; i < config->route_count; i++) {
        if (config->routes[i].destination == destination &&
            config->routes[i].netmask == netmask) {
            /* Shift remaining routes */
            for (int j = i; j < config->route_count - 1; j++) {
                memcpy(&config->routes[j], &config->routes[j + 1], sizeof(pnet_route_entry_t));
            }
            config->route_count--;
            return 0;
        }
    }
    return -1; /* Route not found */
}

int pnet_route_flush(pnet_network_config_t *config) {
    if (!config) return -1;
    config->route_count = 0;
    memset(config->routes, 0, sizeof(config->routes));
    return 0;
}

const pnet_route_entry_t* pnet_route_lookup(const pnet_network_config_t *config, uint32_t dest_ip) {
    if (!config) return NULL;

    /* Simple longest prefix match */
    const pnet_route_entry_t *best = NULL;
    uint32_t best_mask = 0;

    for (int i = 0; i < config->route_count; i++) {
        if (!config->routes[i].active) continue;
        if ((dest_ip & config->routes[i].netmask) ==
            (config->routes[i].destination & config->routes[i].netmask)) {
            if (config->routes[i].netmask >= best_mask) {
                best = &config->routes[i];
                best_mask = config->routes[i].netmask;
            }
        }
    }
    return best;
}

/* Network configuration */
int pnet_network_init(pnet_network_config_t *config) {
    if (!config) return -1;
    memset(config, 0, sizeof(pnet_network_config_t));
    config->topology = PNET_TOPOLOGY_STAR;
    return 0;
}

int pnet_network_apply(pnet_network_config_t *config) {
    if (!config) return -1;

    /* Apply interface configurations */
    for (int i = 0; i < config->interface_count; i++) {
        if (config->interfaces[i].configured) {
            pnet_interface_up(&config->interfaces[i]);
        }
    }

    return 0;
}

int pnet_network_validate(const pnet_network_config_t *config) {
    if (!config) return -1;

    /* Check that at least one interface is configured */
    bool has_interface = false;
    for (int i = 0; i < config->interface_count; i++) {
        if (config->interfaces[i].configured) {
            has_interface = true;
            break;
        }
    }

    return has_interface ? 0 : -1;
}

int pnet_network_generate_script(const pnet_network_config_t *config, char *buffer, size_t buf_len) {
    if (!config || !buffer || buf_len == 0) return -1;

    int offset = 0;
    offset += snprintf(buffer + offset, buf_len - (size_t)offset,
                       "#!/bin/bash\n# Profinet Network Configuration Script\n# Auto-generated\n\n");

    for (int i = 0; i < config->interface_count; i++) {
        const pnet_interface_t *iface = &config->interfaces[i];
        if (!iface->configured) continue;

        char ip_str[16], mask_str[16], gw_str[16];
        pnet_ip_to_str(iface->ip_addr, ip_str);
        pnet_ip_to_str(iface->subnet_mask, mask_str);
        pnet_ip_to_str(iface->gateway, gw_str);

        offset += snprintf(buffer + offset, buf_len - (size_t)offset,
                           "sudo ip addr add %s/24 dev %s\n", ip_str, iface->name);
        offset += snprintf(buffer + offset, buf_len - (size_t)offset,
                           "sudo ip link set %s up\n", iface->name);
    }

    for (int i = 0; i < config->route_count; i++) {
        const pnet_route_entry_t *route = &config->routes[i];
        if (!route->active) continue;

        char dst_str[16], gw_str[16];
        pnet_ip_to_str(route->destination, dst_str);
        pnet_ip_to_str(route->gateway, gw_str);

        offset += snprintf(buffer + offset, buf_len - (size_t)offset,
                           "sudo ip route add %s via %s\n", dst_str, gw_str);
    }

    return offset;
}

/* TCP/IP tuning */
int pnet_tcp_tuning_init(pnet_tcp_tuning_t *tuning) {
    if (!tuning) return -1;

    /* Default values from document chapter 4 */
    tuning->tcp_timestamps = true;
    tuning->tcp_window_scaling = true;
    tuning->tcp_sack = true;
    tuning->tcp_rmem_min = 4096;
    tuning->tcp_rmem_default = 87380;
    tuning->tcp_rmem_max = 16777216;
    tuning->tcp_wmem_min = 4096;
    tuning->tcp_wmem_default = 65536;
    tuning->tcp_wmem_max = 16777216;
    return 0;
}

int pnet_tcp_tuning_apply(const pnet_tcp_tuning_t *tuning) {
    if (!tuning) return -1;
    /* Simulation: in real deployment, this would modify /etc/sysctl.conf */
    return 0;
}

int pnet_tcp_tuning_generate_sysctl(const pnet_tcp_tuning_t *tuning, char *buffer, size_t buf_len) {
    if (!tuning || !buffer || buf_len == 0) return -1;

    int offset = 0;
    offset += snprintf(buffer + offset, buf_len - (size_t)offset,
                       "# Profinet TCP/IP Tuning - sysctl.conf\n\n");
    offset += snprintf(buffer + offset, buf_len - (size_t)offset,
                       "net.ipv4.tcp_timestamps = %d\n", tuning->tcp_timestamps ? 1 : 0);
    offset += snprintf(buffer + offset, buf_len - (size_t)offset,
                       "net.ipv4.tcp_window_scaling = %d\n", tuning->tcp_window_scaling ? 1 : 0);
    offset += snprintf(buffer + offset, buf_len - (size_t)offset,
                       "net.ipv4.tcp_sack = %d\n", tuning->tcp_sack ? 1 : 0);
    offset += snprintf(buffer + offset, buf_len - (size_t)offset,
                       "net.ipv4.tcp_rmem = %u %u %u\n",
                       tuning->tcp_rmem_min, tuning->tcp_rmem_default, tuning->tcp_rmem_max);
    offset += snprintf(buffer + offset, buf_len - (size_t)offset,
                       "net.ipv4.tcp_wmem = %u %u %u\n",
                       tuning->tcp_wmem_min, tuning->tcp_wmem_default, tuning->tcp_wmem_max);

    return offset;
}

/* Performance monitoring */
int pnet_network_get_stats(const char *interface_name, pnet_network_stats_t *stats) {
    if (!interface_name || !stats) return -1;

    /* Simulation: return dummy stats */
    memset(stats, 0, sizeof(pnet_network_stats_t));
    stats->bandwidth_mbps = 100.0;
    stats->latency_us = 45.0;
    stats->jitter_us = 1.5;
    stats->packet_loss_rate = 0.0;
    stats->packets_sent = 10000;
    stats->packets_received = 9999;
    return 0;
}

int pnet_network_monitor_start(const char *interface_name) {
    if (!interface_name) return -1;
    /* Simulation */
    return 0;
}

int pnet_network_monitor_stop(void) {
    return 0;
}

double pnet_network_calculate_jitter(const double *latencies, int count) {
    if (!latencies || count < 2) return 0.0;

    double sum = 0.0;
    for (int i = 0; i < count; i++) {
        sum += latencies[i];
    }
    double mean = sum / count;

    double variance = 0.0;
    for (int i = 0; i < count; i++) {
        double diff = latencies[i] - mean;
        variance += diff * diff;
    }
    variance /= count;

    return sqrt(variance);
}

/* Utility functions */
uint32_t pnet_ip_from_str(const char *ip_str) {
    if (!ip_str) return 0;
    struct in_addr addr;
    if (inet_pton(AF_INET, ip_str, &addr) != 1) return 0;
    return ntohl(addr.s_addr);
}

void pnet_ip_to_str(uint32_t ip, char *buffer) {
    if (!buffer) return;
    struct in_addr addr;
    addr.s_addr = htonl(ip);
    inet_ntop(AF_INET, &addr, buffer, 16);
}

bool pnet_ip_is_private(uint32_t ip) {
    /* Check RFC 1918 private address ranges */
    uint8_t first = (ip >> 24) & 0xFF;
    uint8_t second = (ip >> 16) & 0xFF;

    if (first == 10) return true;                           /* 10.0.0.0/8 */
    if (first == 172 && second >= 16 && second <= 31) return true; /* 172.16.0.0/12 */
    if (first == 192 && second == 168) return true;         /* 192.168.0.0/16 */
    return false;
}

bool pnet_ip_in_subnet(uint32_t ip, uint32_t subnet, uint32_t mask) {
    return (ip & mask) == (subnet & mask);
}
