/**
 * pnet_api_mock.h - Mock p-net API for testing
 *
 * Provides the same interface as vendor/p-net/include/pnet_api.h
 * but with stub implementations for testing on non-Linux platforms.
 */

#ifndef PNET_API_MOCK_H
#define PNET_API_MOCK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Limits */
#define PNET_PRODUCT_NAME_MAX_LEN   25
#define PNET_ORDER_ID_MAX_LEN       20
#define PNET_SERIAL_NUMBER_MAX_LEN  16
#define PNET_STATION_NAME_MAX_SIZE  241
#define PNET_MAX_DIRECTORYPATH_SIZE 240
#define PNET_INTERFACE_NAME_MAX_SIZE 16

/* DAP slot/subslot identifiers */
#define PNET_SLOT_DAP_IDENT                       0x00000000
#define PNET_SUBSLOT_DAP_WHOLE_MODULE             0x00000000
#define PNET_SUBSLOT_DAP_IDENT                    0x00000001
#define PNET_SUBSLOT_DAP_INTERFACE_1_IDENT        0x00008000
#define PNET_SUBSLOT_DAP_INTERFACE_1_PORT_1_IDENT 0x00008001

#define PNET_MOD_DAP_IDENT                       0x00000001
#define PNET_SUBMOD_DAP_IDENT                    0x00000001
#define PNET_SUBMOD_DAP_INTERFACE_1_IDENT        0x00008000

#define PNET_API_NO_APPLICATION_PROFILE 0

/* Events */
typedef enum pnet_event_values {
    PNET_EVENT_ABORT = 0,
    PNET_EVENT_STARTUP,
    PNET_EVENT_PRMEND,
    PNET_EVENT_APPLRDY,
    PNET_EVENT_DATA
} pnet_event_values_t;

/* IOCS/IOPS values */
typedef enum pnet_ioxs_values {
    PNET_IOXS_BAD = 0x00,
    PNET_IOXS_GOOD = 0x80
} pnet_ioxs_values_t;

/* Submodule direction */
typedef enum pnet_submodule_dir {
    PNET_DIR_NO_IO = 0,
    PNET_DIR_INPUT = 0x20,
    PNET_DIR_OUTPUT = 0x40,
    PNET_DIR_IO = 0x60
} pnet_submodule_dir_t;

/* PNIO status */
typedef struct pnet_pnio_status {
    uint8_t error_code;
    uint8_t error_decode;
    uint8_t error_code_1;
    uint8_t error_code_2;
} pnet_pnio_status_t;

/* Device ID */
typedef struct pnet_cfg_device_id {
    uint8_t vendor_id_high;
    uint8_t vendor_id_low;
    uint8_t device_id_high;
    uint8_t device_id_low;
} pnet_cfg_device_id_t;

/* I&M data structures */
typedef struct pnet_im_0 {
    uint8_t  im_vendor_id_hi;
    uint8_t  im_vendor_id_lo;
    char     im_order_id[PNET_ORDER_ID_MAX_LEN + 1];
    char     im_serial_number[PNET_SERIAL_NUMBER_MAX_LEN + 1];
    uint16_t im_hardware_revision;
    uint8_t  im_sw_revision_prefix;
    uint8_t  im_sw_revision_functional_enhancement;
    uint8_t  im_sw_revision_bug_fix;
    uint8_t  im_sw_revision_internal_change;
    uint16_t im_revision_counter;
    uint16_t im_profile_id;
    uint16_t im_profile_specific_type;
    uint16_t im_version_major;
    uint16_t im_version_minor;
    uint16_t im_supported;
} pnet_im_0_t;

typedef struct pnet_im_1 {
    char im_tag_function[33];
    char im_tag_location[23];
} pnet_im_1_t;

typedef struct pnet_im_2 {
    char im_date[17];
} pnet_im_2_t;

typedef struct pnet_im_3 {
    char im_descriptor[55];
} pnet_im_3_t;

typedef struct pnet_im_4 {
    char im_signature[54];
} pnet_im_4_t;

/* Network interface config */
typedef struct pnet_if_cfg {
    char netif_name[PNET_INTERFACE_NAME_MAX_SIZE];
    uint8_t eth_addr[6];
} pnet_if_cfg_t;

/* OS/platform config (simplified) */
typedef struct pnal_cfg {
    char if_name[PNET_INTERFACE_NAME_MAX_SIZE];
    bool use_dhcp;
    uint32_t ip_addr;
    uint32_t netmask;
    uint32_t gateway;
} pnal_cfg_t;

/* Opaque handle */
typedef struct pnet pnet_t;

/* Callback prototypes */
typedef int (*pnet_state_ind)(pnet_t *net, void *arg, uint32_t arep,
                              pnet_event_values_t state);
typedef int (*pnet_connect_ind)(pnet_t *net, void *arg, uint32_t arep,
                                pnet_pnio_status_t *result);
typedef int (*pnet_release_ind)(pnet_t *net, void *arg, uint32_t arep,
                                pnet_pnio_status_t *result);
typedef int (*pnet_dcontrol_ind)(pnet_t *net, void *arg, uint32_t arep,
                                 uint8_t control_command,
                                 pnet_pnio_status_t *result);
typedef int (*pnet_ccontrol_cnf)(pnet_t *net, void *arg, uint32_t arep,
                                 uint8_t control_command,
                                 pnet_pnio_status_t *result);
typedef int (*pnet_write_ind)(pnet_t *net, void *arg, uint32_t arep,
                              uint32_t api, uint16_t slot, uint16_t subslot,
                              uint16_t index, uint16_t record_length,
                              const uint8_t *data, pnet_pnio_status_t *result);
typedef int (*pnet_read_ind)(pnet_t *net, void *arg, uint32_t arep,
                             uint32_t api, uint16_t slot, uint16_t subslot,
                             uint16_t index, uint16_t *record_length,
                             uint8_t **data, pnet_pnio_status_t *result);
typedef int (*pnet_exp_module_ind)(pnet_t *net, void *arg, uint32_t api,
                                   uint16_t slot, uint32_t module_ident);
typedef int (*pnet_exp_submodule_ind)(pnet_t *net, void *arg, uint32_t api,
                                      uint16_t slot, uint16_t subslot,
                                      uint32_t module_ident,
                                      uint32_t submodule_ident,
                                      const pnet_submodule_dir_t *direction);
typedef int (*pnet_new_data_status_ind)(pnet_t *net, void *arg, uint32_t arep,
                                        uint8_t changes);
typedef int (*pnet_alarm_ind)(pnet_t *net, void *arg, uint32_t arep,
                              uint32_t api, uint16_t slot, uint16_t subslot,
                              uint16_t data_len, const uint8_t *data);
typedef int (*pnet_alarm_cnf)(pnet_t *net, void *arg, uint32_t arep,
                              pnet_pnio_status_t *result);
typedef int (*pnet_alarm_ack_cnf)(pnet_t *net, void *arg, uint32_t arep);
typedef int (*pnet_reset_ind)(pnet_t *net, void *arg, bool should_reset);
typedef int (*pnet_signal_led_ind)(pnet_t *net, void *arg);
typedef int (*pnet_sm_released_ind)(pnet_t *net, void *arg, uint32_t arep,
                                    uint32_t api, uint16_t slot, uint16_t subslot);

/* Configuration */
typedef struct pnet_cfg {
    uint32_t tick_us;
    pnet_state_ind state_cb;
    pnet_connect_ind connect_cb;
    pnet_release_ind release_cb;
    pnet_dcontrol_ind dcontrol_cb;
    pnet_ccontrol_cnf ccontrol_cb;
    pnet_write_ind write_cb;
    pnet_read_ind read_cb;
    pnet_exp_module_ind exp_module_cb;
    pnet_exp_submodule_ind exp_submodule_cb;
    pnet_new_data_status_ind new_data_status_cb;
    pnet_alarm_ind alarm_ind_cb;
    pnet_alarm_cnf alarm_cnf_cb;
    pnet_alarm_ack_cnf alarm_ack_cnf_cb;
    pnet_reset_ind reset_cb;
    pnet_signal_led_ind signal_led_cb;
    pnet_sm_released_ind sm_released_cb;
    void *cb_arg;
    pnet_im_0_t im_0_data;
    pnet_im_1_t im_1_data;
    pnet_im_2_t im_2_data;
    pnet_im_3_t im_3_data;
    pnet_im_4_t im_4_data;
    pnet_cfg_device_id_t device_id;
    pnet_cfg_device_id_t oem_device_id;
    char station_name[PNET_STATION_NAME_MAX_SIZE];
    char product_name[PNET_PRODUCT_NAME_MAX_LEN + 1];
    uint16_t min_device_interval;
    pnal_cfg_t pnal_cfg;
    bool send_hello;
    uint8_t num_physical_ports;
    bool use_qualified_diagnosis;
    pnet_if_cfg_t if_cfg;
    char file_directory[PNET_MAX_DIRECTORYPATH_SIZE];
} pnet_cfg_t;

/* ===== API Functions (mock implementations in pnet_api_mock.c) ===== */

pnet_t *pnet_init(const pnet_cfg_t *cfg);
void pnet_handle_periodic(pnet_t *net);
int pnet_application_ready(pnet_t *net, uint32_t arep);
int pnet_plug_module(pnet_t *net, uint32_t api, uint16_t slot, uint32_t module_ident);
int pnet_plug_submodule(pnet_t *net, uint32_t api, uint16_t slot, uint16_t subslot,
                        uint32_t module_ident, uint32_t submodule_ident,
                        pnet_submodule_dir_t direction,
                        uint16_t length_input, uint16_t length_output);
int pnet_pull_module(pnet_t *net, uint32_t api, uint16_t slot);
int pnet_pull_submodule(pnet_t *net, uint32_t api, uint16_t slot, uint16_t subslot);
int pnet_input_set_data_and_iops(pnet_t *net, uint32_t api, uint16_t slot,
                                 uint16_t subslot, const uint8_t *data,
                                 uint8_t iops);
int pnet_input_get_iocs(pnet_t *net, uint32_t api, uint16_t slot,
                        uint16_t subslot, pnet_ioxs_values_t *iocs);
int pnet_output_get_data_and_iops(pnet_t *net, uint32_t api, uint16_t slot,
                                  uint16_t subslot, uint8_t *data,
                                  uint16_t *data_len, pnet_ioxs_values_t *iops);
int pnet_set_primary_state(pnet_t *net, bool primary);
int pnet_ar_abort(pnet_t *net, uint32_t arep);
int pnet_factory_reset(pnet_t *net);
void pnet_show(pnet_t *net, unsigned level);
int pnet_remove_data_files(const char *file_directory);

#endif /* PNET_API_MOCK_H */
