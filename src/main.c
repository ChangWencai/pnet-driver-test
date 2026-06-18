/**
 * main.c - p-net IO Device Manager Entry Point
 *
 * Orchestrates the lifecycle of a Profinet IO Device:
 *   1. Parse command-line arguments
 *   2. Load (or default) application configuration
 *   3. Apply network, security, and real-time settings
 *   4. Initialise and run the IO device main loop
 *   5. Graceful shutdown on SIGINT / SIGTERM
 *
 * Works on real Linux targets and in mock/simulation mode on
 * other platforms (network and RT calls are guarded).
 */

#include "app_config.h"
#include "io_device.h"
#include "network_config.h"
#include "security_policy.h"
#include "rt_setup.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>

/* ------------------------------------------------------------------ */
/*  Version & defaults                                                */
/* ------------------------------------------------------------------ */

#define PNET_MGR_VERSION   "1.0.0"
#define DEFAULT_CONFIG_FILE "/etc/pnet-manager/config.conf"

/* ------------------------------------------------------------------ */
/*  Signal handling                                                   */
/* ------------------------------------------------------------------ */

static volatile sig_atomic_t g_running = 1;

static void signal_handler(int sig)
{
    (void)sig;
    g_running = 0;
}

static int install_signal_handlers(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) < 0) {
        perror("sigaction(SIGINT)");
        return -1;
    }
    if (sigaction(SIGTERM, &sa, NULL) < 0) {
        perror("sigaction(SIGTERM)");
        return -1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Helpers                                                           */
/* ------------------------------------------------------------------ */

/**
 * Format a uint32_t IP (network byte order) into dotted-quad string.
 * Local copy so we don't depend on network_config at banner time.
 */
static void ip_to_str(uint32_t ip, char *buf, size_t len)
{
    snprintf(buf, len, "%u.%u.%u.%u",
             (ip >> 24) & 0xFF,
             (ip >> 16) & 0xFF,
             (ip >> 8)  & 0xFF,
              ip        & 0xFF);
}

static void print_banner(const app_config_t *cfg)
{
    char ip_buf[16], mask_buf[16], gw_buf[16];

    ip_to_str(cfg->device.ip_addr, ip_buf, sizeof(ip_buf));
    ip_to_str(cfg->device.netmask, mask_buf, sizeof(mask_buf));
    ip_to_str(cfg->device.gateway, gw_buf, sizeof(gw_buf));

    printf("==========================================================\n");
    printf("  p-net IO Device Manager  v%s\n", PNET_MGR_VERSION);
    printf("==========================================================\n");
    printf("  Product name  : %s\n",  cfg->device.product_name);
    printf("  Station name  : %s\n",  cfg->device.station_name);
    printf("  Interface     : %s\n",  cfg->device.interface_name);
    printf("  IP address    : %s\n",  ip_buf);
    printf("  Netmask       : %s\n",  mask_buf);
    printf("  Gateway       : %s\n",  gw_buf);
    printf("  Vendor ID     : 0x%04X\n", cfg->device.vendor_id);
    printf("  Device ID     : 0x%04X\n", cfg->device.device_id);
    printf("  Tick interval : %u us\n", cfg->device.tick_us);
    printf("  Security      : %s\n",  cfg->security_enabled  ? "enabled" : "disabled");
    printf("  Real-time     : %s\n",  cfg->realtime_enabled  ? "enabled" : "disabled");
    printf("  Log level     : %d ",   cfg->log_level);
    switch (cfg->log_level) {
        case 0: printf("(ERROR)"); break;
        case 1: printf("(WARN)");  break;
        case 2: printf("(INFO)");  break;
        case 3: printf("(DEBUG)"); break;
        default: printf("(?)");    break;
    }
    printf("\n");
    printf("  Config file   : %s\n", cfg->config_file);
    printf("==========================================================\n");
}

static void print_usage(const char *prog)
{
    printf("Usage: %s [OPTIONS]\n\n", prog);
    printf("Options:\n");
    printf("  -c <file>   Path to configuration file\n");
    printf("              (default: %s)\n", DEFAULT_CONFIG_FILE);
    printf("  -v          Verbose output (set log level to DEBUG)\n");
    printf("  -h          Show this help message\n");
    printf("\n");
    printf("Signals:\n");
    printf("  SIGINT, SIGTERM   Graceful shutdown\n");
}

/* ------------------------------------------------------------------ */
/*  Apply subsystem configurations                                    */
/* ------------------------------------------------------------------ */

/**
 * Apply network interface settings.
 * On non-Linux platforms this is a no-op (the functions are not
 * available, so we guard with __linux__).
 */
static int apply_network(const app_config_t *cfg)
{
#ifdef __linux__
    const char *iface = cfg->device.interface_name;

    printf("[net] Configuring interface %s ...\n", iface);

    if (net_if_set_ip(iface, cfg->network.ip_addr) < 0) {
        fprintf(stderr, "[net] Failed to set IP address on %s\n", iface);
        return -1;
    }

    if (net_if_set_netmask(iface, cfg->network.netmask) < 0) {
        fprintf(stderr, "[net] Failed to set netmask on %s\n", iface);
        return -1;
    }

    if (net_if_set_up(iface) < 0) {
        fprintf(stderr, "[net] Failed to bring up %s\n", iface);
        return -1;
    }

    printf("[net] Interface %s configured successfully\n", iface);
#else
    printf("[net] Skipping network configuration (non-Linux platform)\n");
    (void)cfg;
#endif
    return 0;
}

/**
 * Apply security policy if enabled.
 */
static int apply_security(const app_config_t *cfg)
{
    if (!cfg->security_enabled) {
        printf("[sec] Security policy disabled, skipping\n");
        return 0;
    }

    printf("[sec] Applying security policy (%d rules) ...\n",
           cfg->security.rule_count);

    if (sec_policy_apply(&cfg->security) < 0) {
        fprintf(stderr, "[sec] Failed to apply security policy\n");
        return -1;
    }

    printf("[sec] Security policy applied\n");
    return 0;
}

/**
 * Apply real-time setup if enabled.
 * rt_setup_init() stores the config, rt_setup_apply() activates it.
 */
static int apply_realtime(const app_config_t *cfg)
{
    if (!cfg->realtime_enabled) {
        printf("[rt] Real-time setup disabled, skipping\n");
        return 0;
    }

    printf("[rt] Initialising real-time environment (priority=%d) ...\n",
           cfg->realtime.rt_priority);

    if (rt_setup_init(&cfg->realtime) < 0) {
        fprintf(stderr, "[rt] rt_setup_init failed\n");
        return -1;
    }

    if (rt_setup_apply() < 0) {
        fprintf(stderr, "[rt] rt_setup_apply failed\n");
        return -1;
    }

    printf("[rt] Real-time environment configured\n");
    return 0;
}

/* ------------------------------------------------------------------ */
/*  main                                                              */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
    const char *config_path = DEFAULT_CONFIG_FILE;
    int verbose = 0;
    int opt;

    /* ---- Parse command-line arguments ---- */
    while ((opt = getopt(argc, argv, "c:vh")) != -1) {
        switch (opt) {
        case 'c':
            config_path = optarg;
            break;
        case 'v':
            verbose = 1;
            break;
        case 'h':
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        default:
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    /* ---- Load configuration ---- */
    app_config_t config;
    memset(&config, 0, sizeof(config));

    if (app_config_load(&config, config_path) < 0) {
        fprintf(stderr, "[cfg] Could not load '%s', using defaults\n",
                config_path);
        app_config_default(&config);
        strncpy(config.config_file, config_path,
                sizeof(config.config_file) - 1);
    }

    /* Override log level when -v is given */
    if (verbose)
        config.log_level = 3;  /* DEBUG */

    /* ---- Validate ---- */
    if (app_config_validate(&config) < 0) {
        fprintf(stderr, "[cfg] Configuration validation failed\n");
        return EXIT_FAILURE;
    }

    /* ---- Startup banner ---- */
    print_banner(&config);

    /* ---- Install signal handlers ---- */
    if (install_signal_handlers() < 0) {
        fprintf(stderr, "Failed to install signal handlers\n");
        return EXIT_FAILURE;
    }

    /* ---- Apply network configuration ---- */
    if (apply_network(&config) < 0) {
        fprintf(stderr, "Network configuration failed\n");
        return EXIT_FAILURE;
    }

    /* ---- Apply security policy ---- */
    if (apply_security(&config) < 0) {
        fprintf(stderr, "Security policy application failed\n");
        return EXIT_FAILURE;
    }

    /* ---- Apply real-time setup ---- */
    if (apply_realtime(&config) < 0) {
        fprintf(stderr, "Real-time setup failed\n");
        /* Non-fatal in mock mode: continue */
    }

    /* ---- Initialise IO device ---- */
    io_device_t device;
    memset(&device, 0, sizeof(device));

    printf("[io] Initialising IO device ...\n");
    if (io_device_init(&device, &config.device) < 0) {
        fprintf(stderr, "[io] io_device_init failed\n");
        goto cleanup_rt;
    }

    /* ---- Start IO device ---- */
    printf("[io] Starting IO device ...\n");
    if (io_device_start(&device) < 0) {
        fprintf(stderr, "[io] io_device_start failed\n");
        goto cleanup_device;
    }

    printf("[io] IO device running (state: %s)\n",
           io_device_state_str(io_device_get_state(&device)));
    printf("[main] Entering main loop (tick=%u us). Press Ctrl+C to stop.\n",
           config.device.tick_us);

    /* ============================================================ */
    /*  Main loop                                                    */
    /* ============================================================ */
    while (g_running) {
        int rc = io_device_tick(&device);
        if (rc < 0) {
            fprintf(stderr, "[io] io_device_tick returned error (%d)\n", rc);
            device.error_count++;
            /* Continue running; transient errors are expected */
        }
        usleep(config.device.tick_us);
    }

    /* ============================================================ */
    /*  Shutdown                                                     */
    /* ============================================================ */
    printf("\n[main] Shutdown signal received, stopping ...\n");

    printf("[io] Stopping IO device (cycles=%lu, errors=%lu) ...\n",
           (unsigned long)device.cycle_count,
           (unsigned long)device.error_count);
    io_device_stop(&device);

cleanup_device:
    printf("[io] Cleaning up IO device ...\n");
    io_device_cleanup(&device);

cleanup_rt:
    if (config.realtime_enabled) {
        printf("[rt] Restoring real-time environment ...\n");
        rt_setup_restore();
    }

    printf("[main] p-net IO Device Manager stopped. Goodbye.\n");
    return EXIT_SUCCESS;
}
