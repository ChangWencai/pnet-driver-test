/**
 * pnet_device.c - Profinet Device Abstraction Layer Implementation
 *
 * Implements device file operations, memory mapping, and interrupt handling
 * for Profinet devices. Simulates Linux kernel device interactions in userspace.
 */

#include "pnet_device.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

int pnet_device_init(pnet_device_t *dev) {
    if (!dev) return -1;

    memset(dev, 0, sizeof(pnet_device_t));
    dev->fd = -1;
    dev->major_number = -1;
    dev->state = PNET_DEV_STATE_INITIALIZED;
    return 0;
}

void pnet_device_cleanup(pnet_device_t *dev) {
    if (!dev) return;

    if (dev->fd >= 0) {
        close(dev->fd);
        dev->fd = -1;
    }
    dev->state = PNET_DEV_STATE_CLOSED;
    memset(dev->rx_buffer, 0, sizeof(dev->rx_buffer));
    memset(dev->tx_buffer, 0, sizeof(dev->tx_buffer));
}

int pnet_device_open(pnet_device_t *dev, const char *device_path) {
    if (!dev || !device_path) return -1;

    if (dev->state < PNET_DEV_STATE_INITIALIZED) {
        return -1;
    }

    /* Try to open the actual device file if it exists */
    dev->fd = open(device_path, O_RDWR);
    if (dev->fd < 0) {
        /* Device file doesn't exist yet - simulation mode */
        dev->fd = -1;
    }

    dev->state = PNET_DEV_STATE_OPEN;
    return 0;
}

int pnet_device_close(pnet_device_t *dev) {
    if (!dev) return -1;

    if (dev->fd >= 0) {
        close(dev->fd);
        dev->fd = -1;
    }

    dev->state = PNET_DEV_STATE_CLOSED;
    return 0;
}

int pnet_device_read(pnet_device_t *dev, void *buffer, size_t len) {
    if (!dev || !buffer || len == 0) return -1;

    if (dev->state < PNET_DEV_STATE_OPEN) return -1;

    if (dev->fd >= 0) {
        ssize_t bytes = read(dev->fd, buffer, len);
        if (bytes > 0) {
            dev->bytes_received += (uint64_t)bytes;
            return (int)bytes;
        }
        return (int)bytes;
    }

    /* Simulation: return data from rx_buffer */
    size_t to_copy = (len < dev->rx_len) ? len : dev->rx_len;
    if (to_copy > 0) {
        memcpy(buffer, dev->rx_buffer, to_copy);
        dev->bytes_received += to_copy;
        dev->rx_len -= to_copy;
        if (dev->rx_len > 0) {
            memmove(dev->rx_buffer, dev->rx_buffer + to_copy, dev->rx_len);
        }
        return (int)to_copy;
    }

    return 0;
}

int pnet_device_write(pnet_device_t *dev, const void *buffer, size_t len) {
    if (!dev || !buffer || len == 0) return -1;

    if (dev->state < PNET_DEV_STATE_OPEN) return -1;

    if (dev->fd >= 0) {
        ssize_t bytes = write(dev->fd, buffer, len);
        if (bytes > 0) {
            dev->bytes_sent += (uint64_t)bytes;
            return (int)bytes;
        }
        return (int)bytes;
    }

    /* Simulation: write to tx_buffer */
    size_t space = PNET_MAX_BUFFER_SIZE - dev->tx_len;
    size_t to_copy = (len < space) ? len : space;
    if (to_copy > 0) {
        memcpy(dev->tx_buffer + dev->tx_len, buffer, to_copy);
        dev->tx_len += to_copy;
        dev->bytes_sent += to_copy;
        return (int)to_copy;
    }

    return 0;
}

int pnet_device_get_info(pnet_device_t *dev, pnet_device_info_t *info) {
    if (!dev || !info) return -1;
    memcpy(info, &dev->info, sizeof(pnet_device_info_t));
    return 0;
}

int pnet_device_set_info(pnet_device_t *dev, const pnet_device_info_t *info) {
    if (!dev || !info) return -1;
    memcpy(&dev->info, info, sizeof(pnet_device_info_t));
    return 0;
}

const char* pnet_device_state_str(pnet_dev_state_t state) {
    switch (state) {
        case PNET_DEV_STATE_UNINITIALIZED: return "UNINITIALIZED";
        case PNET_DEV_STATE_INITIALIZED:   return "INITIALIZED";
        case PNET_DEV_STATE_OPEN:          return "OPEN";
        case PNET_DEV_STATE_RUNNING:       return "RUNNING";
        case PNET_DEV_STATE_ERROR:         return "ERROR";
        case PNET_DEV_STATE_CLOSED:        return "CLOSED";
        default: return "UNKNOWN";
    }
}

int pnet_mmap_create(pnet_device_t *dev, pnet_mmap_t *mmap, uint64_t phys_addr, size_t size) {
    if (!dev || !mmap || size == 0) return -1;

    /* Simulation: allocate a memory region to represent the mapped area */
    mmap->virt_addr = calloc(1, size);
    if (!mmap->virt_addr) return -1;

    mmap->phys_addr = phys_addr;
    mmap->size = size;
    mmap->mapped = true;
    return 0;
}

int pnet_mmap_destroy(pnet_mmap_t *mmap) {
    if (!mmap) return -1;

    if (mmap->virt_addr) {
        free(mmap->virt_addr);
        mmap->virt_addr = NULL;
    }
    mmap->mapped = false;
    return 0;
}

void *pnet_mmap_get_addr(pnet_mmap_t *mmap) {
    if (!mmap || !mmap->mapped) return NULL;
    return mmap->virt_addr;
}

int pnet_irq_register(pnet_device_t *dev, pnet_irq_info_t *irq) {
    if (!dev || !irq) return -1;
    if (!irq->handler) return -1;

    irq->registered = true;
    irq->irq_count = 0;
    return 0;
}

int pnet_irq_unregister(pnet_irq_info_t *irq) {
    if (!irq) return -1;

    irq->registered = false;
    irq->handler = NULL;
    irq->context = NULL;
    return 0;
}

int pnet_irq_simulate(pnet_irq_info_t *irq) {
    if (!irq || !irq->registered || !irq->handler) return -1;

    /* Simulate interrupt by calling the handler */
    irq->handler(irq->context, irq->irq_number);
    irq->irq_count++;
    return 0;
}
