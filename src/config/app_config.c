/**
 * app_config.c - Application Configuration Implementation
 *
 * Loads, saves, and validates application configuration in simple
 * key=value format. Supports IO device, network, security, and
 * real-time settings.
 */

#include "app_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ------------------------------------------------------------------ */
/*  Helpers                                                           */
/* ------------------------------------------------------------------ */

/**
 * Strip leading/trailing whitespace in-place and return pointer.
 */
static char *strip(char *s)
{
    while (*s && isspace((unsigned char)*s))
        s++;

    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end))
        *end-- = '\0';

    return s;
}

/**
 * Parse a "dotted-quad" IPv4 string into a uint32_t in network byte
 * order (big-endian).  Returns 0 on failure.
 */
static uint32_t parse_ip(const char *str)
{
    unsigned a, b, c, d;
    if (sscanf(str, "%u.%u.%u.%u", &a, &b, &c, &d) != 4)
        return 0;
    if (a > 255 || b > 255 || c > 255 || d > 255)
        return 0;
    /* Network byte order: first octet in highest byte */
    return (uint32_t)((a << 24) | (b << 16) | (c << 8) | d);
}

/**
 * Format a uint32_t IP (network byte order) into dotted-quad string.
 */
static void format_ip(uint32_t ip, char *buf, size_t len)
{
    snprintf(buf, len, "%u.%u.%u.%u",
             (ip >> 24) & 0xFF,
             (ip >> 16) & 0xFF,
             (ip >> 8)  & 0xFF,
              ip        & 0xFF);
}

/**
 * Apply a single key=value pair to the configuration struct.
 */
static void apply_kv(app_config_t *cfg, const char *key, const char *val)
{
    /* ---- IO Device ---- */
    if (strcmp(key, "product_name") == 0) {
        strncpy(cfg->device.product_name, val, sizeof(cfg->device.product_name) - 1);
    } else if (strcmp(key, "station_name") == 0) {
        strncpy(cfg->device.station_name, val, sizeof(cfg->device.station_name) - 1);
    } else if (strcmp(key, "interface") == 0) {
        strncpy(cfg->device.interface_name, val, sizeof(cfg->device.interface_name) - 1);
        /* Keep network interface in sync */
        strncpy(cfg->network.if_name, val, sizeof(cfg->network.if_name) - 1);
    } else if (strcmp(key, "ip_addr") == 0) {
        uint32_t ip = parse_ip(val);
        cfg->device.ip_addr = ip;
        cfg->network.ip_addr = ip;
    } else if (strcmp(key, "netmask") == 0) {
        uint32_t mask = parse_ip(val);
        cfg->device.netmask = mask;
        cfg->network.netmask = mask;
    } else if (strcmp(key, "gateway") == 0) {
        uint32_t gw = parse_ip(val);
        cfg->device.gateway = gw;
        cfg->network.gateway = gw;
    } else if (strcmp(key, "vendor_id") == 0) {
        cfg->device.vendor_id = (uint16_t)strtoul(val, NULL, 0);
    } else if (strcmp(key, "device_id") == 0) {
        cfg->device.device_id = (uint16_t)strtoul(val, NULL, 0);
    } else if (strcmp(key, "tick_us") == 0) {
        cfg->device.tick_us = (uint32_t)strtoul(val, NULL, 0);
    } else if (strcmp(key, "send_hello") == 0) {
        cfg->device.send_hello = (strcmp(val, "true") == 0 || strcmp(val, "1") == 0);
    } else if (strcmp(key, "file_directory") == 0) {
        strncpy(cfg->device.file_directory, val, sizeof(cfg->device.file_directory) - 1);

    /* ---- Logging ---- */
    } else if (strcmp(key, "log_level") == 0) {
        cfg->log_level = atoi(val);
    } else if (strcmp(key, "log_file") == 0) {
        strncpy(cfg->log_file, val, sizeof(cfg->log_file) - 1);

    /* ---- Security ---- */
    } else if (strcmp(key, "security_enabled") == 0) {
        cfg->security_enabled = (strcmp(val, "true") == 0 || strcmp(val, "1") == 0);

    /* ---- Real-time ---- */
    } else if (strcmp(key, "realtime_enabled") == 0) {
        cfg->realtime_enabled = (strcmp(val, "true") == 0 || strcmp(val, "1") == 0);
    } else if (strcmp(key, "rt_priority") == 0) {
        cfg->realtime.rt_priority = atoi(val);
    } else if (strcmp(key, "rt_scheduling") == 0) {
        cfg->realtime.use_rt_scheduling = (strcmp(val, "true") == 0 || strcmp(val, "1") == 0);
    } else if (strcmp(key, "lock_memory") == 0) {
        cfg->realtime.lock_memory = (strcmp(val, "true") == 0 || strcmp(val, "1") == 0);
    } else if (strcmp(key, "isolate_cpu") == 0) {
        cfg->realtime.isolate_cpu = atoi(val);
    }
}

/* ------------------------------------------------------------------ */
/*  Public API                                                        */
/* ------------------------------------------------------------------ */

void app_config_default(app_config_t *cfg)
{
    if (!cfg)
        return;

    memset(cfg, 0, sizeof(app_config_t));

    /* IO Device defaults */
    strncpy(cfg->device.product_name, "p-net IO Device",
            sizeof(cfg->device.product_name) - 1);
    strncpy(cfg->device.station_name, "iodevice1",
            sizeof(cfg->device.station_name) - 1);
    strncpy(cfg->device.interface_name, "eth0",
            sizeof(cfg->device.interface_name) - 1);
    cfg->device.ip_addr   = parse_ip("192.168.0.100");
    cfg->device.netmask   = parse_ip("255.255.255.0");
    cfg->device.gateway   = parse_ip("192.168.0.1");
    cfg->device.vendor_id = 0x0001;
    cfg->device.device_id = 0x0001;
    cfg->device.tick_us   = 1000;
    cfg->device.send_hello = true;
    strncpy(cfg->device.file_directory, "/tmp/pnet",
            sizeof(cfg->device.file_directory) - 1);

    /* Network defaults (mirror device interface) */
    strncpy(cfg->network.if_name, "eth0", sizeof(cfg->network.if_name) - 1);
    cfg->network.ip_addr = cfg->device.ip_addr;
    cfg->network.netmask = cfg->device.netmask;
    cfg->network.gateway = cfg->device.gateway;
    cfg->network.configured = true;

    /* TCP tuning defaults (disabled) */
    cfg->tcp_tuning.tcp_timestamps    = false;
    cfg->tcp_tuning.tcp_window_scaling = false;
    cfg->tcp_tuning.tcp_sack          = false;

    /* Security defaults */
    cfg->security_enabled = true;
    sec_policy_init(&cfg->security);

    /* Real-time defaults */
    cfg->realtime_enabled = true;
    rt_config_default(&cfg->realtime);

    /* Logging */
    cfg->log_level = 2;  /* INFO */
    strncpy(cfg->log_file, "/var/log/pnet-manager.log",
            sizeof(cfg->log_file) - 1);

    /* Config file path */
    strncpy(cfg->config_file, "/etc/pnet-manager/config.conf",
            sizeof(cfg->config_file) - 1);
}

int app_config_load(app_config_t *cfg, const char *path)
{
    if (!cfg || !path)
        return -1;

    FILE *fp = fopen(path, "r");
    if (!fp)
        return -1;

    /* Start from defaults so missing keys keep sane values */
    app_config_default(cfg);
    strncpy(cfg->config_file, path, sizeof(cfg->config_file) - 1);

    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        /* Strip newline */
        line[strcspn(line, "\r\n")] = '\0';

        char *trimmed = strip(line);

        /* Skip empty lines and comments */
        if (*trimmed == '\0' || *trimmed == '#' || *trimmed == ';')
            continue;

        /* Split on first '=' */
        char *eq = strchr(trimmed, '=');
        if (!eq)
            continue;

        *eq = '\0';
        char *key = strip(trimmed);
        char *val = strip(eq + 1);

        if (*key && *val)
            apply_kv(cfg, key, val);
    }

    fclose(fp);
    return 0;
}

int app_config_save(const app_config_t *cfg, const char *path)
{
    if (!cfg || !path)
        return -1;

    FILE *fp = fopen(path, "w");
    if (!fp)
        return -1;

    char ip_buf[16];

    fprintf(fp, "# p-net IO Device Manager Configuration\n");
    fprintf(fp, "# Auto-generated - edit with care\n\n");

    /* IO Device */
    fprintf(fp, "# IO Device\n");
    fprintf(fp, "product_name=%s\n", cfg->device.product_name);
    fprintf(fp, "station_name=%s\n", cfg->device.station_name);
    fprintf(fp, "interface=%s\n",    cfg->device.interface_name);

    format_ip(cfg->device.ip_addr, ip_buf, sizeof(ip_buf));
    fprintf(fp, "ip_addr=%s\n", ip_buf);

    format_ip(cfg->device.netmask, ip_buf, sizeof(ip_buf));
    fprintf(fp, "netmask=%s\n", ip_buf);

    format_ip(cfg->device.gateway, ip_buf, sizeof(ip_buf));
    fprintf(fp, "gateway=%s\n", ip_buf);

    fprintf(fp, "vendor_id=0x%04X\n", cfg->device.vendor_id);
    fprintf(fp, "device_id=0x%04X\n", cfg->device.device_id);
    fprintf(fp, "tick_us=%u\n",       cfg->device.tick_us);
    fprintf(fp, "send_hello=%s\n",    cfg->device.send_hello ? "true" : "false");
    fprintf(fp, "file_directory=%s\n", cfg->device.file_directory);

    /* Logging */
    fprintf(fp, "\n# Logging\n");
    fprintf(fp, "log_level=%d\n",  cfg->log_level);
    fprintf(fp, "log_file=%s\n",   cfg->log_file);

    /* Security */
    fprintf(fp, "\n# Security\n");
    fprintf(fp, "security_enabled=%s\n",
            cfg->security_enabled ? "true" : "false");

    /* Real-time */
    fprintf(fp, "\n# Real-time\n");
    fprintf(fp, "realtime_enabled=%s\n",
            cfg->realtime_enabled ? "true" : "false");
    fprintf(fp, "rt_priority=%d\n",    cfg->realtime.rt_priority);
    fprintf(fp, "rt_scheduling=%s\n",  cfg->realtime.use_rt_scheduling ? "true" : "false");
    fprintf(fp, "lock_memory=%s\n",    cfg->realtime.lock_memory ? "true" : "false");
    fprintf(fp, "isolate_cpu=%d\n",    cfg->realtime.isolate_cpu);

    fclose(fp);
    return 0;
}

int app_config_validate(const app_config_t *cfg)
{
    if (!cfg)
        return -1;

    /* Interface name must be non-empty */
    if (cfg->device.interface_name[0] == '\0') {
        fprintf(stderr, "config: interface name is empty\n");
        return -1;
    }

    /* IP address must be valid (non-zero) */
    if (cfg->device.ip_addr == 0) {
        fprintf(stderr, "config: ip_addr is not set or invalid\n");
        return -1;
    }

    /* Reject all-zeros and broadcast */
    if (cfg->device.ip_addr == 0x00000000 ||
        cfg->device.ip_addr == 0xFFFFFFFF) {
        fprintf(stderr, "config: ip_addr is reserved (0.0.0.0 or 255.255.255.255)\n");
        return -1;
    }

    /* Tick interval must be positive */
    if (cfg->device.tick_us == 0) {
        fprintf(stderr, "config: tick_us must be > 0\n");
        return -1;
    }

    /* Vendor and device IDs must be set */
    if (cfg->device.vendor_id == 0) {
        fprintf(stderr, "config: vendor_id is not set\n");
        return -1;
    }
    if (cfg->device.device_id == 0) {
        fprintf(stderr, "config: device_id is not set\n");
        return -1;
    }

    /* Log level sanity */
    if (cfg->log_level < 0 || cfg->log_level > 3) {
        fprintf(stderr, "config: log_level must be 0-3\n");
        return -1;
    }

    /* RT priority range (if RT is enabled) */
    if (cfg->realtime_enabled) {
        if (cfg->realtime.rt_priority < 1 || cfg->realtime.rt_priority > 99) {
            fprintf(stderr, "config: rt_priority must be 1-99 when realtime is enabled\n");
            return -1;
        }
    }

    return 0;
}
