/**
 * main.c - p-net IO Device Manager Entry Point
 *
 * 7×24 production entry point with:
 *   - Signal-based graceful shutdown
 *   - systemd watchdog notification (optional)
 *   - Automatic device recovery with exponential backoff
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
#include <time.h>
#include <stdarg.h>

#ifdef HAVE_SYSTEMD
#include <systemd/sd-daemon.h>
#endif

/* ------------------------------------------------------------------ */
/*  Version & defaults                                                */
/* ------------------------------------------------------------------ */

#define PNET_MGR_VERSION     "1.0.1"
#define DEFAULT_CONFIG_FILE  "/etc/pnet-manager/config.conf"

/* Recovery constants */
#define MAX_CONSECUTIVE_ERRORS  5
#define RECOVERY_BACKOFF_BASE   1000000    /* 1 second in us */
#define RECOVERY_BACKOFF_MAX    30000000   /* 30 seconds in us */
#define MAX_RECOVERY_ATTEMPTS   0          /* 0 = unlimited */

/* Watchdog: systemd expects a ping every WatchdogSec/2 */
#define WATCHDOG_PING_MS       15000       /* 15 seconds */

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
    /* Also handle SIGHUP for config reload (reserved for future use) */
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Helpers                                                           */
/* ------------------------------------------------------------------ */

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
/*  systemd watchdog                                                  */
/* ------------------------------------------------------------------ */

#ifdef HAVE_SYSTEMD
static void watchdog_notify(void)
{
    sd_notify(0, "WATCHDOG=1");
}
static void watchdog_notify_stopping(void)
{
    sd_notify(0, "STOPPING=1");
}
static void watchdog_notify_ready(void)
{
    sd_notify(0, "READY=1");
}
static void watchdog_notify_status(const char *fmt, ...)
{
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    sd_notifyf(0, "STATUS=%s", buf);
}
#else
static void watchdog_notify(void)            { (void)0; }
static void watchdog_notify_stopping(void)   { (void)0; }
static void watchdog_notify_ready(void)      { (void)0; }
static void watchdog_notify_status(const char *fmt, ...) { (void)fmt; }
#endif

/* ------------------------------------------------------------------ */
/*  Apply subsystem configurations                                    */
/* ------------------------------------------------------------------ */

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
/*  IO device init / start wrapper                                    */
/* ------------------------------------------------------------------ */

static int device_init_and_start(io_device_t *dev, const io_device_cfg_t *cfg)
{
    memset(dev, 0, sizeof(*dev));
    printf("[io] Initialising IO device ...\n");
    if (io_device_init(dev, cfg) < 0) {
        fprintf(stderr, "[io] io_device_init failed\n");
        return -1;
    }
    printf("[io] Starting IO device ...\n");
    if (io_device_start(dev) < 0) {
        fprintf(stderr, "[io] io_device_start failed\n");
        io_device_cleanup(dev);
        return -1;
    }
    return 0;
}

static void device_stop_and_cleanup(io_device_t *dev)
{
    io_device_stop(dev);
    io_device_cleanup(dev);
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
        snprintf(config.config_file, sizeof(config.config_file), "%s", config_path);
    }

    if (verbose)
        config.log_level = 3;

    if (app_config_validate(&config) < 0) {
        fprintf(stderr, "[cfg] Configuration validation failed\n");
        return EXIT_FAILURE;
    }

    /* ---- Startup ---- */
    print_banner(&config);

    if (install_signal_handlers() < 0) {
        fprintf(stderr, "Failed to install signal handlers\n");
        return EXIT_FAILURE;
    }

    if (apply_network(&config) < 0) {
        fprintf(stderr, "Network configuration failed\n");
        return EXIT_FAILURE;
    }
    if (apply_security(&config) < 0) {
        fprintf(stderr, "Security policy application failed\n");
        return EXIT_FAILURE;
    }
    if (apply_realtime(&config) < 0) {
        fprintf(stderr, "Real-time setup failed\n");
    }

    /* ============================================================ */
    /*  Main loop with watchdog + auto-recovery                     */
    /* ============================================================ */

    io_device_t device;
    memset(&device, 0, sizeof(device));

    int consecutive_errors = 0;
    int recovery_attempts  = 0;
    useconds_t backoff_us  = RECOVERY_BACKOFF_BASE;
    unsigned long last_watchdog_ms = 0;

    /* Initial device start */
    if (device_init_and_start(&device, &config.device) < 0) {
        fprintf(stderr, "[main] Initial device startup failed, exiting\n");
        goto cleanup_rt;
    }
    watchdog_notify_ready();

    printf("[io] IO device running (state: %s)\n",
           io_device_state_str(io_device_get_state(&device)));
    printf("[main] Entering production loop (tick=%u us). "
           "Watchdog=%s, Auto-recovery=%s\n",
           config.device.tick_us,
#ifdef HAVE_SYSTEMD
           "enabled",
#else
           "not available",
#endif
           MAX_CONSECUTIVE_ERRORS > 0 ? "enabled" : "disabled");

    while (g_running) {
        /* ---- Periodic tick ---- */
        int rc = io_device_tick(&device);
        if (rc == 0) {
            /* Success: reset error tracking */
            consecutive_errors = 0;
            recovery_attempts  = 0;
            backoff_us         = RECOVERY_BACKOFF_BASE;
        } else {
            /* Error: increment and check threshold */
            device.error_count++;
            consecutive_errors++;

            fprintf(stderr, "[io] io_device_tick error (%d), "
                    "consecutive=%d/%d\n",
                    rc, consecutive_errors, MAX_CONSECUTIVE_ERRORS);

            if (MAX_CONSECUTIVE_ERRORS > 0 &&
                consecutive_errors >= MAX_CONSECUTIVE_ERRORS) {
                /* Attempt recovery */
                recovery_attempts++;
                if (MAX_RECOVERY_ATTEMPTS > 0 &&
                    recovery_attempts > MAX_RECOVERY_ATTEMPTS) {
                    fprintf(stderr, "[main] Max recovery attempts (%d) "
                            "reached, exiting\n", MAX_RECOVERY_ATTEMPTS);
                    break;
                }

                fprintf(stderr, "[main] Starting recovery attempt %d "
                        "(backoff %.3fs) ...\n",
                        recovery_attempts, (double)backoff_us / 1000000.0);
                watchdog_notify_status("recovering attempt %d",
                                       recovery_attempts);

                /* Stop and clean up */
                device_stop_and_cleanup(&device);

                /* Wait with backoff before retry */
                usleep(backoff_us);

                /* Exponential backoff (capped) */
                backoff_us *= 2;
                if (backoff_us > RECOVERY_BACKOFF_MAX)
                    backoff_us = RECOVERY_BACKOFF_MAX;

                /* Re-init and re-start */
                if (device_init_and_start(&device, &config.device) == 0) {
                    consecutive_errors = 0;
                    printf("[main] Recovery successful\n");
                    watchdog_notify_status("running");
                } else {
                    fprintf(stderr, "[main] Recovery attempt %d failed\n",
                            recovery_attempts);
                    /* Continue loop: will retry with longer backoff */
                }
            }
        }

        /* ---- systemd watchdog ping (monotonic clock) ---- */
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        unsigned long now_ms = (unsigned long)(ts.tv_sec * 1000UL
                                             + ts.tv_nsec / 1000000UL);
        if (now_ms - last_watchdog_ms >= WATCHDOG_PING_MS) {
            watchdog_notify();
            last_watchdog_ms = now_ms;
        }

        /* ---- Sleep for tick interval ---- */
        usleep(config.device.tick_us);
    }

    /* ============================================================ */
    /*  Shutdown                                                     */
    /* ============================================================ */
    printf("\n[main] Shutdown signal received, stopping ...\n");
    watchdog_notify_stopping();

    printf("[io] Stopping IO device (cycles=%lu, errors=%lu) ...\n",
           (unsigned long)device.cycle_count,
           (unsigned long)device.error_count);
    device_stop_and_cleanup(&device);

cleanup_rt:
    if (config.realtime_enabled) {
        printf("[rt] Restoring real-time environment ...\n");
        rt_setup_restore();
    }

    watchdog_notify_status("stopped");
    printf("[main] p-net IO Device Manager stopped. Goodbye.\n");
    return EXIT_SUCCESS;
}
