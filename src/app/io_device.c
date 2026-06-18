/**
 * io_device.c - Profinet IO Device Application Implementation
 *
 * Manages the lifecycle of a Profinet IO Device using the p-net stack.
 * Handles initialization, module plugging, cyclic data exchange,
 * diagnosis, and graceful shutdown.
 *
 * Build with -DUSE_REAL_PNET on Linux to use the real p-net stack.
 * Without that define, the mock API is used (suitable for macOS/testing).
 */

#include "io_device.h"

#include <stdio.h>
#include <string.h>

#ifdef USE_REAL_PNET
#include "pnal_sys.h"
#include "pnal.h"
#include "pnet_api.h"
#else
#include "pnet_api_mock.h"

/* The mock header is missing some constants that the real pnet_api.h defines.
 * Provide fallback definitions so the same code compiles in both modes. */
#ifndef PNET_SUBMOD_DAP_INTERFACE_1_PORT_1_IDENT
#define PNET_SUBMOD_DAP_INTERFACE_1_PORT_1_IDENT 0x00008001
#endif
#ifndef PNET_SUPPORTED_IM1
#define PNET_SUPPORTED_IM1 0x0002
#endif
#endif

/* ======================================================================
 * Private constants - Module and submodule ident numbers
 *
 * These must match the GSDML file for the device. The DAP (Device Access
 * Point) always occupies slot 0. User I/O modules start at slot 1.
 * ====================================================================== */

/* User module at slot 1: 8-channel digital I/O example */
#define EXAMPLE_API             0
#define EXAMPLE_SLOT            1
#define EXAMPLE_MODULE_IDENT    0x00000030

/* Input submodule: subslot 1, direction = INPUT */
#define EXAMPLE_SUBSLOT_INPUT   0x00000001
#define EXAMPLE_SUBMOD_INPUT    0x00000031
#define EXAMPLE_INPUT_SIZE      1   /* 8 digital inputs = 1 byte */

/* Output submodule: subslot 2, direction = OUTPUT */
#define EXAMPLE_SUBSLOT_OUTPUT  0x00000002
#define EXAMPLE_SUBMOD_OUTPUT   0x00000032
#define EXAMPLE_OUTPUT_SIZE     1   /* 8 digital outputs = 1 byte */

/* ======================================================================
 * Safe string copy helper
 *
 * Copies at most (dst_size - 1) bytes and always null-terminates.
 * ====================================================================== */
static void safe_strncpy(char *dst, const char *src, size_t dst_size)
{
    if (dst_size == 0)
        return;
    size_t srclen = strlen(src);
    size_t copy = srclen < dst_size - 1 ? srclen : dst_size - 1;
    memcpy(dst, src, copy);
    dst[copy] = '\0';
}

/* ======================================================================
 * Event name helper (shared between real and mock callback sets)
 * ====================================================================== */
static const char *event_name(pnet_event_values_t event)
{
    switch (event)
    {
    case PNET_EVENT_ABORT:   return "ABORT";
    case PNET_EVENT_STARTUP: return "STARTUP";
    case PNET_EVENT_PRMEND:  return "PRMEND";
    case PNET_EVENT_APPLRDY: return "APPLRDY";
    case PNET_EVENT_DATA:    return "DATA";
    default:                 return "UNKNOWN";
    }
}

/* ======================================================================
 * Callback functions
 *
 * The real p-net API and the mock API have different callback signatures
 * (the mock uses simplified parameter types). We provide two sets of
 * callbacks selected at compile time via USE_REAL_PNET.
 *
 * All callbacks receive the io_device_t pointer through the cb_arg
 * mechanism, allowing them to update device state.
 * ====================================================================== */

#ifdef USE_REAL_PNET

/* ---------- Real p-net callbacks (full signatures) ------------------ */

static int app_state_ind_cb(
    pnet_t *net, void *arg, uint32_t arep, pnet_event_values_t state)
{
    io_device_t *dev = (io_device_t *)arg;
    (void)net;

    printf("[IO-DEVICE] State event: %s (AREP=%u)\n",
           event_name(state), (unsigned)arep);

    /* Save the active AREP for later use (e.g. pnet_application_ready) */
    if (state == PNET_EVENT_STARTUP)
    {
        dev->active_arep = arep;
    }

    if (state == PNET_EVENT_DATA)
    {
        dev->state = IO_DEVICE_STATE_RUNNING;
    }
    else if (state == PNET_EVENT_ABORT)
    {
        dev->state = IO_DEVICE_STATE_CONNECTED;
        dev->active_arep = 0;
    }

    return 0;
}

static int app_connect_ind_cb(
    pnet_t *net, void *arg, uint32_t arep, pnet_result_t *p_result)
{
    (void)net; (void)arg; (void)p_result;
    printf("[IO-DEVICE] Connect indication (AREP=%u)\n", (unsigned)arep);
    return 0;  /* Accept the connection */
}

static int app_release_ind_cb(
    pnet_t *net, void *arg, uint32_t arep, pnet_result_t *p_result)
{
    (void)net; (void)arg; (void)p_result;
    printf("[IO-DEVICE] Release indication (AREP=%u)\n", (unsigned)arep);
    return 0;
}

static int app_dcontrol_ind_cb(
    pnet_t *net, void *arg, uint32_t arep,
    pnet_control_command_t control_command, pnet_result_t *p_result)
{
    (void)net; (void)arg; (void)p_result;
    printf("[IO-DEVICE] DControl indication (AREP=%u, cmd=%d)\n",
           (unsigned)arep, (int)control_command);
    return 0;
}

static int app_ccontrol_cnf_cb(
    pnet_t *net, void *arg, uint32_t arep, pnet_result_t *p_result)
{
    (void)net; (void)arg; (void)p_result;
    printf("[IO-DEVICE] CControl confirmation (AREP=%u)\n", (unsigned)arep);
    return 0;
}

static int app_write_ind_cb(
    pnet_t *net, void *arg, uint32_t arep,
    uint32_t api, uint16_t slot, uint16_t subslot,
    uint16_t idx, uint16_t sequence_number,
    uint16_t write_length, const uint8_t *p_write_data,
    pnet_result_t *p_result)
{
    (void)net; (void)arg; (void)p_result;
    (void)p_write_data;
    printf("[IO-DEVICE] Write indication (AREP=%u, api=%u, slot=%u, "
           "subslot=%u, idx=0x%04x, seq=%u, len=%u)\n",
           (unsigned)arep, (unsigned)api, (unsigned)slot,
           (unsigned)subslot, (unsigned)idx,
           (unsigned)sequence_number, (unsigned)write_length);
    return 0;
}

static int app_read_ind_cb(
    pnet_t *net, void *arg, uint32_t arep,
    uint32_t api, uint16_t slot, uint16_t subslot,
    uint16_t idx, uint16_t sequence_number,
    uint8_t **pp_read_data, uint16_t *p_read_length,
    pnet_result_t *p_result)
{
    (void)net; (void)arg; (void)p_result;
    (void)pp_read_data; (void)p_read_length;
    printf("[IO-DEVICE] Read indication (AREP=%u, api=%u, slot=%u, "
           "subslot=%u, idx=0x%04x, seq=%u)\n",
           (unsigned)arep, (unsigned)api, (unsigned)slot,
           (unsigned)subslot, (unsigned)idx, (unsigned)sequence_number);
    return 0;
}

static int app_exp_module_ind_cb(
    pnet_t *net, void *arg, uint32_t api,
    uint16_t slot, uint32_t module_ident)
{
    (void)net; (void)arg;
    printf("[IO-DEVICE] Expected module indication "
           "(api=%u, slot=%u, module_ident=0x%08x)\n",
           (unsigned)api, (unsigned)slot, (unsigned)module_ident);
    return 0;
}

static int app_exp_submodule_ind_cb(
    pnet_t *net, void *arg, uint32_t api,
    uint16_t slot, uint16_t subslot,
    uint32_t module_ident, uint32_t submodule_ident,
    const pnet_data_cfg_t *p_exp_data)
{
    (void)net; (void)arg;
    printf("[IO-DEVICE] Expected submodule indication "
           "(api=%u, slot=%u, subslot=%u, mod=0x%08x, submod=0x%08x)\n",
           (unsigned)api, (unsigned)slot, (unsigned)subslot,
           (unsigned)module_ident, (unsigned)submodule_ident);
    if (p_exp_data)
    {
        printf("[IO-DEVICE]   direction=%d, in_size=%u, out_size=%u\n",
               (int)p_exp_data->data_dir,
               (unsigned)p_exp_data->insize,
               (unsigned)p_exp_data->outsize);
    }
    return 0;
}

static int app_new_data_status_ind_cb(
    pnet_t *net, void *arg, uint32_t arep, uint32_t crep,
    uint8_t changes, uint8_t data_status)
{
    (void)net; (void)arg;
    printf("[IO-DEVICE] New data status (AREP=%u, CREP=%u, "
           "changes=0x%02x, status=0x%02x)\n",
           (unsigned)arep, (unsigned)crep,
           (unsigned)changes, (unsigned)data_status);
    return 0;
}

static int app_alarm_ind_cb(
    pnet_t *net, void *arg, uint32_t arep,
    const pnet_alarm_argument_t *p_alarm_argument,
    uint16_t data_len, uint16_t data_usi, const uint8_t *p_data)
{
    (void)net; (void)arg; (void)p_data;
    printf("[IO-DEVICE] Alarm indication (AREP=%u, slot=%u, subslot=%u, "
           "type=%u, data_usi=0x%04x, data_len=%u)\n",
           (unsigned)arep,
           p_alarm_argument ? (unsigned)p_alarm_argument->slot_nbr : 0,
           p_alarm_argument ? (unsigned)p_alarm_argument->subslot_nbr : 0,
           p_alarm_argument ? (unsigned)p_alarm_argument->alarm_type : 0,
           (unsigned)data_usi, (unsigned)data_len);
    return 0;
}

static int app_alarm_cnf_cb(
    pnet_t *net, void *arg, uint32_t arep,
    const pnet_pnio_status_t *p_pnio_status)
{
    (void)net; (void)arg; (void)p_pnio_status;
    printf("[IO-DEVICE] Alarm confirmation (AREP=%u)\n", (unsigned)arep);
    return 0;
}

static int app_alarm_ack_cnf_cb(
    pnet_t *net, void *arg, uint32_t arep, int res)
{
    (void)net; (void)arg;
    printf("[IO-DEVICE] Alarm ACK confirmation (AREP=%u, result=%d)\n",
           (unsigned)arep, res);
    return 0;
}

static int app_reset_ind_cb(
    pnet_t *net, void *arg, bool should_reset_application, uint16_t reset_mode)
{
    (void)net; (void)arg;
    printf("[IO-DEVICE] Reset indication (reset_app=%d, mode=%u)\n",
           (int)should_reset_application, (unsigned)reset_mode);
    return 0;
}

static int app_signal_led_ind_cb(pnet_t *net, void *arg, bool led_state)
{
    (void)net; (void)arg;
    printf("[IO-DEVICE] Signal LED: %s\n", led_state ? "ON" : "OFF");
    return 0;
}

static int app_sm_released_ind_cb(
    pnet_t *net, void *arg, uint32_t arep,
    uint32_t api, uint16_t slot, uint16_t subslot,
    pnet_result_t *p_result)
{
    (void)net; (void)arg; (void)p_result;
    printf("[IO-DEVICE] Submodule released (AREP=%u, api=%u, "
           "slot=%u, subslot=%u)\n",
           (unsigned)arep, (unsigned)api,
           (unsigned)slot, (unsigned)subslot);
    return 0;
}

#else /* Mock p-net callbacks (simplified signatures) */

static int app_state_ind_cb(
    pnet_t *net, void *arg, uint32_t arep, pnet_event_values_t state)
{
    io_device_t *dev = (io_device_t *)arg;
    (void)net;

    printf("[IO-DEVICE] State event: %s (AREP=%u)\n",
           event_name(state), (unsigned)arep);

    if (state == PNET_EVENT_STARTUP)
    {
        dev->active_arep = arep;
    }

    if (state == PNET_EVENT_DATA)
    {
        dev->state = IO_DEVICE_STATE_RUNNING;
    }
    else if (state == PNET_EVENT_ABORT)
    {
        dev->state = IO_DEVICE_STATE_CONNECTED;
        dev->active_arep = 0;
    }

    return 0;
}

static int app_connect_ind_cb(
    pnet_t *net, void *arg, uint32_t arep, pnet_pnio_status_t *result)
{
    (void)net; (void)arg; (void)result;
    printf("[IO-DEVICE] Connect indication (AREP=%u)\n", (unsigned)arep);
    return 0;
}

static int app_release_ind_cb(
    pnet_t *net, void *arg, uint32_t arep, pnet_pnio_status_t *result)
{
    (void)net; (void)arg; (void)result;
    printf("[IO-DEVICE] Release indication (AREP=%u)\n", (unsigned)arep);
    return 0;
}

static int app_dcontrol_ind_cb(
    pnet_t *net, void *arg, uint32_t arep,
    uint8_t control_command, pnet_pnio_status_t *result)
{
    (void)net; (void)arg; (void)result;
    printf("[IO-DEVICE] DControl indication (AREP=%u, cmd=%u)\n",
           (unsigned)arep, (unsigned)control_command);
    return 0;
}

static int app_ccontrol_cnf_cb(
    pnet_t *net, void *arg, uint32_t arep,
    uint8_t control_command, pnet_pnio_status_t *result)
{
    (void)net; (void)arg; (void)result;
    printf("[IO-DEVICE] CControl confirmation (AREP=%u, cmd=%u)\n",
           (unsigned)arep, (unsigned)control_command);
    return 0;
}

static int app_write_ind_cb(
    pnet_t *net, void *arg, uint32_t arep,
    uint32_t api, uint16_t slot, uint16_t subslot,
    uint16_t index, uint16_t record_length,
    const uint8_t *data, pnet_pnio_status_t *result)
{
    (void)net; (void)arg; (void)api; (void)data; (void)result;
    printf("[IO-DEVICE] Write indication (AREP=%u, slot=%u, subslot=%u, "
           "idx=0x%04x, len=%u)\n",
           (unsigned)arep, (unsigned)slot, (unsigned)subslot,
           (unsigned)index, (unsigned)record_length);
    return 0;
}

static int app_read_ind_cb(
    pnet_t *net, void *arg, uint32_t arep,
    uint32_t api, uint16_t slot, uint16_t subslot,
    uint16_t index, uint16_t *record_length,
    uint8_t **data, pnet_pnio_status_t *result)
{
    (void)net; (void)arg; (void)api; (void)data;
    (void)record_length; (void)result;
    printf("[IO-DEVICE] Read indication (AREP=%u, slot=%u, subslot=%u, "
           "idx=0x%04x)\n",
           (unsigned)arep, (unsigned)slot, (unsigned)subslot,
           (unsigned)index);
    return 0;
}

static int app_exp_module_ind_cb(
    pnet_t *net, void *arg, uint32_t api,
    uint16_t slot, uint32_t module_ident)
{
    (void)net; (void)arg;
    printf("[IO-DEVICE] Expected module indication "
           "(api=%u, slot=%u, module_ident=0x%08x)\n",
           (unsigned)api, (unsigned)slot, (unsigned)module_ident);
    return 0;
}

static int app_exp_submodule_ind_cb(
    pnet_t *net, void *arg, uint32_t api,
    uint16_t slot, uint16_t subslot,
    uint32_t module_ident, uint32_t submodule_ident,
    const pnet_submodule_dir_t *direction)
{
    (void)net; (void)arg; (void)direction;
    printf("[IO-DEVICE] Expected submodule indication "
           "(api=%u, slot=%u, subslot=%u, mod=0x%08x, submod=0x%08x)\n",
           (unsigned)api, (unsigned)slot, (unsigned)subslot,
           (unsigned)module_ident, (unsigned)submodule_ident);
    return 0;
}

static int app_new_data_status_ind_cb(
    pnet_t *net, void *arg, uint32_t arep, uint8_t changes)
{
    (void)net; (void)arg;
    printf("[IO-DEVICE] New data status (AREP=%u, changes=0x%02x)\n",
           (unsigned)arep, (unsigned)changes);
    return 0;
}

static int app_alarm_ind_cb(
    pnet_t *net, void *arg, uint32_t arep,
    uint32_t api, uint16_t slot, uint16_t subslot,
    uint16_t data_len, const uint8_t *data)
{
    (void)net; (void)arg; (void)api; (void)data;
    printf("[IO-DEVICE] Alarm indication (AREP=%u, slot=%u, subslot=%u, "
           "data_len=%u)\n",
           (unsigned)arep, (unsigned)slot, (unsigned)subslot,
           (unsigned)data_len);
    return 0;
}

static int app_alarm_cnf_cb(
    pnet_t *net, void *arg, uint32_t arep, pnet_pnio_status_t *result)
{
    (void)net; (void)arg; (void)result;
    printf("[IO-DEVICE] Alarm confirmation (AREP=%u)\n", (unsigned)arep);
    return 0;
}

static int app_alarm_ack_cnf_cb(pnet_t *net, void *arg, uint32_t arep)
{
    (void)net; (void)arg;
    printf("[IO-DEVICE] Alarm ACK confirmation (AREP=%u)\n", (unsigned)arep);
    return 0;
}

static int app_reset_ind_cb(
    pnet_t *net, void *arg, bool should_reset)
{
    (void)net; (void)arg;
    printf("[IO-DEVICE] Reset indication (should_reset=%d)\n",
           (int)should_reset);
    return 0;
}

static int app_signal_led_ind_cb(pnet_t *net, void *arg)
{
    (void)net; (void)arg;
    printf("[IO-DEVICE] Signal LED callback\n");
    return 0;
}

static int app_sm_released_ind_cb(
    pnet_t *net, void *arg, uint32_t arep,
    uint32_t api, uint16_t slot, uint16_t subslot)
{
    (void)net; (void)arg;
    printf("[IO-DEVICE] Submodule released (AREP=%u, api=%u, "
           "slot=%u, subslot=%u)\n",
           (unsigned)arep, (unsigned)api,
           (unsigned)slot, (unsigned)subslot);
    return 0;
}

#endif /* USE_REAL_PNET */

/* ======================================================================
 * Private helper: plug modules and submodules
 *
 * This plugs the mandatory DAP (Device Access Point) module in slot 0
 * with its standard submodules, plus an example user I/O module in
 * slot 1 with input and output submodules.
 *
 * The DAP submodules are required by the Profinet specification:
 *   - Whole module (subslot 0x0000): represents the device itself
 *   - Interface (subslot 0x8000): network interface properties
 *   - Port 1 (subslot 0x8001): physical port properties
 * ====================================================================== */
static int plug_all_modules(pnet_t *net)
{
    int ret;

    /* ----- DAP module (slot 0) ----- */
    ret = pnet_plug_module(net, PNET_API_NO_APPLICATION_PROFILE,
                           PNET_SLOT_DAP_IDENT, PNET_MOD_DAP_IDENT);
    if (ret != 0)
    {
        printf("[IO-DEVICE] ERROR: Failed to plug DAP module\n");
        return ret;
    }

    /* DAP subslot 0x0000: Whole module (no I/O data, just identity) */
    ret = pnet_plug_submodule(net, PNET_API_NO_APPLICATION_PROFILE,
                              PNET_SLOT_DAP_IDENT,
                              PNET_SUBSLOT_DAP_WHOLE_MODULE,
                              PNET_MOD_DAP_IDENT, PNET_SUBMOD_DAP_IDENT,
                              PNET_DIR_NO_IO, 0, 0);
    if (ret != 0)
    {
        printf("[IO-DEVICE] ERROR: Failed to plug DAP whole module\n");
        return ret;
    }

    /* DAP subslot 0x8000: Interface submodule (network interface) */
    ret = pnet_plug_submodule(net, PNET_API_NO_APPLICATION_PROFILE,
                              PNET_SLOT_DAP_IDENT,
                              PNET_SUBSLOT_DAP_INTERFACE_1_IDENT,
                              PNET_MOD_DAP_IDENT,
                              PNET_SUBMOD_DAP_INTERFACE_1_IDENT,
                              PNET_DIR_NO_IO, 0, 0);
    if (ret != 0)
    {
        printf("[IO-DEVICE] ERROR: Failed to plug DAP interface submodule\n");
        return ret;
    }

    /* DAP subslot 0x8001: Port 1 submodule (physical port) */
    ret = pnet_plug_submodule(net, PNET_API_NO_APPLICATION_PROFILE,
                              PNET_SLOT_DAP_IDENT,
                              PNET_SUBSLOT_DAP_INTERFACE_1_PORT_1_IDENT,
                              PNET_MOD_DAP_IDENT,
                              PNET_SUBMOD_DAP_INTERFACE_1_PORT_1_IDENT,
                              PNET_DIR_NO_IO, 0, 0);
    if (ret != 0)
    {
        printf("[IO-DEVICE] ERROR: Failed to plug DAP port 1 submodule\n");
        return ret;
    }

    /* ----- User module (slot 1): example digital I/O ----- */
    ret = pnet_plug_module(net, EXAMPLE_API,
                           EXAMPLE_SLOT, EXAMPLE_MODULE_IDENT);
    if (ret != 0)
    {
        printf("[IO-DEVICE] ERROR: Failed to plug user module at slot %u\n",
               (unsigned)EXAMPLE_SLOT);
        return ret;
    }

    /* Input submodule: sends process data to the controller */
    ret = pnet_plug_submodule(net, EXAMPLE_API,
                              EXAMPLE_SLOT, EXAMPLE_SUBSLOT_INPUT,
                              EXAMPLE_MODULE_IDENT, EXAMPLE_SUBMOD_INPUT,
                              PNET_DIR_INPUT, EXAMPLE_INPUT_SIZE, 0);
    if (ret != 0)
    {
        printf("[IO-DEVICE] ERROR: Failed to plug input submodule\n");
        return ret;
    }

    /* Output submodule: receives process data from the controller */
    ret = pnet_plug_submodule(net, EXAMPLE_API,
                              EXAMPLE_SLOT, EXAMPLE_SUBSLOT_OUTPUT,
                              EXAMPLE_MODULE_IDENT, EXAMPLE_SUBMOD_OUTPUT,
                              PNET_DIR_OUTPUT, 0, EXAMPLE_OUTPUT_SIZE);
    if (ret != 0)
    {
        printf("[IO-DEVICE] ERROR: Failed to plug output submodule\n");
        return ret;
    }

    printf("[IO-DEVICE] All modules plugged successfully\n");
    return 0;
}

/* ======================================================================
 * Public API: Lifecycle
 * ====================================================================== */

int io_device_init(io_device_t *dev, const io_device_cfg_t *cfg)
{
    if (!dev || !cfg)
    {
        printf("[IO-DEVICE] ERROR: NULL pointer in init\n");
        return -1;
    }

    /* Copy configuration into the device handle */
    memcpy(&dev->config, cfg, sizeof(io_device_cfg_t));

    /* Set initial state */
    dev->state        = IO_DEVICE_STATE_INITIALIZED;
    dev->pnet_handle  = NULL;
    dev->active_arep  = 0;
    dev->cycle_count  = 0;
    dev->error_count  = 0;

    /* Default I/O sizes from the example module definition */
    dev->input_size   = EXAMPLE_INPUT_SIZE;
    dev->output_size  = EXAMPLE_OUTPUT_SIZE;
    memset(dev->input_data, 0, sizeof(dev->input_data));
    memset(dev->output_data, 0, sizeof(dev->output_data));

    printf("[IO-DEVICE] Initialized: product=\"%s\", station=\"%s\"\n",
           dev->config.product_name, dev->config.station_name);
    return 0;
}

int io_device_start(io_device_t *dev)
{
    if (!dev)
    {
        return -1;
    }

    if (dev->state != IO_DEVICE_STATE_INITIALIZED)
    {
        printf("[IO-DEVICE] ERROR: Cannot start, state=%s\n",
               io_device_state_str(dev->state));
        return -1;
    }

    /* Build the p-net configuration structure */
    pnet_cfg_t pnet_cfg;
    memset(&pnet_cfg, 0, sizeof(pnet_cfg));

    /* Basic settings */
    pnet_cfg.tick_us = dev->config.tick_us;
    safe_strncpy(pnet_cfg.product_name, dev->config.product_name,
                 sizeof(pnet_cfg.product_name));
    safe_strncpy(pnet_cfg.station_name, dev->config.station_name,
                 sizeof(pnet_cfg.station_name));

    /* Device identity from vendor_id / device_id.
     * Note: the real p-net API uses _hi/_lo suffixes while the mock
     * uses _high/_low. We handle both with a compile-time switch. */
#ifdef USE_REAL_PNET
    pnet_cfg.device_id.vendor_id_hi =
        (uint8_t)(dev->config.vendor_id >> 8);
    pnet_cfg.device_id.vendor_id_lo =
        (uint8_t)(dev->config.vendor_id & 0xFF);
    pnet_cfg.device_id.device_id_hi =
        (uint8_t)(dev->config.device_id >> 8);
    pnet_cfg.device_id.device_id_lo =
        (uint8_t)(dev->config.device_id & 0xFF);
#else
    pnet_cfg.device_id.vendor_id_high =
        (uint8_t)(dev->config.vendor_id >> 8);
    pnet_cfg.device_id.vendor_id_low =
        (uint8_t)(dev->config.vendor_id & 0xFF);
    pnet_cfg.device_id.device_id_high =
        (uint8_t)(dev->config.device_id >> 8);
    pnet_cfg.device_id.device_id_low =
        (uint8_t)(dev->config.device_id & 0xFF);
#endif

    /* OEM identity (same as device identity for this example) */
    pnet_cfg.oem_device_id = pnet_cfg.device_id;

    /* I&M0 data: mandatory identification record.
     * im_0_data field names are the same in both APIs. */
#ifdef USE_REAL_PNET
    pnet_cfg.im_0_data.im_vendor_id_hi = pnet_cfg.device_id.vendor_id_hi;
    pnet_cfg.im_0_data.im_vendor_id_lo = pnet_cfg.device_id.vendor_id_lo;
#else
    pnet_cfg.im_0_data.im_vendor_id_hi = pnet_cfg.device_id.vendor_id_high;
    pnet_cfg.im_0_data.im_vendor_id_lo = pnet_cfg.device_id.vendor_id_low;
#endif
    pnet_cfg.im_0_data.im_hardware_revision = 1;
    pnet_cfg.im_0_data.im_sw_revision_prefix = 'V';
    pnet_cfg.im_0_data.im_sw_revision_functional_enhancement = 0;
    pnet_cfg.im_0_data.im_sw_revision_bug_fix = 1;
    pnet_cfg.im_0_data.im_sw_revision_internal_change = 0;
    pnet_cfg.im_0_data.im_revision_counter = 0;
    pnet_cfg.im_0_data.im_profile_id = 0;
    pnet_cfg.im_0_data.im_profile_specific_type = 0;
    pnet_cfg.im_0_data.im_version_major = 1;
    pnet_cfg.im_0_data.im_version_minor = 0;
    pnet_cfg.im_0_data.im_supported = PNET_SUPPORTED_IM1;

    safe_strncpy(pnet_cfg.im_0_data.im_order_id, "PNET-IO-DEVICE",
                 sizeof(pnet_cfg.im_0_data.im_order_id));
    safe_strncpy(pnet_cfg.im_0_data.im_serial_number, "00001",
                 sizeof(pnet_cfg.im_0_data.im_serial_number));

    /* Minimum allowed data exchange interval in units of 31.25 us.
     * 32 * 31.25 us = 1 ms, a typical minimum cycle time. */
    pnet_cfg.min_device_interval = 32;

    /* Network / platform configuration */
    safe_strncpy(pnet_cfg.pnal_cfg.if_name, dev->config.interface_name,
                 sizeof(pnet_cfg.pnal_cfg.if_name));
    pnet_cfg.pnal_cfg.ip_addr  = dev->config.ip_addr;
    pnet_cfg.pnal_cfg.netmask  = dev->config.netmask;
    pnet_cfg.pnal_cfg.gateway  = dev->config.gateway;
    pnet_cfg.pnal_cfg.use_dhcp = false;

#ifdef USE_REAL_PNET
    /* Real p-net uses a richer interface config structure */
    pnet_cfg.if_cfg.main_netif_name = dev->config.interface_name;
    pnet_cfg.if_cfg.ip_cfg.ip_addr.a =
        (uint8_t)((dev->config.ip_addr >> 24) & 0xFF);
    pnet_cfg.if_cfg.ip_cfg.ip_addr.b =
        (uint8_t)((dev->config.ip_addr >> 16) & 0xFF);
    pnet_cfg.if_cfg.ip_cfg.ip_addr.c =
        (uint8_t)((dev->config.ip_addr >> 8) & 0xFF);
    pnet_cfg.if_cfg.ip_cfg.ip_addr.d =
        (uint8_t)(dev->config.ip_addr & 0xFF);
    pnet_cfg.if_cfg.ip_cfg.ip_mask.a =
        (uint8_t)((dev->config.netmask >> 24) & 0xFF);
    pnet_cfg.if_cfg.ip_cfg.ip_mask.b =
        (uint8_t)((dev->config.netmask >> 16) & 0xFF);
    pnet_cfg.if_cfg.ip_cfg.ip_mask.c =
        (uint8_t)((dev->config.netmask >> 8) & 0xFF);
    pnet_cfg.if_cfg.ip_cfg.ip_mask.d =
        (uint8_t)(dev->config.netmask & 0xFF);
    pnet_cfg.if_cfg.ip_cfg.ip_gateway.a =
        (uint8_t)((dev->config.gateway >> 24) & 0xFF);
    pnet_cfg.if_cfg.ip_cfg.ip_gateway.b =
        (uint8_t)((dev->config.gateway >> 16) & 0xFF);
    pnet_cfg.if_cfg.ip_cfg.ip_gateway.c =
        (uint8_t)((dev->config.gateway >> 8) & 0xFF);
    pnet_cfg.if_cfg.ip_cfg.ip_gateway.d =
        (uint8_t)(dev->config.gateway & 0xFF);
    pnet_cfg.if_cfg.ip_cfg.dhcp_enable = false;
    pnet_cfg.num_physical_ports = 1;
#else
    /* Mock: simpler if_cfg with just a char array */
    safe_strncpy(pnet_cfg.if_cfg.netif_name, dev->config.interface_name,
                 sizeof(pnet_cfg.if_cfg.netif_name));
    memcpy(pnet_cfg.if_cfg.eth_addr, dev->config.mac_addr, 6);
    pnet_cfg.num_physical_ports = 1;
#endif

    pnet_cfg.send_hello            = dev->config.send_hello;
    pnet_cfg.use_qualified_diagnosis = false;

    /* File storage directory */
    safe_strncpy(pnet_cfg.file_directory, dev->config.file_directory,
                 sizeof(pnet_cfg.file_directory));

    /* Register all callback functions.
     * cb_arg is passed to every callback as the 'arg' parameter. */
    pnet_cfg.cb_arg              = dev;
    pnet_cfg.state_cb            = app_state_ind_cb;
    pnet_cfg.connect_cb          = app_connect_ind_cb;
    pnet_cfg.release_cb          = app_release_ind_cb;
    pnet_cfg.dcontrol_cb         = app_dcontrol_ind_cb;
    pnet_cfg.ccontrol_cb         = app_ccontrol_cnf_cb;
    pnet_cfg.write_cb            = app_write_ind_cb;
    pnet_cfg.read_cb             = app_read_ind_cb;
    pnet_cfg.exp_module_cb       = app_exp_module_ind_cb;
    pnet_cfg.exp_submodule_cb    = app_exp_submodule_ind_cb;
    pnet_cfg.new_data_status_cb  = app_new_data_status_ind_cb;
    pnet_cfg.alarm_ind_cb        = app_alarm_ind_cb;
    pnet_cfg.alarm_cnf_cb        = app_alarm_cnf_cb;
    pnet_cfg.alarm_ack_cnf_cb    = app_alarm_ack_cnf_cb;
    pnet_cfg.reset_cb            = app_reset_ind_cb;
    pnet_cfg.signal_led_cb       = app_signal_led_ind_cb;
    pnet_cfg.sm_released_cb      = app_sm_released_ind_cb;

    /* Initialize the p-net stack */
    printf("[IO-DEVICE] Starting p-net stack...\n");
    dev->pnet_handle = pnet_init(&pnet_cfg);
    if (!dev->pnet_handle)
    {
        printf("[IO-DEVICE] ERROR: pnet_init() failed\n");
        dev->state = IO_DEVICE_STATE_ERROR;
        return -1;
    }

    /* Plug DAP and user modules/submodules */
    if (plug_all_modules(dev->pnet_handle) != 0)
    {
        printf("[IO-DEVICE] ERROR: Failed to plug modules\n");
        dev->state = IO_DEVICE_STATE_ERROR;
        return -1;
    }

    dev->state = IO_DEVICE_STATE_CONNECTED;
    printf("[IO-DEVICE] Started successfully (product=\"%s\", station=\"%s\")\n",
           dev->config.product_name, dev->config.station_name);
    return 0;
}

int io_device_tick(io_device_t *dev)
{
    if (!dev || !dev->pnet_handle)
    {
        return -1;
    }

    if (dev->state == IO_DEVICE_STATE_ERROR)
    {
        return -1;
    }

    /* Let the p-net stack process pending network events, timeouts,
     * LLDP transmissions, and cyclic data exchange. */
    pnet_handle_periodic(dev->pnet_handle);

    dev->cycle_count++;
    return 0;
}

int io_device_stop(io_device_t *dev)
{
    if (!dev || !dev->pnet_handle)
    {
        return -1;
    }

    printf("[IO-DEVICE] Stopping device...\n");

    /* Factory reset clears all p-net internal state and data files.
     * This is the recommended way to cleanly shut down a device. */
    int ret = pnet_factory_reset(dev->pnet_handle);
    if (ret != 0)
    {
        printf("[IO-DEVICE] WARNING: pnet_factory_reset() returned %d\n", ret);
    }

    /* Clean up resources */
    io_device_cleanup(dev);

    printf("[IO-DEVICE] Stopped\n");
    return 0;
}

void io_device_cleanup(io_device_t *dev)
{
    if (!dev)
    {
        return;
    }

    /* The p-net stack does not expose a dedicated destroy/free function.
     * After pnet_factory_reset() the handle is still valid but the stack
     * is in a clean state. We null out the handle to prevent further use. */
    dev->pnet_handle = NULL;

    /* Zero out the entire struct for a clean slate */
    memset(dev, 0, sizeof(io_device_t));
    dev->state = IO_DEVICE_STATE_IDLE;

    printf("[IO-DEVICE] Cleaned up\n");
}

/* ======================================================================
 * Public API: Data exchange
 * ====================================================================== */

int io_device_update_input(io_device_t *dev, const uint8_t *data, uint16_t len)
{
    if (!dev || !dev->pnet_handle || !data)
    {
        return -1;
    }

    if (len > sizeof(dev->input_data))
    {
        printf("[IO-DEVICE] ERROR: input data length %u exceeds buffer size %zu\n",
               (unsigned)len, sizeof(dev->input_data));
        return -1;
    }

    /* Cache the data locally */
    memcpy(dev->input_data, data, len);
    dev->input_size = len;

    /* Push data and IOPS=GOOD to the p-net stack for transmission to PLC.
     * The mock API takes 6 args; the real API takes 7 (includes data_len). */
#ifdef USE_REAL_PNET
    return pnet_input_set_data_and_iops(
        dev->pnet_handle, EXAMPLE_API, EXAMPLE_SLOT, EXAMPLE_SUBSLOT_INPUT,
        data, len, PNET_IOXS_GOOD);
#else
    return pnet_input_set_data_and_iops(
        dev->pnet_handle, EXAMPLE_API, EXAMPLE_SLOT, EXAMPLE_SUBSLOT_INPUT,
        data, PNET_IOXS_GOOD);
#endif
}

int io_device_read_output(io_device_t *dev, uint8_t *data, uint16_t *len)
{
    if (!dev || !dev->pnet_handle || !data || !len)
    {
        return -1;
    }

    pnet_ioxs_values_t iops = PNET_IOXS_BAD;
    uint16_t data_len = *len;
    int ret;

    /* Read latest output data and provider status from the p-net stack.
     * The mock API takes 7 args; the real API takes 8 (includes p_new_flag). */
#ifdef USE_REAL_PNET
    bool new_flag = false;
    ret = pnet_output_get_data_and_iops(
        dev->pnet_handle, EXAMPLE_API, EXAMPLE_SLOT, EXAMPLE_SUBSLOT_OUTPUT,
        &new_flag, data, &data_len, (uint8_t *)&iops);
#else
    ret = pnet_output_get_data_and_iops(
        dev->pnet_handle, EXAMPLE_API, EXAMPLE_SLOT, EXAMPLE_SUBSLOT_OUTPUT,
        data, &data_len, &iops);
#endif

    if (ret == 0)
    {
        /* Cache locally for inspection */
        memcpy(dev->output_data, data, data_len);
        dev->output_size = data_len;
        *len = data_len;
    }

    return ret;
}

/* ======================================================================
 * Public API: Status and diagnostics
 * ====================================================================== */

io_device_state_t io_device_get_state(const io_device_t *dev)
{
    if (!dev)
    {
        return IO_DEVICE_STATE_ERROR;
    }
    return dev->state;
}

const char *io_device_state_str(io_device_state_t state)
{
    switch (state)
    {
    case IO_DEVICE_STATE_IDLE:        return "IDLE";
    case IO_DEVICE_STATE_INITIALIZED: return "INITIALIZED";
    case IO_DEVICE_STATE_CONNECTED:   return "CONNECTED";
    case IO_DEVICE_STATE_RUNNING:     return "RUNNING";
    case IO_DEVICE_STATE_ERROR:       return "ERROR";
    default:                          return "UNKNOWN";
    }
}

void io_device_show(const io_device_t *dev)
{
    if (!dev)
    {
        printf("IO Device: (null)\n");
        return;
    }

    printf("=== IO Device Status ===\n");
    printf("  Product name : %s\n", dev->config.product_name);
    printf("  Station name : %s\n", dev->config.station_name);
    printf("  Interface    : %s\n", dev->config.interface_name);
    printf("  MAC          : %02x:%02x:%02x:%02x:%02x:%02x\n",
           dev->config.mac_addr[0], dev->config.mac_addr[1],
           dev->config.mac_addr[2], dev->config.mac_addr[3],
           dev->config.mac_addr[4], dev->config.mac_addr[5]);
    printf("  IP address   : %u.%u.%u.%u\n",
           (dev->config.ip_addr >> 24) & 0xFF,
           (dev->config.ip_addr >> 16) & 0xFF,
           (dev->config.ip_addr >> 8) & 0xFF,
           dev->config.ip_addr & 0xFF);
    printf("  Vendor ID    : 0x%04x\n", (unsigned)dev->config.vendor_id);
    printf("  Device ID    : 0x%04x\n", (unsigned)dev->config.device_id);
    printf("  State        : %s\n", io_device_state_str(dev->state));
    printf("  Cycle count  : %llu\n",
           (unsigned long long)dev->cycle_count);
    printf("  Error count  : %llu\n",
           (unsigned long long)dev->error_count);
    printf("  Active AREP  : %u\n", (unsigned)dev->active_arep);
    printf("  Input size   : %u bytes\n", (unsigned)dev->input_size);
    printf("  Output size  : %u bytes\n", (unsigned)dev->output_size);
    printf("  p-net handle : %s\n",
           dev->pnet_handle ? "active" : "null");

    /* Delegate to the p-net stack for its internal state dump */
    if (dev->pnet_handle)
    {
        printf("\n--- p-net stack internals ---\n");
        pnet_show(dev->pnet_handle, 1);
    }
}
