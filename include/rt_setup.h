/**
 * rt_setup.h - Real-Time Environment Setup
 *
 * Configures Linux system for deterministic real-time performance
 * required by Profinet RT/IRT communication.
 */

#ifndef RT_SETUP_H
#define RT_SETUP_H

#include <stdbool.h>

typedef struct {
    bool use_rt_scheduling;     /* SCHED_FIFO for main thread */
    int  rt_priority;           /* RT priority (1-99) */
    bool lock_memory;           /* mlockall to prevent page faults */
    bool disable_cpu_freq;      /* Disable CPU frequency scaling */
    int  isolate_cpu;           /* CPU core to isolate (-1 = none) */
    bool disable_irq_balance;   /* Disable irqbalance daemon */
} rt_config_t;

typedef struct {
    bool rt_scheduling_ok;
    bool memory_locked;
    bool cpu_isolated;
    bool irq_balance_disabled;
    double max_latency_us;      /* Measured with cyclictest-like probe */
    char report[512];
} rt_check_result_t;

/* Setup and teardown */
int  rt_setup_init(const rt_config_t *cfg);
int  rt_setup_apply(void);
void rt_setup_restore(void);

/* Verification */
int  rt_setup_check(rt_check_result_t *result);
int  rt_setup_measure_latency(double *max_latency_us, int samples);

/* Default configuration */
void rt_config_default(rt_config_t *cfg);

#endif /* RT_SETUP_H */
