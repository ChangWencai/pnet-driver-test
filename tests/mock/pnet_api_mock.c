/**
 * pnet_api_mock.c - Mock implementation of p-net API
 *
 * Simulates p-net behavior for testing without real hardware/network.
 */

#include "pnet_api_mock.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MOCK_MAX_MODULES    16
#define MOCK_MAX_SUBMODULES 32

typedef struct mock_submodule {
    uint32_t api;
    uint16_t slot;
    uint16_t subslot;
    uint32_t module_ident;
    uint32_t submodule_ident;
    pnet_submodule_dir_t direction;
    uint16_t input_len;
    uint16_t output_len;
    uint8_t  input_data[256];
    uint8_t  output_data[256];
    pnet_ioxs_values_t input_iops;
    pnet_ioxs_values_t output_iops;
    bool plugged;
} mock_submodule_t;

struct pnet {
    pnet_cfg_t cfg;
    bool initialized;
    bool primary;
    uint32_t next_arep;
    uint32_t tick_count;
    mock_submodule_t submodules[MOCK_MAX_SUBMODULES];
    int submodule_count;
};

pnet_t *pnet_init(const pnet_cfg_t *cfg) {
    if (!cfg) return NULL;

    pnet_t *net = (pnet_t *)calloc(1, sizeof(pnet_t));
    if (!net) return NULL;

    memcpy(&net->cfg, cfg, sizeof(pnet_cfg_t));
    net->initialized = true;
    net->primary = true;
    net->next_arep = 1;
    net->tick_count = 0;

    return net;
}

void pnet_handle_periodic(pnet_t *net) {
    if (!net || !net->initialized) return;
    net->tick_count++;
}

int pnet_application_ready(pnet_t *net, uint32_t arep) {
    if (!net || !net->initialized) return -1;
    (void)arep;
    return 0;
}

int pnet_plug_module(pnet_t *net, uint32_t api, uint16_t slot, uint32_t module_ident) {
    if (!net || !net->initialized) return -1;
    (void)api;
    (void)slot;
    (void)module_ident;
    return 0;
}

int pnet_plug_submodule(pnet_t *net, uint32_t api, uint16_t slot, uint16_t subslot,
                        uint32_t module_ident, uint32_t submodule_ident,
                        pnet_submodule_dir_t direction,
                        uint16_t length_input, uint16_t length_output) {
    if (!net || !net->initialized) return -1;
    if (net->submodule_count >= MOCK_MAX_SUBMODULES) return -1;

    mock_submodule_t *sm = &net->submodules[net->submodule_count];
    sm->api = api;
    sm->slot = slot;
    sm->subslot = subslot;
    sm->module_ident = module_ident;
    sm->submodule_ident = submodule_ident;
    sm->direction = direction;
    sm->input_len = length_input;
    sm->output_len = length_output;
    sm->input_iops = PNET_IOXS_GOOD;
    sm->output_iops = PNET_IOXS_GOOD;
    sm->plugged = true;
    net->submodule_count++;

    return 0;
}

int pnet_pull_module(pnet_t *net, uint32_t api, uint16_t slot) {
    if (!net) return -1;
    (void)api; (void)slot;
    return 0;
}

int pnet_pull_submodule(pnet_t *net, uint32_t api, uint16_t slot, uint16_t subslot) {
    if (!net) return -1;

    for (int i = 0; i < net->submodule_count; i++) {
        if (net->submodules[i].api == api &&
            net->submodules[i].slot == slot &&
            net->submodules[i].subslot == subslot) {
            net->submodules[i].plugged = false;
            return 0;
        }
    }
    return -1;
}

int pnet_input_set_data_and_iops(pnet_t *net, uint32_t api, uint16_t slot,
                                 uint16_t subslot, const uint8_t *data,
                                 uint8_t iops) {
    if (!net || !data) return -1;

    for (int i = 0; i < net->submodule_count; i++) {
        mock_submodule_t *sm = &net->submodules[i];
        if (sm->api == api && sm->slot == slot && sm->subslot == subslot && sm->plugged) {
            size_t len = (sm->input_len < 256) ? sm->input_len : 256;
            memcpy(sm->input_data, data, len);
            sm->input_iops = (pnet_ioxs_values_t)iops;
            return 0;
        }
    }
    return -1;
}

int pnet_input_get_iocs(pnet_t *net, uint32_t api, uint16_t slot,
                        uint16_t subslot, pnet_ioxs_values_t *iocs) {
    if (!net || !iocs) return -1;
    (void)api; (void)slot; (void)subslot;
    *iocs = PNET_IOXS_GOOD;
    return 0;
}

int pnet_output_get_data_and_iops(pnet_t *net, uint32_t api, uint16_t slot,
                                  uint16_t subslot, uint8_t *data,
                                  uint16_t *data_len, pnet_ioxs_values_t *iops) {
    if (!net || !data || !data_len || !iops) return -1;

    for (int i = 0; i < net->submodule_count; i++) {
        mock_submodule_t *sm = &net->submodules[i];
        if (sm->api == api && sm->slot == slot && sm->subslot == subslot && sm->plugged) {
            size_t len = (sm->output_len < *data_len) ? sm->output_len : *data_len;
            memcpy(data, sm->output_data, len);
            *data_len = (uint16_t)len;
            *iops = sm->output_iops;
            return 0;
        }
    }
    return -1;
}

int pnet_set_primary_state(pnet_t *net, bool primary) {
    if (!net) return -1;
    net->primary = primary;
    return 0;
}

int pnet_ar_abort(pnet_t *net, uint32_t arep) {
    if (!net) return -1;
    (void)arep;
    return 0;
}

int pnet_factory_reset(pnet_t *net) {
    if (!net) return -1;
    return 0;
}

void pnet_show(pnet_t *net, unsigned level) {
    if (!net) {
        printf("p-net: not initialized\n");
        return;
    }
    if (level >= 1) {
        printf("p-net: initialized, product=%s, station=%s\n",
               net->cfg.product_name, net->cfg.station_name);
        printf("p-net: submodules=%d, tick_count=%u\n",
               net->submodule_count, net->tick_count);
    }
}

int pnet_remove_data_files(const char *file_directory) {
    (void)file_directory;
    return 0;
}
