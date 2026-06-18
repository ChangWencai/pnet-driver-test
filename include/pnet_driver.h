/**
 * pnet_driver.h - p-net Driver Interface Layer
 *
 * Based on document chapters 2.3 and 3: Driver interface and installation
 * Provides the top-level driver management interface including initialization,
 * service management, and deployment verification.
 */

#ifndef PNET_DRIVER_H
#define PNET_DRIVER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "pnet_device.h"
#include "pnet_protocol.h"
#include "pnet_network.h"
#include "pnet_security.h"

/* Driver version info */
#define PNET_DRIVER_VERSION_MAJOR 0
#define PNET_DRIVER_VERSION_MINOR 1
#define PNET_DRIVER_VERSION_PATCH 0
#define PNET_DRIVER_VERSION_STRING "0.1.0"

/* Service states */
typedef enum {
    PNET_SERVICE_STOPPED = 0,
    PNET_SERVICE_STARTING,
    PNET_SERVICE_RUNNING,
    PNET_SERVICE_STOPPING,
    PNET_SERVICE_FAILED
} pnet_service_state_t;

/* Driver configuration */
typedef struct {
    char  config_path[256];
    char  log_path[256];
    int   log_level;  /* 0=ERROR, 1=WARN, 2=INFO, 3=DEBUG */
    bool  enable_security;
    bool  enable_monitoring;
    bool  enable_redundancy;
    int   max_devices;
    int   max_connections;
    pnet_network_config_t network;
} pnet_driver_config_t;

/* Driver instance */
typedef struct {
    pnet_driver_config_t config;
    pnet_service_state_t service_state;
    pnet_device_t        devices[PNET_MAX_DEVICES];
    int                  device_count;
    pnet_protocol_stats_t stats;
    bool                 initialized;
    uint64_t             uptime_seconds;
} pnet_driver_t;

/* Installation check result */
typedef struct {
    bool system_compatible;
    bool dependencies_met;
    bool source_available;
    bool compiled;
    bool installed;
    bool service_configured;
    char kernel_version[64];
    char os_name[128];
    char error_message[256];
    int  missing_deps_count;
    char missing_deps[16][64];
} pnet_install_check_t;

/* Deployment scenario for chapter 6 */
typedef struct {
    char  scenario_name[64];
    char  description[256];
    int   device_count;
    pnet_topology_t topology;
    bool  use_redundancy;
    bool  use_irt;
    int   expected_cycle_time_us;
    pnet_device_info_t devices[PNET_MAX_DEVICES];
    pnet_network_config_t network;
} pnet_deployment_t;

/* Driver lifecycle */
int  pnet_driver_init(pnet_driver_t *driver, const pnet_driver_config_t *config);
int  pnet_driver_start(pnet_driver_t *driver);
int  pnet_driver_stop(pnet_driver_t *driver);
int  pnet_driver_shutdown(pnet_driver_t *driver);
pnet_service_state_t pnet_driver_get_state(const pnet_driver_t *driver);
const char* pnet_service_state_str(pnet_service_state_t state);

/* Device management through driver */
int  pnet_driver_add_device(pnet_driver_t *driver, const pnet_device_info_t *info);
int  pnet_driver_remove_device(pnet_driver_t *driver, int device_index);
int  pnet_driver_get_device(pnet_driver_t *driver, int index, pnet_device_t **dev);
int  pnet_driver_list_devices(const pnet_driver_t *driver, char *buffer, size_t buf_len);

/* Installation verification (chapter 3) */
int  pnet_install_check(pnet_install_check_t *result);
int  pnet_install_check_deps(pnet_install_check_t *result);
int  pnet_install_verify_service(pnet_install_check_t *result);
int  pnet_install_generate_report(const pnet_install_check_t *check, char *buffer, size_t buf_len);

/* Deployment management (chapter 6) */
int  pnet_deployment_create(pnet_deployment_t *deploy, const char *name);
int  pnet_deployment_validate(const pnet_deployment_t *deploy);
int  pnet_deployment_simulate(pnet_deployment_t *deploy);
int  pnet_deployment_generate_report(const pnet_deployment_t *deploy, char *buffer, size_t buf_len);

/* Configuration management */
int  pnet_config_load(pnet_driver_config_t *config, const char *path);
int  pnet_config_save(const pnet_driver_config_t *config, const char *path);
int  pnet_config_default(pnet_driver_config_t *config);

#endif /* PNET_DRIVER_H */
