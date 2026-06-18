/**
 * rt_setup.c - Real-Time Environment Setup
 *
 * Configures the host system for deterministic real-time performance
 * required by PROFINET RT/IRT cyclic communication.
 *
 * C11 standard. Linux-specific operations guarded by PLATFORM_LINUX.
 */

#include "rt_setup.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <time.h>

#ifdef PLATFORM_LINUX
#include <sched.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#endif

/* ------------------------------------------------------------------ */
/* Static state                                                        */
/* ------------------------------------------------------------------ */

static rt_config_t  g_rt_config;
static bool         g_rt_initialized = false;

#ifdef PLATFORM_LINUX
/* Saved state for restore */
static struct sched_param   g_saved_sched;
static int                  g_saved_policy;
static char                 g_saved_governor[64];
#endif

/* ------------------------------------------------------------------ */
/* Default configuration                                               */
/* ------------------------------------------------------------------ */

void rt_config_default(rt_config_t *cfg)
{
    if (!cfg) {
        return;
    }

    memset(cfg, 0, sizeof(*cfg));
    cfg->use_rt_scheduling   = true;
    cfg->rt_priority         = 80;
    cfg->lock_memory         = true;
    cfg->disable_cpu_freq    = true;
    cfg->isolate_cpu         = -1;     /* No CPU isolation by default */
    cfg->disable_irq_balance = true;
}

/* ------------------------------------------------------------------ */
/* Initialization                                                      */
/* ------------------------------------------------------------------ */

int rt_setup_init(const rt_config_t *cfg)
{
    if (!cfg) {
        return -EINVAL;
    }

    /* Validate priority range */
    if (cfg->rt_priority < 1 || cfg->rt_priority > 99) {
        fprintf(stderr, "[rt_setup] invalid RT priority %d (must be 1-99)\n",
                cfg->rt_priority);
        return -EINVAL;
    }

    g_rt_config     = *cfg;
    g_rt_initialized = true;

    return 0;
}

/* ------------------------------------------------------------------ */
/* Apply                                                               */
/* ------------------------------------------------------------------ */

int rt_setup_apply(void)
{
    if (!g_rt_initialized) {
        fprintf(stderr, "[rt_setup] not initialized\n");
        return -EPERM;
    }

#ifdef PLATFORM_LINUX
    /* ---- 1. Real-time scheduling (SCHED_FIFO) ---- */
    if (g_rt_config.use_rt_scheduling) {
        /* Save current scheduling policy for restore */
        g_saved_policy = sched_getscheduler(0);
        if (g_saved_policy < 0) {
            fprintf(stderr, "[rt_setup] sched_getscheduler failed: %s\n",
                    strerror(errno));
            g_saved_policy = SCHED_OTHER;
        }

        if (sched_getparam(0, &g_saved_sched) != 0) {
            fprintf(stderr, "[rt_setup] sched_getparam failed: %s\n",
                    strerror(errno));
            memset(&g_saved_sched, 0, sizeof(g_saved_sched));
        }

        struct sched_param param;
        memset(&param, 0, sizeof(param));
        param.sched_priority = g_rt_config.rt_priority;

        if (sched_setscheduler(0, SCHED_FIFO, &param) != 0) {
            fprintf(stderr, "[rt_setup] sched_setscheduler(SCHED_FIFO, %d) failed: %s\n",
                    g_rt_config.rt_priority, strerror(errno));
            return -EPERM;
        }

        fprintf(stdout, "[rt_setup] SCHED_FIFO priority %d applied\n",
                g_rt_config.rt_priority);
    }

    /* ---- 2. Lock all memory pages ---- */
    if (g_rt_config.lock_memory) {
        if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
            fprintf(stderr, "[rt_setup] mlockall failed: %s\n",
                    strerror(errno));
            /* Non-fatal: continue with remaining setup */
        } else {
            fprintf(stdout, "[rt_setup] memory locked (mlockall)\n");
        }
    }

    /* ---- 3. CPU frequency scaling lock ---- */
    if (g_rt_config.disable_cpu_freq) {
        long ncpus = sysconf(_SC_NPROCESSORS_ONLN);
        for (long cpu = 0; cpu < ncpus; cpu++) {
            char path[128];
            snprintf(path, sizeof(path),
                     "/sys/devices/system/cpu/cpu%ld/cpufreq/scaling_governor",
                     cpu);

            /* Save original governor from CPU 0 */
            if (cpu == 0) {
                FILE *fp = fopen(path, "r");
                if (fp) {
                    if (fgets(g_saved_governor, sizeof(g_saved_governor), fp)) {
                        /* Strip trailing newline */
                        size_t slen = strlen(g_saved_governor);
                        if (slen > 0 && g_saved_governor[slen - 1] == '\n') {
                            g_saved_governor[slen - 1] = '\0';
                        }
                    }
                    fclose(fp);
                }
            }

            FILE *fp = fopen(path, "w");
            if (fp) {
                fputs("performance", fp);
                fclose(fp);
            } else {
                fprintf(stderr, "[rt_setup] cannot write to %s: %s\n",
                        path, strerror(errno));
            }
        }
        fprintf(stdout, "[rt_setup] CPU frequency governor set to 'performance'\n");
    }

    /* ---- 4. CPU isolation ---- */
    if (g_rt_config.isolate_cpu >= 0) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(g_rt_config.isolate_cpu, &cpuset);

        if (sched_setaffinity(0, sizeof(cpuset), &cpuset) != 0) {
            fprintf(stderr, "[rt_setup] sched_setaffinity(cpu %d) failed: %s\n",
                    g_rt_config.isolate_cpu, strerror(errno));
        } else {
            fprintf(stdout, "[rt_setup] process pinned to CPU %d\n",
                    g_rt_config.isolate_cpu);
        }
    }

    /* ---- 5. Disable irqbalance ---- */
    if (g_rt_config.disable_irq_balance) {
        int ret = system("systemctl stop irqbalance 2>/dev/null");
        if (ret == 0) {
            fprintf(stdout, "[rt_setup] irqbalance daemon stopped\n");
        } else {
            fprintf(stdout,
                    "[rt_setup] irqbalance stop returned %d "
                    "(may not be installed or already stopped)\n", ret);
        }
    }

    fprintf(stdout, "[rt_setup] real-time environment applied\n");
    return 0;

#else
    /* Non-Linux platform: log what would be done */
    fprintf(stdout,
            "[rt_setup] PLATFORM_LINUX not defined - "
            "real-time setup skipped. Requested configuration:\n"
            "  RT scheduling (SCHED_FIFO priority %d): not available\n"
            "  Memory lock (mlockall):               not available\n"
            "  CPU freq scaling lock:                not available\n"
            "  CPU isolation (core %d):               not available\n"
            "  irqbalance disable:                   not available\n",
            g_rt_config.rt_priority, g_rt_config.isolate_cpu);
    return -ENOSYS;
#endif
}

/* ------------------------------------------------------------------ */
/* Restore                                                             */
/* ------------------------------------------------------------------ */

void rt_setup_restore(void)
{
    if (!g_rt_initialized) {
        return;
    }

#ifdef PLATFORM_LINUX
    /* Restore scheduling policy */
    if (g_rt_config.use_rt_scheduling) {
        if (sched_setscheduler(0, g_saved_policy, &g_saved_sched) != 0) {
            fprintf(stderr, "[rt_setup] failed to restore scheduler: %s\n",
                    strerror(errno));
        } else {
            fprintf(stdout, "[rt_setup] scheduling policy restored\n");
        }
    }

    /* Unlock memory */
    if (g_rt_config.lock_memory) {
        if (munlockall() != 0) {
            fprintf(stderr, "[rt_setup] munlockall failed: %s\n",
                    strerror(errno));
        } else {
            fprintf(stdout, "[rt_setup] memory unlocked\n");
        }
    }

    /* Restore CPU frequency governor */
    if (g_rt_config.disable_cpu_freq && g_saved_governor[0] != '\0') {
        long ncpus = sysconf(_SC_NPROCESSORS_ONLN);
        for (long cpu = 0; cpu < ncpus; cpu++) {
            char path[128];
            snprintf(path, sizeof(path),
                     "/sys/devices/system/cpu/cpu%ld/cpufreq/scaling_governor",
                     cpu);

            FILE *fp = fopen(path, "w");
            if (fp) {
                fputs(g_saved_governor, fp);
                fclose(fp);
            }
        }
        fprintf(stdout, "[rt_setup] CPU governor restored to '%s'\n",
                g_saved_governor);
    }

    /* Restore CPU affinity to all CPUs */
    if (g_rt_config.isolate_cpu >= 0) {
        long ncpus = sysconf(_SC_NPROCESSORS_ONLN);
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        for (long cpu = 0; cpu < ncpus; cpu++) {
            CPU_SET(cpu, &cpuset);
        }
        if (sched_setaffinity(0, sizeof(cpuset), &cpuset) != 0) {
            fprintf(stderr, "[rt_setup] failed to restore CPU affinity: %s\n",
                    strerror(errno));
        } else {
            fprintf(stdout, "[rt_setup] CPU affinity restored to all cores\n");
        }
    }

    /* Re-enable irqbalance */
    if (g_rt_config.disable_irq_balance) {
        int ret = system("systemctl start irqbalance 2>/dev/null");
        (void)ret;
        fprintf(stdout, "[rt_setup] irqbalance restart requested\n");
    }

#else
    fprintf(stdout, "[rt_setup] PLATFORM_LINUX not defined - nothing to restore\n");
#endif

    g_rt_initialized = false;
    fprintf(stdout, "[rt_setup] real-time environment restored\n");
}

/* ------------------------------------------------------------------ */
/* Verification                                                        */
/* ------------------------------------------------------------------ */

int rt_setup_check(rt_check_result_t *result)
{
    if (!result) {
        return -EINVAL;
    }

    memset(result, 0, sizeof(*result));

#ifdef PLATFORM_LINUX
    int  offset  = 0;
    int  written;
    char *report = result->report;
    size_t rlen  = sizeof(result->report);

    /* Check RT scheduling */
    int current_policy = sched_getscheduler(0);
    if (current_policy == SCHED_FIFO || current_policy == SCHED_RR) {
        struct sched_param param;
        sched_getparam(0, &param);
        result->rt_scheduling_ok = true;

        written = snprintf(report + offset, rlen - (size_t)offset,
                           "[OK] RT scheduling active: policy=%d, priority=%d\n",
                           current_policy, param.sched_priority);
    } else {
        result->rt_scheduling_ok = false;
        written = snprintf(report + offset, rlen - (size_t)offset,
                           "[FAIL] RT scheduling not active (current policy=%d)\n",
                           current_policy);
    }
    if (written > 0) { offset += written; }

    /* Check memory lock status via /proc/self/status VmLck field */
    result->memory_locked = false;
    FILE *fp = fopen("/proc/self/status", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            unsigned long locked_kb = 0;
            if (sscanf(line, "VmLck: %lu kB", &locked_kb) == 1) {
                result->memory_locked = (locked_kb > 0);
                break;
            }
        }
        fclose(fp);
    }

    written = snprintf(report + offset, rlen - (size_t)offset,
                       "[%s] Memory lock: %s\n",
                       result->memory_locked ? "OK" : "FAIL",
                       result->memory_locked ? "locked" : "not locked");
    if (written > 0) { offset += written; }

    /* Check CPU isolation */
    if (g_rt_initialized && g_rt_config.isolate_cpu >= 0) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        if (sched_getaffinity(0, sizeof(cpuset), &cpuset) == 0) {
            int count = 0;
            long ncpus = sysconf(_SC_NPROCESSORS_ONLN);
            for (long cpu = 0; cpu < ncpus; cpu++) {
                if (CPU_ISSET((int)cpu, &cpuset)) {
                    count++;
                }
            }
            result->cpu_isolated = (count == 1);
        }
    }

    written = snprintf(report + offset, rlen - (size_t)offset,
                       "[%s] CPU isolation: %s\n",
                       result->cpu_isolated ? "OK" : "INFO",
                       result->cpu_isolated ? "isolated" : "not configured");
    if (written > 0) { offset += written; }

    /* Check irqbalance */
    result->irq_balance_disabled = false;
    int rc = system("systemctl is-active --quiet irqbalance 2>/dev/null");
    /* rc != 0 means irqbalance is not active (stopped or not installed) */
    result->irq_balance_disabled = (rc != 0);

    written = snprintf(report + offset, rlen - (size_t)offset,
                       "[%s] irqbalance: %s\n",
                       result->irq_balance_disabled ? "OK" : "WARN",
                       result->irq_balance_disabled ? "disabled" : "still running");
    if (written > 0) { offset += written; }

    /* Run a quick latency measurement (1000 samples) */
    double max_lat = 0.0;
    if (rt_setup_measure_latency(&max_lat, 1000) == 0) {
        result->max_latency_us = max_lat;
        written = snprintf(report + offset, rlen - (size_t)offset,
                           "[INFO] Max loop latency (1000 samples): %.2f us\n",
                           max_lat);
    } else {
        written = snprintf(report + offset, rlen - (size_t)offset,
                           "[WARN] Latency measurement failed\n");
    }
    if (written > 0) { offset += written; }

    return 0;

#else
    /* Non-Linux: report limitations */
    result->rt_scheduling_ok    = false;
    result->memory_locked       = false;
    result->cpu_isolated        = false;
    result->irq_balance_disabled = false;
    result->max_latency_us      = 0.0;

    snprintf(result->report, sizeof(result->report),
             "PLATFORM_LINUX not defined - real-time features unavailable.\n"
             "RT scheduling:   not available\n"
             "Memory lock:     not available\n"
             "CPU isolation:   not available\n"
             "irqbalance:      not available\n"
             "Latency measure: using CLOCK_MONOTONIC (limited accuracy)\n");

    return 0;
#endif
}

/* ------------------------------------------------------------------ */
/* Latency measurement                                                 */
/* ------------------------------------------------------------------ */

int rt_setup_measure_latency(double *max_latency_us, int samples)
{
    if (!max_latency_us || samples <= 0) {
        return -EINVAL;
    }

    struct timespec t_start, t_end;
    double max_dev_us = 0.0;

    /*
     * Expected period for a tight busy-loop iteration is very short
     * (sub-microsecond). We measure actual elapsed time per iteration
     * and track the maximum deviation from the first sample as a
     * proxy for scheduling jitter.
     */

    /* Take one reference sample to establish baseline */
    if (clock_gettime(CLOCK_MONOTONIC, &t_start) != 0) {
        fprintf(stderr, "[rt_setup] clock_gettime failed: %s\n", strerror(errno));
        return -EIO;
    }

    /* Warm up: one dummy iteration */
    volatile uint64_t sink = 0;
    for (int w = 0; w < 1000; w++) {
        sink += (uint64_t)w;
    }

    if (clock_gettime(CLOCK_MONOTONIC, &t_end) != 0) {
        return -EIO;
    }

    double baseline_ns =
        (double)(t_end.tv_sec  - t_start.tv_sec)  * 1.0e9 +
        (double)(t_end.tv_nsec - t_start.tv_nsec);

    double baseline_per_iter_ns = baseline_ns / 1000.0;

    /* Measurement loop: run N batches of 1000 iterations each */
    for (int i = 0; i < samples; i++) {
        if (clock_gettime(CLOCK_MONOTONIC, &t_start) != 0) {
            return -EIO;
        }

        sink = 0;
        for (int w = 0; w < 1000; w++) {
            sink += (uint64_t)w;
        }

        if (clock_gettime(CLOCK_MONOTONIC, &t_end) != 0) {
            return -EIO;
        }

        double elapsed_ns =
            (double)(t_end.tv_sec  - t_start.tv_sec)  * 1.0e9 +
            (double)(t_end.tv_nsec - t_start.tv_nsec);

        /* Deviation from baseline in microseconds */
        double deviation_us = (elapsed_ns - baseline_per_iter_ns * 1000.0) / 1000.0;
        if (deviation_us < 0.0) {
            deviation_us = -deviation_us;
        }

        if (deviation_us > max_dev_us) {
            max_dev_us = deviation_us;
        }
    }

    *max_latency_us = max_dev_us;
    (void)sink; /* suppress unused warning */

    return 0;
}
