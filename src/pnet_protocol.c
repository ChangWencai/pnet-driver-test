/**
 * pnet_protocol.c - Profinet Protocol Core Layer Implementation
 *
 * Implements Profinet frame construction, parsing, cyclic/acyclic data exchange,
 * and Communication Relationship (CR) management.
 */

#include "pnet_protocol.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* Global protocol statistics */
static pnet_protocol_stats_t g_proto_stats = {0};
static bool g_proto_initialized = false;

int pnet_protocol_init(void) {
    if (g_proto_initialized) return -1;

    memset(&g_proto_stats, 0, sizeof(g_proto_stats));
    g_proto_stats.min_latency_us = 1e9; /* Start with a very high min */
    g_proto_initialized = true;
    return 0;
}

void pnet_protocol_shutdown(void) {
    g_proto_initialized = false;
}

uint16_t pnet_frame_id(uint16_t high, uint16_t low) {
    return (uint16_t)((high << 8) | low);
}

int pnet_frame_build(pnet_frame_t *frame, const pnet_io_data_t *io_data) {
    if (!frame || !io_data) return -1;

    memset(frame, 0, sizeof(pnet_frame_t));

    /* Build Ethernet header */
    frame->eth_header.ethertype = __builtin_bswap16(PNET_ETHERTYPE_PROFINET);

    /* Build RT header */
    uint16_t fid = 0x8000; /* Default frame ID for RT class 1 */
    frame->rt_header.frame_id_high = (uint8_t)(fid >> 8);
    frame->rt_header.frame_id_low = (uint8_t)(fid & 0xFF);

    /* Build payload from IO data */
    size_t offset = 0;
    size_t total = io_data->input_len + io_data->output_len;
    if (total > PNET_MAX_PAYLOAD_SIZE) return -1;

    if (io_data->input_len > 0) {
        memcpy(frame->payload + offset, io_data->input_data, io_data->input_len);
        offset += io_data->input_len;
    }
    if (io_data->output_len > 0) {
        memcpy(frame->payload + offset, io_data->output_data, io_data->output_len);
        offset += io_data->output_len;
    }

    frame->payload_len = offset;
    frame->rt_header.data_length = __builtin_bswap16((uint16_t)offset);
    frame->total_len = sizeof(pnet_eth_header_t) + sizeof(pnet_rt_header_t) + offset;
    frame->rt_class = PNET_RT_CLASS_RT;
    frame->cycle_counter = 0;
    frame->valid = true;

    return 0;
}

int pnet_frame_parse(const uint8_t *raw, size_t len, pnet_frame_t *frame) {
    if (!raw || !frame) return -1;

    size_t min_len = sizeof(pnet_eth_header_t) + sizeof(pnet_rt_header_t);
    if (len < min_len) return -1;

    memset(frame, 0, sizeof(pnet_frame_t));

    /* Parse Ethernet header */
    memcpy(&frame->eth_header, raw, sizeof(pnet_eth_header_t));

    /* Check EtherType */
    uint16_t ethertype = __builtin_bswap16(frame->eth_header.ethertype);
    if (ethertype != PNET_ETHERTYPE_PROFINET) {
        frame->valid = false;
        return -1;
    }

    /* Parse RT header */
    memcpy(&frame->rt_header, raw + sizeof(pnet_eth_header_t), sizeof(pnet_rt_header_t));

    /* Extract payload */
    frame->payload_len = __builtin_bswap16(frame->rt_header.data_length);
    if (frame->payload_len > PNET_MAX_PAYLOAD_SIZE) return -1;

    size_t payload_offset = sizeof(pnet_eth_header_t) + sizeof(pnet_rt_header_t);
    if (payload_offset + frame->payload_len > len) return -1;

    memcpy(frame->payload, raw + payload_offset, frame->payload_len);
    frame->total_len = payload_offset + frame->payload_len;
    frame->rt_class = PNET_RT_CLASS_RT;
    frame->valid = true;

    g_proto_stats.frames_received++;
    return 0;
}

int pnet_frame_validate(const pnet_frame_t *frame) {
    if (!frame) return -1;
    if (!frame->valid) return -1;

    /* Check minimum frame size */
    if (frame->total_len < PNET_MIN_FRAME_SIZE && frame->total_len > 0) {
        return -2; /* Frame too small */
    }

    /* Check maximum frame size */
    if (frame->total_len > PNET_MAX_FRAME_SIZE) {
        return -3; /* Frame too large */
    }

    /* Check EtherType */
    uint16_t ethertype = __builtin_bswap16(frame->eth_header.ethertype);
    if (ethertype != PNET_ETHERTYPE_PROFINET) {
        return -4; /* Wrong EtherType */
    }

    return 0;
}

size_t pnet_frame_serialize(const pnet_frame_t *frame, uint8_t *buffer, size_t buf_len) {
    if (!frame || !buffer) return 0;

    if (buf_len < frame->total_len) return 0;

    size_t offset = 0;

    /* Ethernet header */
    memcpy(buffer + offset, &frame->eth_header, sizeof(pnet_eth_header_t));
    offset += sizeof(pnet_eth_header_t);

    /* RT header */
    memcpy(buffer + offset, &frame->rt_header, sizeof(pnet_rt_header_t));
    offset += sizeof(pnet_rt_header_t);

    /* Payload */
    if (frame->payload_len > 0) {
        memcpy(buffer + offset, frame->payload, frame->payload_len);
        offset += frame->payload_len;
    }

    return offset;
}

int pnet_cyclic_send(pnet_cr_descriptor_t *cr, const pnet_io_data_t *data) {
    if (!cr || !data) return -1;
    if (!cr->active) return -1;

    pnet_frame_t frame;
    int ret = pnet_frame_build(&frame, data);
    if (ret != 0) return ret;

    frame.cycle_counter++;
    g_proto_stats.frames_sent++;
    return 0;
}

int pnet_cyclic_receive(pnet_cr_descriptor_t *cr, pnet_io_data_t *data) {
    if (!cr || !data) return -1;
    if (!cr->active) return -1;

    /* Simulation: mark data as valid */
    data->data_valid = true;
    g_proto_stats.frames_received++;
    return 0;
}

int pnet_acyclic_read(uint16_t slot, uint16_t subslot, uint16_t index,
                      void *buffer, size_t buf_len) {
    if (!buffer || buf_len == 0) return -1;

    /* Simulation: return dummy acyclic data */
    memset(buffer, 0, buf_len);
    snprintf((char *)buffer, buf_len, "ACYCLIC_READ slot=%u subslot=%u index=%u",
             slot, subslot, index);
    return (int)strlen((char *)buffer);
}

int pnet_acyclic_write(uint16_t slot, uint16_t subslot, uint16_t index,
                       const void *buffer, size_t len) {
    if (!buffer || len == 0) return -1;
    (void)slot;
    (void)subslot;
    (void)index;
    /* Simulation: accept the write */
    return (int)len;
}

int pnet_cr_create(pnet_cr_descriptor_t *cr, pnet_cr_type_t type, pnet_rt_class_t rt_class) {
    if (!cr) return -1;

    memset(cr, 0, sizeof(pnet_cr_descriptor_t));
    cr->type = type;
    cr->rt_class = rt_class;
    cr->frame_id = (uint16_t)(0x8000 + type * 0x100);
    cr->send_clock_factor = 32;    /* 1ms default */
    cr->reduction_ratio = 1;
    cr->watchdog_factor = 3;
    cr->cycle_time_us = 1000;      /* 1ms default cycle time */
    cr->active = false;
    return 0;
}

int pnet_cr_activate(pnet_cr_descriptor_t *cr) {
    if (!cr) return -1;
    cr->active = true;
    return 0;
}

int pnet_cr_deactivate(pnet_cr_descriptor_t *cr) {
    if (!cr) return -1;
    cr->active = false;
    return 0;
}

int pnet_cr_destroy(pnet_cr_descriptor_t *cr) {
    if (!cr) return -1;
    cr->active = false;
    memset(cr, 0, sizeof(pnet_cr_descriptor_t));
    return 0;
}

int pnet_protocol_get_stats(pnet_protocol_stats_t *stats) {
    if (!stats) return -1;
    memcpy(stats, &g_proto_stats, sizeof(pnet_protocol_stats_t));
    return 0;
}

void pnet_protocol_reset_stats(void) {
    memset(&g_proto_stats, 0, sizeof(g_proto_stats));
    g_proto_stats.min_latency_us = 1e9;
}

const char* pnet_rt_class_str(pnet_rt_class_t rt_class) {
    switch (rt_class) {
        case PNET_RT_CLASS_UDP: return "NRT/UDP";
        case PNET_RT_CLASS_RT:  return "RT";
        case PNET_RT_CLASS_IRT: return "IRT";
        case PNET_RT_CLASS_TSN: return "TSN";
        default: return "UNKNOWN";
    }
}

const char* pnet_cr_type_str(pnet_cr_type_t type) {
    switch (type) {
        case PNET_CR_TYPE_IO_DATA:  return "IO_DATA";
        case PNET_CR_TYPE_IO_PARAM: return "IO_PARAM";
        case PNET_CR_TYPE_ALARM:    return "ALARM";
        case PNET_CR_TYPE_ADMIN:    return "ADMIN";
        default: return "UNKNOWN";
    }
}
