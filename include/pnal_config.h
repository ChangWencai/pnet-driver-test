/**
 * pnal_config.h - Platform Abstraction Layer Configuration for Linux
 *
 * This file provides the OS abstraction layer (OSAL) types and
 * configuration needed by p-net on Linux.
 */

#ifndef PNAL_CONFIG_H
#define PNAL_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Thread and mutex types (POSIX) */
#include <pthread.h>
#include <semaphore.h>

typedef pthread_t       pnal_thread_t;
typedef pthread_mutex_t pnal_mutex_t;
typedef sem_t           pnal_sem_t;
typedef uint32_t        pnal_buf_t;  /* Placeholder for buffer handle */

/* Time types */
typedef struct pnal_time {
    int64_t sec;
    int32_t nsec;
} pnal_time_t;

/* System uptime in microseconds */
static inline uint32_t pnal_get_system_uptime_10ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)((ts.tv_sec * 100) + (ts.tv_nsec / 10000000));
}

/* Byte order - Linux provides these */
#include <endian.h>
#include <arpa/inet.h>

/* Network types */
#include <sys/socket.h>
#include <netinet/in.h>

typedef int pnal_socket_t;
#define PNAL_INVALID_SOCKET (-1)

/* Ethernet raw socket support */
#include <net/if.h>
#include <net/ethernet.h>

/* Logging macros */
#define PNAL_LOG_ERROR(...)   fprintf(stderr, "[PNAL ERROR] " __VA_ARGS__)
#define PNAL_LOG_WARNING(...) fprintf(stderr, "[PNAL WARN]  " __VA_ARGS__)
#define PNAL_LOG_INFO(...)    fprintf(stdout, "[PNAL INFO]  " __VA_ARGS__)
#define PNAL_LOG_DEBUG(...)   fprintf(stdout, "[PNAL DEBUG] " __VA_ARGS__)

/* SNMP support (optional) */
/* #define PNAL_USE_SNMP */

#endif /* PNAL_CONFIG_H */
