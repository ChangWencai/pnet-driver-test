/**
 * network_config.h - Linux Network Configuration (Real Implementation)
 *
 * Configures network interfaces, routes, and TCP tuning using
 * Linux system calls (ioctl, netlink).
 */

#ifndef NETWORK_CONFIG_H
#define NETWORK_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char     if_name[16];
    uint32_t ip_addr;
    uint32_t netmask;
    uint32_t gateway;
    uint8_t  mac_addr[6];
    bool     configured;
} net_if_config_t;

typedef struct {
    bool tcp_timestamps;
    bool tcp_window_scaling;
    bool tcp_sack;
    uint32_t tcp_rmem[3];  /* min, default, max */
    uint32_t tcp_wmem[3];
} tcp_tuning_config_t;

/* Interface configuration (uses ioctl on Linux) */
int  net_if_get_ip(const char *if_name, uint32_t *ip);
int  net_if_get_mac(const char *if_name, uint8_t mac[6]);
int  net_if_set_ip(const char *if_name, uint32_t ip);
int  net_if_set_netmask(const char *if_name, uint32_t mask);
int  net_if_set_up(const char *if_name);
int  net_if_set_down(const char *if_name);
int  net_if_get_config(const char *if_name, net_if_config_t *config);

/* TCP/IP tuning (writes to /proc/sys on Linux) */
int  net_tcp_tuning_apply(const tcp_tuning_config_t *tuning);
int  net_tcp_tuning_read_current(tcp_tuning_config_t *tuning);

/* Utility */
uint32_t net_ip_from_string(const char *str);
void net_ip_to_string(uint32_t ip, char *buf, size_t buf_size);
bool net_ip_is_valid(uint32_t ip);

#endif /* NETWORK_CONFIG_H */
