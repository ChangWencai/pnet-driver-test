/**
 * pnet_driver.c - p-net Driver Interface Implementation
 *
 * Implements driver lifecycle management, device registration,
 * installation verification, and deployment management.
 */

#include "pnet_driver.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/utsname.h>

int pnet_driver_init(pnet_driver_t *driver, const pnet_driver_config_t *config) {
    if (!driver) return -1;

    memset(driver, 0, sizeof(pnet_driver_t));

    if (config) {
        memcpy(&driver->config, config, sizeof(pnet_driver_config_t));
    } else {
        pnet_config_default(&driver->config);
    }

    driver->service_state = PNET_SERVICE_STOPPED;
    driver->initialized = true;
    return 0;
}

int pnet_driver_start(pnet_driver_t *driver) {
    if (!driver || !driver->initialized) return -1;
    if (driver->service_state == PNET_SERVICE_RUNNING) return 0;

    driver->service_state = PNET_SERVICE_STARTING;

    /* Initialize protocol stack */
    int ret = pnet_protocol_init();
    if (ret != 0) {
        driver->service_state = PNET_SERVICE_FAILED;
        return ret;
    }

    /* Initialize security if enabled */
    if (driver->config.enable_security) {
        pnet_acl_init();
        pnet_firewall_init();
    }

    /* Apply network configuration */
    pnet_network_apply(&driver->config.network);

    driver->service_state = PNET_SERVICE_RUNNING;
    return 0;
}

int pnet_driver_stop(pnet_driver_t *driver) {
    if (!driver) return -1;
    if (driver->service_state != PNET_SERVICE_RUNNING) return 0;

    driver->service_state = PNET_SERVICE_STOPPING;

    /* Close all devices */
    for (int i = 0; i < driver->device_count; i++) {
        pnet_device_close(&driver->devices[i]);
    }

    /* Shutdown protocol stack */
    pnet_protocol_shutdown();

    driver->service_state = PNET_SERVICE_STOPPED;
    return 0;
}

int pnet_driver_shutdown(pnet_driver_t *driver) {
    if (!driver) return -1;

    pnet_driver_stop(driver);

    /* Cleanup all devices */
    for (int i = 0; i < driver->device_count; i++) {
        pnet_device_cleanup(&driver->devices[i]);
    }

    driver->initialized = false;
    return 0;
}

pnet_service_state_t pnet_driver_get_state(const pnet_driver_t *driver) {
    if (!driver) return PNET_SERVICE_STOPPED;
    return driver->service_state;
}

const char* pnet_service_state_str(pnet_service_state_t state) {
    switch (state) {
        case PNET_SERVICE_STOPPED:  return "STOPPED";
        case PNET_SERVICE_STARTING: return "STARTING";
        case PNET_SERVICE_RUNNING:  return "RUNNING";
        case PNET_SERVICE_STOPPING: return "STOPPING";
        case PNET_SERVICE_FAILED:   return "FAILED";
        default: return "UNKNOWN";
    }
}

int pnet_driver_add_device(pnet_driver_t *driver, const pnet_device_info_t *info) {
    if (!driver || !info) return -1;
    if (driver->device_count >= PNET_MAX_DEVICES) return -1;

    int idx = driver->device_count;
    pnet_device_init(&driver->devices[idx]);
    pnet_device_set_info(&driver->devices[idx], info);
    driver->device_count++;
    return idx;
}

int pnet_driver_remove_device(pnet_driver_t *driver, int device_index) {
    if (!driver) return -1;
    if (device_index < 0 || device_index >= driver->device_count) return -1;

    pnet_device_cleanup(&driver->devices[device_index]);

    /* Shift remaining devices */
    for (int i = device_index; i < driver->device_count - 1; i++) {
        memcpy(&driver->devices[i], &driver->devices[i + 1], sizeof(pnet_device_t));
    }
    driver->device_count--;
    return 0;
}

int pnet_driver_get_device(pnet_driver_t *driver, int index, pnet_device_t **dev) {
    if (!driver || !dev) return -1;
    if (index < 0 || index >= driver->device_count) return -1;

    *dev = &driver->devices[index];
    return 0;
}

int pnet_driver_list_devices(const pnet_driver_t *driver, char *buffer, size_t buf_len) {
    if (!driver || !buffer || buf_len == 0) return -1;

    int offset = 0;
    offset += snprintf(buffer + offset, buf_len - (size_t)offset,
                       "Registered devices: %d\n", driver->device_count);

    for (int i = 0; i < driver->device_count; i++) {
        const pnet_device_t *dev = &driver->devices[i];
        offset += snprintf(buffer + offset, buf_len - (size_t)offset,
                           "  [%d] %s (vendor=0x%04X device=0x%04X) state=%s\n",
                           i, dev->info.device_name,
                           dev->info.vendor_id, dev->info.device_id,
                           pnet_device_state_str(dev->state));
    }

    return offset;
}

/* Installation verification */
int pnet_install_check(pnet_install_check_t *result) {
    if (!result) return -1;

    memset(result, 0, sizeof(pnet_install_check_t));

    /* Check system info */
    struct utsname uname_data;
    if (uname(&uname_data) == 0) {
        strncpy(result->kernel_version, uname_data.release, sizeof(result->kernel_version) - 1);
        snprintf(result->os_name, sizeof(result->os_name), "%s %s",
                 uname_data.sysname, uname_data.machine);
        result->system_compatible = true;
    }

    return 0;
}

int pnet_install_check_deps(pnet_install_check_t *result) {
    if (!result) return -1;

    /* Check for required tools */
    const char *required_tools[] = {
        "gcc", "make", "git", "cmake"
    };
    int tool_count = 4;

    result->dependencies_met = true;
    result->missing_deps_count = 0;

    for (int i = 0; i < tool_count; i++) {
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "which %s > /dev/null 2>&1", required_tools[i]);
        if (system(cmd) != 0) {
            result->dependencies_met = false;
            if (result->missing_deps_count < 16) {
                strncpy(result->missing_deps[result->missing_deps_count],
                        required_tools[i], 63);
                result->missing_deps_count++;
            }
        }
    }

    return 0;
}

int pnet_install_verify_service(pnet_install_check_t *result) {
    if (!result) return -1;

    /* Check if p-net source is available (simulation) */
    result->source_available = true;  /* We're simulating this */
    result->compiled = false;         /* Not compiled yet in test mode */
    result->installed = false;
    result->service_configured = false;

    return 0;
}

int pnet_install_generate_report(const pnet_install_check_t *check, char *buffer, size_t buf_len) {
    if (!check || !buffer || buf_len == 0) return -1;

    int offset = 0;
    offset += snprintf(buffer + offset, buf_len - (size_t)offset,
                       "=== p-net Installation Check Report ===\n\n");
    offset += snprintf(buffer + offset, buf_len - (size_t)offset,
                       "System:          %s\n", check->os_name);
    offset += snprintf(buffer + offset, buf_len - (size_t)offset,
                       "Kernel:          %s\n", check->kernel_version);
    offset += snprintf(buffer + offset, buf_len - (size_t)offset,
                       "Compatible:      %s\n", check->system_compatible ? "Yes" : "No");
    offset += snprintf(buffer + offset, buf_len - (size_t)offset,
                       "Dependencies:    %s\n", check->dependencies_met ? "All met" : "Missing");
    offset += snprintf(buffer + offset, buf_len - (size_t)offset,
                       "Source:          %s\n", check->source_available ? "Available" : "Not found");
    offset += snprintf(buffer + offset, buf_len - (size_t)offset,
                       "Compiled:        %s\n", check->compiled ? "Yes" : "No");
    offset += snprintf(buffer + offset, buf_len - (size_t)offset,
                       "Installed:       %s\n", check->installed ? "Yes" : "No");

    if (check->missing_deps_count > 0) {
        offset += snprintf(buffer + offset, buf_len - (size_t)offset,
                           "\nMissing dependencies:\n");
        for (int i = 0; i < check->missing_deps_count; i++) {
            offset += snprintf(buffer + offset, buf_len - (size_t)offset,
                               "  - %s\n", check->missing_deps[i]);
        }
    }

    return offset;
}

/* Deployment management */
int pnet_deployment_create(pnet_deployment_t *deploy, const char *name) {
    if (!deploy || !name) return -1;

    memset(deploy, 0, sizeof(pnet_deployment_t));
    strncpy(deploy->scenario_name, name, sizeof(deploy->scenario_name) - 1);
    deploy->topology = PNET_TOPOLOGY_STAR;
    deploy->expected_cycle_time_us = 1000;
    return 0;
}

int pnet_deployment_validate(const pnet_deployment_t *deploy) {
    if (!deploy) return -1;
    if (deploy->device_count <= 0) return -1;
    if (deploy->expected_cycle_time_us <= 0) return -1;
    return 0;
}

int pnet_deployment_simulate(pnet_deployment_t *deploy) {
    if (!deploy) return -1;
    /* Simulation: mark as validated */
    return 0;
}

int pnet_deployment_generate_report(const pnet_deployment_t *deploy, char *buffer, size_t buf_len) {
    if (!deploy || !buffer || buf_len == 0) return -1;

    int offset = 0;
    offset += snprintf(buffer + offset, buf_len - (size_t)offset,
                       "=== Deployment Report: %s ===\n\n", deploy->scenario_name);
    offset += snprintf(buffer + offset, buf_len - (size_t)offset,
                       "Description:     %s\n", deploy->description);
    offset += snprintf(buffer + offset, buf_len - (size_t)offset,
                       "Devices:         %d\n", deploy->device_count);
    offset += snprintf(buffer + offset, buf_len - (size_t)offset,
                       "Redundancy:      %s\n", deploy->use_redundancy ? "Enabled" : "Disabled");
    offset += snprintf(buffer + offset, buf_len - (size_t)offset,
                       "IRT:             %s\n", deploy->use_irt ? "Enabled" : "Disabled");
    offset += snprintf(buffer + offset, buf_len - (size_t)offset,
                       "Cycle time:      %d us\n", deploy->expected_cycle_time_us);

    return offset;
}

/* Configuration management */
int pnet_config_load(pnet_driver_config_t *config, const char *path) {
    if (!config || !path) return -1;
    /* Simulation: load default config */
    pnet_config_default(config);
    strncpy(config->config_path, path, sizeof(config->config_path) - 1);
    return 0;
}

int pnet_config_save(const pnet_driver_config_t *config, const char *path) {
    if (!config || !path) return -1;
    /* Simulation */
    return 0;
}

int pnet_config_default(pnet_driver_config_t *config) {
    if (!config) return -1;

    memset(config, 0, sizeof(pnet_driver_config_t));
    strncpy(config->config_path, "/etc/pnet/config.conf", sizeof(config->config_path) - 1);
    strncpy(config->log_path, "/var/log/pnet/pnet.log", sizeof(config->log_path) - 1);
    config->log_level = 2;  /* INFO */
    config->enable_security = true;
    config->enable_monitoring = true;
    config->enable_redundancy = false;
    config->max_devices = PNET_MAX_DEVICES;
    config->max_connections = 64;

    pnet_network_init(&config->network);
    return 0;
}
