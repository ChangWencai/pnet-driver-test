/**
 * pnet_device.h - Profinet Device Abstraction Layer
 *
 * Based on document chapter 2: p-net driver architecture
 * Provides device file operations, memory mapping, and interrupt handling
 * interfaces for Profinet devices on Linux.
 */

#ifndef PNET_DEVICE_H
#define PNET_DEVICE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Device name and class constants */
#define PNET_DEVICE_NAME     "pnet_dev"
#define PNET_CLASS_NAME      "pnet_class"
#define PNET_DEVICE_PATH     "/dev/pnet_dev"
#define PNET_MAX_DEVICES     16
#define PNET_MAX_BUFFER_SIZE 4096

/* Device states */
typedef enum {
    PNET_DEV_STATE_UNINITIALIZED = 0,
    PNET_DEV_STATE_INITIALIZED,
    PNET_DEV_STATE_OPEN,
    PNET_DEV_STATE_RUNNING,
    PNET_DEV_STATE_ERROR,
    PNET_DEV_STATE_CLOSED
} pnet_dev_state_t;

/* Device types in Profinet network */
typedef enum {
    PNET_DEVICE_TYPE_IO_CONTROLLER = 0,
    PNET_DEVICE_TYPE_IO_DEVICE,
    PNET_DEVICE_TYPE_IO_SUPERVISOR,
    PNET_DEVICE_TYPE_IO_MONITOR
} pnet_device_type_t;

/* Profinet device information structure */
typedef struct {
    uint32_t vendor_id;
    uint32_t device_id;
    uint32_t profile_id;
    char     device_name[64];
    char     station_name[64];
    uint8_t  mac_addr[6];
    uint32_t ip_addr;
    uint32_t subnet_mask;
    uint32_t gateway;
    pnet_device_type_t type;
} pnet_device_info_t;

/* Device handle (opaque to users) */
typedef struct pnet_device {
    int              fd;
    int              major_number;
    pnet_dev_state_t state;
    pnet_device_info_t info;
    uint8_t          rx_buffer[PNET_MAX_BUFFER_SIZE];
    uint8_t          tx_buffer[PNET_MAX_BUFFER_SIZE];
    size_t           rx_len;
    size_t           tx_len;
    uint32_t         error_count;
    uint64_t         bytes_sent;
    uint64_t         bytes_received;
} pnet_device_t;

/* Memory mapping structure */
typedef struct {
    void    *virt_addr;
    uint64_t phys_addr;
    size_t   size;
    bool     mapped;
} pnet_mmap_t;

/* Interrupt callback type */
typedef void (*pnet_irq_handler_t)(void *context, uint32_t irq_number);

/* Interrupt info structure */
typedef struct {
    uint32_t          irq_number;
    pnet_irq_handler_t handler;
    void              *context;
    bool               registered;
    uint64_t           irq_count;
} pnet_irq_info_t;

/* Device lifecycle functions */
int  pnet_device_init(pnet_device_t *dev);
void pnet_device_cleanup(pnet_device_t *dev);
int  pnet_device_open(pnet_device_t *dev, const char *device_path);
int  pnet_device_close(pnet_device_t *dev);

/* Device I/O functions */
int  pnet_device_read(pnet_device_t *dev, void *buffer, size_t len);
int  pnet_device_write(pnet_device_t *dev, const void *buffer, size_t len);

/* Device info functions */
int  pnet_device_get_info(pnet_device_t *dev, pnet_device_info_t *info);
int  pnet_device_set_info(pnet_device_t *dev, const pnet_device_info_t *info);
const char* pnet_device_state_str(pnet_dev_state_t state);

/* Memory mapping functions */
int  pnet_mmap_create(pnet_device_t *dev, pnet_mmap_t *mmap, uint64_t phys_addr, size_t size);
int  pnet_mmap_destroy(pnet_mmap_t *mmap);
void *pnet_mmap_get_addr(pnet_mmap_t *mmap);

/* Interrupt handling functions */
int  pnet_irq_register(pnet_device_t *dev, pnet_irq_info_t *irq);
int  pnet_irq_unregister(pnet_irq_info_t *irq);
int  pnet_irq_simulate(pnet_irq_info_t *irq); /* For testing purposes */

#endif /* PNET_DEVICE_H */
