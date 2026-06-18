/**
 * app_config.h - Application Configuration (TOML-based)
 *
 * Loads and parses the application configuration file.
 */

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "io_device.h"
#include "network_config.h"
#include "security_policy.h"
#include "rt_setup.h"

#define CONFIG_MAX_PATH 256

typedef struct {
    /* IO Device settings */
    io_device_cfg_t device;

    /* Network settings */
    net_if_config_t network;
    tcp_tuning_config_t tcp_tuning;

    /* Security settings */
    sec_policy_t security;
    bool security_enabled;

    /* Real-time settings */
    rt_config_t realtime;
    bool realtime_enabled;

    /* Logging */
    int  log_level;       /* 0=ERROR, 1=WARN, 2=INFO, 3=DEBUG */
    char log_file[CONFIG_MAX_PATH];

    /* Paths */
    char config_file[CONFIG_MAX_PATH];
} app_config_t;

/* Load config from file (simple key=value format) */
int  app_config_load(app_config_t *cfg, const char *path);

/* Save config to file */
int  app_config_save(const app_config_t *cfg, const char *path);

/* Set default values */
void app_config_default(app_config_t *cfg);

/* Validate configuration */
int  app_config_validate(const app_config_t *cfg);

#endif /* APP_CONFIG_H */
