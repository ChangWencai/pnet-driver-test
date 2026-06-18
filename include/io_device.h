/**
 * io_device.h - Profinet IO Device Application
 *
 * Manages the lifecycle of a Profinet IO Device using the p-net stack.
 * Handles connection, data exchange, diagnosis, and alarms.
 */

#ifndef IO_DEVICE_H
#define IO_DEVICE_H

#include <stdint.h>
#include <stdbool.h>

/* Forward declaration - actual type comes from p-net */
typedef struct pnet pnet_t;

/* IO Device configuration */
typedef struct {
    char     product_name[26];
    char     station_name[241];
    char     interface_name[16];
    uint8_t  mac_addr[6];
    uint32_t ip_addr;
    uint32_t netmask;
    uint32_t gateway;
    uint16_t vendor_id;
    uint16_t device_id;
    uint32_t tick_us;           /* p-net tick interval */
    bool     send_hello;
    char     file_directory[240];
} io_device_cfg_t;

/* Module/submodule definition */
typedef struct {
    uint32_t api;
    uint16_t slot;
    uint16_t subslot;
    uint32_t module_ident;
    uint32_t submodule_ident;
    uint8_t  direction;    /* 0=no_io, 0x20=input, 0x40=output, 0x60=io */
    uint16_t input_size;
    uint16_t output_size;
} io_module_def_t;

/* IO Device state */
typedef enum {
    IO_DEVICE_STATE_IDLE = 0,
    IO_DEVICE_STATE_INITIALIZED,
    IO_DEVICE_STATE_CONNECTED,
    IO_DEVICE_STATE_RUNNING,
    IO_DEVICE_STATE_ERROR
} io_device_state_t;

/* IO Device handle */
typedef struct {
    io_device_cfg_t   config;
    io_device_state_t state;
    pnet_t           *pnet_handle;
    uint32_t          active_arep;
    uint64_t          cycle_count;
    uint64_t          error_count;
    /* Application data buffers */
    uint8_t           input_data[256];
    uint8_t           output_data[256];
    uint16_t          input_size;
    uint16_t          output_size;
} io_device_t;

/* Lifecycle */
int  io_device_init(io_device_t *dev, const io_device_cfg_t *cfg);
int  io_device_start(io_device_t *dev);
int  io_device_tick(io_device_t *dev);
int  io_device_stop(io_device_t *dev);
void io_device_cleanup(io_device_t *dev);

/* Data exchange */
int  io_device_update_input(io_device_t *dev, const uint8_t *data, uint16_t len);
int  io_device_read_output(io_device_t *dev, uint8_t *data, uint16_t *len);

/* Status */
io_device_state_t io_device_get_state(const io_device_t *dev);
const char* io_device_state_str(io_device_state_t state);
void io_device_show(const io_device_t *dev);

#endif /* IO_DEVICE_H */
